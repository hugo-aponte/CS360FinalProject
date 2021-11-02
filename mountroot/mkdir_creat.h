#include "type.h"
#include "util.h"

#ifndef LINK_UNLINK_H
#define LINK_UNLINK_H

int tst_bit(char *buf, int bit);
int set_bit(char *buf, int bit);
int balloc(int dev);
int ialloc(int dev);

#endif