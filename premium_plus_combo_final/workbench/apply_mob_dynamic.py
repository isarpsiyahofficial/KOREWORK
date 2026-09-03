import pathlib, sys

src = pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'premium_plus_combo_final/workbench/makro2_v4811_exact.cpp')
out = pathlib.Path(sys.argv[2] if len(sys.argv)>2 else 'premium_plus_combo_final/workbench/makro2_v4813_mob.cpp')
s = src.read_text(encoding='utf-8')


def once(old, new, label):
    global s
    c=s.count(old)
    if c!=1:
        raise RuntimeError(f'{label}: expected 1 anchor, got {c}')
    s=s.replace(old,new,1)


def function_span(text, sig):
    p=text.index(sig); q=text.index('{',p); d=0
    for i in range(q,len(text)):
        if text[i]=='{': d+=1
        elif text[i]=='}':
            d-=1
            if d==0: return p,i+1
    raise RuntimeError('unclosed '+sig)


def edit_fn(sig, editor):
    global s
    a,b=function_span(s,sig)
    old=s[a:b]
    new=editor(old)
    if old==new: raise RuntimeError('no edit '+sig)
    s=s[:a]+new+s[b:]

# -------------------- declarations / settings --------------------
once('constexpr wchar_t kAttackRegistry[] = L"Software\\\\PremiumPlusCombo\\\\AttackV2";',
'''constexpr wchar_t kAttackRegistry[] = L"Software\\\\PremiumPlusCombo\\\\AttackV2";
constexpr wchar_t kMobRegistry[] = L"Software\\\\PremiumPlusCombo\\\\MobAttackV1";
constexpr wchar_t kPositionMappingName[] = L"Local\\\\PremiumPlusCombo.MobAttack.Position.v1";''','registries')
once('constexpr int kWindowHeight = 620;','constexpr int kWindowHeight = 700;','window height')
once('constexpr int IDC_AUTO_MINOR_CAL = 1627;', '''constexpr int IDC_AUTO_MINOR_CAL = 1627;
constexpr int IDC_CATEGORY_MOB = 1700;
constexpr int IDC_ATTACK_RANDOM = 1701;
constexpr int IDC_ATTACK_EXTRA_BAR = 1702;
constexpr int IDC_ATTACK_EXTRA_SLOT = 1703;
constexpr int IDC_ATTACK_EXTRA_MS = 1704;
constexpr int IDC_ATTACK_EXTRA_ADD = 1705;
constexpr int IDC_ATTACK_EXTRA_LIST = 1706;
constexpr int IDC_ATTACK_EXTRA_REMOVE = 1707;
constexpr int IDC_MOB_GENERAL_ENABLE = 1720;
constexpr int IDC_MOB_PRIEST_ENABLE = 1721;
constexpr int IDC_MOB_START = 1722;
constexpr int IDC_MOB_STOP = 1723;
constexpr int IDC_MOB_START_ASSIGN = 1724;
constexpr int IDC_MOB_STOP_ASSIGN = 1725;
constexpr int IDC_MOB_RANGE = 1726;
constexpr int IDC_MOB_ANCHOR = 1727;
constexpr int IDC_MOB_RANDOM = 1728;
constexpr int IDC_MOB_SKILL_BAR = 1730;
constexpr int IDC_MOB_SKILL_SLOT = 1731;
constexpr int IDC_MOB_SKILL_MS = 1732;
constexpr int IDC_MOB_SKILL_ADD = 1733;
constexpr int IDC_MOB_SKILL_LIST = 1734;
constexpr int IDC_MOB_SKILL_REMOVE = 1735;
constexpr int IDC_MOB_SCROLL_BAR = 1740;
constexpr int IDC_MOB_SCROLL_SLOT = 1741;
constexpr int IDC_MOB_SCROLL_MIN = 1742;
constexpr int IDC_MOB_SCROLL_ADD = 1743;
constexpr int IDC_MOB_SCROLL_LIST = 1744;
constexpr int IDC_MOB_SCROLL_REMOVE = 1745;
constexpr int IDC_MOB_HEAL_ENABLE = 1750;
constexpr int IDC_MOB_HEAL_PCT = 1751;
constexpr int IDC_MOB_HEAL_BAR = 1752;
constexpr int IDC_MOB_HEAL_SLOT = 1753;
constexpr int IDC_MOB_HP_CAL = 1754;
constexpr int IDC_MOB_SAVE = 1755;''','ids')
once('struct NormalizedRect {\n  int x=0,y=0,w=0,h=0;\n  bool valid() const { return w>1500 && h>1500; }\n};', '''struct NormalizedRect {
  int x=0,y=0,w=0,h=0;
  bool valid() const { return w>1500 && h>1500; }
};
struct SkillEntry { bool enabled=true; int bar=1; int slot=1; int delayMs=1; };
struct ScrollEntry { bool enabled=true; int bar=1; int slot=1; int intervalSec=1800; };
struct PositionSharedState { uint32_t magic=0; uint32_t version=0; volatile LONG64 heartbeatMs=0; double x=0,y=0,z=0; };
constexpr uint32_t kPositionMagic=0x504D4F42u; // PMOB
constexpr uint32_t kPositionVersion=1u;''','generic structs')
once('  std::array<int,4> skillDelayMs{1,1,1,1};\n  bool zCombo=false;', '  std::array<int,4> skillDelayMs{1,1,1,1};\n  std::vector<SkillEntry> extraSkills;\n  bool randomSkills=false;\n  bool zCombo=false;','attack extras')
once('};\n\nstruct Ui {\n  HWND main{},power{},catRogue{},catAttack{},status{};', '''};

struct MobSettings {
  int startHotkey=VK_F8;
  int stopHotkey=VK_F8;
  bool generalEnabled=false;
  bool priestEnabled=false;
  int leashRange=48;
  bool anchorValid=false;
  double anchorX=0,anchorZ=0;
  bool randomSkills=false;
  int restoreBar=1;
  int loopMs=125;
  std::vector<SkillEntry> skills;
  std::vector<ScrollEntry> scrolls;
  bool healEnabled=false;
  int healThreshold=40;
  int healBar=1;
  int healSlot=1;
};

struct Ui {
  HWND main{},power{},catRogue{},catAttack{},catMob{},status{};''','mob settings ui head')
