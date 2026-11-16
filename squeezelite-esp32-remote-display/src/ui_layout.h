#pragma once
#include <string>
#include <algorithm>
namespace UiLayout {
inline int panelX(int x){return 12+x*296/320;}
inline int panelY(int y){return 12+y*216/240;}
inline bool insideMenuPanel(int x,int y){return x>=12&&x<308&&y>=12&&y<228;}
inline int menuX(int x){return std::max(0,std::min(319,(x-12)*320/296));}
inline int menuY(int y){return std::max(0,std::min(239,(y-12)*240/216));}
inline bool insideOverlay(int x,int y){return x>=20&&x<300&&y>=32&&y<224;}
inline int volumeAt(int y){return std::max(0,std::min(100,((198-y)*100+58)/116));}
inline int volumeY(int value){return 198-(std::max(0,std::min(100,value))*116+50)/100;}
struct LabelLines {std::string first,second;bool wrapped;LabelLines(const std::string &a="",const std::string &b="",bool wrap=false):first(a),second(b),wrapped(wrap){}};
template<class Measure> LabelLines splitLabel(const std::string &label,int width,Measure measure){
 LabelLines result{label,"",false};if(measure(label)<=width)return result;
 int bestWidth=0x7fffffff;
 for(size_t n=0;n<label.size();n++)if(label[n]==' '&&n>0&&n+1<label.size()){
  auto first=label.substr(0,n),second=label.substr(n+1);int widest=std::max(measure(first),measure(second));
  if(widest<=width&&widest<bestWidth){bestWidth=widest;result={first,second,true};}
 }
 return result;
}
}
