/*
 * Implementation of a Unix-like file system.
*/
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif
#define MAGIC 4275476
#define SUPER_LOC 0
#define SUPER_COPY_LOC FS_SIZE - 1
#define MAX_INODES 256

int work_dir;
#define ROOT_INODE_N 0
#define MAX_FD_NUM 64
#define OK 1
#define ENTRIES_PER_BLOCK (BLOCK_SIZE/sizeof(fd_t))
sb_t * superblock;
char sb_buffer[BLOCK_SIZE];

inode_t * inode_read(int inode_index,char * block_buffer);
void inode_free(int inode_number);
//copy a string from t to s
char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}
unsigned int
mystrlen(char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}
int strcmp(char *s,char *t){
    while(0 != (*s == *t) && (0 != *s))
        {s++;t++;}
    return *s - *t;
}
//---------------------------------------------------------------------------------------
//some useful macros for common operations
#define CEILING_DIV(m,n) ((m-1)/n)+1
#define INODES_PER_BLOCK ((BLOCK_SIZE)/sizeof(inode_t))
//"bytemap"
#define BITS_PER_BYTE 1
fd_t file_d_table[MAX_FD_NUM];
//---------------------------------------------------------------------------------------
//START of bitmap
void bitmap_init(sb_t* sb){
    char temp[BLOCK_SIZE];
    int i;
    bzero_block(temp);
    if(sb->data_addr<=BLOCK_SIZE-1){
        temp[BLOCK_SIZE-1] = 1;     //mark the last block to be busy
        block_write(sb->bitmap_addr+sb->n_bitmap_blocks-1,temp);//because it is used to keep copy of superblock

        temp[BLOCK_SIZE-1] = 0;
        for(i=0;i<sb->data_addr;i++)
            temp[i] = 1;
        block_write(sb->bitmap_addr,temp);
    }
    else{
        print_str(0, 0, "waiting to be completed ");
    }
}
//allocate the bitmap block given the date block number
int bitmap_block(int data_block_index){
    return superblock->bitmap_addr + 
    (data_block_index / BLOCK_SIZE);
}

char * map_read(int data_block_index,char *buffer){
    block_read(bitmap_block(data_block_index),buffer);
    return buffer + (data_block_index % BLOCK_SIZE);
}

void map_write(int data_block_index, char *buffer){
    block_write(bitmap_block(data_block_index),buffer);
}

//END of bitmap
//---------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//START of superblock


void sb_init(sb_t *sb) {
    sb->fs_size = FS_SIZE;
    sb->magic_number = MAGIC;

    sb->inode0_addr = SUPER_LOC + 1;
    sb->n_inodes = MAX_INODES;
    sb->inode_blocks = CEILING_DIV(MAX_INODES, INODES_PER_BLOCK);
    
    sb->bitmap_addr = sb->inode0_addr + sb->inode_blocks;
    sb->n_bitmap_blocks = CEILING_DIV(FS_SIZE, BLOCK_SIZE*BITS_PER_BYTE);
    
    sb->data_addr = sb->bitmap_addr + sb->n_bitmap_blocks;
    sb->n_data = FS_SIZE-sb->data_addr-1;
}

//END of superblock
//----------------------------------------------------------------------------------------------
 
//----------------------------------------------------------------------------------------
//START of data blocks
void data_read(int block_number, char *block_buffer) {
    block_read(superblock->data_addr + block_number, block_buffer);
}
int balloc(){
    int i, j;
    char block_buffer[BLOCK_SIZE];
    //search for free block
    for (i = 0; i < superblock->n_bitmap_blocks; i++) 
    {
        block_read(superblock->bitmap_addr + i, block_buffer);
        for (j = 0; j < BLOCK_SIZE; j++) 
        {
            if (block_buffer[j] == 0)
             {
                // Mark block as used on disk
                block_buffer[j] = 1;
                block_write(superblock->bitmap_addr + i, block_buffer);
                // Return block number of newly allocated block
                return (i * BLOCK_SIZE) + j;
            }
        }
    }

    // No free blocks found
    return ERROR;
}   
void data_write(int block_number, char *block_buffer) {
    block_write(superblock->data_addr + block_number, block_buffer);
}

