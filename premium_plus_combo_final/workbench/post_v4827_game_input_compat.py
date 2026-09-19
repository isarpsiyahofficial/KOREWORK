import pathlib,sys

p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(src,sig):
    start=src.find(sig)
    if start<0: raise SystemExit('MISSING '+sig)
    q=src.find('{',start)
    if q<0: raise SystemExit('NO_BODY '+sig)
    depth=0
    for i in range(q,len(src)):
        if src[i]=='{': depth+=1
        elif src[i]=='}':
            depth-=1
            if depth==0: return start,i+1
    raise SystemExit('UNCLOSED '+sig)

def replace_once(src,old,new,label):
    n=src.count(old)
    if n!=1: raise SystemExit(f'{label}: expected 1 occurrence, got {n}')
    return src.replace(old,new,1)

# Native fallback must remain visible long enough for game input polling.
# Fast shared bridge keeps the legacy 120/240 cadence because it consumes every event.
s=replace_once(s,'constexpr int kMinorNativeHoldUs=300;','constexpr int kMinorNativeHoldUs=12000;','MINOR_HOLD')
s=replace_once(s,'constexpr int kMinorNativeGapUs=30;','constexpr int kMinorNativeGapUs=800;','MINOR_GAP')

# Long minor holds must not burn a CPU core while waiting.
a,b=span(s,'void MinorPulseWaitUntil(')
fn=s[a:b]
fn=replace_once(fn,
'''    if(left>spinTicks){Sleep(0);continue;}
    YieldProcessor();''',
'''    const LONGLONG sleepTicks=std::max<LONGLONG>(1,freq/500); // ~2 ms
    if(left>sleepTicks){Sleep(1);continue;}
    if(left>spinTicks){Sleep(0);continue;}
    YieldProcessor();''','MINOR_WAIT_LOW_CPU')
s=s[:a]+fn+s[b:]

# Generic SendInput path: turn sub-millisecond taps into game-visible key states.
insert='constexpr int kGameNativeHoldUs=12000;\nconstexpr int kGameNativeGapUs=1000;\nconstexpr int kGameRetryUs=800;\n'
mark='// Game transport. The caller must own g_gameInputGate.'
if mark not in s: raise SystemExit('TRANSPORT_MARKER_MISSING')
s=s.replace(mark,insert+mark,1)

a,b=span(s,'UINT ReferenceSendInputsUnlocked(')
fn=s[a:b]
fn=replace_once(fn,'      PreciseDelayUs(1000);\n      INPUT second=NativeNormalizedInput(inputs[done+1]);',
'''      PreciseDelayUs(kGameNativeHoldUs);
      INPUT second=NativeNormalizedInput(inputs[done+1]);''','GENERIC_HOLD')
fn=replace_once(fn,'        PreciseDelayUs(1000);\n        if(SendInput(1,&second,sizeof(INPUT))!=1)break;',
'''        PreciseDelayUs(kGameRetryUs);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;''','GENERIC_RETRY')
fn=replace_once(fn,'      PreciseDelayUs(75);','      PreciseDelayUs(kGameNativeGapUs);','GENERIC_GAP')
fn=replace_once(fn,'      PreciseDelayUs(up?75:1000);','      PreciseDelayUs(up?kGameNativeGapUs:kGameNativeHoldUs);','GENERIC_SINGLE')
s=s[:a]+fn+s[b:]

# Explicit timed taps used by bars/slots/potions also get a safe minimum pulse.
a,b=span(s,'bool DirectTimedTapUnlocked(')
fn=s[a:b]
fn=replace_once(fn,'std::clamp(holdUs,1000,50000)','std::clamp(holdUs,10000,50000)','DIRECT_MIN_HOLD')
fn=replace_once(fn,'std::clamp(releaseGapUs,75,10000)','std::clamp(releaseGapUs,800,10000)','DIRECT_MIN_GAP')
s=s[:a]+fn+s[b:]

# Native minor mode is now paced for real acceptance: 20 Hz Max / 26 Hz Turbo.
# Bridge mode retains 120/240 Hz.
a,b=span(s,'void MinorWorker()')
fn=s[a:b]
fn=replace_once(fn,
'int rate=g_turbo.load()?240:120;',
'const bool fastBridge=BridgeReceiverLive();int rate=fastBridge?(g_turbo.load()?240:120):(g_turbo.load()?26:20);',
'MINOR_ADAPTIVE_RATE')
s=s[:a]+fn+s[b:]

