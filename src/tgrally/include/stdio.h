#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
typedef struct _iobuf FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 1024
int printf(const char*,...); int sprintf(char*,const char*,...);
int fprintf(FILE*,const char*,...); int sscanf(const char*,const char*,...);
int vsprintf(char*,const char*,char*);
FILE *fopen(const char*,const char*); int fclose(FILE*); int fflush(FILE*);
size_t fread(void*,size_t,size_t,FILE*); size_t fwrite(const void*,size_t,size_t,FILE*);
int fseek(FILE*,long,int); long ftell(FILE*); void rewind(FILE*);
char *fgets(char*,int,FILE*); int fgetc(FILE*); int fputc(int,FILE*);
int fputs(const char*,FILE*); int puts(const char*); int remove(const char*);
#endif
