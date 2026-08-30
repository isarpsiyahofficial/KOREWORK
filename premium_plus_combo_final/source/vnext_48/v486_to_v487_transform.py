from pathlib import Path
import sys, hashlib

p=Path(sys.argv[1] if len(sys.argv)>1 else 'main.cpp')
s=p.read_text(encoding='utf-8')

def once(old,new,label):
    global s
    n=s.count(old)
    if n!=1:
        raise SystemExit(f'{label}: expected 1 match, got {n}')
    s=s.replace(old,new,1)

def replace_fn(signature,new_text):
    global s
    start=s.index(signature)
    brace=s.index('{',start)
    depth=0
    end=None
    for i in range(brace,len(s)):
        if s[i]=='{': depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:
                end=i+1
                break
    if end is None: raise SystemExit('unterminated '+signature)
    s=s[:start]+new_text+s[end:]

once('g_attack.wDelayMs=ClampD(ReadDword(k,L"WMs",400),1,1000);',
     'g_attack.wDelayMs=ClampD(ReadDword(k,L"WMs",400),400,1000);','registry W floor')

once('int NextEnabledSkill(const AttackSettings& a){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;unsigned turn=g_skillTurn.fetch_add(1,std::memory_order_relaxed);return enabled[turn%(unsigned)n];}',
'''int EnabledSkillAtTurn(const AttackSettings& a,unsigned turn){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;return enabled[turn%(unsigned)n];}\nint CurrentEnabledSkill(const AttackSettings& a){return EnabledSkillAtTurn(a,g_skillTurn.load(std::memory_order_relaxed));}\nvoid CommitSkillTurn(){g_skillTurn.fetch_add(1,std::memory_order_relaxed);}\nint NextEnabledSkill(const AttackSettings& a){int i=CurrentEnabledSkill(a);if(i>=0)CommitSkillTurn();return i;}\nconstexpr int kSkillCommitMs=400;''','strict selector')

replace_fn('void ExecuteAttack(const AttackSettings& a)',r'''struct AttackCycleStart { int skill=-1; int wantedBar=0; LONGLONG skillAt=0; };
AttackCycleStart ExecuteAttack(const AttackSettings& a){
  AttackCycleStart out{};
  if(!g_running||!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode)return out;
  if(a.zCombo&&AttackZEnabledNow()){ReferenceTapKey('Z');if(g_cureExclusive||g_potionExclusive||g_chatMode)return out;}
  int i=CurrentEnabledSkill(a);if(i<0)return out;const int wantedBar=a.attackBars[i];
  g_attackExclusive=true;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
    int knownBar=g_attackKnownBar.load(std::memory_order_relaxed);
    if(AttackNeedsBarTap(knownBar,wantedBar)){DirectTimedTapUnlocked(BarToVk(wantedBar),12000,1000);g_attackKnownBar=wantedBar;PreciseDelayUs(30000);}
    DirectTimedTapUnlocked(SlotToVk(a.slots[i]),18000,1500);out.skillAt=AttackQpcNow();out.skill=i;out.wantedBar=wantedBar;
  }}
  g_attackExclusive=false;
  if(!out.skillAt||g_cureExclusive||g_potionExclusive||g_chatMode)return AttackCycleStart{};
  MaybeSendWsCombo(a,out.skillAt);
  if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode)InterruptibleAttackDelayFrom(out.skillAt,a.skillDelayMs[i]);
  return out;
}''')

replace_fn('void AttackWorker()',r'''void RestoreAttackBarAfterCycle(const AttackSettings& a){
  int known=g_attackKnownBar.load(std::memory_order_relaxed);if(known==a.restoreBar)return;
  FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_potionExclusive&&!g_chatMode&&g_attackActive){DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}
}
bool RunOneAttackCycle(const AttackSettings& a){
  AttackCycleStart cycle=ExecuteAttack(a);if(cycle.skill<0||!cycle.skillAt)return false;
  const bool combo=a.wCombo||a.sCombo;bool completed=true;
  if(combo)completed=WaitWsCycleCompletion(a);
  if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
    if(combo)InterruptibleAttackDelayFrom(cycle.skillAt,kSkillCommitMs);
    else InterruptibleAttackDelay(std::max(a.delayMs,a.skillDelayMs[cycle.skill]));
  }
  if(!completed||!g_running||!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode)return false;
  RestoreAttackBarAfterCycle(a);
  if(!g_running||!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode)return false;
  CommitSkillTurn();return true;
}
void AttackWorker(){bool wasReady=false;while(g_running){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}bool ready=r.powerEnabled&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode;if(!ready){wasReady=false;g_attackKnownBar=0;Sleep(1);continue;}if(!wasReady){g_attackKnownBar=0;ClearWsPending();wasReady=true;}if(!RunOneAttackCycle(a))Sleep(1);}g_attackKnownBar=0;ClearWsPending();}''')

once('n.wDelayMs=GetInt(g_ui.wDelay,n.wDelayMs,1,1000);',
     'n.wDelayMs=GetInt(g_ui.wDelay,n.wDelayMs,400,1000);','UI W floor')

# Observer harness follows the same committed runtime cycle but otherwise remains untouched.
s=s.replace('ExecuteAttack(a);WaitWsCycleCompletion(a);','RunOneAttackCycle(a);')
s=s.replace('ExecuteAttack(a);g_attackActive=false;','RunOneAttackCycle(a);g_attackActive=false;')

p.write_text(s,encoding='utf-8',newline='\n')
print(hashlib.sha256(p.read_bytes()).hexdigest())