once('  std::vector<HWND> roguePage,attackPage;', '  std::vector<HWND> roguePage,attackPage,mobPage;','mob page vector')
once('  HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackCategoryEnable{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},wDelay{},sDelay{};', '  HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackCategoryEnable{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},wDelay{},sDelay{},attackRandom{},attackExtraBar{},attackExtraSlot{},attackExtraMs{},attackExtraAdd{},attackExtraList{},attackExtraRemove{};','attack ui extras')
once('  HWND mpCheck{},mpThreshold{},mpBar{},mpSlot{},mpCal{},mpPercent{},saveAttack{};\n};', '''  HWND mpCheck{},mpThreshold{},mpBar{},mpSlot{},mpCal{},mpPercent{},saveAttack{};
  HWND mobGeneralEnable{},mobPriestEnable{},mobStart{},mobStop{},mobStartAssign{},mobStopAssign{},mobRange{},mobAnchor{},mobRandom{};
  HWND mobSkillBar{},mobSkillSlot{},mobSkillMs{},mobSkillAdd{},mobSkillList{},mobSkillRemove{};
  HWND mobScrollBar{},mobScrollSlot{},mobScrollMin{},mobScrollAdd{},mobScrollList{},mobScrollRemove{};
  HWND mobHealEnable{},mobHealPct{},mobHealBar{},mobHealSlot{},mobHpCal{},mobSave{},mobPosStatus{};
};''','mob ui fields')
once('AttackSettings g_attack;\nstd::mutex g_settingsMutex;', 'AttackSettings g_attack;\nMobSettings g_mob;\nstd::mutex g_settingsMutex;','mob global')
once('std::atomic<bool> g_attackCategoryEnabled{true};', '''std::atomic<bool> g_attackCategoryEnabled{true};
std::atomic<bool> g_mobActive{false};
std::atomic<unsigned> g_mobSkillTurn{0};
std::atomic<unsigned> g_randomSeed{0x6A09E667u};
std::atomic<int> g_lastAttackRandom{-1},g_lastMobRandom{-1};
std::atomic<bool> g_mobHealArmed{true};
HANDLE g_positionMap{};
PositionSharedState* g_position{};
std::mutex g_mobTimelineMutex;
std::vector<ULONGLONG> g_scrollNextDue;''','mob runtime globals')

# -------------------- registry load/save --------------------
def load_edit(fn):
    insert=r'''
  // v4.8.13 extensions are read from separate values, leaving legacy v4.8.11 values untouched.
  if(RegCreateKeyExW(HKEY_CURRENT_USER,kAttackRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){
    g_attack.randomSkills=ReadDword(k,L"RandomSkills",0)!=0;
    int n=ClampD(ReadDword(k,L"ExtraSkillCount",0),0,24);g_attack.extraSkills.clear();
    for(int i=0;i<n;i++){wchar_t a[40],b[40],c[40],d[40];wsprintfW(a,L"ExtraSkill%dEnabled",i+1);wsprintfW(b,L"ExtraSkill%dBar",i+1);wsprintfW(c,L"ExtraSkill%dSlot",i+1);wsprintfW(d,L"ExtraSkill%dMs",i+1);SkillEntry e;e.enabled=ReadDword(k,a,1)!=0;e.bar=ClampD(ReadDword(k,b,1),1,12);e.slot=ClampD(ReadDword(k,c,1),1,10);e.delayMs=ClampD(ReadDword(k,d,1),1,1000);g_attack.extraSkills.push_back(e);}RegCloseKey(k);
  }
  if(RegCreateKeyExW(HKEY_CURRENT_USER,kMobRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){
    g_mob.startHotkey=ClampD(ReadDword(k,L"StartHotkey",VK_F8),1,255);g_mob.stopHotkey=ClampD(ReadDword(k,L"StopHotkey",VK_F8),1,255);
    g_mob.generalEnabled=ReadDword(k,L"GeneralEnabled",0)!=0;g_mob.priestEnabled=ReadDword(k,L"PriestEnabled",0)!=0;g_mob.leashRange=ClampD(ReadDword(k,L"LeashRange",48),10,100);g_mob.randomSkills=ReadDword(k,L"RandomSkills",0)!=0;g_mob.restoreBar=ClampD(ReadDword(k,L"RestoreBar",1),1,12);g_mob.loopMs=ClampD(ReadDword(k,L"LoopMs",125),1,2000);
    g_mob.anchorValid=ReadDword(k,L"AnchorValid",0)!=0;DWORD ax=ReadDword(k,L"AnchorX100",0),az=ReadDword(k,L"AnchorZ100",0);g_mob.anchorX=(int32_t)ax/100.0;g_mob.anchorZ=(int32_t)az/100.0;
    g_mob.healEnabled=ReadDword(k,L"HealEnabled",0)!=0;g_mob.healThreshold=ClampD(ReadDword(k,L"HealPct",40),1,99);g_mob.healBar=ClampD(ReadDword(k,L"HealBar",1),1,12);g_mob.healSlot=ClampD(ReadDword(k,L"HealSlot",1),1,10);
    int sn=ClampD(ReadDword(k,L"SkillCount",0),0,32);g_mob.skills.clear();for(int i=0;i<sn;i++){wchar_t a[32],b[32],c[32],d[32];wsprintfW(a,L"Skill%dEnabled",i+1);wsprintfW(b,L"Skill%dBar",i+1);wsprintfW(c,L"Skill%dSlot",i+1);wsprintfW(d,L"Skill%dMs",i+1);SkillEntry e;e.enabled=ReadDword(k,a,1)!=0;e.bar=ClampD(ReadDword(k,b,1),1,12);e.slot=ClampD(ReadDword(k,c,1),1,10);e.delayMs=ClampD(ReadDword(k,d,1),1,1000);g_mob.skills.push_back(e);}
    int cn=ClampD(ReadDword(k,L"ScrollCount",0),0,32);g_mob.scrolls.clear();for(int i=0;i<cn;i++){wchar_t a[32],b[32],c[32],d[32];wsprintfW(a,L"Scroll%dEnabled",i+1);wsprintfW(b,L"Scroll%dBar",i+1);wsprintfW(c,L"Scroll%dSlot",i+1);wsprintfW(d,L"Scroll%dSec",i+1);ScrollEntry e;e.enabled=ReadDword(k,a,1)!=0;e.bar=ClampD(ReadDword(k,b,1),1,12);e.slot=ClampD(ReadDword(k,c,1),1,10);e.intervalSec=ClampD(ReadDword(k,d,1800),5,86400);g_mob.scrolls.push_back(e);}RegCloseKey(k);
  }
'''
    return fn[:-1]+insert+'}\n'
edit_fn('void LoadSettings()',load_edit)

def save_attack_edit(fn):
    marker='RegCloseKey(k);'
    p=fn.rfind(marker)
    if p<0: raise RuntimeError('SaveAttack close')
    ext=r'''WriteDword(k,L"RandomSkills",g_attack.randomSkills?1:0);WriteDword(k,L"ExtraSkillCount",(DWORD)std::min<size_t>(24,g_attack.extraSkills.size()));for(size_t i=0;i<g_attack.extraSkills.size()&&i<24;i++){wchar_t a[40],b[40],c[40],d[40];wsprintfW(a,L"ExtraSkill%dEnabled",(int)i+1);wsprintfW(b,L"ExtraSkill%dBar",(int)i+1);wsprintfW(c,L"ExtraSkill%dSlot",(int)i+1);wsprintfW(d,L"ExtraSkill%dMs",(int)i+1);WriteDword(k,a,g_attack.extraSkills[i].enabled?1:0);WriteDword(k,b,g_attack.extraSkills[i].bar);WriteDword(k,c,g_attack.extraSkills[i].slot);WriteDword(k,d,g_attack.extraSkills[i].delayMs);}'''
    return fn[:p]+ext+fn[p:]
