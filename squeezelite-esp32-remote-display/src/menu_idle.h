#pragma once
#include <cstdint>
struct MenuIdle {
 uint32_t lastActivity=0;
 uint16_t seconds=10;
 void activity(uint32_t now){lastActivity=now;}
 bool expired(uint32_t now,bool inMenu)const{return inMenu&&seconds>0&&uint32_t(now-lastActivity)>=uint32_t(seconds)*1000;}
};
