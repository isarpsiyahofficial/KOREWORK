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

# Minor-only low-CPU timing path. Uses APIs already present in the validated
# release import surface: QueryPerformanceCounter + Sleep. Long waits sleep;
# only the final tiny precision tail spins. Generic transport stays untouched.
insert_at,_=span(s,'std::vector<INPUT> BuildMinorBatch(')
helpers=r'''void MinorWaitUntil(LONGLONG target,LONGLONG freq){
  LARGE_INTEGER now{};
  const LONGLONG spinTicks=std::max<LONGLONG>(1,freq/10000); // ~100 us precision tail
  const LONGLONG sleepFloor=std::max<LONGLONG>(spinTicks+1,freq/2500); // ~400 us
  for(;;){
    QueryPerformanceCounter(&now);LONGLONG left=target-now.QuadPart;if(left<=0)break;
    if(left>sleepFloor){Sleep(1);continue;}
    if(left>spinTicks){Sleep(0);continue;}
    YieldProcessor();
  }
}
void MinorDelayUs(int us,LONGLONG freq){
  if(us<=0)return;
  if(us>=900){
    LARGE_INTEGER begin{},now{};QueryPerformanceCounter(&begin);Sleep(1);
    const LONGLONG target=begin.QuadPart+std::max<LONGLONG>(1,(freq*(LONGLONG)us)/1000000LL);
    QueryPerformanceCounter(&now);if(now.QuadPart<target)MinorWaitUntil(target,freq);return;
  }
  LARGE_INTEGER now{};QueryPerformanceCounter(&now);
  const LONGLONG ticks=std::max<LONGLONG>(1,(freq*(LONGLONG)us)/1000000LL);
  MinorWaitUntil(now.QuadPart+ticks,freq);
}
UINT MinorSendInputsLowCpu(const INPUT* inputs,UINT count,LONGLONG freq){
  if(!inputs||!count)return 0;
  if(BridgeReceiverLive()&&PublishBridgeInputsUnlocked(inputs,count))return count;
  UINT done=0;
  while(done<count){
    const bool pair=(done+1<count)&&MatchingDownUpPair(inputs[done],inputs[done+1]);
    INPUT first=NativeNormalizedInput(inputs[done]);
    if(SendInput(1,&first,sizeof(INPUT))!=1)break;
    if(pair){
      MinorDelayUs(1000,freq);
      INPUT second=NativeNormalizedInput(inputs[done+1]);
      if(SendInput(1,&second,sizeof(INPUT))!=1){
        MinorDelayUs(1000,freq);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;
      }
      done+=2;MinorDelayUs(75,freq);
    }else{
      const bool up=(inputs[done].ki.dwFlags&KEYEVENTF_KEYUP)!=0;
      ++done;MinorDelayUs(up?75:1000,freq);
    }
  }
  return done;
}
'''
s=s[:insert_at]+helpers+s[insert_at:]

a,b=span(s,'void MinorWorker()')
minor_replacement=r'''void MinorWorker(){
  timeBeginPeriod(1);LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);
  LONGLONG nextTick=0;int lastRate=0;int autoKnownBar=0;bool autoWasRunning=false;
  std::array<INPUT,6> manualBatch{};std::array<int,3> cachedSeq{-1,-1,-1};bool batchReady=false;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    if(!r.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;Sleep(2);continue;}
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
    if(!nextTick||rate!=lastRate){nextTick=now.QuadPart;lastRate=rate;}
    if(now.QuadPart<nextTick){MinorWaitUntil(nextTick,fq.QuadPart);continue;}
    RogueSettings fresh;{std::lock_guard<std::mutex>lk(g_settingsMutex);fresh=g_rogue;}
    if(!fresh.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;continue;}
    const bool autoOwned=g_autoMinorOwned.load(std::memory_order_acquire)&&fresh.autoMinorEnabled;
    if(autoOwned){
      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){QueryPerformanceCounter(&now);nextTick=now.QuadPart+step;SwitchToThread();continue;}
      FifoTicketGuard sequence(g_gameInputGate);
      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){QueryPerformanceCounter(&now);nextTick=now.QuadPart+step;continue;}
      bool needBar=!autoWasRunning||autoKnownBar!=fresh.autoMinorBar;
      if(g_attackActive.load())needBar=g_attackKnownBar.load(std::memory_order_relaxed)!=fresh.autoMinorBar;
      if(needBar){DirectTimedTapUnlocked(BarToVk(fresh.autoMinorBar),12000,1000);autoKnownBar=fresh.autoMinorBar;if(g_attackActive.load())g_attackKnownBar=fresh.autoMinorBar;PreciseDelayUs(2500);}
      ReferenceTapKeyUnlocked(SlotToVk(fresh.autoMinorSlot));
      autoWasRunning=true;
    }else{
      autoKnownBar=0;autoWasRunning=false;
      if(!batchReady||cachedSeq!=fresh.seq){cachedSeq=fresh.seq;for(int i=0;i<3;i++){BuildKeyInput(manualBatch[i*2],fresh.seq[i],false);BuildKeyInput(manualBatch[i*2+1],fresh.seq[i],true);}batchReady=true;}
      FifoTicketGuard sequence(g_gameInputGate);
      MinorSendInputsLowCpu(manualBatch.data(),(UINT)manualBatch.size(),fq.QuadPart);
    }
    QueryPerformanceCounter(&now);nextTick+=step;if(now.QuadPart-nextTick>step*2)nextTick=now.QuadPart+step;
  }
  timeEndPeriod(1);
}'''
s=s[:a]+minor_replacement+s[b:]

