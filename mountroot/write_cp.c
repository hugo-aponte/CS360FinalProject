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
    printf("Entering mywrite\n");

    OFT *oftp;
    MINODE *mip;
    INODE *ip;

    int count = 0, avail, lbk, blk, start, remain, *offset;
    char *cq = buf, *cp, wbuf[BLKSIZE], indirect[256], doubleIndirect[256], temp[BLKSIZE];

    // get oftp from running process
    oftp = running->fd[fd];

    // get minode from oftp
    mip = oftp->minodePtr;
    ip = &mip->INODE;

    // get offset from oftp
    offset = &oftp->offset;

    // get available bytes from mip and offset
    avail = mip->INODE.i_size - *offset;

    printf("Entering whileloop\n");
    while (nbytes > 0)
    {
        //compute LOGICAL BLOCK (lbk) and the startByte in that lbk:

        int lbk = oftp->offset / BLKSIZE;
        int startByte = oftp->offset % BLKSIZE;

        // I only show how to write DIRECT data blocks, you figure out how to
        // write indirect and double-indirect blocks.
        if (lbk < 12)
        { // direct block
            if (ip->i_block[lbk] == 0)
            { // if no data block yet

                mip->INODE.i_block[lbk] = balloc(mip->dev); // MUST ALLOCATE a block
            }
            blk = mip->INODE.i_block[lbk]; // blk should be a disk block now
        }
        else if (lbk >= 12 && lbk < 256 + 12)
        { // INDIRECT blocks
            // HELP INFO:
            if (ip->i_block[12] == 0)
            {
                //   allocate a block for it;
                //   zero out the block on disk !!!!
                get_block(mip->dev, ip->i_block[12], indirect);
                blk = indirect[lbk - 12];
                blk = balloc(mip->dev);
                get_block(mip->dev, blk, temp);
                bzero(temp, BLKSIZE);
                put_block(mip->dev, blk, temp);
            }
            // get i_block[12] into an int ibuf[256];
            // blk = ibuf[lbk - 12];
            // if (blk == 0)
            // {
            //     // allocate a disk block;
            //     // record it in i_block[12];
            // }
        }
        // else
        // {
        //     // double indirect blocks */
        // }

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