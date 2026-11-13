#pragma once
#include <stdint.h>
inline int displayScrollOffset(int width,int visible,uint32_t elapsed,bool once,uint32_t pause=1800,uint32_t interval=60){
 if(width<=visible||elapsed<=pause)return 0;
 uint32_t step=(elapsed-pause)/interval,cycle=(uint32_t)width+36;
 return once&&step>=cycle?0:(int)(step%cycle);
}
