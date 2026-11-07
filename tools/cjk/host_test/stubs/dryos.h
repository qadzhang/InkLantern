/* 宿主测试 stub：替代 dryos.h（最小面，只覆盖 rbf_font.c 用到的符号） */
#ifndef STUB_DRYOS_H
#define STUB_DRYOS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FAST
#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#define ABS(a) ((a)<0?-(a):(a))
#define SGN(a) ((a)<0?-1:1)
#define COERCE(x,lo,hi) MIN(MAX(x,lo),hi)

#define O_RDONLY 0
#define O_SYNC   0

typedef void* ML_FILE;   /* rbf_font.c: FILE *fd = FIO_OpenFile(...) */

/* FIO 存根：字体加载走 read_entire_file，不需要真实实现 */
ML_FILE   FIO_OpenFile(const char* path, int flags);
int       FIO_ReadFile(ML_FILE fd, void* buf, int len);
int       FIO_CloseFile(ML_FILE fd);
int       FIO_SeekSkipFile(ML_FILE fd, int off, int whence);
int       FIO_GetFileSize(const char* path, uint32_t* size);
void*     fio_malloc(int len);
void      fio_free(void* p);

void*     read_entire_file(const char* path, int* size);
void      DryosDebugMsg(int a, int b, const char* fmt, ...);

#endif
