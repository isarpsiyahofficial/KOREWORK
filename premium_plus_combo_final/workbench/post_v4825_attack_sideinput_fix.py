import pathlib,sys

p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')


def span(src,sig):
    start=src.find(sig)
    if start<0:
        raise SystemExit('MISSING '+sig)
    q=src.find('{',start)
    if q<0:
        raise SystemExit('NO_BODY '+sig)
    depth=0
    for i in range(q,len(src)):
        if src[i]=='{': depth+=1
        elif src[i]=='}':
            depth-=1
            if depth==0:
                return start,i+1
    raise SystemExit('UNCLOSED '+sig)


def replace_in_fn(src,sig,old,new,count=1):
    a,b=span(src,sig)
    body=src[a:b]
    found=body.count(old)
    if found<count:
        raise SystemExit(f'{sig}: expected at least {count} occurrence(s), got {found}: {old!r}')
    body=body.replace(old,new,count)
    return src[:a]+body+src[b:]

# 1) Auto Minor must never build a FIFO backlog ahead of ATTACK/W-S.
# It remains fully functional, but yields for the current tick while ATTACK owns
# the input critical section or has a W/S timing job reserved.
old='''    if(autoOwned){\n      FifoTicketGuard sequence(g_gameInputGate);'''
new='''    if(autoOwned){\n      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){\n        QueryPerformanceCounter(&now);nextTick=now.QuadPart+step;SwitchToThread();continue;\n      }\n      FifoTicketGuard sequence(g_gameInputGate);\n      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){\n        QueryPerformanceCounter(&now);nextTick=now.QuadPart+step;continue;\n      }'''
s=replace_in_fn(s,'void MinorWorker()',old,new)

# 2) Potion activity must not invalidate ATTACK timing/state. The global input
# gate still prevents physically overlapping key events; ATTACK no longer tears
# down/restarts merely because a pot is being sent.
s=replace_in_fn(s,'void InterruptibleAttackDelayFrom(',
    '&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode',
    '&&g_attackActive&&!g_cureExclusive&&!g_chatMode')
s=replace_in_fn(s,'void MaybeSendWsCombo(',
    '||g_cureExclusive||g_potionExclusive||g_chatMode||!g_attackActive||!skillAt',
    '||g_cureExclusive||g_chatMode||!g_attackActive||!skillAt')
s=replace_in_fn(s,'void MaybeSendWsCombo(',
    'if(g_wsPending.pending)return; // one animation-cancel job at a time; never build a W/S backlog\n  g_wsPending.pending=true;',
    'if(g_wsPending.pending)return; // one animation-cancel job at a time; never build a W/S backlog\n  g_wsPriority=true;\n  g_wsPending.pending=true;')

# WsWorker must keep its already-reserved timing job alive when a pot is pending.
a,b=span(s,'void WsWorker()')
ws=s[a:b]
ws=ws.replace('||g_potionExclusive','').replace('&&!g_potionExclusive','')
s=s[:a]+ws+s[b:]

# ExecuteAttack owns priority from before Z until the skill has been sent and a
# W/S reservation (if enabled) has been published. This closes the old race in
# which side inputs could obtain FIFO tickets between Z/bar/skill phases.
a,b=span(s,'void ExecuteAttack(const AttackSettings& a)')
old_body=s[a:b]
new_body='''void ExecuteAttack(const AttackSettings& a){
  if(!g_running||!g_attackActive||g_cureExclusive||g_chatMode)return;
  g_attackExclusive=true;
  if(a.zCombo&&AttackZEnabledNow()){
    ReferenceTapKey('Z');
    if(g_cureExclusive||g_chatMode){g_attackExclusive=false;return;}
  }
  int i=NextEnabledSkill(a);if(i<0){g_attackExclusive=false;return;}const int wantedBar=a.attackBars[i];LONGLONG skillAt=0;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_chatMode){
    int knownBar=g_attackKnownBar.load(std::memory_order_relaxed);
    if(AttackNeedsBarTap(knownBar,wantedBar)){DirectTimedTapUnlocked(BarToVk(wantedBar),12000,1000);g_attackKnownBar=wantedBar;PreciseDelayUs(30000);}
    skillAt=AttackQpcNow();ReferenceTapKeyUnlocked(SlotToVk(a.slots[i]));
    if(wantedBar!=a.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}
  }}
  if(!skillAt||g_cureExclusive||g_chatMode){g_attackExclusive=false;return;}
  MaybeSendWsCombo(a,skillAt);
  g_attackExclusive=false;
  if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode)InterruptibleAttackDelayFrom(skillAt,a.skillDelayMs[i]);
}'''
s=s[:a]+new_body+s[b:]

