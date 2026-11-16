#include <cassert>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "clock_model.h"
int main(){
 auto epoch=ClockModel::httpDate("Sun, 15 Nov 2026 14:14:17 GMT");assert(epoch>0);time_t t=epoch;tm utc;gmtime_r(&t,&utc);assert(utc.tm_year==126&&utc.tm_mon==10&&utc.tm_mday==15&&utc.tm_hour==14&&utc.tm_min==14&&utc.tm_sec==17);
 assert(ClockModel::httpDate("Sun, 31 Feb 2026 14:14:17 GMT")==0);assert(ClockModel::httpDate("Thu, 29 Feb 2024 01:02:03 GMT")>0);assert(ClockModel::httpDate("Sat, 29 Feb 2025 01:02:03 GMT")==0);assert(ClockModel::httpDate("invalid")==0);
 setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();tm local;localtime_r(&t,&local);assert(local.tm_hour==15&&local.tm_isdst==0);
 t=ClockModel::httpDate("Sun, 29 Mar 2026 01:30:00 GMT");localtime_r(&t,&local);assert(local.tm_hour==3&&local.tm_isdst==1);
 ClockModel::Idle idle;idle.observe(true,100);assert(!idle.due(10099));idle.observe(true,9000);assert(idle.due(10100));idle.activity(10000);assert(!idle.due(19999));assert(idle.due(20000));idle.observe(false,21000);assert(!idle.due(50000));idle.observe(true,50000);assert(!idle.due(59999)&&idle.due(60000));idle.activity(0xfffffff0u);assert(idle.due(0x2700u));
 std::cout<<"14 clock, timezone and pause/standby timing checks passed\n";
}
