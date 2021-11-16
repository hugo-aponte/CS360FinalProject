#include "type.h"

extern int mode;

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128];
extern char *name[64];
extern int n;

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128];

int open_file()
{
    int ino;
    MINODE *mip;
    char filename[64];

    // get name of file, separate from path
    strcpy(filename, pathname);
    strcpy(filename, basename(filename));

    // get inode number for file
    ino = getino(filename);

    // adjust dev based on pathname
    if(pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }
    
    // check if ino == 0
    if(!ino)
    {
        // creat filename if it doesnt exist
        myCreat(filename);

        // get the inode number of newly created filename
        ino = getino(filename);
    }

    // get minode in memory
    mip = iget(dev, ino);

    // check if minode is already open
    if(mip->lock)
    {
        printf("open: pathname %s is already open\n", pathname);
        return -1;
    }

    // check if minode is a file
    if(!S_ISREG(mip->INODE.i_mode))
    {
        printf("open: pathname %s is not a file\n", pathname);
        return -1;
    }

    // allocate an openTable entry OFT; initialize OFT entries
    OFT *openTable = (OFT *)malloc(sizeof(OFT));

    openTable->mode = mode;
    openTable->minodePtr = mip;
    openTable->refCount = 1;

    // check mode for offset
    if(mode == RD || mode == RW)
    {
        openTable->offset = 0;
    }
    else if(mode == WR)
    {
        truncate(mip);
        openTable->offset = 0;
    }
    else if(mode == AP)
    {
        openTable->offset = mip->INODE.i_size;
    }
    else
    {
        // error case
        printf("open: mode %d not recognized", mode);
        return -1;
    }

    // find the index of the running process
    int index;
    for(index = 0; index < NFD; index++)
    {
        if(running->fd[index] == NULL)
        {
            running->fd[index] = openTable;
            break;
        }
    }

    // update the access time of the MINODE, lock it, and mark it dirty
    mip->INODE.i_atime = time(0L);
    mip->lock = mode;
    mip->dirty = 1;

    // return index of file descriptor
    return index;
}