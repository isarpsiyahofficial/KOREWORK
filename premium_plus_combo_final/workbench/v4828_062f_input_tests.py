from pathlib import Path
import base64,gzip,sys

WB=Path(__file__).resolve().parent
base=(WB/'premiumplus_v4825_final.cpp').read_text(encoding='utf-8')
new=(WB/'premiumplus_v4828_062f_input.cpp').read_text(encoding='utf-8')
src=WB.parent/'source'/'timed_exact'
exact=gzip.decompress(base64.b64decode(''.join(x.read_text().strip() for x in sorted(src.glob('part*.txt'))))).decode('utf-8')

def fn(s,sig):
    a=s.index(sig);q=s.index('{',a);d=0
    for i in range(q,len(s)):
        if s[i]=='{':d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[a:i+1]
    raise RuntimeError(sig)

def mask(s,sigs):
    pieces=[];pos=0
    spans=[]
    for sig in sigs:
        a=s.index(sig);q=s.index('{',a);d=0
        for i in range(q,len(s)):
            if s[i]=='{':d+=1
            elif s[i]=='}':
                d-=1
                if d==0: spans.append((a,i+1,sig));break
    for a,b,sig in sorted(spans):
        pieces.append(s[pos:a]);pieces.append('/*MASK:'+sig+'*/');pos=b
    pieces.append(s[pos:]);return ''.join(pieces)

changed=['UINT ReferenceSendInputsUnlocked(','void MinorWorker()','void RefreshStatus()','bool RunMinorTimingTest()']
checks={}
checks['ONLY_FOUR_SCOPES_CHANGED']=mask(base,changed)==mask(new,changed)

ex=fn(exact,'UINT ReferenceSendInputs(')
cur=fn(new,'UINT ReferenceSendInputsUnlocked(')
checks['062F_HOLD_1000']='PreciseDelayUs(1000);' in ex and 'PreciseDelayUs(1000);' in cur
checks['062F_GAP_50']='PreciseDelayUs(50);' in ex and 'PreciseDelayUs(50);' in cur
checks['062F_RETRY_SLEEP']='Sleep(1);if(SendInput(1,&second,sizeof(INPUT))!=1)break;' in ex.replace('\n','') and 'Sleep(1);' in cur

em=fn(exact,'void MinorWorker()')
nm=fn(new,'void MinorWorker()')
for token in ['LONGLONG base=0,emitted=0','due=(elapsed*rate)','std::clamp<LONGLONG>(due-emitted,1,8)','BuildMinorBatch']:
    checks['MINOR_'+token[:16].replace(' ','_')]=token in em and token in nm
checks['MINOR_RATES_120_240']='g_turbo.load()?240:120' in em and 'g_turbo.load()?240:120' in nm
checks['MANUAL_SHORT_PULSE_REMOVED']='MinorSendInputsLowCpu(manualBatch' not in nm
checks['AUTO_MINOR_STILL_PRESENT']='g_autoMinorOwned.load' in nm and 'autoMinorBar' in nm
checks['BRIDGE_FIRST']='BridgeReceiverLive()&&PublishBridgeInputsUnlocked' in cur
checks['SCANCODE_FALLBACK']='NativeNormalizedInput' in cur and 'KEYEVENTF_SCANCODE' in new
checks['ROUTE_VISIBLE']='Input: ' in fn(new,'void RefreshStatus()') and 'BRIDGE' in fn(new,'void RefreshStatus()') and 'NATIVE' in fn(new,'void RefreshStatus()')
checks['NO_V4827_LONG_GENERIC']='kGameNativeHoldUs' not in new and 'kGameNativeGapUs' not in new

passed=sum(checks.values())
for k,v in checks.items():print(f'{k}={"PASS" if v else "FAIL"}')
print(f'TOTAL={len(checks)}')
print(f'PASSED={passed}')
sys.exit(0 if passed==len(checks) else 1)
