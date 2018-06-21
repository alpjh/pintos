#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer_cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define INDIRECT_BLOCK_ENTRIES (BLOCK_SECTOR_SIZE / sizeof (block_sector_t))
#define DIRECT_BLOCK_ENTRIES 123

//inode가 디스크 블록의 번호를 가리키는 방식들을 열거
enum direct_t {
    NORMAL_DIRECT,   //inode에 디스크 블록번호를 저장
    INDIRECT,        //1개의 인덱스블록을 통해 디스크블록번호에 접근
    DOUBLE_INDIRECT, //2개의 인덱스블록을 통해 디스크블록번호에 접근
    OUT_LIMIT        //잘못된 파일 오프셋 값일 경우
};

//블록주소 접근 방식과, 인덱스 블록 내의 오프셋 값을 저장
struct sector_location {
    enum direct_t directness; //디스크 블록 접근 방법(Direct, 
                              // Indirect, or Double indirect) 
    int index1; //첫번째 index block에서 접근할 entry의 offset
    int index2; //두번째 index block에서 접근할 entry의 offset
};

//인덱스 블록을 표현하는 자료구조
struct inode_indirect_block {
    block_sector_t map_table [INDIRECT_BLOCK_ENTRIES];
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t is_dir;
    //Extensible file
    block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect_block_sec;
    block_sector_t double_indirect_block_sec;
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extend_lock;
  };

static bool get_disk_inode (const struct inode *inode, 
                            struct inode_disk* inode_disk);
static void locate_byte (off_t pos, struct sector_location *sec_loc);
static inline off_t map_table_offset (int index);
static bool register_sector (struct inode_disk *inode_disk,
                             block_sector_t new_sector, 
                             struct sector_location sec_loc);
static block_sector_t byte_to_sector (const struct inode_disk *inode_disk, 
                                      off_t pos);
bool inode_update_file_length (struct inode_disk *, off_t, off_t);
static void free_inode_sectors (struct inode_disk *inode_disk);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, uint32_t is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      /* inode생성시, structinode_disk에추가한파일,
         디렉터리 구분을 위한 필드를 is_dir 인자값으로 설정 */
      disk_inode->is_dir = is_dir;

      if(length > 0)
          /* length 만큼의디스크블록을inode_updafe_file_length()를
             호출하여할당*/
          if(!inode_update_file_length(disk_inode, 0, length-1))
              return false;
      /* on—disk inode를bc_write()를통해buffer cache에기록*/
      bc_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
      /* 할당받은disk_inode변수해제*/
      free (disk_inode);
      /* success 변수update */
      success = true;
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* open_inodes리스트에inode가존재하는지검사*/

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  //block_read (fs_device, inode->sector, &inode->data);
  lock_init (&inode->extend_lock);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
        list_remove (&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed) 
        {
            struct inode_disk *disk_inode = malloc (sizeof (struct inode_disk));
            if (disk_inode == NULL)
                PANIC ("Failed to close inode. Could not allocate disk_inode.\n");
            get_disk_inode (inode, disk_inode);
            free_inode_sectors (disk_inode);
            free_map_release (inode->sector, 1);
            free (disk_inode);

       }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  /* inode_disk자료형의disk_inode변수를동적할당*/
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL)
      return 0;
  /* on-disk inode를buffer cache에서읽어옴 */
  get_disk_inode(inode, disk_inode);

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode->length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      block_sector_t sector_idx = byte_to_sector (disk_inode, offset);
      if (sector_idx == 0)
          break;

 /*     if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          //block_read (fs_device, sector_idx, buffer + bytes_read);
          //Read from buffer cache
/*        }
      else 
      {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
/*          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
*/
          //bc_read (sector_idx, buffer, bytes_read, chunk_size, bounce + sector_ofs);
