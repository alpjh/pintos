/*
alloc_page(flag){

	while (palloc_get_page == NULL)
		swap_out();

	struct page *page = malloc():
	page->paddr = …;
…

	list_insert(lru_list, page->lru)

	return addr;
}
//함수의 위치는 어디인지 모르겠어요....

*/

void swap_init(){
	struct block *swap_block = block_get_role(BLOCK_SWAP);

	//전역 변수
	struct bitmap *swap_bitmap = bitmap_create(스왑 파티션 크기(block_size라는 함수 이용, 섹터단위로 크기를 반환하므로 계산해줘야함)*512 / 4096);

}


//alloc_page()함수에서 palloc_get_page()가 NULL을 반환했을 경우 쓰임
size_t swap_out() {
//스왑파티션에 접근할수 있어야하고 어느번째위치에 저장해야하는지 알 수 있어야함
//swap_bitmap에서 값이 0인 자리를 찾아야함

	int index = bitmap_scan_and_flip(swap_bitmap, 0);
	block_write(swap_block, index * 4096 / 512, 4096 / 512);

	vme->swap_slot = index;
}


//page_fault함수 발생시 vme는 있는데 present bit 가 0일때 쓰임
swap_in(size_t used_index, void *kaddr){
	if (BITMAP_ERROR == bitmap_scan(swap_bitmap, used_index, 1, 1))
		return ERROR;

	void *kaddr = alloc_page();
	block_read(swap_block, used_index*4096/512, 4096/512);
	bitmap_flip();
}
