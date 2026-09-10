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

insert_at,_=span(s,'std::vector<INPUT> BuildMinorBatch(')
helpers=r'''constexpr DWORD kCreateWaitableTimerHighResolution=0x00000002u;
HANDLE CreateMinorTimer(){
  HANDLE h=CreateWaitableTimerExW(nullptr,nullptr,kCreateWaitableTimerHighResolution,TIMER_ALL_ACCESS);
  if(!h)h=CreateWaitableTimerW(nullptr,FALSE,nullptr);
  return h;
}
void MinorWaitUntil(HANDLE timer,LONGLONG target,LONGLONG freq){
  LARGE_INTEGER now{};
  const LONGLONG spinTicks=std::max<LONGLONG>(1,freq/12500); // ~80 us precision tail
  for(;;){
    QueryPerformanceCounter(&now);LONGLONG left=target-now.QuadPart;if(left<=0)break;
    if(left<=spinTicks){YieldProcessor();continue;}
    LONGLONG sleepTicks=left-spinTicks;
    LONGLONG hundredNs=(sleepTicks*10000000LL)/freq;
    if(timer&&hundredNs>=1000){
      LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,hundredNs);
      if(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)){WaitForSingleObject(timer,INFINITE);continue;}
    }
    if(left>freq/2000)Sleep(0); else YieldProcessor();
  }
}
void MinorDelayUs(HANDLE timer,int us,LONGLONG freq){
  if(us<=0)return;LARGE_INTEGER now{};QueryPerformanceCounter(&now);
  LONGLONG ticks=std::max<LONGLONG>(1,(freq*(LONGLONG)us)/1000000LL);
  MinorWaitUntil(timer,now.QuadPart+ticks,freq);
}
UINT MinorSendInputsLowCpu(HANDLE timer,const INPUT* inputs,UINT count,LONGLONG freq){
  if(!inputs||!count)return 0;
  if(BridgeReceiverLive()&&PublishBridgeInputsUnlocked(inputs,count))return count;
  UINT done=0;
  while(done<count){
    const bool pair=(done+1<count)&&MatchingDownUpPair(inputs[done],inputs[done+1]);
    INPUT first=NativeNormalizedInput(inputs[done]);
    if(SendInput(1,&first,sizeof(INPUT))!=1)break;
    if(pair){
      MinorDelayUs(timer,1000,freq);
      INPUT second=NativeNormalizedInput(inputs[done+1]);
      if(SendInput(1,&second,sizeof(INPUT))!=1){
        MinorDelayUs(timer,1000,freq);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;
      }
      done+=2;MinorDelayUs(timer,75,freq);
    }else{
      const bool up=(inputs[done].ki.dwFlags&KEYEVENTF_KEYUP)!=0;
      ++done;MinorDelayUs(timer,up?75:1000,freq);
    }
  }
  return done;
}
'''
s=s[:insert_at]+helpers+s[insert_at:]

a,b=span(s,'void MinorWorker()')
new=r'''void MinorWorker(){
  timeBeginPeriod(1);LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);HANDLE timer=CreateMinorTimer();
  LONGLONG nextTick=0;int lastRate=0;int autoKnownBar=0;bool autoWasRunning=false;
  std::array<INPUT,6> manualBatch{};std::array<int,3> cachedSeq{-1,-1,-1};bool batchReady=false;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    if(!r.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;Sleep(2);continue;}
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
    if(!nextTick||rate!=lastRate){nextTick=now.QuadPart;lastRate=rate;}
    if(now.QuadPart<nextTick){MinorWaitUntil(timer,nextTick,fq.QuadPart);continue;}
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
      MinorSendInputsLowCpu(timer,manualBatch.data(),(UINT)manualBatch.size(),fq.QuadPart);
    }
    QueryPerformanceCounter(&now);nextTick+=step;if(now.QuadPart-nextTick>step*2)nextTick=now.QuadPart+step;
  }
  if(timer)CloseHandle(timer);timeEndPeriod(1);
}'''
s=s[:a]+new+s[b:]

