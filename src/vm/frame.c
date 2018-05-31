#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"


/* LRU list 를 초기화 */
void lru_list_init (void) {
    list_init (&lru_list);
    lock_init (&lru_list_lock);
    lru_clock = NULL;
}

/* LRU list의 끝에 유저 페이지 삽입 */
void add_page_to_lru_list (struct page* page) {
    
    lock_acquire (&lru_list_lock);
    
    //Insert back
    list_push_back (&lru_list, &page->lru);
    
    lock_release (&lru_list_lock);
}

/* LRU list에 유저 페이지 제거 */
void del_page_from_lru_list (struct page* page) {       
    //use list function
    list_remove(&page->lru);        
}

/* 페이지 할당 */
struct page* alloc_page (enum palloc_flags flags) {
 
    //palloc_get_page()를 통해 페이지 할당
    void *kaddr = palloc_get_page (flags); 
    
    //Not alloc -> free page 
    while (!kaddr) {         
        lock_acquire(&lru_list_lock);
        kaddr=try_to_free_pages(flags); 
        lock_release(&lru_list_lock);               
    }
    //Not alloc2 -> PANIC
    ASSERT(kaddr);

    //page 구조체를 할당, 초기화
    struct page *page;
    if (page = malloc (sizeof(struct page)) )
        return NULL;


    //Insert page
    page->kaddr = kaddr;
    page ->thread = thread_current();
    page->vme = NULL;
    //add_page_to_lru_list()를 통해 LRU리스트에 page구조체 삽입
    add_page_to_lru_list(page);
    
    //page구조체의 주소를 리턴
    return page;    
}

/* LRU list 리스트 내 page 해제 */
void __free_page (struct page* page) {

    lock_acquire(&lru_list_lock);
    //Use del_page func
    //LRU리스트의 제거
    del_page_from_lru_list(page);
    
    lock_release(&lru_list_lock);
    
    //Free page.
    //Page구조체에 할당받은 메모리 공간을 해제
    palloc_free_page(page->kaddr);

    free(page);
}

/* 물리주소 kaddr에 해당하는 page 해제 */
void free_page (void *kaddr) {
    
    struct list_elem *e;        
    
    e = list_begin (&lru_list);

    while (e != list_end (&lru_list)) {
        
        struct page *page = list_entry(e, struct page, lru);
        //매치하는 항목을 찾으면 __free_page() 호출
        if (page->kaddr == kaddr) {
            __free_page(page);          
            break;
        }
        e = list_next(e);
    }

}

/* LRU list 리스트의 next 리스트를 반환 */
//Clock 알고리즘의 LRU리스트를 이동하는 작업 수행
static struct list_elem* get_next_lru_clock() {
    //Declare next of argument
    struct list_elem* next = list_next(lru_clock);

    //Clock 알고리즘의 LRU리스트를 이동하는 작업 수행
    if(next == list_end(&lru_list))
        next = list_begin(&lru_list);

    //현재 LRU리스트가 마지막 노드일때 NULL값을 반환
    if(next == list_end(&lru_list))
        next = NULL;

    //LRU리스트의 다음 노드의 위치를 반환
    return next;
}



void* try_to_free_pages (enum palloc_flags flags) {  

    struct list_elem *e;

    //If empty return NULL
    if (list_empty(&lru_list)) {
        lru_clock = NULL;
        return NULL;
    }  
    //If not lru_clock
    if (!lru_clock) 
        lru_clock = list_begin (&lru_list);
    //Use lru algorithm
    while (lru_clock) {       
        struct list_elem *next = get_next_lru_clock();  
        struct page *page = list_entry(lru_clock, struct page, lru);

        if (pagedir_is_accessed (t->pagedir, page->vme->vaddr)) {
            pagedir_set_accessed(t->pagedir, 
                    page->vme->vaddr, false);
        } else {
            //Initialize Page to replace
            page->vme->is_loaded = false;
            //Delete from lru list
            del_page_from_lru_list(page);
            //clear page
            pagedir_clear_page(t->pagedir, page->vme->vaddr);
            palloc_free_page(page->kaddr);
            free(page);
            //return new page
            return palloc_get_page(flags);
        }

        lru_clock = next;
    }
}

