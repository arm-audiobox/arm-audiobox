#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace ClockModel {
inline bool leap(int year){return year%4==0&&(year%100!=0||year%400==0);}
inline int64_t httpDate(const char* value){
 char weekday[4]={},month[4]={},zone[4]={};int day,year,hour,minute,second;
 if(std::sscanf(value,"%3s, %d %3s %d %d:%d:%d %3s",weekday,&day,month,&year,&hour,&minute,&second,zone)!=8||std::strcmp(zone,"GMT")!=0)return 0;
 static const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};int m=0;for(;m<12;m++)if(std::strcmp(month,months[m])==0)break;
 if(m==12||year<2020||year>2099||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return 0;
 static const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};int limit=lengths[m]+(m==1&&leap(year)?1:0);if(day<1||day>limit)return 0;
 int64_t days=0;for(int y=1970;y<year;y++)days+=leap(y)?366:365;for(int n=0;n<m;n++)days+=lengths[n]+(n==1&&leap(year)?1:0);days+=day-1;
 return ((days*24+hour)*60+minute)*60+second;
}
struct Idle {
 bool idle=false;uint32_t lastActivity=0;
 void observe(bool value,uint32_t now){if(value!=idle){idle=value;lastActivity=now;}}
 void activity(uint32_t now){lastActivity=now;}
 bool due(uint32_t now)const{return idle&&uint32_t(now-lastActivity)>=10000;}
};
}
