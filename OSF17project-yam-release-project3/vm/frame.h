#ifndef _FRAMEH_
#define _FRAMEH_

#include <hash.h>

struct frameTableEntry {
	struct hash_elem frameTableElem;
	bool recent;
	bool occupied;
	void *frame;
	struct supplPTEntry *spte;
};

void frame_table_init(void);
void addFrame(void *frame);
void updateFrameSpte(void *frame, struct supplPTEntry *spte);
unsigned hashPhysAddress(const struct hash_elem *e, void *aux);
bool frameLessThan(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void *get_free_user_frame(void);
struct frameTableEntry* getClockVictim(void);
void increment_clockHead(void);
void replaceUsedFrame(void* frame);

#endif

