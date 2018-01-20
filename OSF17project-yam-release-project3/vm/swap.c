#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "devices/block.h"
#include <bitmap.h>
#include <debug.h>

/*Notes: 
  Trace up to swapping:
	frame.c has a LL of frames (using palloc_get_page)
	as program need a page (lazy loading), they call
	get_free_user_Frame() -> next LL frame entry is now 
	occupied with process or if LL reaches end call swap_in() 
*/

#define sectorSize 4
#define startIndex 0
#define firstTrue 1

struct block *swap_block;
struct bitmap *bm; //free bitmap using bitmap_destory()


/*find swap block that should be created in terminal
using pintos-mkdisk swap.dsk 4 for 4MB swap disk in vm/build*/
void swap_init(void){
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block != NULL){
		//block sector is 512 bytes, a page is 4KB
		bm = bitmap_create (block_size(swap_block)/sectorSize);
	}//else { Panic("No swap area found in HDD/SSD"); }
}

//puts evicted frame into swap disk at sector freeBlockSector
int swap_out(struct frameTableEntry *e){
	int freeBlockSector = bitmap_scan_and_flip(bm, startIndex, firstTrue, false);
	if(freeBlockSector == 0){
		//Panic("Swap space full");
	}

	//freeBlockSector is array of bits stating free or not -> index 1 *= 4
	for(int i = 0; i < sectorSize; i++){
		uint32_t swap_block_sector = (freeBlockSector * sectorSize) + i;
		e->frame = ((uint8_t*)e->frame) + (i * BLOCK_SECTOR_SIZE); //point to each 512 byte section of frame

		block_write(swap_block, swap_block_sector, (void*)e->frame);
	}	

	//dont clear mapping since PD points to same Frame
	//pagedir_clear_page(); 

	e->occupied = false;
	return freeBlockSector; 
}

/*Page Fault if page is in swap area - function allocates frame
 for page in swap disk, and moves it into main memory*/
void swap_in(int freeBlockSector, uint8_t *frame){ //pass in free frame, circular dependency
	if(!bitmap_contains(bm, freeBlockSector, 1, true)){
		//Panic("Not a valid sector in swap disk");
	}

	bitmap_scan_and_flip(bm, freeBlockSector, 1, true);
	// uint8_t *frame = (uint8_t*)get_free_user_Frame();

	for(int i = 0; i < sectorSize; i++){
		uint32_t swap_block_sector = (freeBlockSector * sectorSize) + i;
		frame += (i * BLOCK_SECTOR_SIZE); //point to each 512 byte section of frame

		block_read(swap_block, swap_block_sector, (void*)frame);
		//todo: clear the sector bc if you write but dont write all 4 sectors?
	}
}

/*on process termination, remove a process's pages from swap area*/
void swap_clean(void){ }
