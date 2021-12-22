#ifndef FILESYS_H
#define FILESYS_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void emu_FilesysInit(void);
extern int emu_FileOpen(char * filename);
extern int emu_FileRead(int fd, char * buf, int size);
extern char emu_FileGetc(int fd);
extern int emu_FileSeekSet(int fd, int seek);
extern int emu_FileSeekCur(int fd, int seek);
extern void emu_FileClose(int fd);
extern int emu_FileSize(char * filename);

#ifdef __cplusplus
}
#endif

#endif /* FILESYS_H */