# Actual compiled-executable timing/CPU gate. It exercises the same wait helpers
# with the native Minor workload shape: 3 x (1000 us hold + 75 us gap) per cycle.
insert,_=span(s,'bool RunSelfTest()')
test=r'''static ULONGLONG Ft64(const FILETIME& f){ULARGE_INTEGER u{};u.LowPart=f.dwLowDateTime;u.HighPart=f.dwHighDateTime;return u.QuadPart;}
bool RunMinorCpuTimingTest(){
  timeBeginPeriod(1);HANDLE timer=CreateMinorTimer();LARGE_INTEGER fq{},start{},now{};QueryPerformanceFrequency(&fq);
  FILETIME c0{},e0{},k0{},u0{},c1{},e1{},k1{},u1{};GetThreadTimes(GetCurrentThread(),&c0,&e0,&k0,&u0);
  QueryPerformanceCounter(&start);LONGLONG next=start.QuadPart;const int rate=240,cycles=240;const LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
  for(int i=0;i<cycles;i++){
    for(int k=0;k<3;k++){MinorDelayUs(timer,1000,fq.QuadPart);MinorDelayUs(timer,75,fq.QuadPart);}
    next+=step;QueryPerformanceCounter(&now);if(now.QuadPart<next)MinorWaitUntil(timer,next,fq.QuadPart);else if(now.QuadPart-next>step*2)next=now.QuadPart;
  }
  QueryPerformanceCounter(&now);GetThreadTimes(GetCurrentThread(),&c1,&e1,&k1,&u1);if(timer)CloseHandle(timer);timeEndPeriod(1);
  double wallMs=1000.0*(double)(now.QuadPart-start.QuadPart)/(double)fq.QuadPart;
  double cpuMs=(double)((Ft64(k1)-Ft64(k0))+(Ft64(u1)-Ft64(u0)))/10000.0;
  double hz=wallMs>0?cycles*1000.0/wallMs:0.0;double cpuPct=wallMs>0?100.0*cpuMs/wallMs:100.0;
  bool cadence=hz>=225.0&&hz<=255.0;bool cpu=cpuPct<=30.0;
  std::ofstream f("minor-cpu-timing-report.txt",std::ios::trunc);f<<"TargetHz=240\nMeasuredHz="<<hz<<"\nWallMs="<<wallMs<<"\nThreadCpuMs="<<cpuMs<<"\nThreadCpuPct="<<cpuPct<<"\nCadence="<<(cadence?"PASS":"FAIL")<<"\nLowCpu="<<(cpu?"PASS":"FAIL")<<"\nRESULT="<<((cadence&&cpu)?"PASS":"FAIL")<<"\n";
  return cadence&&cpu;
}

'''
s=s[:insert]+test+s[insert:]

# Wire the isolated diagnostic into the existing CLI without altering normal startup.
needle='int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show){g_instance=hi;'
if needle not in s: raise SystemExit('WMAIN_MARKER_MISSING')
s=s.replace(needle,needle+'if(cmd&&wcsstr(cmd,L"--minor-cpu-timing-test"))return RunMinorCpuTimingTest()?0:10;',1)

minor=s[span(s,'void MinorWorker()')[0]:span(s,'void MinorWorker()')[1]]
checks={
 'MAX_RATE_120':'?240:120' in minor,
 'TURBO_RATE_240':'?240:120' in minor,
 'MANUAL_THREE_PAIRS':'std::array<INPUT,6>' in minor,
 'LOW_CPU_TIMER':'MinorWaitUntil(timer,nextTick' in minor and 'MinorSendInputsLowCpu' in minor,
 'CACHED_BATCH':'cachedSeq!=fresh.seq' in minor,
 'SIDEINPUT_GUARD':'g_attackExclusive.load' in minor and 'g_wsPriority.load' in minor,
 'GENERIC_PRECISE_DELAY_UNTOUCHED':'void PreciseDelayUs(int microseconds)' in s,
 'CPU_TEST_MODE':'--minor-cpu-timing-test' in s and 'minor-cpu-timing-report.txt' in s,
}
for k,v in checks.items():
    print(k+'='+('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)

p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_MINOR_CPU_FIX=APPLIED')