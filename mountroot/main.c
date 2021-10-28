/****************************************************************************
*                   HEA: mount root file system                             *
*****************************************************************************/
#include "type.h"

extern MINODE *iget();

MINODE minode[NMINODE];
MINODE *root;
PROC proc[NPROC], *running;

char gpath[128]; // global for tokenized components
char *name[64];  // assume at most 64 components in pathname
int n;           // number of component strings

int fd, dev;
int nblocks, ninodes, bmap, imap, iblk;
char line[128], cmd[32], pathname[128];

// #include "cd_ls_pwd.c"
// #include "util.c"

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
    mip->mptr = 0;
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

int cd()
{
  int ino = getino(pathname);
  MINODE *mip;

  // verify ino != 0
  if (!ino)
  {
    printf("cd: FAIL\n");
    return -1;
  }

  mip = iget(dev, ino);

  // verify mip->INODE is a directory
  if (!S_ISDIR(mip->INODE.i_mode))
  {
    printf("cd: FAIL\n");
    return -1;
  }

  iput(running->cwd); // release old cwd
  running->cwd = mip; // change cwd to mip

  printf("cd: OK\n");
  return 0;
}

int ls_dir(MINODE *mip)
{
  printf("ls_dir: list CWD's file names; YOU FINISH IT as ls -l\n");

  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  get_block(dev, mip->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;

  while (cp < buf + BLKSIZE)
  {
    struct stat *buf;
    buf = malloc(sizeof(struct stat));
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;

    stat(temp, buf);

    // if (dp->pid == 0)
    // {
    //   printf("root\t");
    // }
    // else
    // {
    //   printf("user\t");
    // }

    printf("%ld\t", buf->st_size);
    printf("%ld\t", buf->st_mtim);
    printf("%s\n", temp);

    cp += dp->rec_len;
    dp = (DIR *)cp;
    free(buf);
  }
  printf("\n");
}

int ls_file(MINODE *mip, char *name)
{
  // printf("ls_file: to be done: READ textbook!!!!\n");
  // READ Chapter 11.7.3 HOW TO ls
  printf("hello");
  MINODE *curPath;
  int curIno = getino(pathname);
  curPath = iget(dev, curIno);

  if (S_ISDIR(curPath->INODE.i_mode))
  {
    ls_dir(curPath);
  }
  else
  {
    printf("ERROR: not a directory\n");
  }
}
int ls(char *pathname)
{
  // printf("ls: list CWD only! YOU FINISH IT for ls pathname\n");
  if (!pathname[0])
  {
    ls_dir(running->cwd);
  }
  else
  {
    ls_file(running->cwd, pathname);
  }
}

void rpwd(MINODE *wd)
{
  char buf[BLKSIZE], dirname[BLKSIZE];
  int my_ino, parent_ino;

  DIR *dp;
  char *cp;

  // parent minode
  MINODE *parentMINODE;

  if (wd == root)
    return;

  // get dir block of cwd
  get_block(wd->dev, wd->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;

  // search through cwd for my_ino and parent ino
  while (cp > buf + BLKSIZE)
  {
    strcpy(dirname, dp->name);
    dirname[dp->name_len] = '\0';

    // check for "." dir
    if (strcmp(dirname, ".") == 0)
    {
      my_ino = dp->inode;
    }

    // check for ".." dir
    if (strcmp(dirname, "..") == 0)
    {
      parent_ino = dp->inode;
    }

    // advance to next record
    cp += dp->rec_len;
    dp = (DIR *)cp;
  }

  parentMINODE = iget(wd->dev, parent_ino);
  get_block(wd->dev, parentMINODE->INODE.i_block[0], buf);
  dp = (DIR *)buf;
  cp = buf;

  while (cp < buf + BLKSIZE)
  {
    strncpy(dirname, dp->name, dp->name_len);
    dirname[dp->name_len] = 0;

    // check if we found directory associated with my_ino
    if (dp->inode == my_ino)
    {
      break;
    }

    // advance to next record
    cp += dp->rec_len;
    dp = (DIR *)cp;
  }
  rpwd(parentMINODE);
  iput(parentMINODE);

  printf("/%s", dirname);
  return;
}

void pwd(MINODE *wd)
{
  if (wd == root)
  {
    printf("/\n");
    return;
  }

  rpwd(wd);
  printf("\n");
  return;
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

char *disk = "diskimage";
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

  while (1)
  {
    printf("P%d running: ", running->pid);
    printf("input command : [ls|cd|pwd|quit] ");
    fgets(line, 128, stdin);
    line[strlen(line) - 1] = 0;

    if (line[0] == 0)
      continue;
    pathname[0] = 0;

    sscanf(line, "%s %s", cmd, pathname);
    printf("cmd=%s pathname=%s\n", cmd, pathname);

    if (strcmp(cmd, "ls") == 0)
      ls(pathname);
    else if (strcmp(cmd, "cd") == 0)
      cd();
    else if (strcmp(cmd, "pwd") == 0)
      pwd(running->cwd);
    else if (strcmp(cmd, "quit") == 0)
      quit();
  }
}