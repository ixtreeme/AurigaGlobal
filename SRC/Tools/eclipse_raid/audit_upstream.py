"""Inventory upstream encounter logic; this is evidence, never a parity claim."""
from pathlib import Path
import argparse,hashlib,json,re

def inspect(source):
    scripts=source/'src/server/scripts/Northrend/IcecrownCitadel'
    files=[]
    for path in sorted(scripts.glob('*.cpp')):
        text=path.read_text(encoding='utf-8-sig')
        def records(pattern):
            return [dict(line=text.count('\n',0,m.start())+1,text=m.group(0).strip()) for m in re.finditer(pattern,text,re.M)]
        files.append(dict(file=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),lines=len(text.splitlines()),
          events=records(r'(?:events|scheduler)\.(?:ScheduleEvent|RescheduleEvent|Repeat|CancelEvent|Schedule|Repeat)\([^;]+;'),
          branches=records(r'[^\n]*(?:IsHeroic\(|Is25ManRaid\(|RAID_MODE|HealthBelowPct|HealthAbovePct|HealthBelowPctDamaged)[^\n]*'),
          scripts=records(r'^(?:class|struct)\s+\w+[^\n]*'),
          spells=records(r'^\s*SPELL_\w+\s*=\s*\d+[^\n]*')))
    return {'upstream':'AzerothCore ICC 3.3.5a, local source snapshot','modes':['10_normal','25_normal','10_heroic','25_heroic'],
      'scope':'Inventory only. No script or mechanic is marked implemented by this report.','files':files}

def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    report=inspect(a.source);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('UPSTREAM_INVENTORY',len(report['files']),'files',sum(f['lines'] for f in report['files']),'lines')
if __name__=='__main__':main()
