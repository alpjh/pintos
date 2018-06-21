#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include <string.h>
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <devices/input.h>
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);
struct vm_entry* check_address(void *addr, void* esp);
void get_argument(void *esp, int *arg, int count);
 
void halt (void);
void exit (int status);
int wait(tid_t tid);
tid_t exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

bool sys_isdir(int fd);
bool sys_chdir(const char *dir);
bool sys_mkdir(const char *dir);
int sys_inumber(int fd);
bool sys_readdir(int fd, char *name);

void
syscall_init (void) {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) {

    uint32_t *esp = f->esp;// Get user stack pointer
    check_address((void *)esp, (void *)esp); // 주소값이 유효한지 확인
    int syscall_nr = *esp; 
    int arg[3];
    
    /* System Call switch */
    switch(syscall_nr) {
        //HALT
        case SYS_HALT:
            halt();
            break;
        //EXIT
        case SYS_EXIT :
            get_argument(esp, arg, 1);
            exit(arg[0]);
            f->eax = arg[0];
            break;
        //EXEC
        case SYS_EXEC :
            get_argument(esp, arg, 1);
            /* buffer 사용유무를 고려하여 유효성 검사*/     
            check_valid_string((const void *) arg[0], f->esp);
            f->eax = exec((const char *)arg[0]);
            break;
        //WAIT
        case SYS_WAIT :
            get_argument(esp, arg, 1);
            f->eax = wait(arg[0]);
            break;
        //CREATE
        case SYS_CREATE :
            get_argument(esp, arg, 2);
            //check_address((void *)arg[0]);
            check_valid_string((const void *) arg[0], f->esp);
            f->eax = create((const char *)arg[0], arg[1]);
            break;
        //REMOVE
        case SYS_REMOVE :
            get_argument(esp, arg, 1);
            //check_address((void *)arg[0]);
            check_valid_string((const void *) arg[0], f->esp);
            f->eax = remove((const char *)arg[0]);
            break;
        //OPEN
        case SYS_OPEN :
            get_argument(esp, arg, 1);
            /* buffer 사용유무를 고려하여 유효성 검사 */
            check_valid_string((const void *)arg[0], f->esp);
            f->eax = open((const char *)arg[0]);
            break;
        //FILESIZE
        case SYS_FILESIZE :
            get_argument(esp, arg, 1);
            f->eax = filesize(arg[0]);
            break;
        //READ
        case SYS_READ :
            get_argument(esp, arg , 3);
            //check_address((void *)arg[1]);
            check_valid_buffer ((void *)arg[1], (unsigned)arg[2], 
                                f->esp, true);
            f -> eax = read(arg[0] , (void *)arg[1] , (unsigned)arg[2]);
            break;
        //WRITE
        case SYS_WRITE :
            get_argument(esp, arg , 3);
            //check_address((void *)arg[1]);
            /* buffer 사용 유무를 고려하여 유효성 검사 */
            check_valid_buffer((void *) arg[1], (unsigned) arg[2], 
                               f->esp,  false);
            f -> eax = write(arg[0] , (const void *)arg[1] , 
                             (unsigned)arg[2]);
            break;
        //SEEK
        case SYS_SEEK :
            get_argument(esp , arg , 2);
            seek(arg[0] , (unsigned) arg[1]);
            break;
            //TELL
        case SYS_TELL :
            get_argument(esp , arg , 1);
            f -> eax = tell(arg[0]);
            break;
            //CLOSE
        case SYS_CLOSE :
            get_argument(esp , arg , 1);
            close(arg[0]);  
            break;
        case SYS_MMAP:
            get_argument(esp, arg, 2);
            f->eax =mmap(arg[0], (void*)arg[1]);
            break;
        case SYS_MUNMAP:
            get_argument(esp, arg, 1);
            munmap(arg[0]);
            break;
        case SYS_CHDIR:
            get_argument(esp , arg , 1);
            check_address(arg[0], esp);
            f -> eax = sys_chdir((const char *)arg[0]);
            break;

        case SYS_MKDIR:
            get_argument(esp , arg , 1);
            check_address(arg[0], esp);
            f -> eax = sys_mkdir((const char *)arg[0]);
            break;

        case SYS_READDIR:
            get_argument(esp , arg , 2);
            check_address(arg[1], esp);
            f -> eax = sys_readdir(arg[0], (char *)arg[1]);
            break;

        case SYS_ISDIR:
            get_argument(esp , arg , 1);
            f -> eax = sys_isdir(arg[0]);
            break;

        case SYS_INUMBER:
            get_argument(esp , arg , 1);
            f -> eax = sys_inumber(arg[0]);
            break;
        //NOT SYSCALL
        default :
            exit(-1);
    }  
}

