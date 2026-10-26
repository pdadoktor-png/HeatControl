#!/usr/bin/env python3
"""Read-only HeatControl recovery preparation: old 10-column -> current 9-column CSV.
Does not write to ESP32, does not change source files.
Old schema: epoch,uptime,outside_C,room_C,boiler_C,burner,pump,starts,burnerSec,pumpSec.
New schema: epoch,uptime,outside_x10,room_x10,boiler_x10,flags,starts,burnerSec,pumpSec.
"""
import argparse,csv,hashlib,json
from pathlib import Path
from decimal import Decimal, ROUND_HALF_UP

p=argparse.ArgumentParser()
p.add_argument("--archive",type=Path,default=Path.home()/"Downloads/HeatControl-Backup/HeatControl-Zusammengefuehrt")
p.add_argument("--live",type=Path,required=True,help="Folder from heatcontrol_csv_backup.py backup")
p.add_argument("--out",type=Path,default=Path("heatcontrol-recovery-review"))
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True)

def read(path,delimiter):
    with path.open(newline="",encoding="utf-8-sig") as f:
        return [r for r in csv.reader(f,delimiter=delimiter) if r and any(v.strip() for v in r)]
def integer(v):
    if not v.strip().isdigit():raise ValueError("not unsigned integer: "+repr(v))
    n=int(v); 
    if n>4294967295:raise ValueError("integer overflow")
    return n
def temp(v):
    n=int((Decimal(v)*10).quantize(Decimal("1"),rounding=ROUND_HALF_UP))
    if not -32768<=n<=32767:raise ValueError("temperature overflow")
    return n
def old_history(r):
    if len(r)!=10:raise ValueError("old history requires 10 fields")
    e,u=map(integer,r[:2]); t=list(map(temp,r[2:5]))
    burner,pump=map(integer,r[5:7])
    if burner not in (0,1) or pump not in (0,1):raise ValueError("burner/pump not binary")
    tail=list(map(integer,r[7:10]))
    return [e,u,*t,burner|(pump<<1),*tail]
def live_history(r):
    if len(r)!=9:raise ValueError("live history requires 9 fields")
    vals=list(map(integer,r[:2]))+[int(v) for v in r[2:5]]+list(map(integer,r[5:]))
    if any(not -32768<=v<=32767 for v in vals[2:5]):raise ValueError("temperature overflow")
    if not 0<=vals[5]<=255:raise ValueError("flags overflow")
    return vals
def event(r):
    if len(r)!=4:raise ValueError("event requires 4 fields")
    return [integer(r[0]),integer(r[1]),r[2],r[3]]
def merge(kind):
    sep=";" if kind=="events" else ","
    old=read(a.archive/(kind+"-merged.csv"),sep)
    live=read(a.live/(kind+".csv"),sep)
    transform=event if kind=="events" else old_history
    transform_live=event if kind=="events" else live_history
    rows=[];seen=set();errors=[]
    for origin,source,fn in (("archive",old,transform),("live",live,transform_live)):
        for idx,r in enumerate(source,1):
            try:
                v=fn(r);key=tuple(map(str,v))
                if key not in seen:seen.add(key);rows.append(v)
            except Exception as ex:errors.append({"origin":origin,"line":idx,"error":str(ex)})
    if errors:raise ValueError(kind+" invalid data: "+json.dumps(errors[:10]))
    valid=[r for r in rows if r[0]>=1577836800]
    undated=[r for r in rows if r[0]<1577836800]
    valid.sort(key=lambda r:(r[0],r[1]))
    for name,data in ((kind+"-review.csv",valid),(kind+"-undated-review.csv",undated)):
        with (a.out/name).open("w",newline="",encoding="utf-8") as f:
            w=csv.writer(f,delimiter=sep,lineterminator="\n");w.writerows(data)
    return {"archive":len(old),"live":len(live),"unique":len(rows),"dated":len(valid),"undated":len(undated),"duplicates":len(old)+len(live)-len(rows)}
summary={k:merge(k) for k in ("history","events")}
(a.out/"review-report.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
print(json.dumps(summary,indent=2))
print("Review only: no ESP32 write; source files unchanged.")
