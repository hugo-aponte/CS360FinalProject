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

int rm_child(MINODE *pip, char *fName, int ino)
{
    char buf[BLKSIZE], name[256], *cp;
    char modBuf[BLKSIZE];
    char *size = &buf[BLKSIZE];
    int removedSize = 0;
    int middle = 0; // 0 for false 1 for true
    char *prevCp;
    char nextName[256];
    DIR *prevDp;
    DIR *dp;

    //check all inode blocks, don't quite understand why, but rmdir example code does it.
    for (int i = 0; i < 12; i++)
    {
        // check if direct block where our directories are is valid
        if (!pip->INODE.i_block[i])
            break;

        printf("iblock %d is valid\n", i);
        get_block(dev, pip->INODE.i_block[i], buf);
        cp = buf;
        dp = (DIR *)buf;

        while (cp + dp->rec_len < size)
        {
            // handle directory name properly
            strncpy(name, dp->name, dp->name_len);
            name[dp->name_len] = 0;
            // set inode number to that of the current directory

            printf("compairing %s to %s\n", name, fName);
            if (!strcmp(name, fName))
            {
                // bdalloc(dev, pip->INODE.i_block[i]);
                // incFreeInodes(dev);

                printf("removing %s\n", name);
                if (cp + dp->rec_len >= &buf[BLKSIZE])
                {
                    prevDp->rec_len += dp->rec_len;
                    break;
                }
                else
                {
                    printf("coppying buf to mod\n");
                    middle = 1;
                    int leftIndex = cp - buf;
                    removedSize = dp->rec_len;

                    memcpy(modBuf, buf, leftIndex);
                    memcpy(&modBuf[leftIndex], &buf[leftIndex + dp->rec_len], (&buf[BLKSIZE] - buf) - (leftIndex + dp->rec_len)); // insane adress reading
                    memcpy(buf, modBuf, BLKSIZE);
                    //memcpy(&buf[cp - buf], &buf[cp + dp->rec_len - buf], &buf[BLKSIZE] - cp + dp->rec_len);

                    printf("origional size %llu\n", (long long int)size);
                    size -= removedSize;
                    printf("removed size %d\n", removedSize);
                    printf("new size %llu\n", (long long int)size);
                    dp = (DIR *)cp;

                    continue;
                }
            }

            ino = dp->inode;

            prevCp = cp;
            prevDp = dp;

            // advance to next record and set directory pointer to next directory
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        printf("pre add dp->rec_len = %d\n", dp->rec_len);
        dp->rec_len += removedSize;
        printf("final dp->rec_len = %d\n", dp->rec_len);

        put_block(dev, pip->INODE.i_block[i], buf);

        // traverse directories utilizing dp and cp
        // while (cp < &buf[BLKSIZE])
        // {
        //     // handle directory name properly
        //     strncpy(name, dp->name, dp->name_len);
        //     name[dp->name_len] = 0;
        //     // set inode number to that of the current directory
        //     ino = dp->inode;

        //     // advance to next record and set directory pointer to next directory
        //     cp += dp->rec_len;
        //     dp = (DIR *)cp;
        // }
    }
}

int myRmdir()
{
    int ino = getino(pathname);

    char fileName[128];
    MINODE *currentMinode;
    makePath(fileName, currentMinode);

    // check if pathname is not ., .., nor /
    if (strcmp(fileName, ".") == 0)
    {
        printf("rmdir: cannot remove . directory\n");
        return -1;
    }
    if (strcmp(fileName, "..") == 0)
    {
        printf("rmdir: cannot remove .. directory\n");
        return -1;
    }
    if (strcmp(fileName, "/") == 0)
    {
        printf("rmdir: cannot remove / directory\n");
        return -1;
    }

    // get parent inode number
    int pino = getino(pathname);
    MINODE *pip = iget(dev, pino);

    // get parent minode with parent inode number
    MINODE *mip = iget(dev, ino);

    // check for valid parent MINODE
    if (!pip)
    {
        printf("rmdir: parent directory not found\n");
        return -1;
    }

    // find filename in parent directory
    if (checkDir(pip, pino, fileName) != 1)
    {
        printf("rmdir: file not found\n");
        return -1;
    }

    // if (mip->refCount > 1)
    // {
    //     printf("rmdir: node in use, cannot rmdir\n");
    //     return -1;
    // }
    if (mip->INODE.i_blocks > 2)
        return -1;

    char buf[BLKSIZE], name[256], *cp;
    DIR *dp;
    MINODE *temp;

    //check all inode blocks, don't quite understand why, but rmdir example code does it.
    for (int i = 2; i < 12; i++)
    {
        // check if direct block where our directories are is valid
        if (mip->INODE.i_block[i])
        {
            printf("rmdir: cannot rmdir, dir not empty\n");
            return -1;
        }

        // set block content to buf
        get_block(dev, mip->INODE.i_block[i], buf);
        cp = buf;
        dp = (DIR *)buf;

        // traverse directories utilizing dp and cp
        // while (cp < &buf[BLKSIZE])
        // {
        //     // handle directory name properly
        //     strncpy(name, dp->name, dp->name_len);
        //     name[dp->name_len] = 0;

        //     // set inode number to that of the current directory
        //     ino = dp->inode;

        //     if (name[0] != 0)
        //     {
        //         printf("cannot rmdir, dir not empty\n");
        //         return 1;
        //     }

        //     // advance to next record and set directory pointer to next directory
        //     cp += dp->rec_len;
        //     dp = (DIR *)cp;
        //     iput(temp);
        // }
    }

    //dir is now confirmed not to be empty, and rmdir can begin
    printf("trying to rm child\n");
    idalloc(dev, ino);
    rm_child(pip, fileName, ino);
    pip->INODE.i_links_count--; // We just lost ".." from the deleted child
    pip->INODE.i_mtime = time(0L);
    pip->INODE.i_atime = pip->INODE.i_mtime;
    pip->dirty = 1;
    iput(pip);
    return 0;
}