/* Check if address point user domain */
struct vm_entry* check_address(void *addr, void *esp) {
    if(addr < (void *)0x08048000 || addr >= (void *)0xc0000000) {
        // user domain
        exit(-1);
    }
    /*addr이vm_entry에존재하면vm_entry를반환하도록코드작성*/
    struct vm_entry* vme;
    /*find_vme() 사용*/
    if (vme = find_vme((void*)addr)) {
        //handle page fault
        handle_mm_fault(vme);
        //handle fault and return vme
        return vme;
    }
    //if no NULL, 
    return NULL; 
}

/* Copy Value in UserStack to Kernel */    
void get_argument(void *esp, int *arg, int count) {

    int i; //For loop
    int *ptr; //temp pointer
    esp += 4;//stack pointer

    for (i=0; i<count; i++) {
        ptr = (int *)esp + i; //Copy to temp pointer       
        check_address(esp+(i*4), esp); //Check address
        arg[i] = *ptr; //Copy to Kernel
    }

}

void halt(void) {
    shutdown_power_off(); //핀토스 종료
}

void exit(int status) {
    struct thread *current = thread_current(); //실행중인 스레드 구조체 정보
    /* 프로세스 디스크립터에 exit status 저장 */
    current -> exit_status = status;
    printf("%s: exit(%d)\n", current->name, status); // 스레드 이름과 exit status 출력
    thread_exit(); // 스레드 종료
}

tid_t exec(const char *cmd_line) {
    
    //Process create
    tid_t tid;
    if((tid = process_execute (cmd_line)) == TID_ERROR)
        return TID_ERROR;

    //Find child process
    struct thread *child;
    if ( !( child= get_child_process(tid) ) )
        return -1;

    //wait for child process tapjae
    sema_down(&child->load_sema);
    //if load success, return pid
    if (child->loaded) 
        return tid;
    
    //if load fail, return -1
    else
        return -1;

}

//Call process_wait
int wait(tid_t tid) {
    return process_wait(tid);
}

bool create(const char *file, unsigned initial_size) {
    bool success = filesys_create(file, initial_size); // 파일이름과 크기
    return success; //파일 생성에 성공하면 true 리턴
}

bool remove(const char *file) {
    bool success = filesys_remove(file); // 파일 이름에 해당하는 파일 제거
    return success; //파일 제거에 성공하면 true리턴 
}

int open(const char *file){

    int fd = -1;
    //Lock acquire
    lock_acquire(&filesys_lock);
   
    //IF add fail, fd = -1
    fd = process_add_file(filesys_open(file));

    //Lock release
    lock_release(&filesys_lock);
    return fd;
}

int filesize(int fd){
    struct file *f;
    //Get file
    if (!(f = process_get_file(fd)))
        return -1;
    //Use file_length in file.c
    return file_length(f);
}  //open이 되었다는 가정 하에 진행

int read(int fd, void *buffer, unsigned size){

    struct file *f;
    
    //Lock acquire
    lock_acquire (&filesys_lock);

    if (fd == 0) {    
        unsigned count = size;
        //Loop for size
        while (count--)
            *((char *)buffer++) = input_getc();
        //Lock release
        lock_release(&filesys_lock);
        return size;
    } // fd값이 0인 파일은 파일크기가 없음. 키보드로부터 데이터를 읽어오는 >동작을 추가해줘야함

    //If NULL file, return -1
    if((f = process_get_file(fd)) == NULL) {
        //Lock release
        lock_release(&filesys_lock);
        return -1;
    }

    //Get size
    size = file_read(f, buffer, size);

    //Lock release
    lock_release(&filesys_lock);
    return size;

}

