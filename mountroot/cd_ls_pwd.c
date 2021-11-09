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

int ls_file(MINODE *mip, char *name)
{
    // printf("ls_file: to be done: READ textbook!!!!\n");
    // READ Chapter 11.7.3 HOW TO ls
    // printf("hello");
    // MINODE *curPath;
    // int curIno = getino(pathname);
    // curPath = iget(dev, curIno);

    // if (S_ISDIR(curPath->INODE.i_mode))
    // {
    //   ls_dir(curPath);
    // }
    // else
    // {
    //   printf("ERROR: not a directory\n");
    // }

    // set INODE parent to mip->INODE | this represents the parent directory
    INODE *parentip = &mip->INODE;
    u16 mode = parentip->i_mode;

    // track calendar time to include in file description
    time_t calendarTime = parentip->i_ctime;
    char *mtime = ctime(&calendarTime);
    mtime[strlen(mtime) - 1] = '\0';

    // extract file type
    u16 type = mode & 0xF000;

    // check if file type if a file or directory
    if (type == 0x4000)
        printf("d");
    else if (type == 0x8000)
        printf("-");
    else if (type == 0xA000)
        printf("l");
    else
        printf("-");

    // check permissions associated with current file to print in file description
    ((mode & S_IRUSR) == S_IRUSR) ? printf("r") : printf("-");
    ((mode & S_IWUSR) == S_IWUSR) ? printf("w") : printf("-");
    ((mode & S_IXUSR) == S_IXUSR) ? printf("x") : printf("-");
    ((mode & S_IRGRP) == S_IRGRP) ? printf("r") : printf("-");
    ((mode & S_IWGRP) == S_IWGRP) ? printf("w") : printf("-");
    ((mode & S_IXGRP) == S_IXGRP) ? printf("x") : printf("-");
    ((mode & S_IROTH) == S_IROTH) ? printf("r") : printf("-");
    ((mode & S_IWOTH) == S_IWOTH) ? printf("w") : printf("-");
    ((mode & S_IXOTH) == S_IXOTH) ? printf("x") : printf("-");

    // print further current file descriptions
    printf("%4d%4d%4d  %s%8d  %s", parentip->i_links_count, parentip->i_gid, parentip->i_uid, mtime, parentip->i_size, name);

    // check for symlink file, and show the file if it exists
    ((mode & 0120000) == 0120000) ? printf(" => %s\n", (char *)(mip->INODE.i_block)) : printf("\n");
}

int ls_dir(MINODE *mip, int dev)
{
    // printf("ls_dir: list CWD's file names; YOU FINISH IT as ls -l\n");

    // char buf[BLKSIZE], temp[256];
    // DIR *dp;
    // char *cp;

    // get_block(dev, mip->INODE.i_block[0], buf);
    // dp = (DIR *)buf;
    // cp = buf;

    // while (cp < buf + BLKSIZE)
    // {
    //   struct stat *buf;
    //   buf = malloc(sizeof(struct stat));
    //   strncpy(temp, dp->name, dp->name_len);
    //   temp[dp->name_len] = 0;

    //   stat(temp, buf);

    //   // if (dp->pid == 0)
    //   // {
    //   //   printf("root\t");
    //   // }
    //   // else
    //   // {
    //   //   printf("user\t");
    //   // }

    //   printf("%ld\t", (long)buf->st_size);
    //   printf("%ld\t", (long)buf->st_mtim.tv_sec);
    //   printf("%s\n", temp);

    //   cp += dp->rec_len;
    //   dp = (DIR *)cp;
    //   free(buf);
    // }
    // printf("\n");

    char buf[BLKSIZE], name[256], *cp;
    DIR *dp;
    int i, ino;
    MINODE *temp;

    //check all inode blocks, don't quite understand why, but rmdir example code does it.
    for (int i = 0; i < 12; i++)
    {
        // check if direct block where our directories are is valid
        if (!mip->INODE.i_block[i])
            break;
        // set block content to buf
        get_block(dev, mip->INODE.i_block[i], buf);
        cp = buf;
        dp = (DIR *)buf;

        // traverse directories utilizing dp and cp
        while (cp < &buf[BLKSIZE])
        {
            // handle directory name properly
            strncpy(name, dp->name, dp->name_len);
            name[dp->name_len] = 0;

            // set inode number to that of the current directory
            ino = dp->inode;

            // set temp to MINODE from iget call using global dev and directory inode number
            temp = iget(dev, ino);

            // print file from ls_file using MINODE temp and directory name
            ls_file(temp, name);

            // advance to next record and set directory pointer to next directory
            cp += dp->rec_len;
            dp = (DIR *)cp;
            iput(temp);
        }
    }
}

int ls()
{
    // printf("ls: list CWD only! YOU FINISH IT for ls pathname\n");
    // if (!pathname[0])
    // {
    //   ls_dir(running->cwd);
    // }
    // else
    // {
    //   ls_file(running->cwd, pathname);
    // }

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

void rpwd(MINODE *wd)
{
    char buf[BLKSIZE], dirname[BLKSIZE];
    int my_ino, parent_ino;

    DIR *dp;
    char *cp;

    // parent minode
    MINODE *parentMINODE;

    if (wd == root)
        return;

    // get dir block of cwd
    get_block(wd->dev, wd->INODE.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;

    // search through cwd for my_ino and parent ino
    while (cp < buf + BLKSIZE)
    {
        strcpy(dirname, dp->name);
        dirname[dp->name_len] = '\0';

        // check for "." dir
        if (strcmp(dirname, ".") == 0)
        {
            my_ino = dp->inode;
        }

        // check for ".." dir
        if (strcmp(dirname, "..") == 0)
        {
            parent_ino = dp->inode;
        }

        // advance to next record
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }

    parentMINODE = iget(wd->dev, parent_ino);
    get_block(wd->dev, parentMINODE->INODE.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;

    while (cp < buf + BLKSIZE)
    {
        strncpy(dirname, dp->name, dp->name_len);
        dirname[dp->name_len] = 0;

        // check if we found directory associated with my_ino
        if (dp->inode == my_ino)
        {
            break;
        }

        // advance to next record
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }
    rpwd(parentMINODE);
    iput(parentMINODE);

    printf("/%s", dirname);
    return;
}

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