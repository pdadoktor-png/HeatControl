#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <esp32_smartdisplay.h>
#include <lvgl.h>

// HeatControl v0.2.3: reine Simulation, KEINE GPIO-Brenneransteuerung.
WebServer server(80);
Preferences prefs;
float outsideTemp=5, roomTemp=20, boilerTemp=42;
float slope=1.2f, targetRoom=21, hysteresis=5;
bool automatic=true, burnerDemand=false;
unsigned long lastTick=0;

float desiredBoiler() {
  // Vorlaeufige, begrenzte Test-Heizkurve; spaeter kalibrieren.
  float setpoint = targetRoom + slope * (targetRoom-outsideTemp) + 2.0f*(targetRoom-roomTemp);
  return constrain(setpoint, 25.0f, 75.0f);
}
String json() {
  String s="{\"outside\":"+String(outsideTemp,1)+",\"room\":"+String(roomTemp,1)+
    ",\"boiler\":"+String(boilerTemp,1)+",\"setpoint\":"+String(desiredBoiler(),1)+
    ",\"slope\":"+String(slope,2)+",\"targetRoom\":"+String(targetRoom,1)+
    ",\"automatic\":"+String(automatic?"true":"false")+
    ",\"burnerDemand\":"+String(burnerDemand?"true":"false")+"}";
  return s;
}
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HeatControl</title><style>body{font:17px system-ui;background:#111b26;color:#e7eef7;max-width:650px;margin:24px auto;padding:0 15px}section{background:#203247;border-radius:12px;padding:16px;margin:12px 0}input{width:95%}button{padding:12px;background:#e7a94c;border:0;border-radius:8px;font-size:16px}</style><h1>HeatControl <small>v0.2.3</small></h1><p>SIMULATION – keine Brenneransteuerung</p><section id="state">Lade...</section><section><h3>Temperaturen simulieren</h3><label>Aussen <input id="outside" type="number" step="0.5"></label><label>Raum <input id="room" type="number" step="0.5"></label><label>Kessel <input id="boiler" type="number" step="0.5"></label><h3>Regelung</h3><label>Steilheit <input id="slope" type="number" min="0.2" max="3" step="0.1"></label><label>Soll-Raumtemperatur <input id="targetRoom" type="number" min="10" max="28" step="0.5"></label><label><input id="automatic" type="checkbox" style="width:auto"> Automatik</label><p><button onclick="save()">Werte uebernehmen</button></p></section><script>let init=false;async function load(){let j=await(await fetch('/api/status')).json();document.getElementById('state').innerHTML=`Aussen: ${j.outside} °C<br>Raum: ${j.room} °C<br>Kessel: ${j.boiler} °C<br>Heizkurve Soll: ${j.setpoint} °C<br>Brenneranforderung (nur virtuell): <b>${j.burnerDemand?'EIN':'AUS'}</b>`;if(!init){for(let k of ['outside','room','boiler','slope','targetRoom'])document.getElementById(k).value=j[k];document.getElementById('automatic').checked=j.automatic;init=true}}async function save(){let p=new URLSearchParams();for(let k of ['outside','room','boiler','slope','targetRoom'])p.set(k,document.getElementById(k).value);p.set('automatic',document.getElementById('automatic').checked?'1':'0');await fetch('/api/config',{method:'POST',body:p});await load()}setInterval(load,1500);load();</script></html>)HTML";

// 480x272-Touchoberflaeche, bewusst ohne GPIO-Ausgaenge.
static lv_obj_t *statusLabel=nullptr, *tempLabel=nullptr, *curveLabel=nullptr;
static lv_obj_t *modeLabel=nullptr;
static void adjustOutside(lv_event_t *e){
  intptr_t d=(intptr_t)lv_event_get_user_data(e);
  outsideTemp=constrain(outsideTemp+(float)d,-30.0f,45.0f);
}
static void adjustBoiler(lv_event_t *e){
  intptr_t d=(intptr_t)lv_event_get_user_data(e);
  boilerTemp=constrain(boilerTemp+(float)d,0.0f,110.0f);
}
static void toggleMode(lv_event_t *){automatic=!automatic; if(!automatic)burnerDemand=false;}
static lv_obj_t* makeLabel(lv_obj_t *parent,const char *value,int x,int y){
  lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,value);lv_obj_set_pos(o,x,y);return o;
}
static void makeButton(lv_obj_t *parent,const char *label,int x,int y,int w,
                       lv_event_cb_t callback,intptr_t arg){
  lv_obj_t *b=lv_btn_create(parent);lv_obj_set_size(b,w,39);lv_obj_set_pos(b,x,y);
  lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,(void*)arg);
  lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,label);lv_obj_center(l);
}
static void initDisplay(){
  Serial.println("DISPLAY: smartdisplay_init begin");
  smartdisplay_init();
  Serial.println("DISPLAY: smartdisplay_init done");
  smartdisplay_lcd_set_backlight(1.0f);
  Serial.println("DISPLAY: backlight 100 percent");
  lv_obj_t *root=lv_scr_act();
  lv_obj_set_style_bg_color(root,lv_color_hex(0x142434),0);
  lv_obj_set_style_text_color(root,lv_color_hex(0xFFFFFF),0);
  makeLabel(root,"HeatControl  |  SIMULATION",12,9);
  tempLabel=makeLabel(root,"",12,48);
  curveLabel=makeLabel(root,"",12,86);
  statusLabel=makeLabel(root,"",12,124);
  modeLabel=makeLabel(root,"",12,160);
  makeButton(root,"Aussen -",12,211,104,adjustOutside,-1);
  makeButton(root,"Aussen +",124,211,104,adjustOutside,1);
  makeButton(root,"Kessel -",236,211,104,adjustBoiler,-1);
  makeButton(root,"Kessel +",348,211,104,adjustBoiler,1);
  makeButton(root,"Modus",348,155,104,toggleMode,0);
  Serial.println("DISPLAY: LVGL objects created");
}
static void refreshDisplay(){
  if (!tempLabel) return;
  char buffer[128];
  snprintf(buffer,sizeof(buffer),"Aussen: %.1f C   Raum: %.1f C   Kessel: %.1f C",outsideTemp,roomTemp,boilerTemp);
  lv_label_set_text(tempLabel,buffer);
  snprintf(buffer,sizeof(buffer),"Heizkurve: %.1f C   Steilheit: %.1f",desiredBoiler(),slope);
  lv_label_set_text(curveLabel,buffer);
  snprintf(buffer,sizeof(buffer),"Brenner (virtuell): %s",burnerDemand?"EIN":"AUS");
  lv_label_set_text(statusLabel,buffer);
  snprintf(buffer,sizeof(buffer),"Betriebsart: %s",automatic?"AUTOMATIK":"HAND / AUS");
  lv_label_set_text(modeLabel,buffer);
}