edit_fn('void SaveAttack()',save_attack_edit)

# Add MOB persistence and optional position feed without changing the existing input bridge ABI.
once('\nbool InitBridge(){', r'''
void SaveMob(){HKEY k{};std::lock_guard<std::mutex>lk(g_settingsMutex);if(RegCreateKeyExW(HKEY_CURRENT_USER,kMobRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return;WriteDword(k,L"StartHotkey",g_mob.startHotkey);WriteDword(k,L"StopHotkey",g_mob.stopHotkey);WriteDword(k,L"GeneralEnabled",g_mob.generalEnabled?1:0);WriteDword(k,L"PriestEnabled",g_mob.priestEnabled?1:0);WriteDword(k,L"LeashRange",g_mob.leashRange);WriteDword(k,L"RandomSkills",g_mob.randomSkills?1:0);WriteDword(k,L"RestoreBar",g_mob.restoreBar);WriteDword(k,L"LoopMs",g_mob.loopMs);WriteDword(k,L"AnchorValid",g_mob.anchorValid?1:0);WriteDword(k,L"AnchorX100",(DWORD)(int32_t)std::lround(g_mob.anchorX*100.0));WriteDword(k,L"AnchorZ100",(DWORD)(int32_t)std::lround(g_mob.anchorZ*100.0));WriteDword(k,L"HealEnabled",g_mob.healEnabled?1:0);WriteDword(k,L"HealPct",g_mob.healThreshold);WriteDword(k,L"HealBar",g_mob.healBar);WriteDword(k,L"HealSlot",g_mob.healSlot);WriteDword(k,L"SkillCount",(DWORD)std::min<size_t>(32,g_mob.skills.size()));for(size_t i=0;i<g_mob.skills.size()&&i<32;i++){wchar_t a[32],b[32],c[32],d[32];wsprintfW(a,L"Skill%dEnabled",(int)i+1);wsprintfW(b,L"Skill%dBar",(int)i+1);wsprintfW(c,L"Skill%dSlot",(int)i+1);wsprintfW(d,L"Skill%dMs",(int)i+1);WriteDword(k,a,g_mob.skills[i].enabled?1:0);WriteDword(k,b,g_mob.skills[i].bar);WriteDword(k,c,g_mob.skills[i].slot);WriteDword(k,d,g_mob.skills[i].delayMs);}WriteDword(k,L"ScrollCount",(DWORD)std::min<size_t>(32,g_mob.scrolls.size()));for(size_t i=0;i<g_mob.scrolls.size()&&i<32;i++){wchar_t a[32],b[32],c[32],d[32];wsprintfW(a,L"Scroll%dEnabled",(int)i+1);wsprintfW(b,L"Scroll%dBar",(int)i+1);wsprintfW(c,L"Scroll%dSlot",(int)i+1);wsprintfW(d,L"Scroll%dSec",(int)i+1);WriteDword(k,a,g_mob.scrolls[i].enabled?1:0);WriteDword(k,b,g_mob.scrolls[i].bar);WriteDword(k,c,g_mob.scrolls[i].slot);WriteDword(k,d,g_mob.scrolls[i].intervalSec);}RegCloseKey(k);}

bool InitPositionBridge(){if(g_position)return true;HANDLE map=OpenFileMappingW(FILE_MAP_READ,FALSE,kPositionMappingName);if(!map)return false;auto* view=(PositionSharedState*)MapViewOfFile(map,FILE_MAP_READ,0,0,sizeof(PositionSharedState));if(!view){CloseHandle(map);return false;}if(view->magic!=kPositionMagic||view->version!=kPositionVersion){UnmapViewOfFile(view);CloseHandle(map);return false;}g_positionMap=map;g_position=view;return true;}
void ClosePositionBridge(){if(g_position){UnmapViewOfFile(g_position);g_position=nullptr;}if(g_positionMap){CloseHandle(g_positionMap);g_positionMap=nullptr;}}
bool ReadPosition(double&x,double&z){if(!g_position&&!InitPositionBridge())return false;LONG64 hb=InterlockedCompareExchange64(&g_position->heartbeatMs,0,0);ULONGLONG now=GetTickCount64();if(hb<=0||now<(ULONGLONG)hb||now-(ULONGLONG)hb>2000)return false;x=g_position->x;z=g_position->z;return std::isfinite(x)&&std::isfinite(z);}
double MobAnchorDistance(const MobSettings&m,double x,double z){double dx=x-m.anchorX,dz=z-m.anchorZ;return std::sqrt(dx*dx+dz*dz);}

bool InitBridge(){''','mob save position bridge')

# -------------------- ATTACK dynamic selector --------------------
once('std::vector<int> BuildAttackSequence(const AttackSettings& a){std::vector<int> v;if(a.zCombo)v.push_back(\'Z\');for(int i=0;i<4;i++)if(a.skillEnabled[i]){v.push_back(BarToVk(a.attackBars[i]));v.push_back(SlotToVk(a.slots[i]));}v.push_back(BarToVk(a.restoreBar));return v;}\nint NextEnabledSkill(const AttackSettings& a){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;unsigned turn=g_skillTurn.fetch_add(1,std::memory_order_relaxed);return enabled[turn%(unsigned)n];}', r'''std::vector<int> BuildAttackSequence(const AttackSettings& a){std::vector<int> v;if(a.zCombo)v.push_back('Z');for(int i=0;i<4;i++)if(a.skillEnabled[i]){v.push_back(BarToVk(a.attackBars[i]));v.push_back(SlotToVk(a.slots[i]));}for(const auto&e:a.extraSkills)if(e.enabled){v.push_back(BarToVk(e.bar));v.push_back(SlotToVk(e.slot));}v.push_back(BarToVk(a.restoreBar));return v;}
unsigned NextPseudoRandom(unsigned n,std::atomic<int>&last){if(n<=1)return 0;uint32_t old=g_randomSeed.load(std::memory_order_relaxed),x;do{x=old;x^=x<<13;x^=x>>17;x^=x<<5;if(!x)x=0xA341316Cu;}while(!g_randomSeed.compare_exchange_weak(old,x,std::memory_order_relaxed));unsigned idx=x%n;int prev=last.load(std::memory_order_relaxed);if((int)idx==prev)idx=(idx+1+(x%(n-1)))%n;last=(int)idx;return idx;}
bool NextAttackSkill(const AttackSettings&a,SkillEntry&out){std::vector<SkillEntry> enabled;enabled.reserve(4+a.extraSkills.size());for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled.push_back({true,a.attackBars[i],a.slots[i],a.skillDelayMs[i]});for(const auto&e:a.extraSkills)if(e.enabled)enabled.push_back(e);if(enabled.empty())return false;unsigned idx=a.randomSkills?NextPseudoRandom((unsigned)enabled.size(),g_lastAttackRandom):(g_skillTurn.fetch_add(1,std::memory_order_relaxed)%(unsigned)enabled.size());out=enabled[idx];return true;}''','attack dynamic selector')