# W/S completion waits are no longer cancelled by a pot request.
a,b=span(s,'bool WaitWsCycleCompletion(const AttackSettings& a)')
w=s[a:b]
w=w.replace('&&!g_potionExclusive','')
s=s[:a]+w+s[b:]

# AttackWorker now resets state only when ATTACK itself is actually disabled,
# power/category is off, or chat mode owns input. Cure remains a transient hard
# exclusion, but it no longer forces an ATTACK state reset. Potion is side-band.
a,b=span(s,'void AttackWorker()')
old_attack_worker=s[a:b]
new_attack_worker='''void AttackWorker(){bool wasReady=false;while(g_running){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}bool ready=r.powerEnabled&&g_attackCategoryEnabled&&g_attackActive&&!g_chatMode;if(!ready){wasReady=false;g_attackKnownBar=0;ClearWsPending();Sleep(1);continue;}if(g_cureExclusive){Sleep(1);continue;}if(!wasReady){g_attackKnownBar=0;ClearWsPending();wasReady=true;}ExecuteAttack(a);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode){if(a.wCombo||a.sCombo)WaitWsCycleCompletion(a);else InterruptibleAttackDelay(a.delayMs);}}g_attackKnownBar=0;ClearWsPending();}'''
s=s[:a]+new_attack_worker+s[b:]

# 3) HP/MP pot is opportunistic side-input: if ATTACK is in its critical input
# phase or W/S has reserved timing, pot simply retries on the next Vitals tick.
# This prevents a pot producer from queueing in front of ATTACK. The live enable
# state is re-read before and after acquiring the input gate, so unchecking HP/MP
# stops further pot sends immediately (apart from a key event already sent).
insert_sig='bool UsePotion(bool hp,const AttackSettings&a)'
a,_=span(s,insert_sig)
helpers='''bool AttackSideInputReserved(){return g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire));}
bool PotionEnabledNow(bool hp){std::lock_guard<std::mutex>lk(g_settingsMutex);return hp?g_attack.hpEnabled:g_attack.mpEnabled;}
'''
s=s[:a]+helpers+s[a:]
a,b=span(s,insert_sig)
new_potion='''bool UsePotion(bool hp,const AttackSettings&a){
  if(g_cureExclusive||g_chatMode||AttackSideInputReserved()||!PotionEnabledNow(hp))return false;
  bool expected=false;if(!g_potionExclusive.compare_exchange_strong(expected,true))return false;
  int bar=hp?a.hpBar:a.mpBar,slot=hp?a.hpSlot:a.mpSlot;bool ok=false;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_chatMode&&!AttackSideInputReserved()&&PotionEnabledNow(hp)){
    if(bar!=a.restoreBar){ok=DirectTimedTapUnlocked(BarToVk(bar),1800,120);PreciseDelayUs(10000);}else ok=true;
    if(ok){ok=DirectTimedTapUnlocked(SlotToVk(slot),2300,160);PreciseDelayUs(36000);}
    if(bar!=a.restoreBar){DirectTimedTapUnlocked(BarToVk(a.restoreBar),1800,120);if(g_attackActive.load(std::memory_order_relaxed))g_attackKnownBar=a.restoreBar;}
  }}
  g_potionExclusive=false;return ok;
}'''
s=s[:a]+new_potion+s[b:]

# 4) HP/MP enable toggles are runtime-effective already; persist them on the
# toggle event as well so OFF remains OFF after restart. Other live edit fields
# keep the established non-persist-until-save behavior.
old='if(g_ui.saveAttack&&((id>=1540&&id<=1595)||(id>=1610&&id<=1613)))ReadAttackUi(false);'
new='if(g_ui.saveAttack&&((id>=1540&&id<=1595)||(id>=1610&&id<=1613))){const bool persistToggle=(id==IDC_HP_CHECK||id==IDC_MP_CHECK);ReadAttackUi(persistToggle);}'
if s.count(old)!=1:
    raise SystemExit(f'WM_COMMAND live-read marker count={s.count(old)}')
s=s.replace(old,new,1)

# Hard invariants for this surgical patch.
for sig in ['void CureWorker()','void RWorker()','void VitalsWorker()','void CreateRoguePage()','void CreateAttackPage()']:
    span(s,sig)
if 'g_wsPriority=true;' not in s:
    raise SystemExit('WS_PRIORITY_NOT_PUBLISHED')
if 'AttackSideInputReserved()' not in s or 'PotionEnabledNow(hp)' not in s:
    raise SystemExit('SIDEINPUT_GUARD_MISSING')

p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_ATTACK_SIDEINPUT_FIX=APPLIED')
