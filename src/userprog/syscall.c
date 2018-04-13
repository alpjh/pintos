#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>

static void syscall_handler (struct intr_frame *);
void check_address(void *addr);
void get_argument(void *esp, int *arg, int count);

void halt (void);
void exit (int status);
int wait(tid_t tid);
tid_t exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);

void
syscall_init (void) {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
}

//실습내용 sudo
int open(const char *file){
    struct fild *f = filesys_open(file);

    int fd;

    //filesys_open fail
    if (!f)
        return -1;
    
    fd = process_add_file(f);
    //process_add_file 실패 경우도 생각해야함
    return fd;
}

int filesize(int fd){
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f){
        lock_release(&filesys_lock);
        return -1;
    }
    int size = file_length(f);
    lock_release(&filesys_lock);
    return size;
}  //open이 되었다는 가정 하에 진행

int read(int fd, void *buffer, unsigned size){

    if (fd == 1)
        return -1;

    if (fd == 0){
        unsigned i;
        uint8_t* localbuf = (uint8_t*) buffer;
        for (i = 0; i < size; i++){
            localbuf[i] = input_getc();
        }
        return size;
    } // fd값이 0인 파일은 파일크기가 없음. 키보드로부터 데이터를 읽어오는 >동작을 추가해줘야함


    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd); 
    if (!f) {
        lock_release(&filesys_lock);
        return -1;
    }

    int bytesize = file_read(f, buffer, size);

    lock_release(&filesys_lock);
    return bytesize;
}

int write(int fd, void *buffer, unsigned size){

    if (fd == 1) {
        putbuf(buffer,size); //파일디스크립터가 1일 경우 버퍼에 저장된 갑승ㄹ 화면에 출력하고 버퍼의 크기를 리턴
        return size;
    }

    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);

    if (!f){
        lock_release(&filesys_lock);
        return -1;
    }

    int bytesize = file_write(f, buffer, size);

    lock_release(&filesys_lock);
    return bytesize;
}

////////////////////실습

void seek (int fd, unsigned position){
    struct file *f = process_get_file(fd);

    if(!f){
        return;
    }

    file_seek(f, position); //열린 파일의 위치를 position만큼 이동
}

unsigned tell (int fd){
    struct file *f = process_get_file(fd);
    if(!f){
        return -1;
    }

    off_t offset = file_tell(f);
    return offset; //열린 파일의 위치 리턴
}

void close (int fd){
    process_close_file(fd); //파일 닫음
}

static void
syscall_handler (struct intr_frame *f) {

    uint32_t *esp = f->esp;// Get user stack pointer
    check_address((void *)esp); // 주소값이 유효한지 확인
    int syscall_nr = *esp; 
    int arg[10];
/*
    printf("%d", thread_current);
    printf("system call number : %d\n", syscall_nr);
    */
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
            f->eax = (arg[0]);
            break;
        case SYS_EXEC :
            get_argument(esp, arg, 1);
            check_address((void *) arg[0]);
            f->eax = exec((const char *)arg[0]);
            break;
        case SYS_WAIT :
            get_argument(esp, arg, 1);
            f->eax = wait(arg[0]);
            break;
            //CREATE
        case SYS_CREATE :
            get_argument(esp, arg, 2);
            check_address(arg[0]);
            f->eax = create((const char *)arg[0], (const char *)arg[1]);
            break;
            //REMOVE
        case SYS_REMOVE :
            get_argument(esp, arg, 1);
            check_address(arg[0]);
            f->eax = remove((const char *)arg[0]);
            break;
        case SYS_OPEN :
            get_argument(esp, arg, 1);
            check_address(arg[0]);                
            f->eax = open((const char *)arg[0]);
            break;
        case SYS_FILESIZE :
            get_argument(esp, arg, 1);
            f->eax = filesize(arg[0]);
            break;
        case SYS_READ :
            get_argument(esp, arg , 3);
            check_address(arg[1]);
            f -> eax = read(arg[0] , (void *)arg[1] , (unsigned)arg[2]);
            break;
        case SYS_WRITE :
            get_argument(esp, arg , 3);
            check_address(arg[1]);
            f -> eax = write(arg[0] , (const void *)arg[1] , (unsigned)arg[2]);
            break;
        case SYS_SEEK :
            get_argument(esp , arg , 2);
            seek(arg[0] , (unsigned) arg[1]);
            break;

        case SYS_TELL :
            get_argument(esp , arg , 1);
            f -> eax = tell(arg[0]);
            break;

        case SYS_CLOSE :
            get_argument(esp , arg , 1);
            close(arg[0]);  
            break;
        default :
             // printf("Not system call! \n");
              thread_exit();
    }  
}

/* Check if address point user domain */
void check_address(void *addr) {
    if(!(0x08048000 < addr && addr < 0xc0000000)) // user domain
        exit(-1);
}

/* Copy Value in UserStack to Kernel */    
void get_argument(void *esp, int *arg, int count) {

    int i;
    int *ptr; //temp pointer
    esp += 4;

    for (i=0; i<count; i++) {
        ptr = (int *)esp + i; //Copy to temp pointer       
        check_address(esp+(i*4)); //Check address
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
    tid_t tid = process_execute(cmd_line);

    //Find child process
    struct thread *child = get_child_process(tid);

    if(!child)
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

