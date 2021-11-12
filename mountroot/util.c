/*********** util.c file ****************/
#include "type.h"

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128];
extern char *name[64];
extern int n;

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;

extern char line[128], cmd[32], pathname[128];

int tst_bit(char *buf, int bit)
{
   return buf[bit / 8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit)
{
   buf[bit / 8] |= (1 << (bit % 8));
}

int clr_bit(char *buf, int bit) // clear bit in char buf[BLKSIZE]
{
   buf[bit / 8] &= ~(1 << (bit % 8));
}

int ialloc(int dev) // allocate an inode number from inode_bitmap
{
   int i;
   char buf[BLKSIZE];

   // read inode_bitmap block
   get_block(dev, imap, buf);

   for (i = 0; i < ninodes; i++)
   {
      if (tst_bit(buf, i) == 0)
      {
         set_bit(buf, i);
         put_block(dev, imap, buf);
         printf("allocated ino = %d\n", i + 1); // bits count from 0; ino from 1
         return i + 1;
      }
   }
   return 0;
}

int balloc(int dev)
{
   int i;
   char buf[BLKSIZE];

   // read inode_bitmap block
   get_block(dev, bmap, buf);

   for (i = 0; i < ninodes; i++)
   {
      if (tst_bit(buf, i) == 0)
      {
         set_bit(buf, i);
         put_block(dev, bmap, buf);
         printf("allocated block number = %d\n", i + 1); // bits count from 0; ino from 1
         return i + 1;
      }
   }
   return 0;
}

int idalloc(int dev, int ino)
{
   int i;
   char buf[BLKSIZE];

   if (ino > ninodes)
   {
      printf("inumber %d out of range\n", ino);
      return 1;
   }

   get_block(dev, imap, buf); // get inode bitmap block into buf[]

   clr_bit(buf, ino - 1); // clear bit ino-1 to 0

   put_block(dev, imap, buf); // write buf back
}

int bdalloc(int dev, int blk)
{
   int i;
   char buf[BLKSIZE];

   if (blk > ninodes)
   {
      printf("inumber %d out of range\n", blk);
      return 1;
   }

   get_block(dev, imap, buf); // get  block into buf[]

   clr_bit(buf, blk - 1); // clear bit ino-1 to 0

   put_block(dev, imap, buf); // write buf back
}

int idealLength(int len) { return 4 * ((8 + len + 3) / 4); }

int decFreeInodes(int dev, char *buf)
{
   // dec free inodes count in SUPER and GD
   get_block(dev, 1, buf);
   sp = (SUPER *)buf;
   sp->s_free_inodes_count--;
   put_block(dev, 1, buf);
   get_block(dev, 2, buf);
   gp = (GD *)buf;
   gp->bg_free_inodes_count--;
   put_block(dev, 2, buf);
}

int incFreeInodes(int dev)
{
   char buf[BLKSIZE];
   // dec free inodes count in SUPER and GD
   get_block(dev, 1, buf);
   sp = (SUPER *)buf;
   sp->s_free_inodes_count++;
   put_block(dev, 1, buf);
   get_block(dev, 2, buf);
   gp = (GD *)buf;
   gp->bg_free_inodes_count++;
   put_block(dev, 2, buf);
}

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk * BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
   return 0;
}

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk * BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
   return 0;
}

