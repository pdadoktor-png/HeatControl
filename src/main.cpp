#include <Arduino.h>
#ifndef HEATCONTROL_GIT_SHA
#define HEATCONTROL_GIT_SHA "unknown"
#endif
#ifndef HEATCONTROL_BUILD_NUMBER
#define HEATCONTROL_BUILD_NUMBER "0"
#endif
#define HEATCONTROL_VERSION "0.5.9.2"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <time.h>
#include <math.h>
#include <esp32_smartdisplay.h>
#include <lvgl.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "RoomSource.h"

// HeatControl v0.5.9.2 | Build 0001: Strukturierung des Quellcodes, unveraenderte Regelungslogik.
// Basis: v0.5.9, Simulation mit Niedervolt-Pruefstand-Automatik.
// OTA nur im Heim-WLAN (STA), nie im offenen Test-Access-Point.
#ifndef HEATCONTROL_OTA_PASSWORD
#define HEATCONTROL_OTA_PASSWORD ""
#endif
static void relayOff();
static bool relayPinsInitialized = false;
// PCF8574 erkannt und initialisiert
static String relayDiagnosticJson();
static bool relayArmed=false;
static bool relayOtaInProgress=false;
static bool otaStarted=false;
static void maintainOTA(){
  if(WiFi.status()!=WL_CONNECTED){
if(otaStarted){ArduinoOTA.end();
    otaStarted=false;
    Serial.println("OTA: WLAN getrennt");
    }
return;
  }
  if(!otaStarted){
    if(strlen(HEATCONTROL_OTA_PASSWORD)<8){
static bool warned=false;
if(!warned){Serial.println("OTA: deaktiviert – Passwort in platformio.ini setzen (mind. 8 Zeichen)");
      warned=true;
      }
return;
    }
ArduinoOTA.setHostname("heatcontrol");
ArduinoOTA.setPassword(HEATCONTROL_OTA_PASSWORD);
ArduinoOTA.onStart([](){relayOtaInProgress=true;relayArmed=false;relayOff();Serial.println("OTA: Firmwaretransfer gestartet");});
ArduinoOTA.onEnd([](){Serial.println("OTA: abgeschlossen, Neustart");});
ArduinoOTA.onError([](ota_error_t error){relayOtaInProgress=false;relayOff();Serial.printf("OTA: Fehler %u\n",(unsigned)error);});
ArduinoOTA.begin();
    otaStarted=true;
Serial.print("OTA: bereit unter ");
    Serial.println(WiFi.localIP());
  }
ArduinoOTA.handle();
}
WebServer server(80);
Preferences prefs;
RoomSource roomSource;
float outsideTemp=NAN, roomTemp=NAN, boilerTemp=NAN;
float slope=1.2f, targetRoom=21, hysteresis=5;
uint16_t minBurnerOnSeconds=30, minBurnerOffSeconds=20;
// Nur Testbetrieb
uint32_t lastBurnerTransitionMs=0;
bool burnerTransitionKnown=false;
float comfortRoom=21.0f, reducedRoom=17.0f, holidayRoom=15.0f;
bool scheduleEnabled=false, holidayEnabled=false;
// 0=Montag .. 6=Sonntag; bis zu 3 Zeitfenster je Tag.
struct Phase {
uint16_t from;
uint16_t to;
bool enabled;
};
Phase phases[7][3];
int selectedDay=0, selectedPhase=0;
int64_t holidayStart=0, holidayEnd=0;
// Unix-Sekunden, Endzeit exklusiv
bool testClockEnabled=false;
int testWeekday=0, testMinute=720;
bool clockValid=false, holidayActive=false, comfortActive=true;
const char *tzRule="CET-1CEST,M3.5.0/2,M10.5.0/3";
bool automatic=true, burnerDemand=false;
bool manualMode=false, manualBurner=false, manualPump=false;
bool pumpDemand=false;
// Vor historySample() deklariert
// 0: Software-Temperatursimulation (Relais strikt gesperrt), 1: reale NTC am ADS1115.
uint8_t inputMode=1;
float virtualOutside=10.0f,virtualRoom=21.0f,virtualBoiler=45.0f;
static constexpr float POT_MAX_R=20000.0f;
bool simulationRunning=true; // Automatischer Start nur am Niedervolt-Pruefstand.
static bool sensorFaultLatched=false; // Hand-Brenner bleibt nach Sensor-/Grenzwertfehler AUS.
static uint8_t previousSensorFaultMask=255; // 255 = erster Messzyklus
uint32_t simCommandSequence=0;
// Simuliertes thermisches Modell, kein realer Brenner.
// Bei Brenner EIN: +0.20 C/s, bei AUS: -0.06 C/s (vereinfachtes Modell).
static constexpr float HEATING_C_PER_S=0.20f;
static constexpr float COOLING_C_PER_S=0.06f;
static constexpr float SIM_STEP_SECONDS=1.0f;
unsigned long lastTick=0;
uint32_t burnerStarts=0, burnerSeconds=0, simulationSeconds=0;
float oilLitersPerHour=2.0f;
// Schaetzwert, keine Messung
// Statistik: 5-Minuten-Samples, 7 Tage im RAM, optional in LittleFS gespeichert.
// Alle Laufzeiten und Verbrauchswerte stammen AUSSCHLIESSLICH aus der Simulation.
static constexpr size_t HISTORY_MAX=2016;
#include "HistorySample.h"
static HistorySample history[HISTORY_MAX];
static size_t historyCount=0,historyHead=0;
static bool historyFS=false;
static uint32_t pumpSeconds=0;
static uint32_t lastHistoryMs=0;
static uint32_t historyWrites=0;
static uint32_t historyWriteErrors=0;
static const char *HISTORY_FILE="/history.csv";
// Ereignislog: simulierte Anforderungen, keine Rueckmeldung vom realen Brenner.
static constexpr size_t EVENT_MAX=500;
struct EventEntry {
uint32_t epoch;
uint32_t uptime;
String kind;
String detail;
};
static EventEntry events[EVENT_MAX];
static size_t eventHead=0,eventCount=0;
static uint32_t eventFileErrors=0;
static String csvSafe(String s){s.replace(";",",");
s.replace("\r"," ");
s.replace("\n"," ");
return s;
}
static void eventLog(const String &kind,const String &detail){
time_t now=time(nullptr);
EventEntry e={(uint32_t)(now>1700000000?now:0),millis()/1000,kind,detail};
events[eventHead]=e;
  eventHead=(eventHead+1)%EVENT_MAX;
if(eventCount<EVENT_MAX)eventCount++;
Serial.printf("EVENT [%s] %s\n",kind.c_str(),detail.c_str());
if(!historyFS)return;
File f=LittleFS.open("/events.csv","a");
if(!f){eventFileErrors++;
  return;
  }
size_t n=f.printf("%lu;%lu;%s;%s\n",(unsigned long)e.epoch,(unsigned long)e.uptime,csvSafe(kind).c_str(),csvSafe(detail).c_str());
size_t size=f.size();
  f.close();
  if(!n)eventFileErrors++;
  if(size>250000){
LittleFS.remove("/events-old.csv");
if(!LittleFS.rename("/events.csv","/events-old.csv"))eventFileErrors++;
  }
}
static void eventLoadFromFlash(){
 if(!historyFS)return;
 for(const char *path : {"/events-old.csv","/events.csv"}){
  File f=LittleFS.open(path,"r");
  if(!f)continue;
  while(f.available()){
   String line=f.readStringUntil('\n');line.trim();
   int a=line.indexOf(';'),b=line.indexOf(';',a+1),c=line.indexOf(';',b+1);
   if(a<0||b<0||c<0)continue;
   EventEntry e={(uint32_t)strtoul(line.substring(0,a).c_str(),nullptr,10),
                 (uint32_t)strtoul(line.substring(a+1,b).c_str(),nullptr,10),
                 line.substring(b+1,c),line.substring(c+1)};
   events[eventHead]=e;eventHead=(eventHead+1)%EVENT_MAX;
   if(eventCount<EVENT_MAX)eventCount++;
  }
  f.close();
 }
}
static void eventSendJson(){
server.sendHeader("Cache-Control","no-store");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"application/json","");
server.sendContent(String("{\"simulationOnly\":true,\"storage\":\"")+(historyFS?"LittleFS":"RAM")+"\",\"writeErrors\":"+String(eventFileErrors)+",\"events\":[");
  for(size_t i=0;i<eventCount;i++){
const EventEntry &e=events[(eventHead+EVENT_MAX-eventCount+i)%EVENT_MAX];
String d=e.detail;
    d.replace("\\","\\\\");
    d.replace("\"","\\\"");
server.sendContent(String(i?",":"")+"{\"t\":"+String(e.epoch)+",\"uptime\":"+String(e.uptime)+",\"type\":\""+e.kind+"\",\"detail\":\""+d+"\"}");
  }
server.sendContent("]}");
  server.sendContent("");
}
static void eventSendCsv(){
server.sendHeader("Content-Disposition","attachment; filename=heatcontrol-events.csv");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/csv; charset=utf-8","");
server.sendContent("epoch;uptime_s;typ;details\n");
  // Alle gespeicherten Ereignisse aus Flash (einschliesslich rotierter Datei).
  if(historyFS){
    for(const char *name : {"/events-old.csv","/events.csv"}){
File f=LittleFS.open(name,"r");
      if(f){while(f.available()){String line=f.readStringUntil('\n');
      server.sendContent(line+"\n");
      }f.close();
      }
    }
  }else{
    for(size_t i=0;i<eventCount;i++){
const EventEntry &e=events[(eventHead+EVENT_MAX-eventCount+i)%EVENT_MAX];
server.sendContent(String(e.epoch)+";"+String(e.uptime)+";"+csvSafe(e.kind)+";"+csvSafe(e.detail)+"\n");
    }
  }
server.sendContent("");
}
static void historyInsert(const HistorySample &r){
history[historyHead]=r;
  historyHead=(historyHead+1)%HISTORY_MAX;
if(historyCount<HISTORY_MAX)historyCount++;
}
static HistorySample historyAt(size_t i){
return history[(historyHead+HISTORY_MAX-historyCount+i)%HISTORY_MAX];
}
static void historyInit(){
historyFS=LittleFS.begin(false);
if(!historyFS){Serial.println("STATISTIK: LittleFS nicht bereit, RAM-Aufzeichnung aktiv");
  return;
  }
File f=LittleFS.open(HISTORY_FILE,"r");
  if(f){
    while(f.available()){
String line=f.readStringUntil('\n');
unsigned long e,u,st,bs,ps;
      int a,r,b,fl;
      if(sscanf(line.c_str(),"%lu,%lu,%d,%d,%d,%d,%lu,%lu,%lu",&e,&u,&a,&r,&b,&fl,&st,&bs,&ps)==9){
HistorySample h={(uint32_t)e,(uint32_t)u,(int16_t)a,(int16_t)r,(int16_t)b,(uint8_t)fl,(uint32_t)st,(uint32_t)bs,(uint32_t)ps};
        historyInsert(h);
      }
    }
f.close();
  }
Serial.printf("STATISTIK: %u Datensaetze geladen\n",(unsigned)historyCount);
}
static void historySample(){
time_t t=time(nullptr);
  HistorySample h={t>1700000000?(uint32_t)t:0,(uint32_t)(millis()/1000),
    (int16_t)lroundf(outsideTemp*10),(int16_t)lroundf(roomTemp*10),(int16_t)lroundf(boilerTemp*10),
(uint8_t)((burnerDemand?1:0)|(pumpDemand?2:0)|(simulationRunning?4:0)),burnerStarts,burnerSeconds,pumpSeconds};
historyInsert(h);
if(!historyFS)return;
File f=LittleFS.open(HISTORY_FILE,"a");
if(!f){historyWriteErrors++;
  return;
  }
size_t n=f.printf("%lu,%lu,%d,%d,%d,%u,%lu,%lu,%lu\n",(unsigned long)h.epoch,(unsigned long)h.uptime,(int)h.outside10,(int)h.room10,(int)h.boiler10,(unsigned)h.flags,(unsigned long)h.starts,(unsigned long)h.burnerSec,(unsigned long)h.pumpSec);
f.close();
  if(n>0)historyWrites++;
  else historyWriteErrors++;
  // Rotation nur selten: bei ca. 180 KB Datei auf die letzten 7 Tage kuerzen.
  if(LittleFS.exists(HISTORY_FILE)){
File check=LittleFS.open(HISTORY_FILE,"r");
    size_t size=check?check.size():0;
    if(check)check.close();
    if(size>180000){
File out=LittleFS.open("/history.tmp","w");
if(out){for(size_t i=0;i<historyCount;i++){HistorySample x=historyAt(i);
      out.printf("%lu,%lu,%d,%d,%d,%u,%lu,%lu,%lu\n",(unsigned long)x.epoch,(unsigned long)x.uptime,(int)x.outside10,(int)x.room10,(int)x.boiler10,(unsigned)x.flags,(unsigned long)x.starts,(unsigned long)x.burnerSec,(unsigned long)x.pumpSec);
      }out.close();
      LittleFS.remove(HISTORY_FILE);
      LittleFS.rename("/history.tmp",HISTORY_FILE);
      }
    }
  }
}
static String historyStatusJson(){
  return String("{\"simulationOnly\":true,\"storage\":\"")+(historyFS?"LittleFS":"RAM")+
    "\",\"samples\":"+String(historyCount)+",\"intervalSeconds\":300,\"burnerStarts\":"+String(burnerStarts)+
    ",\"burnerSeconds\":"+String(burnerSeconds)+",\"pumpSeconds\":"+String(pumpSeconds)+
    ",\"oilRate\":"+String(oilLitersPerHour,2)+",\"estimatedLiters\":"+String(burnerSeconds*oilLitersPerHour/3600.f,3)+
",\"writeErrors\":"+String(historyWriteErrors)+"}";
}
static void historySendJson(){
int hours=server.hasArg("hours")?server.arg("hours").toInt():24;
hours=constrain(hours,1,168);
uint32_t nowEpoch=time(nullptr)>1700000000?(uint32_t)time(nullptr):0;
uint32_t nowUp=millis()/1000;
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200,"application/json","[ ");
bool first=true;
  for(size_t i=0;i<historyCount;i++){
HistorySample h=historyAt(i);
    bool recent=(nowEpoch && h.epoch)?(nowEpoch>=h.epoch && nowEpoch-h.epoch<=(uint32_t)hours*3600):
(nowUp>=h.uptime && nowUp-h.uptime<=(uint32_t)hours*3600);
if(!recent)continue;
    String item=String(first?"":",")+"{\"t\":"+String(h.epoch)+",\"u\":"+String(h.uptime)+
      ",\"a\":"+String(h.outside10/10.f,1)+",\"r\":"+String(h.room10/10.f,1)+
      ",\"b\":"+String(h.boiler10/10.f,1)+",\"burner\":"+String((h.flags&1)?1:0)+
",\"pump\":"+String((h.flags&2)?1:0)+"}";
server.sendContent(item);
    first=false;
  }
server.sendContent("]");
  server.sendContent("");
}
static void historyCsv(){
server.sendHeader("Content-Disposition","attachment; filename=heatcontrol-simulation.csv");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200,"text/csv; charset=utf-8","epoch_utc,uptime_s,aussen_C,raum_C,kessel_C,brenner_sim,pumpe_sim,starts_sim,brennersekunden_sim,pumpensekunden_sim\n");
  for(size_t i=0;i<historyCount;i++){
HistorySample h=historyAt(i);
server.sendContent(String(h.epoch)+","+String(h.uptime)+","+String(h.outside10/10.f,1)+","+String(h.room10/10.f,1)+","+String(h.boiler10/10.f,1)+","+String((h.flags&1)?1:0)+","+String((h.flags&2)?1:0)+","+String(h.starts)+","+String(h.burnerSec)+","+String(h.pumpSec)+"\n");
  }
server.sendContent("");
}