void block_free(int data_block_index) {
    //no need to clear the block to all 0s
    //just set the bitmap
    char *map_entry;
    char block_buffer[BLOCK_SIZE];
    map_entry = map_read(data_block_index, block_buffer);
    *map_entry = 0; // set this byte to 0
    map_write(data_block_index, block_buffer);    
}
//END of data blocks
//----------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------
//START of directory
//insert source file into dest directory
//with file name
int insert_entry_into_dir(int source_inode_number,int dest_inode_number,char *file_name){
    inode_t *directory_inode;
    char inode_buffer[BLOCK_SIZE]; 
    directory_inode = inode_read(dest_inode_number,inode_buffer);
    //must be a directory, or it cannot contain any files
    ASSERT(directory_inode->type==DIRECTORY);
    int current_entries;
    current_entries = directory_inode->file_size / sizeof(dir_t);

    /* if too many entries in this directory */
    if(current_entries >= N_DIRECT_BLOCK * ENTRIES_PER_BLOCK){
        print_str(0,0,"too many entries, waiting to be completed");
        return ERROR;
    }
    int which_block;//which block
    int which_entry;//in a block
    which_block = current_entries / ENTRIES_PER_BLOCK;
    which_entry = current_entries % ENTRIES_PER_BLOCK;   

    if(which_block >= directory_inode->n_blocks){
        int new_block;
        new_block = balloc();

        if(new_block == ERROR){
            ASSERT(0);
            return ERROR;
        }
        directory_inode->dir_blk_addr[which_block] = new_block;
        directory_inode->n_blocks++;
        // ASSERT(0);
    }

    char data_buffer[BLOCK_SIZE];
    block_read(directory_inode->dir_blk_addr[which_block],data_buffer); //read this block into data_buffer
    dir_t * dir_struct_ptr;
    dir_struct_ptr = (dir_t *)data_buffer;
    dir_struct_ptr[which_entry].inode = source_inode_number;

    if( mystrlen(file_name) >= MAX_FILE_NAME){
        //in other words, only support MAX_FILE_NAME-1 chars
        return ERROR;
    }
    strcpy(dir_struct_ptr[which_entry].name, file_name);
    block_write(directory_inode->dir_blk_addr[which_block],data_buffer);//write back
    directory_inode->file_size += sizeof(dir_t);
    inode_write(dest_inode_number,inode_buffer);
    return OK;      
}

int find_file_inode_in_dir(int directory_inode,char *file_name){
    if(!file_name||strlen(file_name)>=MAX_FILE_NAME)//null ptr
        return ERROR;
    if(directory_inode < 0 || directory_inode > MAX_INODES - 1)
        return ERROR;//wrong directory inode

    char block_buffer[BLOCK_SIZE];
    inode_t * inode_ptr;
    inode_ptr = inode_read(directory_inode,block_buffer);
    if(inode_ptr->type != DIRECTORY)
        return ERROR;

    int current_entries;
    current_entries = inode_ptr->file_size / sizeof(dir_t);
    int how_many_blocks;
    int last_entry_last_block;
    how_many_blocks = current_entries / ENTRIES_PER_BLOCK;
    last_entry_last_block = current_entries % ENTRIES_PER_BLOCK;   
    
    int i,j;
    dir_t * dir_struct_ptr;
    char data_buffer[BLOCK_SIZE];
    for(i = 0;i <= how_many_blocks; i++)
    {
        block_read(inode_ptr->dir_blk_addr[i],data_buffer);
        dir_struct_ptr = (dir_t *)data_buffer;
        if(i == how_many_blocks )
        {
            //final block
            for(j = 0; j < last_entry_last_block; j++)
            {
                if(strcmp(file_name,dir_struct_ptr[j].name) == 0)
                {
                    return dir_struct_ptr[j].inode;
                }
            }
        }
        else
        {
            //scan all entries
            for(j = 0; j < ENTRIES_PER_BLOCK; j++)
            {
                if(strcmp(file_name,dir_struct_ptr[j].name) == 0)
                {
                    return dir_struct_ptr[j].inode;
                }
            }
        }
    }
    return ERROR;
}