int tokenize(char *pathname)
{
   int i;
   char *s;
   printf("tokenize %s\n", pathname);

   strcpy(gpath, pathname); // tokens are in global gpath[ ]
   n = 0;

   s = strtok(gpath, "/");
   while (s)
   {
      name[n] = s;
      n++;
      s = strtok(0, "/");
   }
   name[n] = 0;

   for (i = 0; i < n; i++)
      printf("%s  ", name[i]);
   printf("\n");
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
   int i;
   MINODE *mip;
   char buf[BLKSIZE];
   int blk, offset;
   INODE *ip;

   for (i = 0; i < NMINODE; i++)
   {
      mip = &minode[i];
      if (mip->refCount && mip->dev == dev && mip->ino == ino)
      {
         mip->refCount++;
         //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
         return mip;
      }
   }

   for (i = 0; i < NMINODE; i++)
   {
      mip = &minode[i];
      if (mip->refCount == 0)
      {
         //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
         mip->refCount = 1;
         mip->dev = dev;
         mip->ino = ino;

         // get INODE of ino to buf
         blk = (ino - 1) / 8 + iblk;
         offset = (ino - 1) % 8;

         //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

         get_block(dev, blk, buf);
         ip = (INODE *)buf + offset;
         // copy INODE to mp->INODE
         mip->INODE = *ip;
         return mip;
      }
   }
   printf("PANIC: no more free minodes\n");
   return 0;
}

int midalloc(MINODE *mip) // release a used minode
{
   mip->refCount = 0;
}

int iput(MINODE *mip)
{
   INODE *ip;
   int i, block, offset;
   char buf[BLKSIZE];
   if (mip == 0)
      return 0;     // SUS origionally returned nothing, but was giving error
   mip->refCount--; // dec refCount by 1
   if (mip->refCount > 0)
      return 0; // still has user SUS origionally returned nothing, but was giving error
   if (mip->dirty == 0)
      return 0; // no need to write back SUS origionally returned nothing, but was giving error
   // write INODE back to disk
   block = (mip->ino - 1) / 8 + iblk;
   offset = (mip->ino - 1) % 8;
   // get block containing this inode
   get_block(mip->dev, block, buf);
   ip = (INODE *)buf + offset;      // ip points at INODE
   *ip = mip->INODE;                // copy INODE to inode in block
   put_block(mip->dev, block, buf); // write back to disk
   midalloc(mip);                   // mip->refCount = 0;
}

