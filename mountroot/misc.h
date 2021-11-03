#ifndef MISC_H
#define MISC_H

#include <type.h>

int stat(int dev, char *pathname);
int chmod(int dev, char *pathname, u16 mode);

#endif