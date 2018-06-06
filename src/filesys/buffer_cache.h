#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include "threads/synch.h"


#define BUFFER_CACHE_ENTRY_NB 64



/* buffer cache entry */
struct buffer_head
{
    bool dirty;  //해당entry가dirty인지를나타내는flag 
    bool valid;  //해당entry의사용여부를나타내는flag        
    block_sector_t sector;  //해당 entry의 disk sector 주소 
    bool clock_bit;     //clock algorithm을위한clock bit
    struct lock lock;   //lock 변수(structlock)
    void *data;         //buffer cache entry를 가리키기 위한 데이터 포인터
};

bool bc_read (block_sector_t sector_idx, void *buffer, 
              off_t buffer_ofs, int chunk_size, int sector_ofs);
bool bc_write (block_sector_t sector_idx, void *buffer, 
               off_t buffer_ofs, int chunk_size, int sector_ofs);
void bc_init (void);
void bc_term (void);
struct buffer_head *bc_lookup (block_sector_t sector);
struct buffer_head *bc_select_victim (void);

void bc_flush_entry (struct buffer_head*);
void bc_flush_all_entries (void);


#endif
