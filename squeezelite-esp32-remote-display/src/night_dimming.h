#pragma once
#include <algorithm>
#include <cstring>
namespace NightDimming {
inline bool active(bool enabled,bool clockReady,int start,int end,int minute){
 if(!enabled||!clockReady||start<0||start>=1440||end<0||end>=1440||minute<0||minute>=1440||start==end)return false;
 return start<end?(minute>=start&&minute<end):(minute>=start||minute<end);
}
inline int level(int day,int night,bool dimmed){day=std::max(5,std::min(100,day));return dimmed?std::min(day,std::max(5,std::min(100,night))):day;}
inline int adjust(int value,bool hours,int direction){int h=value/60,m=value%60;if(hours)h=(h+(direction>0?1:23))%24;else m=(m+(direction>0?1:59))%60;return h*60+m;}
inline int parseTime(const char* value){if(!value||std::strlen(value)!=4)return -1;int h=0,m=0;for(int n=0;n<4;n++){if(value[n]<'0'||value[n]>'9')return -1;if(n<2)h=h*10+value[n]-'0';else m=m*10+value[n]-'0';}return value[4]||h>23||m>59?-1:h*60+m;}
}
