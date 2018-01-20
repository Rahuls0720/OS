#ifndef _SWAPH_
#define _SWAPH_

// Forward declare frame table entry
struct frameTableEntry;

void swap_init(void);
int swap_out(struct frameTableEntry *e);
void swap_in(int freeBlockSector, uint8_t *frame);
void swap_clean(void);

#endif
