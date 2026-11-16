#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <esp_netif.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "display.h"
#include "menu_model.h"
#include "playback_model.h"
#include "touch_gesture.h"
#include "menu_idle.h"
#include "clock_model.h"
#include "night_dimming.h"
#include "flip_clock.h"
#include <sys/time.h>
#include <time.h>
#include "ui_layout.h"
#include <i18n_generated.h>
#include <vector>
#include <algorithm>

String uiLanguage="de";
String tr(const String &source){const char* out=I18n::lookup(uiLanguage.c_str(),source.c_str());return out?String(out):source;}
CydDisplay lcd;
FlipClock flipClock(lcd);
LGFX_Sprite fieldBuffer(&lcd);
LGFX_Sprite menuCanvas(&lcd);
lgfx::LGFXBase* drawing=&lcd;
bool menuCanvasReady=false;
int drawingOffset=0;
uint16_t menuInk(uint16_t c);
bool invalidateDisplay=true;int renderedScreen=-1;
uint32_t fullRedraws=0,fieldRedraws=0,coverRedraws=0;
WebServer web(80);
DNSServer dns;
Preferences prefs;
String host="lms.lan", player, playerName, title, artist, album, mode, artKey;
String errorText,serverIP;
bool portal=false, portalAutomatic=false, dirty=true, restartPending=false;
uint32_t restartAt=0, lastPoll=0, lastReconnect=0;
int displayBrightness=71,savedBrightness=71;
bool nightEnabled=true,nightActive=false,nightEditingEnd=false;
int nightStart=22*60,nightEnd=7*60,nightBrightness=10,effectiveBrightness=-1,nightDraft=0;
bool timeStepActive=false;int heldTimeStep=0;uint32_t timeStepStarted=0,timeStepLast=0,timeStepSeen=0;
int volume=0, track=0, playlistTracks=0, duration=0, elapsed=0;
bool isStream=false;String stationName;
constexpr uint16_t BG=0x0841, CARD=0x18E3, ACCENT=0x3E5F, MUTED=0x9CD3;
uint8_t cover[40000]; size_t coverLen=0;
bool calibratePending=false;uint32_t touchEvents=0;int lastTouchX=-1,lastTouchY=-1;
struct Item { String name,id,url,action,playAction,nextAction,addAction; bool folder=false,translateLabel=false; };
Item items[4]; int itemCount=0,total=0,offset=0;
enum Screen { NOW, HOME, PLAYERS, ALBUMS, ARTISTS, TRACKS, LISTS, FAVORITES, QUEUE, SETTINGS, BROWSE, CONTROLS, TEXTINPUT, LANGUAGE, CONTEXT, IDLESETUP, CLOCK, VOLUME_OVERLAY, WIFI_OVERLAY, PLAYER_OVERLAY, BRIGHTNESS_OVERLAY, NIGHTSETUP, NIGHTTIME, NIGHTLEVEL };
Screen screen=PLAYERS;
bool compactOverlay(){return screen==VOLUME_OVERLAY||screen==WIFI_OVERLAY||screen==PLAYER_OVERLAY||screen==BRIGHTNESS_OVERLAY||screen==NIGHTLEVEL;}
bool menuOverlay(){return screen!=NOW&&screen!=CLOCK&&!compactOverlay();}
bool overlayActive(){return screen!=NOW&&screen!=CLOCK;}
uint16_t menuInk(uint16_t c){return overlayActive()&&c==MUTED?0xCE79:c;}
struct OverlayPlayer {String name,id;bool connected;};
std::vector<OverlayPlayer> overlayPlayers;int overlayOffset=0;
bool sliderDragging=false;uint32_t sliderSeen=0,lastVolumeSend=0;int sentVolume=-1;
String artistId,albumId,favoriteId;
String pageAction,pageTitle,pageNode,menuOwner,pendingInput,inputValue;bool keyboardCaps=false;
JsonDocument rootMenu,nodeMenu;
struct Nav { Screen page; int offset; String aid,bid,fid,action,title,node; };
Nav nav[8]; int depth=0;
struct ContextChoice {String label,action;};ContextChoice contextChoices[3];int contextCount=0;
TouchGesture touchGesture;bool blockTouchUntilRelease=false;
MenuIdle menuIdle;String timeoutInput;
ClockModel::Idle clockIdle;bool playerPower=true,clockSynced=false;uint32_t clockSyncedAt=0,lastClockAttempt=0;
void noteActivity(){menuIdle.activity(millis());clockIdle.activity(millis());}
void syncClock(const String &value){if(clockSynced&&uint32_t(millis()-clockSyncedAt)<60000)return;int64_t epoch=ClockModel::httpDate(value.c_str());if(epoch<=0)return;timeval tv{};tv.tv_sec=epoch;settimeofday(&tv,nullptr);clockSynced=true;clockSyncedAt=millis();}
void wakeClock(){screen=NOW;depth=0;noteActivity();invalidateDisplay=true;dirty=true;touchGesture.reset();}


