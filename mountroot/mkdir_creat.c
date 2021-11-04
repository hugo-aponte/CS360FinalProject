#include "type.h"

int tst_bit(char *buf, int bit)
{
    return buf[bit / 8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit)
{
    buf[bit / 8] |= (1 << (bit % 8));
}

int ialloc(int dev, int imap, int bmap, int ninodes) // allocate an inode number from inode_bitmap
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

int balloc(int dev, int imap, int bmap, int ninodes)
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
            printf("allocated ino = %d\n", i + 1); // bits count from 0; ino from 1
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

int enter_name(MINODE *pip, DIR *fPtr)
{
    //hard part
    char *cp;
    char buf[BLKSIZE];
    int remain;
    int ideal_length = idealLength(strlen(fPtr->name));
    DIR *dp, *newFile;

    for (int i = 0; i < 12; i++)
    {
        if (pip->INODE.i_block[i] == 0)
            break;

        get_block(pip->dev, pip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        remain = dp->rec_len;
        if (remain >= ideal_length)
        {
            dp->rec_len = idealLength(strlen(dp->name));
            remain -= dp->rec_len;

            cp += dp->rec_len;
            newFile = (DIR *)cp;

            newFile->inode = fPtr->inode;
            newFile->name_len = fPtr->name_len;
            newFile->rec_len = fPtr->rec_len;
            strncpy(newFile->name, fPtr->name, fPtr->name_len);

            put_block(pip->dev, pip->INODE.i_block[i], buf);
        }
    }
    return 0;
}

DIR kmkdir(MINODE *pmip, int dev, int imap, int bmap, int ninodes, PROC *running, int pino, char *fileName)
{
    char *charPtr;
    char buf[BLKSIZE];
    int ino = ialloc(dev, imap, bmap, ninodes);
    int blk = balloc(dev, imap, bmap, ninodes);
    DIR dirPtr;

    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x41ED;      // 040755: DIR type and permissions
    ip->i_uid = running->uid; // owner uid
    ip->i_gid = running->gid; // group Id
    ip->i_size = BLKSIZE;     // size in bytes
    ip->i_links_count = 2;    // links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 2;     // LINUX: Blocks count in 512-byte chunks
    ip->i_block[0] = blk; // new DIR has one data block
    for (int i = 0; i < 15; i++)
        ip->i_block[i] = 0;
    mip->dirty = 1; // mark minode dirty

    iput(mip); // write INODE to disk

    bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
    DIR *dp = (DIR *)buf;
    // make . entry
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->name[0] = '.';
    // make .. entry: pino=parent DIR ino, blk=allocated block
    dp = (char *)dp + 12;
    dp->inode = pino;
    dp->rec_len = BLKSIZE - 12; // rec_len spans block
    dp->name_len = 2;
    dp->name[0] = dp->name[1] = '.';
    put_block(dev, blk, buf); // write to blk on diks

    dirPtr.inode = ino;
    strncpy(dirPtr.name, fileName, strlen(fileName));
    dirPtr.name_len = strlen(fileName);
    dirPtr.rec_len = 4 * ((8 + dirPtr.name_len + 3) / 4);

    enter_name(pmip, &dirPtr);
}

int myMkdir(char *pathname, PROC *running, MINODE *root, int dev, int imap, int bmap, int ninodes)
{
    int ino;
    char fileName[128];
    char *pCopy[64];
    MINODE *currentMinode;

    if (pathname[0] == '/')
    {
        currentMinode = root;
    }
    else
    {
        currentMinode = running->cwd;
    }

    strcpy(fileName, pathname);

    strcpy(pathname, dirname(pathname));
    printf("path: %s ", pathname);
    strcpy(fileName, basename(fileName));
    printf("file: %s ", fileName);

    int pino = getino(pathname);
    MINODE *pmip = iget(dev, pino);

    if (S_ISDIR(pmip->INODE.i_mode))
    {
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

    kmkdir(pmip, dev, imap, bmap, ninodes, running, pino, fileName);

    pmip->dirty = 1;
    iput(pmip);
    return 0;
}