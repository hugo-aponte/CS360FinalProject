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
