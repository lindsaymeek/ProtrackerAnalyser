
#ifndef _LANG_H
#define _LANG_H

/* parallel processing constructs */

void read3(char *variable,void *addr,short size);
void write3(char *variable,void *addr,short size);
int fork(void (*pc)(void),int pri);
void fork_wait(void);
int changepri(int pri);

#define write2(x,y) write3((x),(y),sizeof(*(y)) )
#define read2(x,y) read3((x),(y),sizeof(*(y)) )

#endif
