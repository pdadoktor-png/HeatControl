#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class RoomSource {
 public:
  enum Source : uint8_t { NTC=0, WLAN=1 };
  void begin(Preferences& prefs) {
    store=&prefs;
    source=store->getUChar("roomSource",0)==1?WLAN:NTC;
    ip=store->getString("roomIP","192.168.0.26");
    if(!validIP(ip)) ip="192.168.0.26";
  }
  bool configure(int nextSource,const String& nextIP) {
    if(nextSource!=0 && nextSource!=1) return false;
    if(!validIP(nextIP)) return false;
    source=(Source)nextSource; ip=nextIP;
    store->putUChar("roomSource",source);
    store->putString("roomIP",ip);
    valid=false; lastSuccess=0; lastPoll=0;
    return true;
  }
  void poll() {
    if(source!=WLAN) return;
    const uint32_t now=millis();
    if(lastPoll!=0 && (uint32_t)(now-lastPoll)<10000) return;
    lastPoll=now?now:1;
    if(WiFi.status()!=WL_CONNECTED) { valid=false; return; }
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(1200);
    if(!http.begin(client,String("http://")+ip+"/api/temperature")) { valid=false; return; }
    int code=http.GET();
    if(code==200) {
      JsonDocument doc;
      DeserializationError err=deserializeJson(doc,http.getStream());
      if(!err && doc["valid"].is<bool>() && doc["valid"].as<bool>()
          && doc["temperature"].is<JsonFloat>()
          && doc["sample_age_ms"].is<uint32_t>()) {
        float t=doc["temperature"].as<float>();
        uint32_t age=doc["sample_age_ms"].as<uint32_t>();
        if(isfinite(t) && t>=-20 && t<=50 && age<=120000) {
          temperature=t; lastSuccess=millis(); valid=true;
        } else valid=false;
      } else valid=false;
    } else valid=false;
    http.end();
  }
  bool available() const { return valid && lastSuccess!=0 && (uint32_t)(millis()-lastSuccess)<=120000; }
  float selected(float ntc) const {
    if(source==NTC) return ntc;
    return available()?temperature:NAN; // Never silently substitute another sensor.
  }
  String json() const {
    String s="{\"source\":\""+String(source==NTC?"ntc":"wlan")+"\",\"ip\":\""+ip+
      "\",\"wlanValid\":"+(available()?"true":"false")+
      ",\"wlanTemperature\":"+(available()?String(temperature,2):String("null"))+
      ",\"ageMs\":"+(lastSuccess?String((uint32_t)(millis()-lastSuccess)):String("null"))+"}";
    return s;
  }
  Source source=NTC;
  String ip="192.168.0.26";
 private:
  Preferences* store=nullptr;
  float temperature=NAN;
  bool valid=false;
  uint32_t lastSuccess=0,lastPoll=0;
  static bool validIP(const String& s) {
    IPAddress addr;
    return addr.fromString(s) && addr!=IPAddress(0,0,0,0) && addr!=IPAddress(255,255,255,255);
  }
};
