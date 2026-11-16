#pragma once
#include <ArduinoJson.h>
#include <string>
namespace PlaybackModel {
struct Position {bool stream=false;int current=0,total=0;std::string station;};
inline std::string str(JsonVariantConst v){return v.isNull()?std::string():v.as<std::string>();}
inline Position position(JsonVariantConst r){
 Position p;p.total=r["playlist_tracks"].as<int>();p.current=p.total>0?r["playlist_cur_index"].as<int>()+1:0;
 if(p.current>p.total)p.current=p.total;
 auto t=r["playlist_loop"][0];p.station=str(r["current_title"]);
 p.stream=r["remote"].as<bool>()&&(r["duration"].as<double>()<=0||(!r["can_seek"].as<bool>()&&!p.station.empty()));
 if(p.stream&&p.station.empty())p.station=str(t["station_name"]);
 if(p.stream&&p.station.empty())p.station=str(t["remote_title"]);
 if(p.stream&&p.station.empty())p.station=str(t["title"]);
 if(!p.stream)p.station.clear();return p;
}
}
