#include "filesys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FD 16

#include "ff.h"
static FATFS fatfs;
static FIL file[MAX_FD]; 
static int fpos[MAX_FD]; 
extern int sd_init_driver(void);


/************************************************
 * Filesystem
 ************************************************/
void emu_FilesysInit(void)
{
    sd_init_driver(); 
    FRESULT fr = f_mount(&fatfs, "0:", 1);
    for (int i=0; i<MAX_FD; i++) {
      fpos[i] = -1; // fpos = -1 means FD available
    }   
}

int emu_FileOpen(char * filename)
{
  int fd = -1;

  for (int i=0; i<MAX_FD; i++) {
    if (fpos[i] < 0) {
      fd = i;
      fpos[fd] = 0;
      break;
    }   
  } 
  if (fd >= 0) {
    if( !(f_open(&file[fd], filename, FA_READ)) ) {
    }
    else {
      //emu_printf("FileOpen failed");
      fd = -1;
      fpos[fd] = -1; 
    }
  }

  return (fd);
}

int emu_FileRead(int fd, char * buf, int size)
{
  unsigned int retval=0;

  f_read (&file[fd], buf, size, &retval);
  if (retval > 0) fpos[fd] += retval;
  
  return retval;
}

char emu_FileGetc(int fd) {
  char c;
  unsigned int retval=0;
  
  if( !(f_read (&file[fd], &c, 1, &retval)) ) {
    if (retval != 1) {
      c = EOF;
    //emu_printf("emu_FileGetc failed");
    }
    else {
        fpos[fd] += 1;  
    }   
  }   
  return c; 
}

void emu_FileClose(int fd)
{
  f_close(&file[fd]); 
  fpos[fd] = -1;
}

int emu_FileSeekSet(int fd, int seek) 
{
  f_lseek(&file[fd], seek);
  fpos[fd] = seek;
  return (seek);
}

int emu_FileSeekCur(int fd, int seek) 
{
  f_lseek(&file[fd], fpos[fd] + seek);
  fpos[fd] += seek;
  return (fpos[fd]);
}

int emu_FileTell(int fd) 
{
  return (f_tell(&file[fd]));
}

int emu_FileSize(char * filename) 
{
  int filesize=0;
  FILINFO entry;
  f_stat(filename, &entry);
  filesize = entry.fsize; 
  return(filesize);    
}

