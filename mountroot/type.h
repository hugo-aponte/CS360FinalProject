/*************** type.h file for LEVEL-1 ****************/
#ifndef TYPE_H
#define TYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc GD;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD *gp;
INODE *ip;
DIR *dp;

#define FREE 0
#define READY 1

#define BLKSIZE 1024
#define NMINODE 128
#define NPROC 2

typedef struct minode
{
  INODE INODE;  // INODE structure on disk
  int dev, ino; // (dev, ino) of INODE
  int refCount; // in use count
  int dirty;    // 0 for clean, 1 for modified

  int mounted;          // for level-3
  struct mntable *mptr; // for level-3
} MINODE;

typedef struct proc
{
  struct proc *next;
  int pid; // process ID
  int ppid;
  int status;
  int uid; // user ID
  int gid;
  MINODE *cwd; // CWD directory pointer
} PROC;

int get_block(int dev, int blk, char *buf);
int put_block(int dev, int blk, char *buf);
int tokenize(char *pathname);
MINODE *iget(int dev, int ino);
void iput(MINODE *mip);
int search(MINODE *mip, char *name);
int getino(char *pathname);
int findmyname(MINODE *parent, u32 myino, char myname[]);
int findino(MINODE *mip, u32 *myino); // myino = i# of . return i# of ..

#endif