/*          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
    
*/
      bc_read (sector_idx, buffer, bytes_read, chunk_size, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (disk_inode);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
//  uint8_t *bounce = NULL;

  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(!disk_inode)
      return 0;
  get_disk_inode(inode, disk_inode);


  if (inode->deny_write_cnt) {
      free(disk_inode);
      return 0;
  }

  /* inode의lock 획득*/
  lock_acquire(&inode->extend_lock);
  int old_length = disk_inode->length;
  int write_end = offset +  size - 1;
  if (write_end > old_length -1 ) {
      /*파일길이가증가하였을경우, on-disk inode업데이트*/
      disk_inode->length = write_end + 1;
      if(!inode_update_file_length(disk_inode, old_length, write_end)){
          free(disk_inode);
          return 0;
      }
  }
  /* inode의lock 해제*/
  lock_release(&inode->extend_lock);
  
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      //off_t inode_left = inode_length (inode) - offset;
      off_t inode_left = disk_inode->length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
    
      block_sector_t sector_idx = byte_to_sector (disk_inode, offset);
      if (sector_idx == 0)
          break;

      bc_write(sector_idx, (void*)buffer, bytes_written, 
               chunk_size, sector_ofs);


      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  bc_write(inode->sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
  free(disk_inode);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
    off_t result;
    struct inode_disk *disk_inode = malloc (sizeof (struct inode_disk));
    if (disk_inode == NULL)
        return 0;
    get_disk_inode (inode, disk_inode);
    result = disk_inode->length;
    free (disk_inode);
    return result;
}

//read disk inode at buffer
bool get_disk_inode (const struct inode *inode, 
                            struct inode_disk* inode_disk) {
    /* inode->sector에해당하는on-disk inode를buffer cache에서
       읽어inode_disk에저장(bc_read() 함수사용) */
    bc_read (inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
    /* true 반환*/
    return true;
}

//
void locate_byte (off_t pos, struct sector_location *sec_loc) {
    off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
    
    //Direct 
    if (pos_sector < DIRECT_BLOCK_ENTRIES) {
        //sec_loc자료구조의변수값업데이트(구현)
        sec_loc->directness = NORMAL_DIRECT;
        sec_loc->index1 = pos_sector;
    }//Indirect 
    else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES + 
                                   INDIRECT_BLOCK_ENTRIES)) { 
        //sec_loc자료구조의변수값업데이트(구현)
        sec_loc->directness = INDIRECT;
        sec_loc->index1 = pos_sector - DIRECT_BLOCK_ENTRIES;
    }   
    /* Double Indirect 방식일경우*/
    else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES +
                INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1))) {
        //sec_loc자료구조의변수값업데이트(구현)
        sec_loc->directness = DOUBLE_INDIRECT;
        //update pos_sector
        pos_sector = pos_sector 
                     - DIRECT_BLOCK_ENTRIES
                     - INDIRECT_BLOCK_ENTRIES;
        //update sec_loc->index
        sec_loc->index1 = pos_sector / INDIRECT_BLOCK_ENTRIES;
        sec_loc->index2 = pos_sector % INDIRECT_BLOCK_ENTRIES;
    }
    /* 잘못된파일오프셋일경우*/
    else
        sec_loc->directness = OUT_LIMIT;
}


inline off_t map_table_offset (int index) {
    /* byte 단위로변환한오프셋값return */
    off_t base = 0;
    return base + index * sizeof (block_sector_t);
}

