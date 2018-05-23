#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void argument_stack(char **parse, int count, void **esp);

int process_add_file(struct file *f);
struct file *process_get_file (int fd);
void process_close_file(int fd);

struct thread *get_child_process (int pid); 
void child_process (struct thread *cp);
void remove_child_process (struct thread *cp);
int count_token(const char* str);


#endif /* userprog/process.h */
