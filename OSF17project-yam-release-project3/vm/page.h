#include <hash.h>
#include <stdbool.h>


/* States in a thread's life cycle. */
enum page_location {
    PHYS_MEM,
    DISK,		
    SWAP	
};

struct supplPTEntry {
	bool valid;

	// User page this SPTE refers to
	void *upage;

	// In PHYSICAL_MEM, DISK, or SWAP
	enum page_location location;

	// offset is only set when on DISK
	unsigned offset;

	// sector of location in) block area - only set when in SWAP
	unsigned blockSector;	

	// is page writeable? for updating PTE bits on swap
	bool writable;

	struct hash_elem hashElem;

	// locks?
};



void supplPT_init(void);
struct supplPTEntry *supplPTEntry_create(const void *va);
struct supplPTEntry *getSPTEntry(const void *va);
