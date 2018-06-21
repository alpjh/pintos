#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

struct lock file_sys_lock;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  bc_init();
  inode_init ();
  lock_init(&file_sys_lock);
  free_map_init ();


  if (format) 
    do_format ();

  free_map_open ();
  /* struct thread에서 추가한 필드를 root 디렉터리로 설정 */
  thread_current() -> cur_dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool //root 디렉터리에 파일생성을 name경로에 파일생성하도록 변경
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;

  /* name의 파일경로를 cp_name에 복사 */
  int name_len = strlen(name) + 1;
  char *cp_name = malloc(name_len);
  if (!cp_name)
      return false;
  strlcpy (cp_name, name, name_len);

  /* cp_name의 경로 분석 */
  struct dir *dir;
  char file_name[NAME_MAX + 1];
  dir = parse_path (cp_name, file_name);
  free (cp_name);

  if (!dir) {
      return false;
  }
  if (inode_is_removed(dir_get_inode(dir))) {
      return NULL;
  }

  lock_acquire (&file_sys_lock);

  /* inode의is_dir값설정*/
  /* 추가되는디렉터리엔트리의이름을file_name으로수정*/
  bool success = (dir != NULL
          && free_map_allocate (1, &inode_sector)
          && inode_create (inode_sector, initial_size, 0)
          && dir_add (dir, file_name, inode_sector));

  if (!success && inode_sector != 0) 
      free_map_release (inode_sector, 1);

  dir_close (dir);

  lock_release(&file_sys_lock);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  int name_len = strlen (name) + 1;
  char *cp_name = malloc (name_len);
  if (!cp_name)
      return false;
  strlcpy (cp_name, name, name_len);

  struct dir *dir;
  char file_name[NAME_MAX + 1];
  dir = parse_path(cp_name, file_name);
  free(cp_name);

  if (!dir) {
      return false;
  }
  if (inode_is_removed(dir_get_inode(dir))) {
      return NULL;
  }
  
  lock_acquire(&file_sys_lock);

  struct inode *inode = NULL;

  if (dir != NULL)
      dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  struct file *file = file_open (inode);
  lock_release(&file_sys_lock);
  return file;
}


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
    int name_len = strlen(name) + 1;
    char *cp_name = malloc(name_len);
    if(cp_name == NULL)
        return false;
    strlcpy(cp_name, name, name_len);

    struct dir *dir;
    char file_name[NAME_MAX + 1];
    dir = parse_path(cp_name, file_name);
    free(cp_name);

    if (dir == NULL) /* if invalid path, return false */
    {
        //printf("filesys_remove error, parse\n");
        return false;
    }

    /* make sure parent directory is not about to be removed */
    if (inode_is_removed(dir_get_inode(dir)))
    {
        //printf("filesys_remove error, inode to be removed\n");
        return NULL;
    }

    struct inode *inode = NULL;
    if(dir != NULL)
        dir_lookup(dir, file_name, &inode);

    bool success = false;

  if (inode!= NULL && inode_is_dir(inode)) {
      /* 디렉터리가삭제가능한지판단*/
      bool is_deletable= true;
      struct inode *cp_inode = inode_reopen (inode);
      struct dir *child_dir = dir_open (cp_inode);
      char dir_name[NAME_MAX + 1];

      while(dir_readdir(child_dir, dir_name)) {
          if(strcmp(dir_name, ".") == 0|| strcmp(dir_name, "..") == 0)
              continue;
          else{
              is_deletable = false;
              break;
          }
      }
      dir_close (child_dir);
      /* file_name이디렉터리일경우디렉터리내에파일이존재하지
       * 않으면삭제*/
      success = is_deletable && dir_remove (dir, file_name);
  }
  else{
      /* file_name이파일일경우삭제*/
      success = dir != NULL && dir_remove(dir, file_name);
  }
  inode_close (inode);
  dir_close (dir);
  return success;
}

  
/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");

  struct dir *root_dir = dir_open_root();
  if(!dir_add(root_dir, ".", ROOT_DIR_SECTOR))
      PANIC ("root directory init of '.' failed");
  if(!dir_add(root_dir, "..", ROOT_DIR_SECTOR))
      PANIC ("root directory init of '..' failed");
  dir_close(root_dir);

  free_map_close ();
  printf ("done.\n");
}



//Root디렉터리에 파일생성을 name경로에 파일 생성하도록 변경
bool filesys_create_dir(const char *name) {
    block_sector_t inode_sector = 0;

    int name_len = strlen(name) + 1;
    char *cp_name = malloc(name_len);
    if(cp_name == NULL)
        return false;
    strlcpy(cp_name, name, name_len);

    /* name 경로분석*/
    struct dir *dir;
    char file_name[NAME_MAX + 1];
    dir = parse_path(cp_name, file_name);
    free(cp_name);

    if (!dir)
        return false;

    if (inode_is_removed(dir_get_inode(dir)))
        return NULL;

    struct dir *newDir = NULL;
 
    /* bitmap에서 inode sector 번호 할당 */
    /* 할당받은 sector에 file_name의 디렉터리 생성 */
    /* 디렉터리 엔트리에 file_name의 엔트리추가 */
    /* 디렉터리 엔트리에 ‘.’, ‘..’ 파일의 엔트리 추가 */
    bool success = (dir != NULL
            && free_map_allocate (1, &inode_sector)
            && dir_create (inode_sector, 16)
            && dir_add (dir, file_name, inode_sector)
            && (newDir = dir_open (inode_open (inode_sector)))
            && dir_add (newDir, ".", inode_sector)
            && dir_add (newDir, "..", inode_get_inumber (dir_get_inode (dir))));
   if (!success && inode_sector != 0)
        free_map_release (inode_sector, 1);
    dir_close (dir);
    dir_close (newDir);

    return success;
}

struct dir* parse_path (char *path_name, char *file_name) {
    
    struct dir *dir;
    
    if (path_name== NULL|| file_name== NULL)
        return NULL;
    
    if (strlen(path_name) == 0)
        return NULL;

    /* PATH_NAME의절대/상대경로에따른디렉터리정보저장(구현)*/
    if (path_name[0] == '/') {
        dir = dir_open_root ();
    }
    else {
        dir = dir_reopen(thread_current() -> cur_dir);
    }

    char *token, *nextToken, *savePtr;
    token = strtok_r(path_name, "/", &savePtr);
    nextToken = strtok_r (NULL, "/", &savePtr);


    while (token != NULL && nextToken != NULL) {
        struct inode *inode;
        bool success;
        /* dir에서 token이름의 파일을 검색하여 inode의 정보를 저장*/
        if (!(success = dir_lookup (dir, token, &inode))) {
            return NULL;
        }
        
        /* inode가 파일일 경우 NULL 반환 */
        if (!inode_is_dir (inode))
            return NULL;
        
        struct dir *next_dir = dir_open (inode);
        /* dir의 디렉터리정보를 메모리에서 해지 */
        dir_close (dir);
        /* inode의디렉터리정보를dir에저장 */
        dir = next_dir;

        /* token에 검색할 경로이름 저장 */
        token = nextToken;
        nextToken = strtok_r (NULL, "/", &savePtr);
    }

    if (!token)
        token = ".";

    /* token의 파일이름을 file_name에 저장 */
    size_t size = strlcpy (file_name, token, NAME_MAX+1);

    //Wrong size
    if (size > NAME_MAX+1)
        return NULL;

    /* dir정보반환*/
    return dir;
}
