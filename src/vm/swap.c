#include "vm/swap.h" 

struct bitmap *swap_bitmap;
struct lock swap_lock;
struct block *swap_block;

void swap_init(){

    swap_block = block_get_role(BLOCK_SWAP);

    if (!swap_block)
        return;
   
    swap_bitmap = bitmap_create (BLOCK_SECTOR_SIZE * 
                                 block_size (swap_block) / PGSIZE);
    if (!swap_bitmap)
        return;

    bitmap_set_all(swap_bitmap, 0);
    lock_init(&swap_lock);
}


//alloc_page()함수에서 palloc_get_page()가 NULL을 반환했을 경우 쓰임
size_t swap_out(void *kaddr) {
//스왑파티션에 접근할수 있어야하고 어느번째위치에 저장해야하는지 알 수 있어야함
//swap_bitmap에서 값이 0인 자리를 찾아야함

    size_t i;

    if (!(swap_block && swap_bitmap)) {
        printf("\nin swap_out no swap block or swap map\n");
        return;
    }

    lock_acquire (&swap_lock);

	size_t index = bitmap_scan_and_flip(swap_bitmap, 0, 1, SWAP_FREE);

    for (i=0; i<PGSIZE/BLOCK_SECTOR_SIZE; i++) {
	    block_write(swap_block, index * PGSIZE/BLOCK_SECTOR_SIZE, 
                    kaddr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);
    return index;
}


//used_index의 swap slot에 저장된 데이터를 논리주소 kaddr로 복사
//page_fault함수 발생시 vme는 있는데 present bit 가 0일때 쓰임
void swap_in (size_t used_index, void *kaddr) {
	
    size_t i;
    if (!(swap_block && swap_bitmap)) {
        printf("\nin swap_in no swap block or swap map\n");
        return;

    lock_acquire (&swap_lock);

    for (i=0; i<PGSIZE/BLOCK_SECTOR_SIZE; i++) {
	    block_read (swap_block, 
                    used_index * (PGSIZE/BLOCK_SECTOR_SIZE) + i, 
                    kaddr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);

    /*if (BITMAP_ERROR == bitmap_scan(swap_bitmap, used_index, 1, 1))
		return ERROR;

	void *kaddr = alloc_page();
	bitmap_flip();
*/
    }
}
