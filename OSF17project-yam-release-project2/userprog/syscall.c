#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdbool.h>
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include <devices/input.h>
#include "filesys/file.h"

typedef int pid_t;

//for peeking at parts of stack
#define syscallSize 1
#define intSize 1 
#define ptrSize 1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

static void syscall_handler (struct intr_frame *);
static bool isFDValid(int fd);
static bool isAddressValid(const void *pointer);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Terminates Pintos by calling shutdown_power_off() 
(declared in threads/init.h). This should be seldom used, 
because you lose some information about possible deadlock situations, etc.*/
static void 
halt(void){ 
  shutdown_power_off();
}

/*Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status 
that will be returned. Conventionally, a status of 0 indicates success 
and nonzero values indicate errors. */
static void 
exit(int status){ 
  struct thread *cur = thread_current ();

  // Lock parent's list to remove self
  struct thread *parent = cur->parent;
  lock_acquire(&(parent->child_lock));

  // Looking for self on parent's child_list
  struct list_elem *e = list_head(&(parent->child_list));
  while ((e = list_next(e)) != list_end(&(parent->child_list))) {
      struct child *current_child = list_entry(e, struct child, elem);

      // If child_tid, set exit status 
      if (current_child->tid == cur->tid) {
          current_child->exitStatus = status;
      }
  }

  // Unlock parent's list
  lock_release(&(parent->child_lock));

  printf ("%s: exit(%d)\n", thread_name(), status); 
  thread_exit();
}

/*Runs the executable whose name is given in cmd_line, passing any given arguments, 
and returns the new process's program id (pid). Must return pid -1, 
which otherwise should not be a valid pid, if the program cannot load or run for any reason. 
Thus, the parent process cannot return from the exec
until it knows whether the child process successfully loaded its executable.
You must use appropriate synchronization to ensure this.*/
static pid_t
exec(const char *cmd_line){ //todo: add synchronization for parent/child
  if(*cmd_line != NULL){
    return process_execute(cmd_line);
  }
  return -1;
}

/*Waits for a child process pid and retrieves the child's exit status.*/
static int
wait(pid_t pid){
  return process_wait(pid);
}

/*Creates a new file called file initially initial_size bytes in size.
 Returns true if successful, false otherwise. Creating a new file does not open it:
 opening the new file is a separate operation which would require a open system call.*/
static bool 
create (const char *file, unsigned initial_size){
  bool success = false;

  if(file != NULL && isAddressValid(file)){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    success = filesys_create(file, initial_size); 

    lock_release(&(current->child_lock));
  }else{exit(-1);}

  return success;
}

/*Deletes the file called file. Returns true if successful, false otherwise.
A file may be removed regardless of whether it is open or closed,
and removing an open file does not close it. See Removing an Open File, for details.*/
static bool
remove (const char *file){
  struct thread *current = thread_current();
  lock_acquire(&(current->child_lock));

  bool success = filesys_remove(file);

  lock_release(&(current->child_lock));
  return success;
}

/*Opens the file called file. Returns a nonnegative integer handle 
called a "file descriptor" (fd), or -1 if the file could not be opened.*/
static int
open (const char *file){ 
  if(*file == NULL || file == NULL){return -1;}
  struct thread *current = thread_current();
  lock_acquire(&(current->child_lock));
  int index = 2;

  struct file *openedFile = filesys_open(file);
  if(openedFile == NULL){
    return -1;
  }
  struct thread *currentThread = thread_current();
  struct file *next = currentThread->fdt[index]; //skip STDIN/STDOUT
  while(next != NULL){ //what if you keep adding files and dont close?
    index++;
    next = currentThread->fdt[index];
    if(index > 100){
      exit(-1);
    }
  }
  currentThread->fdt[index] = openedFile;

  lock_release(&(current->child_lock));
  return index;
}

/*Returns the size, in bytes, of the file open as fd.*/
static int
filesize (int fd){
  int fileSize = 0;

  if(isFDValid(fd)){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    struct thread *currentThread = thread_current();
    struct file *f = currentThread->fdt[fd];
    fileSize = file_length(f);

    lock_release(&(current->child_lock));
  }

  return fileSize;
}

/*Reads size bytes from the file open as fd into buffer.
 Returns the number of bytes actually read (0 at end of file), 
 or -1 if the file could not be read (due to a condition other than end of file).
 Fd 0 reads from the keyboard using input_getc().*/
static int
read (int fd, void *buffer, unsigned length){
  off_t bytesRead = 0;
  isAddressValid(buffer);
  if(isFDValid(fd) && fd != STDOUT_FILENO){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    if(fd == STDIN_FILENO){
      uint8_t* local_buffer = (uint8_t *) buffer;
      for (unsigned i = 0; i < length; i++){
        local_buffer[i] = input_getc();
        bytesRead++;
      }
    }else{
      struct thread *currentThread = thread_current();
      struct file *f = currentThread->fdt[fd];
      if(f == NULL){
        return -1;
      }
      bytesRead = file_read(f, buffer, length);
    }

    lock_release(&(current->child_lock));
  }

  return bytesRead;
}

/*Writes size bytes from buffer to the open file fd.
  Returns the number of bytes actually written, 
  which may be less than size if some bytes could not be written.*/