def execute_edit(fn):
    fn=fn.replace('int i=NextEnabledSkill(a);if(i<0)return;const int wantedBar=a.attackBars[i];LONGLONG skillAt=0;','SkillEntry chosen;if(!NextAttackSkill(a,chosen))return;const int wantedBar=chosen.bar;LONGLONG skillAt=0;')
    fn=fn.replace('SlotToVk(a.slots[i])','SlotToVk(chosen.slot)')
    fn=fn.replace('a.skillDelayMs[i]','chosen.delayMs')
    return fn
edit_fn('void ExecuteAttack(const AttackSettings& a)',execute_edit)

# -------------------- MOB runtime --------------------
once('\nstruct RGBc{double r,g,b;};', r'''
void ResetMobTimelines(){std::lock_guard<std::mutex>lk(g_mobTimelineMutex);g_scrollNextDue.clear();}
bool NextMobSkill(const MobSettings&m,SkillEntry&out){std::vector<SkillEntry> e;for(const auto&s:m.skills)if(s.enabled)e.push_back(s);if(e.empty())return false;unsigned idx=m.randomSkills?NextPseudoRandom((unsigned)e.size(),g_lastMobRandom):(g_mobSkillTurn.fetch_add(1,std::memory_order_relaxed)%(unsigned)e.size());out=e[idx];return true;}
bool MobWithinLeash(const MobSettings&m,double*distance=nullptr){if(!m.anchorValid){if(distance)*distance=-1;return false;}double x=0,z=0;if(!ReadPosition(x,z)){if(distance)*distance=-1;return false;}double d=MobAnchorDistance(m,x,z);if(distance)*distance=d;return d<=m.leashRange;}
void MobSkillWorker(){bool was=false;while(g_running){RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}bool ready=r.powerEnabled&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode;if(!ready){was=false;Sleep(2);continue;}if(!was){g_mobSkillTurn=0;g_lastMobRandom=-1;was=true;}SkillEntry e;if(!NextMobSkill(m,e)){Sleep(25);continue;}FifoTicketGuard gate(g_gameInputGate);if(g_running&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked(BarToVk(e.bar),12000,1000);PreciseDelayUs(25000);ReferenceTapKeyUnlocked(SlotToVk(e.slot));if(e.bar!=m.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(m.restoreBar),10000,1000);}}Sleep((DWORD)std::clamp(std::max(m.loopMs,e.delayMs),1,2000));}}
void MobScrollWorker(){while(g_running){RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}if(!r.powerEnabled||!g_mobActive||!m.generalEnabled||g_cureExclusive||g_potionExclusive||g_chatMode){Sleep(100);continue;}ULONGLONG now=GetTickCount64();{std::lock_guard<std::mutex>tl(g_mobTimelineMutex);if(g_scrollNextDue.size()!=m.scrolls.size()){g_scrollNextDue.assign(m.scrolls.size(),now);}}for(size_t i=0;i<m.scrolls.size();i++){const auto&e=m.scrolls[i];if(!e.enabled)continue;bool due=false;{std::lock_guard<std::mutex>tl(g_mobTimelineMutex);if(i<g_scrollNextDue.size()&&now>=g_scrollNextDue[i]){due=true;g_scrollNextDue[i]=now+(ULONGLONG)e.intervalSec*1000ULL;}}if(due){FifoTicketGuard gate(g_gameInputGate);if(g_running&&g_mobActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked(BarToVk(e.bar),12000,1000);PreciseDelayUs(22000);ReferenceTapKeyUnlocked(SlotToVk(e.slot));PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(m.restoreBar),10000,1000);}}}Sleep(50);}}
void MobPriestWorker(){while(g_running){RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}if(!r.powerEnabled||!g_mobActive||!m.priestEnabled||!m.healEnabled||!a.hpRect.valid()){g_mobHealArmed=true;Sleep(80);continue;}int hp=g_hpPercent.load();if(hp>m.healThreshold+3)g_mobHealArmed=true;if(hp>=0&&hp<=m.healThreshold&&g_mobHealArmed.exchange(false)){FifoTicketGuard gate(g_gameInputGate);if(g_running&&g_mobActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked(BarToVk(m.healBar),12000,1000);PreciseDelayUs(25000);ReferenceTapKeyUnlocked(SlotToVk(m.healSlot));PreciseDelayUs(5000);DirectTimedTapUnlocked(BarToVk(m.restoreBar),10000,1000);}}Sleep(40);}}

struct RGBc{double r,g,b;};''','mob workers')

# Player HP sensing must continue when Priest is active, even if ATTACK category is disabled.
def vitals_edit(fn):
    return fn.replace('const bool attackSense=r.powerEnabled&&g_attackCategoryEnabled;','const bool attackSense=r.powerEnabled&&(g_attackCategoryEnabled||(g_mobActive.load()&&g_mob.priestEnabled));')
edit_fn('void VitalsWorker()',vitals_edit)

# -------------------- UI helpers --------------------
once('void ShowCategory(int category){bool rogue=category==0;for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);for(HWND h:g_ui.attackPage)ShowWindow(h,rogue?SW_HIDE:SW_SHOW);InvalidateRect(g_ui.main,nullptr,TRUE);}', r'''void ShowCategory(int category){bool rogue=category==0,attack=category==1,mob=category==2;for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);for(HWND h:g_ui.attackPage)ShowWindow(h,attack?SW_SHOW:SW_HIDE);for(HWND h:g_ui.mobPage)ShowWindow(h,mob?SW_SHOW:SW_HIDE);InvalidateRect(g_ui.main,nullptr,TRUE);}''','show category')
once('void RefreshHotkeyLabels(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}', 'void RefreshHotkeyLabels(){RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}','hotkey mob snapshot')
once('set(g_ui.attackStartAssign,L"Açma",a.startHotkey);set(g_ui.attackStopAssign,L"Kapatma",a.stopHotkey);SetWindowTextW(g_ui.minorStart', 'set(g_ui.attackStartAssign,L"Açma",a.startHotkey);set(g_ui.attackStopAssign,L"Kapatma",a.stopHotkey);if(g_ui.mobStartAssign)set(g_ui.mobStartAssign,L"Açma",m.startHotkey);if(g_ui.mobStopAssign)set(g_ui.mobStopAssign,L"Kapatma",m.stopHotkey);SetWindowTextW(g_ui.minorStart','hotkey labels mob')
once('s+=L"   |   Attack: "+std::wstring(g_attackActive.load()?L"ÇALIŞIYOR":L"Hazır");SetWindowTextW(g_ui.status,s.c_str());', 's+=L"   |   Attack: "+std::wstring(g_attackActive.load()?L"ÇALIŞIYOR":L"Hazır");s+=L"   |   Mob: "+std::wstring(g_mobActive.load()?L"ÇALIŞIYOR":L"Hazır");SetWindowTextW(g_ui.status,s.c_str());','status mob')
once('if(!on){g_autoMinorLatched=false;g_autoMinorOwned=false;g_minorActive=false;g_attackActive=false;ReleaseKeys();}', 'if(!on){g_autoMinorLatched=false;g_autoMinorOwned=false;g_minorActive=false;g_attackActive=false;g_mobActive=false;ReleaseKeys();}','power mob off')

