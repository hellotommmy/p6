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
sb_t * superblock;
int work_dir;
#define ROOT_INODE_N 0
#define MAX_FD_NUM 64

fd_t fd_table[MAX_FD_NUM];

void 
fs_init( void) {
    block_init();
    /* More code HERE */
    char super_buffer[BLOCK_SIZE];
    block_read(SUPER_LOC, super_buffer);
    superblock = (sb_t * ) super_buffer;
    if(superblock->magic_number != MAGIC){
        fs_mkfs();
    }
    else{
        work_dir = ROOT_INODE_N;
        bzero((char *)fd_table,sizeof(fd_table));
    }

}

int
fs_mkfs( void) {
    return -1;
}

int 
fs_open( char *fileName, int flags) {
    return -1;
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
    return -1;
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
    return -1;
}

