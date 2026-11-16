#include <cassert>
#include <fstream>
#include <iostream>
#include "menu_model.h"
JsonDocument fixture(int n){JsonDocument d;std::ifstream f("tests/fixtures/menu-response-"+std::to_string(n)+".json");assert(!deserializeJson(d,f));return d;}
JsonDocument actionFor(JsonVariantConst i,JsonVariantConst b=JsonVariantConst()){JsonDocument a;auto raw=MenuModel::action(i,b);assert(!deserializeJson(a,raw));return a;}
int main(){
 auto folders=fixture(0);auto r=folders["result"].as<JsonVariantConst>();auto a=actionFor(r["item_loop"][0],r["base"]);
 assert(a["mode"]=="browse");assert(a["params"]["mode"]=="bmf");assert(a["params"]["item_id"]=="0");assert(a["cmd"][1]=="items");
 auto albums=fixture(1);r=albums["result"];a=actionFor(r["item_loop"][0],r["base"]);assert(a["mode"]=="browse");assert(a["params"]["mode"]=="tracks");assert(a["params"]["album_id"]=="8800");
 auto favorites=fixture(2);r=favorites["result"];a=actionFor(r["item_loop"][1],r["base"]);assert(a["mode"]=="play");assert(a["cmd"][2]=="play");assert(a["params"]["item_id"]=="0ead3215.1");assert(a["params"]["xmlbrowserPlayControl"].isNull());
 auto radios=fixture(3);r=radios["result"];a=actionFor(r["item_loop"][0],r["base"]);assert(a["cmd"][0]=="presets");assert(a["params"]["menu"]=="presets");
 JsonDocument i;deserializeJson(i,R"({"isANode":1,"id":"myMusic"})");a=actionFor(i.as<JsonVariantConst>());assert(a["node"]=="myMusic");
 deserializeJson(i,R"({"command":["browselibrary","items"],"params":["mode:bmf","menu:1"]})");a=actionFor(i.as<JsonVariantConst>());assert(a["params"]["mode"]=="bmf");
 deserializeJson(i,R"({"url":"http://radio.example/stream","isPinned":true})");a=actionFor(i.as<JsonVariantConst>());assert(a["mode"]=="play");assert(a["cmd"][2]=="http://radio.example/stream");
 deserializeJson(i,R"({"hidden":true})");assert(!MenuModel::visible(i.as<JsonVariantConst>()));
 deserializeJson(i,R"({"visible":0})");assert(!MenuModel::visible(i.as<JsonVariantConst>()));
 deserializeJson(i,R"({"enabled":false})");assert(!MenuModel::visible(i.as<JsonVariantConst>()));
 deserializeJson(i,R"({"actions":{"go":null}})");assert(MenuModel::action(i.as<JsonVariantConst>(),folders["result"]["base"]).empty());
 deserializeJson(i,R"({"title":"Web settings","url":"settings.html"})");a=actionFor(i.as<JsonVariantConst>());assert(a["mode"]=="web");
 deserializeJson(i,R"({"actions":{"go":{"cmd":["sounds","items"],"params":{"menu":"sounds"}}}})");a=actionFor(i.as<JsonVariantConst>(),radios["result"]["base"]);assert(a["cmd"][0]=="sounds");assert(a["params"]["menu"]=="sounds");
 JsonDocument hidden;deserializeJson(hidden,R"(["top:/ra","top:/apps","top:/extra"])");
 deserializeJson(i,R"({"id":"top:/ra"})");assert(!MenuModel::visible(i.as<JsonVariantConst>(),hidden.as<JsonArrayConst>()));assert(MenuModel::visible(i.as<JsonVariantConst>()));
 deserializeJson(i,R"({"id":"pinned:Music Folder"})");assert(MenuModel::visible(i.as<JsonVariantConst>(),hidden.as<JsonArrayConst>()));
 std::cout<<"15 menu protocol checks passed\n";
}
