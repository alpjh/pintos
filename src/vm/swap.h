#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>

#define SWAP_FREE 0

void swap_init(void);
size_t swap_out(void * kaddr);
void swap_in(size_t used_index, void * kaddr);

#endif