// v0.5.4: Read-only simulation diagnostics; no automatic changes to control settings.
static String optimizationJson(){
  // Query parameter is deliberately restricted to the two supported periods.
const int periodHours=(server.hasArg("hours") && server.arg("hours").toInt()==168)?168:24;
const uint32_t periodSeconds=(uint32_t)periodHours*3600UL;
const uint32_t now=(uint32_t)time(nullptr);
const bool clockOk=now>1700000000UL;
const uint32_t up=millis()/1000UL;
uint32_t first=0,last=0,startsFirst=0,startsLast=0,burnFirst=0,burnLast=0,pumpFirst=0,pumpLast=0;
double sumA=0,sumR=0,sumB=0;
  size_t n=0;
  for(size_t i=0;i<historyCount;i++){
HistorySample h=historyAt(i);
    bool recent=clockOk && h.epoch ? (now>=h.epoch && now-h.epoch<=periodSeconds) :
(up>=h.uptime && up-h.uptime<=periodSeconds);
if(!recent)continue;
    // Counter reset or reboot means rates cannot be compared reliably.
    if(n && (h.starts<startsLast || h.burnerSec<burnLast || h.pumpSec<pumpLast)){
n=0;
      sumA=sumR=sumB=0;
    }
if(!n){first=clockOk && h.epoch?h.epoch:h.uptime;
    startsFirst=h.starts;
    burnFirst=h.burnerSec;
    pumpFirst=h.pumpSec;
    }
last=clockOk && h.epoch?h.epoch:h.uptime;
startsLast=h.starts;
    burnLast=h.burnerSec;
    pumpLast=h.pumpSec;
sumA+=h.outside10/10.0;
    sumR+=h.room10/10.0;
    sumB+=h.boiler10/10.0;
    n++;
  }
uint32_t duration=n>1 && last>=first?last-first:0;
uint32_t starts=n>1?startsLast-startsFirst:0;
uint32_t burner=n>1?burnLast-burnFirst:0;
uint32_t pump=n>1?pumpLast-pumpFirst:0;
float hours=duration/3600.0f;
float duty=duration?100.0f*burner/duration:0.0f;
float startsPerHour=hours>=0.25f?starts/hours:0.0f;
bool sufficient=n>=4 && duration>=900;
String advice="Zu wenig Messdaten: mindestens vier Messpunkte und 15 Minuten benoetigt.";
  if(sufficient){
if(startsPerHour>6.0f)advice="Mehr als sechs Brenneranforderungen pro Stunde: Hysterese und Mindestlaufzeit pruefen. Vorschlag: Hysterese um 0,5 Grad erhoehen und Verlauf erneut vergleichen. Keine automatische Aenderung.";
else if(duty>90.0f)advice="Brenneranforderung ueber 90 Prozent der Zeit: Kesselsollwert, Heizkurve und Sensorwerte pruefen. Bei zu kaltem Raum Steilheit vorsichtig erhoehen; ohne Raumabweichung keine pauschale Aenderung.";
else if(startsPerHour<1.0f)advice="Weniger als eine Brenneranforderung pro Stunde: kein Hinweis auf haeufiges Takten. Raumtemperatur gegen Sollwert beobachten.";
else advice="Keine auffaellige Anforderungsrate. Heizkurve nur bei wiederholter Abweichung der Raumtemperatur vom Sollwert anpassen.";
  }
  String j="{\"simulationOnly\":true,\"period\":\"letzte "+String(periodHours==168?"7 Tage":"24 Stunden")+"\",\"samples\":"+String(n)+
    ",\"requestedHours\":"+String(periodHours)+",\"windowSeconds\":"+String(duration)+",\"burnerStarts\":"+String(starts)+
    ",\"burnerSeconds\":"+String(burner)+",\"pumpSeconds\":"+String(pump)+
    ",\"burnerDutyPercent\":"+String(duty,1)+",\"startsPerHour\":"+String(startsPerHour,2)+
    ",\"estimatedOilLiters\":"+String(burner*oilLitersPerHour/3600.0f,3)+
    ",\"averageOutside\":"+String(n?sumA/n:0,1)+",\"averageRoom\":"+String(n?sumR/n:0,1)+
    ",\"averageBoiler\":"+String(n?sumB/n:0,1)+",\"sufficientData\":"+(sufficient?"true":"false")+
",\"advice\":\""+advice+"\"}";
return j;
}

// Manuelle Oeltank-Buchfuehrung: keine Ableitung aus der Simulation.
static constexpr float TANK_CAPACITY_L=4200.0f;
float tankLiters=0.0f;
bool tankLevelKnown=false;
float tankLastDeliveryL=0.0f;
uint32_t tankDeliveryCount=0;
String tankJson(){
  return String("{\"capacity\":4200,\"levelKnown\":")+(tankLevelKnown?"true":"false")+
    ",\"liters\":"+String(tankLiters,1)+",\"percent\":"+String(tankLevelKnown?tankLiters*100.0f/TANK_CAPACITY_L:0,1)+
    ",\"lastDelivery\":"+String(tankLastDeliveryL,1)+",\"deliveryCount\":"+String(tankDeliveryCount)+
",\"source\":\"manual\",\"geometry\":\"slightly_bulged_steel\"}";
}

// Virtuelle Heizkreispumpe: KEIN GPIO und keine Netzspannungs-Schaltung.
// Pumpenbetrieb waehrend Heizbedarf, Nachlauf nach Wegfall des Bedarfs.
uint8_t pumpMode=0;
// 0=Automatik, 1=Hand EIN, 2=Hand AUS
uint16_t pumpOverrunSeconds=180;
float summerCutoffC=18.0f, frostThresholdC=3.0f;
bool pumpHeatRequest=false, pumpFrostRequest=false, pumpSummerLock=false;
uint32_t lastPumpNeedMs=0;
bool pumpHadNeed=false;
uint32_t pumpOverrunRemaining(){
if(!pumpHadNeed || pumpHeatRequest || pumpFrostRequest)return 0;
uint32_t elapsed=millis()-lastPumpNeedMs;
uint32_t duration=(uint32_t)pumpOverrunSeconds*1000UL;
return elapsed>=duration?0:(duration-elapsed+999)/1000;
}
static const char* pumpReason(){
if(!pumpDemand)return "Kein Heizbedarf";
if(pumpMode==1)return "Handbetrieb";
if(pumpFrostRequest)return "Frostschutz";
if(pumpHeatRequest)return "Witterungsgefuehrter Heizbetrieb";
if(pumpOverrunRemaining()>0)return "Pumpennachlauf";
return "Automatik";
}
void updateVirtualPump(bool permitted){
  // Fehler und Simulationsstopp: aus, auch bei Hand EIN. Keine reale Schutzfunktion.
if(!permitted){pumpDemand=false;
  pumpHeatRequest=false;
  pumpFrostRequest=false;
  return;
  }
pumpSummerLock=outsideTemp>=summerCutoffC;
pumpFrostRequest=outsideTemp<=frostThresholdC || boilerTemp<=frostThresholdC;
  // Raumtemperatur wird nicht fuer die Pumpenanforderung verwendet.
  // Bei aktiver witterungsgefuehrter Heizung laeuft die Heizkreispumpe
  // unabhaengig davon, ob der Brenner gerade seine Solltemperatur erreicht hat.
  pumpHeatRequest = automatic && !pumpSummerLock;
bool need=pumpHeatRequest || pumpFrostRequest;
if(need){lastPumpNeedMs=millis();
  pumpHadNeed=true;
  }
if(pumpMode==1){pumpDemand=true;
  return;
  }
if(pumpMode==2){pumpDemand=false;
  return;
  }
pumpDemand=need || (pumpHadNeed && pumpOverrunRemaining()>0);
}

static void setVirtualBurner(bool on){
if(burnerDemand==on)return;
burnerDemand=on;
  lastBurnerTransitionMs=millis();
  burnerTransitionKnown=true;
}
static uint32_t burnerLockRemaining(){
if(!burnerTransitionKnown)return 0;
uint32_t minMs=(uint32_t)(burnerDemand?minBurnerOnSeconds:minBurnerOffSeconds)*1000UL;
uint32_t elapsed=millis()-lastBurnerTransitionMs;
return elapsed>=minMs?0:(minMs-elapsed+999)/1000;
}
static void updateVirtualBurner(float setpoint, bool permitted){
  // Freigabeentzug und Sensorausfall haben immer Vorrang vor Mindestlaufzeit.
if(!permitted){setVirtualBurner(false);
  return;
  }
  if(burnerDemand){
if(boilerTemp>setpoint+hysteresis/2 && burnerLockRemaining()==0)setVirtualBurner(false);
  }else{
if(boilerTemp<setpoint-hysteresis/2 && burnerLockRemaining()==0)setVirtualBurner(true);
  }
}


