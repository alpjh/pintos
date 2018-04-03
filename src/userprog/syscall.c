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

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

void
syscall_init (void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

int exec(const char *cmd_line) {

    //Process create
    int pid = process_exequte(cmd_line);

    //Find child process
    struct thread *child = get_child_process(pid);

    //wait for child process tapjae
    sema_down(&child->load_sema);
    
    //if load success, return pid
    if (child->loaded) 
        return pid;
    //if load fail, return -1
    else
        return -1;

}

static void
syscall_handler (struct intr_frame *f) {

  uint32_t *esp = f->esp;// Get user stack pointer
  check_address((void *)esp); // 주소값이 유효한지 확인
  int syscall_nr = *esp; 
  int arg[5];

  printf("system call number : %d\n", syscall_nr);
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
      //CREATE
      case SYS_CREATE :
        get_argument(esp, arg, 2);
        f->eax = create((const char *)arg[0], (const char *)arg[1]);
        break;
      //REMOVE
      case SYS_REMOVE :
        get_argument(esp, arg, 1);
        f->eax = remove((const char *)arg[0]);
        break;
      case SYS_EXEC :
        get_argument(esp, arg, 1);
        f->eas = exec((const char *)arg[0]);
        break;
      defaule :
        printf("Not system call! \n");
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
	printf("%s: exit(%d)\n", current->name, status); // 스레드 이름과 exit status 출력
	thread_exit(); // 스레드 종료
}

bool create(const char *file, unsigned initial_size) {
	bool success = filesys_create(file, initial_size); // 파일이름과 크기
	return success; //파일 생성에 성공하면 true 리턴
}

bool remove(const char *file) {
	bool success = filesys_remove(file); // 파일 이름에 해당하는 파일 제거
	return success; //파일 제거에 성공하면 true리턴 
}

int wait(int pid) {
    return process_wait(pid);
}
