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

int enter_child(MINODE *pip, DIR *fPtr)
{
    //hard part
    // printf("inside enter_child\n");

    // pip is our parent minode, fptr is our new directory
    // printf("Parent->ino=%d, new_dir ino=%d, new_name=%s\n", pip->ino, fPtr->inode, fPtr->name);

    char *cp;
    char buf[BLKSIZE];
    int remain;
    DIR *dp, *newFile;

    for (int i = 0; i < 12; i++)
    {
        printf("inside block%d\n", i);
        get_block(pip->dev, pip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        printf("block value = %d\n", pip->INODE.i_block[i]);

        if (pip->INODE.i_block[i] == 0)
        {
            int nblk = balloc(dev);
            pip->INODE.i_block[i] = nblk;
            pip->INODE.i_blocks++;
            printf("adding to empty blocks\n");
            printf("\n\n-->pip_iblocks = %u<--\n\n", pip->INODE.i_blocks);
            newFile = dp;
            newFile->rec_len = BLKSIZE;
            newFile->inode = fPtr->inode;
            newFile->name_len = fPtr->name_len;
            strncpy(newFile->name, fPtr->name, fPtr->name_len);
            //printf("name %s\n", newFile->name);
            put_block(pip->dev, pip->INODE.i_block[i], buf);
            return 0;
        }
        else
        {
            // find last directory entry
            while (cp + dp->rec_len < buf + BLKSIZE)
            {
                // printf("inside for ~ inside while\n");
                if (dp->rec_len == 0)
                    break;

                cp += dp->rec_len;
                dp = (DIR *)cp;
            }

            // cp and dp should be ready to enter fptr
            // printf("last entry is %s\n", dp->name);
            cp = (char *)dp;

            // remaining bytes in current block;
            remain = dp->rec_len - idealLength(dp->name_len);
            // printf("remain %d ideal_length %d\n", remain, ideal_length);

            if (remain >= idealLength(fPtr->name_len))
            {
                // printf("inside for ~ inside if condition\n");
                dp->rec_len = idealLength(strlen(dp->name));

                cp += dp->rec_len;
                newFile = (DIR *)cp;

                newFile->inode = fPtr->inode;
                newFile->name_len = fPtr->name_len;

                // newFile should take over remaining size
                newFile->rec_len = remain;
                strncpy(newFile->name, fPtr->name, fPtr->name_len);
                // printf("putting this at pip->INODE.i_block[%d]=%d | newfile: inode=%d rec_len=%d name_len=%d\n", i, pip->INODE.i_block[i], newFile->inode, newFile->rec_len, newFile->name_len);

                put_block(pip->dev, pip->INODE.i_block[i], buf);
                break;
            }
        }
    }

    // printf("exiting enter_child\n");

    return 0;
}

// ************ mkdir *****************
int myMkdir()
{
    int ino;
    char fileName[128];
    MINODE *currentMinode;

    makePath(fileName, currentMinode);

    // get parent inode number
    int pino = getino(pathname);

    // get parent minode with parent inode number
    MINODE *pmip = iget(dev, pino);

    if (checkDir(pmip, pino, fileName))
    {
        return 1;
    }

    // create the new directory in kmkdir
    kmkdir(pmip, fileName);

    return 0;
}

// ************ creat *****************
int myCreat()
{
    int ino;
    char fileName[128];
    MINODE *currentMinode;

    // check pathname for root or cwd
    makePath(fileName, currentMinode);

    // get parent inode number
    int pino = getino(pathname);

    // get parent minode with parent inode number
    MINODE *pmip = iget(dev, pino);

    // check if parent minode is a directory
    if (checkDir(pmip, pino, fileName))
    {
        return 1;
    }

    // create the new file in kcreat
    kcreat(pmip, fileName);

    return 0;
}