// ADS1115: Messung nur zu Diagnosezwecken, NICHT zur Brennerregelung.
Adafruit_ADS1115 ads;
bool adsReady=false;
uint32_t adsErrorCount=0;
uint32_t adsRecoveryCount=0;
uint32_t adsGoodPolls=0;
static constexpr uint8_t ADS_ADDRESS=0x48;
struct SensorReading { float volts=NAN, ohms=NAN, temp=NAN;
bool valid=false;
};
SensorReading sensor[3];
static constexpr float DIVIDER_R=NTC_DIVIDER_R;
static constexpr float SENSOR_VDD=3.3f;
static constexpr float TEMP_BETA[3]={NTC_A0_BETA,NTC_A1_BETA,NTC_A2_BETA};
// Individuelle NTC-Nennwiderstaende; Wolf-Fuehlerkennlinien noch nicht kalibriert.
static constexpr float TEMP_R25[3]={NTC_A0_R25,NTC_A1_R25,NTC_A2_R25};
uint32_t lastSensorPoll=0;
// 20-second moving average per channel (sampling currently every 1.5 seconds).
static constexpr uint8_t TEMP_FILTER_SAMPLES=14;
float tempSamples[3][TEMP_FILTER_SAMPLES]={};
float rawTemperature[3]={NAN,NAN,NAN};
uint8_t tempSampleCount[3]={},tempSampleNext[3]={};
float tempSampleSum[3]={};
static void clearTempFilter(uint8_t channel){
  tempSampleCount[channel]=0;
  tempSampleNext[channel]=0;
  tempSampleSum[channel]=0;
}
static float smoothTemperature(uint8_t channel,float value){
  if(tempSampleCount[channel]==TEMP_FILTER_SAMPLES){
    tempSampleSum[channel]-=tempSamples[channel][tempSampleNext[channel]];
  }else{
    tempSampleCount[channel]++;
  }
  tempSamples[channel][tempSampleNext[channel]]=value;
  tempSampleSum[channel]+=value;
  tempSampleNext[channel]=(tempSampleNext[channel]+1)%TEMP_FILTER_SAMPLES;
  return tempSampleSum[channel]/tempSampleCount[channel];
}
bool adsPing(){
Wire.beginTransmission(ADS_ADDRESS);
return Wire.endTransmission()==0;
}
void adsConfigure(){
ads.setGain(GAIN_ONE);
ads.setDataRate(RATE_ADS1115_128SPS);
}
void adsInitBus(){
  // NACH smartdisplay_init(): Bibliotheken koennen Wire zwischenzeitlich umkonfigurieren.
Wire.end();
delay(5);
Wire.begin(HEATCONTROL_I2C_SDA, HEATCONTROL_I2C_SCL);
Wire.setClock(100000);
Wire.setTimeOut(100);
adsReady=ads.begin(ADS_ADDRESS,&Wire);
if(adsReady){adsConfigure();
  Serial.println("ADS1115: erkannt an 0x48 (nach Displayinitialisierung)");
  }
else Serial.println("ADS1115: nicht erreichbar, nur Simulation");
}
void pollSensors(){
  // Ungueltige Werte niemals als gemessene Temperaturen ausgeben.
for(auto &v:sensor)v=SensorReading{};
for(float &t:rawTemperature)t=NAN;
  if(!adsPing()){
adsErrorCount++;
adsReady=false;
    // Ein einzelner erneuter Initialisierungsversuch pro Messzyklus.
adsInitBus();
if(!adsReady || !adsPing()){for(uint8_t i=0;i<3;i++)clearTempFilter(i);return;}
adsRecoveryCount++;
  }
  if(!adsReady){
adsReady=ads.begin(ADS_ADDRESS,&Wire);
if(!adsReady){adsErrorCount++;
    for(uint8_t i=0;i<3;i++)clearTempFilter(i);
    return;
    }
adsConfigure();
    adsRecoveryCount++;
  }
  for(int i=0;i<3;i++){
    // Bei Ausfall vor jedem Kanal nicht weiter mit veralteten Daten rechnen.
if(!adsPing()){adsErrorCount++;
    adsReady=false;
    for(uint8_t j=0;j<3;j++)clearTempFilter(j);
    return;
    }
int16_t raw=ads.readADC_SingleEnded(i);
float v=ads.computeVolts(raw);
SensorReading &r=sensor[i];
    r.volts=v;
    // 3.3V -- 5k Festwiderstand -- ADC -- NTC -- GND
if(!isfinite(v)||v<0.04f||v>SENSOR_VDD-0.04f){clearTempFilter(i);continue;}
r.ohms=DIVIDER_R*v/(SENSOR_VDD-v);
if(!isfinite(r.ohms)||r.ohms<100.0f||r.ohms>150000.0f){clearTempFilter(i);continue;}
float invT=1.0f/298.15f+logf(r.ohms/TEMP_R25[i])/TEMP_BETA[i];
r.temp=1.0f/invT-273.15f;
r.valid=isfinite(r.temp)&&r.ohms>=100.0f&&r.ohms<=POT_MAX_R*1.05f;
    if(r.valid){rawTemperature[i]=r.temp;r.temp=smoothTemperature(i,r.temp);}
    else clearTempFilter(i);
  }
adsGoodPolls++;
}
bool applyPotiTemperatures(){
  outsideTemp = inputMode==0 ? virtualOutside : (sensor[0].valid ? sensor[0].temp : NAN);
  roomTemp = inputMode==0 ? virtualRoom : roomSource.selected(sensor[1].valid ? sensor[1].temp : NAN);
  boilerTemp = inputMode==0 ? virtualBoiler : (sensor[2].valid ? sensor[2].temp : NAN);
  return inputMode==0 || (sensor[0].valid && sensor[2].valid && isfinite(roomTemp));
}
String sensorJson(){
String out="{\"connected\":"+String(adsReady?"true":"false")+",\"calibrated\":false,\"i2cErrors\":"+String(adsErrorCount)+",\"recoveries\":"+String(adsRecoveryCount)+",\"goodPolls\":"+String(adsGoodPolls)+",\"channels\":[";
  for(int i=0;i<3;i++){
if(i)out+=",";
out+="{\"valid\":"+String(sensor[i].valid?"true":"false");
out+=",\"volts\":"+(isfinite(sensor[i].volts)?String(sensor[i].volts,4):String("null"));
out+=",\"ohms\":"+(isfinite(sensor[i].ohms)?String(sensor[i].ohms,1):String("null"));
out+=",\"tempApprox\":"+(sensor[i].valid?String(sensor[i].temp,1):String("null"));
out+=",\"tempCurrent\":"+(sensor[i].valid&&isfinite(rawTemperature[i])?String(rawTemperature[i],1):String("null"))+"}";
  }
return out+"]}";
}
bool clockNow(struct tm &t) {
if(testClockEnabled){memset(&t,0,sizeof(t));
  t.tm_wday=(testWeekday+1)%7;
  t.tm_hour=testMinute/60;
  t.tm_min=testMinute%60;
  clockValid=true;
  return true;
  }
time_t now=time(nullptr);
if(now<1700000000){clockValid=false;
  return false;
  }
localtime_r(&now,&t);
  clockValid=true;
  return true;
}
int dayIndex(const struct tm &t){return (t.tm_wday+6)%7;
}
int minuteNow(const struct tm &t){return t.tm_hour*60+t.tm_min;
}
bool inPhase(int day,int minute){
  for(int i=0;i<3;i++){
Phase &p=phases[day][i];
if(p.enabled && p.from<p.to && minute>=p.from && minute<p.to)return true;
  }
return false;
}
int activeSchedulePhase(){
 if(!scheduleEnabled || holidayActive)return -1;
 struct tm t{};
 if(!clockNow(t))return -1;
 int day=dayIndex(t),minute=minuteNow(t);
 for(int i=0;i<3;i++){
  const Phase &p=phases[day][i];
  if(p.enabled && p.from<p.to && minute>=p.from && minute<p.to)return i;
 }
 return -1;
}
void evaluateProgram(){
struct tm t{};
bool valid=clockNow(t);
holidayActive=false;
comfortActive=true;
  if(!valid){
if(scheduleEnabled || holidayEnabled)setVirtualBurner(false);
    // Bei aktivierter Schaltuhr ohne Zeit keine automatische Brenneranforderung.
if(scheduleEnabled || holidayEnabled)targetRoom=holidayRoom;
else targetRoom=comfortRoom;
return;
  }
  if(holidayEnabled && !testClockEnabled){
time_t now=time(nullptr);
holidayActive=holidayStart>0 && holidayEnd>holidayStart && now>=holidayStart && now<holidayEnd;
  }
if(holidayActive){targetRoom=holidayRoom;
  comfortActive=false;
  }
else if(scheduleEnabled){comfortActive=inPhase(dayIndex(t),minuteNow(t));
  targetRoom=comfortActive?comfortRoom:reducedRoom;
  }
else targetRoom=comfortRoom;
}
void saveProgram(){
prefs.putBytes("phases",phases,sizeof(phases));
prefs.putBool("schedule",scheduleEnabled);
  prefs.putBool("holiday",holidayEnabled);
prefs.putFloat("comfort",comfortRoom);
  prefs.putFloat("reduced",reducedRoom);
  prefs.putFloat("holidaytemp",holidayRoom);
prefs.putLong64("holidayfrom",holidayStart);
  prefs.putLong64("holidayto",holidayEnd);
}
void initProgram(){
  for(int d=0;d<7;d++){
phases[d][0]={360,540,true};
    phases[d][1]={960,1320,true};
    phases[d][2]={0,0,false};
  }
if(prefs.getBytesLength("phases")==sizeof(phases))prefs.getBytes("phases",phases,sizeof(phases));
for(auto &day:phases)for(auto &p:day){if(p.from>1439||p.to>1440||p.from>=p.to){p.enabled=false;
  p.from=0;
  p.to=0;
  }}
scheduleEnabled=prefs.getBool("schedule",false);
  holidayEnabled=prefs.getBool("holiday",false);
comfortRoom=constrain(prefs.getFloat("comfort",21),10.f,28.f);
reducedRoom=constrain(prefs.getFloat("reduced",17),10.f,28.f);
holidayRoom=constrain(prefs.getFloat("holidaytemp",15),10.f,28.f);
holidayStart=prefs.getLong64("holidayfrom",0);
  holidayEnd=prefs.getLong64("holidayto",0);
}
String programJson(){
  String out="{\"scheduleEnabled\":"+String(scheduleEnabled?"true":"false")+
    ",\"holidayEnabled\":"+String(holidayEnabled?"true":"false")+
    ",\"holidayActive\":"+String(holidayActive?"true":"false")+
    ",\"clockValid\":"+String(clockValid?"true":"false")+
    ",\"testClock\":"+String(testClockEnabled?"true":"false")+
    ",\"comfortActive\":"+String(comfortActive?"true":"false")+
    ",\"comfort\":"+String(comfortRoom,1)+",\"reduced\":"+String(reducedRoom,1)+
    ",\"holidayTemp\":"+String(holidayRoom,1)+",\"holidayStart\":"+String((double)holidayStart,0)+
",\"holidayEnd\":"+String((double)holidayEnd,0)+",\"days\":[";
  for(int d=0;d<7;d++){
if(d)out+=",";
    out+="[";
    for(int i=0;i<3;i++){
if(i)out+=",";
out+="["+String(phases[d][i].from)+","+String(phases[d][i].to)+","+String(phases[d][i].enabled?1:0)+"]";
    }
out+="]";
  }
out+="]}";
  return out;
}
// Heizkurven-Ermittlung: nur ein fluechtiger, begrenzter Sollwert-Override.
// Die gespeicherte Steilheit wird niemals automatisch geaendert.
bool slopeTestActive=false;
float slopeTestOffset=0.0f;
float slopeTestStartRoom=NAN;
float slopeTestStartOutside=NAN;
uint32_t slopeTestStartedMs=0;
uint32_t slopeTestLastStepMs=0;
uint16_t slopeTestSteps=0;
String slopeTestStopReason="Nicht gestartet";
static constexpr uint32_t SLOPE_TEST_STEP_MS=3UL*60UL*60UL*1000UL;
static constexpr float SLOPE_TEST_MAX_OFFSET=6.0f;
void slopeTestStop(const String &reason){
 if(slopeTestActive){slopeTestActive=false;slopeTestOffset=0;eventLog("STEILHEIT",String("Ermittlung beendet: ")+reason);}
 slopeTestStopReason=reason;
}
void slopeTestTick(){
 if(!slopeTestActive)return;
 if(inputMode!=1 || manualMode || !automatic || !simulationRunning || !clockValid ||
    !isfinite(roomTemp) || !isfinite(outsideTemp) || !isfinite(boilerTemp) ||
    !sensor[0].valid || !sensor[2].valid || boilerTemp>=70.0f ||
    (roomSource.source==RoomSource::WLAN && !roomSource.available())){
   slopeTestStop("Regelung/Sensoren nicht freigegeben");return;
 }
 if(fabsf(outsideTemp-slopeTestStartOutside)>2.0f){
   slopeTestStop("Aussentemperatur zu stark veraendert");return;
 }
 uint32_t now=millis();
 if((uint32_t)(now-slopeTestLastStepMs)<SLOPE_TEST_STEP_MS)return;
 // Nur nach mindestens drei Stunden: in 1-K-Schritten zur Raum-Solltemperatur.
 float error=targetRoom-roomTemp;
 if(fabsf(error)<0.3f){slopeTestStop("Raumtemperatur stabil; Messpunkt erreicht");return;}
 float next=slopeTestOffset+(error>0?1.0f:-1.0f);
 if(fabsf(next)>SLOPE_TEST_MAX_OFFSET){slopeTestStop("Maximale Testabweichung erreicht");return;}
 slopeTestOffset=next;slopeTestSteps++;slopeTestLastStepMs=now;
 eventLog("STEILHEIT",String("Kessel-Soll Korrektur ")+String(slopeTestOffset,1)+" K; Raum "+String(roomTemp,1)+" C");
}
String slopeTestJson(){
 float delta=targetRoom-outsideTemp;
 float candidate=(isfinite(delta)&&delta>5.0f)?slope+slopeTestOffset/delta:NAN;
 return String("{\"active\":")+(slopeTestActive?"true":"false")+
 ",\"offset\":"+String(slopeTestOffset,1)+
 ",\"steps\":"+String(slopeTestSteps)+
 ",\"elapsedMinutes\":"+String(slopeTestActive?(uint32_t)(millis()-slopeTestStartedMs)/60000UL:0)+
 ",\"suggestedSlope\":"+(isfinite(candidate)?String(candidate,2):String("null"))+
 ",\"savedSlope\":"+String(slope,2)+
 ",\"reason\":\""+slopeTestStopReason+"\"}";
}
float desiredBoiler() {
  // Vorlaeufige, begrenzte Test-Heizkurve; spaeter kalibrieren.
if (!isfinite(outsideTemp)) return NAN;
float setpoint = targetRoom + slope * (targetRoom-outsideTemp);
if(slopeTestActive)setpoint+=slopeTestOffset;
return constrain(setpoint, 25.0f, 70.0f);
}
static String jsonFloat(float v, unsigned decimals=1) { return isfinite(v) ? String(v,decimals) : String("null"); }
String burnerBlockReason(){
 if(!sensor[0].valid && !sensor[2].valid)return "Sensorfehler A0 und A2";
 if(!sensor[0].valid)return "Sensorfehler A0 (Aussen)";
 if(!sensor[2].valid)return "Sensorfehler A2 (Kessel)";
 if(!isfinite(boilerTemp) || boilerTemp>=75.0f)return "Kesselgrenze 75 C";
 if(manualMode && sensorFaultLatched)return "Handfreigabe nach Stoerung erforderlich";
 if(!simulationRunning)return "Regelung gesperrt";
 if(!manualMode && !automatic)return "Automatik ausgeschaltet";
 if(!manualMode && !clockValid && (scheduleEnabled || holidayEnabled))return "Uhrzeit fehlt";
 if(manualMode && !manualBurner)return "Handbetrieb: Brenner AUS";
 if(!manualMode && burnerLockRemaining()>0 && !burnerDemand)return "Mindestpause";
 return "Keine Stoerung";
}
String json() {
  String s="{\"version\":\"" HEATCONTROL_VERSION "\",\"build\":\"" HEATCONTROL_BUILD_NUMBER "\",\"git\":\"" HEATCONTROL_GIT_SHA "\",\"inputMode\":"+String(inputMode)+",\"potiReady\":"+String(inputMode==1 && sensor[0].valid && sensor[2].valid?"true":"false")+",\"outside\":"+jsonFloat(outsideTemp,1)+",\"room\":"+jsonFloat(roomTemp,1)+
    ",\"boiler\":"+jsonFloat(boilerTemp,1)+",\"setpoint\":"+jsonFloat(desiredBoiler(),1)+
    ",\"slope\":"+String(slope,2)+",\"targetRoom\":"+String(targetRoom,1)+
    ",\"scheduleEnabled\":"+String(scheduleEnabled?"true":"false")+
    ",\"holidayActive\":"+String(holidayActive?"true":"false")+
    ",\"clockValid\":"+String(clockValid?"true":"false")+
    ",\"comfortActive\":"+String(comfortActive?"true":"false")+
    ",\"activePhase\":"+String(activeSchedulePhase())+
    ",\"automatic\":"+String(automatic?"true":"false")+
    ",\"manualMode\":"+String(manualMode?"true":"false")+
    ",\"pumpDemand\":"+String(pumpDemand?"true":"false")+
    ",\"pumpMode\":"+String(pumpMode)+
    ",\"pumpOverrun\":"+String(pumpOverrunSeconds)+
    ",\"pumpReason\":\""+String(pumpReason())+"\""+
    ",\"pumpOverrunRemaining\":"+String(pumpOverrunRemaining())+
    ",\"summerCutoff\":"+String(summerCutoffC,1)+
    ",\"frostThreshold\":"+String(frostThresholdC,1)+
    ",\"pumpHeatRequest\":"+String(pumpHeatRequest?"true":"false")+
    ",\"pumpFrostRequest\":"+String(pumpFrostRequest?"true":"false")+
    ",\"pumpSummerLock\":"+String(pumpSummerLock?"true":"false")+
    ",\"burnerDemand\":"+String(burnerDemand?"true":"false")+
    ",\"burnerBlockReason\":\""+burnerBlockReason()+"\""+
    ",\"sensorFaultLatched\":"+String(sensorFaultLatched?"true":"false")+
    ",\"simulationRunning\":"+String(simulationRunning?"true":"false")+
    ",\"simCommandSequence\":"+String(simCommandSequence)+
    ",\"burnerStarts\":"+String(burnerStarts)+
    ",\"burnerSeconds\":"+String(burnerSeconds)+
    ",\"simulationSeconds\":"+String(simulationSeconds)+
    ",\"hysteresis\":"+String(hysteresis,1)+
    ",\"minBurnerOn\":"+String(minBurnerOnSeconds)+
    ",\"minBurnerOff\":"+String(minBurnerOffSeconds)+
    ",\"burnerLockRemaining\":"+String(burnerLockRemaining())+
    ",\"clockValid\":"+String(clockValid?"true":"false")+
    ",\"holidayActive\":"+String(holidayActive?"true":"false")+
    ",\"comfortActive\":"+String(comfortActive?"true":"false")+
    ",\"oilEstimate\":"+String(burnerSeconds*oilLitersPerHour/3600.0f,2)+
    ",\"oilRate\":"+String(oilLitersPerHour,1)+
",\"clockText\":\""+([](){struct tm t{};if(!clockNow(t))return String("--:--");char b[24];strftime(b,sizeof(b),"%d.%m.%Y %H:%M",&t);return String(b);})()+"\"}";
return s;
}
#include "WebDashboardPage.h"

// 480x272 Touchdisplay: grafische Heizkurve und aktueller Betriebspunkt.
// Nur Simulation: keinerlei Brenner-GPIO.
static lv_obj_t *statusLabel=nullptr,*tempLabel=nullptr,*curveLabel=nullptr,*modeLabel=nullptr,*statsLabel=nullptr;
static lv_obj_t *curveLine=nullptr,*marker=nullptr,*graphLabel=nullptr;
static lv_obj_t *boilerLine=nullptr,*targetLine=nullptr;
static lv_obj_t *pages[4]={};
static int activeTab=0;
static lv_obj_t *schedInfo=nullptr,*holidayInfo=nullptr,*overviewInfo=nullptr,*touchFeedback=nullptr;
static lv_obj_t *scheduleFeedback=nullptr,*holidayFeedback=nullptr;
static uint32_t feedbackUntil=0;
static void refreshDisplay();
static void touchManualMode(lv_event_t *e){
 bool hand=(intptr_t)lv_event_get_user_data(e)==1;
 manualMode=hand;automatic=!hand;manualBurner=false;manualPump=false;
 eventLog("BETRIEB",hand?"Touch: Handbetrieb":"Touch: Automatik");
 setVirtualBurner(false);pumpDemand=false;refreshDisplay();
}
static void touchManualOutputs(lv_event_t *e){
 if(!manualMode)return;
 intptr_t action=(intptr_t)lv_event_get_user_data(e);
 manualBurner=action==1 && sensor[0].valid && (inputMode==0 || sensor[2].valid) && isfinite(boilerTemp) && boilerTemp<75.0f;manualPump=action==1 || action==2;
 if(manualBurner && sensorFaultLatched){sensorFaultLatched=false;eventLog("STOERUNG","Hand-Brenner nach gueltigen Sensorwerten erneut freigegeben");}
 eventLog("BEDIENUNG",String("Touch: Brenner ")+(manualBurner?"EIN":"AUS")+", Pumpe "+(manualPump?"EIN":"AUS"));
 refreshDisplay();
}
static void touchAck(lv_event_t *) {
evaluateProgram();
feedbackUntil=millis()+2500;
if(touchFeedback)lv_label_set_text(touchFeedback,"Eingabe uebernommen");
if(scheduleFeedback)lv_label_set_text(scheduleFeedback,"Auswahl / Einstellung uebernommen");
if(holidayFeedback)lv_label_set_text(holidayFeedback,"Einstellung uebernommen");
refreshDisplay();
}
static void selectTab(int index){activeTab=index;
for(int i=0;i<4;i++)if(pages[i])lv_obj_set_hidden(pages[i],i!=index);
}
static void tabClick(lv_event_t *e){selectTab((int)(intptr_t)lv_event_get_user_data(e));
}
static void nextDay(lv_event_t *e){selectedDay=(selectedDay+7+(int)(intptr_t)lv_event_get_user_data(e))%7;
}
static void nextPhase(lv_event_t *e){selectedPhase=(selectedPhase+3+(int)(intptr_t)lv_event_get_user_data(e))%3;
}
static void togglePhase(lv_event_t *){Phase &p=phases[selectedDay][selectedPhase];
p.enabled=!p.enabled;
if(p.from>=p.to){p.from=360;
p.to=540;
}saveProgram();
}
static void adjustPhase(lv_event_t *e){
int code=(int)(intptr_t)lv_event_get_user_data(e);
Phase &p=phases[selectedDay][selectedPhase];
if(p.from>=p.to){p.from=360;
  p.to=540;
  }
int delta=(abs(code)==1?-30:30);
if(code<0)p.from=(uint16_t)constrain((int)p.from+delta,0,(int)p.to-1);
else p.to=(uint16_t)constrain((int)p.to+delta,(int)p.from+1,1440);
saveProgram();
}
static void toggleSchedule(lv_event_t *){scheduleEnabled=!scheduleEnabled;
saveProgram();
}
static bool realClockAvailable(){return time(nullptr)>=1700000000;
}
static void initializeHolidayDates(){
if(holidayStart>=1700000000LL && holidayEnd>holidayStart)return;
if(!realClockAvailable())return;
time_t now=time(nullptr);
holidayStart=(int64_t)now;
holidayEnd=(int64_t)now+7LL*86400LL;
}
static void toggleHoliday(lv_event_t *){
  if(!holidayEnabled){
    if(!realClockAvailable()){
if(holidayFeedback)lv_label_set_text(holidayFeedback,"NTP-Uhr fehlt: Urlaub nicht aktivierbar");
feedbackUntil=millis()+6000;
return;
    }
initializeHolidayDates();
    if(holidayEnd<=holidayStart){
if(holidayFeedback)lv_label_set_text(holidayFeedback,"Zeitraum ungueltig");
feedbackUntil=millis()+6000;
return;
    }
  }
holidayEnabled=!holidayEnabled;
  saveProgram();
}
static void adjustHolidayDate(lv_event_t *e){
  if(!realClockAvailable()){
if(holidayFeedback)lv_label_set_text(holidayFeedback,"NTP-Uhr erforderlich");
feedbackUntil=millis()+6000;
return;
  }
initializeHolidayDates();
int direction=(int)(intptr_t)lv_event_get_user_data(e);
int64_t delta=86400LL*(direction<0?-1:1);
  if(abs(direction)==1){
int64_t candidate=holidayStart+delta;
if(candidate>=1700000000LL && candidate<holidayEnd)holidayStart=candidate;
  }else{
int64_t candidate=holidayEnd+delta;
if(candidate>holidayStart)holidayEnd=candidate;
  }
saveProgram();
}
static void adjustScheduleRoom(lv_event_t *e){
int direction=(int)(intptr_t)lv_event_get_user_data(e);
float &value=(abs(direction)==1)?comfortRoom:reducedRoom;
value=roundf(constrain(value+(direction<0?-0.5f:0.5f),10.0f,28.0f)*2.0f)/2.0f;
saveProgram();
}
static void adjustHolidayTemp(lv_event_t *e){holidayRoom=constrain(holidayRoom+(float)(intptr_t)lv_event_get_user_data(e)*0.5f,10.f,20.f);
saveProgram();
}