String positionLabel(){if(isStream)return stationName.isEmpty()?tr("Stream"):stationName;if(playlistTracks<=0)return tr("No playback");String label=tr("Track {current} of {total}");label.replace("{current}",String(track));label.replace("{total}",String(playlistTracks));return label;}
bool setMenuTimeout(unsigned seconds){if(seconds>3600)return false;menuIdle.seconds=seconds;prefs.putUInt("menuIdle",seconds);noteActivity();dirty=true;return true;}
void setLanguage(const String &code){if(I18n::languageIndex(code.c_str())<0)return;uiLanguage=code;prefs.putString("language",code);noteActivity();invalidateDisplay=true;dirty=true;}
String base(){return "http://"+host+":9000";}
void refreshClock(){if(WiFi.status()!=WL_CONNECTED)return;uint32_t interval=clockSynced?60000:5000;if(lastClockAttempt&&uint32_t(millis()-lastClockAttempt)<interval)return;lastClockAttempt=millis();HTTPClient h;h.setConnectTimeout(1500);h.setTimeout(1800);h.begin(base()+"/");const char* keys[]={"Date"};h.collectHeaders(keys,1);if(h.sendRequest("HEAD")==200)syncClock(h.header("Date"));h.end();}
String enc(const String &s){String r; const char*h="0123456789ABCDEF"; for(size_t i=0;i<s.length();i++){uint8_t c=s[i];if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')r+=char(c);else {r+='%';r+=h[c>>4];r+=h[c&15];}} return r;}
// Diagnose DNS and mDNS separately; .lan uses the DHCP-provided DNS server.
IPAddress directDNS(IPAddress resolver,const String &name){
 if(!uint32_t(resolver))return IPAddress();
 WiFiUDP udp;if(!udp.begin(0))return IPAddress();uint8_t q[256]={0x42,0x17,1,0,0,1,0,0,0,0,0,0};int n=12,start=0;
 for(unsigned i=0;i<=name.length();i++){if(i==name.length()||name[i]=='.'){int len=i-start;if(len<1||len>63||n+len+6>256){udp.stop();return IPAddress();}q[n++]=len;for(int j=start;j<int(i);j++)q[n++]=name[j];start=i+1;}}
 q[n++]=0;q[n++]=0;q[n++]=1;q[n++]=0;q[n++]=1;udp.beginPacket(resolver,53);udp.write(q,n);udp.endPacket();
 uint32_t until=millis()+1800;int size=0;while(!(size=udp.parsePacket())&&int32_t(millis()-until)<0)delay(5);
 uint8_t b[512];int len=size?udp.read(b,sizeof(b)):0;udp.stop();IPAddress result;
 if(len>=12&&b[0]==0x42&&b[1]==0x17){int answers=(b[6]<<8)|b[7];int pos=n;
  for(int i=0;i<answers&&pos<len;i++){if((b[pos]&0xc0)==0xc0)pos+=2;else{while(pos<len&&b[pos])pos+=b[pos]+1;pos++;}if(pos+10>len)break;
   int type=(b[pos]<<8)|b[pos+1],rdlen=(b[pos+8]<<8)|b[pos+9];pos+=10;if(pos+rdlen>len)break;
   if(type==1&&rdlen==4){result=IPAddress(b[pos],b[pos+1],b[pos+2],b[pos+3]);break;}pos+=rdlen;}
  Serial.printf("DNS UDP server=%s host=%s rcode=%d answers=%d A=%s\n",resolver.toString().c_str(),name.c_str(),b[3]&15,answers,result.toString().c_str());
 }else Serial.printf("DNS UDP server=%s host=%s timeout\n",resolver.toString().c_str(),name.c_str());return result;
}
void applyDNSOverride(const String &value){IPAddress ip;if(!ip.fromString(value))return;esp_netif_dns_info_t d{};d.ip.type=ESP_IPADDR_TYPE_V4;d.ip.u_addr.ip4.addr=uint32_t(ip);auto netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");int code=esp_netif_set_dns_info(netif,ESP_NETIF_DNS_MAIN,&d);Serial.printf("DNS override server=%s result=%d\n",value.c_str(),code);}
void dnsDiagnostics(){
 Serial.printf("DNS CONFIG IP=%s gateway=%s primary=%s secondary=%s\n",WiFi.localIP().toString().c_str(),WiFi.gatewayIP().toString().c_str(),WiFi.dnsIP(0).toString().c_str(),WiFi.dnsIP(1).toString().c_str());
 directDNS(WiFi.dnsIP(0),host);if(WiFi.dnsIP(1)!=WiFi.dnsIP(0))directDNS(WiFi.dnsIP(1),host);
 if(WiFi.gatewayIP()!=WiFi.dnsIP(0))directDNS(WiFi.gatewayIP(),host);
 directDNS(IPAddress(10,10,172,1),host);
 IPAddress ip;int ok=WiFi.hostByName(host.c_str(),ip);Serial.printf("DNS RESOLVE host=%s ok=%d A=%s\n",host.c_str(),ok,ip.toString().c_str());
 IPAddress md=MDNS.queryHost("lms",1500);Serial.printf("MDNS QUERY lms.local A=%s\n",md.toString().c_str());
}
bool rpcArgs(const String &pid, JsonArrayConst args, JsonDocument &out){
 if(WiFi.status()!=WL_CONNECTED){errorText="Wi-Fi disconnected";return false;}
 JsonDocument req; req["id"]=1; req["method"]="slim.request";
 auto p=req["params"].to<JsonArray>();p.add(pid);auto a=p.add<JsonArray>();for(JsonVariantConst v:args)a.add(v);
 String body;serializeJson(req,body);HTTPClient http;http.setConnectTimeout(1800);http.setTimeout(2500);
 http.begin(base()+"/jsonrpc.js");http.addHeader("Content-Type","application/json");
 const char* responseHeaders[]={"Date"};http.collectHeaders(responseHeaders,1);int code=http.POST(body);if(code==200)syncClock(http.header("Date"));bool ok=false;
 if(code==200){String data=http.getString(); auto e=deserializeJson(out,data);ok=!e&&!out["error"].is<JsonObject>()&&!out["error"].is<JsonArray>()&&!out["result"].isNull();}
 if(!ok)Serial.printf("LMS FAIL command=%s HTTP=%d heap=%u\n",args[0].as<const char*>(),code,ESP.getFreeHeap());
 http.end();errorText=ok?"":"LMS unavailable";return ok;
}
bool rpc(const String &pid,const std::initializer_list<String> &args,JsonDocument &out){JsonDocument d;auto a=d.to<JsonArray>();for(const auto &s:args)a.add(s);return rpcArgs(pid,a,out);}
void command(std::initializer_list<String> args){if(player.isEmpty())return;JsonDocument d;rpc(player,args,d);lastPoll=0;dirty=true;}
void text(String s,int x,int y,int width,uint16_t color=TFT_WHITE,int size=1){
 if(drawing==&menuCanvas){x=UiLayout::panelX(x);y=UiLayout::panelY(y);width=width*296/320;}
 drawing->setFont(&fonts::efontJA_16);drawing->setTextSize(size);drawing->setTextColor(menuInk(color),menuInk(BG));
 while(s.length()&&drawing->textWidth(s)>width){int n=s.length()-1;while(n>0&&((uint8_t)s[n]&0xC0)==0x80)n--;s.remove(n);}
 drawing->drawString(s,x,y-drawingOffset);
}
void button(int x,int y,int w,int h,const String &source){
 if(drawing==&menuCanvas){int right=UiLayout::panelX(x+w),bottom=UiLayout::panelY(y+h);x=UiLayout::panelX(x);y=UiLayout::panelY(y);w=right-x;h=bottom-y;}
 y-=drawingOffset;String label=tr(source);drawing->fillRoundRect(x,y,w,h,5,menuInk(CARD));drawing->setFont(&fonts::efontJA_16);drawing->setTextSize(1);drawing->setTextColor(menuInk(TFT_WHITE),menuInk(CARD));drawing->setTextDatum(middle_center);
 if(source=="Back"||source=="Zurück"){drawing->setTextDatum(middle_right);drawing->drawString(label,x+w-8,y+h/2);drawing->setTextDatum(top_left);return;}
 auto lines=UiLayout::splitLabel(std::string(label.c_str()),w-8,[](const std::string &s){return drawing->textWidth(s.c_str());});
 if(h>=32&&lines.wrapped){drawing->drawString(lines.first.c_str(),x+w/2,y+h/2-8);drawing->drawString(lines.second.c_str(),x+w/2,y+h/2+8);}
 else{if(overlayActive()){if(drawing->textWidth(label)>w-8){while(label.length()&&drawing->textWidth(label+"…")>w-8){int n=label.length()-1;while(n>0&&((uint8_t)label[n]&0xc0)==0x80)n--;label.remove(n);}label+="…";}}else{if(drawing->textWidth(label)>w-8)drawing->setTextSize(0.875f);if(drawing->textWidth(label)>w-4)drawing->setTextSize(0.75f);}drawing->drawString(label,x+w/2,y+h/2);}
 drawing->setTextDatum(top_left);drawing->setTextSize(1);
}
String clockText(int s){char b[16];snprintf(b,sizeof(b),"%d:%02d",max(0,s)/60,max(0,s)%60);return b;}
class CoverSink : public Stream {
 public:
 size_t used=0;bool overflow=false;
 size_t write(uint8_t b) override {return write(&b,1);}
 size_t write(const uint8_t *b,size_t n) override {if(n>sizeof(cover)-used){overflow=true;return 0;}memcpy(cover+used,b,n);used+=n;return n;}
 int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}void flush() override{}
};
void loadCover(const String &key){
 coverLen=0;if(key.isEmpty()||player.isEmpty())return;
 HTTPClient h;h.setTimeout(2200);h.setConnectTimeout(1600);h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
 h.begin(base()+"/music/current/cover_144x144_f.jpg?player="+enc(player));
 int code=h.GET();CoverSink sink;
 if(code==200){int copied=h.writeToStream(&sink);if(copied>0&&!sink.overflow&&sink.used>2&&cover[0]==0xff&&cover[1]==0xd8)coverLen=sink.used;}
 Serial.printf("COVER HTTP=%d bytes=%u valid=%u\n",code,unsigned(sink.used),unsigned(coverLen));h.end();
}

void pollStatus(){
 if(player.isEmpty())return;bool menuView=screen!=NOW&&screen!=CLOCK;bool savedDirty=dirty;String savedError=errorText;JsonDocument d;
 if(!rpc(player,{"status","-","1","tags:alKctu"},d)){dirty=menuView?savedDirty:true;if(menuView)errorText=savedError;return;}
 JsonVariant r=d["result"];playerName=r["player_name"]|playerName;mode=r["mode"]|"stop";playerPower=r["power"].isNull()?true:r["power"].as<bool>();clockIdle.observe(!playerPower||mode=="pause",millis());if(screen==CLOCK&&playerPower&&mode=="play")wakeClock();if(!sliderDragging)volume=r["mixer volume"]|0;
 auto position=PlaybackModel::position(r);track=position.current;playlistTracks=position.total;isStream=position.stream;stationName=String(position.station.c_str());elapsed=int(r["time"]|0.0);duration=int(r["duration"]|0.0);
 JsonVariant t=r["playlist_loop"][0]; title=t["title"]|"";if(title.isEmpty())title=tr("No playback");artist=t["artist"]|"";album=t["album"]|"";
 String key=t["artwork_url"]|"";if(key.isEmpty())key=t["coverid"].as<String>();if(key=="null")key="";
 if(!menuView&&screen!=CLOCK&&(key!=artKey||(coverLen==0&&!key.isEmpty()))){artKey=key;loadCover(key);}static uint32_t logged=0;if(millis()-logged>15000||logged==0){logged=millis();Serial.printf("PLAYER %s mode=%s title=%s cover=%u connected=%d\n",playerName.c_str(),mode.c_str(),title.c_str(),unsigned(coverLen),r["player_connected"].as<int>());}if(menuView){errorText=savedError;dirty=savedDirty;}else dirty=true;
}
void requestStandby(){if(player.isEmpty())return;JsonDocument d;if(rpc(player,{"power","0"},d)){playerPower=false;clockIdle.observe(true,millis());pollStatus();noteActivity();}dirty=true;}
void loadPage();
String jsonText(JsonVariantConst v){return String(MenuModel::str(v).c_str());}
String encodeAction(JsonDocument &d){String s;serializeJson(d,s);return s;}
void rememberPage(){noteActivity();if(depth<8)nav[depth++]={screen,offset,artistId,albumId,favoriteId,pageAction,pageTitle,pageNode};}
String nodeAction(const String &id){JsonDocument d;d["node"]=id;d["mode"]="browse";return encodeAction(d);}
bool menuRequest(const String &raw,bool browse,JsonDocument &out){
 JsonDocument d,args;if(deserializeJson(d,raw)){errorText="Ungültige Menüaktion";return false;}
 auto a=args.to<JsonArray>();for(JsonVariantConst v:d["cmd"].as<JsonArrayConst>())a.add(v);
 if(a.size()==0){errorText="Keine Menüaktion";return false;}
 if(browse){a.add(String(offset));a.add("4");}
 bool hasMenu=false;for(JsonPairConst p:d["params"].as<JsonObjectConst>()){String key=p.key().c_str();if(key=="_index"||key=="_quantity")continue;if(key=="menu")hasMenu=true;a.add(key+":"+jsonText(p.value()));}
 if(browse&&!hasMenu)a.add("menu:1");return rpcArgs(player,a,out);
}
void putMenuItem(JsonVariantConst item,JsonVariantConst base=JsonVariantConst()){
 if(itemCount>=4)return;Item &i=items[itemCount++];i=Item{};i.name=jsonText(item["text"]);if(i.name.isEmpty())i.name=jsonText(item["title"]);if(i.name.isEmpty())i.name=jsonText(item["name"]);if(i.name.isEmpty())i.name="Information";i.name.replace("\n"," / ");
 i.playAction=String(MenuModel::action(item,base,"play").c_str());i.nextAction=String(MenuModel::action(item,base,"add-hold").c_str());if(i.nextAction.isEmpty())i.nextAction=String(MenuModel::action(item,base,"insert").c_str());i.addAction=String(MenuModel::action(item,base,"add").c_str());
 i.id=jsonText(item["id"]);i.action=String(MenuModel::action(item,base).c_str());JsonDocument d;deserializeJson(d,i.action);i.folder=d["mode"]=="browse";String type=jsonText(item["type"]);bool media=type=="audio"||type=="playlist"||item["actions"]["play"].is<JsonObjectConst>()||(item["isPinned"].as<bool>()&&item["url"].is<const char*>());if(!media){i.playAction="";i.nextAction="";i.addAction="";}i.translateLabel=d["mode"]!="play"&&(item["command"].is<JsonArrayConst>()||item["isANode"].as<bool>()||type=="redirect"||(!item["commonParams"].is<JsonObjectConst>()&&!item["presetParams"].is<JsonObjectConst>()&&type!="playlist"&&type!="audio"));
}
void loadNodePage(const String &node){
 std::vector<JsonVariantConst> rows;
 for(JsonVariantConst i:nodeMenu["result"]["item_loop"].as<JsonArrayConst>())if(jsonText(i["node"])==node&&MenuModel::visible(i))rows.push_back(i);
 std::stable_sort(rows.begin(),rows.end(),[](JsonVariantConst a,JsonVariantConst b){return a["weight"].as<int>()<b["weight"].as<int>();});total=rows.size();
 for(int n=offset;n<total&&itemCount<4;n++)putMenuItem(rows[n]);
}
String playerMenuKey(){String id=player;id.replace(":","");return "mh"+id;}
void loadRootMenu(){
 JsonDocument d,hidden;deserializeJson(hidden,prefs.getString(playerMenuKey().c_str(),"[]"));
 if(!rpc(player,{"menu","0","200","direct:1"},d))return;
 nodeMenu=std::move(d);rootMenu.clear();String saved;
 if(rpc(player,{"playerpref","plugin.material-skin:items","?"},d))saved=jsonText(d["result"]["_p2"]);
 if(saved.isEmpty()&&rpc("",{"pref","plugin.material-skin:items","?"},d))saved=jsonText(d["result"]["_p2"]);
 bool configured=!saved.isEmpty()&&!deserializeJson(rootMenu,saved)&&rootMenu.is<JsonArray>();
 if(configured){
  for(JsonObject entry:rootMenu.as<JsonArray>()){
   String id=jsonText(entry["id"]);
   if(id=="top:/mm"){entry["isANode"]=1;entry["id"]="myMusic";entry["text"]="Meine Musik";}
   else if(id=="top:/fav")entry["text"]="Favoriten";
   else if(id=="top:/apps")entry["text"]="Anwendungen";
  }
  std::vector<JsonVariantConst> rows;for(JsonVariantConst i:rootMenu.as<JsonArrayConst>())if(MenuModel::visible(i,hidden.as<JsonArrayConst>()))rows.push_back(i);total=rows.size();
  for(int n=offset;n<total&&itemCount<4;n++)putMenuItem(rows[n]);
 }else loadNodePage("home");
 menuOwner=player;Serial.printf("MENU root player=%s source=%s total=%d offset=%d\n",player.c_str(),configured?"material-skin":"lms",total,offset);
 for(int i=0;i<itemCount;i++)Serial.printf("MENU item %d %s\n",offset+i,items[i].name.c_str());
}
void loadBrowseMenu(){
 if(!pageNode.isEmpty()){loadNodePage(pageNode);return;}
 JsonDocument a;deserializeJson(a,pageAction);
 if(a["choices"].is<JsonArray>()){total=a["choices"].size();for(int n=offset;n<total&&itemCount<4;n++){JsonDocument item;item["text"]=a["labels"][n];item["actions"]["do"]=a["choices"][n];putMenuItem(item.as<JsonVariantConst>());}return;}
 JsonDocument d;if(!menuRequest(pageAction,true,d))return;
 JsonVariantConst r=d["result"].as<JsonVariantConst>();JsonArrayConst loop=r["item_loop"].as<JsonArrayConst>();bool localPage=false;
 if(loop.isNull())loop=r["loop_loop"].as<JsonArrayConst>();
 if(loop.isNull()&&r["extras_loop"].is<JsonArrayConst>()){loop=r["extras_loop"].as<JsonArrayConst>();localPage=true;}
 total=r["count"].isNull()?int(loop.size()):r["count"].as<int>();int index=0;
 for(JsonVariantConst i:loop){if(localPage&&index++<offset)continue;if(!MenuModel::visible(i))continue;if(itemCount>=4)break;putMenuItem(i,r["base"]);}
 if(itemCount==0){String message=jsonText(r["window"]["text"]);if(message.isEmpty())message=jsonText(r["text"]);if(!message.isEmpty()){items[0]=Item{};items[0].name=message;itemCount=1;}}
 Serial.printf("MENU browse player=%s title=%s total=%d offset=%d items=%d\n",player.c_str(),pageTitle.c_str(),total,offset,itemCount);
 for(int i=0;i<itemCount;i++)Serial.printf("MENU item %d %s\n",offset+i,items[i].name.c_str());
}
void openDynamic(const String &raw,const String &label){rememberPage();pageAction=raw;pageTitle=label;JsonDocument d;deserializeJson(d,raw);pageNode=jsonText(d["node"]);screen=BROWSE;offset=0;loadPage();}
void activateDynamic(const String &raw,const String &label){
 if(raw.isEmpty())return;JsonDocument a;if(deserializeJson(a,raw))return;String mode=jsonText(a["mode"]);
 if(mode=="web"){errorText="Im LMS-Webmenü verfügbar";dirty=true;return;}
 if(!a["input"].isNull()||raw.indexOf("__INPUT__")>=0||raw.indexOf("__TAGGEDINPUT__")>=0){rememberPage();pendingInput=raw;pageTitle=label;inputValue=jsonText(a["input"]["initialText"]);screen=TEXTINPUT;dirty=true;return;}
 if(mode=="browse"){openDynamic(raw,label);return;}
 JsonDocument response;if(menuRequest(raw,false,response)){if(mode=="play"){screen=NOW;depth=0;pollStatus();}else loadPage();}dirty=true;
}
bool openContext(int index){
 if((screen!=HOME&&screen!=BROWSE)||index<0||index>=itemCount)return false;
 Item item=items[index];contextCount=0;
 if(!item.playAction.isEmpty())contextChoices[contextCount++]={"Play now",item.playAction};
 if(!item.nextAction.isEmpty())contextChoices[contextCount++]={"Play next",item.nextAction};
 if(!item.addAction.isEmpty())contextChoices[contextCount++]={"Add to queue",item.addAction};
 if(contextCount==0)return false;
 rememberPage();pageTitle=item.name;screen=CONTEXT;noteActivity();offset=0;errorText="";dirty=true;
 Serial.printf("MENU context player=%s title=%s count=%d\n",player.c_str(),pageTitle.c_str(),contextCount);
 for(int n=0;n<contextCount;n++)Serial.printf("CONTEXT item %d %s\n",n,contextChoices[n].label.c_str());return true;
}
void back();
void selectContext(int index){
 if(index<0||index>=contextCount)return;String raw=contextChoices[index].action;JsonDocument action,response;deserializeJson(action,raw);
 if(menuRequest(raw,false,response)){if(action["mode"]=="play"){screen=NOW;depth=0;pollStatus();}else back();}dirty=true;
}
void drawInput(){
 text(inputValue+"_",8,36,304,ACCENT);const char *keys="abcdefghijklmnopqrstuvwxyz0123456789 .-_";
 for(int i=0;i<40;i++){String key=String(keys[i]);if(keyboardCaps)key.toUpperCase();button(2+(i%10)*32,68+(i/10)*27,29,25,key);}
 button(238,181,78,53,"Zurück");button(110,181,60,53,"<-");button(174,181,60,53,"Aa");button(4,181,102,53,"OK");
}
void touchInput(int x,int y){
 if(y>=181){if(x>=236){if(depth){auto n=nav[--depth];screen=n.page;offset=n.offset;pageAction=n.action;pageTitle=n.title;pageNode=n.node;loadPage();}}else if(x>=108&&x<172){if(inputValue.length())inputValue.remove(inputValue.length()-1);dirty=true;}else if(x>=172){keyboardCaps=!keyboardCaps;dirty=true;}else{
  JsonDocument a;deserializeJson(a,pendingInput);a.remove("input");for(JsonPair p:a["params"].as<JsonObject>()){if(p.value().is<const char*>()){String v=p.value().as<String>();v.replace("__TAGGEDINPUT__",inputValue);v.replace("__INPUT__",inputValue);p.value().set(v);}}
  String label=pageTitle;if(depth){auto n=nav[--depth];screen=n.page;offset=n.offset;pageAction=n.action;pageTitle=n.title;pageNode=n.node;}
  activateDynamic(encodeAction(a),label);
 }}else if(y>=68&&y<176&&inputValue.length()<100){int i=((y-68)/27)*10+x/32;if(i<40){char c="abcdefghijklmnopqrstuvwxyz0123456789 .-_"[i];if(keyboardCaps)c=toupper(c);inputValue+=c;dirty=true;}}
}
void loadPage(){
 itemCount=0;total=0;errorText="";JsonDocument d;bool ok=false;String loop,label;
 switch(screen){
 case LANGUAGE:total=I18n::languageCount;dirty=true;return;
 case HOME:if(player.isEmpty()){errorText="Bitte zuerst Player wählen";dirty=true;return;}loadRootMenu();dirty=true;return;
 case BROWSE:loadBrowseMenu();dirty=true;return;
 case PLAYERS:ok=rpc("",{"serverstatus",String(offset),"4"},d);loop="players_loop";label="name";break;
 case ALBUMS:ok=artistId.isEmpty()?rpc("",{"albums",String(offset),"4"},d):rpc("",{"albums",String(offset),"4","artist_id:"+artistId},d);loop="albums_loop";label="album";break;
 case ARTISTS:ok=rpc("",{"artists",String(offset),"4"},d);loop="artists_loop";label="artist";break;
 case TRACKS:ok=rpc("",{"titles",String(offset),"4","album_id:"+albumId,"sort:tracknum","tags:t"},d);loop="titles_loop";label="title";break;
 case LISTS:ok=rpc("",{"playlists",String(offset),"4"},d);loop="playlists_loop";label="playlist";break;
 case FAVORITES:ok=favoriteId.isEmpty()?rpc(player,{"favorites","items",String(offset),"4"},d):rpc(player,{"favorites","items",String(offset),"4","item_id:"+favoriteId},d);loop="loop_loop";label="name";break;
 case QUEUE:ok=rpc(player,{"status",String(offset),"4","tags:t"},d);loop="playlist_loop";label="title";break;
 default:dirty=true;return;
 }
 if(ok){JsonVariant r=d["result"];total=screen==PLAYERS?(r["player count"]|0):(screen==QUEUE?(r["playlist_tracks"]|0):(r["count"]|0));
 for(JsonObject o:r[loop].as<JsonArray>()){if(itemCount>=4)break;auto &i=items[itemCount++];i.name=o[label]|"?";i.id=screen==PLAYERS?o["playerid"].as<String>():o["id"].as<String>();i.url=o["url"]|"";String type=o["type"]|"";i.folder=type=="outline"||type=="folder"||((o["hasitems"]|0)>0);if(screen==PLAYERS&&!(o["connected"]|0))i.name+=" ("+tr("Offline")+")";}}
 dirty=true;
}
void go(Screen s){rememberPage();screen=s;offset=0;loadPage();}
void back(){timeStepActive=false;noteActivity();if(depth){auto n=nav[--depth];screen=n.page;offset=n.offset;artistId=n.aid;albumId=n.bid;favoriteId=n.fid;pageAction=n.action;pageTitle=n.title;pageNode=n.node;loadPage();}else{screen=NOW;dirty=true;}}
String heading(){switch(screen){case NIGHTSETUP:return tr("Night dimming");case NIGHTTIME:return tr(nightEditingEnd?"End time":"Start time");case CLOCK:return tr("Clock");case IDLESETUP:return tr("Auto return");case HOME:return tr("Home");case BROWSE:case TEXTINPUT:case CONTEXT:return tr(pageTitle);case CONTROLS:return tr("Remote control");case LANGUAGE:return tr("Language");case PLAYERS:return tr("Select player");case ALBUMS:return tr("Albums");case ARTISTS:return tr("Artists");case TRACKS:return tr("Tracks");case LISTS:return tr("Playlists");case FAVORITES:return tr("Favorites");case QUEUE:return tr("Queue");case SETTINGS:return tr("Settings");default:return playerName.isEmpty()?"LMS Remote":playerName;}}

// A small row buffer replaces individual fields without clearing the display.
// The destination clip keeps a text update from touching the adjacent cover.
void fieldText(const String &value,int x,int y,int width,uint16_t color=TFT_WHITE){
 fieldBuffer.fillSprite(BG);fieldBuffer.setFont(&fonts::efontJA_16);fieldBuffer.setTextSize(1);
 fieldBuffer.setTextDatum(top_left);fieldBuffer.setTextColor(color,BG);
 String s=value;
 while(s.length()&&fieldBuffer.textWidth(s)>width){int n=s.length()-1;while(n>0&&((uint8_t)s[n]&0xc0)==0x80)n--;s.remove(n);}
 fieldBuffer.drawString(s,0,0);lcd.setClipRect(x,y,width,24);fieldBuffer.pushSprite(x,y);lcd.clearClipRect();++fieldRedraws;
}
void drawNow(bool force){
 static String oldHeading,oldTitle,oldArtist,oldAlbum,oldTime,oldMode,oldError,oldCover,oldPosition;
 static int oldTrack=-1,oldVolume=-1,oldProgress=-1,oldWifi=-1,oldPower=-1;
 String name=playerName.isEmpty()?"LMS Remote":playerName,time=errorText.isEmpty()?(clockText(elapsed)+(duration>0?" / "+clockText(duration):" / "+tr("Live"))):tr(errorText);
 String coverState=player+"|"+artKey+"|"+String(coverLen);
 int wifi=WiFi.status()==WL_CONNECTED;
 if(force){lcd.fillScreen(BG);++fullRedraws;lcd.drawFastHLine(0,28,320,CARD);
  fieldText(tr("Wi-Fi"),208,4,40,MUTED);
  lcd.drawLine(129,11,133,11,TFT_WHITE);lcd.drawLine(133,11,138,7,TFT_WHITE);lcd.drawLine(138,7,138,21,TFT_WHITE);lcd.drawLine(138,21,133,17,TFT_WHITE);lcd.drawLine(133,17,129,17,TFT_WHITE);lcd.drawLine(129,17,129,11,TFT_WHITE);lcd.drawBezier(141,10,146,14,141,18,TFT_WHITE);lcd.drawBezier(143,7,154,14,143,21,TFT_WHITE);
  button(4,184,64,52,"");button(172,184,64,52,"");button(240,184,76,52,"Menü");
  // Single arrows mean previous/next track, not fast winding.
  for(int cx:{36,204}){int dir=cx==36?-1:1;lcd.drawLine(cx-12,210,cx+12,210,TFT_WHITE);lcd.drawLine(cx+dir*12,210,cx+dir*4,202,TFT_WHITE);lcd.drawLine(cx+dir*12,210,cx+dir*4,218,TFT_WHITE);}}
 if(force||name!=oldHeading)fieldText(name,8,4,116,ACCENT);
 if(force||oldPower!=int(playerPower)){lcd.fillRoundRect(294,4,22,20,4,CARD);uint16_t color=playerPower?TFT_WHITE:ACCENT;lcd.drawCircle(305,15,6,color);lcd.fillRect(302,7,7,6,CARD);lcd.drawLine(305,7,305,15,color);oldPower=playerPower;}
 if(force||volume!=oldVolume)fieldText(String(volume)+"%",157,4,42,MUTED);
 if(force||wifi!=oldWifi)lcd.fillCircle(260,13,3,wifi?TFT_GREEN:TFT_ORANGE);
 if(force||title!=oldTitle)fieldText(title,8,38,154);
 if(force||artist!=oldArtist)fieldText(artist,8,66,154,ACCENT);
 if(force||album!=oldAlbum)fieldText(album,8,92,154,MUTED);
 String position=positionLabel();if(force||position!=oldPosition)fieldText(position,8,124,154,MUTED);
 if(force||time!=oldTime||errorText!=oldError)fieldText(time,8,148,154,errorText.isEmpty()?TFT_WHITE:TFT_ORANGE);
 if(force||mode!=oldMode)button(72,184,96,52,mode=="play"?"Pause":"Play");
 if(force||coverState!=oldCover){lcd.fillRoundRect(168,34,144,144,5,CARD);++coverRedraws;
  if(coverLen){if(!lcd.drawJpg(cover,coverLen,168,34,144,144))text(tr("Kein Cover"),192,96,112,MUTED);}else text(tr("Kein Cover"),192,96,112,MUTED);}
 int progress=duration>0?constrain(int(int64_t(elapsed)*154/duration),0,154):0;
 if(force||progress!=oldProgress){fieldBuffer.fillSprite(CARD);if(progress)fieldBuffer.fillRect(0,0,progress,4,ACCENT);lcd.setClipRect(8,174,154,4);fieldBuffer.pushSprite(8,174);lcd.clearClipRect();++fieldRedraws;}
 oldHeading=name;oldTitle=title;oldArtist=artist;oldAlbum=album;oldTime=time;oldMode=mode;oldError=errorText;oldCover=coverState;oldTrack=track;oldPosition=position;oldVolume=volume;oldProgress=progress;oldWifi=wifi;
}
String wallMinute(int minute){char b[6];snprintf(b,sizeof b,"%02d:%02d",minute/60,minute%60);return b;}
void applyDisplayBrightness(){
 time_t epoch=time(nullptr);tm local{};bool valid=clockSynced&&epoch>1700000000;if(valid)localtime_r(&epoch,&local);
 nightActive=NightDimming::active(nightEnabled,valid,nightStart,nightEnd,local.tm_hour*60+local.tm_min);
 int target=screen==NIGHTLEVEL?nightBrightness:screen==BRIGHTNESS_OVERLAY?displayBrightness:NightDimming::level(displayBrightness,nightBrightness,nightActive);
 if(target!=effectiveBrightness){lcd.setBrightness((target*255+50)/100);effectiveBrightness=target;}
}
void saveNightSettings(){prefs.putBool("nightOn",nightEnabled);prefs.putUShort("nightStart",nightStart);prefs.putUShort("nightEnd",nightEnd);prefs.putUChar("nightLevel",nightBrightness);applyDisplayBrightness();}
void setNightBrightness(int value){nightBrightness=constrain(value,5,100);applyDisplayBrightness();dirty=true;}
void setDisplayBrightness(int percent){displayBrightness=constrain(percent,5,100);applyDisplayBrightness();dirty=true;}
void saveBrightness(){if(displayBrightness!=savedBrightness){prefs.putUChar("brightness",displayBrightness);savedBrightness=displayBrightness;}}
void closeOverlay(){timeStepActive=false;if(screen==BRIGHTNESS_OVERLAY)saveBrightness();if(screen==NIGHTLEVEL)saveNightSettings();sliderDragging=false;screen=NOW;depth=0;noteActivity();touchGesture.reset();invalidateDisplay=true;dirty=true;lastPoll=0;}
void openOverlay(Screen target){
 screen=target;overlayOffset=0;sliderDragging=false;noteActivity();touchGesture.reset();invalidateDisplay=true;dirty=true;
 if(target==PLAYER_OVERLAY){overlayPlayers.clear();JsonDocument d;int start=0,totalPlayers=1;
  while(start<totalPlayers){if(!rpc("",{"serverstatus",String(start),"50"},d))break;JsonVariant result=d["result"];totalPlayers=result["player count"]|0;int received=0;
   for(JsonObject entry:result["players_loop"].as<JsonArray>()){overlayPlayers.push_back({entry["name"]|"?",entry["playerid"]|"",bool(entry["connected"]|0)});received++;}if(!received)break;start+=received;
  }
 }
}
void sendOverlayVolume(bool finalSend){if(player.isEmpty()||volume==sentVolume)return;if(!finalSend&&uint32_t(millis()-lastVolumeSend)<140)return;JsonDocument d;if(rpc(player,{"mixer","volume",String(volume)},d))sentVolume=volume;lastVolumeSend=millis();lastPoll=millis();}
void drawOverlay(bool force){
 if(force){drawNow(true);lcd.fillRoundRect(20,32,280,192,8,BG);lcd.drawRoundRect(20,32,280,192,8,ACCENT);}
 if(screen==VOLUME_OVERLAY||screen==BRIGHTNESS_OVERLAY||screen==NIGHTLEVEL){
  bool brightness=screen==BRIGHTNESS_OVERLAY||screen==NIGHTLEVEL;int value=screen==NIGHTLEVEL?nightBrightness:brightness?displayBrightness:volume;
  if(force){text(tr(screen==NIGHTLEVEL?"Night brightness":brightness?"Display brightness":"Volume"),36,44,248,ACCENT);text("100%",58,77,65,MUTED);text(brightness?"5%":"0%",58,brightness?163:185,65,MUTED);if(brightness)button(202,188,90,28,"Back");}
  lcd.fillRect(137,76,46,130,BG);lcd.fillRoundRect(155,82,10,116,5,CARD);int y=UiLayout::volumeY(value);lcd.fillRoundRect(155,y,10,198-y,5,ACCENT);lcd.fillCircle(160,y,11,TFT_WHITE);
  lcd.fillRect(204,126,80,24,BG);text(String(value)+"%",204,126,78);
 }else if(screen==WIFI_OVERLAY&&force){
  text(tr("Wi-Fi"),36,44,248,ACCENT);text(tr("Wi-Fi name"),36,76,248,MUTED);text(WiFi.SSID(),36,98,248);
  text(tr("IP address")+": "+WiFi.localIP().toString(),36,130,248);text(tr("Signal")+": "+String(WiFi.RSSI())+" dBm",36,158,248);text(tr(WiFi.status()==WL_CONNECTED?"Connected":"Offline"),36,190,248,ACCENT);
 }else if(screen==PLAYER_OVERLAY){
  lcd.fillRect(28,40,264,176,BG);text(tr("Select player"),36,42,248,ACCENT);
  for(int n=0;n<4&&overlayOffset+n<int(overlayPlayers.size());n++){auto &entry=overlayPlayers[overlayOffset+n];button(28,68+n*29,264,27,entry.name+(entry.id==player?" *":"")+(entry.connected?"":" ("+tr("Offline")+")"));}
  button(28,188,128,28,"Up");button(164,188,128,28,"Down");
  if(overlayPlayers.empty())text(tr(errorText.isEmpty()?"No entries":errorText),36,100,248,MUTED);
 }
}
void touchOverlay(int x,int y){
 if(!UiLayout::insideOverlay(x,y)){closeOverlay();return;}
 if(screen==BRIGHTNESS_OVERLAY||screen==NIGHTLEVEL){bool night=screen==NIGHTLEVEL;if(y>=188&&y<216&&x>=202&&x<292){if(night)saveNightSettings();else saveBrightness();sliderDragging=false;back();}else if(x>=125&&x<=195&&y>=70&&y<=211){if(night){setNightBrightness(UiLayout::volumeAt(y));saveNightSettings();}else{setDisplayBrightness(UiLayout::volumeAt(y));saveBrightness();}}return;}
 if(screen==PLAYER_OVERLAY){if(y>=188&&y<216){if(x<160)overlayOffset=max(0,overlayOffset-4);else if(overlayOffset+4<int(overlayPlayers.size()))overlayOffset+=4;dirty=true;}
  else if(y>=68&&y<184&&(y-68)%29<27){int n=overlayOffset+(y-68)/29;if(n<int(overlayPlayers.size())){auto entry=overlayPlayers[n];player=entry.id;playerName=entry.name;rootMenu.clear();nodeMenu.clear();menuOwner="";prefs.putString("player",player);artKey="";coverLen=0;closeOverlay();pollStatus();}}
 }
}
void draw(){
 dirty=false;bool forceBackdrop=invalidateDisplay;bool force=invalidateDisplay||renderedScreen!=int(screen);invalidateDisplay=false;
 if(!menuOverlay()&&menuCanvasReady){menuCanvas.deleteSprite();menuCanvasReady=false;}
 if(compactOverlay()){drawOverlay(force);renderedScreen=int(screen);return;}
 if(screen==CLOCK){if(force){lcd.fillScreen(BG);++fullRedraws;}flipClock.draw(millis(),time(nullptr),force);renderedScreen=int(screen);return;}
 if(screen==NOW){drawNow(force);renderedScreen=int(screen);return;}
 if(force&&(!menuCanvasReady||forceBackdrop))drawNow(true);
 if(!menuCanvasReady){menuCanvas.setColorDepth(16);menuCanvasReady=menuCanvas.createSprite(320,20)!=nullptr;menuCanvas.setPivot(160,10);}
 if(!menuCanvasReady){Serial.println("MENU overlay allocation failed");closeOverlay();return;}
 drawing=&menuCanvas;renderedScreen=int(screen);
 lcd.setClipRect(12,12,296,216);
 for(int strip=0;strip<12;strip++){drawingOffset=strip*20;menuCanvas.fillSprite(BG);
 text("×",8,6,30,MUTED);text(heading(),42,6,232,ACCENT);text("←",289,6,26,ACCENT);menuCanvas.drawFastHLine(12,UiLayout::panelY(28)-drawingOffset,296,CARD);
 if(screen==NIGHTSETUP){
 button(4,34,312,30,tr("Night dimming")+": "+tr(nightEnabled?"On":"Off"));
 button(4,70,152,42,tr("Start time")+" "+wallMinute(nightStart));button(162,70,154,42,tr("End time")+" "+wallMinute(nightEnd));
 button(4,117,312,40,tr("Night brightness")+": "+String(nightBrightness)+"%");
 text(tr(clockSynced?"Daily, local time":"Waiting for time"),8,160,300,MUTED);button(234,181,82,53,"Back");
 }else if(screen==NIGHTTIME){
 if(errorText.isEmpty()){text(tr("Hours"),8,36,140,MUTED);text(tr("Minutes"),166,36,140,MUTED);}else text(tr(errorText),8,36,300,TFT_ORANGE);
 button(4,62,152,34,"↑");button(162,62,154,34,"↑");
 String value=wallMinute(nightDraft);text(value.substring(0,2),69,106,68,ACCENT);text(value.substring(3),228,106,68,ACCENT);
 button(4,134,152,34,"↓");button(162,134,154,34,"↓");
 button(4,181,154,53,"Save");button(234,181,82,53,"Back");
 }else if(screen==IDLESETUP){
 text(tr("Seconds")+": "+timeoutInput,8,36,300,ACCENT);text(tr(errorText.isEmpty()?"0 = Off, 1-3600 seconds":errorText),8,57,300,errorText.isEmpty()?MUTED:TFT_ORANGE);
 const char* keys[]={"1","2","3","4","5","6","7","8","9","Off","0","<-"};for(int n=0;n<12;n++)button(4+(n%3)*105,78+(n/3)*25,102,23,keys[n]);
 button(234,181,82,53,"Back");button(4,181,154,53,"Save");
 }else if(screen==CONTEXT){
 for(int n=0;n<contextCount;n++)button(4,34+n*47,312,43,contextChoices[n].label);button(234,181,82,53,"Back");
 }else if(screen==LANGUAGE){
 for(int n=offset;n<total&&n<offset+4;n++)button(4,32+(n-offset)*36,312,33,String(I18n::languageNames[n])+(uiLanguage==I18n::languageCodes[n]?" *":""));
 button(234,181,82,53,"Back");button(90,181,68,53,"Up");button(162,181,68,53,"Down");button(4,181,82,53,"Now playing");
 }else if(screen==TEXTINPUT){drawInput();
 }else if(screen==CONTROLS){
 button(4,34,154,60,"Player");button(162,34,154,60,"Warteschlange");button(4,99,154,60,"Einstellungen");button(162,99,154,60,"Aktuell");button(234,181,82,53,"Zurück");
 }else if(screen==SETTINGS){
 text(WiFi.localIP().toString(),8,34,300,ACCENT);text(tr("Server")+": "+host,8,54,300);button(4,74,152,22,"Brightness");button(162,74,154,22,"Night dimming");
 button(4,98,152,38,"Calibrate touch");button(162,98,154,38,"Wi-Fi setup");button(4,140,152,36,"Language");button(162,140,154,36,"Auto return");button(234,181,82,53,"Back");
 }else{
 if(itemCount==0)text(tr(errorText.isEmpty()?"No entries":errorText),10,76,300,MUTED);
 for(int i=0;i<itemCount;i++){menuCanvas.fillRoundRect(UiLayout::panelX(4),UiLayout::panelY(32+i*36)-drawingOffset,UiLayout::panelX(316)-UiLayout::panelX(4),UiLayout::panelY(65+i*36)-UiLayout::panelY(32+i*36),4,CARD);text((items[i].translateLabel?tr(items[i].name):items[i].name)+(items[i].folder?" >":""),10,41+i*36,300);}
 button(234,181,82,53,"Zurück");button(90,181,68,53,"Hoch");button(162,181,68,53,"Runter");button(4,181,82,53,screen==HOME?"Gerät":"Aktuell");
 }
 if(!errorText.isEmpty()&&screen!=IDLESETUP&&screen!=NIGHTTIME)text(tr(errorText),8,160,300,TFT_ORANGE);
 menuCanvas.pushSprite(0,strip*20);
 }
 lcd.clearClipRect();drawingOffset=0;drawing=&lcd;
 lcd.drawRoundRect(11,11,298,218,5,ACCENT);
}

void saveTouch(){Serial.println("TOUCH calibration started: tap all crosses");uint16_t cal[8];lcd.fillScreen(BG);lcd.setTextColor(TFT_WHITE);lcd.drawString(tr("Tap the crosses accurately"),35,105);lcd.calibrateTouch(cal,uint16_t(TFT_WHITE),BG,12);prefs.putBytes("touch",cal,sizeof(cal));Serial.println("TOUCH calibration saved");invalidateDisplay=true;dirty=true;}
void drawPortal(){lcd.fillScreen(BG);text(tr("Wi-Fi setup"),12,15,300,ACCENT);text(tr("Wi-Fi: LMS-Remote"),12,60,300);text(tr("Connect without a password"),12,90,300);text(tr("Browser: 192.168.4.1"),12,130,300);text(tr("Open on your phone or computer"),12,170,300,MUTED);}
void startPortal(bool automatic=false){portal=true;portalAutomatic=automatic;WiFi.mode(WIFI_AP_STA);WiFi.softAP("LMS-Remote");dns.start(53,"*",WiFi.softAPIP());drawPortal();Serial.println("SETUP WLAN LMS-Remote / open / http://192.168.4.1");}

void selectItem(int i){if(i<0||i>=itemCount)return;auto it=items[i];if(screen==HOME||screen==BROWSE){activateDynamic(it.action,it.name);return;}
 switch(screen){case PLAYERS:player=it.id;playerName=it.name;rootMenu.clear();nodeMenu.clear();menuOwner="";prefs.putString("player",player);screen=NOW;depth=0;artKey="";coverLen=0;pollStatus();break;
 case ARTISTS:artistId=it.id;go(ALBUMS);break;
 case ALBUMS:albumId=it.id;go(TRACKS);break;
 case TRACKS:command({"playlistcontrol","cmd:load","track_id:"+it.id});screen=NOW;depth=0;break;
 case LISTS:command({"playlistcontrol","cmd:load","playlist_id:"+it.id});screen=NOW;depth=0;break;
 case FAVORITES:if(it.folder){go(FAVORITES);favoriteId=it.id;loadPage();}else{command({"favorites","playlist","play","item_id:"+it.id});screen=NOW;depth=0;}break;
 case QUEUE:command({"playlist","index",String(offset+i)});screen=NOW;depth=0;break;default:break;}dirty=true;
}
int nightStepAt(int x,int y){if(!((x>=4&&x<156)||(x>=162&&x<316)))return 0;int sign=y>=62&&y<96?1:y>=134&&y<168?-1:0;return sign*(x<160?60:1);}
void stepNightTime(int step){nightDraft=NightDimming::adjust(nightDraft,abs(step)==60,step>0?1:-1);errorText="";dirty=true;}
void touch(int x,int y){
 if(compactOverlay()){touchOverlay(x,y);return;}
 if(menuOverlay()){
  if(!UiLayout::insideMenuPanel(x,y)){closeOverlay();return;}
  x=UiLayout::menuX(x);y=UiLayout::menuY(y);
  if(y<28){if(x>=280)back();else if(x<38)closeOverlay();return;}
 }
 if(screen==NOW){if(y<28){if(x>=290)requestStandby();else if(x<126)openOverlay(PLAYER_OVERLAY);else if(x<202)openOverlay(VOLUME_OVERLAY);else openOverlay(WIFI_OVERLAY);return;}if(y<184)return;if(x<70)command({"playlist","index","-1"});else if(x<170){if(mode=="play")command({"pause","1"});else{if(!playerPower)command({"power","1"});command({"play"});}}else if(x<238)command({"playlist","index","+1"});else go(HOME);}
 else if(screen==NIGHTSETUP){
 if(y>=181){if(x>=232)back();}
 else if(y>=34&&y<64){nightEnabled=!nightEnabled;saveNightSettings();dirty=true;}
 else if(y>=70&&y<112){nightEditingEnd=x>=160;nightDraft=nightEditingEnd?nightEnd:nightStart;go(NIGHTTIME);}
 else if(y>=117&&y<157){rememberPage();openOverlay(NIGHTLEVEL);}
 }
 else if(screen==NIGHTTIME){
 if(y>=181){if(x>=232)back();else if(x<158){if(nightDraft==(nightEditingEnd?nightStart:nightEnd)){errorText="Start and end must differ";dirty=true;}else{if(nightEditingEnd)nightEnd=nightDraft;else nightStart=nightDraft;saveNightSettings();back();}}}
 else{int step=nightStepAt(x,y);if(step)stepNightTime(step);}
 }
 else if(screen==IDLESETUP){
 if(y>=181){if(x>=232)back();else if(x<158){if(!timeoutInput.isEmpty()&&setMenuTimeout(timeoutInput.toInt()))back();else{errorText="Enter 0 to 3600 seconds";dirty=true;}}}
 else if(y>=78&&y<178){int n=((y-78)/25)*3+min(2,x/105);if(n==9)timeoutInput="0";else if(n==11){if(timeoutInput.length())timeoutInput.remove(timeoutInput.length()-1);}else if(n>=0&&n<12&&timeoutInput.length()<4){if(timeoutInput=="0")timeoutInput="";timeoutInput+=n==10?'0':char('1'+n);}errorText="";dirty=true;}
 }
 else if(screen==CONTEXT){if(y>=181){if(x>=232)back();}else if(y>=34&&y<175){int n=(y-34)/47;if((y-34)%47<43)selectContext(n);}}
 else if(screen==LANGUAGE){if(y>=181){if(x<88){screen=NOW;depth=0;dirty=true;}else if(x<160){offset=max(0,offset-4);dirty=true;}else if(x<232){if(offset+4<total){offset+=4;dirty=true;}}else back();}else if(y>=32&&y<176){int n=offset+(y-32)/36;if(n<total){setLanguage(I18n::languageCodes[n]);back();}}}
 else if(screen==TEXTINPUT){touchInput(x,y);}
 else if(screen==CONTROLS){if(y>=181){if(x>=232)back();}else if(y>=34&&y<94){if(x<160)go(PLAYERS);else go(QUEUE);}else if(y>=99&&y<159){if(x<160)go(SETTINGS);else{screen=NOW;depth=0;dirty=true;}}}
 else if(screen==SETTINGS){if(y>=181){if(x>=232)back();}else if(y>=74&&y<96){if(x<160){rememberPage();openOverlay(BRIGHTNESS_OVERLAY);}else go(NIGHTSETUP);}else if(y>=140&&y<176){if(x<160)go(LANGUAGE);else{timeoutInput=String(menuIdle.seconds);go(IDLESETUP);}}else if(y>=98&&y<136){if(x<160)saveTouch();else startPortal();}}
 else if(y>=181){if(x<88){if(screen==HOME)go(CONTROLS);else{screen=NOW;depth=0;dirty=true;}}else if(x<160){offset=max(0,offset-4);loadPage();}else if(x<232){if(offset+4<total){offset+=4;loadPage();}}else back();}
 else if(y>=32&&y<176)selectItem((y-32)/36);
}
String htmlEscape(String value){value.replace("&","&amp;");value.replace("<","&lt;");value.replace(">","&gt;");value.replace("\"","&quot;");return value;}
String translatedHtml(String page){int from=0;while(true){int start=page.indexOf("{{",from);if(start<0)break;int end=page.indexOf("}}",start+2);if(end<0)break;String value=htmlEscape(tr(page.substring(start+2,end)));page=page.substring(0,start)+value+page.substring(end+2);from=start+value.length();}return page;}
void setupWeb(){
 web.on("/",HTTP_GET,[]{String page=R"HTML(<!doctype html><html lang="%LANG%"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LMS Remote</title><style>body{font:18px system-ui;background:#101820;color:white;max-width:480px;margin:30px auto;padding:20px}input,select,button{box-sizing:border-box;width:100%;padding:14px;margin:8px 0;font:inherit}button{background:#54cdee;border:0;border-radius:5px}form{margin:24px 0}</style><h1>LMS Remote</h1><form action="/language" method="post"><label>{{Language}}<select name="language">%LANGUAGES%</select></label><button>{{Apply language}}</button></form><form action="/timeout" method="post"><label>{{Auto return}} ({{Seconds}})<input type="number" name="seconds" min="0" max="3600" value="%TIMEOUT%" required></label><p>{{0 = Off, 1-3600 seconds}}</p><button>{{Save}}</button></form><form action="/save" method="post"><label>{{Wi-Fi name}}<input name="ssid" required maxlength="32" autocomplete="off"></label><label>{{Wi-Fi password}}<input name="pass" type="password" maxlength="64" autocomplete="new-password"></label><label>{{LMS server: IP address or DNS name}}<input name="host" value="%LMS_HOST%" placeholder="{{lms.lan or 10.10.172.109}}" required maxlength="100" autocapitalize="none" spellcheck="false"></label><button>{{Save and connect}}</button></form><p>{{Cover on the right, music information on the left.}}</p></html>)HTML";
 String choices;for(size_t n=0;n<I18n::languageCount;n++)choices+="<option value='"+String(I18n::languageCodes[n])+"'"+(uiLanguage==I18n::languageCodes[n]?" selected":"")+">"+htmlEscape(I18n::languageNames[n])+"</option>";
 page=translatedHtml(page);page.replace("%LANG%",uiLanguage);page.replace("%TIMEOUT%",String(menuIdle.seconds));page.replace("%LANGUAGES%",choices);page.replace("%LMS_HOST%",htmlEscape(host));web.send(200,"text/html; charset=utf-8",page);});
 web.on("/timeout",HTTP_POST,[]{String value=web.arg("seconds");bool valid=value.length()>0&&value.length()<=4;for(size_t n=0;n<value.length();n++)if(!isDigit(value[n]))valid=false;if(!valid||!setMenuTimeout(value.toInt())){web.send(400,"text/plain; charset=utf-8",tr("Enter 0 to 3600 seconds"));return;}web.sendHeader("Location","/");web.send(303);});
 web.on("/language",HTTP_POST,[]{String code=web.arg("language");if(I18n::languageIndex(code.c_str())<0){web.send(400,"text/plain; charset=utf-8",tr("Unknown language"));return;}setLanguage(code);if(portal)drawPortal();web.sendHeader("Location","/");web.send(303);});
 web.on("/save",HTTP_POST,[]{String ssid=web.arg("ssid"), pass=web.arg("pass"), h=web.arg("host");h.trim();bool valid=!ssid.isEmpty()&&ssid.length()<=32&&pass.length()<=64&&!h.isEmpty()&&h.length()<=100;for(size_t i=0;i<h.length();i++)if(!isalnum(h[i])&&h[i]!='.'&&h[i]!='-')valid=false;if(!valid){web.send(400,"text/plain; charset=utf-8",tr("Check settings: IP address or DNS name without http:// or port."));return;}prefs.putString("ssid",ssid);prefs.putString("pass",pass);prefs.putString("host",h);if(h!=host)prefs.remove("serverip");web.send(200,"text/plain; charset=utf-8",tr("Saved. The display will restart and connect to Wi-Fi."));restartPending=true;restartAt=millis()+1500;});
 web.on("/calibrate",HTTP_POST,[]{calibratePending=true;web.send(200,"text/plain; charset=utf-8",tr("Tap display crosses"));});
 web.on("/status",HTTP_GET,[]{JsonDocument d;auto context=d["context_actions"].to<JsonArray>();if(screen==CONTEXT)for(int n=0;n<contextCount;n++)context.add(tr(contextChoices[n].label));d["firmware"]="0.13.1-overlay-test";d["menu_overlay"]=menuOverlay();d["overlay"]=screen==VOLUME_OVERLAY?"volume":screen==WIFI_OVERLAY?"wifi":screen==PLAYER_OVERLAY?"players":screen==BRIGHTNESS_OVERLAY?"brightness":screen==NIGHTLEVEL?"night_brightness":menuOverlay()?"menu":"";d["volume"]=volume;d["display_brightness"]=displayBrightness;d["effective_brightness"]=effectiveBrightness;d["night_enabled"]=nightEnabled;d["night_start"]=wallMinute(nightStart);if(screen==NIGHTTIME)d["night_time_draft"]=wallMinute(nightDraft);d["night_end"]=wallMinute(nightEnd);d["night_brightness"]=nightBrightness;d["night_active"]=nightActive;auto players=d["overlay_players"].to<JsonArray>();if(screen==PLAYER_OVERLAY)for(auto &entry:overlayPlayers)players.add(entry.name);d["clock_active"]=screen==CLOCK;d["player_mode"]=mode;d["player_power"]=playerPower;d["clock_synced"]=clockSynced;d["clock_epoch"]=int64_t(time(nullptr));char wallTime[32]="";time_t epoch=time(nullptr);if(epoch>1700000000){tm local;localtime_r(&epoch,&local);strftime(wallTime,sizeof wallTime,"%d.%m.%Y %H:%M:%S",&local);}d["clock_local"]=wallTime;d["menu_timeout_seconds"]=menuIdle.seconds;d["view"]=int(screen);d["ui_language"]=uiLanguage;d["is_stream"]=isStream;d["station"]=stationName;d["playlist_position"]=track;d["playlist_tracks"]=playlistTracks;d["position_label"]=positionLabel();d["menu_title"]=heading();d["menu_count"]=total;d["menu_player"]=menuOwner;auto menu=d["menu_items"].to<JsonArray>();if(screen==HOME||screen==BROWSE)for(int i=0;i<itemCount;i++)menu.add(items[i].name);d["full_redraws"]=fullRedraws;d["field_redraws"]=fieldRedraws;d["cover_redraws"]=coverRedraws;d["touch_events"]=touchEvents;d["touch_x"]=lastTouchX;d["touch_y"]=lastTouchY;d["cover_bytes"]=coverLen;d["error"]=errorText;d["wifi"]=WiFi.status()==WL_CONNECTED;d["ip"]=WiFi.localIP().toString();d["server"]=host;d["server_ip"]=serverIP;d["dns_primary"]=WiFi.dnsIP(0).toString();d["dns_secondary"]=WiFi.dnsIP(1).toString();d["player"]=playerName;d["title"]=title;d["free_heap"]=ESP.getFreeHeap();String s;serializeJson(d,s);web.send(200,"application/json",s);});web.onNotFound([]{web.sendHeader("Location","/");web.send(302);});web.begin();
}
void setup(){
 Serial.begin(115200);pinMode(0,INPUT_PULLUP);pinMode(26,OUTPUT);digitalWrite(26,LOW);for(int p:{4,16,17}){pinMode(p,OUTPUT);digitalWrite(p,HIGH);}prefs.begin("lmsremote",false);menuIdle.seconds=min(3600u,prefs.getUInt("menuIdle",10));uiLanguage=prefs.isKey("language")?prefs.getString("language"):"de";if(I18n::languageIndex(uiLanguage.c_str())<0)uiLanguage="de";if(!prefs.getBool("e02menuset",false)){prefs.putString("mh0025310229e7",R"(["top:/ra","top:/apps","top:/extra"])");prefs.putBool("e02menuset",true);}host=prefs.getString("host","lms.lan");serverIP=prefs.getString("serverip","");player=prefs.getString("player","");if(!prefs.getBool("e02preset",false)){player="00:25:31:02:29:e7";prefs.putString("player",player);prefs.putBool("e02preset",true);}
 setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();lcd.init();lcd.setRotation(1);displayBrightness=constrain(int(prefs.getUChar("brightness",71)),5,100);savedBrightness=displayBrightness;nightEnabled=prefs.getBool("nightOn",true);nightStart=min(1439,int(prefs.getUShort("nightStart",1320)));nightEnd=min(1439,int(prefs.getUShort("nightEnd",420)));nightBrightness=constrain(int(prefs.getUChar("nightLevel",10)),5,100);lcd.setBrightness((displayBrightness*255+50)/100);fieldBuffer.setColorDepth(16);if(!fieldBuffer.createSprite(320,24)){Serial.println("Row buffer allocation failed");ESP.restart();}uint16_t cal[8];if(prefs.getBytesLength("touch")==sizeof(cal)){prefs.getBytes("touch",cal,sizeof(cal));lcd.setTouchCalibrate(cal);}
 lcd.fillScreen(BG);text(tr("LMS Remote startet ..."),10,90,300,ACCENT);Serial.printf("LMS Remote 0.13.1-overlay-test heap=%u psram=%u\n",ESP.getFreeHeap(),ESP.getPsramSize());
 WiFi.mode(WIFI_STA);WiFi.setHostname("lms-remote");String ssid=prefs.getString("ssid","");if(!ssid.isEmpty())WiFi.begin(ssid.c_str(),prefs.getString("pass","").c_str());else WiFi.begin();
 WiFi.setAutoReconnect(true);uint32_t until=millis()+(ssid.isEmpty()?3000:30000);while(WiFi.status()!=WL_CONNECTED&&millis()<until)delay(100);
 setupWeb();if(WiFi.status()!=WL_CONNECTED){startPortal(!ssid.isEmpty());return;}Serial.printf("WIFI OK IP %s\n",WiFi.localIP().toString().c_str());applyDNSOverride(prefs.getString("dnsOverride",""));bool mdnsOK=MDNS.begin("lms-remote");if(mdnsOK)MDNS.addService("http","tcp",80);Serial.printf("MDNS responder lms-remote.local started=%d DNS primary=%s secondary=%s\n",mdnsOK,WiFi.dnsIP(0).toString().c_str(),WiFi.dnsIP(1).toString().c_str());IPAddress addr;bool resolved=WiFi.hostByName(host.c_str(),addr);if(resolved){serverIP=addr.toString();prefs.putString("serverip",serverIP);}Serial.printf("LMS endpoint %s\n",base().c_str());Serial.printf("LMS DNS %s -> %s resolved=%d gateway=%s\n",host.c_str(),addr.toString().c_str(),resolved,WiFi.gatewayIP().toString().c_str());Serial.printf("FONT umlaut widths: %d %d %d\n",lcd.textWidth("ä"),lcd.textWidth("ö"),lcd.textWidth("ü"));
 refreshClock();if(player.isEmpty()){screen=PLAYERS;loadPage();}else{screen=NOW;pollStatus();}noteActivity();draw();
}
void loop(){
 web.handleClient();if(portal)dns.processNextRequest();
 if(portalAutomatic){
  if(WiFi.status()==WL_CONNECTED){dns.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);portal=false;portalAutomatic=false;invalidateDisplay=true;screen=player.isEmpty()?PLAYERS:NOW;if(screen==PLAYERS)loadPage();else pollStatus();dirty=true;Serial.printf("WIFI recovered IP %s\n",WiFi.localIP().toString().c_str());}
  else if(millis()-lastReconnect>15000){lastReconnect=millis();WiFi.reconnect();}
 }if(restartPending&&int32_t(millis()-restartAt)>=0)ESP.restart();
 uint16_t x=0,y=0;bool down=lcd.getTouch(&x,&y);
 if(screen==CLOCK&&down){wakeClock();blockTouchUntilRelease=true;}
 if(down)noteActivity();
 if(blockTouchUntilRelease){if(!down){blockTouchUntilRelease=false;touchGesture.reset();}}
 else if(!portal&&screen==NIGHTTIME&&(timeStepActive||(down&&UiLayout::insideMenuPanel(x,y)&&nightStepAt(UiLayout::menuX(x),UiLayout::menuY(y))))){
  if(down){uint32_t now=millis();int step=UiLayout::insideMenuPanel(x,y)?nightStepAt(UiLayout::menuX(x),UiLayout::menuY(y)):0;
   if(!timeStepActive){timeStepActive=true;heldTimeStep=step;timeStepStarted=timeStepLast=now;stepNightTime(step);}
   else if(step==heldTimeStep&&uint32_t(now-timeStepStarted)>=450&&uint32_t(now-timeStepLast)>=120){stepNightTime(step);timeStepLast=now;}
   timeStepSeen=now;
  }else if(uint32_t(millis()-timeStepSeen)>=50){timeStepActive=false;touchGesture.reset();}
 }
 else if(!portal&&(screen==VOLUME_OVERLAY||screen==BRIGHTNESS_OVERLAY||screen==NIGHTLEVEL)&&(sliderDragging||(down&&x>=125&&x<=195&&y>=70&&y<=211))){
  if(down){sliderDragging=true;sliderSeen=millis();int value=UiLayout::volumeAt(y);if(screen==NIGHTLEVEL){if(max(5,value)!=nightBrightness)setNightBrightness(value);}else if(screen==BRIGHTNESS_OVERLAY){if(max(5,value)!=displayBrightness)setDisplayBrightness(value);}else{if(value!=volume){volume=value;dirty=true;}sendOverlayVolume(false);}}
  else if(uint32_t(millis()-sliderSeen)>=50){if(screen==NIGHTLEVEL)saveNightSettings();else if(screen==BRIGHTNESS_OVERLAY)saveBrightness();else sendOverlayVolume(true);sliderDragging=false;touchGesture.reset();}
 }
 else{int menuY=menuOverlay()?UiLayout::menuY(y):int(y);int row=(menuY-32)/36;bool longAllowed=(screen==HOME||screen==BROWSE)&&menuY>=32&&menuY<176&&(!menuOverlay()||UiLayout::insideMenuPanel(x,y))&&row>=0&&row<itemCount&&(!items[row].playAction.isEmpty()||!items[row].nextAction.isEmpty()||!items[row].addAction.isEmpty());
  auto event=touchGesture.update(down,x,y,millis(),longAllowed);
  if(!portal&&event!=TouchGesture::None){++touchEvents;lastTouchX=touchGesture.x;lastTouchY=touchGesture.y;Serial.printf("TOUCH %s x=%d y=%d\n",event==TouchGesture::Hold?"hold":"tap",lastTouchX,lastTouchY);if(event==TouchGesture::Hold)openContext(((menuOverlay()?UiLayout::menuY(lastTouchY):lastTouchY)-32)/36);else touch(lastTouchX,lastTouchY);noteActivity();}
 }
 static uint32_t bootAt=0;static bool longBoot=false;
 if(!digitalRead(0)){if(!bootAt){bootAt=millis();longBoot=false;}if(!longBoot&&millis()-bootAt>2500){startPortal();longBoot=true;}}
 else if(bootAt){if(!longBoot&&!portal)calibratePending=true;bootAt=0;}
 static String serialLine;while(Serial.available()){char c=Serial.read();if(c=='\n'){serialLine.trim();if(serialLine.startsWith("uitap ")){int separator=serialLine.indexOf(' ',6);if(separator>0){touch(serialLine.substring(6,separator).toInt(),serialLine.substring(separator+1).toInt());noteActivity();}}if(serialLine=="overlay volume")openOverlay(VOLUME_OVERLAY);if(serialLine=="overlay wifi")openOverlay(WIFI_OVERLAY);if(serialLine=="overlay players")openOverlay(PLAYER_OVERLAY);if(serialLine=="overlay close"&&overlayActive())closeOverlay();if(serialLine=="calibrate")calibratePending=true;if(serialLine=="dns")dnsDiagnostics();if(serialLine=="menu"){rememberPage();screen=HOME;offset=0;loadPage();}if(serialLine.startsWith("menuopen ")){int i=serialLine.substring(9).toInt();selectItem(i);}if(serialLine=="menuback")back();if(serialLine=="standby")requestStandby();if(serialLine=="wake"&&screen==CLOCK)wakeClock();if(serialLine=="settings")go(SETTINGS);if(serialLine=="timeoutedit"){timeoutInput=String(menuIdle.seconds);go(IDLESETUP);}if(serialLine.startsWith("menulong "))openContext(serialLine.substring(9).toInt());if(serialLine=="menunext"&&(screen==HOME||screen==BROWSE)&&offset+4<total){offset+=4;loadPage();}if(serialLine=="now"){screen=NOW;depth=0;pollStatus();dirty=true;}if(serialLine.startsWith("dnsserver ")){String ip=serialLine.substring(10);IPAddress parsed;if(parsed.fromString(ip)){prefs.putString("dnsOverride",ip);applyDNSOverride(ip);lastPoll=0;}}if(serialLine.startsWith("language "))setLanguage(serialLine.substring(9));if(serialLine=="status"){Serial.printf("DISPLAY language=%s stream=%d station=%s position=%s title=%s artist=%s\n",uiLanguage.c_str(),isStream,stationName.c_str(),positionLabel().c_str(),title.c_str(),artist.c_str());Serial.printf("DIAG IP=%s player=%s mode=%s cover=%u touches=%u IRQ=%d error=%s redraw_full=%u fields=%u cover_draws=%u calibration=%u\n",WiFi.localIP().toString().c_str(),playerName.c_str(),mode.c_str(),unsigned(coverLen),unsigned(touchEvents),digitalRead(36),errorText.c_str(),unsigned(fullRedraws),unsigned(fieldRedraws),unsigned(coverRedraws),unsigned(prefs.getBytesLength("touch")));}serialLine="";}else if(serialLine.length()<80)serialLine+=c;}
 if(calibratePending&&!portal){calibratePending=false;saveTouch();touchGesture.reset();blockTouchUntilRelease=true;noteActivity();}
 if(!portal&&menuIdle.expired(millis(),screen!=NOW&&screen!=CLOCK)){if(overlayActive())closeOverlay();screen=NOW;depth=0;contextCount=0;touchGesture.reset();lastPoll=0;dirty=true;Serial.println("MENU idle timeout: now playing");}
 if(!portal&&screen!=CLOCK&&!overlayActive()&&clockIdle.due(millis())){screen=CLOCK;depth=0;touchGesture.reset();invalidateDisplay=true;dirty=true;Serial.println("CLOCK idle: flipclock");}
 if(!portal){refreshClock();applyDisplayBrightness();if(WiFi.status()!=WL_CONNECTED){if(millis()-lastReconnect>15000){lastReconnect=millis();WiFi.reconnect();errorText="Wi-Fi disconnected";dirty=true;}}else if(!player.isEmpty()&&millis()-lastPoll>2000){lastPoll=millis();pollStatus();}if(dirty)draw();if(screen==CLOCK)flipClock.draw(millis(),time(nullptr));}delay(8);
}
