/****************************************************************************
*                   HEA: & JS: EXT2 File System                             *
*****************************************************************************/
#include "type.h"

extern MINODE *iget();

MINODE minode[NMINODE];
MINODE *root;
PROC proc[NPROC], *running;

char gpath[128]; // global for tokenized components
char *name[64];  // assume at most 64 components in pathname
int n;           // number of component strings

int fd, dev, mode;
int nblocks, ninodes, bmap, imap, iblk;
char line[128], cmd[32], pathname[128], destination[128], position[10];

int init()
{
  int i, j;
  MINODE *mip;
  PROC *p;

  printf("init()\n");

  for (i = 0; i < NMINODE; i++)
  {
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    // mip->mptr = 0;
  }
  for (i = 0; i < NPROC; i++)
  {
    p = &proc[i];
    p->status = READY;
    p->pid = i;
    p->uid = p->gid = i;
    p->cwd = 0;
  }

  printf("creating P0 as running process\n");
  proc[NPROC - 1].next = &proc[0]; // circular list
  running = &proc[0];              // P0 runs first
}

// load root INODE and set root pointer to it
int mount_root()
{
  printf("mount_root()\n");
  root = iget(dev, 2);
}

int quit()
{
  int i;
  MINODE *mip;
  for (i = 0; i < NMINODE; i++)
  {
    mip = &minode[i];
    if (mip->refCount > 0)
      iput(mip);
  }
  exit(0);
}

char *disk = "disk";
int main(int argc, char *argv[])
{
  int ino;
  char buf[BLKSIZE];

  printf("checking EXT2 FS ....");
  if ((fd = open(disk, O_RDWR)) < 0)
  {
    printf("open %s failed\n", disk);
    exit(1);
  }

  dev = fd; // global dev same as this fd

  /********** read super block  ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system ***********/
  if (sp->s_magic != 0xEF53)
  {
    printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
    exit(1);
  }

  printf("EXT2 FS OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;

  get_block(dev, 2, buf);
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  iblk = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, iblk);

  init();
  mount_root();
  printf("root refCount = %d\n", root->refCount);

  running->cwd = iget(dev, 2);
  printf("root refCount = %d\n", root->refCount);

  // P1 created as user process
  running = &proc[1];
  running->cwd = root;
 
  // char temp[128];
  // strcpy(temp, "d");

  // testing block creation
  // for (int i = 0; i < 200; i++)
  // {
  //   strcpy(pathname, temp);
  //   myMkdir();
  //   sprintf(temp, "%s%d", "d", i);
  // }

  while (1)
  {
    printf("P%d running: ", running->pid);
    printf("input command : [ls|cd|pwd|mkdir|rmdir|creat|link|symlink|unlink|open|close|lseek|read|quit] ");
    fgets(line, 128, stdin);
    line[strlen(line) - 1] = 0;

    if (line[0] == 0)
      continue;
    pathname[0] = 0;

    sscanf(line, "%s %s", cmd, pathname);
    printf("cmd=%s pathname=%s\n", cmd, pathname);

    if (strcmp(cmd, "ls") == 0)
      ls();
    else if (strcmp(cmd, "cd") == 0)
      cd();
    else if (strcmp(cmd, "pwd") == 0)
      pwd(running->cwd);
    else if (strcmp(cmd, "quit") == 0)
      quit();
    else if (strcmp(cmd, "mkdir") == 0)
      myMkdir();
    else if (strcmp(cmd, "creat") == 0)
      myCreat();
    else if (strcmp(cmd, "link") == 0)
    {
      sscanf(line, "%s %s %s", cmd, pathname, destination);
      printf("pathname=%s destination=%s\n", pathname, destination);
      myLink();
    }
    else if (strcmp(cmd, "symlink") == 0)
    {
      sscanf(line, "%s %s %s", cmd, pathname, destination);
      printf("pathname=%s destination=%s\n", pathname, destination);
      mySymLink();
    }
    else if (strcmp(cmd, "unlink") == 0)
    {
      myUnlink();
    }
    else if (strcmp(cmd, "rmdir") == 0)
      printf("%d", myRmdir());
    else if (strcmp(cmd, "open") == 0)
    {
      sscanf(line, "%s %s %d", cmd, pathname, &mode);
      printf("pathname=%s, mode=%d\n", pathname, mode);
      open_file();
    }
    else if (strcmp(cmd, "close") == 0)
    {
      int fd = atoi(pathname);
      close_file(fd);
    }
    else if (strcmp(cmd, "pfd") == 0)
      pfd();
    else if (strcmp(cmd, "lseek") == 0)
    {
      sscanf(line, "%s %s %s", cmd, pathname, position);
      printf("pathname=%s, position=%s\n", pathname, position);
      myLseek();
    }
    else if (strcmp(cmd, "read") == 0)
    {
      sscanf(line, "%s %s %s", cmd, pathname, destination);
      printf("pathname=%s, destination(nbytes)=%s\n", pathname, destination);
      read_file();
    }
    else if (strcmp(cmd, "cat") == 0)
      myCat();
    // else if write
  }
}
