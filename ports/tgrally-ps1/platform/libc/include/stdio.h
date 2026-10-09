/* stdio.h: the port's C library (platform/libc).  No files: the console's
 * output is the TTY, a host's files are reached over PCDRV (pcdrv.h). */
#ifndef PS1_STDIO_H
#define PS1_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct PS1_FILE FILE;
extern FILE *const stderr, *const stdout;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *dst, const char *fmt, ...);
int snprintf(char *dst, size_t n, const char *fmt, ...);
int vsprintf(char *dst, const char *fmt, va_list ap);
int vsnprintf(char *dst, size_t n, const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);
int puts(const char *s);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t size, size_t n, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
int fflush(FILE *f);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int fgetc(FILE *f);
int rename(const char *from, const char *to);
int remove(const char *path);
char *fgets(char *s, int n, FILE *f);
int vfprintf(FILE *f, const char *fmt, va_list ap);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
int putchar(int c);
#endif
