#include "type.h"

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128]; // global for tokenized components
extern char *name[64];  // assume at most 64 components in pathname
extern int n;           // number of component strings
extern int mode;

extern int dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128], destination[128];

int write_file(int fd, char *writebuf)
{
    // verify that an opened file descriptor was found
    if (running->fd[fd] == NULL)
    {
        printf("write_file: file descriptor %s not found\n", pathname);
        return -1;
    }
    return mywrite(fd, writebuf, strlen(writebuf));
}

int mywrite(int fd, char buf[], int nbytes)
{

    OFT *oftp;
    MINODE *mip;
    INODE *ip;

    int count = 0, avail, lbk, blk, iblk, start, remain, *offset;
    char *cq = buf, *cp, wbuf[BLKSIZE], indirect[BLKSIZE], doubleIndirect[BLKSIZE];

    // get oftp from running process
    oftp = running->fd[fd];

    // get minode from oftp
    mip = oftp->minodePtr;
    ip = &mip->INODE;

    // get offset from oftp
    offset = &oftp->offset;

    // get available bytes from mip and offset
    avail = mip->INODE.i_size - *offset;
    while (nbytes > 0)
    {
        //compute LOGICAL BLOCK (lbk) and the startByte in that lbk:

        int lbk = oftp->offset / BLKSIZE;
        int startByte = oftp->offset % BLKSIZE;

        // I only show how to write DIRECT data blocks, you figure out how to
        // write indirect and double-indirect blocks.
        printf("\nlbk %d\n", lbk);
        if (lbk < 12)
        { // direct block
            if (ip->i_block[lbk] == 0)
            { // if no data block yet

                mip->INODE.i_block[lbk] = balloc(mip->dev); // MUST ALLOCATE a block
            }
            blk = mip->INODE.i_block[lbk]; // blk should be a disk block now
        }
        else if (lbk >= 12 && lbk < (256 + 12))
        { // INDIRECT blocks
            //printf("\n\nindirect blocks\n\n");
            // HELP INFO:
            int ibuf[256];
            if (ip->i_block[12] == 0)
            {
                ip->i_block[12] = balloc(mip->dev);
                explicit_bzero(indirect, BLKSIZE);
                put_block(mip->dev, ip->i_block[12], indirect);
            }

            // get i_block[12] into an int ibuf[256];
            get_block(mip->dev, ip->i_block[12], indirect);
            memcpy(&ibuf, &indirect, BLKSIZE);
            blk = ibuf[lbk - 12];
            if (blk == 0)
            {
                blk = balloc(mip->dev);
                ibuf[lbk - 12] = blk;
                memcpy(&indirect, &ibuf, BLKSIZE);
                put_block(mip->dev, ip->i_block[12], indirect);
            }
        }
        else
        {
            int ibuf[256], ibuf2[256];
            // double indirect blocks */
            if (ip->i_block[13] == 0)
            {
                ip->i_block[13] = balloc(mip->dev);
                explicit_bzero(indirect, BLKSIZE);
                put_block(mip->dev, ip->i_block[13], indirect);
            }

            get_block(mip->dev, ip->i_block[13], indirect);
            memcpy(&ibuf, &indirect, BLKSIZE);
            if (ibuf[(lbk - (256 + 12)) / 256] == 0)
            {
                ibuf[(lbk - (256 + 12)) / 256] = balloc(mip->dev);
                explicit_bzero(doubleIndirect, BLKSIZE);
                put_block(mip->dev, ibuf[(lbk - (256 + 12)) / 256], doubleIndirect);
                memcpy(&indirect, &ibuf, BLKSIZE);
                put_block(mip->dev, ip->i_block[13], indirect);
            }

            get_block(mip->dev, ibuf[(lbk - (256 + 12)) / 256], doubleIndirect);
            memcpy(&ibuf2, &doubleIndirect, BLKSIZE);
            blk = ibuf2[(lbk - (256 + 12)) % 256];
            if (blk == 0)
            {
                blk = balloc(mip->dev);
                ibuf2[(lbk - (256 + 12)) % 256] = blk;
                memcpy(&doubleIndirect, &ibuf2, BLKSIZE);
                put_block(mip->dev, ibuf[(lbk - (256 + 12)) / 256], doubleIndirect);
            }
        }

        /* all cases come to here : write to the data block */
        get_block(mip->dev, blk, wbuf); // read disk block into wbuf[ ]
        char *cp = wbuf + startByte;    // cp points at startByte in wbuf[]
        remain = BLKSIZE - startByte;   // number of BYTEs remain in this block

        while (remain > 0)
        {                  // write as much as remain allows
            *cp++ = *cq++; // cq points at buf[ ]
            nbytes--;
            remain--;                        // dec counts
            oftp->offset++;                  // advance offset
            if (*offset > mip->INODE.i_size) // especially for RW|APPEND mode
                mip->INODE.i_size++;         // inc file size (if offset > fileSize)
            if (nbytes <= 0)
                break; // if already nbytes, break
        }
        put_block(mip->dev, blk, wbuf); // write wbuf[ ] to disk

        // loop back to outer while to write more .... until nbytes are written
    }

    mip->dirty = 1; // mark mip dirty for iput()
    printf("wrote %d char into file descriptor fd=%d\n", nbytes, fd);
    return nbytes;
}

int myCp(char *f1, char *f2)
{
    char mybuf[BLKSIZE] = "\0", dummy = 0, *sender, filename[128];
    int n, fd1, fd2, ino;
    MINODE *mip;
    mode = RD;

    // calling open_file, expect to return fd index
    // assuming: main.c should have set global pathname to the entered filename (open_file uses global pathname and mode)
    // we manually set the global mode to RD

    strcpy(pathname, f1);
    fd1 = open_file();
    // check if file exists
    if (fd1 < 0)
    {
        printf("cp: %s does not exist or fd=%d invalid\n", pathname, fd2);
        return -1;
    }
    strcpy(pathname, f2);
    strcpy(filename, pathname);
    strcpy(filename, basename(filename));
    ino = getino(filename);
    mip = iget(dev, ino);

    // myTruncate(mip);

    fd2 = open_file();
    // check if file exists
    if (fd2 < 0)
    {
        printf("cp: %s does not exist or fd=%d invalid\n", pathname, fd2);
        return -1;
    }

    // printf("cat: entering while loop\n");
    while (n = myRead(fd1, mybuf, BLKSIZE))
    {
        // make sure not to go out of bounds in case of error
        if (n <= 0)
            break;

        mybuf[n] = 0;           // as a null terminated string
        mywrite(fd2, mybuf, n); // this works but not good

        // spit out chars from mybuf[] but handle \n properly
    }
    // printf("cat: exiting while loop\n");

    // close given file descriptor
    close_file(fd1);
    close_file(fd2);

    // printf("Exiting myCat\n\n");
    return 0;
}