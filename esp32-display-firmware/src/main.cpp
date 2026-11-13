#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <driver/gpio.h>
#include <time.h>
#include <TJpg_Decoder.h>
#include <mbedtls/base64.h>
#include "DisplayConfig.h"
#include "DisplayProtocol.h"
#include "DisplayScroll.h"
#if defined(PLAYER_DISPLAY_ST7789)
#include <Adafruit_ST7789.h>
Adafruit_ST7789 panel(&SPI,display_config::CS,display_config::DC,display_config::RESET);
#else
#include <Adafruit_ILI9341.h>
Adafruit_ILI9341 panel(&SPI,display_config::CS,display_config::DC,display_config::RESET);
#endif
constexpr uint32_t SCROLL_INTERVAL_MS=60,SCROLL_PAUSE_MS=1800,STALE_AFTER_MS=6000;
constexpr uint16_t BACKGROUND=0x0841,FOREGROUND=0xffff,MUTED=0x9d14,ACCENT=0xbfb2;
DisplayFrame frame;
uint32_t receivedAt=0,titleChangedAt=0,artistChangedAt=0,lastPaint=0;
bool haveFrame=false,discardLine=false;
char inputLine[24576];size_t inputLength=0;
String titleText,artistText;
GFXcanvas16 *titleCanvas,*artistCanvas,*trackCanvas;
int titleY,artistY,coverX,coverY;
constexpr int COVER_SIZE=80;
constexpr size_t MAX_COVER_BYTES=16384;
uint8_t coverData[MAX_COVER_BYTES];
#include "FlipClock.h"
uint32_t coverCRC(const uint8_t *data,size_t size) {
 uint32_t crc=0xffffffff;
 for(size_t i=0;i<size;++i){crc^=data[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320U&-(int32_t)(crc&1));}
 return crc^0xffffffff;
}
constexpr uint8_t BACKLIGHT_CHANNEL=0;
bool backlightEnabled=true;uint8_t backlightBrightness=100;
void applyBacklight(){uint32_t duty=backlightEnabled?(uint32_t)backlightBrightness*display_config::BACKLIGHT_MAX_DUTY/100:0;if(!display_config::BACKLIGHT_ON)duty=255-duty;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
ledcWrite(display_config::BACKLIGHT,duty);
#else
ledcWrite(BACKLIGHT_CHANNEL,duty);
#endif
}
void acceptDisplayControl(char *line){bool enabled;uint8_t brightness;if(!parseDisplayControl(line,enabled,brightness))return;backlightEnabled=enabled;backlightBrightness=brightness;applyBacklight();Serial.printf("P5 DISPLAY %d %u\n",enabled?1:0,(unsigned)brightness);}
void emptyCover() {
 panel.fillRect(coverX,coverY,COVER_SIZE,COVER_SIZE,0x10a2);
 panel.drawRect(coverX-1,coverY-1,COVER_SIZE+2,COVER_SIZE+2,0x31c7);
 panel.fillCircle(coverX+30,coverY+52,6,MUTED);
 panel.fillRect(coverX+35,coverY+22,3,30,MUTED);
 panel.fillRect(coverX+38,coverY+22,16,4,MUTED);
}
bool coverBlock(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t *pixels) {
 if(x<coverX||y<coverY||x+w>coverX+COVER_SIZE||y+h>coverY+COVER_SIZE)return false;
 panel.drawRGBBitmap(x,y,pixels,w,h);return true;
}
void acceptCover(char *line) {
 char *crcText=line+4,*image=strchr(crcText,'\t');if(!image)return;*image++=0;
 if(strlen(crcText)!=8)return;char *end;uint32_t crc=strtoul(crcText,&end,16);if(*end)return;
 if(!strcmp(image,"-")){if(!flipActive)emptyCover();Serial.println("P5 COVER 00000000");return;}
 size_t bytes=0;
 if(mbedtls_base64_decode(coverData,sizeof coverData,&bytes,(const uint8_t*)image,strlen(image))||bytes<4||coverCRC(coverData,bytes)!=crc){Serial.println("P5 COVER_ERROR CRC");return;}
 if(flipActive){Serial.printf("P5 COVER %08lX\n",(unsigned long)crc);return;}
 uint16_t w=0,h=0;
 if(TJpgDec.getJpgSize(&w,&h,coverData,bytes)!=JDR_OK||w>COVER_SIZE||h>COVER_SIZE||!w||!h){Serial.println("P5 COVER_ERROR SIZE");return;}
 panel.fillRect(coverX,coverY,COVER_SIZE,COVER_SIZE,BACKGROUND);
 if(TJpgDec.drawJpg(coverX+(COVER_SIZE-w)/2,coverY+(COVER_SIZE-h)/2,coverData,bytes)!=JDR_OK){emptyCover();Serial.println("P5 COVER_ERROR JPEG");return;}
 Serial.printf("P5 COVER %08lX\n",(unsigned long)crc);
}
// Adafruit's classic font is CP437. Decode UTF-8 and map common European letters.
String displayText(const char *s) {
 String result;const unsigned char *p=(const unsigned char*)s;
 while(*p) {
  uint32_t c=*p++;
  if(c>=0xc0) {
   unsigned count=(c<0xe0?1:c<0xf0?2:3);c&=(count==1?31:count==2?15:7);
   while(count--&&*p) { if((*p&0xc0)!=0x80){c='?';break;}c=(c<<6)|(*p++&63); }
  }
  if(c>=32&&c<127){result+=(char)c;continue;}
  struct Map {uint16_t u;uint8_t g;};
  static const Map map[]={{0xe4,0x84},{0xf6,0x94},{0xfc,0x81},{0xc4,0x8e},{0xd6,0x99},{0xdc,0x9a},{0xdf,0xe1},{0xe9,0x82},{0xe8,0x8a},{0xea,0x88},{0xeb,0x89},{0xe0,0x85},{0xe1,0xa0},{0xe2,0x83},{0xe5,0x86},{0xc5,0x8f},{0xe6,0x91},{0xc6,0x92},{0xe7,0x87},{0xc7,0x80},{0xf1,0xa4},{0xd1,0xa5},{0xf3,0xa2},{0xf4,0x93},{0xf2,0x95},{0xfa,0xa3},{0xf9,0x97},{0xfb,0x96},{0xed,0xa1},{0xee,0x8c},{0xef,0x8b},{0xc9,0x90},{0x2013,'-'},{0x2014,'-'},{0x2019,'\''},{0x2018,'\''},{0x201c,'"'},{0x201d,'"'},{0xa0,' '}};
  bool found=false;for(const auto &x:map)if(c==x.u){result+=(char)x.g;found=true;break;}
  if(!found)result+='?';
 }
 return result;
}
void header() {
 static int64_t lastMinute=-1;static bool lastStale=true;
 bool stale=!haveFrame||(uint32_t)(millis()-receivedAt)>STALE_AFTER_MS;
 int64_t now=haveFrame&&frame.validTime?frame.epoch+(uint32_t)(millis()-receivedAt)/1000+frame.offset:0;
 if(!playerLayoutDirty&&lastMinute==now/60&&stale==lastStale)return;
 lastMinute=now/60;lastStale=stale;
 panel.fillRect(12,8,panel.width()-24,40,BACKGROUND);
 char clock[12]="--:--",date[20]="--.--.----";
 if(now>1700000000LL){time_t seconds=now;tm local;gmtime_r(&seconds,&local);strftime(clock,sizeof clock,"%H:%M",&local);strftime(date,sizeof date,"%d.%m.%Y",&local);}
 panel.setTextWrap(false);panel.setTextColor(stale?MUTED:FOREGROUND);panel.setTextSize(4);panel.setCursor(12,10);panel.print(clock);
 panel.setTextColor(MUTED);panel.setTextSize(2);panel.setCursor(panel.width()-132,18);panel.print(date);

}
void line(GFXcanvas16 &canvas,const String &text,int size,int y,uint16_t color,uint32_t since,bool once=false) {
 canvas.fillScreen(BACKGROUND);canvas.setTextSize(size);canvas.setTextWrap(false);canvas.setTextColor(color);canvas.cp437(true);
 int width=text.length()*6*size,offset=0;uint32_t elapsed=millis()-since;
 offset=displayScrollOffset(width,canvas.width(),elapsed,once,SCROLL_PAUSE_MS,SCROLL_INTERVAL_MS);
 canvas.setCursor(-offset,2);canvas.print(text);
 if(width>canvas.width()){canvas.setCursor(width+36-offset,2);canvas.print(text);}
 panel.drawRGBBitmap(12,y,canvas.getBuffer(),canvas.width(),canvas.height());
}
void trackStatus() {
 static String lastTrack;static int lastStale=-1,lastOffset=-1;static uint32_t changedAt=0;
 bool stale=!haveFrame||(uint32_t)(millis()-receivedAt)>STALE_AFTER_MS;
 bool radio=!strcmp(frame.mode,"radio"),airplay=!strcmp(frame.mode,"airplay");bool once=radio||airplay;String text=radio?"SENDERINFO":airplay?"ALBUM":"TRACK";
 if(frame.track[0]){text+=" ";text+=displayText(frame.track);}
 bool changed=text!=lastTrack||playerLayoutDirty,indicator=changed||lastStale!=(int)stale;
 if(changed){lastTrack=text;changedAt=millis();lastOffset=-1;}
 int offset=displayScrollOffset(text.length()*6,trackCanvas->width(),millis()-changedAt,once,SCROLL_PAUSE_MS,SCROLL_INTERVAL_MS);
 if(offset==lastOffset&&!indicator)return;
 lastOffset=offset;lastStale=stale;
 // Transfer the finished canvas directly; never blank the visible text first.
 line(*trackCanvas,text,1,65,ACCENT,changedAt,once);
 if(indicator){panel.fillRect(12+trackCanvas->width(),65,coverX-24-trackCanvas->width(),13,BACKGROUND);int dotX=min(12+(int)text.length()*6+8,coverX-17);panel.fillCircle(dotX,69,3,stale?0xc986:ACCENT);}
}
void acceptFrame(const DisplayFrame &next) {
 frame=next;receivedAt=millis();haveFrame=true;
 String title=displayText(frame.title),artist=displayText(frame.artist);
 if(title.isEmpty())title=!strcmp(frame.mode,"airplay")?"AirPlay":!strcmp(frame.mode,"unavailable")?"LMS nicht erreichbar":"Keine Wiedergabe";
 if(title!=titleText){titleText=title;titleChangedAt=millis();}
 if(artist!=artistText){artistText=artist;artistChangedAt=millis();}
 bool standby=displayStandby(frame);
 if(standby!=flipActive){flipActive=standby;panel.fillScreen(BACKGROUND);if(standby){flipLayoutDirty=true;}else{playerLayoutDirty=true;titleChangedAt=artistChangedAt=millis();panel.drawFastHLine(12,55,panel.width()-24,0x31c7);emptyCover();}}
 if(frame.sequence%10==0)Serial.printf("P5 ACK %lu\n",(unsigned long)frame.sequence);
}
void setup() {
 
#if defined(PLAYER_DISPLAY_WAVESHARE_C6_147)
 Serial.setRxBufferSize(32768);
#else
 Serial.setRxBufferSize(4096);
#endif
Serial.begin(115200);pinMode(display_config::BACKLIGHT,OUTPUT);digitalWrite(display_config::BACKLIGHT,!display_config::BACKLIGHT_ON);
 #if defined(PLAYER_DISPLAY_WAVESHARE_C6_147)
 pinMode(4,OUTPUT);digitalWrite(4,HIGH); // Keep the shared-SPI microSD unselected.
#endif
 pinMode(display_config::CS,OUTPUT);digitalWrite(display_config::CS,HIGH);SPI.begin(display_config::SCLK,display_config::MISO,display_config::MOSI);
#if defined(PLAYER_DISPLAY_ST7789)
 panel.init(display_config::ST7789_WIDTH,display_config::ST7789_HEIGHT,display_config::SPI_MODE);panel.invertDisplay(display_config::INVERT);
#else
 panel.begin(display_config::SPI_HZ);
#endif
 panel.setSPISpeed(display_config::SPI_HZ);panel.setRotation(display_config::ROTATION);panel.cp437(true);panel.fillScreen(BACKGROUND);
 #if ESP_ARDUINO_VERSION_MAJOR >= 3
 ledcAttach(display_config::BACKLIGHT,20000,8);
#else
 ledcSetup(BACKLIGHT_CHANNEL,20000,8);ledcAttachPin(display_config::BACKLIGHT,BACKLIGHT_CHANNEL);
#endif
 applyBacklight();
 titleY=panel.height()<200?86:108;artistY=panel.height()<200?128:164;
 coverX=panel.width()-12-COVER_SIZE;coverY=panel.height()-12-COVER_SIZE;
 flipOld=new GFXcanvas16(FLIP_W,FLIP_H);flipNew=new GFXcanvas16(FLIP_W,FLIP_H);
 trackCanvas=new GFXcanvas16(coverX-40,13);
 titleCanvas=new GFXcanvas16(panel.width()-24-COVER_SIZE-12,30);artistCanvas=new GFXcanvas16(panel.width()-24-COVER_SIZE-12,22);
 TJpgDec.setJpgScale(1);TJpgDec.setSwapBytes(false);TJpgDec.setCallback(coverBlock);emptyCover();
 if(!trackCanvas->getBuffer()||!titleCanvas->getBuffer()||!artistCanvas->getBuffer()){Serial.println("P5 ERROR canvas allocation");while(true)delay(1000);}
 panel.drawFastHLine(12,55,panel.width()-24,0x31c7);titleText="USB verbinden";titleChangedAt=millis();artistChangedAt=millis();
 Serial.printf("P5 READY 5.3 %dx%d %s\n",panel.width(),panel.height(),display_config::MODEL);
}
void loop() {
 while(Serial.available()) {
  char c=Serial.read();
  if(c=='\n') { if(!discardLine){inputLine[inputLength]=0;if(!strncmp(inputLine,"P5C\t",4))acceptCover(inputLine);else if(!strncmp(inputLine,"P5D\t",4))acceptDisplayControl(inputLine);else{DisplayFrame next;if(parseDisplayFrame(inputLine,next))acceptFrame(next);}}inputLength=0;discardLine=false; }
  else if(c!='\r') { if(inputLength+1<sizeof inputLine&&!discardLine)inputLine[inputLength++]=c;else discardLine=true; }
 }
 if(flipActive){flipClock();delay(2);return;}
 header();trackStatus();
 if(playerLayoutDirty||(uint32_t)(millis()-lastPaint)>=SCROLL_INTERVAL_MS){lastPaint=millis();line(*titleCanvas,titleText,3,titleY,FOREGROUND,titleChangedAt);line(*artistCanvas,artistText,2,artistY,MUTED,artistChangedAt);playerLayoutDirty=false;}
 delay(2);
}
