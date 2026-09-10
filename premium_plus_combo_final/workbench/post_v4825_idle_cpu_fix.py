import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.index(sig); q=s.index('{',a); d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise RuntimeError('unclosed '+sig)

def replace_fn(sig,new):
    global s
    a,b=span(sig); s=s[:a]+new+s[b:]

replace_fn('void MinorWorker()',r'''void MinorWorker(){
  LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);bool timer1ms=false;
  LONGLONG nextTick=0;int lastRate=0;int autoKnownBar=0;bool autoWasRunning=false;
  std::array<INPUT,6> manualBatch{};std::array<int,3> cachedSeq{-1,-1,-1};bool batchReady=false;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    const bool featureReady=r.powerEnabled&&g_rogueCategoryEnabled&&g_minorActive.load();
    if(!featureReady){if(timer1ms){timeEndPeriod(1);timer1ms=false;}nextTick=0;autoKnownBar=0;autoWasRunning=false;Sleep(20);continue;}
    if(!timer1ms){timeBeginPeriod(1);timer1ms=true;}
    if(g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;Sleep(2);continue;}
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
    if(!nextTick||rate!=lastRate){nextTick=now.QuadPart;lastRate=rate;}
    if(now.QuadPart<nextTick){MinorCycleWaitUntil(nextTick,fq.QuadPart);continue;}
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
      MinorSendInputsLowCpu(manualBatch.data(),(UINT)manualBatch.size(),fq.QuadPart);
    }
    QueryPerformanceCounter(&now);nextTick+=step;if(now.QuadPart-nextTick>step*2)nextTick=now.QuadPart+step;
  }
  if(timer1ms)timeEndPeriod(1);
}''')

replace_fn('void RWorker()',r'''void RWorker(){
  bool timer1ms=false;ULONGLONG next=0;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    const bool featureReady=r.powerEnabled&&g_rogueCategoryEnabled&&g_minorActive.load()&&!g_autoMinorOwned.load()&&r.rEnabled;
    if(!featureReady){if(timer1ms){timeEndPeriod(1);timer1ms=false;}next=0;Sleep(20);continue;}
    if(!timer1ms){timeBeginPeriod(1);timer1ms=true;}
    ULONGLONG now=GetTickCount64();
    if(g_cureExclusive||g_potionExclusive||g_chatMode||now<g_rPauseUntil){Sleep(1);continue;}
    int rate=std::max(1,g_turbo.load()?r.rTurbo:r.rMax);if(now<next){Sleep(1);continue;}
    RogueSettings fresh;{std::lock_guard<std::mutex>lk(g_settingsMutex);fresh=g_rogue;}now=GetTickCount64();
    if(!fresh.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_autoMinorOwned||!fresh.rEnabled||g_cureExclusive||g_potionExclusive||g_chatMode||now<g_rPauseUntil)continue;
    ReferenceTapKey('R');rate=std::max(1,g_turbo.load()?fresh.rTurbo:fresh.rMax);next=GetTickCount64()+std::max(1,1000/rate);
  }
  if(timer1ms)timeEndPeriod(1);
}''')

replace_fn('void WsWorker()',r'''void WsWorker(){
  while(g_running){
    WsPendingState job;{std::lock_guard<std::mutex>lk(g_wsMutex);job=g_wsPending;}
    if(!job.pending){Sleep(g_attackActive.load()?1:15);continue;}
    if(!g_attackActive||g_cureExclusive||g_chatMode){ClearWsPending();Sleep(1);continue;}
    InterruptibleAttackDelayFrom(job.skillAt,WsFirstDelayMs(job));
    if(!g_running||!g_attackActive||g_cureExclusive||g_chatMode){ClearWsPending();continue;}
    LONGLONG wAt=0;
    if(job.w){
      {FifoTicketGuard sequence(g_gameInputGate);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode){wAt=AttackQpcNow();DirectTimedTapUnlocked('W',kWsVisibleHoldUs,kWsReleaseGapUs);}}
      if(wAt)g_lastComboAt=GetTickCount64();
    }
    if(job.s){
      if(job.w&&wAt)InterruptibleAttackDelayFrom(wAt,job.sDelayMs);else InterruptibleAttackDelayFrom(job.skillAt,job.sDelayMs);
      if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode){FifoTicketGuard sequence(g_gameInputGate);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode){DirectTimedTapUnlocked('S',kWsVisibleHoldUs,kWsReleaseGapUs);g_lastComboAt=GetTickCount64();}}
    }
    ClearWsPending();
  }
  ClearWsPending();
}''')

