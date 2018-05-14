#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

struct vm_entry{

    uint8_t type; /* VM_BIN, VM_FILE, VM_ANON의타입*/
    void *vaddr; /* vm_entry의가상페이지번호*/
    bool writable; /* True일경우해당주소에write 가능
                      False일경우해당주소에write 불가능*/
    bool is_loaded; /* 물리메모리의탑재여부를알려주는플래그*/

    struct file* file;/* 가상주소와맵핑된파일*/

    /* Memory Mapped File 에서다룰예정*/
    struct list_elem mmap_elem; /* mmap리스트element */
    size_t offset;/* 읽어야할파일오프셋*/
    size_t read_bytes;/* 가상페이지에쓰여져있는데이터크기*/
    size_t zero_bytes;/* 0으로채울남은페이지의바이트*/

    /* Swapping 과제에서다룰예정*/
    size_t swap_slot; /* 스왑슬롯*/

    /* ‘vm_entry들을위한자료구조’ 부분에서다룰예정*/
    struct hash_elem elem; /*해시테이블Element */
}
