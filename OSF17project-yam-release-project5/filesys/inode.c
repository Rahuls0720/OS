#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define BLOCK 8 //8 sectors in a block

// Forward Declarations
block_sector_t *get_inode_block(const struct inode *inode, \
								block_sector_t blockIndex, \
								uint32_t **working_block);
block_sector_t *get_block_ptr(struct inode *inode, \
								block_sector_t given_index, \
								uint32_t **working_block);
void block_clear(block_sector_t first_sector_in_block);
block_sector_t get_table_block(struct inode *inode, block_sector_t block_index);
bool blocks_alloc(struct inode* inode, block_sector_t numOfBlocks);
void inode_release_blocks(struct inode *inode);

// /* On-disk inode.
//    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk
//   {
//   	 /* All pointers point to the first sector of their
//      respective block */
// 	 block_sector_t direct_ptrs[12];	/* Direct Pointer */
// 	 block_sector_t indirect_ptr;		/* Indirect Pointer */
// 	 block_sector_t double_ind_ptr;		 Double Indirect Pointer 
// 	 uint32_t blk_allocated;			/* Number of blocks allocated for this inode */
// 	 off_t length;						/* File size in bytes. */
// 	 off_t original_length;				/* Size in bytes at original creation */
//    bool isdir;                  /* Is inode a directory? */

// 	 unsigned magic;		/* Magic number. */
// 	 uint32_t unused[110];	/* Not used. */
//   };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

// /* In-memory inode. */
// struct inode
//   {
//     struct list_elem elem;              /* Element in inode list. */
//     block_sector_t sector;              /* Sector number of disk location. */
//     int open_cnt;                        Number of openers. 
//     bool removed;                       /* True if deleted, false otherwise. */
//     int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
//     struct inode_disk data;             /* Inode content. */
//   };

/* Returns the block index that contains byte offset POS within
   INODE. The return value is not guaranteed to be initialized. */
static block_sector_t //change to signed int?
byte_to_block (off_t pos) {
	return (pos / (4*1024));		// 4kb = SIZE_OF_BLOCK
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t //change to signed int?
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);

  if (pos >= (inode->data.blk_allocated * 4096)) {
	return -1;
  }

  uint32_t inode_block_index = pos / 4096;  // 4096 == Size of block
  block_sector_t sector_in_block = (pos - (inode_block_index*4096)) / BLOCK_SECTOR_SIZE;

  uint32_t *working_block = NULL;
  block_sector_t *block_num = get_inode_block(inode, inode_block_index, &working_block);

  if (block_num == NULL) {
    return -1; 
  } else {
	block_sector_t first_sector_in_block = *block_num;
  	if (working_block) {
		free(working_block);
	}
    return (first_sector_in_block + sector_in_block);
  }
}

/* Returns a pointer to an inode's block given by 'blockIndex'.
   Does not allocate more blocks if blockIndex is out of bounds.
   'working_block' holds the block for the returned pointer or
   NULL if the returned pointer is a direct block.
   You must eventually free 'working_block'. */
