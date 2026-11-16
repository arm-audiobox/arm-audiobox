#include <cassert>
#include <iostream>
#include "ui_layout.h"
int main(){
 assert(UiLayout::volumeAt(198)==0);assert(UiLayout::volumeAt(82)==100);assert(UiLayout::volumeAt(140)==50);
 assert(UiLayout::volumeAt(0)==100);assert(UiLayout::volumeAt(240)==0);
 for(int v=0;v<=100;v++){assert(UiLayout::volumeAt(UiLayout::volumeY(v))==v);if(v)assert(UiLayout::volumeY(v)<=UiLayout::volumeY(v-1));}
 assert(UiLayout::insideOverlay(20,32));assert(UiLayout::insideOverlay(299,223));
 assert(!UiLayout::insideOverlay(19,100));assert(!UiLayout::insideOverlay(300,100));assert(!UiLayout::insideOverlay(100,31));assert(!UiLayout::insideOverlay(100,224));
 std::cout<<"Overlay bounds and all 101 volume positions passed\n";
}
