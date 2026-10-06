#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
/* decode a descrambled Roland wave ROM as one continuous DPCM stream (PCM chip rules:
   shift=(10-nibble)&15, round-half via low bit, 20-bit clipping accumulator) */
static int32_t addclip20(int32_t a,int32_t b,int c){int32_t s=a+b+c; if(s>0x7ffff)s=0x7ffff; if(s<-0x80000)s=-0x80000; return s;}
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  uint8_t*d=malloc(n); fread(d,1,n,f); fclose(f);
  int32_t*o=malloc(n*4); int32_t ref=0;
  for(long a=0;a<n;a++){
    uint8_t sb=d[((a&0xFFFFF)>>5)|(a&0xF00000)];
    int nib=(a&0x10)?(sb>>4):(sb&15);
    int shift=(10-nib)&15;
    int32_t pre=(int32_t)(int8_t)d[a]<<10;
    int32_t sh=(pre<<1)>>shift;
    ref=addclip20(ref,sh>>1,sh&1);
    o[a]=ref;
  }
  FILE*g=fopen(argv[2],"wb"); fwrite(o,4,n,g); fclose(g); return 0;
}