# list refresh + draft handlers
once('void ReadRogueUi(bool warn){', r'''void RefreshAttackExtraList(){if(!g_ui.attackExtraList)return;SendMessageW(g_ui.attackExtraList,LB_RESETCONTENT,0,0);AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);a=g_attack;}for(size_t i=0;i<a.extraSkills.size();i++){const auto&e=a.extraSkills[i];std::wstring t=L"Ek "+std::to_wstring(i+1)+L"  F"+std::to_wstring(e.bar)+L" / "+(e.slot==10?L"0":std::to_wstring(e.slot))+L"  "+std::to_wstring(e.delayMs)+L"ms";SendMessageW(g_ui.attackExtraList,LB_ADDSTRING,0,(LPARAM)t.c_str());}}
void RefreshMobLists(){if(g_ui.mobSkillList){SendMessageW(g_ui.mobSkillList,LB_RESETCONTENT,0,0);MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);m=g_mob;}for(size_t i=0;i<m.skills.size();i++){auto&e=m.skills[i];std::wstring t=L"Skill "+std::to_wstring(i+1)+L"  F"+std::to_wstring(e.bar)+L" / "+(e.slot==10?L"0":std::to_wstring(e.slot))+L"  "+std::to_wstring(e.delayMs)+L"ms";SendMessageW(g_ui.mobSkillList,LB_ADDSTRING,0,(LPARAM)t.c_str());}SendMessageW(g_ui.mobScrollList,LB_RESETCONTENT,0,0);for(size_t i=0;i<m.scrolls.size();i++){auto&e=m.scrolls[i];std::wstring t=L"SC "+std::to_wstring(i+1)+L"  F"+std::to_wstring(e.bar)+L" / "+(e.slot==10?L"0":std::to_wstring(e.slot))+L"  "+std::to_wstring(e.intervalSec/60)+L" dk";SendMessageW(g_ui.mobScrollList,LB_ADDSTRING,0,(LPARAM)t.c_str());}}}
void AddAttackExtra(){SkillEntry e;e.bar=ComboVal(g_ui.attackExtraBar,1,12,1);e.slot=ComboVal(g_ui.attackExtraSlot,1,10,1);e.delayMs=GetInt(g_ui.attackExtraMs,1,1,1000);{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_attack.extraSkills.size()<24)g_attack.extraSkills.push_back(e);}SaveAttack();RefreshAttackExtraList();}
void RemoveAttackExtra(){int i=(int)SendMessageW(g_ui.attackExtraList,LB_GETCURSEL,0,0);if(i<0)return;{std::lock_guard<std::mutex>lk(g_settingsMutex);if(i<(int)g_attack.extraSkills.size())g_attack.extraSkills.erase(g_attack.extraSkills.begin()+i);}SaveAttack();RefreshAttackExtraList();}
void AddMobSkill(){SkillEntry e;e.bar=ComboVal(g_ui.mobSkillBar,1,12,1);e.slot=ComboVal(g_ui.mobSkillSlot,1,10,1);e.delayMs=GetInt(g_ui.mobSkillMs,1,1,1000);{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_mob.skills.size()<32)g_mob.skills.push_back(e);}SaveMob();RefreshMobLists();}
void RemoveMobSkill(){int i=(int)SendMessageW(g_ui.mobSkillList,LB_GETCURSEL,0,0);if(i<0)return;{std::lock_guard<std::mutex>lk(g_settingsMutex);if(i<(int)g_mob.skills.size())g_mob.skills.erase(g_mob.skills.begin()+i);}SaveMob();RefreshMobLists();}
void AddMobScroll(){ScrollEntry e;e.bar=ComboVal(g_ui.mobScrollBar,1,12,1);e.slot=ComboVal(g_ui.mobScrollSlot,1,10,1);e.intervalSec=GetInt(g_ui.mobScrollMin,30,1,1440)*60;{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_mob.scrolls.size()<32)g_mob.scrolls.push_back(e);}ResetMobTimelines();SaveMob();RefreshMobLists();}
void RemoveMobScroll(){int i=(int)SendMessageW(g_ui.mobScrollList,LB_GETCURSEL,0,0);if(i<0)return;{std::lock_guard<std::mutex>lk(g_settingsMutex);if(i<(int)g_mob.scrolls.size())g_mob.scrolls.erase(g_mob.scrolls.begin()+i);}ResetMobTimelines();SaveMob();RefreshMobLists();}
void ReadMobUi(bool persist=true){MobSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_mob;}n.generalEnabled=SendMessageW(g_ui.mobGeneralEnable,BM_GETCHECK,0,0)==BST_CHECKED;n.priestEnabled=SendMessageW(g_ui.mobPriestEnable,BM_GETCHECK,0,0)==BST_CHECKED;n.leashRange=GetInt(g_ui.mobRange,n.leashRange,10,100);n.randomSkills=SendMessageW(g_ui.mobRandom,BM_GETCHECK,0,0)==BST_CHECKED;n.healEnabled=SendMessageW(g_ui.mobHealEnable,BM_GETCHECK,0,0)==BST_CHECKED;n.healThreshold=GetInt(g_ui.mobHealPct,n.healThreshold,1,99);n.healBar=ComboVal(g_ui.mobHealBar,1,12,n.healBar);n.healSlot=ComboVal(g_ui.mobHealSlot,1,10,n.healSlot);{std::lock_guard<std::mutex>lk(g_settingsMutex);g_mob=n;}if(persist)SaveMob();}
void PopulateMobUi(){if(!g_ui.mobGeneralEnable)return;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);m=g_mob;}SendMessageW(g_ui.mobGeneralEnable,BM_SETCHECK,m.generalEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.mobPriestEnable,BM_SETCHECK,m.priestEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.mobRandom,BM_SETCHECK,m.randomSkills?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.mobRange,std::to_wstring(m.leashRange).c_str());SendMessageW(g_ui.mobHealEnable,BM_SETCHECK,m.healEnabled?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.mobHealPct,std::to_wstring(m.healThreshold).c_str());SetCombo(g_ui.mobHealBar,m.healBar,1);SetCombo(g_ui.mobHealSlot,m.healSlot,1);RefreshMobLists();}

void ReadRogueUi(bool warn){''','list helpers')

