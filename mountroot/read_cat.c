#include "type.h"

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128]; // global for tokenized components
extern char *name[64];  // assume at most 64 components in pathname
extern int n;           // number of component strings

extern int dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128], destination[128];

int read_file()
{
    printf("Entering read_file\n");
    // ASSUME: File is opened for RD or RW;
    // ask for a fd and nbytes to read;
    // verify that fd is indeed opened for RD or RW;
    // return myread(fd, buf, nbytes)

    int ino, fd = -1, nbytes;
    MINODE *mip;
    char *buf;

    if(pathname[0] == 0)
    {
        printf("read_file: pathname is null\n");
        return -1;
    }

    // using destination to extract nbytes to read
    // name might be misleading
    if(destination[0] == 0)
    {
        printf("read_file: nbytes is null\n");
        return -1;
    }

    // get mip from pathname
    ino = getino(pathname);
    mip = iget(dev, ino);
    
    // get nbytes from destination var
    nbytes = atoi(destination);

    for(int i = 0; i < NFD; i++)
    {
        // verify oft is not null, member minodePtr = mip, and mode aligns with our initial assumption (read or read/write)
        // file to read found if true
        if((running->fd[i] != NULL && running->fd[i]->minodePtr == mip) &&  (running->fd[i]->mode == 0 || running->fd[i]->mode == 2))
        {
            fd = i;
            break;
        }
    }

    // verify that an opened file descriptor was found
    if(fd < 0)
    {
        printf("read_file: file %s not found\n", pathname);
        return -1;
    }

    // allocate nbytes in buf
    buf = (char*)malloc(nbytes);

    printf("Exiting read_file\n");
    return myRead(fd, buf, nbytes);
}

int myRead(int fd, char *buf, int nbytes)
{
    printf("Entering myread\n");

    OFT *openTable;
    MINODE *mip;
    INODE *ip;

    int count = 0, avail, lbk, blk, start, remain, *oftOffset;
    char *cq = buf, *cp, readBuf[BLKSIZE], indirect[256], doubleIndirect[256], temp[BLKSIZE];

    // get openTable from running process
    openTable = running->fd[fd];

    // get minode from openTable
    mip = openTable->minodePtr;

    // get offset from openTable
    oftOffset = &openTable->offset;

    // get available bytes from mip and oftOffset
    avail = mip->INODE.i_size - *oftOffset;

    printf("Entering while loop | nbytes=%d, avail=%d\n", nbytes, avail);
    while(nbytes && avail)
    {
        // get lbk and start
        lbk = *oftOffset / BLKSIZE;
        start = *oftOffset % BLKSIZE;

        printf("lbk=%d, start=%d\n", lbk, start);

        // check lbk for direct, indirect, and double indirect
        if(lbk < 12)
        {
            blk = mip->INODE.i_block[lbk]; // map LOGICAL lbk to PHYSICAL blk
        }
        else if(lbk >= 12 && lbk < 256 + 12)
        {
            // indirect blocks
            get_block(mip->dev, ip->i_block[12], indirect);
            blk = indirect[lbk-12];
        }
        else
        {
            // double indirect blocks
            get_block(mip->dev, ip->i_block[13], indirect);
            get_block(mip->dev, indirect[lbk-(256+12)/256], doubleIndirect);
            blk = doubleIndirect[(lbk-(256+12)%256)];
        }

        // get the data block into readBuf[BLKSIZE]
        get_block(mip->dev, blk, readBuf);

        // check if file is empty
        if(readBuf[0] == 0)
        {
            printf("read: file is empty\n");
            put_block(mip->dev, blk, readBuf);
            return -1;
        }

        // copy from start to buf[], at most remain bytes in this block
        cp = readBuf + start;
        remain = BLKSIZE - start; // number of bytes remain in readBuf[]

        printf("Entering inner while loop | cq=%d -> %s, cp=%d -> %s, remain=%d\n", *(int*)cq, cq, *(int*)cp, cp, remain);
        while(remain > 0)
        {
            *cq++ = *cp++;
            *oftOffset++;
            count++;
            avail--, nbytes--, remain--;
            
            if (nbytes <= 0 || avail <= 0)
                break;
        }
        // if one data block is not enough, loop back to OUTER while for more 
    }

    printf("myRead: read %d char from file descriptor %d\n", count, fd);
    return count;
}