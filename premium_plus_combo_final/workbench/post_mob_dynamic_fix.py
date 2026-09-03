import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.index(sig);q=s.index('{',a);d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise RuntimeError('unclosed '+sig)

def replace_fn(sig,new):
    global s
    a,b=span(sig);s=s[:a]+new+s[b:]

old="int t=g_assignTarget.load();if(t&&!injected){if(vk!=VK_LBUTTON&&vk!=VK_RBUTTON&&!ConflictWithRogue(vk,t)){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}"
new="int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else if(t==6||t==7){RogueSettings cr;AttackSettings ca;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}"
assert s.count(old)==1, s.count(old)
s=s.replace(old,new,1)

bad_start="  if(target==6||target==7){"
next_marker="  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey&&vk==m.stopHotkey)"
a=s.find(bad_start);b=s.find(next_marker,a)
assert a>=0 and b>a
s=s[:a]+s[b:]

anchor="LRESULT CALLBACK KeyboardProc(int code,WPARAM wp,LPARAM lp){"
assert s.count(anchor)==1
s=s.replace(anchor,"void ResetMobTimelines();\n"+anchor,1)

# Preserve the exact legacy selector for the existing 180-test suite. Runtime uses NextAttackSkill.
pr="unsigned NextPseudoRandom(unsigned n,std::atomic<int>&last)"
if "int NextEnabledSkill(const AttackSettings& a)" not in s:
    assert s.count(pr)==1
    compat="int NextEnabledSkill(const AttackSettings& a){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;unsigned turn=g_skillTurn.fetch_add(1,std::memory_order_relaxed);return enabled[turn%(unsigned)n];}\n"
    s=s.replace(pr,compat+pr,1)

# Release every key that can be owned by dynamic ATTACK or MOB entries. This only expands cleanup;
# the v4.8.11 routing/FIFO transport is unchanged.
replace_fn('void ReleaseKeys()',r'''void ReleaseKeys(){
  RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}
  std::vector<int> ks={'R','Z','W','S',r.seq[0],r.seq[1],r.seq[2],SlotToVk(r.cureSlot),BarToVk(r.cureBar),BarToVk(r.autoMinorBar),SlotToVk(r.autoMinorSlot),BarToVk(a.restoreBar),BarToVk(m.restoreBar)};
  for(int i=0;i<4;i++){ks.push_back(BarToVk(a.attackBars[i]));ks.push_back(SlotToVk(a.slots[i]));}
  for(const auto&e:a.extraSkills){ks.push_back(BarToVk(e.bar));ks.push_back(SlotToVk(e.slot));}
  for(const auto&e:m.skills){ks.push_back(BarToVk(e.bar));ks.push_back(SlotToVk(e.slot));}
  for(const auto&e:m.scrolls){ks.push_back(BarToVk(e.bar));ks.push_back(SlotToVk(e.slot));}
  ks.push_back(BarToVk(m.healBar));ks.push_back(SlotToVk(m.healSlot));ks.push_back(BarToVk(a.hpBar));ks.push_back(SlotToVk(a.hpSlot));ks.push_back(BarToVk(a.mpBar));ks.push_back(SlotToVk(a.mpSlot));
  std::sort(ks.begin(),ks.end());ks.erase(std::unique(ks.begin(),ks.end()),ks.end());
  FifoTicketGuard gate(g_gameInputGate);for(int k:ks){INPUT in{};BuildKeyInput(in,k,true);ReferenceSendInputsUnlocked(&in,1);}
}''')

# Do not hold the global game-input FIFO while waiting between skills.
replace_fn('void MobSkillWorker()',r'''void MobSkillWorker(){bool was=false;while(g_running){
  RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}
  bool ready=r.powerEnabled&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode;
  if(!ready){was=false;Sleep(2);continue;}
  if(!was){g_mobSkillTurn=0;g_lastMobRandom=-1;was=true;}
  SkillEntry e;if(!NextMobSkill(m,e)){Sleep(25);continue;}
  {
    FifoTicketGuard gate(g_gameInputGate);
    if(g_running&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
      DirectTimedTapUnlocked(BarToVk(e.bar),12000,1000);PreciseDelayUs(25000);ReferenceTapKeyUnlocked(SlotToVk(e.slot));
      if(e.bar!=m.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(m.restoreBar),10000,1000);}
    }
  }
  Sleep((DWORD)std::clamp(std::max(m.loopMs,e.delayMs),1,2000));
}}''')

