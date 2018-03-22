#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void get_argument(void *esp, int *arg, int count) {

    /* Copy inza in UserStack to Kernel */
    esp += 4;
    
    for (i=0; i<count; i++) {
      arg[i] = *(esp+(i*4));       
      check_address(esp+(i*4));
    }
    
    /* If inza in USER*/

}

void check_address() {
    if(! (0x804800000 < addr && addr < 0xc00000000))
        exit(-1);
} // need value soojung


static void
syscall_handler (struct intr_frame *f) 
{
  /*printf ("system call!\n");
  thread_exit (); */
  int syscall_nr = *(int)f->esp;
  int arg[5];
  
  switch(syscall_nr) {
      case SYS_HALT:
        halt();
        break;
      case SYS_EXIT :
        get_argument(esp, arg, 1);
        check_address(arg[0]);
        f->eax = exit(arg[0]); // can go in void return value
        break;
      case SYS_CREATE :
        get_argument(esp, arg, 2);
        check_address(arg[0]);
        f->eax = create(arg[0], arg[1]);
        break;
      case SYS_REMOVE :
        get_argument(esp, arg, 1);
        check_address(arg[0]);
        f->eax = remove(arg[0]);
        break;
  }
  
}
