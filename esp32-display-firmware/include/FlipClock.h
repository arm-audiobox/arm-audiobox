#pragma once
// Included after the panel and shared display state have been declared.
constexpr int FLIP_W=64,FLIP_H=96,FLIP_HALF=48;
GFXcanvas16 *flipOld=nullptr,*flipNew=nullptr;
uint16_t flipPixels[FLIP_W*FLIP_HALF];
bool flipActive=false,flipLayoutDirty=true,playerLayoutDirty=true;
template<class Surface> void flipCard(Surface &surface,int x,int y,char digit,uint16_t ink){
 surface.fillRect(x,y,FLIP_W,FLIP_H,BACKGROUND);
 surface.fillRoundRect(x,y,FLIP_W,FLIP_H,8,0x2945);
 surface.fillRoundRect(x,y+FLIP_HALF,FLIP_W,FLIP_HALF,8,0x2104);
 surface.setTextWrap(false);surface.setTextColor(ink);surface.setTextSize(7);surface.setCursor(x+11,y+20);surface.print(digit);
 surface.drawFastHLine(x,y+FLIP_HALF,FLIP_W,BACKGROUND);
 surface.fillRect(x+1,y+FLIP_HALF-1,3,3,0x4a69);surface.fillRect(x+FLIP_W-4,y+FLIP_HALF-1,3,3,0x4a69);
}
void flipClock(){
 static int64_t lastMinute=-1;static int lastStale=-1;
 static char shown[5]="----",target[5]="----";static uint8_t mask=0;
 static uint32_t started=0,lastDraw=0;
 bool stale=!haveFrame||(uint32_t)(millis()-receivedAt)>STALE_AFTER_MS;
 int64_t now=haveFrame&&frame.validTime?frame.epoch+(uint32_t)(millis()-receivedAt)/1000+frame.offset:0;
 int top=(panel.height()-120)/2;const int xs[4]={12,80,176,244};uint16_t ink=stale?MUTED:FOREGROUND;
 bool canvas=flipOld&&flipNew&&flipOld->getBuffer()&&flipNew->getBuffer();
 if(flipLayoutDirty||lastMinute!=now/60||lastStale!=(int)stale){
  char wanted[5]="----",date[20]="--.--.----";
  if(now>1700000000LL){time_t seconds=now;tm local;gmtime_r(&seconds,&local);strftime(wanted,sizeof wanted,"%H%M",&local);strftime(date,sizeof date,"%d.%m.%Y",&local);}
  bool full=flipLayoutDirty||lastStale!=(int)stale||mask||!canvas;mask=0;memcpy(target,wanted,sizeof target);
  if(full){memcpy(shown,target,sizeof shown);for(int i=0;i<4;i++){if(canvas){flipCard(*flipNew,0,0,target[i],ink);panel.drawRGBBitmap(xs[i],top,flipNew->getBuffer(),FLIP_W,FLIP_H);}else flipCard(panel,xs[i],top,target[i],ink);}}
  else{for(int i=0;i<4;i++)if(shown[i]!=target[i])mask|=(1U<<i);started=millis();lastDraw=0;}
  panel.fillCircle(160,top+33,3,ink);panel.fillCircle(160,top+63,3,ink);
  panel.fillRect(12,panel.height()-29,panel.width()-24,20,BACKGROUND);panel.setTextSize(2);panel.setTextColor(MUTED);panel.setCursor((panel.width()-(int)strlen(date)*12)/2,panel.height()-28);panel.print(date);
  lastMinute=now/60;lastStale=stale;flipLayoutDirty=false;
 }
 if(!mask||(uint32_t)(millis()-lastDraw)<25)return;
 lastDraw=millis();uint32_t elapsed=millis()-started;
 for(int i=0;i<4;i++)if(mask&(1U<<i)){
  flipCard(*flipNew,0,0,target[i],ink);
  if(elapsed>=300){panel.drawRGBBitmap(xs[i],top,flipNew->getBuffer(),FLIP_W,FLIP_H);continue;}
  flipCard(*flipOld,0,0,shown[i],ink);
  panel.drawRGBBitmap(xs[i],top,flipNew->getBuffer(),FLIP_W,FLIP_HALF);
  panel.drawRGBBitmap(xs[i],top+FLIP_HALF,flipOld->getBuffer()+FLIP_W*FLIP_HALF,FLIP_W,FLIP_HALF);
  int height=elapsed<150?FLIP_HALF-(int)(elapsed*FLIP_HALF/150):(int)((elapsed-150)*FLIP_HALF/150);
  const uint16_t *pixels=(elapsed<150?flipOld:flipNew)->getBuffer();int base=elapsed<150?0:FLIP_HALF;
  if(height>0){for(int row=0;row<height;row++)memcpy(flipPixels+row*FLIP_W,pixels+(base+row*FLIP_HALF/height)*FLIP_W,FLIP_W*sizeof(uint16_t));panel.drawRGBBitmap(xs[i],elapsed<150?top+FLIP_HALF-height:top+FLIP_HALF,flipPixels,FLIP_W,height);}
  panel.drawFastHLine(xs[i],top+FLIP_HALF,FLIP_W,BACKGROUND);
 }
 if(elapsed>=300){memcpy(shown,target,sizeof shown);mask=0;}
}
