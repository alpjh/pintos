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

static void
syscall_handler (struct intr_frame *f) {
  /*printf ("system call!\n");
  thread_exit (); */
  int *esp = f->esp;
  check_address(esp); // 주소값이 유효한지 확인
  int syscall_nr = *esp; 
  int arg[5];

  switch(syscall_nr) {
      case SYS_HALT:
        halt();
        break;
      case SYS_EXIT :
        get_argument(esp, arg, 1);
        check_address(arg[0]);
        exit(arg[0]);
        f->eax =(arg[0]); // can go in void return value
        break;
      case SYS_CREATE :
        get_argument(esp, arg, 2);
        check_address(arg[0]);
        f->eax = create(arg[0], arg[1]);
        break;
      case SYS_REMOVE :
        get_argument(esp, arg, 1);
        check_address(arg[0]);
        f->eax = remove((const char *)arg[0]);
        break;
  }  
}

/* Check address of pointer point user domain */
void check_address(void *addr) {
    if(!(0x80480000 < addr && addr <0xc0000000))
        exit(-1);
}

/* Copy Value in UserStack to Kernel */    
void get_argument(void *esp, int *arg, int count) {
    
    int i;
    int *ptr;
    esp += 4;

    for (i=0; i<count; i++) {
        ptr = (int *)esp + i;       
        check_address(esp+(i*4));
        arg[i] = *ptr;
    }

}

void halt(void) {
	shutdown_power_off(); //핀토스 종료
}

void exit(int status) {
	struct thread *current = thread_current(); //실행중인 스레드 구조체 정보	
    //if(thread_alive(current->parent)
    //    current->exit_status = status;
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

