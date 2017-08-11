#ifndef _GETCH_H_
#define _GETCH_H_
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
extern char _getch();
extern int _kbhit(void);

#endif //_GETCH_H_
