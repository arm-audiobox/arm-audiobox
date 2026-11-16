#include <cassert>
#include <cstring>
#include <iostream>
#include <ArduinoJson.h>
#include "playback_model.h"
#include "i18n_generated.h"
int main(){
 JsonDocument r;deserializeJson(r,R"({"remote":1,"playlist_cur_index":"0","playlist_tracks":1,"current_title":"80s80s NRW","playlist_loop":[{"title":"Just Like Heaven","artist":"The Cure"}]})");
 auto p=PlaybackModel::position(r.as<JsonVariantConst>());assert(p.stream&&p.station=="80s80s NRW");assert(r["playlist_loop"][0]["title"]=="Just Like Heaven");
 deserializeJson(r,R"({"playlist_cur_index":"10","playlist_tracks":16,"duration":237.533,"playlist_loop":[{"title":"L'Hôtel","artist":"Yello"}]})");p=PlaybackModel::position(r.as<JsonVariantConst>());assert(!p.stream&&p.current==11&&p.total==16&&p.station.empty());
 deserializeJson(r,R"({"remote":1,"playlist_cur_index":2,"playlist_tracks":12,"duration":180,"can_seek":1,"current_title":"On-demand track"})");p=PlaybackModel::position(r.as<JsonVariantConst>());assert(!p.stream&&p.current==3&&p.total==12);
 deserializeJson(r,R"({"remote":1,"playlist_tracks":1,"playlist_loop":[{"station_name":"WDR 2","title":"Musiktitel"}]})");p=PlaybackModel::position(r.as<JsonVariantConst>());assert(p.stream&&p.station=="WDR 2");
 deserializeJson(r,R"({"playlist_tracks":0})");p=PlaybackModel::position(r.as<JsonVariantConst>());assert(p.current==0&&p.total==0&&!p.stream);
 deserializeJson(r,R"({"remote":1,"duration":220,"can_seek":0,"current_title":"WDR 2","playlist_tracks":1})");assert(PlaybackModel::position(r.as<JsonVariantConst>()).stream);
 assert(std::strcmp(I18n::lookup("de","Back"),"Zurück")==0);assert(std::strcmp(I18n::lookup("en","Zurück"),"Back")==0);
 assert(std::strcmp(I18n::lookup("en","Meine Musik"),"My Music")==0);assert(std::strcmp(I18n::lookup("de","Track {current} of {total}"),"Track {current} von {total}")==0);
 assert(I18n::lookup("de","The Cure")==nullptr);assert(I18n::lookup("en","80s80s NRW")==nullptr);assert(I18n::languageIndex("invalid")==-1);
 std::cout<<"13 playback and localization checks passed\n";
}
