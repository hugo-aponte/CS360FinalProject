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

// ************ mkdir *****************
int enter_child(MINODE *pip, DIR *fPtr)
{
    //hard part
    // printf("inside enter_child\n");

    // pip is our parent minode, fptr is our new directory 
    // printf("Parent->ino=%d, new_dir ino=%d, new_name=%s\n", pip->ino, fPtr->inode, fPtr->name);

    char *cp;
    char buf[BLKSIZE];
    int remain;
    int ideal_length = idealLength(strlen(fPtr->name));
    DIR *dp, *newFile;


    for (int i = 0; i < 12; i++)
    {
        // printf("inside for loop\n");

        if (pip->INODE.i_block[i] == 0)
            break;

        get_block(pip->dev, pip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        // find last directory entry
        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            // printf("inside for ~ inside while\n");
            if(dp->rec_len == 0)
                break;

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        // cp and dp should be ready to enter fptr        
        // printf("last entry is %s\n", dp->name);
        cp = (char*)dp;

        // remaining bytes in current block;
        remain = dp->rec_len;
        // printf("remain %d ideal_length %d\n", remain, ideal_length);

        if (remain >= ideal_length)
        {
            // printf("inside for ~ inside if condition\n");
            dp->rec_len = idealLength(strlen(dp->name));
            remain -= dp->rec_len;

            cp += dp->rec_len;
            newFile = (DIR *)cp;

            newFile->inode = fPtr->inode;
            newFile->name_len = fPtr->name_len;

            // newFile should take over remaining size
            newFile->rec_len = remain;
            strncpy(newFile->name, fPtr->name, fPtr->name_len);
            // printf("putting this at pip->INODE.i_block[%d]=%d | newfile: inode=%d rec_len=%d name_len=%d\n", i, pip->INODE.i_block[i], newFile->inode, newFile->rec_len, newFile->name_len);

            put_block(pip->dev, pip->INODE.i_block[i], buf);
        }
    }

    // printf("exiting enter_child\n");

    return 0;
}

DIR kmkdir(MINODE *pmip, char *fileName)
{
    // printf("Inside kmkdir\n");
    char buf[BLKSIZE];
    int ino = ialloc(dev);
    int blk = balloc(dev);
    DIR dirPtr;

    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x41ED;      // 040755: DIR type and permissions
    ip->i_uid = running->uid; // owner uid
    ip->i_gid = running->gid; // group Id
    ip->i_size = BLKSIZE;     // size in bytes
    ip->i_links_count = 2;    // links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 2;     // linux: blocks count in 512-byte chunks
    ip->i_block[0] = blk; // new DIR has one data block
    
    for (int i = 1; i < 15; i++)
        ip->i_block[i] = 0; // setting all other blocks to 0
    
    mip->dirty = 1; // mark minode dirty
    iput(mip); // write INODE to disk

    bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
    get_block(mip->dev, blk, buf); // initialize new allocated block
    DIR *dp = (DIR *)buf;

    // make . entry
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->name[0] = '.';
    dp->file_type = (u8)EXT2_FT_DIR;
    // printf("dp: inode=%d, rec_len=%d, name_len=%d, dp->name=%c\n", dp->inode, dp->rec_len, dp->name_len, dp->name[0]);

    // make .. entry: pmip->ino=parent DIR ino, blk=allocated block
    dp = (DIR *)((char *)dp + dp->rec_len);
    dp->inode = pmip->ino;
    dp->rec_len = BLKSIZE - 12; // rec_len spans block
    dp->name_len = 2;
    dp->name[0] = dp->name[1] = '.';
    dp->file_type = (u8)EXT2_FT_DIR;
    // printf("dp: inode=%d, rec_len=%d, name_len=%d, dp->name=%c%c\n", dp->inode, dp->rec_len, dp->name_len, dp->name[0], dp->name[1]);

    put_block(mip->dev, blk, buf); // write to blk on disk
    
    // printf("past put_block ~ created new dir\n");
    dirPtr.inode = ino;
    strncpy(dirPtr.name, fileName, strlen(fileName));
    dirPtr.name_len = strlen(fileName);
    dirPtr.rec_len = 4 * ((8 + dirPtr.name_len + 3) / 4);
    // printf("new dir name: %s\n", dirPtr.name);

    enter_child(pmip, &dirPtr);
    // printf("past enter_child");

    pmip->INODE.i_atime = time(0L);
    pmip->dirty = 1;
    iput(pmip);
    // printf("exiting kmkdir\n");
}

int myMkdir()
{
    int ino;
    char fileName[128];
    MINODE *currentMinode;

    // check pathname for root or cwd
    if (pathname[0] == '/')
    {
        currentMinode = root;
    }
    else
    {
        currentMinode = running->cwd;
    }

    // get parentpath and basename
    strcpy(fileName, pathname);
    strcpy(pathname, dirname(pathname));
    strcpy(fileName, basename(fileName));
    printf("path: %s ", pathname);
    printf("file: %s ", fileName);

    // get parent inode number
    int pino = getino(pathname);
    
    // get parent minode with parent inode number
    MINODE *pmip = iget(dev, pino);

    // check if parent minode is a directory
    if (S_ISDIR(pmip->INODE.i_mode))
    {
        // check if basename already exists in parent minode 
        ino = search(pmip, fileName);
        if (ino)
        {
            printf("file %s already exists\n", fileName);
            return -1;
        }
    }
    else
    {
        printf("file %s is not a dir\n", pathname);
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
    if (pathname[0] == '/')
    {
        currentMinode = root;
    }
    else
    {
        currentMinode = running->cwd;
    }

    // get parentpath and basename
    strcpy(fileName, pathname);
    strcpy(pathname, dirname(pathname));
    strcpy(fileName, basename(fileName));
    printf("path: %s ", pathname);
    printf("file: %s ", fileName);

    // get parent inode number
    int pino = getino(pathname);
    
    // get parent minode with parent inode number
    MINODE *pmip = iget(dev, pino);

    // check if parent minode is a directory
    if (S_ISDIR(pmip->INODE.i_mode))
    {
        // check if basename already exists in parent minode 
        ino = search(pmip, fileName);
        if (ino)
        {
            printf("file %s already exists\n", fileName);
            return -1;
        }
    }
    else
    {
        printf("file %s is not a dir\n", pathname);
    }

    // create the new file in kcreat
    kcreat(pmip, fileName);

    return 0;   
}

int kcreat(MINODE *pmip, char *filename)
{
    printf("inside my_creat\n");
    printf("pmip->ino=%d, filename=%s\n", pmip->ino, filename);

    // get ino available and blk numbers
    int ino = ialloc(dev);
    int blk = balloc(dev);

    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    // write INODE into memory inode
    ip->i_mode = 0x81A4; // File type and permissions
    ip->i_uid = running->uid; // user id
    ip->i_gid = running->gid; // group id
    ip->i_size = 0;
    ip->i_links_count = 1;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_blocks = 2;
    ip->i_block[0] = blk;
    mip->refCount = 0;

    for(int i = 1; i < 13; i++)
    {
        ip->i_block[i] = 0; // set other inode blocks to 0
    }

    mip->dirty = 1;
    iput(mip);

    // create new directory entry for new file
    DIR newdir;
    newdir.inode = ino;
    strncpy(newdir.name, filename, strlen(filename));
    newdir.name_len = strlen(filename);
    newdir.rec_len = idealLength(newdir.name_len);

    // enter file entry into parent directory 
    enter_child(pmip, &newdir);

    pmip->INODE.i_atime = time(0L);
    pmip->dirty = 1;

    iput(pmip);

    // when this function is used by symlink, it will expect the inode number to be returned
    return ino;
}