replace_fn('void AttackWorker()',r'''void AttackWorker(){bool wasReady=false;while(g_running){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}bool ready=r.powerEnabled&&g_attackCategoryEnabled&&g_attackActive&&!g_chatMode;if(!ready){wasReady=false;g_attackKnownBar=0;ClearWsPending();Sleep(15);continue;}if(g_cureExclusive){Sleep(1);continue;}if(!wasReady){g_attackKnownBar=0;ClearWsPending();wasReady=true;}ExecuteAttack(a);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode){if(a.wCombo||a.sCombo)WaitWsCycleCompletion(a);else InterruptibleAttackDelay(a.delayMs);}}g_attackKnownBar=0;ClearWsPending();}''')

replace_fn('void MobSkillWorker()',r'''void MobSkillWorker(){
  bool was=false;
  while(g_running){
    RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}
    bool ready=r.powerEnabled&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode&&g_mobTargetConfirmed.load();
    if(!ready){was=false;Sleep(20);continue;}
    if(!MobWithinLeash(m)){g_mobTargetConfirmed=false;Sleep(20);continue;}
    if(!was){g_mobSkillTurn=0;g_lastMobRandom=-1;was=true;}
    SkillEntry e;if(!NextMobSkill(m,e)){Sleep(20);continue;}
    bool sent=false;
    {FifoTicketGuard gate(g_gameInputGate);if(g_running&&g_mobActive&&g_mobTargetConfirmed&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked(BarToVk(e.bar),7000,500);PreciseDelayUs(6500);DirectTimedTapUnlocked(SlotToVk(e.slot),7000,500);if(m.restoreBar!=e.bar){PreciseDelayUs(2500);DirectTimedTapUnlocked(BarToVk(m.restoreBar),6000,400);}sent=true;}}
    if(!sent){Sleep(4);continue;}Sleep((DWORD)std::clamp(e.delayMs,1,1000));
  }
}''')

replace_fn('void WarriorEchoWorker()',r'''void WarriorEchoWorker(){
  LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);bool timer1ms=false;
  std::array<LONGLONG,kMaxWarriorTimedSkills>next{},step0{};std::array<bool,kMaxWarriorTimedSkills>pending{};
  unsigned rev0=0;bool armed=false;int cursor=0;
  while(g_running){
    WarriorSettings w;RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;r=g_rogue;}
    HWND game=(HWND)g_gameWindow.load();bool ready=w.enabled&&w.echoEnabled&&r.powerEnabled&&game&&IsWindow(game)&&GetForegroundWindow()==game&&!g_chatMode.load();
    if(!ready){if(timer1ms){timeEndPeriod(1);timer1ms=false;}armed=false;pending.fill(false);Sleep(20);continue;}
    if(!timer1ms){timeBeginPeriod(1);timer1ms=true;}
    unsigned rev=g_echoScheduleRevision.load();LARGE_INTEGER now{};QueryPerformanceCounter(&now);int count=WarriorTimedSkillCount(w);
    if(!armed||rev!=rev0){for(int i=0;i<kMaxWarriorTimedSkills;i++){next[i]=now.QuadPart;step0[i]=0;pending[i]=false;}armed=true;rev0=rev;cursor=0;}
    for(int i=0;i<count;i++){LONGLONG step=std::max<LONGLONG>(1,(LONGLONG)((long double)fq.QuadPart*(long double)WarriorTimedIntervalMs(w,i)/1000.0L));if(step0[i]!=step){step0[i]=step;next[i]=now.QuadPart;}if(now.QuadPart>=next[i])pending[i]=true;}
    bool attempted=false;
    for(int n=0;n<count;n++){int i=(cursor+n)%count;if(!pending[i])continue;int bar=1,slot=1;WarriorTimedSkillSpec(w,i,bar,slot);bool ok=WarriorCastBarSlot(game,bar,slot);attempted=true;if(ok){LARGE_INTEGER after{};QueryPerformanceCounter(&after);pending[i]=false;next[i]=after.QuadPart+step0[i];g_echoLastFireAt=GetTickCount64();g_echoFireCount.fetch_add(1);cursor=(i+1)%count;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);}break;}
    Sleep(1);
  }
  if(timer1ms)timeEndPeriod(1);
}''')

# Guard: no high-resolution timer may be acquired unconditionally at worker entry.
for sig in ('void MinorWorker()','void RWorker()','void WarriorEchoWorker()'):
    a,b=span(sig); body=s[a:b]
    first=body.find('timeBeginPeriod(1)')
    ready=body.find('featureReady') if sig!='void WarriorEchoWorker()' else body.find('if(!ready)')
    if first<0 or ready<0 or first<ready: raise RuntimeError('idle timer guard failed '+sig)

p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_IDLE_CPU_FIX=APPLIED')
