#include "type.h"

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC proc[NPROC], *running;

extern char gpath[128];
extern char *name[64];
extern int n;

extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, iblk;
extern char line[128], cmd[32], pathname[128], destination[128];

// ************ link *****************
int myLink()
{
    // oino refers to source file's (oldfile) inode number
    // npino refers to destination file's (newfile) intended parent inode number
    int oino, npino;

    // omip refers to source file's (oldfile) MINODE using oino
    // npmip refers to destination file's (newfile) intended parent MINODE using parent inode number
    MINODE *omip, *npmip;

    // oip refers to source file's (oldfile) INODE information
    // npip refers to destination file's (newfile) intended parent INODE information
    INODE *oip, *npip;

    // oldfile will be used to store the source file's path
    // newfile will be used to store the destination file's path
    // buf will be used as an intermediate to extract the destination file's dirname and basename
    char oldfile[64], newfile[64], buf[64];

    // destination file's dirname and basename respectively
    char newFileParentPath[64], newFileChildPath[64];

    // check if global pathname and destination variables fit requirements (populated)
    if(pathname[0] == 0)
    {
        printf("link: no source file\n");
        return -1;
    }
    if(destination[0] == 0)
    {
        printf("link: no destination file\n");
        return -1;
    }

    strcpy(oldfile, pathname);
    strcpy(newfile, destination);

    // get inode of old file and create MINODE in memory with it
    oino = getino(oldfile);
    omip = iget(dev, oino);

    // check if new file already exists
    if(getino(newfile))
    {
        printf("link: destination file already exists\n");
        return -1;
    }
    if(!omip)
    {
        printf("link: source file does not exist\n");
        return -1;
    }
    if(S_ISDIR(omip->INODE.i_mode))
    {
        printf("link: source file is a directory\n");
        return -1;
    }

    // check if destination path starts from root
    // if not set newFileParentPath to the dirname of newfile using buf as an intermediate so we dont lose data
    if(strcmp(newfile, "/") == 0)
    {
        strcpy(newFileParentPath, "/");
    }
    else
    {
        strcpy(buf, newfile);
        strcpy(newFileParentPath, dirname(buf));
    }

    // extract basename from newfile using buf again as an intermediate
    strcpy(buf, newfile);
    strcpy(newFileChildPath, basename(buf));
    // printf("oldfile: %s, newfile: %s\n", oldfile, newfile);
    // printf("newFileParentPath: %s, newFileChildPath: %s\n", newFileParentPath, newFileChildPath);

    // get parent (pino and pmip) of new file path
    npino = getino(newFileParentPath);
    npmip = iget(dev, npino);

    if(!npmip)
    {
        printf("link: parent of destination file was not found\n");
        return -1;
    }
    if(!S_ISDIR(npmip->INODE.i_mode))
    {
        printf("link: parent of destination file is not a directory");
        return -1;
    }

    // used for enterchild
    DIR newFileEntry;
    newFileEntry.inode = oino;
    strncpy(newFileEntry.name, newFileChildPath, strlen(newFileChildPath));
    newFileEntry.name_len = strlen(newFileChildPath);
    newFileEntry.rec_len = idealLength(newFileEntry.name_len);

    enter_child(npmip, &newFileEntry);

    // increment INODE's link count and set dirty
    omip->INODE.i_links_count++;
    omip->dirty = 1;
    iput(omip);
    iput(npmip);
    return 0;
}

// ************ unlink *****************

