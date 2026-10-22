#include <Arduino.h>
#ifndef RELAY_TEST_GPIO_1
#define RELAY_TEST_GPIO_1 12
#endif
#ifndef RELAY_TEST_GPIO_2
#define RELAY_TEST_GPIO_2 13
#endif
#ifndef RELAY_TEST_ENABLE_DRIVE
#define RELAY_TEST_ENABLE_DRIVE 0
#endif
// Relaismodule sind haeufig LOW-aktiv. Vorher am eigenen Modul pruefen!
static constexpr uint8_t RELAY_OFF = HIGH;
static constexpr uint8_t RELAY_ON = LOW;
static constexpr unsigned long MAX_ON_MS=2000;
static bool armed=false;
static unsigned long armedUntil=0, pulseUntil=0;
static int activePin=-1;
static String line;
static void off(){ digitalWrite(RELAY_TEST_GPIO_1,RELAY_OFF);
digitalWrite(RELAY_TEST_GPIO_2,RELAY_OFF);
activePin=-1;
pulseUntil=0;
}
static void help(){Serial.println("Befehle: HELP, STATUS, ARM, 1, 2, OFF, DISARM");
Serial.println("ARM gilt 30 s; 1/2 schalten hoechstens 2 s. Kein automatischer Schaltzyklus.");
}
static void command(String c){c.trim();
c.toUpperCase();
if(c=="HELP")help();
else if(c=="STATUS")Serial.printf("GPIO12=%d GPIO13=%d, Freigabe=%s, Aktiv=%d, Drive=%d\n",digitalRead(RELAY_TEST_GPIO_1),digitalRead(RELAY_TEST_GPIO_2),armed?"JA":"NEIN",activePin,RELAY_TEST_ENABLE_DRIVE);
else if(c=="OFF"||c=="DISARM"){off();
 armed=false;
 Serial.println("Beide AUS, gesperrt");
 }
 else if(c=="ARM"){
#if RELAY_TEST_ENABLE_DRIVE
armed=true;
  armedUntil=millis()+30000;
  off();
  Serial.println("30 Sekunden freigegeben. Befehle 1 oder 2 fuer 2-Sekunden-Puls.");
#else
Serial.println("Nur GPIO-Diagnose: Relaisansteuerung in diesem Build gesperrt.");
#endif
 }
 else if(c=="1"||c=="2"){
#if RELAY_TEST_ENABLE_DRIVE
if(!armed || (int32_t)(millis()-armedUntil)>=0){off();
  armed=false;
  Serial.println("Nicht freigegeben. Zuerst ARM.");
  return;
  }
off();
  activePin=(c=="1"?RELAY_TEST_GPIO_1:RELAY_TEST_GPIO_2);
  digitalWrite(activePin,RELAY_ON);
  pulseUntil=millis()+MAX_ON_MS;
  Serial.printf("GPIO%d LOW fuer maximal 2 Sekunden\n",activePin);
#else
Serial.println("Schalten deaktiviert (RELAY_TEST_ENABLE_DRIVE=0)");
#endif
}else Serial.println("Unbekannter Befehl; HELP");
}
void setup(){
 // Vor dem Umschalten in OUTPUT: HIGH-Latch setzen (typisch LOW-aktives Modul)
digitalWrite(RELAY_TEST_GPIO_1,RELAY_OFF);
 digitalWrite(RELAY_TEST_GPIO_2,RELAY_OFF);
pinMode(RELAY_TEST_GPIO_1,OUTPUT);
 pinMode(RELAY_TEST_GPIO_2,OUTPUT);
 off();
Serial.begin(115200);
 delay(350);
Serial.println("=== HeatControl RELAIS-GPIO-DIAGNOSE (nur Niedervolt!) ===");
Serial.printf("GPIO12 / GPIO13; RELAY_TEST_ENABLE_DRIVE=%d\n",RELAY_TEST_ENABLE_DRIVE);
Serial.println("ACHTUNG: Dieses Testprogramm ersetzt HeatControl voruebergehend.");
 help();
}
void loop(){
if(activePin>=0 && (int32_t)(millis()-pulseUntil)>=0){off();
 Serial.println("Puls beendet: beide AUS");
 }
if(armed && (int32_t)(millis()-armedUntil)>=0){off();
 armed=false;
 Serial.println("ARM abgelaufen: beide AUS");
 }
while(Serial.available()) {char c=(char)Serial.read();
 if(c=='\r'||c=='\n'){if(line.length()){command(line);
 line="";
 }}else if(line.length()<32)line+=c;
 }
delay(5);
}
