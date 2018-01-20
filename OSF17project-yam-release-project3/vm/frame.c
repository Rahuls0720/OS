#include "vm/frame.h"
#include <debug.h>
#include <hash.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.c"



void addFrame(void *frame);
unsigned hashPhysAddress(const struct hash_elem *e, void *aux UNUSED);
bool frameLessThan(const struct hash_elem *a,
				   const struct hash_elem *b,
				   void *aux);
struct hash_elem* next_FTElement(struct hash_elem *e); 

static struct hash frameTable;
static struct hash_iterator *clockHead;



/*	Takes all pages from palloc_get_page(PAL_USER) and
	initializes 'frameTable' */
void frame_table_init(void) {
	hash_init(&frameTable, hashPhysAddress, frameLessThan, NULL);	
	void *frame = palloc_get_page(PAL_USER | PAL_ZERO);
	while (frame != NULL) {
		addFrame(frame);
		frame = palloc_get_page(PAL_USER | PAL_ZERO);
	}

	// Init clockHead
	clockHead = malloc(sizeof(struct hash_iterator));
}

/*	Adds a new frame to the 'frameTable' */
void addFrame(void *frame) {
	struct frameTableEntry *entry = malloc(sizeof(struct frameTableEntry));
	entry->recent = true;
	entry->occupied = false;
	entry->frame = frame;
	entry->spte = NULL;
	hash_insert(&frameTable, &(entry->frameTableElem));	
}

/* Updates a frame's supplemental page table */
void updateFrameSpte(void *frame, struct supplPTEntry *spte) {
	struct hash_elem *e;
	struct frameTableEntry dummyEntry;
	dummyEntry.frame = frame;	
	
	e = hash_find(&frameTable, &(dummyEntry.frameTableElem));
	struct frameTableEntry *entry = hash_entry(e, struct frameTableEntry, frameTableElem);

	// TODO need a way to access spte without frame
	// freeing for now, to avoid memory leaks
	if (entry->spte != NULL) {
		free(entry->spte);	
	}
	entry->spte = spte;
}

/* Hashing function for hash_init
   Hashes the frame (physical address) */
unsigned hashPhysAddress(const struct hash_elem *e, void *aux UNUSED) {
	const struct frameTableEntry *entry = hash_entry(e, struct frameTableEntry, frameTableElem);
	return hash_int((int)entry->frame);
}

/* Comparison function for hash_init 
   Returns (physical address of a < physcial address of b) */
bool frameLessThan(const struct hash_elem *a,
				   const struct hash_elem *b,
				   void *aux UNUSED) {
	struct frameTableEntry *entry_a = hash_entry(a, struct frameTableEntry, frameTableElem);
	struct frameTableEntry *entry_b = hash_entry(b, struct frameTableEntry, frameTableElem);
	
	return ((uint32_t)entry_a->frame) < ((uint32_t)entry_b->frame);
}

/*	Replaces palloc_get_page(PAL_USER): 
	Returns unoccupied frame, or evicts a frame if all are occupied */
void *get_free_user_frame(void) {
	struct hash_iterator e;	
	hash_first(&e, &frameTable);
	hash_next(&e);
	struct frameTableEntry *entry = hash_entry(hash_cur(&e), 
											   struct frameTableEntry, 
											   frameTableElem);		
	
	// Look for unoccupied frame
	while (hash_next(&e)) {
		entry = hash_entry(hash_cur(&e), struct frameTableEntry, frameTableElem);		
		if (!entry->occupied) {
			break;
		}
	}

	// Evict frame if all occupied (i.e. tail reached)
	if (hash_cur(&e) == NULL) {
			entry = getClockVictim();
			int freeBlockSector = swap_out(entry); //TODO: update supplementary table and PTE bc the bits can change

			//struct supplPTEntry *spte = entry->spte;
			//entry->spte->location = SWAP;
			//entry->spte->blockSector = freeBlockSector;
	}

	// TODO need to remove frame references from page tables
	// TODO need to write to disk? swap?
	entry->recent = true;
	entry->occupied = true;
	return entry->frame;
}

/*	Uses clock algorithm to find a frame to evict */
struct frameTableEntry* getClockVictim(void) {
	// TODO remove this?
	hash_first(clockHead, &frameTable);
	increment_clockHead();

	struct frameTableEntry *entry = \
		hash_entry(hash_cur(clockHead), struct frameTableEntry, frameTableElem);		

	while (entry->recent) {
		entry->recent = false;
		increment_clockHead();
		entry = hash_entry(hash_cur(clockHead), struct frameTableEntry, frameTableElem);
	}
	return entry;
}

/* Iterates through frame table. Loops back to beginning when tail is reached. */
void increment_clockHead(void) {
	if (hash_next(clockHead) == NULL) {
		hash_first(clockHead, &frameTable);	
	}
}

/*	TODO 
	remove refrences from page tables that refer to it
	need to write to disk? swap? */
void replaceUsedFrame(void* frame) {

}

