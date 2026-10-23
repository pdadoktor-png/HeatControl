#pragma once

#include <Arduino.h>

// Statisches Web-Dashboard. Unveraendert aus main.cpp ausgelagert.
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HeatControl</title><style>
body{font:16px system-ui;background:#111b26;color:#e7eef7;max-width:820px;margin:15px auto;padding:0 12px}section{background:#203247;border-radius:12px;padding:15px;margin:10px 0}input{font-size:16px;max-width:100%;box-sizing:border-box;padding:7px;background:#f8fbff;color:#111}input[type=checkbox]{width:auto}input[type=time]{width:120px}input[type=number]{width:90px}button{padding:10px;background:#e7a94c;border:0;border-radius:8px;font-size:15px;margin:3px;cursor:pointer}nav button{background:#35516b;color:white}nav button.sel{background:#e7a94c;color:#111}.tab{display:none}.tab.sel{display:block}.phase{display:flex;flex-wrap:wrap;align-items:center;gap:8px;padding:7px 0;border-bottom:1px solid #456}label{display:inline-block;margin:5px 8px 5px 0}small{color:#b6c4d3}.warn{color:#ffbe7a}
</style><h2>HeatControl v0.5.9.1</h2><p class="warn">TOUCH-KALIBRIERUNG – PCF8574-I2C-Diagnose und Pruefstand-Automatik</p>
<nav><button onclick="tab('overview')">Übersicht</button><button onclick="tab('curve')">Heizkurve</button><button onclick="tab('pump')">Pumpe</button><button onclick="tab('tank')">Öltank</button><button onclick="tab('stats')">Statistik</button><button onclick="tab('opt');loadOptimization()">Optimierung</button><button onclick="tab('events');loadEvents()">Ereignisse</button><button onclick="tab('sensors')">Sensoren</button><button onclick="tab('schedule')">Schaltuhr</button><button onclick="tab('holiday')">Urlaub</button></nav>
<section id="overview" class="tab sel"><h3>Aktueller Zustand</h3><div id="state">Lade ...</div><h4>Handbetrieb (Niedervolt-Pruefstand)</h4><div id="manualState">Lade ...</div><button onclick="setManualMode('manual')">Handbetrieb</button><button onclick="setManualMode('auto')">Automatik</button><p><button onclick="setManualOutputs(1,1)">Brenner EIN</button><button onclick="setManualOutputs(0,1)">Brenner AUS / Pumpe EIN</button><button onclick="setManualOutputs(0,0)">Alles AUS</button></p><p><small>Nur Testaufbau: Brennerfreigabe bei gueltigen A0/A2 und Kessel unter 75 C. Kein echter Oelbrenner.</small></p><h4>Relais-Diagnose (Niedervolt-Pruefstand)</h4><div id="relayDiag">Lade ...</div></section><p id="feedback" role="status" style="padding:8px 12px;background:#35516b;border-radius:8px">Bereit</p>
<section id="events" class="tab"><h3>Ereignis- und Fehlerprotokoll</h3><p><small>Echte ADS1115-Messwerte und manuelle Tankbuchungen; Brenner- und Pumpenanforderungen sind Steuerbefehle, keine Rueckmeldung eines realen Brennerlaufs. Maximal 500 aktuelle Ereignisse im Web; CSV mit gespeichertem Verlauf.</small></p><label>Filter <select id="eventFilter" onchange="loadEvents()"><option value="">Alle</option><option>BRENNER</option><option>PUMPE</option><option>FREIGABE</option><option>SIMULATION</option><option>SYSTEM</option><option>OELTANK</option><option>OELLIEFERUNG</option></select></label><button onclick="loadEvents()">Aktualisieren</button><a href="/api/events.csv" style="color:#82d8ff">CSV herunterladen</a> <button onclick="resetEvents()">Ereignisse löschen</button><p id="eventInfo"></p><div id="eventRows" style="max-height:450px;overflow:auto"></div></section>
<section id="sensors" class="tab"><h3>Temperatursensoren (ADS1115)</h3><p>NTC-Sensoren: A0 Aussen 5 kOhm, A1 Raum 10 kOhm, A2 Kessel 5 kOhm (Nennwerte bei 25 Grad C). Messwerte werden ueber ca. 20 Sekunden gleitend gemittelt; die Temperaturanzeige der Uebersicht wird alle 20 Sekunden erneuert. Im Sensor-Tab sind aktueller und gemittelter Wert direkt vergleichbar. Kennlinienannahme B=3950 K, noch nicht kalibriert.</p><label>Betriebsart <select id="inputMode"><option value="0">Software-Simulation</option><option value="1">Reale Sensoren</option></select></label><button onclick="saveInputMode()">Betriebsart übernehmen</button><p><small>Die angezeigten Temperaturen stammen von den angeschlossenen Sensoren. Sensorfehler werden ohne Verzoegerung gemeldet. Die unabhaengigen Sicherheitseinrichtungen der Heizung muessen erhalten bleiben.</small></p><div id="sensorState">Lade ...</div></section>
<section id="curve" class="tab"><h3>Heizkurve / Simulation</h3><p>Grafik der tatsächlich verwendeten HeatControl-Formel: Raum-Soll + Steilheit × (Raum-Soll − Außen) + 2 × (Raum-Soll − Raum-Ist), begrenzt auf 25–75 °C.</p>
<svg id="curveSvg" viewBox="0 0 640 315" role="img" aria-label="Heizkurve: Außentemperatur gegen Kesselsolltemperatur" style="width:100%;height:auto;background:#142335;border-radius:10px"></svg><div id="curveLegend" style="padding:8px;color:#b6c4d3"></div><label>Außen °C <input id="outside" type="number" step="0.5"></label><label>Raum °C <input id="room" type="number" step="0.5"></label><label>Kessel °C <input id="boiler" type="number" step="0.5"></label><label>Steilheit <input id="slope" type="number" min="0.2" max="3" step="0.1"></label><label>Hysterese °C <input id="hysteresis" type="number" min="2" max="15" step="0.5"></label><label>Mindestlaufzeit (s) <input id="minBurnerOn" type="number" min="0" max="600" step="1"></label><label>Mindestpause (s) <input id="minBurnerOff" type="number" min="0" max="600" step="1"></label><label><input id="automatic" type="checkbox"> Automatik</label><p><button onclick="saveCurve()">Werte übernehmen</button></p></section>
<section id="tank" class="tab"><h3>Öltankmanagement – Stahltank 4.200 Liter</h3>
<p><small>Leicht bauchiger Stahltank. Füllhöhe wird ohne Peiltabelle NICHT in Liter umgerechnet. Nur manuell eingegebene Literwerte sind maßgeblich. Geschaetzter Oelverbrauch wird nicht automatisch abgezogen.</small></p>
<div id="tankState">Lade Tankdaten ...</div><div style="height:18px;background:#0f1a26;border-radius:9px;overflow:hidden;margin:12px 0"><div id="tankBar" style="height:100%;background:#50b6ab;width:0%"></div></div>
<label>Aktueller Bestand (Liter) <input id="tankLiters" type="number" min="0" max="4200" step="1" placeholder="unbekannt"></label><button onclick="setTankLevel()">Bestand speichern</button>
<p><label>Neue Heizöllieferung (Liter) <input id="tankDelivery" type="number" min="1" max="4200" step="1"></label><button onclick="addTankDelivery()">Lieferung hinzubuchen</button></p>
<p><small>Lieferung nur buchen, wenn ein Anfangsbestand gespeichert wurde. Die Anzahl der Lieferungen und die letzte Menge werden gespeichert; Lieferungen und Bestandsaenderungen werden im Ereignisprotokoll erfasst.</small></p></section>
<section id="stats" class="tab"><h3>Betriebsstatistik – Regelungsanforderungen</h3>
<p class="warn">Laufzeiten und Starts werden aus den Brenner- und Pumpenanforderungen berechnet, nicht aus einer Rueckmeldung der angeschlossenen Geraete. Ein daraus geschaetzter Oelverbrauch wird nicht vom manuell gefuehrten Tankbestand abgezogen. Zaehler werden alle 5 Minuten gespeichert.</p>
<div id="statsSummary">Lade Statistik ...</div>
<label>Zeitraum <select id="statsHours" onchange="loadStats()"><option value="24">24 Stunden</option><option value="168">7 Tage</option></select></label>
<svg id="statsSvg" viewBox="0 0 700 310" style="width:100%;height:auto;background:#142335;border-radius:10px" aria-label="Temperaturverlauf"></svg>
<div id="statsLegend" style="font-size:13px;color:#b6c4d3">Außen · Raum · Kessel</div>
<p><label>Simulierter Öldurchsatz (Liter/Stunde) <input id="oilRateInput" type="number" min="0.1" max="20" step="0.1"></label><button onclick="saveOilRate()">Öldurchsatz speichern</button></p>
<p><a href="/api/history.csv" style="color:#82d8ff">Messwerte als CSV herunterladen</a> <button onclick="resetStatistics()">Statistik zurücksetzen</button></p>
<p><small>Messintervall 5 Minuten, maximal 7 Tage. Mit LittleFS dauerhaft, andernfalls nur bis zum Neustart. Bei fehlender NTP-Uhr sind Zeitstempel 0 und nur die Laufzeit seit Neustart nutzbar.</small></p></section>
<section id="opt" class="tab"><h3>Optimierung – Empfehlungen zur Regelung</h3><p class="warn">Analyse der gemessenen Temperaturen und berechneten Schaltanforderungen. Empfehlungen sind Hinweise, keine bestaetigten Fehlerdiagnosen. Parameter werden nicht automatisch geaendert; kein gemessener Oelverbrauch.</p><div id="optSummary">Lade Analyse ...</div><p id="optAdvice"></p><label for="optHours">Zeitraum:</label> <select id="optHours" onchange="loadOptimization()"><option value="24">24 Stunden</option><option value="168">7 Tage</option></select> <button id="optRefresh" onclick="loadOptimization()">Analyse aktualisieren</button> <span id="optRefreshStatus" role="status" aria-live="polite">Noch nicht aktualisiert</span><p><small>Auswertung des gewaehlten Zeitraums. Nach Neustart oder Ruecksetzen der Simulation werden Zaehlerbrueche erkannt; bei fehlender Uhrzeit ist die Auswertung eingeschraenkt. Start-/Laufzeitdifferenzen werden nur zwischen vorhandenen 5-Minuten-Messpunkten berechnet.</small></p></section>
<section id="pump" class="tab"><h3>Wilo Stratos ECO – Pumpensimulation</h3><p class="warn">Nur virtuelle Pumpenanforderung. Relaisausgaenge in dieser Version gesperrt.</p><label>Betriebsart <select id="pumpMode"><option value="0">Automatik</option><option value="1">Hand EIN (Simulation)</option><option value="2">Hand AUS (Simulation)</option></select></label><label>Nachlauf (Sekunden) <input id="pumpOverrun" type="number" min="0" max="1800" step="1"></label><label>Sommergrenze Außen (°C) <input id="summerCutoff" type="number" min="10" max="30" step="0.5"></label><label>Frostgrenze (°C) <input id="frostThreshold" type="number" min="-10" max="10" step="0.5"></label><button onclick="savePump()">Pumpeneinstellungen speichern</button><div id="pumpState">Lade ...</div><p><small>Bei Simulationsstopp oder ungültigen Sensoren wird die virtuelle Pumpe abgeschaltet. Die Frostschutzanzeige ist keine sicherheitsgerichtete Frostschutzfunktion.</small></p></section>
<section id="schedule" class="tab"><h3>Wochen-Schaltuhr</h3><label><input id="scheduleEnabled" type="checkbox"> Wochenprogramm aktiv</label><p><small>Bis zu drei Komfortphasen pro Wochentag. Außerhalb der Phasen gilt Absenktemperatur.</small></p><label>Komfort °C <input id="comfort" type="number" step="0.5" min="10" max="28"></label><label>Absenkung °C <input id="reduced" type="number" step="0.5" min="10" max="28"></label><p><label>Wochentag <select id="day" onchange="changeDay()"><option value="0">Montag</option><option value="1">Dienstag</option><option value="2">Mittwoch</option><option value="3">Donnerstag</option><option value="4">Freitag</option><option value="5">Samstag</option><option value="6">Sonntag</option></select></label></p><div id="phases"></div><p><button onclick="copyDay()">Diesen Tag auf alle kopieren</button><button onclick="saveSchedule()">Schaltuhr speichern</button></p><h4>Uhrzeit / WLAN</h4><p id="clockstate"></p><label>Heim-WLAN SSID <input id="ssid" placeholder="SSID"></label><label>Passwort <input id="password" type="password" placeholder="nur bei Änderung"></label><button onclick="saveWifi()">WLAN speichern</button><p><small>Uhrzeit über NTP, Zeitzone Deutschland inkl. Sommerzeit. Ohne gültige Uhrzeit sperrt ein aktives Zeitprogramm die virtuelle Brenneranforderung.</small></p><h4>Offline-Testuhr</h4><label><input id="testClock" type="checkbox"> Testuhr verwenden</label><label>Tag <select id="testDay"><option value="0">Mo</option><option value="1">Di</option><option value="2">Mi</option><option value="3">Do</option><option value="4">Fr</option><option value="5">Sa</option><option value="6">So</option></select></label><label>Uhrzeit <input type="time" id="testTime" value="12:00"></label><button onclick="setTestClock()">Testuhr setzen</button></section>
<section id="holiday" class="tab"><h3>Urlaubsmodus</h3><label><input id="holidayEnabled" type="checkbox"> Urlaub aktivieren</label><p><label>Beginn <input type="datetime-local" id="holidayStart"></label><label>Rückkehr / Ende <input type="datetime-local" id="holidayEnd"></label></p><label>Urlaubstemperatur °C <input id="holidayTemp" type="number" min="10" max="20" step="0.5"></label><p><button onclick="saveHoliday()">Urlaub speichern</button><button onclick="cancelHoliday()">Urlaub beenden</button></p><p><small>Zum Rückkehrzeitpunkt wird automatisch wieder das Wochenprogramm verwendet. Der Zeitpunkt ist frei wählbar. Urlaub benötigt eine gültige NTP-Uhr; die Testuhr simuliert nur die Wochen-Schaltuhr.</small></p></section>
<script>
let init=false,program=null,loadedProgram=false,simRunning=false,simBusy=false;const $=id=>document.getElementById(id);
function feedback(msg){let x=document.getElementById('feedback');if(x)x.textContent=msg;}
function tab(id){document.querySelectorAll('.tab').forEach(x=>x.classList.toggle('sel',x.id===id));document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('sel',x.getAttribute('onclick')==="tab('"+id+"')"));}
function drawHeatingCurve(j){
 const svg=$('curveSvg');if(!svg)return;
 const W=640,H=315,L=58,R=20,T=20,B=46;
 const x=t=>L+(t+20)/40*(W-L-R),y=t=>T+(75-t)/50*(H-T-B);
 const sp=(a,room)=>Math.max(25,Math.min(75,j.targetRoom+j.slope*(j.targetRoom-a)));
 let h='';for(let t=25;t<=75;t+=10)h+=`<line x1="${L}" y1="${y(t)}" x2="${W-R}" y2="${y(t)}" stroke="#3a4e61"/><text x="${L-8}" y="${y(t)+4}" text-anchor="end" fill="#c5d3e0" font-size="13">${t}°</text>`;
 for(let t=-20;t<=20;t+=10)h+=`<line x1="${x(t)}" y1="${T}" x2="${x(t)}" y2="${H-B}" stroke="#3a4e61"/><text x="${x(t)}" y="${H-B+20}" text-anchor="middle" fill="#c5d3e0" font-size="13">${t}°</text>`;
 const path=Array.from({length:81},(_,i)=>{let a=-20+i*.5;return(i?'L':'M')+x(a).toFixed(1)+' '+y(sp(a,j.room)).toFixed(1)}).join(' ');
 let a=Math.max(-20,Math.min(20,j.outside)),b=sp(j.outside,j.room);
 h+=`<path d="${path}" fill="none" stroke="#5bd1fa" stroke-width="3"/><circle cx="${x(a)}" cy="${y(b)}" r="6" fill="#ffb454" stroke="#fff" stroke-width="2"/><text x="${W/2}" y="${H-5}" fill="#c5d3e0" text-anchor="middle" font-size="14">Außentemperatur (°C)</text><text x="8" y="14" fill="#c5d3e0" font-size="12">Kesselsoll (°C)</text>`;
 svg.innerHTML=h;$('curveLegend').textContent=`Aktuell: Außen ${j.outside} °C, Raum ${j.room} °C, Soll ${j.setpoint} °C | Steilheit ${j.slope}`;
}
async function loadOptimization(){
 const btn=$('optRefresh'), status=$('optRefreshStatus');
 if(btn){btn.disabled=true;btn.textContent='Analysiere ...';}
 if(status)status.textContent='Daten werden ausgewertet ...';
 try{const r=await fetch('/api/optimization?hours='+$('optHours').value,{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const j=await r.json();
 $('optSummary').textContent=`Zeitraum: ${j.period} | Messpunkte: ${j.samples} | Fenster: ${(j.windowSeconds/3600).toFixed(2)} h | Brennerstarts: ${j.burnerStarts} | Starts/h: ${j.startsPerHour.toFixed(2)} | Brenneranteil: ${j.burnerDutyPercent.toFixed(1)} % | Brenner: ${(j.burnerSeconds/3600).toFixed(2)} h | Pumpe: ${(j.pumpSeconds/3600).toFixed(2)} h | Öl (geschätzt aus Anforderung): ${j.estimatedOilLiters.toFixed(2)} L | Mittel Außen/Raum/Kessel: ${j.averageOutside}/${j.averageRoom}/${j.averageBoiler} °C`;
 $('optAdvice').textContent=j.advice;
 if(status)status.textContent='Aktualisiert: '+new Date().toLocaleTimeString('de-DE')+(j.sufficientData?' · Auswertung abgeschlossen':' · Noch zu wenig Messdaten');
 }catch(e){$('optSummary').textContent='Analysefehler: '+e.message;if(status)status.textContent='Aktualisierung fehlgeschlagen';}
 finally{if(btn){btn.disabled=false;btn.textContent='Analyse aktualisieren';}}
}
async function loadStats(){
 try{
  const j=await(await fetch('/api/history/status',{cache:'no-store'})).json();
  $('statsSummary').textContent=`Speicher: ${j.storage} | Punkte: ${j.samples} | Brennerstarts (Anforderung): ${j.burnerStarts} | Brennerlaufzeit: ${(j.burnerSeconds/3600).toFixed(2)} h | Pumpenlaufzeit: ${(j.pumpSeconds/3600).toFixed(2)} h | Öl rechnerisch: ${j.estimatedLiters.toFixed(2)} L | Schreibfehler: ${j.writeErrors}`;
  if(document.activeElement!==$('oilRateInput'))$('oilRateInput').value=j.oilRate;
  const data=await(await fetch('/api/history?hours='+$('statsHours').value,{cache:'no-store'})).json();
  const svg=$('statsSvg'), W=700,H=310,L=45,R=16,T=20,B=40;
  const x=i=>L+(i/Math.max(1,data.length-1))*(W-L-R), y=t=>T+(100-t)/120*(H-T-B);
  let out='';for(let t=-20;t<=100;t+=20)out+=`<line x1="${L}" y1="${y(t)}" x2="${W-R}" y2="${y(t)}" stroke="#3a4e61"/><text x="${L-7}" y="${y(t)+4}" text-anchor="end" fill="#c5d3e0" font-size="12">${t}°</text>`;
  for(const [key,color] of [['a','#56c8ff'],['r','#69df94'],['b','#ffb454']]){
   if(data.length)out+=`<polyline points="${data.map((p,i)=>x(i).toFixed(1)+','+y(p[key]).toFixed(1)).join(' ')}" fill="none" stroke="${color}" stroke-width="2"/>`;
  }
  if(data.length>1){const n=data.length;for(let i=0;i<5;i++){const ix=Math.round(i*(n-1)/4),p=data[ix];const stamp=p.t?new Date(p.t*1000).toLocaleString('de-DE',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'}):'+'+Math.round(p.u/60)+'m';out+=`<text x="${x(ix)}" y="${H-10}" text-anchor="middle" fill="#c5d3e0" font-size="11">${stamp}</text>`;}}
  svg.innerHTML=out;
  $('statsLegend').textContent=`Außen (blau) · Raum (grün) · Kessel (orange) | ${data.length} Messpunkte`;
 }catch(e){$('statsSummary').textContent='Statistik nicht verfügbar: '+e.message;}
}
async function saveOilRate(){if(await post('/api/oilrate',{rate:$('oilRateInput').value}))await loadStats()}

async function resetEvents(){
 if(!confirm('Alle Ereignisse dauerhaft loeschen, auch den gespeicherten CSV-Verlauf? Tankbestand und Statistik bleiben erhalten.'))return;
 try{const r=await fetch('/api/events/reset',{method:'POST'});if(!r.ok)throw Error(await r.text());feedback('Ereignisprotokoll geloescht');await loadEvents();}
 catch(e){feedback('Loeschen fehlgeschlagen: '+e.message)}
}
async function resetStatistics(){
 if(!confirm('Statistik und Betriebszaehler dauerhaft zuruecksetzen? Ereignisse und Tankbestand bleiben erhalten.'))return;
 try{const r=await fetch('/api/history/reset',{method:'POST'});if(!r.ok)throw Error(await r.text());feedback('Statistik zurueckgesetzt');await loadStats();await loadOptimization();}
 catch(e){feedback('Zuruecksetzen fehlgeschlagen: '+e.message)}
}
async function loadEvents(){try{const j=await(await fetch('/api/events',{cache:'no-store'})).json();const filter=$('eventFilter').value;const rows=j.events.filter(e=>!filter||e.type===filter).reverse();$('eventInfo').textContent=rows.length+' Ereignisse | Speicher: '+j.storage+' | Schreibfehler: '+j.writeErrors;$('eventRows').replaceChildren();for(const e of rows){const div=document.createElement('div');div.style.cssText='padding:7px;border-bottom:1px solid #456';const stamp=e.t?new Date(e.t*1000).toLocaleString('de-DE'):'Uptime '+e.uptime+' s';div.textContent=stamp+' | '+e.type+' | '+e.detail;$('eventRows').appendChild(div)}}catch(e){$('eventInfo').textContent='Fehler: '+e.message}}
async function loadTank(){try{const j=await(await fetch('/api/tank',{cache:'no-store'})).json();$('tankState').textContent=j.levelKnown?`${j.liters.toFixed(0)} / ${j.capacity.toFixed(0)} Liter (${j.percent.toFixed(1)} %) | letzte Lieferung: ${j.lastDelivery.toFixed(0)} L | Buchungen: ${j.deliveryCount}`:'Tankbestand noch nicht erfasst';$('tankBar').style.width=(j.levelKnown?j.percent:0)+'%';}catch(e){$('tankState').textContent='Tankdaten nicht verfügbar: '+e.message}}
async function setTankLevel(){if(await post('/api/tank',{action:'set',liters:$('tankLiters').value}))await loadTank()}
async function addTankDelivery(){if(!confirm('Heizöllieferung zum gespeicherten Bestand addieren?'))return;if(await post('/api/tank',{action:'delivery',liters:$('tankDelivery').value})){ $('tankDelivery').value='';await loadTank()}}
async function load(){try{let j=await(await fetch('/api/status',{cache:'no-store'})).json();simRunning=!!j.simulationRunning;const shown=displayedTemperatures(j);$('manualState').textContent=j.manualMode?'HAND – Brenner '+(j.burnerDemand?'EIN':'AUS')+' / Pumpe '+(j.pumpDemand?'EIN':'AUS'):'AUTOMATIK';$('state').innerHTML=`Außen: ${shown.outside} °C<br>Raum: ${shown.room} °C<br>Kessel: ${shown.boiler} °C<br>Heizkurve Soll: ${j.setpoint} °C<br>Heizkurven-Bezugstemperatur: ${j.targetRoom} °C<br>Brenner virtuell: <b>${j.burnerDemand?'EIN':'AUS'}</b><br>Pumpe virtuell: <b>${j.pumpDemand?'EIN':'AUS'}</b> – ${j.pumpReason}${j.pumpReason==='Pumpennachlauf'?' ('+j.pumpOverrunRemaining+' s)':''}<br>Mindestlaufzeit: ${j.minBurnerOn} s / Mindestpause: ${j.minBurnerOff} s<br>Schaltsperre verbleibend: ${j.burnerLockRemaining} s<br>Sensorbetrieb: ${j.inputMode===1?'Reale Sensoren':'Software'}<br>Regelung: ${j.simulationRunning?'AKTIV':'GESPERRT'}<br>Brennerhinweis: <b>${j.burnerBlockReason}</b><br>Uhrzeit: ${j.clockText||'--:--'}<br>Urlaub: ${j.holidayActive?'AKTIV':'inaktiv'}<br>Brennerstarts: ${j.burnerStarts}<br>Brennerlaufzeit: ${(j.burnerSeconds/3600).toFixed(2)} h<br>Ölverbrauch (geschätzt): ${j.oilEstimate} l bei ${j.oilRate} l/h<br>Betriebsdauer: ${j.simulationSeconds} s`; if(document.activeElement!==$('inputMode'))$('inputMode').value=j.inputMode;if(!init){for(let k of ['outside','room','boiler','slope','hysteresis','minBurnerOn','minBurnerOff'])$(k).value=j[k];$('automatic').checked=j.automatic;for(let k of ['pumpMode','pumpOverrun','summerCutoff','frostThreshold'])$(k).value=j[k];init=true}$('pumpState').innerHTML='Virtuelle Pumpe: <b>'+(j.pumpDemand?'EIN':'AUS')+'</b> – '+j.pumpReason+'<br>Witterungsheizbetrieb: '+(j.pumpHeatRequest?'JA':'NEIN')+'<br>Frostanforderung: '+(j.pumpFrostRequest?'JA':'NEIN')+'<br>Sommergrenze erreicht: '+(j.pumpSummerLock?'JA':'NEIN')+'<br>Nachlauf verbleibend: '+j.pumpOverrunRemaining+' s';if(!loadedProgram){await loadProgram();loadedProgram=true}await loadSensors();await loadRelayDiagnostics();drawHeatingCurve(j);await loadTank();if($("stats").classList.contains("sel"))await loadStats()}catch(e){$('state').textContent='Verbindung fehlgeschlagen: '+e}}
async function loadRelayDiagnostics(){try{const r=await fetch('/api/relay',{cache:'no-store'});const j=await r.json();$('relayDiag').textContent='PCF8574 (0x20): '+(j.present?'ERKANNT':'NICHT ANGESCHLOSSEN')+' | ADS1115: 0x48 | Brenneranforderung: '+(j.burnerDemand?'EIN':'AUS')+' | Pumpenanforderung: '+(j.pumpDemand?'EIN':'AUS')+' | Relais: '+(j.outputsEnabled?'PRUEFSTAND':'GESPERRT')+' | I2C-Fehler: '+j.errors;}catch(e){$('relayDiag').textContent='Diagnose nicht verfuegbar: '+e.message;}}
let visibleTemperatures=null,visibleTemperaturesAt=0;
function displayedTemperatures(j){const now=Date.now();if(!visibleTemperatures||now-visibleTemperaturesAt>=20000||[j.outside,j.room,j.boiler].some(v=>v===null||v===undefined||!Number.isFinite(Number(v)))){visibleTemperatures={outside:j.outside,room:j.room,boiler:j.boiler};visibleTemperaturesAt=now;}return visibleTemperatures;}
async function loadSensors(){
 try{
  const r=await fetch('/api/sensors',{cache:'no-store'});
  if(!r.ok)throw Error(r.status);
  const j=await r.json(),names=['Aussen','Raum','Kessel'];
  $('sensorState').innerHTML='ADS1115: '+(j.connected?'verbunden':'nicht gefunden')+
    ' | I2C Fehler: '+j.i2cErrors+' | Wiederherstellungen: '+j.recoveries+
    '<br><small>Aktuell = letzter Messwert, Mittel = gleitender Mittelwert aus ca. 20 Sekunden.</small><br>'+
    j.channels.map((c,i)=>names[i]+': '+(c.valid
      ?'Aktuell <b>'+Number(c.tempCurrent).toFixed(1)+' °C</b> | Mittel <b>'+Number(c.tempApprox).toFixed(1)+' °C</b> | '+Number(c.volts).toFixed(3)+' V / '+Number(c.ohms).toFixed(0)+' Ohm'
      :'Sensorfehler / ungueltig')).join('<br>');
 }catch(e){$('sensorState').textContent='Sensorabfrage fehlgeschlagen: '+e.message}
}
async function heatControlSimCommand(cmd){
 if(simBusy)return;
 simBusy=true;
 const button=$('simButton');button.disabled=true;
 const action=cmd|| (simRunning?'pause':'start');
 feedback('Befehl '+action+' wird ausgeführt ...');
 try{
  let r=await fetch('/api/sim?cmd='+encodeURIComponent(action)+'&_='+Date.now(),{method:'POST',cache:'no-store'});
  if(!r.ok)throw Error(await r.text());
  let j=await r.json();simRunning=!!j.simulationRunning;
  if(action==='pause'&&simRunning)throw Error('ESP32 meldet weiterhin LAEUFT');
  if(action==='start'&&!simRunning)throw Error('ESP32 meldet weiterhin PAUSE');
  feedback(action==='reset'?'Simulation zurückgesetzt':(simRunning?'Simulation gestartet':'Simulation pausiert'));
  await load();
 }catch(e){feedback('Fehler: '+e.message)}
 finally{simBusy=false;button.disabled=false;}
}
async function post(path,data){let r=await fetch(path,{method:'POST',body:new URLSearchParams(data)});if(!r.ok){feedback('Fehler: '+await r.text());return false}feedback('Einstellung gespeichert');return true}
async function saveInputMode(){if(await post('/api/inputmode',{mode:$('inputMode').value})){feedback('Betriebsart gespeichert');await load()}}
async function savePump(){let d={};for(let k of ['pumpMode','pumpOverrun','summerCutoff','frostThreshold'])d[k]=$(k).value;if(await post('/api/pump',d))await load()}
async function setManualMode(mode){if(await post('/api/manual',{mode}))await load()}
async function setManualOutputs(burner,pump){if(await post('/api/manual',{mode:'set',burner:String(burner),pump:String(pump)}))await load()}
async function saveCurve(){let d={};for(let k of ['outside','room','boiler','slope','hysteresis','minBurnerOn','minBurnerOff'])d[k]=$(k).value;d.automatic=$('automatic').checked?'1':'0';if(await post('/api/config',d)){await load();const r=await fetch('/api/status',{cache:'no-store'});if(r.ok){const j=await r.json();$('slope').value=j.slope;$('hysteresis').value=j.hysteresis;$('minBurnerOn').value=j.minBurnerOn;$('minBurnerOff').value=j.minBurnerOff;$('automatic').checked=j.automatic;feedback('Heizkurve gespeichert: Steilheit '+j.slope)}}}
const toTime=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');const fromTime=s=>{let a=s.split(':');return +a[0]*60+(+a[1])};
function changeDay(){if(program && currentDay>=0)collectPhases(currentDay);renderPhases();}
let currentDay=-1;
function renderPhases(){if(!program)return;let d=+$('day').value;currentDay=d;$('phases').innerHTML=program.days[d].map((p,i)=>`<div class="phase"><b>Phase ${i+1}</b><label><input type="checkbox" id="en${i}" ${p[2]?'checked':''}> Aktiv</label><label>Von <input type="time" id="from${i}" value="${toTime(p[0])}"></label><label>Bis <input type="time" id="to${i}" value="${toTime(p[1])}"></label></div>`).join('')}
function collectPhases(day){let d=day===undefined?+$('day').value:day;for(let i=0;i<3;i++)program.days[d][i]=[fromTime($('from'+i).value),fromTime($('to'+i).value),$('en'+i).checked?1:0]}
function copyDay(){collectPhases();let d=+$('day').value;for(let i=0;i<7;i++)program.days[i]=program.days[d].map(x=>x.slice());feedback('Tagesprogramm kopiert – bitte speichern.')}
async function loadProgram(){currentDay=-1;program=await(await fetch('/api/program')).json();$('scheduleEnabled').checked=program.scheduleEnabled;$('holidayEnabled').checked=program.holidayEnabled;$('comfort').value=program.comfort;$('reduced').value=program.reduced;$('holidayTemp').value=program.holidayTemp;$('clockstate').textContent=program.clockValid?'Uhrzeit gültig':'Keine gültige Uhrzeit';$('testClock').checked=program.testClock;let d=new Date(program.holidayStart*1000),e=new Date(program.holidayEnd*1000);const fmt=x=>{if(!Number.isFinite(x.getTime())||x.getFullYear()<2020)return '';let z=n=>String(n).padStart(2,'0');return x.getFullYear()+'-'+z(x.getMonth()+1)+'-'+z(x.getDate())+'T'+z(x.getHours())+':'+z(x.getMinutes())};$('holidayStart').value=fmt(d);$('holidayEnd').value=fmt(e);renderPhases()}
async function saveSchedule(){collectPhases();let data={enabled:$('scheduleEnabled').checked?'1':'0',comfort:$('comfort').value,reduced:$('reduced').value,days:JSON.stringify(program.days)};if(await post('/api/program',data)){loadedProgram=false;await load()}}
async function saveHoliday(){let a=$('holidayStart').value,b=$('holidayEnd').value;if($('holidayEnabled').checked&&(!a||!b||new Date(b)<=new Date(a))){feedback('Bitte gültigen Beginn und Rückkehrzeitpunkt angeben.');return}if(await post('/api/holiday',{enabled:$('holidayEnabled').checked?'1':'0',start:a?Math.floor(new Date(a).getTime()/1000):0,end:b?Math.floor(new Date(b).getTime()/1000):0,temp:$('holidayTemp').value})){loadedProgram=false;await load()}}
async function cancelHoliday(){$('holidayEnabled').checked=false;await saveHoliday()}
async function saveWifi(){if(await post('/api/wifi',{ssid:$('ssid').value,password:$('password').value}))alert('WLAN gespeichert. ESP32 neu starten, damit die Verbindung aufgebaut wird.')}
async function setTestClock(){await post('/api/testclock',{enabled:$('testClock').checked?'1':'0',day:$('testDay').value,minute:fromTime($('testTime').value)});loadedProgram=false;await load()}
setInterval(load,2000);load();
</script></html>)HTML";