int write (int fd, void *buffer, unsigned size) {

    //Lock acquire
    if (fd == 1) {
        putbuf(buffer,size); //파일디스크립터가 1일 경우 버퍼에 저장된 갑승ㄹ 화면에 출력하고 버퍼의 크기를 리턴
        //Lock release
        return size;
    }

    lock_acquire(&filesys_lock);
   //lock_acquire(&filesys_lock);
    struct file *f;

    //Get file, if NULL, retuen -1 
    if (!(f = process_get_file(fd))) {
        //Lock release
        lock_release(&filesys_lock);
        return -1;
    }
    if (inode_is_dir (file_get_inode(f))) {
        lock_release(&filesys_lock);
        return -1;
    }

    //Write and get size
    int bytesize = file_write(f, buffer, size);

    //Lock release
    lock_release(&filesys_lock);
    return bytesize;
}

void seek (int fd, unsigned position) {
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);

    if(!f){
        return;
    }

    file_seek(f, position); //열린 파일의 위치를 position만큼 이동
    lock_release(&filesys_lock);
}

unsigned tell (int fd) {
    struct file *f = process_get_file(fd);
    if(!f){
        return -1;
    }

    off_t offset = file_tell(f);
    return offset; //열린 파일의 위치 리턴
}

void close (int fd) {
    process_close_file(fd); //파일 닫음
}

void check_valid_buffer (void *buffer, unsigned size, void *esp, 
                         bool to_write) {
    unsigned i;
    char *local_buffer = (char *)buffer;
    
    /* 인자로 받은 buffer부터 buffer + size까지의 크기가 한페이지의 
       크기를 넘을 수 도 있음 */
    //Use for loop
    for (i=0; i<size; i++) {
        /* check_address를 이용해서 주소의 유저영역여부를 검사함과 동시에 
           vm_entry구조체를 얻음 */
        struct vm_entry *vme = check_address ((const void*)local_buffer, esp);
        /* 해당 주소에 대한 vm_entry 존재여부와 vm_entry의 writable멤버가
           true인지 검사 */
        if (vme && to_write) {
            if (!vme->writable) {
                exit(-1);
            }
        }
        local_buffer++;
    }
}

void check_valid_string (const void *str, void *esp) {
    /* str에 대한 vm_entry의 존재여부를 확인 */
    //if no vm_entry in str, check address return NULL
    /*if (!check_address (str, esp))
        return;*/
    check_address (str, esp);
    /* check_address()사용 */
    //for all str. check address. 
    while (*(char *)str != 0) {
        //start from str 1. str0 did it up line. 
        str = (char *) str + 1;
        check_address(str, esp);
    }
}

//mmapid를 할당해줄 때 중간에 숫자가 비어있을 경우 ++하면서 그냥 할당할것인가 아니면 비어있는 숫자를 할당해줄 것인가의 문제가 존재함. 

//mapid_t라는 유형은 없으므로 그냥 int 등 사용하면 됨..

int mmap(int fd, void* addr) {
	
    struct file *f = process_get_file(fd);
	struct file *rf; //reopen할 파일
	struct mmap_file *mf;
	static int mapid = 0; //mmap시스템콜 안에서만 쓰이
    
    int read_bytes;
    int offset = 0;

    struct thread* t = thread_current();

	if (!f || !is_user_vaddr(addr) || addr == 0 || 
            (uint32_t)addr % PGSIZE != 0 || addr < 0 ) {
		return -1; //잘못된 인자
	}
	
	rf = file_reopen(f);
    
    if (!rf || file_length (rf) == 0)
        return -1;

	read_bytes = file_length(rf);

	mf = malloc(sizeof(struct mmap_file));
	mf->mapid = mapid++;
    list_init(&mf->vme_list);
	mf->file = rf;
	
	while (read_bytes > 0) {
        struct vm_entry *vme;
        
        //if already exist, return error
        if (find_vme(addr))
            return -1;

		uint32_t page_read_bytes = 
                        read_bytes < PGSIZE ? read_bytes : PGSIZE;
        uint32_t page_zero_bytes = PGSIZE - page_read_bytes;
                        
        vme = malloc(sizeof(struct vm_entry));
        
        //if no memory, return error
        if(!vme)
            return -1;

        //vme 내용들 초기화;
		vme->type = VM_FILE;
        vme->file = rf;
		vme->offset = offset;
		vme->read_bytes = page_read_bytes;
		vme->zero_bytes = page_zero_bytes;
        vme->writable = true;
        vme->is_loaded = false;
        vme->vaddr = addr;

        //input mmap elem to vme list 
        list_push_back(&mf->vme_list, &vme->mmap_elem);
        //insert vme to thread
        insert_vme (&t->vm, vme);

        //control values
        read_bytes -= page_read_bytes;
        offset += page_read_bytes;
        addr += PGSIZE;
    }
    
    //push element to mmaplist 
    list_push_back (&t->mmap_list, &mf->elem);
    return mf->mapid;
}

