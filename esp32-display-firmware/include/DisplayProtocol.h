#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
struct DisplayFrame {
 uint32_t sequence = 0;
 int64_t epoch = 0;
 int32_t offset = 0;
 bool validTime = false;
 char mode[16] = {}, track[192] = {}, title[256] = {}, artist[192] = {};
};
inline int hexDigit(char c) {
 if(c>='0'&&c<='9') return c-'0';
 if(c>='a'&&c<='f') return c-'a'+10;
 if(c>='A'&&c<='F') return c-'A'+10;
 return -1;
}
inline bool decodeField(const char *src, char *dst, size_t cap) {
 size_t n=0;
 for(size_t i=0; src[i]; ++i) {
  unsigned char c=src[i];
  if(c=='%') { if(!src[i+1]||!src[i+2]) return false; int a=hexDigit(src[i+1]),b=hexDigit(src[i+2]); if(a<0||b<0)return false; c=(a<<4)|b;i+=2; }
  if(c<32||c==127||n+1>=cap) return false;
  dst[n++]=c;
 }
 dst[n]=0; return true;
}
inline bool frameInteger(const char *s, int64_t low, int64_t high, int64_t &value) {
 if(!*s) return false;
 char *end;errno=0;long long n=strtoll(s,&end,10);
 if(errno||*end||n<low||n>high)return false;
 value=n;return true;
}
inline bool parseDisplayFrame(char *line, DisplayFrame &out) {
 char *f[9];size_t n=1;f[0]=line;
 for(char *p=line;*p;++p)if(*p=='\t') { if(n==9)return false;*p=0;f[n++]=p+1; }
 if(n!=9||strcmp(f[0],"P5")) return false;
 DisplayFrame v;int64_t number;
 if(!frameInteger(f[1],0,UINT32_MAX,number))return false;v.sequence=number;
 if(!frameInteger(f[2],0,4102444800LL,number))return false;v.epoch=number;
 if(!frameInteger(f[3],-50400,50400,number))return false;v.offset=number;
 if(strcmp(f[4],"0")&&strcmp(f[4],"1"))return false;v.validTime=!strcmp(f[4],"1");
 if(!decodeField(f[5],v.mode,sizeof v.mode)||!decodeField(f[6],v.track,sizeof v.track)||!decodeField(f[7],v.title,sizeof v.title)||!decodeField(f[8],v.artist,sizeof v.artist))return false;
 out=v;return true;
}

inline bool parseDisplayControl(char *line, bool &enabled, uint8_t &brightness) {
 if(strncmp(line,"P5D\t",4))return false;
 char *a=line+4,*b=strchr(a,'\t');if(!b)return false;*b++=0;if(strchr(b,'\t'))return false;
 int64_t on,level;if(!frameInteger(a,0,1,on)||!frameInteger(b,0,100,level))return false;
 enabled=on;brightness=(uint8_t)level;return true;
}

inline bool displayStandby(const DisplayFrame &f){return !strcmp(f.mode,"off")||!strcmp(f.mode,"stop")||!strcmp(f.mode,"pause");}
