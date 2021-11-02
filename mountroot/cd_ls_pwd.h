#include "type.h"
#include "util.h"

#ifndef LS_PWD_CD
#define LS_PWD_CD

int cd(char *pathname, int dev, PROC *running);
int ls(char *pathname, int dev, PROC *running, MINODE *root);
int ls_dir(MINODE *mip, int dev);
int ls_file(MINODE *mip, char *name);
void pwd(MINODE *wd, MINODE *root);
int quit();

#endif
