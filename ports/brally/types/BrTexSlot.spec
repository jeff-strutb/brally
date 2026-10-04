# size 0xD8
# header ports/brally/include/br_dl.h
# headers br_dl.h
0x0000  int32_t                       f00                 cleared on allocation
0x0004  int32_t                       bLive               nonzero while the slot holds a texture
0x0008  int32_t                       f08
0x000C  int32_t                       f0C
0x0010  int32_t                       aspect
0x0014  void *                        pData               the texels last downloaded (also info.data)
0x0018  int32_t                       format
0x001C  int32_t                       mipMode
0x0020  int32_t                       magFilter
0x0024  int32_t                       minFilter
0x0028  int32_t                       clampS
0x002C  int32_t                       clampT
0x0030  int32_t                       f30
0x0034  int32_t                       f34
0x0038  int32_t                       lodBias             bias * 0x1007745C, read back as unsigned * 0x10077460
0x003C  int32_t                       smallLod
0x0040  int32_t                       largeLod
0x0044  int32_t                       tmu
0x0048  int32_t                       evenOdd
0x004C  uint32_t                      start               TMEM start address
0x0050  int32_t                       lodBlend
0x00C4  GrTexInfo                     info
