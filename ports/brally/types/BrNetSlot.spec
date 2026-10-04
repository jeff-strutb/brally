# size 0x978
# header ports/brally/include/slice1_02.h
# headers slice1_02.h
0x0000  void *                        hMutex              guards this record
0x0004  int32_t                       f004                matched against the argument of 0x10005FE0
0x0008  int32_t                       f008
0x000C  uint32_t[8]                   f00C
0x002C  int32_t                       f02C                low 6 bits are flags (see 0x10005FE0)
0x0030  int32_t                       f030                not cleared by BrNetReset
0x0034  char[4]                       f034
0x0038  int32_t[8]                    f038
0x0058  BrCarState[8]                 cars
0x0558  int32_t                       f558
0x055C  int32_t                       f55C
0x0560  int32_t                       f560                reset to -1, not 0
0x0564  int32_t                       f564
0x0568  int32_t                       f568
0x056C  int32_t                       f56C
0x0570  char[0x400]                   szName              NUL-terminated player name
0x0970  int32_t                       f970
0x0974  int32_t                       f974