# Timing regression now validates the native Turbo path (26 full 8/9/0 cycles/s).
a,b=span(s,'bool RunMinorTimingTest()')
fn=s[a:b]
fn=replace_once(fn,'const int rate=240,cycles=720;','const int rate=26,cycles=156;','TIMING_RATE')
fn=replace_once(fn,'bool cadence=hz>=228.0&&hz<=252.0;','bool cadence=hz>=24.5&&hz<=27.5;','TIMING_RANGE')
fn=replace_once(fn,'f<<"TargetHz=240\\nMeasuredHz="','f<<"TargetHz=26\\nMeasuredHz="','TIMING_REPORT')
s=s[:a]+fn+s[b:]

# Real Windows low-level observer gate: verifies the OS sees scan-code DOWN/UP
# and that the DOWN state survives for a game-visible duration.
insert_at=s.find('bool RunSelfTest()')
if insert_at<0: raise SystemExit('SELFTEST_MARKER_MISSING')
compat=r'''bool RunGameInputCompatObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  std::thread observer;bool ready=StartObserver(observer);bool tapOk=false,directOk=false;
  if(ready){
    tapOk=ReferenceTapKey('8');
    {FifoTicketGuard sequence(g_gameInputGate);directOk=DirectTimedTapUnlocked('9',12000,1000);}
  }
  StopObserver(observer);
  auto holdMs=[&](int vk)->long long{ULONGLONG d=g_observerFirstDownAt[vk].load(),u=g_observerFirstUpAt[vk].load();return(d&&u&&u>=d)?(long long)((u-d+500)/1000):-1;};
  long long h8=holdMs('8'),h9=holdMs('9');
  bool counts=g_observerDownCount['8'].load()==1&&g_observerDownCount['9'].load()==1;
  bool scans=g_observerScanNonZero['8']&&g_observerScanNonZero['9'];
  bool visible=h8>=10&&h9>=9;
  bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("game-input-compat-observer-report.txt",std::ios::trunc);
  f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\n";
  f<<"ReferenceTapOk="<<(tapOk?"PASS":"FAIL")<<"\nDirectTapOk="<<(directOk?"PASS":"FAIL")<<"\n";
  f<<"8_HoldMs="<<h8<<"\n9_HoldMs="<<h9<<"\n";
  f<<"ScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\n";
  f<<"GameVisibleHold="<<(visible?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";
  bool ok=ready&&tapOk&&directOk&&counts&&scans&&visible&&overlap;
  f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";
  CloseBridge();return ok;
}

'''
s=s[:insert_at]+compat+s[insert_at:]

needle='if(cmd&&wcsstr(cmd,L"--minor-timing-test"))return RunMinorTimingTest()?0:10;'
if needle not in s: raise SystemExit('WMAIN_TIMING_MARKER_MISSING')
s=s.replace(needle,needle+'if(cmd&&wcsstr(cmd,L"--game-input-compat-observer-test"))return RunGameInputCompatObserverTest()?0:27;',1)

checks={
 'MINOR_HOLD_12MS':'kMinorNativeHoldUs=12000' in s,
 'MINOR_GAP_800US':'kMinorNativeGapUs=800' in s,
 'NATIVE_MAX_20_TURBO_26':'fastBridge?(g_turbo.load()?240:120):(g_turbo.load()?26:20)' in s,
 'BRIDGE_FAST_PRESERVED':'fastBridge?(g_turbo.load()?240:120)' in s,
 'GENERIC_HOLD_12MS':'kGameNativeHoldUs=12000' in s,
 'GENERIC_GAP_1MS':'kGameNativeGapUs=1000' in s,
 'DIRECT_MIN_10MS':'std::clamp(holdUs,10000,50000)' in s,
 'LOW_CPU_LONG_WAIT':'if(left>sleepTicks){Sleep(1);continue;}' in s,
 'OBSERVER_COMPAT_GATE':'--game-input-compat-observer-test' in s and 'GameVisibleHold=' in s,
 'SCAN_CODE_TRANSPORT_PRESERVED':'KEYEVENTF_SCANCODE' in s and 'NativeNormalizedInput' in s,
}
for k,v in checks.items():
    print(k+'='+('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)

p.write_text(s,encoding='utf-8',newline='\n')
print('V4827_GAME_INPUT_COMPAT=APPLIED')