# Attack UI read/populate mode.
def read_attack_edit(fn):
    return fn.replace('n.restoreBar=ComboVal(g_ui.restoreBar,1,12,n.restoreBar);','n.restoreBar=ComboVal(g_ui.restoreBar,1,12,n.restoreBar);n.randomSkills=SendMessageW(g_ui.attackRandom,BM_GETCHECK,0,0)==BST_CHECKED;')
edit_fn('void ReadAttackUi(bool persist=true)',read_attack_edit)
once('SetWindowTextW(g_ui.sDelay,std::to_wstring(a.sDelayMs).c_str());for(int i=0;i<4;i++){', 'SetWindowTextW(g_ui.sDelay,std::to_wstring(a.sDelayMs).c_str());SendMessageW(g_ui.attackRandom,BM_SETCHECK,a.randomSkills?BST_CHECKED:BST_UNCHECKED,0);for(int i=0;i<4;i++){','populate random')
once('SendMessageW(g_ui.mpSlot,CB_SETCURSEL', 'SendMessageW(g_ui.mpSlot,CB_SETCURSEL','noop guard') if False else None
# PopulateUi ends with RefreshHotkeyLabels; add list refresh there.
def populate_edit(fn):
    return fn.replace('RefreshHotkeyLabels();RefreshStatus();','RefreshAttackExtraList();PopulateMobUi();RefreshHotkeyLabels();RefreshStatus();')
edit_fn('void PopulateUi()',populate_edit)

# Attack page: preserve the original four rows; append an extra-skill editor and move HP/MP lower.
def attack_page_edit(fn):
    anchor='}int y=399;'
    extra=r'''}g_ui.attackRandom=Ctrl(L"BUTTON",L"Random kullan",BS_AUTOCHECKBOX,kContentX+455,239,120,22,IDC_ATTACK_RANDOM,g_fontSmall);PageAdd(p,g_ui.attackRandom);PageAdd(p,Label(L"Ek skill",kContentX,397,60,22,g_fontBold));PageAdd(p,Label(L"Bar",kContentX+70,397,28,22,g_fontSmall));g_ui.attackExtraBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+100,396,58,180,IDC_ATTACK_EXTRA_BAR,g_fontSmall);FillBar(g_ui.attackExtraBar);PageAdd(p,g_ui.attackExtraBar);PageAdd(p,Label(L"Slot",kContentX+166,397,30,22,g_fontSmall));g_ui.attackExtraSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+198,396,55,180,IDC_ATTACK_EXTRA_SLOT,g_fontSmall);FillSlot(g_ui.attackExtraSlot);PageAdd(p,g_ui.attackExtraSlot);PageAdd(p,Label(L"ms",kContentX+261,397,20,22,g_fontSmall));g_ui.attackExtraMs=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+282,396,42,22,IDC_ATTACK_EXTRA_MS,g_fontSmall);PageAdd(p,g_ui.attackExtraMs);g_ui.attackExtraAdd=Ctrl(L"BUTTON",L"+ Skill Ekle",BS_PUSHBUTTON,kContentX+334,395,94,25,IDC_ATTACK_EXTRA_ADD,g_fontSmall);PageAdd(p,g_ui.attackExtraAdd);g_ui.attackExtraRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+434,395,45,25,IDC_ATTACK_EXTRA_REMOVE,g_fontSmall);PageAdd(p,g_ui.attackExtraRemove);g_ui.attackExtraList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX+488,392,132,72,IDC_ATTACK_EXTRA_LIST,g_fontSmall);PageAdd(p,g_ui.attackExtraList);int y=476;'''
    if anchor not in fn: raise RuntimeError('attack y anchor')
    fn=fn.replace(anchor,extra,1)
    fn=fn.replace('y=430;','y=507;',1)
    fn=fn.replace('kContentX,475,250,30,1599','kContentX,548,250,30,1599',1)
    return fn
edit_fn('void CreateAttackPage()',attack_page_edit)