//END of directory
//----------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------
//START of inode
int inode_in_which_block(int inode_index){
    return superblock->inode0_addr+inode_index/INODES_PER_BLOCK;
}
void inode_write(int inode_index,char * block_buffer){
    int block_number;
    block_number = inode_in_which_block(inode_index);
    block_write(block_number,block_buffer);
}

void inode_init(inode_t *inode, int type) {
    inode->type = type;
    inode->link_count = 1;
    inode->descriptor_count = 0;
    inode->file_size = 0;
    //make sure those pointers don't point to sth. wrong
    bzero((char *)inode->dir_blk_addr, sizeof(inode->dir_blk_addr));
    bzero((char *)inode->ind_blk_addr, sizeof(inode->ind_blk_addr));
    inode->n_blocks = 0;
}
//usage: read the block which contains the inode wanted into the given buffer address
//return the pointer to the inode
//so the caller needs basically 2 pointers to manage the inode.
inode_t * inode_read(int inode_index,char * block_buffer){
    int block_number;
    block_number = inode_in_which_block(inode_index);
    block_read(block_number,block_buffer);
    inode_t * inode;
    inode = (inode_t *)block_buffer;
    int offset;
    offset = inode_index % INODES_PER_BLOCK;
    return inode + offset;
}
void inode_free(int inode_number){
    //TODO:is there a need to clear sth else
    //such as link count and so on
    inode_t * inode;
    char inode_buffer[BLOCK_SIZE];
    inode = inode_read(inode_number,inode_buffer);
    int i;
    for(i = 0; i < inode->n_blocks; i++){
        block_free(inode->dir_blk_addr[i]);
    }
    inode->type = FREE_INODE;
    inode->file_size = 0;
    inode->n_blocks = 0;
    inode_write(inode_number,inode_buffer);
}
int inode_alloc(int type){
    //look for a free inode and return the inode number
    int block_number;
    int inode_number_in_block;
    char data_buffer[BLOCK_SIZE];
    int j;
    inode_t * inode_ptr;
    for(block_number = superblock->inode0_addr; 
        block_number < superblock->bitmap_addr; 
        block_number++)
    {
        block_read(block_number,data_buffer);
        inode_ptr = (inode_t *)data_buffer;
        
            //scan all entries
        for(j = 0; j < INODES_PER_BLOCK; j++)
        {
            if(inode_ptr[j].type == 0)
            {
                inode_init(&inode_ptr[j],type);
                inode_ptr[j].inode_number =
                (block_number - superblock->inode0_addr)*INODES_PER_BLOCK
                + j;
                return inode_ptr[j].inode_number;
            }
        }
                
    }
}
//END of inode
//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//START of fd
int open_in_fd(int inode_number,int mode){
    int i;
    for(i=0; i<MAX_FD_NUM;i++){
        if(file_d_table[i].used == FALSE){
            file_d_table[i].used = TRUE;
            file_d_table[i].offset = 0;
            file_d_table[i].inode_number = inode_number;
            file_d_table[i].access_mode = mode;
            return i;
        }
    }
    return ERROR;
}
//END of fd
//----------------------------------------------------------------------------------------------

void 
fs_init( void) {
    block_init();
    /* More code HERE */
    block_read(SUPER_LOC, sb_buffer);
    superblock = (sb_t * ) sb_buffer;

    if(superblock->magic_number != MAGIC){
        fs_mkfs();
    }
    else{
        work_dir = ROOT_INODE_N;
        bzero((char *)file_d_table,sizeof(fd_t));
    }

}

