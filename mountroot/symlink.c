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

// ************ readlink *****************
int readLink(char *filePath, char *buf)
{
    // get ino associated with filePath
    int ino = getino(filePath);

    // get MINODE associated with retrieved ino 
    MINODE *mip = iget(dev, ino);

    // set buf to null;
    buf = NULL;

    // check if MINODE is a link type file
    if(!S_ISLNK(mip->INODE.i_mode))
    {
        printf("readlink: file %s is not a symbolic link\n", basename(filePath));
        return -1;
    }

    // extract target block from MINODE's INODE information
    char *target = (char *)(mip->INODE.i_block);

    // allocate proper space in buf
    buf = (char*)malloc((strlen(target) + 1)*sizeof(char));

    // set buf to target and set last index null
    strcpy(buf, target);
    buf[strlen(buf)] = '\0';
    printf("buf=%s\n", buf);

    // return file size
    return strlen(buf);
}

// ************ symlink *****************
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