block_sector_t*
get_inode_block(const struct inode *inode, \
				block_sector_t blockIndex, \
				uint32_t **working_block) {
  /*if (blockIndex > 11 && blockIndex+1 > inode->data.blk_allocated) {
    return NULL;
  }*/

  block_sector_t *block_location = NULL;

  // Direct (Blocks [0-11])
  if (blockIndex <= 11) {
    block_location = &(inode->data.direct_ptrs[blockIndex]);
	working_block = NULL;

  // Indirect (Blocks [12-1035])
  } else if (blockIndex <= 1035) {
    uint32_t *workingBlock = malloc(sizeof(uint32_t) * 128);

    int index_in_indirect = blockIndex - 12;
    int sector_in_indirect = index_in_indirect / 128;   // SectorSize / 8 = 128
    int index_in_sector = index_in_indirect - (sector_in_indirect * 128);

    // Loading Indirect
    block_read(fs_device, inode->data.indirect_ptr + sector_in_indirect, workingBlock);
    block_location = &(workingBlock[index_in_sector]);

	*working_block = workingBlock;

  // Double Indirect (Blocks [1036-1049611])
  } else if (blockIndex <= 1049611) {
    uint32_t *workingBlock = malloc(sizeof(uint32_t) * 128);

    int index_in_doubleInd = (blockIndex - 1036) / 1024;
    int sector_in_doubleInd = index_in_doubleInd / 128;
    int index_in_doubleSector = index_in_doubleInd - (sector_in_doubleInd * 128);

    int index_in_indirect = blockIndex - (index_in_doubleInd*1024 + 1036);
    int sector_in_indirect = index_in_indirect / 128;
    int index_in_indSector = index_in_indirect - (sector_in_indirect * 128);

    // Loading Double Indirect
    block_read(fs_device, inode->data.double_ind_ptr + sector_in_doubleInd, workingBlock);
    block_sector_t indirectTable = workingBlock[index_in_doubleSector];

    // Loading Indirect
    block_read(fs_device, indirectTable + sector_in_indirect, workingBlock);
    block_location = &(workingBlock[index_in_indSector]);

  	*working_block = workingBlock;
  }

  return block_location;
}

/* Returns a pointer to the block, 'given_index'. It also 
   extends the inode's data blocks if needed in order to return
   a pointer. */
block_sector_t
*get_block_ptr(struct inode *inode, \
				block_sector_t block_index, \
				uint32_t **working_block) {

	block_sector_t new_block;
	uint32_t *workingBlock = malloc(sizeof(uint32_t) * 128);

	// Border of Indirect
	if (block_index == 12 && (block_index + 1) > inode->data.blk_allocated) {
		free_map_allocate(BLOCK, &new_block);
		inode->data.indirect_ptr = new_block;

	// Border of Double Indirect
	} else if (block_index == 1036 && (block_index + 1) > inode->data.blk_allocated) {
		// Allocate double indirect
		free_map_allocate(BLOCK, &new_block);
		inode->data.double_ind_ptr = new_block;

		// Allocate double indirect's first entry (block)
		block_read(fs_device, inode->data.double_ind_ptr, workingBlock);
		free_map_allocate(BLOCK, &new_block);
		workingBlock[0] = new_block;

	// Border of Double Indirect Entries
	} else if ((block_index - 12) % 1024 == 0 && (block_index + 1) > inode->data.blk_allocated) {
		// Load double indirect table
		block_read(fs_device, inode->data.double_ind_ptr, workingBlock);
		free_map_allocate(BLOCK, &new_block);

		// Allocate double indirect's next entry (block)
		block_sector_t new_index = (block_index - 12) / 1024;
		workingBlock[new_index] = new_block;
	}

	free(workingBlock);
	return get_inode_block(inode, block_index, working_block);
}

/* Sets the whole block to zero */
void block_clear(block_sector_t first_sector_in_block) {
	uint32_t *zeros = calloc(128, sizeof(uint32_t));
	for (int i = 0; i < BLOCK; i++) {
		block_write (fs_device, first_sector_in_block + i, zeros);
	}
}

/* Returns the block number of the table 'block_index'
   resides in. Returns -1 if the index is not in a table
   (i.e. indices 0-11).

   Ex: 'block_index' == 500
   		return block_sector_t of 'indirect_ptr' */
block_sector_t
get_table_block(struct inode *inode, block_sector_t block_index) {
	if (block_index <= 11) {
		return -1;
	} else if (block_index <= 1035) {
		return inode->data.indirect_ptr;	
	} else if (block_index <= 1049611) {
		uint32_t *doubleIndTableSect = malloc(sizeof(uint32_t) * 128);
		
		int index_in_doubleInd = (block_index - 1036) / 1024;
		int sector_in_doubleInd = index_in_doubleInd / 128;
		int index_in_doubleSector = index_in_doubleInd - (sector_in_doubleInd * 128);

		// Loading Double Indirect
		block_read(fs_device, inode->data.double_ind_ptr + sector_in_doubleInd, doubleIndTableSect);
		block_sector_t residing_block = doubleIndTableSect[index_in_doubleSector];
		free(doubleIndTableSect);
		return residing_block;
	} else {
		return -1;
	}
}