void setup(){
 Serial.begin(115200); delay(300); Serial.println("HeatControl v0.2.3 – SIMULATION");
 prefs.begin("heatcontrol",false);
 slope=prefs.getFloat("slope",1.2f); targetRoom=prefs.getFloat("roomset",21);
 WiFi.mode(WIFI_AP_STA); WiFi.softAP("HeatControl-Test");
 Serial.print("Test-WLAN HeatControl-Test, Browser: http://"); Serial.println(WiFi.softAPIP());
 server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PAGE);});
 server.on("/api/status",HTTP_GET,[]{server.send(200,"application/json",json());});
 server.on("/api/config",HTTP_POST,[]{
   if(server.hasArg("outside"))outsideTemp=constrain(server.arg("outside").toFloat(),-30.f,45.f);
   if(server.hasArg("room"))roomTemp=constrain(server.arg("room").toFloat(),0.f,45.f);
   if(server.hasArg("boiler"))boilerTemp=constrain(server.arg("boiler").toFloat(),0.f,110.f);
   if(server.hasArg("slope"))slope=constrain(server.arg("slope").toFloat(),0.2f,3.f);
   if(server.hasArg("targetRoom"))targetRoom=constrain(server.arg("targetRoom").toFloat(),10.f,28.f);
   automatic=server.arg("automatic")=="1";
   prefs.putFloat("slope",slope);prefs.putFloat("roomset",targetRoom);
   server.send(200,"application/json",json());
 });
 server.begin();
 initDisplay();
 refreshDisplay();
 Serial.println("DISPLAY: setup complete");
}
void loop(){
 server.handleClient();
 static uint32_t prevTick=millis();
 uint32_t now=millis();
 lv_tick_inc(now-prevTick);prevTick=now;
 lv_timer_handler();
 if(millis()-lastTick>=1000){lastTick=millis();float set=desiredBoiler();
   if(automatic){if(boilerTemp<set-hysteresis/2)burnerDemand=true;else if(boilerTemp>set+hysteresis/2)burnerDemand=false;}
   else burnerDemand=false; // Handbetrieb erst nach Sicherheitskonzept
   refreshDisplay();
   Serial.printf("Aussen %.1f Raum %.1f Kessel %.1f Soll %.1f Brenner(sim) %s\n",outsideTemp,roomTemp,boilerTemp,set,burnerDemand?"EIN":"AUS");
 }
}