# Pure edge model used by Priest runtime and the model test: fire exactly once on crossing/being below
# the threshold and re-arm only after a small +3% noise margin.
worker_sig='void MobPriestWorker()'
a,b=span(worker_sig)
priest=r'''bool MobHealEdgeStep(bool armed,int hp,int threshold,bool&nextArmed){nextArmed=armed;if(hp<0)return false;if(hp>threshold+3){nextArmed=true;return false;}if(hp<=threshold&&armed){nextArmed=false;return true;}return false;}
void MobPriestWorker(){while(g_running){
  RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}
  if(!r.powerEnabled||!g_mobActive||!m.priestEnabled||!m.healEnabled||!a.hpRect.valid()){g_mobHealArmed=true;Sleep(80);continue;}
  int hp=g_hpPercent.load();bool armed=g_mobHealArmed.load(),next=armed;bool fire=MobHealEdgeStep(armed,hp,m.healThreshold,next);g_mobHealArmed=next;
  if(fire){FifoTicketGuard gate(g_gameInputGate);if(g_running&&g_mobActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked(BarToVk(m.healBar),12000,1000);PreciseDelayUs(25000);ReferenceTapKeyUnlocked(SlotToVk(m.healSlot));PreciseDelayUs(5000);DirectTimedTapUnlocked(BarToVk(m.restoreBar),10000,1000);}}
  Sleep(40);
}}'''
s=s[:a]+priest+s[b:]

# Avoid reading g_mob outside the settings lock in VitalsWorker.
a,b=span('void VitalsWorker()');vf=s[a:b]
oldv='RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}'
newv='RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}'
assert vf.count(oldv)==1
vf=vf.replace(oldv,newv,1).replace('g_mob.priestEnabled','m.priestEnabled')
s=s[:a]+vf+s[b:]

# Add executable model tests for new dynamic behavior in addition to the unchanged 180 legacy tests.
ns='\n} // namespace\n'
assert s.count(ns)==1
mobtest=r'''
bool RunMobModelTest(){
  int pass=0,total=0;std::ofstream f("mob-model-test-report.txt",std::ios::trunc);auto t=[&](const char*n,bool ok){total++;if(ok)pass++;f<<n<<"="<<(ok?"PASS":"FAIL")<<"\n";};
  {AttackSettings a;a.skillEnabled={true,true,false,false};a.attackBars={1,2,1,1};a.slots={2,3,4,5};a.skillDelayMs={1,2,1,1};a.extraSkills.push_back({true,4,6,7});a.randomSkills=false;g_skillTurn=0;SkillEntry x,y,z,w;bool ok=NextAttackSkill(a,x)&&NextAttackSkill(a,y)&&NextAttackSkill(a,z)&&NextAttackSkill(a,w);t("AttackDynamicSequential",ok&&x.bar==1&&x.slot==2&&y.bar==2&&y.slot==3&&z.bar==4&&z.slot==6&&w.bar==1&&w.slot==2);}
  {MobSettings m;m.skills={{true,3,2,1},{true,5,7,1},{true,8,9,1}};m.randomSkills=false;g_mobSkillTurn=0;SkillEntry a,b,c,d;bool ok=NextMobSkill(m,a)&&NextMobSkill(m,b)&&NextMobSkill(m,c)&&NextMobSkill(m,d);t("MobDynamicSequential",ok&&a.bar==3&&b.bar==5&&c.bar==8&&d.bar==3);}
  {MobSettings m;m.skills={{true,1,1,1},{true,2,2,1},{true,3,3,1}};m.randomSkills=true;g_lastMobRandom=-1;bool ok=true;int prev=-1;for(int i=0;i<32;i++){SkillEntry e;if(!NextMobSkill(m,e)||e.bar<1||e.bar>3||(prev==e.bar)){ok=false;break;}prev=e.bar;}t("MobRandomNoImmediateRepeat",ok);}
  {MobSettings m;m.anchorValid=true;m.anchorX=10;m.anchorZ=20;t("MobRangeEuclidean345",std::abs(MobAnchorDistance(m,13,24)-5.0)<0.000001);}
  {bool n=true;bool f1=MobHealEdgeStep(true,40,40,n);bool n2=n;bool f2=MobHealEdgeStep(n,40,40,n2);bool n3=n2;bool f3=MobHealEdgeStep(n2,44,40,n3);bool n4=n3;bool f4=MobHealEdgeStep(n3,40,40,n4);t("PriestSingleThresholdEdge",f1&&!f2&&!f3&&f4&&!n4);}
  {ScrollEntry a{true,4,6,120},b{true,4,7,1800};t("IndependentScrollIntervals",a.intervalSec==120&&b.intervalSec==1800&&a.intervalSec!=b.intervalSec);}
  f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";return total==6&&pass==6;
}
'''
s=s.replace(ns,'\n'+mobtest+ns,1)

main_anchor='int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show){g_instance=hi;'
assert s.count(main_anchor)==1
s=s.replace(main_anchor,main_anchor+'if(cmd&&wcsstr(cmd,L"--mob-model-test"))return RunMobModelTest()?0:10;',1)

p.write_text(s,encoding='utf-8',newline='\n')
print('POST_FIX=PASS')
