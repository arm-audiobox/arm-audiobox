#include <cassert>
#include <fstream>
#include <iostream>
#include "menu_model.h"
#include "touch_gesture.h"
#include "ui_layout.h"
JsonDocument fixture(int n){JsonDocument d;std::ifstream f("tests/fixtures/menu-response-"+std::to_string(n)+".json");assert(!deserializeJson(d,f));return d;}
JsonDocument actionFor(JsonVariantConst item,JsonVariantConst base,const char* kind){JsonDocument d;assert(!deserializeJson(d,MenuModel::action(item,base,kind)));return d;}
int main(){
 auto folders=fixture(0);auto r=folders["result"].as<JsonVariantConst>();
 auto play=actionFor(r["item_loop"][0],r["base"],"play");assert(play["cmd"][0]=="playlistcontrol"&&play["params"]["folder_id"]=="110134"&&play["params"]["cmd"]=="load"&&play["mode"]=="play");
 auto next=actionFor(r["item_loop"][0],r["base"],"add-hold");assert(next["params"]["folder_id"]=="110134"&&next["params"]["cmd"]=="insert"&&next["mode"]=="enqueue");
 auto add=actionFor(r["item_loop"][0],r["base"],"add");assert(add["params"]["cmd"]=="add"&&add["mode"]=="enqueue");
 auto albums=fixture(1);r=albums["result"];next=actionFor(r["item_loop"][0],r["base"],"add-hold");assert(next["params"]["album_id"]=="8800"&&next["params"]["cmd"]=="insert");
 auto favorites=fixture(2);r=favorites["result"];add=actionFor(r["item_loop"][1],r["base"],"add");assert(add["cmd"][2]=="add"&&add["params"]["item_id"]=="0ead3215.1");
 JsonDocument pin;deserializeJson(pin,R"({"url":"http://radio.example/stream","isPinned":true})");next=actionFor(pin.as<JsonVariantConst>(),JsonVariantConst(),"add-hold");assert(next["cmd"][1]=="insert"&&next["mode"]=="enqueue");
 deserializeJson(pin,R"({"url":"settings.html"})");assert(MenuModel::action(pin.as<JsonVariantConst>(),JsonVariantConst(),"play").empty());
 TouchGesture g;
 assert(g.update(true,10,45,0,true)==TouchGesture::None);assert(g.update(true,10,45,649,true)==TouchGesture::None);assert(g.update(true,10,45,650,true)==TouchGesture::Hold);assert(g.update(true,10,45,700,true)==TouchGesture::None);assert(g.update(false,0,0,751,false)==TouchGesture::None);
 assert(g.update(true,10,45,800,true)==TouchGesture::None);assert(g.update(false,0,0,820,false)==TouchGesture::None);assert(g.update(false,0,0,851,false)==TouchGesture::Tap);
 g.reset();g.update(true,10,45,0,true);g.update(false,0,0,25,false);assert(g.update(true,10,45,40,true)==TouchGesture::None);assert(g.update(true,10,45,650,true)==TouchGesture::Hold);
 g.reset();g.update(true,10,45,0,true);g.update(true,30,45,10,true);assert(g.update(true,30,45,700,true)==TouchGesture::None);assert(g.update(false,0,0,751,false)==TouchGesture::None);
 g.reset();g.update(true,10,45,0,false);assert(g.update(true,10,45,700,false)==TouchGesture::None);assert(g.update(false,0,0,751,false)==TouchGesture::Tap);
 auto measure=[](const std::string& s){return int(s.size()*8);};auto label=UiLayout::splitLabel("Now playing",74,measure);assert(label.wrapped&&label.first=="Now"&&label.second=="playing");assert(measure(label.first)<=74&&measure(label.second)<=74);
 assert(!UiLayout::splitLabel("Aktuell",74,measure).wrapped);assert(!UiLayout::splitLabel("Play next",304,measure).wrapped);
 std::cout<<"15 context-action, gesture and label-layout checks passed\n";
}
