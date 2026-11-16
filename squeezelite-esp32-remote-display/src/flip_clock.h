#pragma once
#include <LovyanGFX.hpp>
#include <ctime>
#include <cstring>
// Same card dimensions, colors, digit style and 300 ms animation as the
// Pogoplug ESP32 display; adapted to LovyanGFX and the local system clock.
class FlipClock {
 static constexpr int W=64,H=96,HALF=48;
 lgfx::LGFX_Device &lcd;
 lgfx::LGFX_Sprite oldCard,newCard;
 uint16_t pixels[W*HALF];
 bool allocated=false;int64_t lastMinute=-1;
 char shown[5]="----",target[5]="----";
 uint8_t mask=0;uint32_t started=0,lastDraw=0;
 static constexpr uint16_t BG=0x0841,INK=0xffff,MUTED=0x9d14;
 template<class Surface> void card(Surface &surface,int x,int y,char digit){
  surface.fillRect(x,y,W,H,BG);surface.fillRoundRect(x,y,W,H,8,0x2945);surface.fillRoundRect(x,y+HALF,W,HALF,8,0x2104);
  surface.setFont(&fonts::Font0);surface.setTextDatum(lgfx::textdatum_t::top_left);surface.setTextColor(INK);surface.setTextSize(7);surface.setCursor(x+11,y+20);surface.print(digit);
  surface.drawFastHLine(x,y+HALF,W,BG);surface.fillRect(x+1,y+HALF-1,3,3,0x4a69);surface.fillRect(x+W-4,y+HALF-1,3,3,0x4a69);
 }
 void image(int x,int y,int height,const uint16_t* data){lcd.pushImage(x,y,W,height,reinterpret_cast<const lgfx::swap565_t*>(data));}
 public:
 explicit FlipClock(lgfx::LGFX_Device &display):lcd(display),oldCard(&display),newCard(&display){}
 void draw(uint32_t now,time_t epoch,bool force=false){
  if(!allocated){oldCard.setColorDepth(16);newCard.setColorDepth(16);allocated=oldCard.createSprite(W,H)&&newCard.createSprite(W,H);}
  int top=(lcd.height()-120)/2;const int xs[4]={12,80,176,244};
  int64_t minute=epoch>1700000000?int64_t(epoch)/60:-1;
  if(force||minute!=lastMinute){
   char wanted[5]="----",date[20]="--.--.----";if(epoch>1700000000){tm local;localtime_r(&epoch,&local);strftime(wanted,sizeof wanted,"%H%M",&local);strftime(date,sizeof date,"%d.%m.%Y",&local);}
   bool full=force||lastMinute<0||mask||!allocated;mask=0;memcpy(target,wanted,sizeof target);
   if(full){memcpy(shown,target,sizeof shown);for(int n=0;n<4;n++){if(allocated){card(newCard,0,0,target[n]);image(xs[n],top,H,(const uint16_t*)newCard.getBuffer());}else card(lcd,xs[n],top,target[n]);}}
   else{for(int n=0;n<4;n++)if(shown[n]!=target[n])mask|=1U<<n;started=now;lastDraw=0;}
   lcd.fillCircle(160,top+33,3,INK);lcd.fillCircle(160,top+63,3,INK);lcd.fillRect(12,lcd.height()-30,lcd.width()-24,24,BG);
   lcd.setFont(&fonts::efontJA_16);lcd.setTextSize(1);lcd.setTextColor(MUTED,BG);lcd.setTextDatum(lgfx::textdatum_t::top_center);lcd.drawString(date,lcd.width()/2,lcd.height()-28);lcd.setTextDatum(lgfx::textdatum_t::top_left);lastMinute=minute;
  }
  if(!mask||uint32_t(now-lastDraw)<25||!allocated)return;lastDraw=now;uint32_t elapsed=now-started;
  for(int n=0;n<4;n++)if(mask&(1U<<n)){
   card(newCard,0,0,target[n]);const uint16_t* fresh=(const uint16_t*)newCard.getBuffer();
   if(elapsed>=300){image(xs[n],top,H,fresh);continue;}
   card(oldCard,0,0,shown[n]);const uint16_t* old=(const uint16_t*)oldCard.getBuffer();image(xs[n],top,HALF,fresh);image(xs[n],top+HALF,HALF,old+W*HALF);
   int height=elapsed<150?HALF-int(elapsed*HALF/150):int((elapsed-150)*HALF/150);const uint16_t* source=elapsed<150?old:fresh;int base=elapsed<150?0:HALF;
   if(height>0){for(int row=0;row<height;row++)memcpy(pixels+row*W,source+(base+row*HALF/height)*W,W*sizeof(uint16_t));image(xs[n],elapsed<150?top+HALF-height:top+HALF,height,pixels);}
   lcd.drawFastHLine(xs[n],top+HALF,W,BG);
  }
  if(elapsed>=300){memcpy(shown,target,sizeof shown);mask=0;}
 }
};
