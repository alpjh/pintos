


void vm_init (struct hash *vm) {
    /* hash_init()으로 해시테이블 초기화 */
    /* 인자로 해시테이블과 vm_hash_func과 vm_less_func 사용 */
}

static unsigned vm_hash_func (const struct hash_elem *e, void *aux) {
    /* hash_entry()로 element에 대한 vm_entry 구조체 검색 */
    /* hash_int()를 이용해서 vm_entry의 멤버 vaddr에 대한 
       해시값을 구하고 반환 */
}

static bool vm_less_func (const struct hash_elem *a, 
                          const struct hash_elem *b) {
    /* hash_entry()로 각각의 element에 대한 vm_entry 구조체를 얻은 후
       vaddr비교 (b가크다면true, a가크다면false) */
}

bool insert_vme (struct hash *vm, struct vm_entry *vme) {
    /* hash_insert() 함수 사용 */
}

bool delete_vme (struct hash *vm, struct vm_entry *vme) {
    /* hash_delete() 함수 사용 */
}

struct vm_entry *find_vme (void *vaddr) {
    /* pg_round_down()으로 vaddr의 페이지번호를 얻음 */
    /* hash_find() 함수를 사용해서 hash_elem 구조체 얻음 */
    /* 만약 존재하지 않는다면 NULL리턴 */
    /* hash_entry()로 해당 hash_elem의 vm_entry구조체 리턴 */
}

void vm_destroy (struct hash *vm) {
    /* hash_destroy()으로 해시테이블의 버킷리스트와vm_entry들을 제거*/
}

void vm_destroy_func (struct hash_elem *e, void *aux UNUSED) {
    /* Get hash element (hash_entry() 사용) */
    /* load가 되어있는 page의 vm_entry인 경우
       page의 할당 해제 및 page mapping 해제
       (palloc_free_page()와pagedir_clear_page() 사용) */
    /* vm_entry객체 할당 해제*/
}