bool register_sector (struct inode_disk *inode_disk,
        block_sector_t new_sector, 
        struct sector_location sec_loc) {

    //If no disk inode
    if (!inode_disk) {
        printf("\nNULL inode_disk!\n");
        return false;
    }
        
    struct inode_indirect_block *new_block = NULL;
    block_sector_t index_2nd_block;

    //Switch case directness
    switch (sec_loc.directness) {

        case NORMAL_DIRECT:
            /* inode_disk에새로할당받은디스크번호업데이트*/
            inode_disk->direct_map_table[sec_loc.index1] = new_sector;
            break;
        
        case INDIRECT:
            new_block = malloc (BLOCK_SECTOR_SIZE);
            //If no memory
            if (!new_block) 
                return false;
            
            if (inode_disk->indirect_block_sec == 0) {
              //aloc free_map 
              if (free_map_allocate (1, &(inode_disk->indirect_block_sec))) {
                  /* 인덱스블록에새로할당받은블록번호저장*/
                  memset (new_block, 0, BLOCK_SECTOR_SIZE);
                  new_block->map_table[sec_loc.index1] = new_sector;
                  /* 인덱스블록을buffer cache에기록*/
                  bc_write(inode_disk->indirect_block_sec, 
                           new_block, 0, BLOCK_SECTOR_SIZE, 0);
              } 
              else { //No block free memory 
                  free (new_block);
                  return false;
              }
            }
            else { 
                /* 인덱스블록을buffer cache에기록*/
                bc_write(inode_disk->indirect_block_sec, &new_sector, 
                        0, sizeof(block_sector_t), 
                        map_table_offset(sec_loc.index1));
            }
            break;
        
        case DOUBLE_INDIRECT : //Indirect Double
            
            new_block = malloc (BLOCK_SECTOR_SIZE);
            //malloc fail 
            if (!new_block)
                return false;

            /* 2차인덱스블록에새로할당받은블록주소저장후,
               각인덱스블록을buffer cache에기록*/
            if (inode_disk->double_indirect_block_sec == 0) {
                //alloc free map
                if (free_map_allocate (1, &(inode_disk->double_indirect_block_sec))) {
                    memset (new_block, 0, BLOCK_SECTOR_SIZE);
                    bc_write(inode_disk->double_indirect_block_sec, new_block, 0,
                            BLOCK_SECTOR_SIZE, 0);
                }
                else {//No free map
                    free (new_block);
                    return false;
                }
            }
            //read indirect block
            bc_read(inode_disk->double_indirect_block_sec, 
                    &index_2nd_block, 0, sizeof(block_sector_t), 
                    map_table_offset(sec_loc.index1));
            //write secont block 
            if (index_2nd_block == 0) {
                //free map alloc
                if (free_map_allocate (1, &index_2nd_block)) {
                    bc_write(inode_disk->double_indirect_block_sec, 
                            &index_2nd_block, 0, sizeof(block_sector_t),
                            map_table_offset(sec_loc.index1));
                    memset (new_block, 0, BLOCK_SECTOR_SIZE);
                    new_block->map_table[sec_loc.index2] = new_sector;
                    bc_write(index_2nd_block, new_block, 0, 
                            BLOCK_SECTOR_SIZE, 0);
                }
                else {//no free map alloc
                    free (new_block);
                    return false;
                }
            }
            else {
                bc_write(index_2nd_block, &new_sector, 0, 
                        sizeof(block_sector_t), 
                        map_table_offset(sec_loc.index2));
            }
            break;
        default :
            return false;
    }
    free(new_block);
    return true;
}