static lv_point_precise_t boilerPoints[2], targetPoints[2];
static lv_point_precise_t curvePoints[16];
static constexpr int GX=42,GY=12,GW=390,GH=90;
static float graphSetpoint(float outside) {
return constrain(targetRoom+slope*(targetRoom-outside),25.0f,75.0f);
}
static int graphX(float t){return GX+(int)((t+20.0f)/45.0f*GW);
}
static int graphY(float t){return GY+GH-(int)((t-20.0f)/60.0f*GH);
}
static void adjustOutside(lv_event_t *e){
intptr_t d=(intptr_t)lv_event_get_user_data(e);
outsideTemp=constrain(outsideTemp+(float)d,-30.0f,45.0f);
}
static void adjustBoiler(lv_event_t *e){
intptr_t d=(intptr_t)lv_event_get_user_data(e);
boilerTemp=constrain(boilerTemp+(float)d,0.0f,110.0f);
}
static void adjustSlope(lv_event_t *e){
intptr_t direction=(intptr_t)lv_event_get_user_data(e);
slope=roundf(constrain(slope+0.1f*(float)direction,0.2f,3.0f)*10.0f)/10.0f;
prefs.putFloat("slope",slope);
}
static void adjustHysteresis(lv_event_t *e){
intptr_t d=(intptr_t)lv_event_get_user_data(e);
hysteresis=roundf(constrain(hysteresis+0.5f*(float)d,2.0f,15.0f)*2.0f)/2.0f;
prefs.putFloat("hyst",hysteresis);
}
static void toggleSimulation(lv_event_t *){
}
static lv_obj_t* makeLabel(lv_obj_t *parent,const char *value,int x,int y){
lv_obj_t *o=lv_label_create(parent);
  lv_label_set_text(o,value);
  lv_obj_set_pos(o,x,y);
lv_obj_set_style_text_color(o,lv_color_hex(0xFFFFFF),0);
lv_obj_set_style_text_opa(o,LV_OPA_COVER,0);
lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);
return o;
}
static void makeButton(lv_obj_t *parent,const char *label,int x,int y,int w,
                       lv_event_cb_t callback,intptr_t arg){
lv_obj_t *b=lv_btn_create(parent);
  lv_obj_set_size(b,w,34);
  lv_obj_set_pos(b,x,y);
lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,(void*)arg);
lv_obj_add_event_cb(b,touchAck,LV_EVENT_CLICKED,nullptr);
lv_obj_t *l=lv_label_create(b);
  lv_label_set_text(l,label);
  lv_obj_center(l);
}
// Touch-Kalibrierung auf Basis der bereits vom Smartdisplay-Treiber gelieferten
// Koordinaten. Drei Punkte ergeben eine affine Korrektur (auch Achsentausch).
// KEINE Aenderung am XPT2046-Treiber und an dessen SPI-GPIOs.
static lv_indev_t *calIndev=nullptr;
static lv_indev_read_cb_t originalTouchRead=nullptr;
static bool touchCalActive=false, touchCalRequested=false, touchCalResetRequested=false;
static int touchCalStep=0;
static lv_point_t touchCalMeasured[3];
static const lv_point_t touchCalTargets[3]={{48,52},{432,52},{240,222}};
static lv_obj_t *touchCalOverlay=nullptr,*touchCalCross=nullptr,*touchCalText=nullptr;
static bool touchCalWasDown=false;
static uint32_t touchCalLastSample=0;
static bool touchCalValid=false;
static float touchAffine[6]={1,0,0,0,1,0};
static void touchReadCalibrated(lv_indev_t *indev, lv_indev_data_t *data){
originalTouchRead(indev,data);
  if(data->state==LV_INDEV_STATE_PRESSED){
const lv_point_t raw=data->point;
    if(touchCalActive){
      // Ein Punkt je Beruehrung, 300ms Entprellung. Keine LVGL-Objekte im Callback aendern.
      if(!touchCalWasDown && millis()-touchCalLastSample>300 && touchCalStep<3){
touchCalMeasured[touchCalStep]=raw;
touchCalStep++;
touchCalLastSample=millis();
Serial.printf("TOUCH CAL: Punkt %d, gemessen (%d,%d)\n",touchCalStep,raw.x,raw.y);
      }
data->state=LV_INDEV_STATE_RELEASED;
      // Kalibrierung darf keine UI-Tasten ausloesen
    }else if(touchCalValid){
float x=touchAffine[0]*raw.x+touchAffine[1]*raw.y+touchAffine[2];
float y=touchAffine[3]*raw.x+touchAffine[4]*raw.y+touchAffine[5];
data->point.x=(int32_t)roundf(constrain(x,0.f,479.f));
data->point.y=(int32_t)roundf(constrain(y,0.f,271.f));
    }
touchCalWasDown=true;
}else touchCalWasDown=false;
}
static bool solveTouchCalibration(){
const float x0=touchCalMeasured[0].x,y0=touchCalMeasured[0].y;
const float x1=touchCalMeasured[1].x,y1=touchCalMeasured[1].y;
const float x2=touchCalMeasured[2].x,y2=touchCalMeasured[2].y;
float det=x0*(y1-y2)+x1*(y2-y0)+x2*(y0-y1);
if(fabsf(det)<1000.f)return false;
const float xx[3]={x0,x1,x2},yy[3]={y0,y1,y2};
float next[6];
  for(int axis=0;axis<2;axis++){
const float a=axis?touchCalTargets[0].y:touchCalTargets[0].x;
const float b=axis?touchCalTargets[1].y:touchCalTargets[1].x;
const float c=axis?touchCalTargets[2].y:touchCalTargets[2].x;
next[axis*3+0]=(a*(yy[1]-yy[2])+b*(yy[2]-yy[0])+c*(yy[0]-yy[1]))/det;
next[axis*3+1]=(a*(xx[2]-xx[1])+b*(xx[0]-xx[2])+c*(xx[1]-xx[0]))/det;
next[axis*3+2]=(a*(xx[1]*yy[2]-xx[2]*yy[1])+b*(xx[2]*yy[0]-xx[0]*yy[2])+c*(xx[0]*yy[1]-xx[1]*yy[0]))/det;
  }
for(int i=0;i<6;i++)if(!isfinite(next[i])||fabsf(next[i])>10000.f)return false;
memcpy(touchAffine,next,sizeof(touchAffine));
touchCalValid=true;
prefs.putBytes("touchMatrix",touchAffine,sizeof(touchAffine));
Serial.println("TOUCH CAL: Kalibrierung gespeichert (NVS)");
return true;
}
static void touchCalDraw(){
if(!touchCalOverlay)return;
  if(touchCalStep<3){
lv_obj_set_pos(touchCalCross,touchCalTargets[touchCalStep].x-16,touchCalTargets[touchCalStep].y-16);
lv_label_set_text_fmt(touchCalText,"Touch-Kalibrierung: Punkt %d/3",touchCalStep+1);
  }
}
static void touchCalStart(){
touchCalValid=false;
  touchCalActive=true;
  touchCalStep=0;
  touchCalWasDown=false;
touchCalLastSample=millis()-1000;
  if(!touchCalOverlay){
touchCalOverlay=lv_obj_create(lv_layer_top());
lv_obj_set_size(touchCalOverlay,480,272);
    lv_obj_set_pos(touchCalOverlay,0,0);
lv_obj_set_style_bg_color(touchCalOverlay,lv_color_hex(0x142434),0);
lv_obj_set_style_bg_opa(touchCalOverlay,LV_OPA_COVER,0);
lv_obj_set_style_border_width(touchCalOverlay,0,0);
lv_obj_set_style_radius(touchCalOverlay,0,0);
lv_obj_set_scrollable(touchCalOverlay,false);
touchCalText=lv_label_create(touchCalOverlay);
lv_obj_set_style_text_color(touchCalText,lv_color_white(),0);
lv_obj_align(touchCalText,LV_ALIGN_BOTTOM_MID,0,-10);
touchCalCross=lv_label_create(touchCalOverlay);
lv_label_set_text(touchCalCross,"+");
lv_obj_set_style_text_font(touchCalCross,&lv_font_montserrat_14,0);
lv_obj_set_style_text_color(touchCalCross,lv_color_hex(0x00FF99),0);
  }
lv_obj_set_hidden(touchCalOverlay,false);
touchCalDraw();
}
static void touchCalService(){
  if(touchCalResetRequested){
touchCalResetRequested=false;
    touchCalActive=false;
    touchCalValid=false;
prefs.remove("touchMatrix");
if(touchCalOverlay)lv_obj_set_hidden(touchCalOverlay,true);
Serial.println("TOUCH CAL: zurueckgesetzt");
  }
if(touchCalRequested){touchCalRequested=false;
  touchCalStart();
  }
  if(touchCalActive){
static int drawn=-1;
if(touchCalStep!=drawn){drawn=touchCalStep;
    touchCalDraw();
    }
    if(touchCalStep==3){
bool ok=solveTouchCalibration();
touchCalActive=false;
if(touchCalOverlay)lv_obj_set_hidden(touchCalOverlay,true);
Serial.println(ok?"TOUCH CAL: ERFOLG":"TOUCH CAL: Punkte ungueltig; bitte wiederholen");
    }
  }
}
static void touchCalInstall(){
  if(prefs.getBytesLength("touchMatrix")==sizeof(touchAffine)){
prefs.getBytes("touchMatrix",touchAffine,sizeof(touchAffine));
    touchCalValid=true;
  }
  for(lv_indev_t *d=lv_indev_get_next(nullptr);d;d=lv_indev_get_next(d)){
    if(lv_indev_get_type(d)==LV_INDEV_TYPE_POINTER){
calIndev=d;
      originalTouchRead=lv_indev_get_read_cb(d);
if(originalTouchRead){lv_indev_set_read_cb(d,touchReadCalibrated);
      Serial.println("TOUCH CAL: Eingabegeraet gefunden");
      }
break;
    }
  }
if(!originalTouchRead)Serial.println("TOUCH CAL: Kein Pointer-Callback gefunden");
}
static void touchCalButton(lv_event_t *){touchCalRequested=true;
}

