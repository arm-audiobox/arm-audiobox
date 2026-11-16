#include <cassert>
#include <iostream>
#include "menu_idle.h"
int main(){MenuIdle t;t.activity(1000);assert(!t.expired(10999,true));assert(t.expired(11000,true));assert(!t.expired(11000,false));t.activity(9000);assert(!t.expired(18000,true));assert(t.expired(19000,true));t.seconds=0;assert(!t.expired(999999,true));t.seconds=3;t.activity(100);assert(!t.expired(3099,true));assert(t.expired(3100,true));t.activity(0xfffffff0u);assert(!t.expired(0x00000b00u,true));assert(t.expired(0x00000ba8u,true));std::cout<<"10 menu inactivity checks passed\n";}
