#include "misc.h"

int stat(int dev, char *pathname)
{
    struct stat myst;
    int ino;
    MINODE *mip;

    ino = getino(pathname);
    mip = iget(dev, ino);

    myst.st_dev = dev;
    myst.st_ino = ino;

    myst.st_blocks = mip->INODE.i_blocks;
    myst.st_gid = mip->INODE.i_gid;
    myst.st_uid = mip->INODE.i_uid;
    myst.st_size = mip->INODE.i_size;
    myst.st_mode = mip->INODE.i_mode;
    myst.st_nlink = mip->INODE.i_links_count;
    
    iput(mip);
}

int chmod(int dev, char *pathname, u16 mode)
{
    int ino;
    MINODE *mip;

    ino = getino(pathname);
    mip = iget(dev, ino);

    mip->INODE.i_mode |= mode;
    mip->dirty = 1;

    iput(mip);
}

// int utime(char *pathname)
// {

// }