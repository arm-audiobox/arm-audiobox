#pragma once
#include <cstdint>
#include <cstdlib>
struct TouchGesture {
 enum Event {None,Tap,Hold};
 bool active=false,longSent=false,moved=false,holdAllowed=false;
 uint32_t started=0,lastSeen=0;int x=0,y=0;
 void reset(){active=false;longSent=false;moved=false;}
 Event update(bool down,int px,int py,uint32_t now,bool canHold){
  if(down){
   if(!active){active=true;longSent=false;moved=false;holdAllowed=canHold;started=now;x=px;y=py;}
   else if(std::abs(px-x)>12||std::abs(py-y)>12)moved=true;
   lastSeen=now;
   if(holdAllowed&&!moved&&!longSent&&uint32_t(now-started)>=650){longSent=true;return Hold;}
  }else if(active&&uint32_t(now-lastSeen)>=50){active=false;if(!moved&&!longSent)return Tap;}
  return None;
 }
};
