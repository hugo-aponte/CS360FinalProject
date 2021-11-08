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

int mySymLink()
{
    // note: comment references to destination = comment references to new file 
    // printf("Inside mySymLink\n");

    // nino will be used for the new file's inode number 
    // npino will be used for the new file's parent's inode number
    int ino, nino, npino;

    // nmip will be used for the new file's MINODE associated with nino
    // npmip will be used for the new file's parent's MINODE associated with npino
    // the INODEs are simply references to the INODEs of their corresponding MINODEs
    MINODE *mip, *nmip, *npmip;
    INODE *ip, *nip, *pip;

    char buf[64], destParentPath[64], destChildName[64], oldFileName[64];
    
    // extract oldFileName from global pathname using buf as an intermediate 
    strcpy(buf, pathname);
    strcpy(oldFileName, basename(buf));
    // printf("oldFileName=%s\n", oldFileName);

    // get inode number associated with source file (using global pathname)
    ino = getino(pathname);

    // get MINODE associated with inode number 
    mip = iget(dev, ino);

    // check if oldFileName is longer than book requirement
    if(strlen(oldFileName) > 60)
    {
        printf("symlink: source file name is too long\n");
        return -1;
    }

    // check if source file exists
    if(!mip)
    {
        printf("symlink: source file %s does not exist\n", pathname);
        return -1;
    }

    // extract destParentPath and destChildName from destination using dirname and basename
    strcpy(destParentPath, destination);
    strcpy(destChildName, destination);
    strcpy(destParentPath, dirname(destParentPath));
    strcpy(destChildName, basename(destChildName));
    // printf("destParentPath=%s, destChildName=%s\n", destParentPath, destChildName);

    // get inode number associated with destParentPath
    npino = getino(destParentPath);
    npmip = iget(dev, npino);

    // check if parent directory exists
    if(!npmip)
    {
        printf("symlink: destination file's parent directory was not found\n");
        return -1;
    }
    if(!S_ISDIR(npmip->INODE.i_mode))
    {
        printf("symlink: destination file's parent is not a directory\n");
    }
    if(getino(destChildName))
    {
        printf("symlink: destination file already exists\n");
        return -1;
    }

    // get inode number associated with destination file
    nino = kcreat(npmip, destChildName);
    // printf("After kcreat, nino=%d\n", nino);

    // get MINODE from nino
    nmip = iget(dev, nino);
    nip = &nmip->INODE;

    // set nip to link type, i_size to oldFileName size, and i_block to oldFileName
    nip->i_mode = S_IFLNK | S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
    nip->i_size = strlen(oldFileName);
    strcpy((char *)(nip->i_block), oldFileName);
    nmip->dirty = 1;
    npmip->dirty = 1;
    iput(nmip);
    iput(npmip);

    // printf("Exit symlink\n");
    return 0;
}