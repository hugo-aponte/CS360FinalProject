#include "type.h"

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

int enter_child(MINODE *pip, DIR *fPtr)
{
    //hard part
    // printf("inside enter_child\n");

    // pip is our parent minode, fptr is our new directory 
    // printf("Parent->ino=%d, new_dir ino=%d, new_name=%s\n", pip->ino, fPtr->inode, fPtr->name);

    char *cp;
    char buf[BLKSIZE];
    int remain;
    int ideal_length = idealLength(strlen(fPtr->name));
    DIR *dp, *newFile;


    for (int i = 0; i < 12; i++)
    {
        // printf("inside for loop\n");

        if (pip->INODE.i_block[i] == 0)
            break;

        get_block(pip->dev, pip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        // find last directory entry
        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            // printf("inside for ~ inside while\n");
            if(dp->rec_len == 0)
                break;

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        // cp and dp should be ready to enter fptr        
        // printf("last entry is %s\n", dp->name);
        cp = (char*)dp;

        // remaining bytes in current block;
        remain = dp->rec_len;
        // printf("remain %d ideal_length %d\n", remain, ideal_length);

        if (remain >= ideal_length)
        {
            // printf("inside for ~ inside if condition\n");
            dp->rec_len = idealLength(strlen(dp->name));
            remain -= dp->rec_len;

            cp += dp->rec_len;
            newFile = (DIR *)cp;

            newFile->inode = fPtr->inode;
            newFile->name_len = fPtr->name_len;

            // newFile should take over remaining size
            newFile->rec_len = BLKSIZE - fPtr->rec_len;
            strncpy(newFile->name, fPtr->name, fPtr->name_len);
            // printf("putting this at pip->INODE.i_block[%d]=%d | dp: inode=%d rec_len=%d name_len=%d\n", i, pip->INODE.i_block[i], dp->inode, dp->rec_len, dp->name_len);

            put_block(pip->dev, pip->INODE.i_block[i], buf);
        }
    }

    // printf("exiting enter_child\n");

    return 0;
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
    iput(mip); // write INODE to disk

    bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
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

int myMkdir()
{
    int ino;
    char fileName[128];
    MINODE *currentMinode;

    // check pathname for root or cwd
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

    // get parent inode number
    int pino = getino(pathname);
    
    // get parent minode with parent inode number
    MINODE *pmip = iget(dev, pino);

    // check if parent minode is a directory
    if (S_ISDIR(pmip->INODE.i_mode))
    {
        // check if basename already exists in parent minode 
        ino = search(pmip, fileName);
        if (ino)
        {
            printf("file %s already exists\n", fileName);
            return -1;
        }
    }
    else
    {
        printf("file %s is not a dir\n", fileName);
    }

    // create the new directory in kmkdir
    kmkdir(pmip, fileName);

    return 0;
}