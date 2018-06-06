#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/buffer_cache.h"
#include "threads/malloc.h"

#include <string.h>
#include <stdio.h>

//buffer cache entry의개수(32kb)
#define BUFFER_CACHE_ENTRY_NB 64

void *p_buffer_cache; //buffer cache 메모리영역을 가리킴
struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB]; //bufferhead array
static int clock_hand; //victim entry 선정시clock 알고리즘을위한변수



bool bc_read (block_sector_t sector_idx, void *buffer, off_t bytes_read, 
              int chunk_size, int sector_ofs) {
    
    struct buffer_head *bf_head;
  
    /* sector_idx를buffer_head에서검색(bc_lookup함수이용)*/
    /* 검색결과가없을경우, 디스크블록을캐싱할buffer entry의
       buffer_head를구함(bc_select_victim함수이용)*/
    if (!(bf_head = bc_lookup(sector_idx))) {
        if (!(bf_head = bc_select_victim()))
            return false;
       
        //lock
        lock_acquire(&bf_head->lock);

        /* block_read함수를이용해, 디스크블록데이터를buffer cache
           로read */ 
        block_read(fs_device, sector_idx, bf_head->data);

        //Alloc buffer head
        bf_head->dirty = false;
        bf_head->valid = true;
        bf_head->sector = sector_idx;

        //unlock
        lock_release(&bf_head->lock);
    }
    //lock before setting
    lock_acquire (&bf_head->lock);
    /* memcpy함수를통해, buffer에디스크블록데이터를복사*/
    memcpy (buffer + bytes_read, bf_head->data + sector_ofs, chunk_size);
    /* buffer_head의clock bit을setting */
    bf_head->clock_bit = true;
    //unlock
    lock_release(&bf_head->lock);
    
    return true;
}

bool bc_write (block_sector_t sector_idx, void *buffer, off_t
        bytes_written, int chunk_size, int sector_ofs) {

    struct buffer_head *bf_head;
    
    /* sector_idx를buffer_head에서검색하여buffer에복사(구현)*/
    if (!(bf_head = bc_lookup (sector_idx))) {
        if (!(bf_head = bc_select_victim()))
            return false;
        block_read (fs_device, sector_idx, bf_head->data);
    }

    lock_acquire(&bf_head->lock);
    memcpy(bf_head->data + sector_ofs, buffer + bytes_written, chunk_size);

    /* update buffer head */
    bf_head->dirty = true;
    bf_head->valid = true;
    bf_head->sector = sector_idx;
    bf_head->clock_bit = true;
    lock_release(&bf_head->lock);

    return true;;
}


void bc_init (void) {

    int i;
    void *p_data;  

    /* Allocation buffer cache in Memory */
    p_buffer_cache = malloc(BLOCK_SECTOR_SIZE*BUFFER_CACHE_ENTRY_NB);

    if (p_buffer_cache == NULL) {
        printf("\n[%s] Memory Allocation Fail.\n",__FUNCTION__);
        return;
    }
    else{
        p_data = p_buffer_cache;
    }
    /* 전역변수buffer_head자료구조초기화*/
    for(i=0; i<BUFFER_CACHE_ENTRY_NB; i++){
        buffer_head[i].dirty = false;
        buffer_head[i].valid = false;
        buffer_head[i].sector = -1;
        buffer_head[i].clock_bit = 0;
        lock_init(&buffer_head[i].lock);
        buffer_head[i].data = p_data;
        p_data = p_data + BLOCK_SECTOR_SIZE;
    }
}

void bc_term(void) {
    /* bc_flush_all_entries함수를 호출하여 모든 
       buffer cache entry를 디스크로 flush */
    bc_flush_all_entries();
    /* buffer cache 영역할당해제*/
    free(p_buffer_cache);
}

struct buffer_head *bc_select_victim (void) {
    
    int idx;

    /* clock 알고리즘을사용하여victim entry를선택*/
    while(1){
        idx = clock_hand;

        /* buffer_head전역변수를순회하며clock_bit변수를검사*/
        if(idx == BUFFER_CACHE_ENTRY_NB)
            idx = 0;

        if(++clock_hand == BUFFER_CACHE_ENTRY_NB)
            clock_hand = 0;

        if(buffer_head[idx].clock_bit){
            lock_acquire(&buffer_head[idx].lock);
            buffer_head[idx].clock_bit = 0;
            lock_release(&buffer_head[idx].lock);
        }
        else{
            lock_acquire(&buffer_head[idx].lock);
            buffer_head[idx].clock_bit = 1;
            lock_release(&buffer_head[idx].lock);
            break;
        }
    }

    /* 선택된 victim entry가 dirty일 경우, 디스크로flush */
    if(buffer_head[idx].dirty == true){
        bc_flush_entry(&buffer_head[idx]);
    }

    /* victim entry에해당하는buffer_head값update */
    lock_acquire(&buffer_head[idx].lock);
    buffer_head[idx].dirty = false;
    buffer_head[idx].valid = false;
    buffer_head[idx].sector = -1;
    lock_release(&buffer_head[idx].lock);
    /* victim entry를return */
    return &buffer_head[idx];
}


struct buffer_head* bc_lookup (block_sector_t sector) {

    int idx;

    /* buffe_head를순회하며, 전달받은sector 값과동일한
       sector 값을갖는buffer cacheentry가있는지확인*/
    for (idx = 0; idx < BUFFER_CACHE_ENTRY_NB; idx++) {
        if (buffer_head[idx].sector == sector) {
            /* 성공: 찾은buffer_head반환 */
            return &buffer_head[idx];
        }
    }
    /* 실패: NULL */
    return NULL;
}

void bc_flush_entry (struct buffer_head *p_flush_entry) {
    lock_acquire(&p_flush_entry->lock);
    /* block_write을 호출하여, 인자로 전달받은
       buffer cache entry의 데이터를 디스크로 flush */
    block_write(fs_device, p_flush_entry->sector, p_flush_entry->data);
    /* buffer_head의dirty 값update */
    p_flush_entry->dirty = false;
    lock_release(&p_flush_entry->lock);
}

void bc_flush_all_entries( void) {
    int idx;

    /* 전역변수 buffer_head를 순회하며, 
       dirty인 entry는 block_write 함수를 호출하여 디스크로 flush */
    for (idx = 0; idx < BUFFER_CACHE_ENTRY_NB; idx++) {

        if (buffer_head[idx].dirty == true) {
            /* 디스크로flush한후, buffer_head의dirty 값update */

            bc_flush_entry (&buffer_head[idx]);
            /*lock_acquire (&buffer_head[idx].lock);
            block_write (fs_device, buffer_head[idx].sector, buffer_head[idx].data);
            buffer_head[idx].dirty = false;
            lock_release (&buffer_head[idx].lock);*/
        }
    }
}