int
fs_mkfs( void) {
    char block_buffer[BLOCK_SIZE];
    int i;
    //zero out all disk sectors(file system blocks, more appropriately)
    bzero_block(block_buffer);
    for(i=0;i<FS_SIZE;i++){
        block_write(i,block_buffer);
    }
    
    //super block 
    bzero_block(sb_buffer);
    superblock = (sb_t *)sb_buffer;
    sb_init(superblock);
    //writing to disk:both original and copy
    block_write(SUPER_LOC,sb_buffer);
    block_write(SUPER_COPY_LOC,sb_buffer);

    //initialize bit(byte)map
    bitmap_init(superblock);

    //root dir management
    //make the inode for root directory
    inode_t * inode;
    inode = inode_read(ROOT_INODE_N,block_buffer);
    inode_init(inode, DIRECTORY);
    //write to disk
    inode_write(ROOT_INODE_N,block_buffer);

    int result;
    result = insert_entry_into_dir(ROOT_INODE_N,ROOT_INODE_N,".");

    if(result == ERROR){
        inode_free(ROOT_INODE_N);
        return ERROR;
    }
    result = insert_entry_into_dir(ROOT_INODE_N,ROOT_INODE_N,"..");

    if(result == ERROR){
        inode_free(ROOT_INODE_N);
        return ERROR;
    }
    work_dir = ROOT_INODE_N;
    bzero((char *)file_d_table,sizeof(file_d_table));

    return 0;
}

int 
fs_open( char *fileName, int flags) {
    if( !fileName)// if the addr given is invalid
        return ERROR;
    if(flags!=FS_O_RDONLY && flags!=FS_O_WRONLY && flags!=FS_O_RDWR)
        return ERROR;
    int open_inode_number;
    open_inode_number = find_file_inode_in_dir(work_dir,fileName);
    int new_file_created = 0;
    if(open_inode_number == ERROR)
    {
        if(flags == FS_O_RDONLY)
            return ERROR;
        open_inode_number = inode_alloc(FILE_TYPE);
        if(open_inode_number == ERROR)
            return ERROR;
        //insert new inode into current dir
        int result;
        result = insert_entry_into_dir(open_inode_number,work_dir,fileName);
        if(result == ERROR)
        {
            inode_free(open_inode_number);
            return ERROR;
        }    
        new_file_created = 1;
    }
    inode_t * inode;
    char inode_buffer[BLOCK_SIZE];
    int debug;
    debug = INODES_PER_BLOCK;
    inode = inode_read(open_inode_number,inode_buffer);
    if(inode->type == DIRECTORY && flags != FS_O_RDONLY){
        //open a directory with write mode
        return ERROR;
    }

    //file descriptor
    int fd;
    fd = open_in_fd(open_inode_number,flags);
    if(fd == ERROR){
        //no file descriptor available
        if(new_file_created){
            inode_free(open_inode_number);
        }
        return ERROR;
    }
    inode->descriptor_count++;
    inode_write(open_inode_number,inode_buffer);
    return fd;
}

int 
fs_close( int fd) {

    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    return -1;
}
    
int 
fs_write( int fd, char *buf, int count) {
    return -1;
}

int 
fs_lseek( int fd, int offset) {
    return -1;
}

int 
fs_mkdir( char *fileName) {
    return -1;
}

int 
fs_rmdir( char *fileName) {
    return -1;
}

int 
fs_cd( char *dirName) {
    //enter a directory
    if(!dirName)
        return ERROR;
    if(strcmp(dirName,".") == 0)
        return 0;
    int dir_inode;
    if(strcmp(dirName,"..") == 0){
        dir_inode = find_file_inode_in_dir(work_dir,"..");
        work_dir = dir_inode;
        return 0;
    }
    dir_inode = find_file_inode_in_dir(work_dir,dirName);
    if(dir_inode == ERROR)
        return ERROR;
    else{
        work_dir = dir_inode;
        return 0;
    }
}

int 
fs_link( char *old_fileName, char *new_fileName) {
    return -1;
}

int 
fs_unlink( char *fileName) {
    return -1;
}

int 
fs_stat( char *fileName, fileStat *buf) {
    if(!fileName||!buf)
        return ERROR;
    int inode_number;
    inode_number = find_file_inode_in_dir(work_dir,fileName);
    if(inode_number == ERROR)
        return ERROR;
    else{
        inode_t * inode;
        char inode_buffer[BLOCK_SIZE];
        inode = inode_read(inode_number,inode_buffer);
        buf->inodeNo = inode_number;
        buf->type = inode->type;
        buf->links = inode->link_count;
        buf->descriptor_count = inode->descriptor_count;
        buf->size = inode->file_size;
        buf->numBlocks = inode->n_blocks;
        return 0;
    }
}