int search(MINODE *mip, char *name)
{
   int i;
   char *cp, c, sbuf[BLKSIZE], temp[256];
   DIR *dp;
   INODE *ip;

   printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
   ip = &(mip->INODE);

   /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

   get_block(dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf;
   printf("  ino   rlen  nlen  name\n");

   while (cp < sbuf + BLKSIZE)
   {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      printf("%4d  %4d  %4d    %s\n",
             dp->inode, dp->rec_len, dp->name_len, dp->name);
      if (strcmp(temp, name) == 0)
      {
         printf("found %s : ino = %d\n", temp, dp->inode);
         return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
   }
   return 0;
}

int getino(char *pathname)
{
   int i, ino, blk, offset;
   char buf[BLKSIZE];
   INODE *ip;
   MINODE *mip;

   printf("getino: pathname=%s\n", pathname);
   if (strcmp(pathname, "/") == 0)
      return 2;

   // starting mip = root OR CWD
   if (pathname[0] == '/')
      mip = root;
   else
      mip = running->cwd;

   mip->refCount++; // because we iput(mip) later

   tokenize(pathname);

   for (i = 0; i < n; i++)
   {
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);

      ino = search(mip, name[i]);

      if (ino == 0)
      {
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);
      mip = iget(dev, ino);
   }

   iput(mip);
   return ino;
}

int findparent(char *pathname)
{
   int i = 0;
   while (i < strlen(pathname))
   {
      if (pathname[i] == '/')
         return 1;

      i++;
   }
   return 0;
}

int ls_file(MINODE *mip, char *name)
{
   // set INODE parent to mip->INODE | this represents the parent directory
   INODE *parentip = &mip->INODE;
   u16 mode = parentip->i_mode;

   // track calendar time to include in file description
   time_t calendarTime = parentip->i_ctime;
   char *mtime = ctime(&calendarTime);
   mtime[strlen(mtime) - 1] = '\0';

   // extract file type
   u16 type = mode & 0xF000;

   // check if file type if a file or directory
   if (type == 0x4000)
      printf("d");
   else if (type == 0x8000)
      printf("-");
   else if (type == 0xA000)
      printf("l");
   else
      printf("-");

   // check permissions associated with current file to print in file description
   ((mode & S_IRUSR) == S_IRUSR) ? printf("r") : printf("-");
   ((mode & S_IWUSR) == S_IWUSR) ? printf("w") : printf("-");
   ((mode & S_IXUSR) == S_IXUSR) ? printf("x") : printf("-");
   ((mode & S_IRGRP) == S_IRGRP) ? printf("r") : printf("-");
   ((mode & S_IWGRP) == S_IWGRP) ? printf("w") : printf("-");
   ((mode & S_IXGRP) == S_IXGRP) ? printf("x") : printf("-");
   ((mode & S_IROTH) == S_IROTH) ? printf("r") : printf("-");
   ((mode & S_IWOTH) == S_IWOTH) ? printf("w") : printf("-");
   ((mode & S_IXOTH) == S_IXOTH) ? printf("x") : printf("-");

   // print further current file descriptions
   printf("%4d%4d%4d  %s%8d  %s", parentip->i_links_count, parentip->i_gid, parentip->i_uid, mtime, parentip->i_size, name);

   // check for symlink file, and show the file if it exists
   ((mode & 0120000) == 0120000) ? printf(" => %s\n", (char *)(mip->INODE.i_block)) : printf("\n");
}

int ls_dir(MINODE *mip, int dev)
{
   char buf[BLKSIZE], name[256], *cp;
   DIR *dp;
   int i, ino;
   MINODE *temp;

   // check if direct block where our directories are is valid
   if (mip->INODE.i_block[0])
   {
      // set block content to buf
      get_block(dev, mip->INODE.i_block[0], buf);
      cp = buf;
      dp = (DIR *)buf;

      // traverse directories utilizing dp and cp
      while (cp < &buf[BLKSIZE])
      {
         // handle directory name properly
         strncpy(name, dp->name, dp->name_len);
         name[dp->name_len] = 0;

         // set inode number to that of the current directory
         ino = dp->inode;

         // set temp to MINODE from iget call using global dev and directory inode number
         temp = iget(dev, ino);

         // print file from ls_file using MINODE temp and directory name
         ls_file(temp, name);

         // advance to next record and set directory pointer to next directory
         cp += dp->rec_len;
         dp = (DIR *)cp;
         iput(temp);
      }
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
   while (cp < buf + BLKSIZE)
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

int kcreat(MINODE *pmip, char *filename)
{
   printf("inside my_creat\n");
   printf("pmip->ino=%d, filename=%s\n", pmip->ino, filename);

   // get ino available and blk numbers
   int ino = ialloc(dev);
   int blk = balloc(dev);

   MINODE *mip = iget(dev, ino);
   INODE *ip = &mip->INODE;

   // write INODE into memory inode
   ip->i_mode = 0x81A4;      // File type and permissions
   ip->i_uid = running->uid; // user id
   ip->i_gid = running->gid; // group id
   ip->i_size = 0;
   ip->i_links_count = 1;
   ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
   ip->i_blocks = 2;
   ip->i_block[0] = blk;
   mip->refCount = 0;

   for (int i = 1; i < 13; i++)
   {
      ip->i_block[i] = 0; // set other inode blocks to 0
   }

   mip->dirty = 1;
   iput(mip);

   // create new directory entry for new file
   DIR newdir;
   newdir.inode = ino;
   strncpy(newdir.name, filename, strlen(filename));
   newdir.name_len = strlen(filename);
   newdir.rec_len = idealLength(newdir.name_len);

   // enter file entry into parent directory
   enter_child(pmip, &newdir);

   pmip->INODE.i_atime = time(0L);
   pmip->dirty = 1;

   iput(pmip);

   // when this function is used by symlink, it will expect the inode number to be returned
   return ino;
}

DIR kmkdir(MINODE *pmip, char *fileName)
{
   // printf("Inside kmkdir\n");
   char buf[BLKSIZE];
   int ino = ialloc(dev);
   int blk = balloc(dev);
   DIR dirPtr;

   MINODE *mip = iget(dev, ino);
   INODE *ip = &mip->INODE;

   ip->i_mode = 0x41ED;      // 040755: DIR type and permissions
   ip->i_uid = running->uid; // owner uid
   ip->i_gid = running->gid; // group Id
   ip->i_size = BLKSIZE;     // size in bytes
   ip->i_links_count = 2;    // links count=2 because of . and ..
   ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
   ip->i_blocks = 2;     // linux: blocks count in 512-byte chunks
   ip->i_block[0] = blk; // new DIR has one data block

   for (int i = 1; i < 15; i++)
      ip->i_block[i] = 0; // setting all other blocks to 0

   mip->dirty = 1; // mark minode dirty
   iput(mip);      // write INODE to disk

   bzero(buf, BLKSIZE);           // optional: clear buf[ ] to 0
   get_block(mip->dev, blk, buf); // initialize new allocated block
   DIR *dp = (DIR *)buf;

   // make . entry
   dp->inode = ino;
   dp->rec_len = 12;
   dp->name_len = 1;
   dp->name[0] = '.';
   dp->file_type = (u8)EXT2_FT_DIR;
   // printf("dp: inode=%d, rec_len=%d, name_len=%d, dp->name=%c\n", dp->inode, dp->rec_len, dp->name_len, dp->name[0]);

   // make .. entry: pmip->ino=parent DIR ino, blk=allocated block
   dp = (DIR *)((char *)dp + dp->rec_len);
   dp->inode = pmip->ino;
   dp->rec_len = BLKSIZE - 12; // rec_len spans block
   dp->name_len = 2;
   dp->name[0] = dp->name[1] = '.';
   dp->file_type = (u8)EXT2_FT_DIR;
   // printf("dp: inode=%d, rec_len=%d, name_len=%d, dp->name=%c%c\n", dp->inode, dp->rec_len, dp->name_len, dp->name[0], dp->name[1]);

   put_block(mip->dev, blk, buf); // write to blk on disk

   // printf("past put_block ~ created new dir\n");
   dirPtr.inode = ino;
   strncpy(dirPtr.name, fileName, strlen(fileName));
   dirPtr.name_len = strlen(fileName);
   dirPtr.rec_len = 4 * ((8 + dirPtr.name_len + 3) / 4);
   // printf("new dir name: %s\n", dirPtr.name);

   enter_child(pmip, &dirPtr);
   // printf("past enter_child");

   pmip->INODE.i_atime = time(0L);
   pmip->dirty = 1;
   iput(pmip);
   // printf("exiting kmkdir\n");
}

int makePath(char *fileName, MINODE *currentMinode)
{
   if (pathname[0] == '/')
   {
      currentMinode = root;
   }
   else
   {
      currentMinode = running->cwd;
   }

   // get parentpath and basename
   strcpy(fileName, pathname);
   strcpy(pathname, dirname(pathname));
   strcpy(fileName, basename(fileName));
   printf("path: %s ", pathname);
   printf("file: %s ", fileName);
}

int checkDir(MINODE *pmip, int ino, char *fileName)
{
   if (S_ISDIR(pmip->INODE.i_mode))
   {
      // check if basename already exists in parent minode
      ino = search(pmip, fileName);
      if (ino)
      {
         printf("file %s exists\n", fileName);
         return 1;
      }
   }
   else
   {
      printf("file %s is not a dir\n", pathname);
      return 2;
   }
}

// These 2 functions are needed for pwd()
int findmyname(MINODE *parent, u32 myino, char myname[])
{
   // WRITE YOUR code here
   // search parent's data block for myino; SAME as search() but by myino
   // copy its name STRING to myname[ ]
}

int findino(MINODE *mip, u32 *myino) // myino = i# of . return i# of ..
{
   // mip points at a DIR minode
   // WRITE your code here: myino = ino of .  return ino of ..
   // all in i_block[0] of this DIR INODE.
}
