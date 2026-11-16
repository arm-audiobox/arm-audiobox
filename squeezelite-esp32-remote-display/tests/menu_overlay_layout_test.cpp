#include <cassert>
#include <iostream>
#include "ui_layout.h"
int main(){
 assert(UiLayout::insideMenuPanel(12,12));assert(UiLayout::insideMenuPanel(307,227));
 assert(!UiLayout::insideMenuPanel(11,120));assert(!UiLayout::insideMenuPanel(308,120));
 assert(!UiLayout::insideMenuPanel(160,11));assert(!UiLayout::insideMenuPanel(160,228));
 assert(UiLayout::menuX(12)==0);assert(UiLayout::menuY(12)==0);
 for(int x=12;x<308;x++)assert(UiLayout::menuX(x)>=0&&UiLayout::menuX(x)<320);
 for(int y=12;y<228;y++)assert(UiLayout::menuY(y)>=0&&UiLayout::menuY(y)<240);
 for(int row=0;row<4;row++){int displayY=12+(50+row*36)*216/240;assert((UiLayout::menuY(displayY)-32)/36==row);}
 assert(UiLayout::menuX(160)==160);assert(UiLayout::menuY(120)==120);
 std::cout<<"Menu overlay coordinates, dismissal bounds and all list rows passed\n";
}
