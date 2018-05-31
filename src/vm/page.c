#include <string.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include "vm/page.h"


void vm_init (struct hash *vm);
bool insert_vme (struct hash *vm, struct vm_entry *vme);
bool delete_vme (struct hash *vm, struct vm_entry *vme);
struct vm_entry* find_vme (void *vaddr);
void vm_destroy (struct hash *vm);
bool load_file (void *kaddr, struct vm_entry *vme);

static unsigned vm_hash_func (const struct hash_elem *e, void *aux) {

    struct vm_entry *vme;
    
    /* hash_entry()로 element에 대한 vm_entry 구조체 검색 */
    vme = hash_entry (e, struct vm_entry, elem);
    
    /* hash_int()를 이용해서 vm_entry의 멤버 vaddr에 대한 
       해시값을 구하고 반환 */
    return hash_int ((int)vme->vaddr);
}

static bool vm_less_func (const struct hash_elem *a, 
                          const struct hash_elem *b) {
    /* hash_entry()로 각각의 element에 대한 vm_entry 구조체 */
    struct vm_entry *vme_a = hash_entry (a, struct vm_entry, elem);
    struct vm_entry *vme_b = hash_entry (b, struct vm_entry, elem);
    
    /*   vaddr비교 (b가크다면true, a가크다면false) */
    if (vme_a->vaddr < vme_b->vaddr)
        return true;
    
    return false;
}
static void vm_destroy_func (struct hash_elem *e, void *aux UNUSED) {
    
    /* Get hash element (hash_entry() 사용) */
    struct vm_entry *vme = hash_entry (e, struct vm_entry, elem);
    
    /* load가 되어있는 page의 vm_entry인 경우 */
    if (vme->is_loaded) {
      /* page의 할당 해제 및 page mapping 해제 */
      /* (palloc_free_page()와pagedir_clear_page() 사용) */ 
      palloc_free_page(pagedir_get_page (thread_current()->pagedir,
                                         vme->vaddr)); 
      pagedir_clear_page (thread_current()->pagedir, vme->vaddr); 
    }

    /* vm_entry객체 할당 해제*/
    free (vme);
}
void vm_init (struct hash *vm) {
    /* hash_init()으로 해시테이블 초기화 */
    /* 인자로 해시테이블과 vm_hash_func과 vm_less_func 사용 */
    hash_init (vm, vm_hash_func, vm_less_func, NULL);
}



bool insert_vme (struct hash *vm, struct vm_entry *vme) {
    bool return_value = false;
    /* hash_insert() 함수 사용 */
    return_value = (hash_insert (vm, &vme->elem) == NULL);
    return return_value;
}

bool delete_vme (struct hash *vm, struct vm_entry *vme) {
    /* hash_delete() 함수 사용 */
    hash_delete (vm, &vme->elem);
}

struct vm_entry* find_vme (void *vaddr) {
    
    struct vm_entry vme;
    struct hash_elem *e;

    /* pg_round_down()으로 vaddr의 페이지번호를 얻음 */
    vme.vaddr = pg_round_down (vaddr);
    /* hash_find() 함수를 사용해서 hash_elem 구조체 얻음 */
    if  (e = hash_find (&thread_current()->vm, &vme.elem)) {
        /* hash_entry()로 해당 hash_elem의 vm_entry구조체 리턴 */
        return hash_entry (e, struct vm_entry, elem);
    }
    /* 만약 존재하지 않는다면 NULL리턴 */
    return NULL;
}

void vm_destroy (struct hash *vm) {
    /* hash_destroy()으로 해시테이블의 버킷리스트와vm_entry들을 제거*/
    hash_destroy (vm, vm_destroy_func);
}



bool load_file (void *kaddr, struct vm_entry *vme) {
    
    bool file_read_result = false;
    /*Using file_read_at()*/

    /* file_read_at으로물리페이지에read_bytes만큼데이터를씀*/
    /* file_read_at여부반환*/
    if (vme->read_bytes > 0) {
        if ((int)vme->read_bytes != file_read_at(vme->file, 
                                                kaddr, 
                                                vme->read_bytes, 
                                                vme->offset)) {
            return file_read_result;
        }
        memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
    }
    /* zero_bytes만큼남는부분을‘0’으로패딩*/
    else
        memset(kaddr, 0, PGSIZE);
    /*정상적으로file을메모리에loading 하면true 리턴*/
    file_read_result = true;
    return file_read_result;
}