/* Allocates and clears 'numOfBlocks' amount of blocks to inode 
   Returns true, if success*/
bool
blocks_alloc(struct inode* inode, block_sector_t numOfBlocks){
  block_sector_t first_sector_in_block;
  uint32_t blocks_allocated = inode->data.blk_allocated;

  if (blocks_allocated > 1049612) {
 	return false; 
  }

  while (numOfBlocks != 0) {
    if (free_map_allocate(BLOCK, &first_sector_in_block)) {
	  uint32_t *working_block = NULL;
      block_sector_t* block_ptr = get_block_ptr(inode, blocks_allocated, &working_block);
	  block_clear(first_sector_in_block);
	  *block_ptr = first_sector_in_block;

	  // Writing changes if block index is < 11
	  if (working_block != NULL) {
		block_sector_t sector_to_write = get_table_block(inode, blocks_allocated);
		block_write(fs_device, sector_to_write, working_block);
	  	free(working_block);
	  }
	  
	  inode->data.blk_allocated++;
	  //inode->data.length = inode->data.blk_allocated * 4096;
	  blocks_allocated = inode->data.blk_allocated;
	  numOfBlocks--;
    } else { return false; }
  }
  return true;
}

 // List of open inodes, so that opening a single inode twice
 //   returns the same `struct inode'. 
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  //disk_inode = calloc (1, sizeof *disk_inode);
  struct inode *temp_inode = calloc(1, sizeof(struct inode));
  if (temp_inode != NULL)
    {
	  temp_inode->data.blk_allocated = 0;
	  temp_inode->data.original_length = length;
	  temp_inode->data.length = 0;
	  temp_inode->data.isdir = isdir; 
      size_t sectors = bytes_to_sectors(length);

	  // Initial size
	  block_sector_t blocks = (sectors%8 == 0) ? (sectors/8) : (sectors/8 + 1);
	  success = blocks_alloc(temp_inode, blocks);

	  if (success) {
	 	block_write(fs_device, sector, &(temp_inode->data));
	  }

      free(disk_inode);
	  free(temp_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Frees all of the datablocks used by an inode:
   direct, indirect, and double indirect */
void inode_release_blocks(struct inode *inode) {
	block_sector_t currentIndex = inode->data.blk_allocated - 1;
	block_sector_t *currentBlock;

	while (currentIndex > 0) {
		uint32_t *working_block = NULL;
		currentBlock = get_inode_block(inode, currentIndex, &working_block);	
		free_map_release(*currentBlock, BLOCK);
		if (working_block) {
			free(working_block);
		}

		inode->data.blk_allocated -= 1;
		currentIndex -= 1;
	}
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Write to disk */
  block_write(fs_device, inode->sector, &(inode->data));

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
		  inode_release_blocks(inode);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      if(sector_idx == -1){ break; } //reached EOF

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

	  /* Grow file when necessary */
	  if (sector_idx == -1) {
	 	blocks_alloc(inode, byte_to_block(offset)+1 - inode->data.blk_allocated); 
		sector_idx = byte_to_sector (inode, offset);
	  }

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      //off_t inode_left = inode_length (inode) - offset;
	  off_t inode_left = (inode->data.blk_allocated * 4096) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
	  
	  if (offset > inode->data.length) {
	 	inode->data.length = offset; 
	  }
    }
  free (bounce);

  //inode->data.length = inode->data.length + bytes_written;
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  /* We return the max length incase the file grew */
  off_t max_length;
  if (inode->data.length > inode->data.original_length) {
 	max_length = inode->data.length;
  } else {
 	max_length = inode->data.original_length;
  }

  return max_length;
}

