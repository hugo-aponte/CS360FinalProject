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
extern char line[128], cmd[32], pathname[128], position[10];

int open_file()
{
    // printf("\nEntering open\n");

    int ino;
    MINODE *mip;
    char filename[128];

    // get name of file, separate from path
    strcpy(filename, pathname);
    strcpy(filename, basename(filename));

    // get inode number for file
    ino = getino(filename);

    // adjust dev based on pathname
    // if(pathname[0] == '/')
    // {
    //     dev = root->dev;
    // }
    // else
    // {
    //     dev = running->cwd->dev;
    // }

    // check if ino == 0
    if (!ino)
    {
        // creat filename if it doesnt exist
        myCreat(filename);

        // get the inode number of newly created filename
        ino = getino(filename);
    }

    // get minode in memory
    mip = iget(dev, ino);

    // check if minode is already open
    if (mip->lock)
    {
        printf("open: pathname %s is already open\n", pathname);
        return -1;
    }

    // check if minode is a file
    if (!S_ISREG(mip->INODE.i_mode))
    {
        printf("open: pathname %s is not a file\n", pathname);
        return -1;
    }

    // allocate an openTable entry OFT; initialize OFT entries
    OFT *openTable = (OFT *)malloc(sizeof(OFT));

    openTable->mode = mode;
    openTable->minodePtr = mip;
    openTable->refCount = 1;
    // printf("open: openTable instantiated\n");

    // check mode for offset
    // printf("open: checking mode\n");
    if (mode == RD || mode == RW)
    {
        openTable->offset = 0;
    }
    else if (mode == WR)
    {
        myTruncate(mip);
        openTable->offset = 0;
    }
    else if (mode == AP)
    {
        openTable->offset = mip->INODE.i_size;
    }
    else
    {
        // error case
        printf("open: mode %d not recognized", mode);
        return -1;
    }
    // printf("open: done checking mode\n");

    // find the index of the running process
    int index;
    // printf("open: finding index of the running process OFT fd array\n");
    for (index = 0; index < NFD; index++)
    {
        if (running->fd[index] == NULL)
        {
            running->fd[index] = openTable;
            break;
        }
    }

    // update the access time of the MINODE, lock it, and mark it dirty
    if (mode == RD)
        mip->INODE.i_atime = time(0L);
    else
        mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);

    mip->lock = 1;
    mip->dirty = 1;

    // return index of file descriptor
    printf("%s opened for %d mode=%d fd=%d\n", filename, mode, mode, mode);
    // printf("Exiting open\n\n");
    return index;
}

int close_file(int fd)
{
    // printf("\nEntering close\n");

    // get fd from pathname
    MINODE *mip;
    OFT *openTable;

    /*commented out close execution with a filename as parameter
    instead we're going to follow the sample and close with file descriptor as parameter

    get name of file, separate from path
    strcpy(filename, pathname);
    strcpy(filename, basename(filename));

    get inode number for file
    ino = getino(filename);

    adjust dev based on pathname
    if(pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }
    
    mip = iget(dev, ino); */

    // get fd and mip from given file descriptor
    // name might be misleading
    openTable = running->fd[fd];
    mip = openTable->minodePtr;

    if (!mip)
    {
        printf("close: file descriptor %s not found\n", pathname);
        return -1;
    }

    /* 
    commented out close execution with a filename as parameter
    instead we're going to follow the sample and close with file descriptor as parameter

    look for file's fd
    printf("close: entering for loop, looking for file's test descriptor\n");
    for(int i = 0; i < NFD; i++)
    {
        if(running->fd[i] != NULL)
        {
            openTable = running->fd[i];
            oftmip = openTable->minodePtr;

            if(mip == oftmip)
            {
                // found file at fd[i]
                printf("close: found file descriptor %d", i);
                fd = i;
                break;
            }
        }
    }
    */

    // verify openTable (running->fd[fd]) is pointing at OFT entry
    if (openTable != NULL)
    {
        // printf("close: setting file descriptor of running process's OFT array to NULL\n");
        running->fd[fd] = NULL;

        // decrement refcount
        openTable->refCount--;

        // verify file is not being referenced
        if (openTable->refCount > 0)
        {
            printf("close: refcount %d\n", openTable->refCount);
            return 0;
        }

        // no other oft references, dispose of mip
        // printf("close: disposing of mip\n");
        mip->lock = 0;
        iput(mip);
    }

    // printf("Exiting close\n\n");
    return 0;
}

int myLseek()
{
    int fd = -1, pos, ino, originalPos = -1;
    MINODE *mip;
    OFT *openTable;

    // check pathname and position
    if (pathname[0] == 0)
    {
        printf("lseek: pathname is null\n");
        return -1;
    }

    if (position[0] == 0)
    {
        printf("lseek: position is null\n");
        return -1;
    }

    ino = getino(pathname);
    mip = iget(dev, ino);

    // extract file descriptor
    for (int i = 0; i < NFD; i++)
    {
        if (running->fd[i] != NULL)
        {
            openTable = running->fd[i];

            // verify file in openTable
            if (openTable->minodePtr == mip)
            {
                fd = i;
                break;
            }
        }
    }

    pos = atoi(position);

    // use extracted file descriptor and given position for lseek
    if (!fd)
    {
        printf("lseek: fd is null\n");
        return -1;
    }

    // verify an openTable was found
    if (openTable == NULL)
    {
        printf("lseek: openTable is null\n");
        return -1;
    }

    // verify openTable refCount is > 0
    if (openTable->refCount < 0)
    {
        printf("lseek: openTable refcount is < 0\n");
        return -1;
    }

    // change OFT entry's offset to position but make sure NOT to over run either end of the file
    originalPos = openTable->offset;

    if (pos <= openTable->minodePtr->INODE.i_size)
    {
        openTable->offset = pos;
    }
    else
    {
        printf("lseek: out of boundary exception\n");
        return -1;
    }

    // return original position
    return originalPos;
}

int pfd()
{
    // disply the currently opened files to help the user know what files have been opened
    OFT *openTable;
    MINODE *mip;
    printf("fd\tmode\toffset\tINODE\n");
    printf("----\t----\t------\t--------\n");

    for (int i = 0; i < NFD; i++)
    {
        // check every openTable in running process
        // if open, it should not be NULL
        if (running->fd[i] != NULL)
        {
            openTable = running->fd[i];
            mip = openTable->minodePtr;
            printf("%d\t%d\t%d\t[%d, %d]\n", i, openTable->mode, openTable->offset, dev, mip->ino);
        }
    }

    printf("--------------------------------------\n");
    return 0;
}

int dup(int fd)
{
    // verify fd is an opened descriptor
    if (fd >= 0 && fd < NFD && running->fd[fd] != NULL)
    {
        // openTable is accessible
        for (int i = 0; i < NFD; i++)
        {
            // look for the next NULL OFT
            if (running->fd[i] == NULL)
            {
                // duplicate openTable at given fd to next NULL OFT
                running->fd[i] = running->fd[fd];
                running->fd[fd]->refCount++;
                return i;
            }
        }
    }

    printf("dup: fd is not an opened descriptor\n");
    return -1;
}

int dup2(int fd, int gd)
{
    // close gd if it is open
    if (running->fd[gd] != NULL)
    {
        close_file(gd);
    }

    running->fd[gd] = running->fd[fd];
    running->fd[fd]->refCount++;

    return 0;
}