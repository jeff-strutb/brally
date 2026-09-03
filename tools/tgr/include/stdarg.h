#ifndef _STDARG_H
#define _STDARG_H
typedef char *va_list;
#define va_start(ap,p) (ap=(va_list)&(p)+sizeof(p))
#define va_arg(ap,t)   (*(t*)((ap+=sizeof(t))-sizeof(t)))
#define va_end(ap)     (ap=0)
#endif
