/*
 * Implementation of a Unix-like file system.
*/
#ifndef FS_INCLUDED
#define FS_INCLUDED

//number of sectors 
#define FS_SIZE 2048

#define N_INDIRECT_BLOCK 4
#define N_DIRECT_BLOCK 10
#define SUPER_BLOCK_SIZE sizeof(sb_t)
#define INODE_SIZE sizeof(inode_t)
typedef struct {
	int fs_size;
	int magic_number;

	int bitmap_addr;
	int n_bitmap_blocks;
	//further info needed:how many inodes can a block contain?
	int n_inodes;
	int inode0_addr;

	int n_data;
	int data_addr;

} sb_t;
typedef struct {
	bool_t is_dir;
	int link_count;
	int file_size;
	int n_blocks;
	
	int dir_blk_addr[N_DIRECT_BLOCK];
	int ind_blk_addr[N_INDIRECT_BLOCK];
} inode_t;

typedef struct 
{
	int inode_addr;
	int access_mode;
	int offset;
	bool_t used; //used when it is pointing to an inode
} fd_t;

void fs_init( void);
int fs_mkfs( void);
int fs_open( char *fileName, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *dirName);
int fs_link( char *old_fileName, char *new_fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
