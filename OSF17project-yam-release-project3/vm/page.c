#include "vm/page.h"
#include <debug.h>
#include <hash.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


unsigned hashVirtualAddress(const struct hash_elem *e, void *aux UNUSED);
bool pageLessThan(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

/* Initializes current thread's hashed supplementary page table*/ 
void supplPT_init(void) {
	hash_init(&thread_current()->spt, hashVirtualAddress, pageLessThan, NULL);
}

/* Hashing function for hash_init
	Hashes the user page (virtual address) */
unsigned hashVirtualAddress(const struct hash_elem *e, void *aux UNUSED) {
	const struct supplPTEntry *entry = \
		hash_entry(e, struct supplPTEntry, hashElem);
	return hash_int((int)entry->upage);
}

/* Comparison function for hash_init 
	Returns (virtual address of a < virtual address of b) */
bool pageLessThan(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct supplPTEntry *entry_a = \
		hash_entry(a, struct supplPTEntry, hashElem);
	struct supplPTEntry *entry_b = \
		hash_entry(b, struct supplPTEntry, hashElem);

	return ((uint32_t)entry_a->upage) < ((uint32_t)entry_b->upage);
}

/* Creates and inserts a new SPTE into a threads SPT 
	Returns a pointer to the new SPTE */
struct supplPTEntry *supplPTEntry_create(const void *va) {
	void *upage = pg_round_down(va);
	struct supplPTEntry *entry = malloc(sizeof(struct supplPTEntry));	

	// Initialize all fields of supplPTEntry
	entry->valid = true;
	entry->location= PHYS_MEM;
	entry->upage = upage;

	// Insert into current thread's spt
	hash_insert(&thread_current()->spt, &(entry->hashElem));
	return entry;
}

/* Returns a pointer to the supplemental page table entry.
   Returns null if invalid entry (not yet mapped) */
struct supplPTEntry *getSPTEntry(const void *va) {
	struct hash_elem *e;
	struct supplPTEntry dummyEntry;
	void *upage = pg_round_down(va);
	dummyEntry.upage = upage;

	struct supplPTEntry *returnEntry;
	e = hash_find(&thread_current()->spt, &(dummyEntry.hashElem));
	returnEntry = hash_entry(e, struct supplPTEntry, hashElem);

	return returnEntry;
}