# MOB page is separate, keeping ATTACK controls isolated.
once('void LayoutChrome(){', r'''void CreateMobPage(){auto&p=g_ui.mobPage;PageAdd(p,Label(L"MOB ATTACK",kContentX,84,220,26,g_fontBold));g_ui.mobGeneralEnable=Ctrl(L"BUTTON",L"GENEL AKTİF",BS_AUTOCHECKBOX,kContentX+360,84,110,24,IDC_MOB_GENERAL_ENABLE,g_fontSmall);PageAdd(p,g_ui.mobGeneralEnable);g_ui.mobPriestEnable=Ctrl(L"BUTTON",L"PRIEST AKTİF",BS_AUTOCHECKBOX,kContentX+480,84,120,24,IDC_MOB_PRIEST_ENABLE,g_fontSmall);PageAdd(p,g_ui.mobPriestEnable);
PageAdd(p,Label(L"Ortak Kontrol",kContentX,116,120,22,g_fontBold));g_ui.mobStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,142,92,29,IDC_MOB_START,g_fontBold);PageAdd(p,g_ui.mobStart);g_ui.mobStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+98,142,92,29,IDC_MOB_STOP,g_fontBold);PageAdd(p,g_ui.mobStop);g_ui.mobStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+200,142,150,29,IDC_MOB_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStartAssign);g_ui.mobStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+358,142,150,29,IDC_MOB_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStopAssign);
PageAdd(p,Label(L"Range",kContentX+516,143,38,24,g_fontSmall));g_ui.mobRange=Ctrl(L"EDIT",L"48",WS_BORDER|ES_CENTER,kContentX+555,143,38,24,IDC_MOB_RANGE,g_fontSmall);PageAdd(p,g_ui.mobRange);g_ui.mobAnchor=Ctrl(L"BUTTON",L"ANKOR AL",BS_PUSHBUTTON,kContentX+514,174,80,25,IDC_MOB_ANCHOR,g_fontSmall);PageAdd(p,g_ui.mobAnchor);g_ui.mobPosStatus=Label(L"POS: bekleniyor",kContentX+390,174,118,24,g_fontSmall);PageAdd(p,g_ui.mobPosStatus);
PageAdd(p,Label(L"GENEL / SKILL",kContentX,207,120,22,g_fontBold));g_ui.mobRandom=Ctrl(L"BUTTON",L"Random kullan",BS_AUTOCHECKBOX,kContentX+125,207,115,22,IDC_MOB_RANDOM,g_fontSmall);PageAdd(p,g_ui.mobRandom);PageAdd(p,Label(L"Bar",kContentX,235,28,22,g_fontSmall));g_ui.mobSkillBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+30,234,58,180,IDC_MOB_SKILL_BAR,g_fontSmall);FillBar(g_ui.mobSkillBar);PageAdd(p,g_ui.mobSkillBar);PageAdd(p,Label(L"Slot",kContentX+96,235,30,22,g_fontSmall));g_ui.mobSkillSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+128,234,55,180,IDC_MOB_SKILL_SLOT,g_fontSmall);FillSlot(g_ui.mobSkillSlot);PageAdd(p,g_ui.mobSkillSlot);PageAdd(p,Label(L"ms",kContentX+191,235,20,22,g_fontSmall));g_ui.mobSkillMs=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+213,234,42,22,IDC_MOB_SKILL_MS,g_fontSmall);PageAdd(p,g_ui.mobSkillMs);g_ui.mobSkillAdd=Ctrl(L"BUTTON",L"+ Skill Ekle",BS_PUSHBUTTON,kContentX+265,233,92,25,IDC_MOB_SKILL_ADD,g_fontSmall);PageAdd(p,g_ui.mobSkillAdd);g_ui.mobSkillRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+363,233,42,25,IDC_MOB_SKILL_REMOVE,g_fontSmall);PageAdd(p,g_ui.mobSkillRemove);g_ui.mobSkillList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX,265,405,78,IDC_MOB_SKILL_LIST,g_fontSmall);PageAdd(p,g_ui.mobSkillList);
PageAdd(p,Label(L"GENEL / SCROLL",kContentX+420,207,150,22,g_fontBold));PageAdd(p,Label(L"Bar",kContentX+420,235,28,22,g_fontSmall));g_ui.mobScrollBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+450,234,55,180,IDC_MOB_SCROLL_BAR,g_fontSmall);FillBar(g_ui.mobScrollBar);PageAdd(p,g_ui.mobScrollBar);PageAdd(p,Label(L"Slot",kContentX+510,235,28,22,g_fontSmall));g_ui.mobScrollSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+540,234,52,180,IDC_MOB_SCROLL_SLOT,g_fontSmall);FillSlot(g_ui.mobScrollSlot);PageAdd(p,g_ui.mobScrollSlot);PageAdd(p,Label(L"dk",kContentX+420,265,24,22,g_fontSmall));g_ui.mobScrollMin=Ctrl(L"EDIT",L"30",WS_BORDER|ES_CENTER,kContentX+446,264,42,22,IDC_MOB_SCROLL_MIN,g_fontSmall);PageAdd(p,g_ui.mobScrollMin);g_ui.mobScrollAdd=Ctrl(L"BUTTON",L"+ Scroll Ekle",BS_PUSHBUTTON,kContentX+494,263,98,25,IDC_MOB_SCROLL_ADD,g_fontSmall);PageAdd(p,g_ui.mobScrollAdd);g_ui.mobScrollRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+550,293,42,24,IDC_MOB_SCROLL_REMOVE,g_fontSmall);PageAdd(p,g_ui.mobScrollRemove);g_ui.mobScrollList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX+420,323,172,90,IDC_MOB_SCROLL_LIST,g_fontSmall);PageAdd(p,g_ui.mobScrollList);
PageAdd(p,Label(L"PRIEST",kContentX,365,90,22,g_fontBold));g_ui.mobHealEnable=Ctrl(L"BUTTON",L"HP altına düşünce Heal",BS_AUTOCHECKBOX,kContentX,393,160,24,IDC_MOB_HEAL_ENABLE,g_fontSmall);PageAdd(p,g_ui.mobHealEnable);PageAdd(p,Label(L"%",kContentX+170,394,16,22,g_fontSmall));g_ui.mobHealPct=Ctrl(L"EDIT",L"40",WS_BORDER|ES_CENTER,kContentX+188,393,40,22,IDC_MOB_HEAL_PCT,g_fontSmall);PageAdd(p,g_ui.mobHealPct);PageAdd(p,Label(L"Bar",kContentX+238,394,28,22,g_fontSmall));g_ui.mobHealBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+268,393,58,180,IDC_MOB_HEAL_BAR,g_fontSmall);FillBar(g_ui.mobHealBar);PageAdd(p,g_ui.mobHealBar);PageAdd(p,Label(L"Slot",kContentX+334,394,30,22,g_fontSmall));g_ui.mobHealSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+366,393,55,180,IDC_MOB_HEAL_SLOT,g_fontSmall);FillSlot(g_ui.mobHealSlot);PageAdd(p,g_ui.mobHealSlot);g_ui.mobHpCal=Ctrl(L"BUTTON",L"HP Kalibre",BS_PUSHBUTTON,kContentX+432,392,88,25,IDC_MOB_HP_CAL,g_fontSmall);PageAdd(p,g_ui.mobHpCal);
PageAdd(p,Label(L"Range modeli: OpenKO dünya X/Z mesafesi. Pozisyon beslemesi yoksa W-kovalama yapılmaz; skill/scroll bağımsızdır.",kContentX,442,590,42,g_fontSmall));g_ui.mobSave=Ctrl(L"BUTTON",L"MOB ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,494,280,32,IDC_MOB_SAVE,g_fontBold);PageAdd(p,g_ui.mobSave);PopulateMobUi();}

void LayoutChrome(){''','mob page create')

# Keep status anchored to bottom after height increase.
def layout_edit(fn):
    return fn.replace('MoveWindow(g_ui.status,kContentX,556,','MoveWindow(g_ui.status,kContentX,std::max(556,ch-44),')
edit_fn('void LayoutChrome()',layout_edit)

def create_controls_edit(fn):
    fn=fn.replace('g_ui.catAttack=Ctrl(L"BUTTON",L"ATTACK",BS_OWNERDRAW,10,146,96,38,IDC_CATEGORY_ATTACK,g_fontBold);','g_ui.catAttack=Ctrl(L"BUTTON",L"ATTACK",BS_OWNERDRAW,10,146,96,38,IDC_CATEGORY_ATTACK,g_fontBold);g_ui.catMob=Ctrl(L"BUTTON",L"MOB ATTACK",BS_OWNERDRAW,10,192,96,38,IDC_CATEGORY_MOB,g_fontSmall);')
    fn=fn.replace('CreateRoguePage();CreateAttackPage();PopulateUi();','CreateRoguePage();CreateAttackPage();CreateMobPage();PopulateUi();')
    return fn
edit_fn('void CreateControls()',create_controls_edit)

def draw_edit(fn):
    return fn.replace('d->CtlID==IDC_CATEGORY_ROGUE||d->CtlID==IDC_CATEGORY_ATTACK','d->CtlID==IDC_CATEGORY_ROGUE||d->CtlID==IDC_CATEGORY_ATTACK||d->CtlID==IDC_CATEGORY_MOB')
edit_fn('void DrawOwnerButton(DRAWITEMSTRUCT* d)',draw_edit)

# -------------------- keyboard / commands --------------------
def conflict_edit(fn):
    # Existing assignment conflict rules remain; MOB assignments are additionally protected in KeyboardProc.
    return fn

