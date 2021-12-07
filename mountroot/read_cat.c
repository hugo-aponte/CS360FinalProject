#include "type.h"

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128]; // global for tokenized components
extern char *name[64];  // assume at most 64 components in pathname
extern int n;           // number of component strings

extern int dev, mode;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128], destination[128];

int read_file()
{
    // printf("\nEntering read_file\n");
    // ASSUME: File is opened for RD or RW;
    // ask for a fd and nbytes to read;
    // verify that fd is indeed opened for RD or RW;
    // return myread(fd, buf, nbytes)

    int ino, fd = -1, nbytes;
    char *buf;

    // using pathname to extract file descriptor to read from
    // name might be misleading
    if (pathname[0] == 0)
    {
        printf("read_file: file descriptor is null\n");
        return -1;
    }

    // using destination to extract nbytes to read
    // name might be misleading
    if (destination[0] == 0)
    {
        printf("read_file: nbytes is null\n");
        return -1;
    }

    // get mip from file descriptor in pathname
    fd = atoi(pathname);

    // get nbytes from destination var
    nbytes = atoi(destination);

    // verify that an opened file descriptor was found
    if (running->fd[fd] == NULL)
    {
        printf("read_file: file descriptor %s not found\n", pathname);
        return -1;
    }

    // allocate nbytes in buf
    buf = (char *)malloc(nbytes);

    // printf("Exiting read_file\n\n");
    return myRead(fd, buf, nbytes);
}

int myRead(int fd, char *buf, int nbytes)
{
    // printf("\nEntering myread\n");

    OFT *openTable;
    MINODE *mip;
    INODE *ip;

    int count = 0, avail, lbk, blk, start, remain, optimize;
    char *cq = buf, *cp, readBuf[BLKSIZE], indirect[BLKSIZE], doubleIndirect[BLKSIZE], temp[BLKSIZE];

    // get openTable from running process
    openTable = running->fd[fd];

    // get minode from openTable
    mip = openTable->minodePtr;
    ip = &mip->INODE;

    // get available bytes from mip and oftOffset
    avail = mip->INODE.i_size - openTable->offset;

    // printf("Entering while loop | nbytes=%d, avail=%d\n", nbytes, avail);
    while (nbytes && avail)
    {
        // get lbk and start
        lbk = openTable->offset / BLKSIZE;
        start = openTable->offset % BLKSIZE;

        // printf("lbk=%d, start=%d\n", lbk, start);

        // check lbk for direct, indirect, and double indirect
        if (lbk < 12)
        {
            blk = mip->INODE.i_block[lbk]; // map LOGICAL lbk to PHYSICAL blk
        }
        else if (lbk >= 12 && lbk < (256 + 12))
        {
            int ibuf[256];
            // indirect blocks
            get_block(mip->dev, ip->i_block[12], indirect);
            memcpy(&ibuf, &indirect, BLKSIZE);
            blk = ibuf[lbk - 12];
        }
        else
        {
            int ibuf[256], ibuf2[256];
            // double indirect blocks
            get_block(mip->dev, ip->i_block[13], indirect);
            memcpy(&ibuf, &indirect, BLKSIZE);
            get_block(mip->dev, ibuf[(lbk - (256 + 12)) / 256], doubleIndirect);
            memcpy(&ibuf2, &doubleIndirect, BLKSIZE);
            blk = ibuf2[(lbk - (256 + 12)) % BLKSIZE];
        }

        // get the data block into readBuf[BLKSIZE]
        get_block(mip->dev, blk, readBuf);

        // check if file is empty
        if (readBuf[0] == 0)
        {
            printf("read: file is empty\n");
            put_block(mip->dev, blk, readBuf);
            return -1;
        }

        // copy from start to buf[], at most remain bytes in this block
        cp = readBuf + start;
        remain = BLKSIZE - start; // number of bytes remain in readBuf[]

        // optimize acts as a remain temp
        optimize = remain;

        // if nbytes < remain, set optimize to nbytes
        if (nbytes < optimize)
        {
            optimize = nbytes;
        }

        // if avail < optimize, set optimize to avail
        if (avail < optimize)
        {
            optimize = avail;
        }

        // read optimize characters at a time
        // increment and decrement variables accordingly
        memcpy(cq, cp, optimize);
        *cp += optimize;
        openTable->offset += optimize;
        count += optimize;
        avail -= optimize;
        nbytes -= optimize;
        remain -= optimize;

        // printf("Entering inner while loop | offset=%d, cp=%s, remain=%d\n", openTable->offset, cp, remain);
        // if (remain > 0)
        // {
        //     if (((nbytes - remain) >= 0 || (avail - remain) >= 0))
        //     {
        //         memcpy(cp, cp, remain);
        //         memcpy(cq, cp, remain);
        //         // *cp += remain;
        //         // *cq = *cp;
        //         openTable->offset += remain;
        //         count += remain;
        //         avail -= remain;
        //         nbytes -= remain;
        //         remain = 0;
        //     }
        //     else
        //     {
        //         // memcpy(cp, cp, avail);
        //         // memcpy(cq, cp, avail);
        //         // openTable->offset += avail;
        //         // count += avail;
        //         // avail -= avail;
        //         // nbytes -= avail;
        //         // remain = remain - avail;
        //     }
        // }
        // else
        //     break;
        // while (remain > 0)
        // {
        //     *cq++ = *cp++;
        //     openTable->offset++;
        //     count++;
        //     avail--, nbytes--, remain--;

        //     if (nbytes <= 0 || avail <= 0)
        //         break;
        // }
        //printf("Exiting inner while loop | offset=%d, cp=%s, remain=%d\n", openTable->offset, cp, remain);
        // if one data block is not enough, loop back to OUTER while for more
    }

    // printf("myRead: read %d char from file descriptor %d\n", count, fd);
    // printf("----------------------\n%s\n----------------------\n", buf);
    // printf("Exiting myRead\n\n");
    return count;
}

int myCat()
{
    // printf("\nEntering myCat\n");

    char mybuf[BLKSIZE + 1] = "\0", dummy = 0;
    int n, fd;
    mode = RD;

    // calling open_file, expect to return fd index
    // assuming: main.c should have set global pathname to the entered filename (open_file uses global pathname and mode)
    // we manually set the global mode to RD
    fd = open_file();

    // check if file exists
    if (fd < 0)
    {
        printf("cat: %s does not exist or fd=%d invalid\n", pathname, fd);
        return -1;
    }

    // printf("cat: entering while loop\n");
    while (n = myRead(fd, mybuf, BLKSIZE))
    {
        // make sure not to go out of bounds in case of error
        if (n <= 0)
            break;

        mybuf[n] = 0;        // as a null terminated string
        printf("%s", mybuf); // this works but not good

        // spit out chars from mybuf[] but handle \n properly
    }
    // printf("cat: exiting while loop\n");

    // close given file descriptor
    close_file(fd);

    // printf("Exiting myCat\n\n");
    return 0;
}