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

// ************ cd *****************
int cd()
{
    int ino = getino(pathname);
    MINODE *mip;

    // verify ino != 0
    if (!ino)
    {
        printf("cd: FAIL\n");
        return -1;
    }

    mip = iget(dev, ino);

    // verify mip->INODE is a directory
    if (!S_ISDIR(mip->INODE.i_mode))
    {
        printf("cd: FAIL\n");
        return -1;
    }

    iput(running->cwd); // release old cwd
    running->cwd = mip; // change cwd to mip

    printf("cd: OK\n");
    return 0;
}

// ************ ls *****************
int ls()
{
    unsigned long ino;
    MINODE *mip, *pip;
    // int device = running->cwd->dev;
    char *child;

    // check if pathname was entered, if not then set mip to iget call relative to global dev and running->cwd->ino number and run ls_dir on mip
    if (pathname[0] == 0)
    {
        mip = iget(dev, running->cwd->ino);
        ls_dir(mip, dev);
    }
    else
    {
        // check if pathname begins with '/'
        if (pathname[0] == '/')
        {
            // set dev to root->dev
            dev = root->dev;
        }

        // get inode number of pathname
        ino = getino(pathname);

        // check if getino worked
        if (ino == -1 || ino == 0)
        {
            return 1;
        }

        // if ino worked, set MINODE mip to iget call for global dev and extracted inode number
        mip = iget(dev, ino);

        // check if mip is a directory
        if ((mip->INODE.i_mode & 0040000) != 0040000)
        {
            // ensure pathname contains parent
            if (findparent(pathname))
            {
                child = basename(pathname);
            }
            else // if not, allocate new heap space for child character array with the size of pathname and set to pathname
            {
                child = (char *)malloc((strlen(pathname) + 1) * sizeof(char));
                strcpy(child, pathname);
            }

            // print file from ls_file using mip and extracted child name
            ls_file(mip, child);

            // release MINODE mip
            iput(mip);
            return 0;
        }

        // print directory from ls_dir using MINODE mip
        ls_dir(mip, dev);
    }

    // release MINODE mip
    iput(mip);
}

// ************ pwd *****************
void pwd(MINODE *wd)
{
    if (wd == root)
    {
        printf("/\n");
        return;
    }

    rpwd(wd);
    printf("\n");
    return;
}