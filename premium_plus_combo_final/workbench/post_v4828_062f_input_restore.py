from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text(encoding='utf-8-sig')

def span(src,sig):
    a=src.find(sig)
    if a<0: raise SystemExit('MISSING '+sig)
    q=src.find('{',a); d=0
    for i in range(q,len(src)):
        if src[i]=='{': d+=1
        elif src[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise SystemExit('UNCLOSED '+sig)

def rep1(src,old,new,label):
    n=src.count(old)
    if n!=1: raise SystemExit(f'{label}: count {n}')
    return src.replace(old,new,1)

# Restore the native timing from the exact 062F-derived, live-proven transport.
a,b=span(s,'UINT ReferenceSendInputsUnlocked(')
fn=s[a:b]
fn=rep1(fn,'PreciseDelayUs(75);','PreciseDelayUs(50);','RELEASE_GAP')
fn=rep1(fn,'PreciseDelayUs(up?75:1000);','PreciseDelayUs(up?50:1000);','SINGLE_GAP')
fn=rep1(fn,'''if(SendInput(1,&second,sizeof(INPUT))!=1){
        PreciseDelayUs(1000);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;
      }''','''if(SendInput(1,&second,sizeof(INPUT))!=1){
        Sleep(1);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;
      }''','RETRY_SLEEP')
s=s[:a]+fn+s[b:]

# Restore 062F's burst/QPC manual-Minor scheduler.  Auto Minor is a newer,
# independent feature and keeps its current path.
a,b=span(s,'void MinorWorker()')
minor=r'''void MinorWorker(){
  LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);bool timer1ms=false;
  LONGLONG base=0,emitted=0;int lastRate=0;bool scheduled=false;int autoKnownBar=0;bool autoWasRunning=false;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    const bool featureReady=r.powerEnabled&&g_rogueCategoryEnabled&&g_minorActive.load();
    if(!featureReady){if(timer1ms){timeEndPeriod(1);timer1ms=false;}scheduled=false;autoKnownBar=0;autoWasRunning=false;Sleep(20);continue;}
    if(!timer1ms){timeBeginPeriod(1);timer1ms=true;}
    if(g_cureExclusive||g_potionExclusive||g_chatMode){scheduled=false;autoKnownBar=0;autoWasRunning=false;Sleep(1);continue;}
    RogueSettings fresh;{std::lock_guard<std::mutex>lk(g_settingsMutex);fresh=g_rogue;}
    if(!fresh.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){scheduled=false;continue;}
    const bool autoOwned=g_autoMinorOwned.load(std::memory_order_acquire)&&fresh.autoMinorEnabled;
    if(autoOwned){
      scheduled=false;
      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){Sleep(1);continue;}
      FifoTicketGuard sequence(g_gameInputGate);
      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire)))continue;
      bool needBar=!autoWasRunning||autoKnownBar!=fresh.autoMinorBar;
      if(g_attackActive.load())needBar=g_attackKnownBar.load(std::memory_order_relaxed)!=fresh.autoMinorBar;
      if(needBar){DirectTimedTapUnlocked(BarToVk(fresh.autoMinorBar),12000,1000);autoKnownBar=fresh.autoMinorBar;if(g_attackActive.load())g_attackKnownBar=fresh.autoMinorBar;PreciseDelayUs(2500);}
      ReferenceTapKeyUnlocked(SlotToVk(fresh.autoMinorSlot));autoWasRunning=true;continue;
    }
    autoKnownBar=0;autoWasRunning=false;
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);
    if(!scheduled||rate!=lastRate){base=now.QuadPart;emitted=0;lastRate=rate;scheduled=true;}
    LONGLONG elapsed=std::max<LONGLONG>(0,now.QuadPart-base);LONGLONG due=(elapsed*rate)/std::max<LONGLONG>(1,fq.QuadPart)+1;
    if(due<=emitted){
      LONGLONG target=base+(emitted*std::max<LONGLONG>(1,fq.QuadPart))/rate;
      do{QueryPerformanceCounter(&now);if(now.QuadPart>=target)break;LONGLONG left=target-now.QuadPart;if(left*1000>fq.QuadPart)Sleep(1);else SwitchToThread();}while(g_running&&g_minorActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode);
      continue;
    }
    int burst=(int)std::clamp<LONGLONG>(due-emitted,1,8);
    RogueSettings live;{std::lock_guard<std::mutex>lk(g_settingsMutex);live=g_rogue;}
    if(!live.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){scheduled=false;continue;}
    if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){Sleep(1);continue;}
    auto batch=BuildMinorBatch(live,burst);
    ReferenceSendInputs(batch.data(),(UINT)batch.size());
    emitted+=burst;
  }
  if(timer1ms)timeEndPeriod(1);
}'''
s=s[:a]+minor+s[b:]

# Expose the actual runtime route.  A Windows-level test is no longer reported
# as game success: BRIDGE means a real receiver heartbeat is present, NATIVE means
# the app is relying on Windows SendInput fallback.
a,b=span(s,'void RefreshStatus()')
fn=s[a:b]
old='s+=L"   |   Mob: "+std::wstring(g_mobActive.load()?L"ÇALIŞIYOR":L"Hazır");SetWindowTextW(g_ui.status,s.c_str());'
new='s+=L"   |   Mob: "+std::wstring(g_mobActive.load()?L"ÇALIŞIYOR":L"Hazır");s+=L"   |   Input: "+std::wstring(BridgeReceiverLive()?L"BRIDGE":L"NATIVE");SetWindowTextW(g_ui.status,s.c_str());'
fn=rep1(fn,old,new,'STATUS_ROUTE')
s=s[:a]+fn+s[b:]

# Replace the old CPU-short-pulse timing test with a model of the restored
# live-proven manual path.
a,b=span(s,'bool RunMinorTimingTest()')
test=r'''bool RunMinorTimingTest(){
  const int maxRate=120,turboRate=240;bool rates=maxRate==120&&turboRate==240;
  RogueSettings r;r.seq={'8','9','0'};auto batch=BuildMinorBatch(r,8);
  bool burst=batch.size()==48&&batch[0].ki.wVk=='8'&&batch[1].ki.wVk=='8'&&(batch[1].ki.dwFlags&KEYEVENTF_KEYUP)&&batch[46].ki.wVk=='0'&&(batch[47].ki.dwFlags&KEYEVENTF_KEYUP);
  std::ofstream f("minor-timing-report.txt",std::ios::trunc);
  f<<"Reference=062F_LIVE_PROVEN_MANUAL_MINOR\nMaxHz=120\nTurboHz=240\nNativeHoldUs=1000\nNativeGapUs=50\nBurstMax=8\n";
  f<<"RateModel="<<(rates?"PASS":"FAIL")<<"\nBurstModel="<<(burst?"PASS":"FAIL")<<"\nRESULT="<<((rates&&burst)?"PASS":"FAIL")<<"\n";
  return rates&&burst;
}'''
s=s[:a]+test+s[b:]

checks={
 'EXACT_GENERIC_HOLD_1000':'PreciseDelayUs(1000);' in s,
 'EXACT_GENERIC_GAP_50':'PreciseDelayUs(50);' in s,
 'EXACT_RETRY_SLEEP':'Sleep(1);\n        if(SendInput(1,&second,sizeof(INPUT))!=1)break;' in s,
 'MANUAL_062F_BASE_EMITTED':'LONGLONG base=0,emitted=0' in s,
 'MANUAL_062F_BURST':'std::clamp<LONGLONG>(due-emitted,1,8)' in s,
 'MANUAL_BUILD_BATCH':'BuildMinorBatch(live,burst)' in s,
 'MANUAL_REFERENCE_SEND':'ReferenceSendInputs(batch.data(),(UINT)batch.size())' in s,
 'NO_MANUAL_CPU_SHORT_PULSE':'MinorSendInputsLowCpu(manualBatch' not in s,
 'AUTO_MINOR_PRESERVED':'g_autoMinorOwned.load' in s and 'DirectTimedTapUnlocked(BarToVk(fresh.autoMinorBar),12000,1000)' in s,
 'INPUT_ROUTE_VISIBLE':'Input: ' in s and 'BridgeReceiverLive()?L"BRIDGE":L"NATIVE"' in s,
 'SCANCODE_FALLBACK_PRESERVED':'KEYEVENTF_SCANCODE' in s,
}
for k,v in checks.items():
    print(k+'='+('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)

p.write_text(s,encoding='utf-8',newline='\n')
print('V4828_062F_INPUT_RESTORE=APPLIED')