static int
write (int fd, const void *buffer, unsigned length){ 
  off_t bytesWritten = 0;
  struct thread *current = thread_current();
  //if(isFDValid(fd) && fd != STDIN_FILENO){
    if(fd == STDOUT_FILENO){
      putbuf(buffer, length);
      bytesWritten = length; //no EOF for console
    }else{
      //stdout test fails if writing to STDOUT is locked
      lock_acquire(&(current->child_lock));
      struct thread *currentThread = thread_current();
      struct file *f = currentThread->fdt[fd];
      bytesWritten = file_write(f, buffer, length);
      lock_release(&(current->child_lock));
    //}
  }

  //lock_release(&(current->child_lock));
  return bytesWritten;
}

/*Changes the next byte to be read or written in open file fd to position,
  expressed in bytes from the beginning of the file. 
  (Thus, a position of 0 is the file's start.)*/
static void
seek (int fd, unsigned position){
  if(isFDValid(fd)){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    struct thread *currentThread = thread_current();
    struct file *f = currentThread->fdt[fd];
    file_seek(f, position);

    lock_release(&(current->child_lock));
  }
}
 
 /*Returns the position of the next byte to be read or written in open file fd,
  expressed in bytes from the beginning of the file.*/
static unsigned 
tell (int fd){
  unsigned nextBytePosition = 0;

  if(isFDValid(fd)){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    struct thread *currentThread = thread_current();
    struct file *f = currentThread->fdt[fd];
    nextBytePosition = file_tell(f);

    lock_release(&(current->child_lock));
  }

  return nextBytePosition;
}

/*Closes file descriptor fd.
 Exiting or terminating a process implicitly closes all its open file descriptors,
  as if by calling this function for each one.*/
static void 
close (int fd){
  if(isFDValid(fd)){
    struct thread *current = thread_current();
    lock_acquire(&(current->child_lock));

    struct thread *currentThread = thread_current();
    struct file *f = currentThread->fdt[fd];
    file_close(f);
    currentThread->fdt[fd] = NULL;

    lock_release(&(current->child_lock));
  }else{}
}

//helper function to ensure FD is not index to null pointer in thread's FDT
static bool
isFDValid(int fd){
  struct thread *currentThread = thread_current();
  if(fd > 100 || fd < 0){
    exit(-1);
  }
  if(currentThread->fdt[fd] != NULL||fd==0||fd==1){
    return true;
  }
  exit(-1);
  return false;
}

//helper function to ensure pointer is in user space, not null, and mapped
static bool
isAddressValid(const void *pointer){
  void *ptr = pagedir_get_page(thread_current()->pagedir, pointer);
  if(!is_user_vaddr(pointer) || (pointer == 0) || !ptr){
      exit(-1);
      return false;
  }
  return true;
}

static void
syscall_handler (struct intr_frame *f UNUSED){
  //peek at system call args from stack -> call corresponding handler
  int *esp = f->esp;
  void* syscall_number_ptr = esp;
  if(!isAddressValid(syscall_number_ptr)){ 
    exit(-1); 
  }

  int syscall_number = *esp;
  void *arg1, *arg2, *arg3;

  switch (syscall_number){
      case SYS_HALT:
          halt();
          break;
      case SYS_EXIT:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){
            f->error_code = *((int*)arg1); //return to kernel?
            exit(*((int*)arg1));
          }
          break;
      case SYS_EXEC:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = exec(*((char**)arg1)); }
          break;
      case SYS_WAIT:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = wait(*(int*)arg1); }
          break;
      case SYS_CREATE:
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + ptrSize;
          if(isAddressValid(arg1) && isAddressValid(arg2)){ f->eax = create(*((char**)arg1), *((unsigned*)arg2)); }
          break;
      case SYS_REMOVE:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = remove(*((char**)arg1)); }
          break;
      case SYS_OPEN:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = open(*((char**)arg1)); }
          break;
      case SYS_FILESIZE:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = filesize(*((int*)arg1)); }
          break;
      case SYS_READ: 
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + intSize;
          arg3 = esp + syscallSize + intSize + ptrSize;
          if(isAddressValid(arg1) && isAddressValid(arg2) && isAddressValid(arg3)){ f->eax = read(*((int*)arg1), *((void**)arg2), *((unsigned*)arg3)); }
          break;
      case SYS_WRITE: 
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + intSize;
          arg3 = esp + syscallSize + intSize + ptrSize;
          isFDValid(*((int*)arg1)); 
          if(isAddressValid(arg1) && isAddressValid(arg2) && isAddressValid(arg3)){ f->eax = write(*((int*)arg1), *((void**)arg2), *((unsigned*)arg3)); }
          break;
      case SYS_SEEK:
          arg1 = esp + syscallSize;
          arg2 = esp + syscallSize + intSize; 
          if(isAddressValid(arg1) && isAddressValid(arg2)){ seek(*((int*)arg1), *((unsigned*)arg2)); }
          break;
      case SYS_TELL:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ f->eax = tell(*((int*)arg1)); }
          break;
      case SYS_CLOSE:
          arg1 = esp + syscallSize;
          if(isAddressValid(arg1)){ close(*((int*)arg1)); }
          break;
      default:
          break;
        }
}
