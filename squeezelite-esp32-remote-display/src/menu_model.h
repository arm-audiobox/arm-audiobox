#pragma once
#include <ArduinoJson.h>
#include <string>
namespace MenuModel {
inline void merge(JsonObject out,JsonObjectConst in){for(JsonPairConst p:in)out[p.key()]=p.value();}
inline std::string str(JsonVariantConst v){return v.isNull()?std::string():v.as<std::string>();}
inline bool visible(JsonVariantConst i,JsonArrayConst hidden=JsonArrayConst()){if(i["hidden"].as<bool>()||(!i["visible"].isNull()&&!i["visible"].as<bool>())||(!i["enabled"].isNull()&&!i["enabled"].as<bool>()))return false;for(JsonVariantConst id:hidden)if(str(id)==str(i["id"]))return false;return true;}
inline std::string encode(JsonDocument &d){std::string out;serializeJson(d,out);return out;}
inline std::string action(JsonVariantConst item,JsonVariantConst base=JsonVariantConst(),const std::string &requested=""){
 JsonDocument d;
 if(requested.empty()&&item["isANode"].as<bool>()){d["node"]=item["id"];d["mode"]="browse";return encode(d);}
 if(item["url"].is<const char*>()){
  bool playable=item["isPinned"].as<bool>()||str(item["type"])=="audio"||str(item["type"])=="playlist";
  if(!requested.empty()&&!playable)return "";
  auto a=d["cmd"].to<JsonArray>();a.add("playlist");a.add(requested=="add-hold"||requested=="insert"?"insert":requested=="add"?"add":"play");a.add(item["url"]);
  d["mode"]=!playable?"web":requested.empty()||requested=="play"?"play":"enqueue";return encode(d);
 }
 if(requested.empty()&&item["command"].is<JsonArrayConst>()&&item["command"].size()){
  d["cmd"]=item["command"];auto p=d["params"].to<JsonObject>();
  for(JsonVariantConst v:item["params"].as<JsonArrayConst>()){auto s=str(v);auto k=s.find(':');if(k!=std::string::npos)p[s.substr(0,k)]=s.substr(k+1);}
  d["mode"]="browse";return encode(d);
 }
 std::string kind=requested.empty()?((str(item["type"])=="audio"||str(item["style"])=="itemplay")?"play":"go"):requested;
 JsonVariantConst own=item["actions"][kind], inherited=base["actions"][kind];
 if(own.isNull()&&!item["actions"][kind].isUnbound())inherited=JsonVariantConst();
 if(requested.empty()&&own.isNull()&&inherited.isNull()){
  kind="do";own=item["actions"][kind];inherited=base["actions"][kind];
  if(own.isNull()&&inherited.isNull()){kind="go";own=item["actions"][kind];inherited=base["actions"][kind];}
 }
 if(own.isNull()&&!item["actions"][kind].isUnbound())return "";
 JsonVariantConst chosen=own.isNull()?inherited:own;
 if(chosen["choices"].is<JsonArrayConst>()){d["choices"]=chosen["choices"];d["labels"]=item["choiceStrings"];d["mode"]="browse";return encode(d);}
 JsonVariantConst cmd=own["cmd"].isNull()?inherited["cmd"]:own["cmd"];
 if(!cmd.is<JsonArrayConst>()||!cmd.size())return "";
 d["cmd"]=cmd;auto params=d["params"].to<JsonObject>();merge(params,inherited["params"].as<JsonObjectConst>());merge(params,own["params"].as<JsonObjectConst>());
 std::string ip=str(own["itemsParams"]);if(ip.empty())ip=str(inherited["itemsParams"]);if(!ip.empty())merge(params,item[ip].as<JsonObjectConst>());
 std::string next=str(own["nextWindow"]);if(next.empty())next=str(inherited["nextWindow"]);if(next.empty())next=str(item["nextWindow"]);
 d["mode"]=!requested.empty()?(requested=="play"?"play":"enqueue"):(kind=="play"||next=="nowPlaying")?"play":kind=="do"?"do":"browse";
 if(!item["input"].isNull())d["input"]=item["input"];
 return encode(d);
}
}