block_sector_t byte_to_sector (const struct inode_disk *inode_disk, 
                                      off_t pos) {
    block_sector_t result_sec;// 반환할 디스크 블록 번호
    if (pos < inode_disk->length) {
      struct inode_indirect_block *ind_block;
      struct sector_location sec_loc;
      locate_byte (pos, &sec_loc);
      switch (sec_loc.directness) {
          /* Direct 방식일경우*/
          case NORMAL_DIRECT:
              /* on-disk inode의direct_map_table에서
                 디스크블록번호를얻음*/
              result_sec = inode_disk->direct_map_table[sec_loc.index1];
              break;
          case INDIRECT:
              /* Indirect 방식일경우*/
              ind_block = (struct inode_indirect_block *) malloc (BLOCK_SECTOR_SIZE);
              if (ind_block) {
                  /* buffer cache에서인덱스블록을읽어옴*/
                  bc_read(inode_disk->indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
                  /* 인덱스블록에서디스크블록번호확인*/
                  result_sec = ind_block->map_table[sec_loc.index1];
              }
              else
                  result_sec = 0;
              free (ind_block);
              break;
              /* Double indirect 방식일경우*/
          case DOUBLE_INDIRECT:
              ind_block = (struct inode_indirect_block *) malloc (BLOCK_SECTOR_SIZE);
              if (ind_block) {
                  /* 1차인덱스블록을buffer cache에서읽음*/
                  bc_read(inode_disk->double_indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
                  /* 2차인덱스블록을buffer cache에서읽음*/
                  bc_read(ind_block->map_table[sec_loc.index1], ind_block, 0, BLOCK_SECTOR_SIZE, 0);
                  /* 2차인덱스블록에서디스크블록번호확인*/
                  result_sec = ind_block->map_table[sec_loc.index2];
              }
              else {
                  result_sec = 0;
              }
              free (ind_block);
              break;

            default:
              result_sec = 0;
        }
    }
    else
        result_sec = 0;      // pos > inode length

    return result_sec;
}

bool inode_update_file_length(struct inode_disk* inode_disk, 
                              off_t start_pos, off_t end_pos) {
    
    block_sector_t sector_idx; 
    off_t offset = start_pos;
    int size = end_pos - start_pos + 1;
    
    uint8_t *zeroes = calloc(sizeof(uint8_t), BLOCK_SECTOR_SIZE);

    if(zeroes == NULL)
        return false;
    /* 블록단위로loop을수행하며새로운디스크블록할당*/
    while (size > 0) {
        /* 디스크블록내오프셋계산*/
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int chunk_size = size < sector_left ? size : sector_left;

        if (chunk_size <= 0)
            break;

        if (sector_ofs > 0) {
            /* 블록오프셋이0보다클경우, 이미할당된블록*/
            sector_idx = byte_to_sector(inode_disk, offset);
            ASSERT(sector_idx != 0);
            //write at buffer cache
            bc_write(sector_idx, zeroes, 0, sector_left, sector_ofs);
        }
        else {
            /* 새로운디스크블록을할당*/
            if(free_map_allocate(1, &sector_idx)){
                /* inode_disk에새로할당받은디스크블록번호업데이트*/
                struct sector_location cur_loc;
                locate_byte(offset, &cur_loc);

                //If no register sector
                if (!register_sector (inode_disk, sector_idx, cur_loc)) {
                    free_map_release(sector_idx, 1);
                    free(zeroes);
                    return false;
                }
            } else {//If no allocate 
                free(zeroes);
                return false;
            }
            /* 새로운디스크블록을0으로초기화*/
            bc_write(sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0);
        }
        size -= chunk_size;
        offset += chunk_size;
    }
    free(zeroes);
    return true;
}

void free_inode_sectors (struct inode_disk *inode_disk) {

    int i, j;

    if (inode_disk->double_indirect_block_sec > 0) {
        
        struct inode_indirect_block *ind_block_1 = malloc (BLOCK_SECTOR_SIZE);
        struct inode_indirect_block *ind_block_2 = malloc (BLOCK_SECTOR_SIZE);
        
        if ( (!ind_block_1) || (!ind_block_2) ) {//fail malloc
            printf ("malloc fail inode.c :690\n");
            return;
        }
        bc_read (inode_disk->double_indirect_block_sec, ind_block_1, 0, BLOCK_SECTOR_SIZE, 0);
        /* 1차인덱스블록을buffer cache에서읽음*/
        i = 0;    
        /* 1차인덱스블록을통해2차인덱스블록을차례로접근*/
        while (ind_block_1->map_table[i] > 0) {
            bc_read (ind_block_1->map_table[i], ind_block_2, 0, BLOCK_SECTOR_SIZE, 0);
            /* 2차인덱스블록을buffer cache에서읽음*/
            j = 0;
            /* 2차인덱스블록에저장된디스크블록번호를접근*/
            while (ind_block_2->map_table[j] > 0) {
                /* free_map업데이틀통해디스크블록할당해지*/
                free_map_release (ind_block_2->map_table[j], 1);
                j++;
            }
            /* 2차인덱스블록할당해지*/
            free_map_release (ind_block_1->map_table[i], 1);
            i++;
        }
        /* 1차인덱스블록할당해지*/
        free_map_release (inode_disk->double_indirect_block_sec, 1);
        free (ind_block_1);
        free (ind_block_2);
    }
    /* Indirect 방식으로할당된디스크블록해지 */
    if (inode_disk->indirect_block_sec > 0) {
        
        struct inode_indirect_block *ind_block = malloc (BLOCK_SECTOR_SIZE);

        //malloc fail
        if (!ind_block) {
            printf ("\ninode.c :696 malloc fail\n");
            return;
        }
        /* 인덱스블록을buffer cache에서읽음*/
        bc_read (inode_disk->double_indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
        i = 0;   
        /* 인덱스블록에저장된디스크블록번호를접근*/
        
        while (ind_block->map_table[i] > 0) {
            /* free_map업데이트를통해디스크블록할당해지*/
            free_map_release (ind_block->map_table[i], 1);
            i++;
        }
        free_map_release (inode_disk->indirect_block_sec, 1);
        free (ind_block);
    }

    i = 0;
    /* Direct 방식으로할당된디스크블록해지*/
    while (inode_disk->direct_map_table[i] > 0) {
        /* free_map업데이트를통해디스크블록할당해지*/
        free_map_release (inode_disk->direct_map_table[i], 1);
        i++;
    }

}

bool inode_is_dir (const struct inode *inode) {
    
    bool result;

    struct inode_disk *disk_inode;
    
    /* inode_disk 자료구조를 메모리에 할당 */
    if ( !(disk_inode = malloc(sizeof(struct inode_disk)))) {
        return 0;
    }
    /* in-memory inode의on-disk inode를읽어inode_disk에저장*/
    bc_read(inode->sector, (void *)disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
    /* on-disk inode의is_dir을result에저장하여반환*/
    result = disk_inode->is_dir;

    free(disk_inode);

    return result;
}

bool inode_is_removed(const struct inode *inode) {
      return inode->removed;
}