# Compiled release timing gate. It exercises the exact production wait helpers
# at Turbo's 240-cycle/s target. CPU percentage is measured by a separate CI
# benchmark executable so the production PE/import surface is not enlarged.
insert,_=span(s,'bool RunSelfTest()')
timing_test=r'''bool RunMinorTimingTest(){
  timeBeginPeriod(1);LARGE_INTEGER fq{},start{},now{};QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&start);
  LONGLONG next=start.QuadPart;const int rate=240,cycles=240;const LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
  for(int i=0;i<cycles;i++){
    for(int k=0;k<3;k++){MinorDelayUs(1000,fq.QuadPart);MinorDelayUs(75,fq.QuadPart);}
    next+=step;QueryPerformanceCounter(&now);if(now.QuadPart<next)MinorWaitUntil(next,fq.QuadPart);else if(now.QuadPart-next>step*2)next=now.QuadPart;
  }
  QueryPerformanceCounter(&now);timeEndPeriod(1);
  double wallMs=1000.0*(double)(now.QuadPart-start.QuadPart)/(double)fq.QuadPart;double hz=wallMs>0?cycles*1000.0/wallMs:0.0;
  bool cadence=hz>=225.0&&hz<=255.0;
  std::ofstream f("minor-timing-report.txt",std::ios::trunc);f<<"TargetHz=240\nMeasuredHz="<<hz<<"\nWallMs="<<wallMs<<"\nCadence="<<(cadence?"PASS":"FAIL")<<"\nRESULT="<<(cadence?"PASS":"FAIL")<<"\n";
  return cadence;
}

'''
s=s[:insert]+timing_test+s[insert:]

needle='int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show){g_instance=hi;'
if needle not in s: raise SystemExit('WMAIN_MARKER_MISSING')
s=s.replace(needle,needle+'if(cmd&&wcsstr(cmd,L"--minor-timing-test"))return RunMinorTimingTest()?0:10;',1)

minor=s[span(s,'void MinorWorker()')[0]:span(s,'void MinorWorker()')[1]]
injected=helpers+minor_replacement+timing_test
forbidden=['CreateWaitableTimerExW','CreateWaitableTimerW','SetWaitableTimer','GetThreadTimes','GetCurrentThread']
checks={
 'MAX_RATE_120':'?240:120' in minor,
 'TURBO_RATE_240':'?240:120' in minor,
 'MANUAL_THREE_PAIRS':'std::array<INPUT,6>' in minor,
 'LOW_CPU_WAIT':'MinorWaitUntil(nextTick' in minor and 'MinorSendInputsLowCpu' in minor,
 'CACHED_BATCH':'cachedSeq!=fresh.seq' in minor,
 'SIDEINPUT_GUARD':'g_attackExclusive.load' in minor and 'g_wsPriority.load' in minor,
 'GENERIC_PRECISE_DELAY_UNTOUCHED':'void PreciseDelayUs(int microseconds)' in s,
 'NO_NEW_APIS_IN_CPU_PATCH':all(x not in injected for x in forbidden),
 'TIMING_TEST_MODE':'--minor-timing-test' in s and 'minor-timing-report.txt' in s,
}
for k,v in checks.items():
    print(k+'='+('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)

p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_MINOR_CPU_FIX=APPLIED')