static void initDisplay(){
Serial.println("DISPLAY: smartdisplay_init begin");
smartdisplay_init();
Serial.println("DISPLAY: smartdisplay_init done");
smartdisplay_lcd_set_backlight(1.0f);
touchCalInstall();
lv_obj_t *screen=lv_scr_act();
lv_obj_set_style_bg_color(screen,lv_color_hex(0x142434),0);
lv_obj_set_style_text_color(screen,lv_color_hex(0xFFFFFF),0);
// Globaler Fussbereich, auf allen vier LCD-Tabs sichtbar.
lv_obj_t *brand=lv_label_create(screen);
lv_label_set_text(brand,"HeatControl by HK 2026 | Version " HEATCONTROL_VERSION " | Build " HEATCONTROL_BUILD_NUMBER " | Git " HEATCONTROL_GIT_SHA);
lv_obj_set_style_text_font(brand,&lv_font_montserrat_14,0);
lv_obj_set_style_text_color(brand,lv_color_hex(0xC9D8E8),0);
lv_obj_set_style_bg_color(brand,lv_color_hex(0x142434),0);
lv_obj_set_style_bg_opa(brand,LV_OPA_COVER,0);
lv_obj_set_style_pad_left(brand,6,0);
lv_obj_set_style_pad_right(brand,6,0);
lv_obj_set_pos(brand,0,252);
lv_obj_set_size(brand,480,20);
const char *tabs[]={"Uebersicht","Heizkurve","Schaltuhr","Urlaub"};
  for(int i=0;i<4;i++){
pages[i]=lv_obj_create(screen);
    lv_obj_set_pos(pages[i],0,32);
    lv_obj_set_size(pages[i],480,220);
lv_obj_set_style_bg_opa(pages[i],LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(pages[i],0,0);
lv_obj_set_style_pad_all(pages[i],0,0);
    lv_obj_set_scrollbar_mode(pages[i],LV_SCROLLBAR_MODE_OFF);
    // Jede Tab-Seite braucht ihre eigene Textfarbe (Styles werden nicht von Geschwister-Tabs geerbt).
lv_obj_set_style_text_color(pages[i],lv_color_hex(0xFFFFFF),0);
lv_obj_set_style_text_opa(pages[i],LV_OPA_COVER,0);
lv_obj_set_style_text_font(pages[i],&lv_font_montserrat_14,0);
makeButton(screen,tabs[i],i*120,0,118,tabClick,i);
  }
lv_obj_t *root=pages[1];
lv_obj_set_style_bg_color(root,lv_color_hex(0x142434),0);
lv_obj_set_style_text_color(root,lv_color_hex(0xFFFFFF),0);

makeLabel(root,"75",8,7);
  makeLabel(root,"20",8,92); // Unterer Wert der Kesseltemperatur-Y-Achse
  // Aussentemperatur-X-Achse: unmittelbar unter dem Diagramm, links -20, rechts +25.
  makeLabel(root,"-20",GX,GH+GY+3);
  makeLabel(root,"+25 C",GX+GW-52,GH+GY+3);
  // Linie in lokalen Koordinaten; statisches Array bleibt dauerhaft gueltig.
curveLine=lv_line_create(root);
lv_obj_set_pos(curveLine,GX,GY);
lv_obj_set_size(curveLine,GW,GH);
lv_obj_set_style_line_color(curveLine,lv_color_hex(0x58C6FF),0);
lv_obj_set_style_line_width(curveLine,3,0);
  // Waagerechte Referenzlinien: Kessel-Ist (orange) und Soll (gruen).
boilerLine=lv_line_create(root);
lv_obj_set_pos(boilerLine,GX,GY);
lv_obj_set_style_line_color(boilerLine,lv_color_hex(0xFFB84A),0);
lv_obj_set_style_line_width(boilerLine,2,0);
targetLine=lv_line_create(root);
lv_obj_set_pos(targetLine,GX,GY);
lv_obj_set_style_line_color(targetLine,lv_color_hex(0x78DD99),0);
lv_obj_set_style_line_width(targetLine,1,0);
marker=lv_obj_create(root);
lv_obj_set_size(marker,11,11);
lv_obj_set_style_radius(marker,LV_RADIUS_CIRCLE,0);
lv_obj_set_style_bg_color(marker,lv_color_hex(0xFFB84A),0);
lv_obj_set_style_border_width(marker,0,0);
graphLabel=makeLabel(root,"",10,123);
tempLabel=makeLabel(root,"",10,140);
curveLabel=makeLabel(root,"",10,157);
statusLabel=makeLabel(root,"",10,174);
modeLabel=makeLabel(root,"",335,174);
statsLabel=makeLabel(root,"",10,190);
makeButton(root,"A -",5,206,72,adjustOutside,-1);
makeButton(root,"A +",84,206,72,adjustOutside,1);
makeButton(root,"K -",163,206,72,adjustBoiler,-1);
makeButton(root,"K +",242,206,72,adjustBoiler,1);
makeButton(root,"S -",321,206,72,adjustSlope,-1);
makeButton(root,"S +",400,206,72,adjustSlope,1);


lv_obj_t *overview=pages[0];
makeLabel(overview,"HEATCONTROL - UEBERSICHT",10,2);
overviewInfo=makeLabel(overview,"",10,22);
lv_obj_set_width(overviewInfo,460);
  lv_label_set_long_mode(overviewInfo,LV_LABEL_LONG_WRAP);
lv_obj_set_style_text_font(overviewInfo,&lv_font_montserrat_14,0);
makeButton(overview,"Automatik",10,152,145,touchManualMode,0);
makeButton(overview,"Handbetrieb",166,152,145,touchManualMode,1);

makeButton(overview,"Brenner EIN",10,191,145,touchManualOutputs,1);
makeButton(overview,"Brenner AUS",166,191,145,touchManualOutputs,2);
makeButton(overview,"Alles AUS",322,191,145,touchManualOutputs,0);
touchFeedback=makeLabel(overview,"",10,230);
lv_obj_t *schedule=pages[2];
schedInfo=makeLabel(schedule,"",8,6);
lv_obj_set_width(schedInfo,460);
  lv_label_set_long_mode(schedInfo,LV_LABEL_LONG_WRAP);
makeButton(schedule,"Tag -",8,78,82,nextDay,-1);
makeButton(schedule,"Tag +",98,78,82,nextDay,1);
makeButton(schedule,"Phase -",188,78,90,nextPhase,-1);
makeButton(schedule,"Phase +",286,78,90,nextPhase,1);
makeButton(schedule,"Von -",8,122,82,adjustPhase,-1);
makeButton(schedule,"Von +",98,122,82,adjustPhase,-2);
makeButton(schedule,"Bis -",188,122,82,adjustPhase,1);
makeButton(schedule,"Bis +",278,122,82,adjustPhase,2);
makeButton(schedule,"Phase EIN/AUS",8,170,160,togglePhase,0);
makeButton(schedule,"Plan EIN/AUS",178,170,160,toggleSchedule,0);
makeButton(schedule,"Komf -",8,205,108,adjustScheduleRoom,-1);
makeButton(schedule,"Komf +",128,205,108,adjustScheduleRoom,1);
makeButton(schedule,"Absenk -",248,205,108,adjustScheduleRoom,-2);
makeButton(schedule,"Absenk +",368,205,108,adjustScheduleRoom,2);
scheduleFeedback=makeLabel(schedule,"Bereit",8,57);
lv_obj_t *holiday=pages[3];
holidayInfo=makeLabel(holiday,"",8,8);
lv_obj_set_width(holidayInfo,460);
  lv_label_set_long_mode(holidayInfo,LV_LABEL_LONG_WRAP);
makeButton(holiday,"Start -1T",8,108,110,adjustHolidayDate,-1);
makeButton(holiday,"Start +1T",128,108,110,adjustHolidayDate,1);
makeButton(holiday,"Ende -1T",248,108,110,adjustHolidayDate,-2);
makeButton(holiday,"Ende +1T",368,108,110,adjustHolidayDate,2);
makeButton(holiday,"Urlaub EIN/AUS",8,152,170,toggleHoliday,0);
makeButton(holiday,"Temp -",188,152,110,adjustHolidayTemp,-1);
makeButton(holiday,"Temp +",308,152,110,adjustHolidayTemp,1);
holidayFeedback=makeLabel(holiday,"Bereit",8,201);
selectTab(0);
Serial.println("DISPLAY: LVGL objects created");
}
static void refreshDisplay(){
if(!tempLabel)return;
char buffer[256];
for(int i=0;i<16;i++){
  float a=-20.0f+i*3.0f;
  curvePoints[i].x=(int)((a+20.0f)/45.0f*GW);
  curvePoints[i].y=graphY(graphSetpoint(a))-GY;
}
lv_line_set_points(curveLine,curvePoints,16);
const bool outsideValid=isfinite(outsideTemp);
const bool boilerValid=isfinite(boilerTemp);
const float setpoint=desiredBoiler();
const bool setpointValid=isfinite(setpoint);
if(boilerValid){
  int by=constrain(graphY(boilerTemp)-GY,0,GH);
  boilerPoints[0]={0,(decltype(boilerPoints[0].y))by};
  boilerPoints[1]={(decltype(boilerPoints[0].x))GW,(decltype(boilerPoints[0].y))by};
  lv_line_set_points(boilerLine,boilerPoints,2);
  lv_obj_set_hidden(boilerLine, false);
} else lv_obj_set_hidden(boilerLine, true);
if(setpointValid){
  int ty=constrain(graphY(setpoint)-GY,0,GH);
  targetPoints[0]={0,(decltype(targetPoints[0].y))ty};
  targetPoints[1]={(decltype(targetPoints[0].x))GW,(decltype(targetPoints[0].y))ty};
  lv_line_set_points(targetLine,targetPoints,2);
  lv_obj_set_hidden(targetLine, false);
} else lv_obj_set_hidden(targetLine, true);
if(outsideValid && setpointValid){
  lv_obj_set_pos(marker,graphX(outsideTemp)-5,graphY(setpoint)-5);
  lv_obj_set_hidden(marker, false);
} else lv_obj_set_hidden(marker, true);
// Display temperatures are held for 20 seconds; control uses fresh filtered readings.
static uint32_t lastVisibleTempMs=0;
static float visibleTemp[3]={NAN,NAN,NAN};
if(lastVisibleTempMs==0 || millis()-lastVisibleTempMs>=20000UL ||
   (isfinite(visibleTemp[0]) && !outsideValid) ||
   (isfinite(visibleTemp[1]) && !isfinite(roomTemp)) ||
   (isfinite(visibleTemp[2]) && !boilerValid)){
  visibleTemp[0]=outsideTemp;visibleTemp[1]=roomTemp;visibleTemp[2]=boilerTemp;
  lastVisibleTempMs=millis();
}
char aText[24],rText[24],bText[24],sText[24];
if(isfinite(visibleTemp[0])) snprintf(aText,sizeof(aText),"%.1f C",visibleTemp[0]); else snprintf(aText,sizeof(aText),"--- C");
if(isfinite(visibleTemp[1])) snprintf(rText,sizeof(rText),"%.1f C",visibleTemp[1]); else snprintf(rText,sizeof(rText),"--- C");
if(isfinite(visibleTemp[2])) snprintf(bText,sizeof(bText),"%.1f C",visibleTemp[2]); else snprintf(bText,sizeof(bText),"--- C");
if(setpointValid) snprintf(sText,sizeof(sText),"%.1f C",setpoint); else snprintf(sText,sizeof(sText),"--- C");
snprintf(buffer,sizeof(buffer),"Aussen %s  ->  Soll %s",aText,sText);
lv_label_set_text(graphLabel,buffer);
snprintf(buffer,sizeof(buffer),"Raum %s    Kessel IST %s",rText,bText);
lv_label_set_text(tempLabel,buffer);
if(setpointValid) snprintf(buffer,sizeof(buffer),"Steilheit %.1f   Schaltschwellen %.1f / %.1f C",slope,setpoint-hysteresis/2,setpoint+hysteresis/2);
else snprintf(buffer,sizeof(buffer),"Steilheit %.1f   Schaltschwellen --- / --- C",slope);
lv_label_set_text(curveLabel,buffer);
snprintf(buffer,sizeof(buffer),"Brenner: %s   Pumpe: %s",burnerDemand?"EIN":"AUS",pumpDemand?"EIN":"AUS");
lv_label_set_text(statusLabel,buffer);
lv_label_set_text(modeLabel,simulationRunning?"REGELUNG AKTIV":"REGELUNG GESPERRT");
snprintf(buffer,sizeof(buffer),"Starts %lu  Laufzeit %lu:%02lu  Betrieb %lu:%02lu",
  (unsigned long)burnerStarts,
  (unsigned long)(burnerSeconds/60),(unsigned long)(burnerSeconds%60),
  (unsigned long)(simulationSeconds/60),(unsigned long)(simulationSeconds%60));
lv_label_set_text(statsLabel,buffer);
  if(overviewInfo){
struct tm t{};
    bool valid=clockNow(t);
    char stamp[32]="--:--";
if(valid)strftime(stamp,sizeof(stamp),"%d.%m.%Y %H:%M",&t);
    const String pumpText=manualMode?(pumpDemand?"Handbetrieb":"AUS"):pumpReason();
const unsigned long overrun=(unsigned long)pumpOverrunRemaining();
const unsigned long lock=(unsigned long)burnerLockRemaining();
const char *fault=(!sensor[0].valid && !sensor[2].valid)?"A0+A2":(!sensor[0].valid?"A0":(!sensor[2].valid?"A2":(isfinite(boilerTemp)&&boilerTemp>=75.f?"KESSEL >=75 C":"OK")));
snprintf(buffer,sizeof(buffer),
"Uhr: %s  %s  Regelung: %s\n"
"Aussen %s  Raum %s\n"
"Kessel %s  Soll %s  S %.1f\n"
"Brenner %s  Sperre %lu s\n"
"Pumpe %s  %s\n"
"Nachlauf %lu s  Sommer %s  Frost %s\n"
"Sensoren %s  Uhr %s  Urlaub %s",
stamp,manualMode?"HAND":"AUTOMATIK",simulationRunning?"AKTIV":"GESPERRT",
aText,rText,bText,sText,slope,
burnerDemand?"EIN":"AUS",lock,
pumpDemand?"EIN":"AUS",pumpText.c_str(),
overrun,pumpSummerLock?"JA":"NEIN",pumpFrostRequest?"JA":"NEIN",
fault,clockValid?"OK":"FEHLT",holidayActive?"JA":"NEIN");
// Stoerungsgrund auf der Uebersicht statt pauschaler Statusmeldung.
if(touchFeedback && feedbackUntil==0)lv_label_set_text(touchFeedback,burnerBlockReason().c_str());
lv_label_set_text(overviewInfo,buffer);
    if(feedbackUntil && (int32_t)(millis()-feedbackUntil)>=0){
if(touchFeedback)lv_label_set_text(touchFeedback,"Bereit");
if(scheduleFeedback)lv_label_set_text(scheduleFeedback,"Bereit");
if(holidayFeedback)lv_label_set_text(holidayFeedback,"Bereit");
feedbackUntil=0;
    }
  }
  if(schedInfo){
Phase &p=phases[selectedDay][selectedPhase];
    snprintf(buffer,sizeof(buffer),"SCHALTUHR: %s  Zeit: %s\nTag %d  Phase %d: %02d:%02d - %02d:%02d  %s\nKomfort %.1f C  Absenk %.1f C",
      scheduleEnabled?"EIN":"AUS",clockValid?"OK":"FEHLT",selectedDay+1,selectedPhase+1,
p.from/60,p.from%60,p.to/60,p.to%60,p.enabled?"EIN":"AUS",comfortRoom,reducedRoom);
lv_label_set_text(schedInfo,buffer);
  }
  if(holidayInfo){
char from[24]="nicht gesetzt",to[24]="nicht gesetzt";
if(holidayStart>=1700000000LL){time_t t=(time_t)holidayStart;
    struct tm local{};
    localtime_r(&t,&local);
    strftime(from,sizeof(from),"%d.%m.%y %H:%M",&local);
    }
if(holidayEnd>=1700000000LL){time_t t=(time_t)holidayEnd;
    struct tm local{};
    localtime_r(&t,&local);
    strftime(to,sizeof(to),"%d.%m.%y %H:%M",&local);
    }
    snprintf(buffer,sizeof(buffer),"URLAUB %s / %s   %.1f C\nBeginn: %s\nRueckkehr: %s\nNTP: %s  (Tasten: +/- 1 Tag)",
holidayEnabled?"EIN":"AUS",holidayActive?"AKTIV":"INAKTIV",holidayRoom,from,to,realClockAvailable()?"OK":"FEHLT");
lv_label_set_text(holidayInfo,buffer);
  }
}

// HeatControl v0.5.5: PCF8574 auf dem bereits initialisierten ADS1115-I2C-Bus.
// KEINE Relais-GPIOs am ESP32! XPT2046-Touch bleibt unberuehrt.
// PCF8574 (ohne A-Suffix): A0=A1=A2=GND -> 0x20. P0=Brenner, P1=Pumpe.
// PCF8574-Ausgaenge sind quasi-bidirektional; HIGH ist nur schwach getrieben.
// Nur fuer galvanisch getrennten Niedervolt-Pruefstand, keine reale Heizung.
#ifndef HEATCONTROL_PCF8574_ADDRESS
#define HEATCONTROL_PCF8574_ADDRESS 0x20
#endif
#ifndef HEATCONTROL_PCF8574_OUTPUTS_ENABLED
#define HEATCONTROL_PCF8574_OUTPUTS_ENABLED 0
#endif
static constexpr uint8_t PCF_OFF=0xFF;
// Alle acht Pins HIGH (quasi-bidirektional)
static bool pcfPresent=false;
static bool pcfOutputHealthy=false;
static uint8_t pcfRequested=PCF_OFF;
static uint8_t pcfLastWritten=PCF_OFF;
static uint32_t pcfLastProbe=0, pcfLastWrite=0;
static uint32_t pcfErrors=0, pcfSuccess=0;
// Ausschliesslich kurzzeitige manuelle Niedervolt-Tests. Keine Automatik-Freigabe.
static uint8_t relayTestPin=255;
static uint32_t relayTestUntil=0;
static bool relayTestActive=false;
// Zeitlich begrenzte Automatik ausschliesslich fuer den getrennten Niedervolt-Pruefstand.
static bool relayBenchAuto=false;
// Beim Start nach erfolgreicher PCF-Initialisierung automatisch freigeben (nur Niedervolt-Pruefstand).
static void relayBenchStop(const char *reason){
if(relayBenchAuto){relayBenchAuto=false;
  eventLog("RELAIS",String("Pruefstand-Automatik AUS: ")+reason);
  }
}

static void relayTestStop(const char *reason);
static char relayCommand[32];
static size_t relayCommandLen=0;
static bool pcfWrite(uint8_t data){
Wire.beginTransmission(HEATCONTROL_PCF8574_ADDRESS);
Wire.write(data);
uint8_t error=Wire.endTransmission();
if(error){pcfErrors++;
  pcfPresent=false;
  pcfOutputHealthy=false;
  return false;
  }
pcfPresent=true;
  pcfOutputHealthy=true;
  pcfLastWritten=data;
  pcfLastWrite=millis();
  pcfSuccess++;
return true;
}
static void relayTestStop(const char *reason){
  if(relayTestActive){
relayTestActive=false;
    relayTestPin=255;
    relayTestUntil=0;
eventLog("RELAIS",String("Test beendet: ")+reason);
  }
pcfRequested=PCF_OFF;
if(pcfPresent)pcfWrite(PCF_OFF);
}
static bool relayTestStart(uint8_t pin){
if(pin>1 || !pcfPresent || !pcfOutputHealthy || relayOtaInProgress)return false;
relayBenchStop("Einzeltest");
relayTestStop("Neuer Test");
relayTestPin=pin;
relayTestUntil=millis()+2000;
relayTestActive=true;
uint8_t value=(uint8_t)(PCF_OFF & ~(1u<<pin));
pcfRequested=value;
  if(!pcfWrite(value)){
relayTestActive=false;
    relayTestPin=255;
    relayTestUntil=0;
eventLog("FEHLER","PCF8574: Testausgang konnte nicht gesetzt werden");
return false;
  }
eventLog("RELAIS",String("Niedervolt-Test P")+String(pin)+" gestartet (max. 2 s)");
return true;
}
static void relayOff(){
relayBenchStop("Not-Aus / OTA");
  // Ein Schreibversuch ist nur sinnvoll, wenn das Modul zuvor erkannt wurde.
relayTestStop("Relais AUS");
}
static void relayInit(){
  // Aufruf NACH initDisplay() und adsInitBus(). PCF kann noch nicht angesteckt sein.
Serial.printf("PCF8574: Suche auf I2C 0x%02X (ADS1115: 0x48)\n",HEATCONTROL_PCF8574_ADDRESS);
pcfLastProbe=millis();
Wire.beginTransmission(HEATCONTROL_PCF8574_ADDRESS);
if(Wire.endTransmission()!=0){Serial.println("PCF8574: noch nicht angeschlossen; Touch/ADS bleiben aktiv");
  return;
  }
pcfPresent=true;
  if(pcfWrite(PCF_OFF)){
relayBenchAuto=true;
eventLog("RELAIS","Niedervolt-Pruefstand: Ausgaenge nach Initialisierung dauerhaft freigegeben");
Serial.println("PCF8574: erkannt; Pruefstand-Ausgaenge automatisch freigegeben (aktuell AUS)");
  }
}
static void relayUpdate(){
if(inputMode==0){
  if(relayTestActive)relayTestStop("Software-Simulation");
  if(relayBenchAuto)relayBenchStop("Software-Simulation");
  if(pcfPresent && pcfLastWritten!=PCF_OFF)pcfWrite(PCF_OFF);
  return;
}
const uint32_t now=millis();
  if(relayBenchAuto && (relayOtaInProgress || !pcfPresent || !pcfOutputHealthy)){
relayBenchStop("I2C / OTA");
  }
  if(relayTestActive){
    if(relayOtaInProgress || (int32_t)(now-relayTestUntil)>=0 || !pcfPresent){
relayTestStop("Zeit abgelaufen / OTA / I2C-Verlust");
}else return;
  }
  if(!pcfPresent){
relayBenchStop("PCF8574 nicht erreichbar");
if(now-pcfLastProbe<5000)return;
pcfLastProbe=now;
Wire.beginTransmission(HEATCONTROL_PCF8574_ADDRESS);
if(Wire.endTransmission()!=0)return;
pcfPresent=true;
if(!pcfWrite(PCF_OFF))return;
relayBenchAuto=true;
eventLog("RELAIS","Niedervolt-Pruefstand: Ausgaenge nach I2C-Wiedererkennung freigegeben");
Serial.println("PCF8574: nachtraeglich erkannt, Pruefstand-Ausgaenge freigegeben");
  }
uint8_t wanted=PCF_OFF;
  if(relayBenchAuto && !relayOtaInProgress && simulationRunning && (automatic || manualMode) && pcfOutputHealthy && inputMode==1 && sensor[0].valid && sensor[2].valid && isfinite(boilerTemp) && boilerTemp<75.0f){
if(burnerDemand && (!manualMode || (!sensorFaultLatched && manualBurner)))wanted &= (uint8_t)~0x01;
if(pumpDemand)wanted &= (uint8_t)~0x02;
  }
pcfRequested=wanted;
  if(wanted!=pcfLastWritten || now-pcfLastWrite>=1000){
const uint8_t previous=pcfLastWritten;
    if(!pcfWrite(wanted)){
relayBenchStop("I2C-Schreibfehler");
Serial.println("PCF8574: I2C-Fehler, Ausgangszustand unbekannt!");
eventLog("FEHLER","PCF8574 I2C-Schreibfehler: Ausgangszustand unbekannt");
    }else if((previous^wanted)&0x03){
eventLog("RELAIS",String("P0=")+((wanted&1)?"AUS":"EIN")+", P1="+((wanted&2)?"AUS":"EIN")+" (Niedervolt-Pruefstand)");
    }
  }
}
static bool relayBenchStart(){
if(!pcfPresent || !pcfOutputHealthy || relayOtaInProgress || !simulationRunning || !automatic || relayTestActive)return false;
relayBenchAuto=true;
eventLog("RELAIS","Pruefstand-Automatik dauerhaft aktiviert (bis AUS/Neustart/Fehler)");
relayUpdate();
return true;
}
static String relayDiagnosticJson(){
  return String("{\"version\":\"0.5.9\",\"type\":\"PCF8574\",\"address\":\"")+String(HEATCONTROL_PCF8574_ADDRESS,HEX)+
    "\",\"present\":"+(pcfPresent?"true":"false")+
    ",\"initialized\":"+(pcfOutputHealthy?"true":"false")+
    ",\"outputsEnabled\":"+(relayBenchAuto?"true":"false")+
    ",\"benchAuto\":"+(relayBenchAuto?"true":"false")+
    ",\"benchRemainingSeconds\":"+String(relayBenchAuto?-1:0)+
    ",\"testActive\":"+(relayTestActive?"true":"false")+
    ",\"testPin\":"+String(relayTestActive?relayTestPin:-1)+
    ",\"testRemainingMs\":"+String(relayTestActive?(uint32_t)max(0L,(long)(relayTestUntil-millis())):0)+
    ",\"burnerDemand\":"+(burnerDemand?"true":"false")+
    ",\"pumpDemand\":"+(pumpDemand?"true":"false")+
    ",\"burnerRelay\":"+((pcfOutputHealthy && !(pcfLastWritten&0x01))?"true":"false")+
    ",\"pumpRelay\":"+((pcfOutputHealthy && !(pcfLastWritten&0x02))?"true":"false")+
    ",\"lastWritten\":"+String(pcfLastWritten)+
    ",\"errors\":"+String(pcfErrors)+
",\"success\":"+String(pcfSuccess)+"}";
}
static void relaySerial(){
  while(Serial.available()){
char c=(char)Serial.read();
    if(c=='\r'||c=='\n'){
if(!relayCommandLen)continue;
relayCommand[relayCommandLen]=0;
String cmd(relayCommand);
      cmd.trim();
      cmd.toUpperCase();
      relayCommandLen=0;
if(cmd=="RELAY STATUS")Serial.println(relayDiagnosticJson());
else if(cmd=="RELAY OFF"){relayOff();
      Serial.println("PCF8574: AUS angefordert; Automatik ggf. im naechsten Zyklus aktiv");
      }
else Serial.println("Befehle: RELAY STATUS, RELAY OFF");
}else if(relayCommandLen<sizeof(relayCommand)-1)relayCommand[relayCommandLen++]=c;
else relayCommandLen=0;
  }
}
void setup(){
Serial.begin(115200);
 delay(300);
 Serial.println("HeatControl v" HEATCONTROL_VERSION " | Build " HEATCONTROL_BUILD_NUMBER " | Git " HEATCONTROL_GIT_SHA " – SIMULATION / RELAIS-PRUEFSTAND");
prefs.begin("heatcontrol",false);
roomSource.begin(prefs);
 burnerStarts=prefs.getUInt("burnStarts",0);
 burnerSeconds=prefs.getUInt("burnSecs",0);
 pumpSeconds=prefs.getUInt("pumpSecs",0);
 simulationSeconds=prefs.getUInt("simSecs",0);
tankLevelKnown=prefs.getBool("tankKnown",false);
tankLiters=constrain(prefs.getFloat("tankLiters",0),0.0f,TANK_CAPACITY_L);
tankLastDeliveryL=prefs.getFloat("tankLast",0);
tankDeliveryCount=prefs.getUInt("tankCount",0);
oilLitersPerHour=constrain(prefs.getFloat("oilRate",2.0f),0.1f,20.f);
historyInit();
eventLoadFromFlash();
eventLog("SYSTEM","Neustart; PCF8574 wird initialisiert; Niedervolt-Pruefstand");
initProgram();
 inputMode=prefs.getUChar("inputMode",1)==0?0:1;
 virtualOutside=prefs.getFloat("simOutside",10.f);
 virtualRoom=prefs.getFloat("simRoom",21.f);
 virtualBoiler=prefs.getFloat("simBoiler",45.f);
 if(inputMode==0)simulationRunning=false; // Kein automatischer Start im Simulationsmodus
setenv("TZ",tzRule,1);
 tzset();
slope=constrain(prefs.getFloat("slope",1.2f),0.2f,3.f);
 targetRoom=prefs.getFloat("roomset",21);
 hysteresis=constrain(prefs.getFloat("hyst",5.f),2.f,15.f);
 minBurnerOnSeconds=constrain((int)prefs.getUInt("minOn",30),0,600);
 minBurnerOffSeconds=constrain((int)prefs.getUInt("minOff",20),0,600);
 pumpMode=constrain((int)prefs.getUChar("pumpMode",0),0,2);
 pumpOverrunSeconds=constrain((int)prefs.getUInt("pumpRun",180),0,1800);
 summerCutoffC=constrain(prefs.getFloat("pumpSummer",18.f),10.f,30.f);
 frostThresholdC=constrain(prefs.getFloat("pumpFrost",3.f),-10.f,10.f);
WiFi.mode(WIFI_AP_STA);
 WiFi.softAP("HeatControl-Test");
String ssid=prefs.getString("ssid","");
 if(ssid.length()){
WiFi.begin(ssid.c_str(),prefs.getString("wifiPass","").c_str());
configTzTime(tzRule,"pool.ntp.org","time.google.com");
 }
Serial.print("Test-WLAN HeatControl-Test, Browser: http://");
 Serial.println(WiFi.softAPIP());
server.on("/",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send_P(200,"text/html; charset=utf-8",PAGE);});
server.on("/api/slope-test",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",slopeTestJson());});
server.on("/api/slope-test",HTTP_POST,[]{
 String cmd=server.arg("cmd");
 if(cmd=="stop"){slopeTestStop("Manuell beendet");server.send(200,"application/json",slopeTestJson());return;}
 if(cmd!="start"){server.send(400,"text/plain","cmd=start oder stop");return;}
 if(inputMode!=1||manualMode||!automatic||!simulationRunning||!clockValid||
    !isfinite(roomTemp)||!isfinite(outsideTemp)||!isfinite(boilerTemp)||
    !sensor[0].valid||!sensor[2].valid||boilerTemp>=70.0f||
    (roomSource.source==RoomSource::WLAN&&!roomSource.available())){
   server.send(409,"text/plain","Ermittlung gesperrt: Sensoren, Uhr oder Regelung pruefen");return;
 }
 slopeTestActive=true;slopeTestOffset=0;slopeTestSteps=0;
 slopeTestStartedMs=millis();slopeTestLastStepMs=millis();
 slopeTestStartRoom=roomTemp;slopeTestStartOutside=outsideTemp;
 slopeTestStopReason="Messung laeuft";
 eventLog("STEILHEIT","Gefuehrte Ermittlung gestartet; kein automatisches Speichern");
 server.send(200,"application/json",slopeTestJson());
});
server.on("/api/room-source",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",roomSource.json());});
server.on("/api/room-source",HTTP_POST,[]{
  if(!server.hasArg("source") || !server.hasArg("ip")) {server.send(400,"text/plain","source und ip erforderlich");return;}
  int nextSource=server.arg("source")=="wlan"?1:server.arg("source")=="ntc"?0:-1;
  if(!roomSource.configure(nextSource,server.arg("ip"))) {server.send(400,"text/plain","Ungueltige Quelle oder IP");return;}
  applyPotiTemperatures();
  eventLog("RAUMQUELLE",String(nextSource?"WLAN ":"NTC ")+roomSource.ip);
  server.send(200,"application/json",roomSource.json());
});
server.on("/raumquelle",HTTP_GET,[]{
  String page=R"HTML(<!doctype html><html lang="de"><meta name="viewport" content="width=device-width,initial-scale=1"><meta charset="utf-8"><title>HeatControl Raumtemperatur</title><style>body{font:16px system-ui;max-width:520px;margin:30px auto;padding:16px;background:#f4f6f8;color:#172133}main{background:white;padding:24px;border-radius:12px}input,select,button{font:inherit;padding:10px;margin:8px 0;width:100%;box-sizing:border-box}</style><main><h2>Raumtemperaturquelle</h2><form action="/api/room-source" method="post"><label>Quelle</label><select name="source"><option value="ntc" SELECT_NTC>Raum-NTC (ADS1115 A1)</option><option value="wlan" SELECT_WLAN>WLAN-Thermometer</option></select><label>IP-Adresse WLAN-Thermometer</label><input name="ip" required value="IP_VALUE"><button>Speichern</button></form><p>WLAN-Ausfall: Raumwert ungueltig, keine automatische Umschaltung auf NTC.</p><p><a href="/">Zurueck zur Uebersicht</a></p></main></html>)HTML";
  page.replace("SELECT_NTC",roomSource.source==RoomSource::NTC?"selected":"");
  page.replace("SELECT_WLAN",roomSource.source==RoomSource::WLAN?"selected":"");
  page.replace("IP_VALUE",roomSource.ip);
  server.send(200,"text/html; charset=utf-8",page);
});
server.on("/api/sensors",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",sensorJson());});
server.on("/api/status",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",json());});
server.on("/api/optimization",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",optimizationJson());});
server.on("/api/events",HTTP_GET,[]{eventSendJson();});
server.on("/api/events.csv",HTTP_GET,[]{eventSendCsv();});
 server.on("/api/events/reset",HTTP_POST,[]{
   if(historyFS){
     if(LittleFS.exists("/events.csv") && !LittleFS.remove("/events.csv")){server.send(500,"text/plain","Ereignisdatei konnte nicht geloescht werden");return;}
     if(LittleFS.exists("/events-old.csv") && !LittleFS.remove("/events-old.csv")){server.send(500,"text/plain","Alte Ereignisdatei konnte nicht geloescht werden");return;}
   }
   eventHead=0;eventCount=0;eventFileErrors=0;
   server.send(200,"application/json","{\"ok\":true}");
 });
 server.on("/api/history/reset",HTTP_POST,[]{
   if(historyFS){
     if(LittleFS.exists(HISTORY_FILE) && !LittleFS.remove(HISTORY_FILE)){server.send(500,"text/plain","Statistikdatei konnte nicht geloescht werden");return;}
     if(LittleFS.exists("/history.tmp") && !LittleFS.remove("/history.tmp")){server.send(500,"text/plain","Statistik-Zwischendatei konnte nicht geloescht werden");return;}
   }
   historyHead=0;historyCount=0;historyWrites=0;historyWriteErrors=0;
   burnerStarts=0;burnerSeconds=0;pumpSeconds=0;simulationSeconds=0;
   prefs.putUInt("burnStarts",0);prefs.putUInt("burnSecs",0);
   prefs.putUInt("pumpSecs",0);prefs.putUInt("simSecs",0);
   lastHistoryMs=millis();
   server.send(200,"application/json","{\"ok\":true}");
 });
// Explicit, one-time initialization only. Never format during boot or OTA.
server.on("/api/storage/initialize",HTTP_POST,[]{
  if(historyFS){server.send(409,"application/json",R"({"ok":false,"error":"already_mounted"})");return;}
  if(!server.hasArg("confirm") || server.arg("confirm")!="INITIALIZE-LITTLEFS"){
    server.send(403,"application/json",R"({"ok":false,"error":"confirmation_required","confirm":"INITIALIZE-LITTLEFS"})");return;
  }
  const esp_partition_t* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if(!p || p->address!=0xC90000 || p->size!=0x360000){
    server.send(409,"application/json",R"({"ok":false,"error":"partition_layout_mismatch"})");return;
  }
  // Arduino LittleFS formats only on mount failure when explicitly requested here.
  bool mounted=LittleFS.begin(true);
  if(!mounted){server.send(500,"application/json",R"({"ok":false,"error":"initialization_failed"})");return;}
  historyFS=true;
  eventLog("STORAGE","LittleFS nach ausdruecklicher Bestaetigung initialisiert");
  server.send(200,"application/json",R"({"ok":true,"mounted":true,"note":"Previous RAM-only history was not imported"})");
});
// Exact CSV file export (not the filtered in-memory history API).
server.on("/api/storage/export",HTTP_GET,[]{
  if(!historyFS){server.send(503,"text/plain","LittleFS not mounted");return;}
  String name=server.arg("file");
  if(name!="history.csv" && name!="events.csv" && name!="events-old.csv" && name!="import-history.csv" && name!="import-events.csv"){
    server.send(400,"text/plain","invalid file");return;
  }
  String path="/"+name;
  File f=LittleFS.open(path,"r");
  if(!f){server.send(404,"text/plain","file not found");return;}
  server.sendHeader("Cache-Control","no-store");
  server.sendHeader("Content-Disposition","attachment; filename="+name);
  server.streamFile(f,"text/csv");f.close();
});
// Upload old CSVs into separate archive files, never overwrite live logs.
// Each request appends at most 1024 bytes; the offset must match the current size.
server.on("/api/storage/import-archive",HTTP_POST,[]{
  if(!historyFS){server.send(503,"text/plain","LittleFS not mounted");return;}
  String kind=server.arg("file");
  if(kind!="history" && kind!="events"){
    server.send(400,"text/plain","invalid file");return;
  }
  String raw=server.arg("offset");
  if(raw.isEmpty()||raw.length()>9){server.send(400,"text/plain","invalid offset");return;}
  for(size_t i=0;i<raw.length();i++)if(raw[i]<'0'||raw[i]>'9'){
    server.send(400,"text/plain","invalid offset");return;
  }
  String payload=server.arg("data");
  if(payload.isEmpty()||payload.length()>1024){server.send(413,"text/plain","invalid chunk size");return;}
  String path="/import-"+kind+".csv";
  File current=LittleFS.open(path,"r");
  size_t currentSize=current?current.size():0;
  if(current)current.close();
  if((size_t)strtoul(raw.c_str(),nullptr,10)!=currentSize){
    server.send(409,"text/plain",String("offset mismatch, expected ")+currentSize);return;
  }
  File f=LittleFS.open(path,"a");
  if(!f){server.send(500,"text/plain","archive open failed");return;}
  size_t written=f.write((const uint8_t*)payload.c_str(),payload.length());f.close();
  if(written!=payload.length()){server.send(500,"text/plain","archive write failed");return;}
  server.send(200,"application/json",String(R"({"ok":true,"bytes":)")+String(currentSize+written)+"}");
});
// Read-only archive preflight. Archive bytes are never modified.
server.on("/api/storage/restore-check",HTTP_GET,[]{
  if(!historyFS){server.send(503,"application/json",R"({"ok":false,"error":"littlefs_unmounted"})");return;}
  String out="{\"ok\":true,\"liveHistoryBytes\":";
  File live=LittleFS.open("/history.csv","r");out+=String(live?live.size():0);if(live)live.close();
  live=LittleFS.open("/events.csv","r");out+=",\"liveEventsBytes\":"+String(live?live.size():0);if(live)live.close();
  for(const char* kind: {"history","events"}){
    const bool isEvent=strcmp(kind,"events")==0;
    String path=String("/import-")+kind+".csv";
    File f=LittleFS.open(path,"r");
    uint32_t rows=0,invalid=0,nine=0,ten=0,undated=0;
    String firstRow="";
    if(f){while(f.available()){
      String line=f.readStringUntil('\n');line.trim();if(line.isEmpty())continue;
      if(firstRow.isEmpty())firstRow=line.substring(0,160);
      int fields=1;for(size_t i=0;i<line.length();i++)if(line[i]==(isEvent?';':','))fields++;
      if(isEvent ? fields!=4 : (fields!=9 && fields!=10))invalid++;
      else {rows++;if(fields==9)nine++;if(fields==10)ten++;
        unsigned long ts=strtoul(line.c_str(),nullptr,10);if(ts<1577836800UL)undated++;
      }
      if((rows+invalid)%50==0)yield();
    }f.close();}
    out+=",\""+String(kind)+"\":{\"present\":"+String(LittleFS.exists(path)?"true":"false")+
      ",\"rows\":"+String(rows)+",\"invalid\":"+String(invalid)+
      ",\"nineColumns\":"+String(nine)+",\"tenColumns\":"+String(ten)+
      ",\"withoutTime\":"+String(undated)+"}";
  }
  out+=",\"liveMergeEnabled\":false,\"reason\":\"10-column archive schema needs mapping to 9-column live format\"}";
  server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
});
// Read-only combined archive preview. No live file is ever replaced here.
server.on("/api/storage/recovery-preview",HTTP_GET,[]{
  if(!historyFS){server.send(503,"application/json",R"({"ok":false,"error":"littlefs_unmounted"})");return;}
  String out="{\"ok\":true,\"mode\":\"read_only\",\"files\":{";
  bool first=true;
  for(const char *name : {"history.csv","events.csv","import-history.csv","import-events.csv"}){
    if(!first)out+=",";
    first=false;
    File f=LittleFS.open(String("/")+name,"r");
    out+="\""+String(name)+"\":{\"exists\":"+String(f?"true":"false")+",\"bytes\":"+String(f?f.size():0)+"}";
    if(f)f.close();
  }
  out+="},\"activeFilesUnchanged\":true}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",out);
});
server.on("/api/storage/diagnostics",HTTP_GET,[]{
  // Read-only diagnostics. Never mount with auto-format or erase flash.
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  String out="{\"mounted\":"+String(historyFS?"true":"false");
  out+=",\"partitionFound\":"+String(part?"true":"false");
  if(part){out+=",\"partitionAddress\":"+String(part->address)+",\"partitionSize\":"+String(part->size);}
  if(historyFS){
    out+=",\"totalBytes\":"+String(LittleFS.totalBytes())+",\"usedBytes\":"+String(LittleFS.usedBytes());
    for(const char* path : {"/history.csv","/events.csv","/events-old.csv"}){
      String key=String(path).substring(1);key.replace(".","_");key.replace("-","_");
      File f=LittleFS.open(path,"r");
      out+=",\""+key+"Exists\":"+String(f?"true":"false");
      if(f){out+=",\""+key+"Bytes\":"+String(f.size());f.close();}
    }
  }
  out+="}";
  server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
});
// Read-only 1 KiB binary chunks for a full off-device filesystem backup.
// No filesystem mount, erase, write, or background scan is performed.
server.on("/api/storage/backup/info",HTTP_GET,[]{
  const esp_partition_t* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if(!p){server.send(404,"application/json",R"({"error":"partition_not_found"})");return;}
  String out=String(R"({"address":)")+String(p->address)+R"(,"size":)"+String(p->size)+R"(,"chunkSize":1024,"readOnly":true})";
  server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
});
server.on("/api/storage/backup/chunk",HTTP_GET,[]{
  const esp_partition_t* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if(!p){server.send(404,"text/plain","partition not found");return;}
  String raw=server.arg("offset");
  if(!server.hasArg("offset") || raw.isEmpty()){
    server.send(400,"text/plain","missing offset");return;
  }
  for(size_t i=0;i<raw.length();i++)if(raw[i]<'0'||raw[i]>'9'){
    server.send(400,"text/plain","invalid offset");return;
  }
  uint32_t offset=(uint32_t)strtoul(raw.c_str(),nullptr,10);
  if(offset>=p->size || (offset%1024)!=0 || raw.length()>10){
    server.send(416,"text/plain","offset outside partition or unaligned");return;
  }
  uint8_t buf[1024];
  size_t n=(p->size-offset)<sizeof(buf)?(p->size-offset):sizeof(buf);
  if(esp_partition_read(p,offset,buf,n)!=ESP_OK){
    server.send(500,"text/plain","flash read failed");return;
  }
  server.sendHeader("Cache-Control","no-store");
  server.setContentLength(n);
  server.send(200,"application/octet-stream","");
  server.sendContent((const char*)buf,n);
});
server.on("/api/storage/flash-probe",HTTP_GET,[]{
  // Read-only flash inspection; never erase, write, format, or mount.
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if(!part){server.send(404,"application/json","{\"error\":\"partition_not_found\"}");return;}
  const uint32_t offsets[]={0,4096,part->size>=4096?part->size-4096:0};
  String out="{\"partitionAddress\":"+String(part->address)+",\"partitionSize\":"+String(part->size)+",\"samples\":[";
  for(size_t i=0;i<3;i++){
    if(i)out+=",";
    uint8_t data[64]={0};
    esp_err_t err=esp_partition_read(part,offsets[i],data,sizeof(data));
    out+="{\"offset\":"+String(offsets[i])+",\"readOk\":"+String(err==ESP_OK?"true":"false");
    if(err==ESP_OK){
      unsigned ff=0,zero=0;
      String hex;hex.reserve(128);
      static const char digits[]="0123456789ABCDEF";
      for(uint8_t b:data){if(b==0xFF)ff++;if(b==0)zero++;hex+=digits[b>>4];hex+=digits[b&15];}
      out+=",\"ffBytes\":"+String(ff)+",\"zeroBytes\":"+String(zero)+",\"hex64\":\""+hex+"\"";
    }
    out+="}";
  }
  out+="]}";
  server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
});
server.on("/api/history/status",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",historyStatusJson());});
server.on("/api/history",HTTP_GET,[]{historySendJson();});
server.on("/api/history.csv",HTTP_GET,[]{historyCsv();});
 server.on("/api/oilrate",HTTP_POST,[]{
if(!server.hasArg("rate")){server.send(400,"text/plain","rate fehlt");
   return;
   }
float v=server.arg("rate").toFloat();
   if(!isfinite(v)||v<0.1f||v>20.f){server.send(400,"text/plain","Ungueltiger Durchsatz");
   return;
   }
oilLitersPerHour=v;
   prefs.putFloat("oilRate",v);
   server.send(200,"application/json",historyStatusJson());
});
server.on("/api/tank",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",tankJson());});
 server.on("/api/tank",HTTP_POST,[]{
String action=server.arg("action"),value=server.arg("liters");
   value.trim();
if(!value.length()){server.send(400,"text/plain","Literwert fehlt");
   return;
   }
char *end=nullptr;
   float liters=strtof(value.c_str(),&end);
if(end==value.c_str() || *end!='\0' || !isfinite(liters)) {server.send(400,"text/plain","Ungueltiger Literwert");
   return;
   }
   if(action=="set"){
if(liters<0 || liters>TANK_CAPACITY_L){server.send(400,"text/plain","Bestand muss zwischen 0 und 4200 Litern liegen");
     return;
     }
float previous=tankLiters;
     bool wasKnown=tankLevelKnown;
     tankLiters=liters;
     tankLevelKnown=true;
     eventLog("OELTANK",String(wasKnown?"Tankbestand korrigiert: ":"Anfangsbestand erfasst: ")+String(liters,1)+" L"+(wasKnown?String(" (vorher ")+String(previous,1)+" L)":String("")));
   }else if(action=="delivery"){
if(!tankLevelKnown){server.send(409,"text/plain","Bitte zuerst Anfangsbestand speichern");
     return;
     }
if(liters<=0 || liters>TANK_CAPACITY_L || tankLiters+liters>TANK_CAPACITY_L){server.send(400,"text/plain","Liefermenge ungueltig oder Tankkapazitaet ueberschritten");
     return;
     }
float previous=tankLiters;
     tankLiters+=liters;
     eventLog("OELLIEFERUNG",String("Heizoellieferung: +")+String(liters,1)+" L; Bestand "+String(previous,1)+" -> "+String(tankLiters,1)+" L");
     tankLastDeliveryL=liters;
     tankDeliveryCount++;
prefs.putFloat("tankLast",tankLastDeliveryL);
     prefs.putUInt("tankCount",tankDeliveryCount);
}else{server.send(400,"text/plain","Unbekannte Aktion");
   return;
   }
prefs.putFloat("tankLiters",tankLiters);
   prefs.putBool("tankKnown",tankLevelKnown);
server.send(200,"application/json",tankJson());
});
 server.on("/touch/calibrate",HTTP_GET,[]{
server.send(200,"text/html; charset=utf-8",R"HTML(<!doctype html><html lang="de"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Touch-Kalibrierung</title><body style="font:18px sans-serif;max-width:650px;margin:30px auto;padding:15px"><h2>HeatControl v0.5.9.2 | Build 0001 – Touch</h2><p>PCF8574-Relaisausgaenge sind am Niedervolt-Pruefstand automatisch freigegeben. Nach Start drei Fadenkreuze auf dem Display antippen. Jeder Punkt erfordert ein neues Antippen.</p><form action="/api/touch/start" method="post"><button style="padding:15px">Kalibrierung starten</button></form><p>Wenn die Bedienung schlechter wird:</p><form action="/api/touch/reset" method="post"><button style="padding:15px">Kalibrierung zuruecksetzen</button></form><p><a href="/">Zurueck zu HeatControl</a></p></body></html>)HTML");
});
server.on("/api/touch/start",HTTP_POST,[]{touchCalRequested=true;server.sendHeader("Location","/touch/calibrate");server.send(303,"text/plain","Kalibrierung gestartet");});
server.on("/api/touch/reset",HTTP_POST,[]{touchCalResetRequested=true;server.sendHeader("Location","/touch/calibrate");server.send(303,"text/plain","Kalibrierung zurueckgesetzt");});
server.on("/api/relay",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",relayDiagnosticJson());});
 server.on("/api/relay/test",HTTP_POST,[]{
   // Achtung: NUR Niedervolt-Testaufbau. Kein echter Brenner angeschlossen!
   if(!server.hasArg("pin") || (server.arg("pin")!="0" && server.arg("pin")!="1")){
server.send(400,"text/plain","pin=0 oder pin=1 erforderlich");
     return;
   }
   if(inputMode==0 || !relayTestStart((uint8_t)server.arg("pin").toInt())){
server.send(503,"text/plain","PCF8574 nicht bereit");
     return;
   }
server.send(200,"application/json",relayDiagnosticJson());
});
server.on("/api/relay/off",HTTP_POST,[]{relayBenchStop("Manuell AUS");relayTestStop("Manuell AUS");server.send(200,"application/json",relayDiagnosticJson());});
 server.on("/api/relay/bench/start",HTTP_POST,[]{
if(inputMode==0 || !relayBenchStart()){server.send(409,"text/plain","Nur bei laufender Simulation, Automatik und erkanntem PCF8574 moeglich");
   return;
   }
server.send(200,"application/json",relayDiagnosticJson());
});
 server.on("/relay/test",HTTP_GET,[]{
server.send(200,"text/html; charset=utf-8",R"PAGE(<!doctype html><html lang="de"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HeatControl Relaispruefstand</title><style>body{background:#142131;color:#f0f3f8;font:18px system-ui;max-width:720px;margin:35px auto;padding:16px}button{padding:15px;margin:6px;background:#284766;color:white;border:1px solid #7792aa;border-radius:8px}pre{white-space:pre-wrap;background:#24354b;padding:14px;border-radius:8px}</style><h2>HeatControl v0.5.9.2 | Build 0001 – PCF8574 Niedervolt-Pruefstand</h2><p>Nur fuer getrennten Niedervolt-Testaufbau. Keine Verbindung zur Heizung oder Netzspannung! Jeder Test dauert hoechstens zwei Sekunden. P0 und P1 schalten aktiv LOW.</p><button onclick="pulse(0)">P0 – 2 Sekunden</button><button onclick="pulse(1)">P1 – 2 Sekunden</button><button onclick="off()">Sofort AUS</button><p><button onclick="bench()">Pruefstand-Automatik dauerhaft EIN</button></p><p>Nur bei laufender Simulation und aktivierter Automatik. Nach jedem Neustart automatisch freigegeben; ohne Simulation bleiben P0/P1 AUS. Manuelles AUS bis zum naechsten Neustart.</p><pre id="diag">Lade...</pre><a style="color:#a9d5ff" href="/">Zurueck</a><script>async function post(url,body){try{const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!r.ok)alert(await r.text());}catch(e){alert(e.message)}refresh()}async function pulse(n){await post('/api/relay/test','pin='+n)}async function off(){await post('/api/relay/off','')}async function bench(){await post('/api/relay/bench/start','')}async function refresh(){try{document.getElementById('diag').textContent=JSON.stringify(await(await fetch('/api/relay',{cache:'no-store'})).json(),null,2)}catch(e){document.getElementById('diag').textContent=e.message}}refresh();setInterval(refresh,600)</script></html>)PAGE");
});
 server.on("/api/inputmode",HTTP_POST,[]{
int mode=server.arg("mode").toInt();
if(mode!=0 && mode!=1){server.send(400,"text/plain","Unbekannte Betriebsart");
   return;
   }
inputMode=(uint8_t)mode;
   simulationRunning=false;
   setVirtualBurner(false);
prefs.putUChar("inputMode",inputMode);
server.send(200,"application/json",json());
});
 server.on("/api/pump",HTTP_POST,[]{
if(server.hasArg("pumpMode"))pumpMode=constrain(server.arg("pumpMode").toInt(),0,2);
if(server.hasArg("pumpOverrun"))pumpOverrunSeconds=constrain(server.arg("pumpOverrun").toInt(),0,1800);
if(server.hasArg("summerCutoff"))summerCutoffC=constrain(server.arg("summerCutoff").toFloat(),10.f,30.f);
if(server.hasArg("frostThreshold"))frostThresholdC=constrain(server.arg("frostThreshold").toFloat(),-10.f,10.f);
prefs.putUChar("pumpMode",pumpMode);
   prefs.putUInt("pumpRun",pumpOverrunSeconds);
prefs.putFloat("pumpSummer",summerCutoffC);
   prefs.putFloat("pumpFrost",frostThresholdC);
server.send(200,"application/json",json());
});
 server.on("/api/manual",HTTP_POST,[]{
String mode=server.arg("mode");
if(mode=="auto"){manualMode=false;manualBurner=false;manualPump=false;automatic=true;setVirtualBurner(false);pumpDemand=false;}
else if(mode=="manual"){manualMode=true;automatic=false;manualBurner=false;manualPump=false;setVirtualBurner(false);pumpDemand=false;}
else if(mode=="set" && manualMode){manualBurner=server.arg("burner")=="1" && (inputMode==0 || sensor[0].valid) && sensor[2].valid && isfinite(boilerTemp) && boilerTemp<75.0f;
if(manualBurner && sensorFaultLatched){sensorFaultLatched=false;eventLog("STOERUNG","Hand-Brenner nach gueltigen Sensorwerten erneut freigegeben");}manualPump=server.arg("pump")=="1";if(manualBurner)manualPump=true;}
else {server.send(400,"text/plain","Ungueltiger Handbefehl");return;}
eventLog("BETRIEB",String("Web: ")+(manualMode?"Handbetrieb":"Automatik")+", Brenner "+(manualBurner?"EIN":"AUS")+", Pumpe "+(manualPump?"EIN":"AUS"));
server.send(200,"application/json",json());
});
server.on("/api/config",HTTP_POST,[]{
if(inputMode==0 && server.hasArg("outside")){virtualOutside=constrain(server.arg("outside").toFloat(),-30.f,45.f);prefs.putFloat("simOutside",virtualOutside);}
if(inputMode==0 && server.hasArg("room")){virtualRoom=constrain(server.arg("room").toFloat(),0.f,45.f);prefs.putFloat("simRoom",virtualRoom);}
if(inputMode==0 && server.hasArg("boiler")){virtualBoiler=constrain(server.arg("boiler").toFloat(),0.f,110.f);prefs.putFloat("simBoiler",virtualBoiler);}
applyPotiTemperatures();
if(server.hasArg("slope"))slope=constrain(server.arg("slope").toFloat(),0.2f,3.f);
if(server.hasArg("hysteresis"))hysteresis=constrain(server.arg("hysteresis").toFloat(),2.f,15.f);
if(server.hasArg("minBurnerOn"))minBurnerOnSeconds=constrain(server.arg("minBurnerOn").toInt(),0,600);
if(server.hasArg("minBurnerOff"))minBurnerOffSeconds=constrain(server.arg("minBurnerOff").toInt(),0,600);
if(server.hasArg("targetRoom")){comfortRoom=constrain(server.arg("targetRoom").toFloat(),10.f,28.f);
   prefs.putFloat("comfort",comfortRoom);
   }
automatic=server.arg("automatic")=="1";manualMode=false;manualBurner=false;manualPump=false;
prefs.putFloat("slope",slope);
   prefs.putFloat("hyst",hysteresis);
   prefs.putUInt("minOn",minBurnerOnSeconds);
   prefs.putUInt("minOff",minBurnerOffSeconds);
server.send(200,"application/json",json());
});
 server.on("/api/sim",HTTP_POST,[]{
 String cmd=server.arg("cmd");
 if(cmd=="pause" || cmd=="stop"){
   simulationRunning=false;
   setVirtualBurner(false);
   pumpDemand=false;
   manualBurner=false;manualPump=false;
   relayBenchStop("Regelung gestoppt");
   relayTestStop("Regelung gestoppt");
   relayUpdate();
   eventLog("FREIGABE","Regelung manuell gestoppt");
 }else if(cmd=="start"){
   if(inputMode!=1){server.send(409,"text/plain","Start nur mit realen Sensoren; Software-Simulation ohne Relaisfreigabe");return;}
   if(!sensor[0].valid || !isfinite(roomTemp) || !sensor[2].valid || !isfinite(boilerTemp) || boilerTemp>=75.0f){
     server.send(409,"text/plain","Start gesperrt: Sensoren oder Kesselgrenzwert pruefen");return;
   }
   if(!clockValid && (scheduleEnabled || holidayEnabled)){
     server.send(409,"text/plain","Start gesperrt: Uhrzeit fuer Schaltprogramm fehlt");return;
   }
   manualBurner=false;manualPump=false;
   sensorFaultLatched=false;
   simulationRunning=true;
   eventLog("FREIGABE","Regelung manuell freigegeben");
 }else{server.send(400,"text/plain","cmd=start oder pause erwartet");return;}
 server.send(200,"application/json",json());
});
 server.on("/api/program",HTTP_GET,[]{server.send(200,"application/json",programJson());});
 server.on("/api/program",HTTP_POST,[]{
if(!server.hasArg("days")){server.send(400,"text/plain","days fehlt");
   return;
   }
   // Kompaktes JSON-Array: exakt 7 Tage x 3 Phasen x 3 Integer, keine externen Parser.
String raw=server.arg("days");
   int values[63],n=0;
   int pos=0;
   while(pos<(int)raw.length() && n<63){
     if(raw[pos]=='-' || isdigit((unsigned char)raw[pos])){
int next=pos+1;
       while(next<(int)raw.length()&&isdigit((unsigned char)raw[next]))next++;
values[n++]=raw.substring(pos,next).toInt();
       pos=next;
}else if(raw[pos]=='['||raw[pos]==']'||raw[pos]==','||isspace((unsigned char)raw[pos]))pos++;
else{server.send(400,"text/plain","ungueltiges JSON");
     return;
     }
   }
if(n!=63){server.send(400,"text/plain","erwartet 63 Werte");
   return;
   }
Phase next[7][3];
   int k=0;
   for(int d=0;d<7;d++)for(int i=0;i<3;i++){
int a=values[k++],b=values[k++],en=values[k++];
     if(a<0||a>1439||b<0||b>1440||(en&&a>=b)||(en!=0&&en!=1)){
server.send(400,"text/plain","ungueltige Phase");
       return;
     }
next[d][i]={(uint16_t)a,(uint16_t)b,en==1};
   }
memcpy(phases,next,sizeof(phases));
scheduleEnabled=server.arg("enabled")=="1";
comfortRoom=constrain(server.arg("comfort").toFloat(),10.f,28.f);
reducedRoom=constrain(server.arg("reduced").toFloat(),10.f,28.f);
saveProgram();
   server.send(200,"application/json",programJson());
});
 server.on("/api/holiday",HTTP_POST,[]{
int64_t a=atoll(server.arg("start").c_str()),b=atoll(server.arg("end").c_str());
bool enabled=server.arg("enabled")=="1";
if(enabled && (a<1700000000LL||b<=a)){server.send(400,"text/plain","ungueltiger Zeitraum");
   return;
   }
holidayStart=a;
   holidayEnd=b;
   holidayEnabled=enabled;
holidayRoom=constrain(server.arg("temp").toFloat(),10.f,20.f);
saveProgram();
   server.send(200,"application/json",programJson());
});
 server.on("/api/wifi",HTTP_POST,[]{
String ssid=server.arg("ssid");
   if(ssid.length()>32){server.send(400,"text/plain","SSID zu lang");
   return;
   }
prefs.putString("ssid",ssid);
if(server.arg("password").length())prefs.putString("wifiPass",server.arg("password"));
server.send(200,"text/plain","gespeichert; bitte neu starten");
});
 server.on("/api/testclock",HTTP_POST,[]{
testClockEnabled=server.arg("enabled")=="1";
testWeekday=constrain(server.arg("day").toInt(),0,6);
testMinute=constrain(server.arg("minute").toInt(),0,1439);
server.send(200,"application/json",programJson());
});
server.begin();
initDisplay();
refreshDisplay();
Serial.println("DISPLAY: setup complete");
adsInitBus();
 // Wichtig: Display vor I2C-Sensorinitialisierung
pollSensors();
 applyPotiTemperatures();
relayInit();
 // PCF8574 erst NACH Display und ADS1115 am gemeinsamen I2C-Bus suchen
}
void loop(){
relaySerial();
relayUpdate();
server.handleClient();
maintainOTA();
roomSource.poll();
if(millis()-lastSensorPoll>=1500){lastSensorPoll=millis();
 pollSensors();
 bool sensorValuesOk=applyPotiTemperatures();
 uint8_t faultMask=(!sensor[0].valid?1:0)|(!isfinite(roomTemp)?2:0)|(!sensor[2].valid?4:0);
 if(previousSensorFaultMask==255){
   eventLog("SENSOREN",faultMask==0?"Alle drei Sensoren gueltig":String("Startfehler: ")+((faultMask&1)?"A0 ":"")+((faultMask&2)?"A1 ":"")+((faultMask&4)?"A2":""));
 }else if(faultMask!=previousSensorFaultMask){
   const uint8_t failed=faultMask & ~previousSensorFaultMask;
   const uint8_t recovered=previousSensorFaultMask & ~faultMask;
   if(failed)eventLog("SENSORFEHLER",String("Ausfall: ")+((failed&1)?"A0 ":"")+((failed&2)?"A1 ":"")+((failed&4)?"A2":""));
   if(recovered)eventLog("SENSOREN",String("Wiederhergestellt: ")+((recovered&1)?"A0 ":"")+((recovered&2)?"A1 ":"")+((recovered&4)?"A2":""));
 }
 previousSensorFaultMask=faultMask;
 if(!sensorValuesOk){setVirtualBurner(false); manualBurner=false; manualPump=false; sensorFaultLatched=true; pumpDemand=false; relayUpdate();}
 }
static uint32_t prevTick=millis();
uint32_t now=millis();
lv_tick_inc(now-prevTick);
 prevTick=now;
lv_timer_handler();
touchCalService();
if(millis()-lastTick>=1000){lastTick=millis();
 evaluateProgram();
 slopeTestTick();
 float set=desiredBoiler();
bool previousDemand=burnerDemand;
   bool sensorsOk=inputMode==0 || (sensor[0].valid && sensor[2].valid && isfinite(roomTemp));
bool permitted=automatic && !manualMode && simulationRunning && (clockValid || (!scheduleEnabled && !holidayEnabled)) && sensorsOk;
if(manualMode){
bool safe=simulationRunning && sensorsOk && isfinite(boilerTemp) && boilerTemp<75.0f;
if(!safe){manualBurner=false; sensorFaultLatched=true;}
setVirtualBurner(safe && !sensorFaultLatched && manualBurner);
}else updateVirtualBurner(set,permitted);
   // Keine Temperatursimulation; Kesseltemperatur ausschliesslich ADS1115.
if(!permitted && !manualMode)setVirtualBurner(false);
if(manualMode){
bool safe=simulationRunning && sensorsOk && isfinite(boilerTemp) && boilerTemp<75.0f;
pumpDemand=simulationRunning && (manualPump || (manualBurner && safe));
pumpHeatRequest=false;pumpFrostRequest=false;
}else updateVirtualPump(permitted);
relayUpdate();
   // Nur Zustandswechsel protokollieren, nicht jede Sekunde.
static bool logInit=false,prevPump=false,prevPermitted=false,prevSim=false;
if(!logInit){logInit=true;
   prevPump=pumpDemand;prevPermitted=permitted;prevSim=simulationRunning;
   eventLog("FREIGABE",permitted?"Automatik aktiv":"Automatik nicht freigegeben");
   eventLog("BRENNER",String("Startzustand ")+(burnerDemand?"EIN":"AUS"));
   eventLog("PUMPE",String("Startzustand ")+(pumpDemand?"EIN":"AUS"));
   }
if(prevSim!=simulationRunning){eventLog("SIMULATION",simulationRunning?"Simulation gestartet":"Simulation pausiert");
   prevSim=simulationRunning;
   }
if(prevPermitted!=permitted){eventLog("FREIGABE",permitted?"Freigabe erteilt":"Freigabe entzogen: Automatik/Simulation/Uhr/Sensoren pruefen");
   prevPermitted=permitted;
   }
if(previousDemand!=burnerDemand){eventLog("BRENNER",String("Virtuelle Anforderung ")+(burnerDemand?"EIN: Kessel unter Einschaltschwelle":"AUS: Sollgrenze erreicht oder Freigabe entzogen")+", Kessel="+String(boilerTemp,1)+", Soll="+String(set,1));
   }
if(prevPump!=pumpDemand){eventLog("PUMPE",String("Virtuelle Anforderung ")+(pumpDemand?"EIN":"AUS")+", Ursache="+pumpReason()+", Frost="+String(pumpFrostRequest?"ja":"nein")+", Nachlauf="+String(pumpOverrunRemaining())+"s");
   prevPump=pumpDemand;
   }
   if(simulationRunning){
simulationSeconds++;
if(burnerDemand)burnerSeconds++;
if(pumpDemand)pumpSeconds++;
if(burnerDemand && !previousDemand)burnerStarts++;
   }
if(lastHistoryMs==0 || millis()-lastHistoryMs>=300000UL){lastHistoryMs=millis();
   historySample();
   prefs.putUInt("burnStarts",burnerStarts);
   prefs.putUInt("burnSecs",burnerSeconds);
   prefs.putUInt("pumpSecs",pumpSeconds);
   prefs.putUInt("simSecs",simulationSeconds);
   }
refreshDisplay();
Serial.printf("ADS1115: I2C %s, Fehler %lu, Wiederherstellungen %lu, OK-Zyklen %lu | AIN0 %.4f V, AIN1 %.4f V, AIN2 %.4f V (Poti-Test/Diagnose)\n",adsReady?"OK":"FEHLER",(unsigned long)adsErrorCount,(unsigned long)adsRecoveryCount,(unsigned long)adsGoodPolls,sensor[0].volts,sensor[1].volts,sensor[2].volts);
Serial.printf("Pumpe(sim) %s | Heizbedarf %s, Frost %s, Sommer %s, Nachlauf %lu s\n",pumpDemand?"EIN":"AUS",pumpHeatRequest?"JA":"NEIN",pumpFrostRequest?"JA":"NEIN",pumpSummerLock?"JA":"NEIN",(unsigned long)pumpOverrunRemaining());
Serial.printf("Aussen %.1f Raum %.1f Kessel %.1f Soll %.1f Brenner(sim) %s | Hyst %.1f C, Min EIN %u s, Min AUS %u s, Sperre %lu s\n",outsideTemp,roomTemp,boilerTemp,set,burnerDemand?"EIN":"AUS",hysteresis,minBurnerOnSeconds,minBurnerOffSeconds,(unsigned long)burnerLockRemaining());
 }
}
