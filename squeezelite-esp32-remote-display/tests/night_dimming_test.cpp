#include <cassert>
#include <iostream>
#include "night_dimming.h"
int main(){
 using namespace NightDimming;
 for(int m=0;m<1440;m++){
  assert(active(true,true,1320,420,m)==(m>=1320||m<420));
  assert(active(true,true,600,720,m)==(m>=600&&m<720));
  assert(!active(false,true,1320,420,m));assert(!active(true,false,1320,420,m));
  assert(!active(true,true,420,420,m));
 }
 assert(level(71,10,true)==10);assert(level(71,10,false)==71);assert(level(5,10,true)==5);
 assert(level(100,0,true)==5);assert(!active(true,true,-1,420,0));
 assert(parseTime("2200")==1320);assert(parseTime("0700")==420);assert(parseTime("0000")==0);assert(parseTime("2359")==1439);
 for(const char* s:{"2400","2360","-100","123","12345","", "abcd"})assert(parseTime(s)==-1);
 assert(adjust(23*60+59,true,1)==59);assert(adjust(0,true,-1)==23*60);
 assert(adjust(22*60+59,false,1)==22*60);assert(adjust(7*60,false,-1)==7*60+59);
 for(int value=0;value<1440;value++)for(bool hour:{false,true}){assert(adjust(adjust(value,hour,1),hour,-1)==value);assert(adjust(adjust(value,hour,-1),hour,1)==value);}
 std::cout<<"Full-day night/day schedules, boundaries, disabled/unsynced clocks and time validation passed\n";
}