# Snapshot MobSettings in keyboard hook and handle assign targets 6/7 and shared MOB start/stop.
def keyboard_edit(fn):
    fn=fn.replace('RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}','RogueSettings r;AttackSettings a;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;}',1)
    # Assignment block anchor: assignment code ends before chat handling. Inject simple 6/7 assignment just before category hotkeys.
    anchor='  if(g_rogueCategoryEnabled&&vk==r.startHotkey&&vk==r.stopHotkey)'
    inject=r'''  if(target==6||target==7){if(vk=='R'||vk==r.startHotkey||vk==r.stopHotkey||vk==r.cureHotkey||vk==a.startHotkey||vk==a.stopHotkey){MessageBeep(MB_ICONWARNING);g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,0,0);return 1;}{std::lock_guard<std::mutex>lk(g_settingsMutex);if(target==6)g_mob.startHotkey=vk;else g_mob.stopHotkey=vk;}g_assignTarget=0;SaveMob();PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,0,0);return 1;}
  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey&&vk==m.stopHotkey){RememberGameWindow();bool on=!g_mobActive.load();g_mobActive=on;if(on){g_mobSkillTurn=0;g_lastMobRandom=-1;ResetMobTimelines();}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey){RememberGameWindow();if(!g_mobActive.exchange(true)){g_mobSkillTurn=0;g_lastMobRandom=-1;ResetMobTimelines();}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if((m.generalEnabled||m.priestEnabled)&&vk==m.stopHotkey){g_mobActive=false;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
'''
    if anchor not in fn: raise RuntimeError('keyboard hotkey anchor')
    fn=fn.replace(anchor,inject+anchor,1)
    return fn
edit_fn('LRESULT CALLBACK KeyboardProc',keyboard_edit)

# WndProc: categories, add/remove, anchor, save and mob controls.
def wnd_edit(fn):
    fn=fn.replace('case IDC_CATEGORY_ATTACK:ShowCategory(1);break;','case IDC_CATEGORY_ATTACK:ShowCategory(1);break;case IDC_CATEGORY_MOB:ShowCategory(2);break;')
    fn=fn.replace('case IDC_ATTACK_STOP_ASSIGN:g_assignTarget=5;SetWindowTextW(g_ui.attackStopAssign,L"Bir tuşa bas...");break;', 'case IDC_ATTACK_STOP_ASSIGN:g_assignTarget=5;SetWindowTextW(g_ui.attackStopAssign,L"Bir tuşa bas...");break;case IDC_ATTACK_EXTRA_ADD:ReadAttackUi(false);AddAttackExtra();break;case IDC_ATTACK_EXTRA_REMOVE:RemoveAttackExtra();break;case IDC_ATTACK_RANDOM:ReadAttackUi(true);g_skillTurn=0;g_lastAttackRandom=-1;break;')
    mobcases=r'''case IDC_MOB_START:ReadMobUi(true);if(g_mob.generalEnabled||g_mob.priestEnabled){g_chatMode=false;g_mobActive=true;g_mobSkillTurn=0;g_lastMobRandom=-1;ResetMobTimelines();}RefreshStatus();break;case IDC_MOB_STOP:g_mobActive=false;RefreshStatus();break;case IDC_MOB_START_ASSIGN:g_assignTarget=6;SetWindowTextW(g_ui.mobStartAssign,L"Bir tuşa bas...");break;case IDC_MOB_STOP_ASSIGN:g_assignTarget=7;SetWindowTextW(g_ui.mobStopAssign,L"Bir tuşa bas...");break;case IDC_MOB_GENERAL_ENABLE:case IDC_MOB_PRIEST_ENABLE:case IDC_MOB_RANDOM:case IDC_MOB_HEAL_ENABLE:ReadMobUi(true);if(!g_mob.generalEnabled&&!g_mob.priestEnabled)g_mobActive=false;RefreshStatus();break;case IDC_MOB_SKILL_ADD:ReadMobUi(false);AddMobSkill();break;case IDC_MOB_SKILL_REMOVE:RemoveMobSkill();break;case IDC_MOB_SCROLL_ADD:ReadMobUi(false);AddMobScroll();break;case IDC_MOB_SCROLL_REMOVE:RemoveMobScroll();break;case IDC_MOB_ANCHOR:{double x=0,z=0;if(ReadPosition(x,z)){{std::lock_guard<std::mutex>lk(g_settingsMutex);g_mob.anchorX=x;g_mob.anchorZ=z;g_mob.anchorValid=true;}SaveMob();SetWindowTextW(g_ui.mobPosStatus,L"POS: ANKOR OK");}else{SetWindowTextW(g_ui.mobPosStatus,L"POS: veri yok");MessageBoxW(h,L"Pozisyon beslemesi bulunamadı. Range tahmin edilmeyecek; koordinat gelene kadar W-kovalama güvenli biçimde kapalı kalır.",L"MOB ATTACK Range",MB_OK|MB_ICONINFORMATION);}break;}case IDC_MOB_HP_CAL:ReadMobUi(true);BeginCalibration(1);break;case IDC_MOB_SAVE:ReadMobUi(true);RefreshMobLists();RefreshStatus();MessageBoxW(h,L"MOB ATTACK ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;'''
    fn=fn.replace('case IDC_MAX:g_turbo=false;break;',mobcases+'case IDC_MAX:g_turbo=false;break;')
    fn=fn.replace('case WM_APP_ASSIGN_DONE:SaveRogue();SaveAttack();RefreshHotkeyLabels();', 'case WM_APP_ASSIGN_DONE:SaveRogue();SaveAttack();SaveMob();RefreshHotkeyLabels();')
    fn=fn.replace('g_attackActive=false;ReleaseKeys();PostQuitMessage(0);', 'g_attackActive=false;g_mobActive=false;ReleaseKeys();PostQuitMessage(0);')
    return fn
edit_fn('LRESULT CALLBACK WndProc',wnd_edit)

# Main: start/stop MOB workers + separate position mapping cleanup.
def main_edit(fn):
    fn=fn.replace('std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker);','std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker),tMobSkill(MobSkillWorker),tMobScroll(MobScrollWorker),tMobPriest(MobPriestWorker);')
    fn=fn.replace('if(tVitals.joinable())tVitals.join();CloseBridge();','if(tVitals.joinable())tVitals.join();if(tMobSkill.joinable())tMobSkill.join();if(tMobScroll.joinable())tMobScroll.join();if(tMobPriest.joinable())tMobPriest.join();ClosePositionBridge();CloseBridge();')
    return fn
edit_fn('int APIENTRY wWinMain',main_edit)

# -------------------- static safety/model assertions --------------------
checks=[
    'std::vector<SkillEntry> extraSkills','Random kullan','+ Skill Ekle','+ Scroll Ekle','MOB ATTACK','PRIEST AKTİF',
    'kPositionMappingName','MobAnchorDistance','g_mob.priestEnabled','NextAttackSkill','NextMobSkill','ScrollCount'
]
for x in checks:
    if x not in s: raise RuntimeError('missing generated feature '+x)
# Default old ATTACK remains sequential and keeps the original 4 registry/value model.
if 'std::array<bool,4> skillEnabled{true,false,false,false}' not in s: raise RuntimeError('legacy four skills changed')
if 'bool randomSkills=false;' not in s: raise RuntimeError('sequential default missing')

out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(s,encoding='utf-8',newline='\n')
print('PATCH=PASS')
print('OUTPUT='+str(out))