//Deleted mmap
void do_munmap (struct mmap_file* mmap_file) {
    
    struct thread* t = thread_current();
    struct list_elem *next, *e;
    
    struct file *f = mmap_file->file;

    //e = first elem of vme_list
    e = list_begin(&mmap_file->vme_list);   
    //remove, find circular
    
    //for list's end 
    while (e!=list_end(&mmap_file->vme_list)) {
        next = list_next(e);

        struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);

        //if loaded vme? 
        if(vme->is_loaded) {
            //Look dirty or not 
            if (pagedir_is_dirty(t->pagedir, vme->vaddr)) {
                lock_acquire(&filesys_lock);
                file_write_at(vme->file, vme->vaddr, 
                              vme->read_bytes, vme->offset);
                lock_release(&filesys_lock);
            }
            //Free Page
            palloc_free_page(pagedir_get_page(t->pagedir, vme->vaddr));
            pagedir_clear_page(t->pagedir, vme->vaddr);
        }   

        //remove element at list
        list_remove(&vme->mmap_elem);
        delete_vme(&t->vm, vme);

        free(vme);

        e = next;
    }
    if (f) {
        //If file exist , close file.
        lock_acquire(&filesys_lock);
        file_close(f);
        lock_release(&filesys_lock);
    }
}

void munmap (int mapid) {   
    struct thread* t = thread_current();
    struct list_elem *e = list_begin(&t->mmap_list);
    struct list_elem *next;

    while(e != list_end(&t->mmap_list)) {

        struct mmap_file* mmap_file = list_entry(e, struct mmap_file, elem);

        //choice mmapfile and delete, if -1 -> close all mmap
        if (mmap_file->mapid == mapid || mapid == -1) {
            // unmap corresponding file 
            do_munmap(mmap_file);
            list_remove(&mmap_file->elem);
            //free memory
            free(mmap_file);
            if(mapid != -1)
                break;
        }
        //printf("\nbefore list_next\n");
        e = list_next(&t->mmap_list);

    }   
}

bool sys_isdir(int fd) {   
    struct file *p;

    if (p = process_get_file(fd))
        return inode_is_dir(file_get_inode(p));
    else
        return false;

}
bool sys_chdir (const char *dir) {
    struct file *p;

    if(!(p = filesys_open(dir)))
        return false;

    /* dir경로를 분석하여 디렉터리를 반환 */
    struct inode *inode = inode_reopen(file_get_inode(p));
    struct dir *p_dir = dir_open(inode);

    file_close(p);

    if(!p_dir)
        return false;

    dir_close(thread_current()->cur_dir);
    /* 스레드의 현재 작업 디렉터리를 변경 */
    thread_current()->cur_dir = p_dir;
    return true;
}

bool sys_mkdir(const char *dir) {
    return filesys_create_dir (dir);
}


int sys_inumber (int fd) {
    struct file *p;
    //fd리스트에서 fd에 대한 file정보를 얻어옴
    if(p = process_get_file(fd)) //fd의 on-disk inode블록주소를 반환
        return (uint32_t)inode_get_inumber(file_get_inode(p));
    else
        return -1;
}

//dir->pos엔트리를 읽어 name에 파일이름 저장
bool sys_readdir(int fd, char *name) {
    
    //need lock
    lock_acquire(&filesys_lock);

    /* fd리스트에서 fd에대한 file정보를 얻어옴 */
    struct file *p = process_get_file(fd);
    /* fd의 file->inode가 디렉터리인지 검사 */
    if (!p && !inode_is_dir(file_get_inode(p))) {
        lock_release(&filesys_lock);
        return false; 
    }

    /* p_file을 dir자료구조로 포인팅 */
    struct dir *p_dir = (struct dir*)p;

    bool success = false;

    do {
        success = dir_readdir(p_dir, name);
    }/* 디렉터리의엔트에서 “.”, ”..” 이름을 제외한 파일이름을 name에 저장 */
    while (success && 
           (strcmp(name,".") == 0 || strcmp(name,"..") == 0));

    lock_release(&filesys_lock);
    return success;
}



