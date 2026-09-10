from pathlib import Path
import re,sys
p=Path(__file__).with_name('premiumplus_v4825_final.cpp')
s=p.read_text(encoding='utf-8')

def fn(sig):
    a=s.index(sig); q=s.index('{',a); d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[a:i+1]
    raise RuntimeError('unclosed '+sig)

tests=[]
def check(name,cond):
    tests.append((name,bool(cond)))

minor=fn('void MinorWorker()')
rw=fn('void RWorker()')
ws=fn('void WsWorker()')
attack=fn('void AttackWorker()')
mob=fn('void MobSkillWorker()')
echo=fn('void WarriorEchoWorker()')

check('MinorTimerDeferred', 'bool timer1ms=false' in minor and minor.find('timeBeginPeriod(1)')>minor.find('if(!featureReady)'))
check('MinorIdle20ms', 'Sleep(20);continue;' in minor)
check('MinorActiveRatesPreserved', 'g_turbo.load()?240:120' in minor)
check('MinorNativePathPreserved', 'MinorSendInputsLowCpu' in minor)
check('RTimerDeferred', 'bool timer1ms=false' in rw and rw.find('timeBeginPeriod(1)')>rw.find('if(!featureReady)'))
check('RIdle20ms', 'Sleep(20);continue;' in rw)
check('RActiveRatePreserved', 'g_turbo.load()?r.rTurbo:r.rMax' in rw and "ReferenceTapKey('R')" in rw)
check('EchoTimerDeferred', 'bool timer1ms=false' in echo and echo.find('timeBeginPeriod(1)')>echo.find('if(!ready)'))
check('EchoIdle20ms', 'Sleep(20);continue;' in echo)
check('WsIdleBackoff', 'Sleep(g_attackActive.load()?1:15)' in ws)
check('WsActiveTimingPreserved', 'kWsVisibleHoldUs' in ws and 'InterruptibleAttackDelayFrom' in ws)
check('AttackIdleBackoff', 'Sleep(15);continue;' in attack)
check('AttackRuntimePreserved', 'ExecuteAttack(a)' in attack and 'WaitWsCycleCompletion(a)' in attack)
check('MobIdleBackoff', 'if(!ready){was=false;Sleep(20);continue;}' in mob)
check('NoUnconditionalMinorTimer', not minor.startswith('void MinorWorker(){\n  timeBeginPeriod'))
check('NoUnconditionalRTimer', not rw.startswith('void RWorker(){timeBeginPeriod'))
check('NoUnconditionalEchoTimer', not echo.startswith('void WarriorEchoWorker(){\n  timeBeginPeriod'))

for name,ok in tests: print(f'{name}={"PASS" if ok else "FAIL"}')
passed=sum(ok for _,ok in tests)
report='\n'.join([f'{n}={"PASS" if ok else "FAIL"}' for n,ok in tests]+[f'TOTAL={len(tests)}',f'PASSED={passed}'])+'\n'
Path(__file__).with_name('v4825-idle-cpu-report.txt').write_text(report,encoding='utf-8')
print(f'TOTAL={len(tests)} PASSED={passed}')
sys.exit(0 if passed==len(tests) else 1)
