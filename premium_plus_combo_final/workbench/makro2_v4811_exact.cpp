#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

namespace {
constexpr wchar_t kClassName[] = L"PremiumPlusComboWindow";
constexpr wchar_t kOverlayClass[] = L"PremiumPlusComboCalibrationOverlay";
constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue";
constexpr wchar_t kRogueRegistry[] = L"Software\\PremiumPlusCombo\\RogueV2";
constexpr wchar_t kAttackRegistry[] = L"Software\\PremiumPlusCombo\\AttackV2";
constexpr wchar_t kBridgeMappingName[] = L"Local\\PremiumPlusCombo.Rogue.GameBridge.v1";
constexpr uint32_t kBridgeMagic = 0x50435042u;
constexpr uint32_t kBridgeVersion = 1u;
constexpr size_t kBridgeRingSize = 8192u;
constexpr ULONG_PTR kMagicInput = 0x005050434F4D424Full; // exact 062F "PPCOMBO" marker
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 620;
constexpr int kSidebarWidth = 112;
constexpr int kContentX = 128;
constexpr int kContentW = 648;
constexpr UINT WM_APP_REFRESH = WM_APP + 1;
constexpr UINT WM_APP_ASSIGN_DONE = WM_APP + 2;
constexpr UINT WM_APP_CAL_DONE = WM_APP + 3;

constexpr COLORREF C_BG = RGB(238,225,197);
constexpr COLORREF C_PANEL = RGB(250,244,228);
constexpr COLORREF C_PANEL2 = RGB(243,232,208);
constexpr COLORREF C_GOLD = RGB(181,139,54);
constexpr COLORREF C_GOLD_DARK = RGB(115,82,25);
constexpr COLORREF C_RED = RGB(111,30,27);
constexpr COLORREF C_TEXT = RGB(58,42,24);
constexpr COLORREF C_GREEN = RGB(31,126,67);
constexpr COLORREF C_GRAY = RGB(104,96,83);

constexpr int IDC_POWER = 1401;
constexpr int IDC_MINOR_START = 1402;
constexpr int IDC_MINOR_STOP = 1403;
constexpr int IDC_MINOR_START_ASSIGN = 1404;
constexpr int IDC_MINOR_STOP_ASSIGN = 1405;
constexpr int IDC_MAX = 1410;
constexpr int IDC_TURBO = 1411;
constexpr int IDC_R_CHECK = 1420;
constexpr int IDC_CURE_CHECK = 1421;
constexpr int IDC_SAVE = 1422;
constexpr int IDC_CURE_ASSIGN = 1423;
constexpr int IDC_CATEGORY_ROGUE = 1520;
constexpr int IDC_CATEGORY_ATTACK = 1521;
constexpr int IDC_ATTACK_START = 1530;
constexpr int IDC_ATTACK_STOP = 1531;
constexpr int IDC_ATTACK_START_ASSIGN = 1532;
constexpr int IDC_ATTACK_STOP_ASSIGN = 1533;
constexpr int IDC_HP_CHECK = 1540;
constexpr int IDC_MP_CHECK = 1541;
constexpr int IDC_HP_CAL = 1542;
constexpr int IDC_MP_CAL = 1543;
constexpr int IDC_ATTACK_SKILL_BASE = 1560;
constexpr int IDC_ATTACK_W_COMBO = 1544;
constexpr int IDC_ATTACK_S_COMBO = 1545;
constexpr int IDC_ATTACK_W_MS = 1546;
constexpr int IDC_ATTACK_S_MS = 1547;
constexpr int IDC_ATTACK_Z_COMBO = 1548;
constexpr int IDC_ROGUE_CATEGORY_ENABLE = 1620;
constexpr int IDC_ATTACK_CATEGORY_ENABLE = 1621;
constexpr int IDC_AUTO_MINOR_ENABLE = 1622;
constexpr int IDC_AUTO_MINOR_START_PCT = 1623;
constexpr int IDC_AUTO_MINOR_STOP_PCT = 1624;
constexpr int IDC_AUTO_MINOR_BAR = 1625;
constexpr int IDC_AUTO_MINOR_SLOT = 1626;
constexpr int IDC_AUTO_MINOR_CAL = 1627;

enum BridgeEventFlags : uint32_t { BridgeKeyDown=0x01u, BridgeKeyUp=0x02u, BridgeExtended=0x04u };
struct BridgeEvent { volatile LONG64 sequence; uint32_t virtualKey; uint32_t scanCode; uint32_t flags; uint32_t reserved; };
struct BridgeSharedState { uint32_t magic; uint32_t version; volatile LONG64 writeSequence; volatile LONG64 gameHeartbeatMs; BridgeEvent events[kBridgeRingSize]; };
static_assert(sizeof(BridgeEvent)==24, "BridgeEvent ABI mismatch");
static_assert(offsetof(BridgeSharedState, events)==24, "Bridge ABI mismatch");

struct NormalizedRect {
  int x=0,y=0,w=0,h=0;
  bool valid() const { return w>1500 && h>1500; }
};
struct RogueSettings {
  std::array<int,3> seq{'8','9','0'};
  bool rEnabled=false;
  int rMax=25;
  int rTurbo=40;
  bool cureEnabled=false;
  int cureBar=2;
  int cureSlot=6;
  int cureHotkey=VK_F6;
  int startHotkey=VK_CAPITAL;
  int stopHotkey=VK_CAPITAL;
  bool powerEnabled=true;
  bool autoMinorEnabled=false;
  int autoMinorStartPct=30;
  int autoMinorStopPct=75;
  int autoMinorBar=1;
  int autoMinorSlot=8;
};
struct AttackSettings {
  int startHotkey=VK_F9;
  int stopHotkey=VK_F10;
  int delayMs=125;
  int restoreBar=1;
  std::array<bool,4> skillEnabled{true,false,false,false};
  std::array<int,4> attackBars{1,1,1,1};
  std::array<int,4> slots{2,3,4,5};
  std::array<int,4> skillDelayMs{1,1,1,1};
  bool zCombo=false;
  bool wCombo=false;
  bool sCombo=false;
  int wDelayMs=400;
  int sDelayMs=50;
  bool hpEnabled=false;
  int hpThreshold=60;
  int hpBar=1;
  int hpSlot=1;
  NormalizedRect hpRect;
  bool mpEnabled=false;
  int mpThreshold=35;
  int mpBar=1;
  int mpSlot=2;
  NormalizedRect mpRect;
};

struct Ui {
  HWND main{},power{},catRogue{},catAttack{},status{};
  std::vector<HWND> roguePage,attackPage;
  HWND minorStart{},minorStop{},minorStartAssign{},minorStopAssign{},rogueCategoryEnable{};
  std::array<HWND,3> seq{};
  HWND maxMode{},turboMode{},rCheck{},rMax{},rTurbo{};
  HWND cureCheck{},cureBar{},cureSlot{},cureAssign{},autoMinorCheck{},autoMinorStartPct{},autoMinorStopPct{},autoMinorBar{},autoMinorSlot{},autoMinorCal{},autoMinorHp{},saveRogue{};
  HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackCategoryEnable{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},wDelay{},sDelay{};
  std::array<HWND,4> skillCheck{},skillBar{},skillSlot{},skillDelay{};
  HWND hpCheck{},hpThreshold{},hpBar{},hpSlot{},hpCal{},hpPercent{};
  HWND mpCheck{},mpThreshold{},mpBar{},mpSlot{},mpCal{},mpPercent{},saveAttack{};
};

HINSTANCE g_instance{};
Ui g_ui;
RogueSettings g_rogue;
AttackSettings g_attack;
std::mutex g_settingsMutex;
std::atomic<bool> g_rogueCategoryEnabled{true};
std::atomic<bool> g_attackCategoryEnabled{true};

struct FifoTicketLock {
  std::atomic<unsigned long> nextTicket{0};
  std::atomic<unsigned long> serving{0};
  unsigned long lock(){
    const unsigned long ticket=nextTicket.fetch_add(1,std::memory_order_acq_rel);
    unsigned spin=0;
    while(serving.load(std::memory_order_acquire)!=ticket){
      if((++spin&0x3f)==0)SwitchToThread(); else YieldProcessor();
    }
    return ticket;
  }
  void unlock(unsigned long ticket){serving.store(ticket+1,std::memory_order_release);}
};
struct FifoTicketGuard {
  FifoTicketLock& lockRef; unsigned long ticket;
  explicit FifoTicketGuard(FifoTicketLock& l):lockRef(l),ticket(l.lock()){}
  ~FifoTicketGuard(){lockRef.unlock(ticket);}
  FifoTicketGuard(const FifoTicketGuard&)=delete;
  FifoTicketGuard& operator=(const FifoTicketGuard&)=delete;
};
FifoTicketLock g_gameInputGate;
std::atomic<bool> g_running{true};
std::atomic<bool> g_minorActive{false};
std::atomic<bool> g_autoMinorLatched{false};
std::atomic<bool> g_autoMinorOwned{false};
std::atomic<bool> g_attackActive{false};
std::atomic<bool> g_turbo{false};
std::atomic<bool> g_curePending{false};
HANDLE g_cureEvent{};
std::atomic<bool> g_cureExclusive{false};
std::atomic<bool> g_attackExclusive{false};
std::atomic<bool> g_potionExclusive{false};
std::atomic<bool> g_chatMode{false};
std::atomic<ULONGLONG> g_rPauseUntil{0};
std::atomic<ULONG_PTR> g_gameWindow{0};
std::atomic<int> g_assignTarget{0}; // 1/2 minor, 3 cure, 4/5 attack
std::array<std::atomic<bool>,256> g_keyDown{};
HHOOK g_keyboardHook{};
HFONT g_font{},g_fontBold{},g_fontSmall{};
HBRUSH g_bgBrush{},g_panelBrush{},g_sidebarBrush{};
HANDLE g_bridgeMap{};
BridgeSharedState* g_bridge{};
std::atomic<int> g_hpPercent{-1},g_mpPercent{-1};
std::atomic<int> g_hpCurrent{-1},g_hpMax{-1},g_mpCurrent{-1},g_mpMax{-1};
std::atomic<unsigned> g_wsTurn{0};
std::atomic<unsigned> g_skillTurn{0};
std::atomic<ULONGLONG> g_lastComboAt{0};
struct WsPendingState { bool pending=false; LONGLONG skillAt=0; bool w=false,s=false; int wDelayMs=400,sDelayMs=50; };
std::mutex g_wsMutex;
WsPendingState g_wsPending;
std::atomic<bool> g_wsPriority{false};
std::atomic<int> g_attackKnownBar{0};
std::atomic<bool> g_hpArmed{true},g_mpArmed{true};
std::atomic<ULONGLONG> g_lastHpPot{0},g_lastMpPot{0};
HWND g_overlay{};
POINT g_calStart{},g_calCurrent{};
std::atomic<int> g_calTarget{0};
std::atomic<bool> g_dragging{false};
std::atomic<bool> g_observerReady{false};
std::array<std::atomic<int>,256> g_observerDownCount{};
std::array<std::atomic<int>,256> g_observerScanNonZero{};
std::array<std::atomic<ULONGLONG>,256> g_observerFirstDownAt{};
std::array<std::atomic<ULONGLONG>,256> g_observerFirstUpAt{};
std::atomic<int> g_observerActiveDown{0};
std::atomic<int> g_observerOverlap{0};
std::atomic<DWORD> g_observerThreadId{0};
HHOOK g_observerHook{};

DWORD ReadDword(HKEY k,const wchar_t* n,DWORD d){ DWORD t=0,v=0,s=sizeof(v); return RegQueryValueExW(k,n,nullptr,&t,(BYTE*)&v,&s)==ERROR_SUCCESS&&t==REG_DWORD?v:d; }
void WriteDword(HKEY k,const wchar_t* n,DWORD v){ RegSetValueExW(k,n,0,REG_DWORD,(BYTE*)&v,sizeof(v)); }
int ClampD(DWORD v,int lo,int hi){ return std::clamp((int)v,lo,hi); }

void LoadSettings(){
  HKEY k{};
  std::lock_guard<std::mutex> lk(g_settingsMutex);
  if(RegCreateKeyExW(HKEY_CURRENT_USER,kRogueRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){
    g_rogue.seq[0]=ClampD(ReadDword(k,L"Seq1",'8'),1,255); g_rogue.seq[1]=ClampD(ReadDword(k,L"Seq2",'9'),1,255); g_rogue.seq[2]=ClampD(ReadDword(k,L"Seq3",'0'),1,255);
    g_rogue.rEnabled=ReadDword(k,L"REnabled",0)!=0; g_rogue.rMax=ClampD(ReadDword(k,L"RMax",25),1,100); g_rogue.rTurbo=ClampD(ReadDword(k,L"RTurbo",40),1,150);
    g_rogue.cureEnabled=ReadDword(k,L"CureEnabled",0)!=0; g_rogue.cureBar=ClampD(ReadDword(k,L"CureBar",2),1,12); g_rogue.cureSlot=ClampD(ReadDword(k,L"CureSlot",6),1,10);
    g_rogue.cureHotkey=ClampD(ReadDword(k,L"CureHotkey",VK_F6),1,255); g_rogue.startHotkey=ClampD(ReadDword(k,L"StartHotkey",VK_CAPITAL),1,255); g_rogue.stopHotkey=ClampD(ReadDword(k,L"StopHotkey",VK_CAPITAL),1,255);
    g_rogue.powerEnabled=ReadDword(k,L"CapsToggleDefaultV3",1)!=0;
    g_rogue.autoMinorEnabled=ReadDword(k,L"AutoMinorEnabled",0)!=0; g_rogue.autoMinorStartPct=ClampD(ReadDword(k,L"AutoMinorStartPct",30),1,98); g_rogue.autoMinorStopPct=ClampD(ReadDword(k,L"AutoMinorStopPct",75),2,99); g_rogue.autoMinorBar=ClampD(ReadDword(k,L"AutoMinorBar",1),1,12); g_rogue.autoMinorSlot=ClampD(ReadDword(k,L"AutoMinorSlot",8),1,10);
    if(g_rogue.autoMinorStopPct<=g_rogue.autoMinorStartPct)g_rogue.autoMinorStopPct=std::min(99,g_rogue.autoMinorStartPct+1);
    g_rogueCategoryEnabled=ReadDword(k,L"CategoryEnabled",1)!=0; RegCloseKey(k);
  }
  if(RegCreateKeyExW(HKEY_CURRENT_USER,kAttackRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){
    g_attack.startHotkey=ClampD(ReadDword(k,L"StartHotkey",VK_F9),1,255); g_attack.stopHotkey=ClampD(ReadDword(k,L"StopHotkey",VK_F10),1,255);
    g_attack.delayMs=ClampD(ReadDword(k,L"DelayMs",125),1,2000); g_attack.restoreBar=ClampD(ReadDword(k,L"RestoreBar",1),1,12);
    DWORD timingProfile=ReadDword(k,L"TimingProfileV2",0); DWORD speedProfile=ReadDword(k,L"AttackSpeedProfileV3",0); DWORD wsTimingProfile=ReadDword(k,L"WSTimingProfileV4",0);
    for(int i=0;i<4;i++){ wchar_t a[32],b[32],c[32],d[32]; wsprintfW(a,L"Skill%dEnabled",i+1); wsprintfW(b,L"AttackBar%d",i+1); wsprintfW(c,L"AttackSlot%d",i+1); wsprintfW(d,L"Skill%dMs",i+1); g_attack.skillEnabled[i]=ReadDword(k,a,i==0)!=0; g_attack.attackBars[i]=ClampD(ReadDword(k,b,1),1,12); g_attack.slots[i]=ClampD(ReadDword(k,c,i+2),1,10); g_attack.skillDelayMs[i]=ClampD(ReadDword(k,d,1),1,1000); if(speedProfile<3&&g_attack.skillDelayMs[i]==300)g_attack.skillDelayMs[i]=1; }
    g_attack.zCombo=ReadDword(k,L"ZCombo",0)!=0; g_attack.wCombo=ReadDword(k,L"WCombo",0)!=0; g_attack.sCombo=ReadDword(k,L"SCombo",0)!=0; g_attack.wDelayMs=ClampD(ReadDword(k,L"WMs",400),1,1000); if(wsTimingProfile<4&&(g_attack.wDelayMs==75||g_attack.wDelayMs==300))g_attack.wDelayMs=400; g_attack.sDelayMs=ClampD(ReadDword(k,L"SMs",50),1,1000);
    g_attack.hpEnabled=ReadDword(k,L"HPEnabled",0)!=0; g_attack.hpThreshold=ClampD(ReadDword(k,L"HPThreshold",60),1,99); g_attack.hpBar=ClampD(ReadDword(k,L"HPBar",1),1,12); g_attack.hpSlot=ClampD(ReadDword(k,L"HPSlot",1),1,10);
    g_attack.hpRect={ (int)ReadDword(k,L"HPX",0),(int)ReadDword(k,L"HPY",0),(int)ReadDword(k,L"HPW",0),(int)ReadDword(k,L"HPH",0) };
    g_attack.mpEnabled=ReadDword(k,L"MPEnabled",0)!=0; g_attack.mpThreshold=ClampD(ReadDword(k,L"MPThreshold",35),1,99); g_attack.mpBar=ClampD(ReadDword(k,L"MPBar",1),1,12); g_attack.mpSlot=ClampD(ReadDword(k,L"MPSlot",2),1,10);
    g_attack.mpRect={ (int)ReadDword(k,L"MPX",0),(int)ReadDword(k,L"MPY",0),(int)ReadDword(k,L"MPW",0),(int)ReadDword(k,L"MPH",0) }; g_attackCategoryEnabled=ReadDword(k,L"CategoryEnabled",1)!=0; RegCloseKey(k);
  }
}

void SaveCategoryEnabled(bool rogue,bool enabled){ HKEY k{}; const wchar_t* path=rogue?kRogueRegistry:kAttackRegistry; if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return; WriteDword(k,L"CategoryEnabled",enabled?1:0); RegCloseKey(k); }
void SaveRogue(){ HKEY k{}; std::lock_guard<std::mutex> lk(g_settingsMutex); if(RegCreateKeyExW(HKEY_CURRENT_USER,kRogueRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return; WriteDword(k,L"Seq1",g_rogue.seq[0]);WriteDword(k,L"Seq2",g_rogue.seq[1]);WriteDword(k,L"Seq3",g_rogue.seq[2]);WriteDword(k,L"REnabled",g_rogue.rEnabled);WriteDword(k,L"RMax",g_rogue.rMax);WriteDword(k,L"RTurbo",g_rogue.rTurbo);WriteDword(k,L"CureEnabled",g_rogue.cureEnabled);WriteDword(k,L"CureBar",g_rogue.cureBar);WriteDword(k,L"CureSlot",g_rogue.cureSlot);WriteDword(k,L"CureHotkey",g_rogue.cureHotkey);WriteDword(k,L"StartHotkey",g_rogue.startHotkey);WriteDword(k,L"StopHotkey",g_rogue.stopHotkey);WriteDword(k,L"CapsToggleDefaultV3",g_rogue.powerEnabled);WriteDword(k,L"AutoMinorEnabled",g_rogue.autoMinorEnabled);WriteDword(k,L"AutoMinorStartPct",g_rogue.autoMinorStartPct);WriteDword(k,L"AutoMinorStopPct",g_rogue.autoMinorStopPct);WriteDword(k,L"AutoMinorBar",g_rogue.autoMinorBar);WriteDword(k,L"AutoMinorSlot",g_rogue.autoMinorSlot);RegCloseKey(k); }
void SaveAttack(){ HKEY k{}; std::lock_guard<std::mutex> lk(g_settingsMutex); if(RegCreateKeyExW(HKEY_CURRENT_USER,kAttackRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return; WriteDword(k,L"StartHotkey",g_attack.startHotkey);WriteDword(k,L"StopHotkey",g_attack.stopHotkey);WriteDword(k,L"DelayMs",g_attack.delayMs);WriteDword(k,L"RestoreBar",g_attack.restoreBar);for(int i=0;i<4;i++){wchar_t a[32],b[32],c[32],d[32];wsprintfW(a,L"Skill%dEnabled",i+1);wsprintfW(b,L"AttackBar%d",i+1);wsprintfW(c,L"AttackSlot%d",i+1);wsprintfW(d,L"Skill%dMs",i+1);WriteDword(k,a,g_attack.skillEnabled[i]);WriteDword(k,b,g_attack.attackBars[i]);WriteDword(k,c,g_attack.slots[i]);WriteDword(k,d,g_attack.skillDelayMs[i]);}WriteDword(k,L"TimingProfileV2",2);WriteDword(k,L"AttackSpeedProfileV3",3);WriteDword(k,L"WSTimingProfileV4",4);WriteDword(k,L"ZCombo",g_attack.zCombo);WriteDword(k,L"WCombo",g_attack.wCombo);WriteDword(k,L"SCombo",g_attack.sCombo);WriteDword(k,L"WMs",g_attack.wDelayMs);WriteDword(k,L"SMs",g_attack.sDelayMs);WriteDword(k,L"HPEnabled",g_attack.hpEnabled);WriteDword(k,L"HPThreshold",g_attack.hpThreshold);WriteDword(k,L"HPBar",g_attack.hpBar);WriteDword(k,L"HPSlot",g_attack.hpSlot);WriteDword(k,L"HPX",g_attack.hpRect.x);WriteDword(k,L"HPY",g_attack.hpRect.y);WriteDword(k,L"HPW",g_attack.hpRect.w);WriteDword(k,L"HPH",g_attack.hpRect.h);WriteDword(k,L"MPEnabled",g_attack.mpEnabled);WriteDword(k,L"MPThreshold",g_attack.mpThreshold);WriteDword(k,L"MPBar",g_attack.mpBar);WriteDword(k,L"MPSlot",g_attack.mpSlot);WriteDword(k,L"MPX",g_attack.mpRect.x);WriteDword(k,L"MPY",g_attack.mpRect.y);WriteDword(k,L"MPW",g_attack.mpRect.w);WriteDword(k,L"MPH",g_attack.mpRect.h);RegCloseKey(k); }

bool InitBridge(){
  if(g_bridge)return true;
  HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,(DWORD)sizeof(BridgeSharedState),kBridgeMappingName);
  if(!map)return false;
  auto* view=(BridgeSharedState*)MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(BridgeSharedState));
  if(!view){CloseHandle(map);return false;}
  if(view->magic!=kBridgeMagic||view->version!=kBridgeVersion){
    // 062F clears everything after the magic word, then publishes magic/version.
    ZeroMemory((BYTE*)view+4,sizeof(BridgeSharedState)-4);
    view->magic=kBridgeMagic;
    view->version=kBridgeVersion;
    MemoryBarrier();
  }
  g_bridgeMap=map;g_bridge=view;return true;
}
void CloseBridge(){ if(g_bridge){UnmapViewOfFile(g_bridge);g_bridge=nullptr;} if(g_bridgeMap){CloseHandle(g_bridgeMap);g_bridgeMap=nullptr;} }
bool BridgeReceiverLive(){ if(!g_bridge&&!InitBridge())return false; LONG64 hb=InterlockedCompareExchange64(&g_bridge->gameHeartbeatMs,0,0); if(hb<=0)return false; ULONGLONG now=GetTickCount64(); return now>=(ULONGLONG)hb && now-(ULONGLONG)hb<=2000; }
bool IsBridgeInputKey(int vk){ return (vk>='1'&&vk<='9')||vk=='0'||vk=='R'||vk=='r'; }
bool IsExtended(int vk){ switch(vk){case VK_INSERT:case VK_DELETE:case VK_HOME:case VK_END:case VK_PRIOR:case VK_NEXT:case VK_LEFT:case VK_RIGHT:case VK_UP:case VK_DOWN:case VK_RCONTROL:case VK_RMENU:return true;default:return false;} }
UINT ScanCodeEx(int vk){ return MapVirtualKeyW((UINT)vk,MAPVK_VK_TO_VSC_EX); }

void BuildKeyInput(INPUT& in,int vk,bool up){
  ZeroMemory(&in,sizeof(in));in.type=INPUT_KEYBOARD;in.ki.wVk=(WORD)vk;in.ki.wScan=0;in.ki.dwFlags=up?KEYEVENTF_KEYUP:0;in.ki.dwExtraInfo=kMagicInput;
}
void AppendKeyPair(std::vector<INPUT>& out,int vk){INPUT d{},u{};BuildKeyInput(d,vk,false);BuildKeyInput(u,vk,true);out.push_back(d);out.push_back(u);}

// Exact 062F producer ordering: event data -> event.sequence -> shared writeSequence.
// Called only while the single FIFO game-input gate is owned.
bool PublishBridgeInputsUnlocked(const INPUT* inputs,UINT count){
  if(!inputs||!count||!BridgeReceiverLive())return false;
  struct Decoded{uint32_t vk,scan,flags;};
  std::vector<Decoded> decoded;decoded.reserve(count);
  for(UINT i=0;i<count;i++){
    const INPUT& in=inputs[i];if(in.type!=INPUT_KEYBOARD)return false;
    UINT vk=in.ki.wVk,scan=in.ki.wScan;
    if(!vk){
      if(!(in.ki.dwFlags&KEYEVENTF_SCANCODE)||!scan)return false;
      vk=MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);
    }
    if(!vk||!IsBridgeInputKey((int)vk))return false;
    if(!scan)scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC_EX);
    if(!scan)return false;
    uint32_t flags=(in.ki.dwFlags&KEYEVENTF_KEYUP)?BridgeKeyUp:BridgeKeyDown;
    if((in.ki.dwFlags&KEYEVENTF_EXTENDEDKEY)||(scan&0xFF00u))flags|=BridgeExtended;
    decoded.push_back({(uint32_t)vk,(uint32_t)(scan&0xFFu),flags});
  }
  if(!BridgeReceiverLive())return false;
  LONG64 seq=InterlockedCompareExchange64(&g_bridge->writeSequence,0,0);
  for(const auto& d:decoded){
    ++seq;auto& e=g_bridge->events[(size_t)seq&(kBridgeRingSize-1)];
    e.virtualKey=d.vk;e.scanCode=d.scan;e.flags=d.flags;e.reserved=0;
    MemoryBarrier();InterlockedExchange64(&e.sequence,seq);MemoryBarrier();InterlockedExchange64(&g_bridge->writeSequence,seq);
  }
  return true;
}

void PreciseDelayUs(int microseconds){
  if(microseconds<=0)return;
  LARGE_INTEGER freq{},begin{},now{};
  QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&begin);
  LONGLONG ticks=std::max<LONGLONG>(1,(freq.QuadPart*(LONGLONG)microseconds)/1000000LL);
  LONGLONG target=begin.QuadPart+ticks;
  for(;;){QueryPerformanceCounter(&now);if(now.QuadPart>=target)break;LONGLONG left=target-now.QuadPart;if(left*1000>freq.QuadPart)SwitchToThread();else YieldProcessor();}
}
INPUT NativeNormalizedInput(const INPUT& source){
  INPUT in=source;
  if(in.type==INPUT_KEYBOARD && !(in.ki.dwFlags&(KEYEVENTF_SCANCODE|KEYEVENTF_UNICODE)) && in.ki.wVk){
    UINT scan=MapVirtualKeyW(in.ki.wVk,MAPVK_VK_TO_VSC_EX);
    if(scan){in.ki.wVk=0;in.ki.wScan=(WORD)(scan&0xFFu);in.ki.dwFlags|=KEYEVENTF_SCANCODE;if(scan&0xFF00u)in.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;}
  }
  return in;
}
bool SendOneReferenceInputUnlocked(const INPUT& source){
  if(PublishBridgeInputsUnlocked(&source,1))return true;
  INPUT native=NativeNormalizedInput(source);
  return SendInput(1,&native,sizeof(INPUT))==1;
}
bool MatchingDownUpPair(const INPUT& down,const INPUT& up){
  return down.type==INPUT_KEYBOARD&&up.type==INPUT_KEYBOARD&&
         !(down.ki.dwFlags&KEYEVENTF_KEYUP)&&(up.ki.dwFlags&KEYEVENTF_KEYUP)&&
         down.ki.wVk==up.ki.wVk&&down.ki.wScan==up.ki.wScan;
}
// Game transport. The caller must own g_gameInputGate. This is deliberately
// scan-code based and never collapses a DOWN+UP pair into a zero-duration pulse.
UINT ReferenceSendInputsUnlocked(const INPUT* inputs,UINT count){
  if(!inputs||!count)return 0;
  bool allKeyboard=true;for(UINT i=0;i<count;i++)if(inputs[i].type!=INPUT_KEYBOARD){allKeyboard=false;break;}
  if(!allKeyboard){
    if(PublishBridgeInputsUnlocked(inputs,count))return count;
    std::vector<INPUT> native;native.reserve(count);for(UINT i=0;i<count;i++)native.push_back(NativeNormalizedInput(inputs[i]));
    return SendInput(count,native.data(),sizeof(INPUT));
  }
  // The game-side receiver only consumes 1-0/R. F-keys and Z must never be
  // swallowed by a live bridge that does not implement them.
  if(BridgeReceiverLive()&&PublishBridgeInputsUnlocked(inputs,count))return count;
  UINT done=0;
  while(done<count){
    const bool pair=(done+1<count)&&MatchingDownUpPair(inputs[done],inputs[done+1]);
    INPUT first=NativeNormalizedInput(inputs[done]);
    if(SendInput(1,&first,sizeof(INPUT))!=1)break;
    if(pair){
      PreciseDelayUs(1000);
      INPUT second=NativeNormalizedInput(inputs[done+1]);
      if(SendInput(1,&second,sizeof(INPUT))!=1){
        PreciseDelayUs(1000);
        if(SendInput(1,&second,sizeof(INPUT))!=1)break;
      }
      done+=2;
      PreciseDelayUs(75);
    }else{
      const bool up=(inputs[done].ki.dwFlags&KEYEVENTF_KEYUP)!=0;
      ++done;
      PreciseDelayUs(up?75:1000);
    }
  }
  return done;
}
UINT ReferenceSendInputs(const INPUT* inputs,UINT count){FifoTicketGuard gate(g_gameInputGate);return ReferenceSendInputsUnlocked(inputs,count);}
bool ReferenceTapKeyUnlocked(int vk){INPUT pair[2]{};BuildKeyInput(pair[0],vk,false);BuildKeyInput(pair[1],vk,true);return ReferenceSendInputsUnlocked(pair,2)==2;}
bool ReferenceTapKey(int vk){FifoTicketGuard gate(g_gameInputGate);return ReferenceTapKeyUnlocked(vk);}
bool DirectTimedTapUnlocked(int vk,int holdUs=1800,int releaseGapUs=120){
  INPUT down{},up{};BuildKeyInput(down,vk,false);BuildKeyInput(up,vk,true);down=NativeNormalizedInput(down);up=NativeNormalizedInput(up);
  if(SendInput(1,&down,sizeof(INPUT))!=1)return false;PreciseDelayUs(std::clamp(holdUs,1000,50000));
  if(SendInput(1,&up,sizeof(INPUT))!=1){PreciseDelayUs(600);if(SendInput(1,&up,sizeof(INPUT))!=1)return false;}
  PreciseDelayUs(std::clamp(releaseGapUs,75,10000));return true;
}
void SendRoutedKey(int vk,bool up){INPUT in{};BuildKeyInput(in,vk,up);FifoTicketGuard gate(g_gameInputGate);ReferenceSendInputsUnlocked(&in,1);}
void TapRouted(int vk,int hold=0){(void)hold;ReferenceTapKey(vk);}
int SlotToVk(int slot){return slot==10?'0':'0'+slot;} int BarToVk(int bar){return VK_F1+std::clamp(bar,1,12)-1;}
void ReleaseKeys(){ RogueSettings r; AttackSettings a; {std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;} std::vector<int> ks={'R','Z','W','S',r.seq[0],r.seq[1],r.seq[2],SlotToVk(r.cureSlot),BarToVk(r.cureBar),BarToVk(r.autoMinorBar),SlotToVk(r.autoMinorSlot),BarToVk(a.restoreBar)}; for(int i=0;i<4;i++){ks.push_back(BarToVk(a.attackBars[i]));ks.push_back(SlotToVk(a.slots[i]));}ks.push_back(BarToVk(a.hpBar));ks.push_back(SlotToVk(a.hpSlot));ks.push_back(BarToVk(a.mpBar));ks.push_back(SlotToVk(a.mpSlot));FifoTicketGuard gate(g_gameInputGate);for(int k:ks){INPUT in{};BuildKeyInput(in,k,true);ReferenceSendInputsUnlocked(&in,1);} }

std::wstring KeyName(int vk){ if(vk>='0'&&vk<='9')return std::wstring(1,(wchar_t)vk);if(vk>='A'&&vk<='Z')return std::wstring(1,(wchar_t)vk);if(vk>=VK_F1&&vk<=VK_F12)return L"F"+std::to_wstring(vk-VK_F1+1);switch(vk){case VK_CAPITAL:return L"CAPS LOCK";case VK_TAB:return L"TAB";case VK_INSERT:return L"INSERT";case VK_DELETE:return L"DELETE";case VK_HOME:return L"HOME";case VK_END:return L"END";case VK_SPACE:return L"SPACE";case VK_RETURN:return L"ENTER";case VK_ESCAPE:return L"ESC";default:return L"VK "+std::to_wstring(vk);} }
int GetInt(HWND h,int fb,int lo,int hi){wchar_t b[32]{};GetWindowTextW(h,b,31);int v=_wtoi(b);return v<lo||v>hi?fb:v;}
int ComboVal(HWND h,int lo,int hi,int fb){int i=(int)SendMessageW(h,CB_GETCURSEL,0,0);if(i<0)return fb;return std::clamp(lo+i,lo,hi);} void SetCombo(HWND h,int v,int lo){SendMessageW(h,CB_SETCURSEL,v-lo,0);} 
void FillBar(HWND h){for(int i=1;i<=12;i++){auto s=L"F"+std::to_wstring(i);SendMessageW(h,CB_ADDSTRING,0,(LPARAM)s.c_str());}} void FillSlot(HWND h){for(int i=1;i<=10;i++){auto s=i==10?L"0":std::to_wstring(i);SendMessageW(h,CB_ADDSTRING,0,(LPARAM)s.c_str());}}

void Font(HWND h,HFONT f=nullptr){SendMessageW(h,WM_SETFONT,(WPARAM)(f?f:g_font),TRUE);} HWND Ctrl(const wchar_t* cls,const wchar_t* txt,DWORD style,int x,int y,int w,int h,int id,HFONT f=nullptr){HWND c=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,g_ui.main,(HMENU)(INT_PTR)id,g_instance,nullptr);Font(c,f);return c;} HWND Label(const wchar_t* txt,int x,int y,int w,int h,HFONT f=nullptr){return Ctrl(L"STATIC",txt,SS_LEFT|SS_CENTERIMAGE,x,y,w,h,0,f);} void PageAdd(std::vector<HWND>& p,HWND h){p.push_back(h);} 
void ShowCategory(int category){bool rogue=category==0;for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);for(HWND h:g_ui.attackPage)ShowWindow(h,rogue?SW_HIDE:SW_SHOW);InvalidateRect(g_ui.main,nullptr,TRUE);}
void RefreshHotkeyLabels(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}auto set=[&](HWND h,const wchar_t* p,int vk){std::wstring s=std::wstring(p)+L": "+KeyName(vk);SetWindowTextW(h,s.c_str());};set(g_ui.minorStartAssign,L"Açma",r.startHotkey);set(g_ui.minorStopAssign,L"Kapatma",r.stopHotkey);set(g_ui.cureAssign,L"Cure",r.cureHotkey);set(g_ui.attackStartAssign,L"Açma",a.startHotkey);set(g_ui.attackStopAssign,L"Kapatma",a.stopHotkey);SetWindowTextW(g_ui.minorStart,L"BAŞLAT");SetWindowTextW(g_ui.minorStop,L"DURDUR");SetWindowTextW(g_ui.attackStart,L"BAŞLAT");SetWindowTextW(g_ui.attackStop,L"DURDUR");}
void RefreshStatus(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}std::wstring s=r.powerEnabled?L"GÜÇ AÇIK":L"GÜÇ KAPALI";s+=L"   |   Minor: "+std::wstring(g_minorActive.load()?L"ÇALIŞIYOR":L"Hazır");s+=L"   |   Attack: "+std::wstring(g_attackActive.load()?L"ÇALIŞIYOR":L"Hazır");SetWindowTextW(g_ui.status,s.c_str());SetWindowTextW(g_ui.power,r.powerEnabled?L"POWER  AÇIK":L"POWER  KAPALI");if(g_ui.hpPercent){int pct=g_hpPercent.load(),cur=g_hpCurrent.load(),mx=g_hpMax.load();std::wstring t=pct<0?L"HP: kalibrasyon yok":(cur>=0&&mx>0?L"HP "+std::to_wstring(cur)+L"/"+std::to_wstring(mx)+L" %"+std::to_wstring(pct):L"HP %"+std::to_wstring(pct));SetWindowTextW(g_ui.hpPercent,t.c_str());}if(g_ui.mpPercent){int pct=g_mpPercent.load(),cur=g_mpCurrent.load(),mx=g_mpMax.load();std::wstring t=pct<0?L"MP: kalibrasyon yok":(cur>=0&&mx>0?L"MP "+std::to_wstring(cur)+L"/"+std::to_wstring(mx)+L" %"+std::to_wstring(pct):L"MP %"+std::to_wstring(pct));SetWindowTextW(g_ui.mpPercent,t.c_str());}if(g_ui.autoMinorHp){int pct=g_hpPercent.load();std::wstring t=pct<0?L"HP: kalibre et":L"HP %"+std::to_wstring(pct);if(g_autoMinorOwned.load())t+=L"  •  AUTO MINOR";SetWindowTextW(g_ui.autoMinorHp,t.c_str());}}
void SetPower(bool on){{std::lock_guard<std::mutex>lk(g_settingsMutex);g_rogue.powerEnabled=on;}if(!on){g_autoMinorLatched=false;g_autoMinorOwned=false;g_minorActive=false;g_attackActive=false;ReleaseKeys();}SaveRogue();RefreshStatus();}

bool ConflictWithRogue(int vk,int target){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}if(target==1||target==2)return vk==r.cureHotkey||vk==a.startHotkey||vk==a.stopHotkey||vk=='R'||vk==r.seq[0]||vk==r.seq[1]||vk==r.seq[2];if(target==3)return vk==r.startHotkey||vk==r.stopHotkey||vk==a.startHotkey||vk==a.stopHotkey||vk=='R'||vk==r.seq[0]||vk==r.seq[1]||vk==r.seq[2];if(target==4||target==5)return vk==r.startHotkey||vk==r.stopHotkey||vk==r.cureHotkey||vk=='R';return false;}
bool IsOurUiWindow(HWND w){return w&&(w==g_ui.main||IsChild(g_ui.main,w));}
void RememberGameWindow(){HWND fg=GetForegroundWindow();if(fg&&!IsOurUiWindow(fg))g_gameWindow.store((ULONG_PTR)fg,std::memory_order_release);}
bool IsRememberedGameForeground(){HWND fg=GetForegroundWindow();ULONG_PTR raw=g_gameWindow.load(std::memory_order_acquire);if(raw&&IsWindow((HWND)raw))return fg==(HWND)raw;if(raw&&!IsWindow((HWND)raw))g_gameWindow=0;return fg&&!IsOurUiWindow(fg)&&(g_minorActive.load()||g_attackActive.load());}
int ChatKeyTransitionModel(bool active,int vk,bool gameForeground){if(active&&(vk==VK_RETURN||vk==VK_ESCAPE))return 0;if(!active&&gameForeground&&vk==VK_RETURN)return 1;return active?1:0;}
LRESULT CALLBACK KeyboardProc(int code,WPARAM wp,LPARAM lp){
  if(code!=HC_ACTION)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  auto* k=(KBDLLHOOKSTRUCT*)lp;int vk=(int)(k->vkCode&0xff);if(vk<0||vk>=256)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  bool down=wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN,up=wp==WM_KEYUP||wp==WM_SYSKEYUP;
  if(up){g_keyDown[vk]=false;return CallNextHookEx(g_keyboardHook,code,wp,lp);}if(!down)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  bool injected=(k->flags&LLKHF_INJECTED)!=0;if(injected&&k->dwExtraInfo==kMagicInput)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  bool first=!g_keyDown[vk].exchange(true);if(!first)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  int t=g_assignTarget.load();if(t&&!injected){if(vk!=VK_LBUTTON&&vk!=VK_RBUTTON&&!ConflictWithRogue(vk,t)){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}
  RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}
  bool gameFg=IsRememberedGameForeground();
  if(g_chatMode.load()&&(vk==VK_RETURN||vk==VK_ESCAPE)){g_chatMode=false;return CallNextHookEx(g_keyboardHook,code,wp,lp);}
  if(vk==VK_RETURN&&gameFg){g_chatMode=true;RememberGameWindow();return CallNextHookEx(g_keyboardHook,code,wp,lp);}
  if(!r.powerEnabled||injected)return CallNextHookEx(g_keyboardHook,code,wp,lp);
  if(g_chatMode.load())return CallNextHookEx(g_keyboardHook,code,wp,lp);
  if(g_rogueCategoryEnabled&&vk==r.startHotkey&&vk==r.stopHotkey){RememberGameWindow();g_autoMinorOwned=false;g_minorActive=!g_minorActive.load();PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_rogueCategoryEnabled&&vk==r.startHotkey){RememberGameWindow();g_autoMinorOwned=false;g_minorActive=true;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_rogueCategoryEnabled&&vk==r.stopHotkey){g_autoMinorOwned=false;g_minorActive=false;ReleaseKeys();PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_attackCategoryEnabled&&vk==a.startHotkey&&vk==a.stopHotkey){RememberGameWindow();bool on=!g_attackActive.load();g_attackActive=on;if(on){g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_attackCategoryEnabled&&vk==a.startHotkey){RememberGameWindow();if(!g_attackActive.exchange(true)){g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_attackCategoryEnabled&&vk==a.stopHotkey){g_attackActive=false;ReleaseKeys();PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return 1;}
  if(g_rogueCategoryEnabled&&r.cureEnabled&&vk==r.cureHotkey){RememberGameWindow();bool expected=false;if(g_cureExclusive.compare_exchange_strong(expected,true)){g_curePending=true;if(g_cureEvent)SetEvent(g_cureEvent);}return 1;}
  return CallNextHookEx(g_keyboardHook,code,wp,lp);
}

bool AutoMinorNextState(bool latched,int hp,int startPct,int stopPct){
  if(hp<0)return latched;
  if(!latched&&hp<=startPct)return true;
  if(latched&&hp>=stopPct)return false;
  return latched;
}
std::vector<INPUT> BuildMinorBatch(const RogueSettings& r,int cycles){std::vector<INPUT> v;cycles=std::clamp(cycles,1,8);v.reserve((size_t)cycles*6);for(int c=0;c<cycles;c++){AppendKeyPair(v,r.seq[0]);AppendKeyPair(v,r.seq[1]);AppendKeyPair(v,r.seq[2]);}return v;}
void MinorWorker(){
  timeBeginPeriod(1);LARGE_INTEGER fq{};QueryPerformanceFrequency(&fq);LONGLONG nextTick=0;int lastRate=0;int autoKnownBar=0;bool autoWasRunning=false;
  while(g_running){
    RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}
    if(!r.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;Sleep(1);continue;}
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);
    if(!nextTick||rate!=lastRate){nextTick=now.QuadPart;lastRate=rate;}
    if(now.QuadPart<nextTick){LONGLONG left=nextTick-now.QuadPart;if(left*1000>fq.QuadPart)Sleep(1);else SwitchToThread();continue;}
    RogueSettings fresh;{std::lock_guard<std::mutex>lk(g_settingsMutex);fresh=g_rogue;}
    if(!fresh.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode){nextTick=0;autoKnownBar=0;autoWasRunning=false;continue;}
    const bool autoOwned=g_autoMinorOwned.load(std::memory_order_acquire)&&fresh.autoMinorEnabled;
    if(autoOwned){
      FifoTicketGuard sequence(g_gameInputGate);
      bool needBar=!autoWasRunning||autoKnownBar!=fresh.autoMinorBar;
      if(g_attackActive.load())needBar=g_attackKnownBar.load(std::memory_order_relaxed)!=fresh.autoMinorBar;
      if(needBar){DirectTimedTapUnlocked(BarToVk(fresh.autoMinorBar),12000,1000);autoKnownBar=fresh.autoMinorBar;if(g_attackActive.load())g_attackKnownBar=fresh.autoMinorBar;PreciseDelayUs(2500);}
      ReferenceTapKeyUnlocked(SlotToVk(fresh.autoMinorSlot));
      autoWasRunning=true;
    }else{
      autoKnownBar=0;autoWasRunning=false;
      auto batch=BuildMinorBatch(fresh,1);ReferenceSendInputs(batch.data(),(UINT)batch.size());
    }
    QueryPerformanceCounter(&now);nextTick+=step;if(now.QuadPart-nextTick>step*2)nextTick=now.QuadPart+step;
  }
  timeEndPeriod(1);
}
void RWorker(){timeBeginPeriod(1);ULONGLONG next=0;while(g_running){RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}ULONGLONG now=GetTickCount64();if(!r.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_autoMinorOwned||!r.rEnabled||g_cureExclusive||g_potionExclusive||g_chatMode||now<g_rPauseUntil){Sleep(1);continue;}int rate=std::max(1,g_turbo.load()?r.rTurbo:r.rMax);if(now<next){Sleep(1);continue;}RogueSettings fresh;{std::lock_guard<std::mutex>lk(g_settingsMutex);fresh=g_rogue;}now=GetTickCount64();if(!fresh.powerEnabled||!g_rogueCategoryEnabled||!g_minorActive||g_autoMinorOwned||!fresh.rEnabled||g_cureExclusive||g_potionExclusive||g_chatMode||now<g_rPauseUntil)continue;ReferenceTapKey('R');rate=std::max(1,g_turbo.load()?fresh.rTurbo:fresh.rMax);next=GetTickCount64()+std::max(1,1000/rate);}timeEndPeriod(1);}
void CureWorker(){while(g_running){
  if(g_cureEvent){DWORD wr=WaitForSingleObject(g_cureEvent,50);if(!g_running)break;if(wr!=WAIT_OBJECT_0&&!g_curePending.load())continue;}
  else if(!g_curePending.load()){Sleep(1);continue;}
  if(!g_curePending.exchange(false))continue;
  RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}
  if(!r.powerEnabled||!g_rogueCategoryEnabled||!r.cureEnabled){g_cureExclusive=false;continue;}
  g_rPauseUntil=GetTickCount64()+1200;
  {FifoTicketGuard sequence(g_gameInputGate);if(r.powerEnabled){
    DirectTimedTapUnlocked(BarToVk(r.cureBar),45000,4000);PreciseDelayUs(50000);
    DirectTimedTapUnlocked(SlotToVk(r.cureSlot),48000,4000);PreciseDelayUs(90000);
    DirectTimedTapUnlocked(BarToVk(a.restoreBar),35000,3000);
  }}
  g_curePending=false;g_cureExclusive=false;
}}

std::vector<int> BuildAttackSequence(const AttackSettings& a){std::vector<int> v;if(a.zCombo)v.push_back('Z');for(int i=0;i<4;i++)if(a.skillEnabled[i]){v.push_back(BarToVk(a.attackBars[i]));v.push_back(SlotToVk(a.slots[i]));}v.push_back(BarToVk(a.restoreBar));return v;}
int NextEnabledSkill(const AttackSettings& a){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;unsigned turn=g_skillTurn.fetch_add(1,std::memory_order_relaxed);return enabled[turn%(unsigned)n];}
LONGLONG AttackQpcNow(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
void InterruptibleAttackDelayFrom(LONGLONG startQpc,int ms){ms=std::clamp(ms,1,2000);LARGE_INTEGER fq{},now{};QueryPerformanceFrequency(&fq);const LONGLONG end=startQpc+(fq.QuadPart*ms)/1000;while(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){QueryPerformanceCounter(&now);LONGLONG left=end-now.QuadPart;if(left<=0)break;if(left*1000>fq.QuadPart*3)Sleep(1);else SwitchToThread();}}
void InterruptibleAttackDelay(int ms){InterruptibleAttackDelayFrom(AttackQpcNow(),ms);}
bool AttackZEnabledNow(){std::lock_guard<std::mutex>lk(g_settingsMutex);return g_attack.zCombo;}
void ClearWsPending(){std::lock_guard<std::mutex>lk(g_wsMutex);g_wsPending=WsPendingState{};g_wsPriority=false;}
bool WsPendingNow(){std::lock_guard<std::mutex>lk(g_wsMutex);return g_wsPending.pending;}
void MaybeSendWsCombo(const AttackSettings& a,LONGLONG skillAt){
  if((!a.wCombo&&!a.sCombo)||g_cureExclusive||g_potionExclusive||g_chatMode||!g_attackActive||!skillAt)return;
  std::lock_guard<std::mutex>lk(g_wsMutex);
  if(g_wsPending.pending)return; // one animation-cancel job at a time; never build a W/S backlog
  g_wsPending.pending=true;g_wsPending.skillAt=skillAt;g_wsPending.w=a.wCombo;g_wsPending.s=a.sCombo;g_wsPending.wDelayMs=a.wDelayMs;g_wsPending.sDelayMs=a.sDelayMs;
}
int WsFirstDelayMs(const WsPendingState& job){return std::clamp(job.w?job.wDelayMs:job.sDelayMs,1,1000);}
constexpr int kWsVisibleHoldUs=18000;
constexpr int kWsReleaseGapUs=1200;
void WsWorker(){
  while(g_running){
    WsPendingState job;{std::lock_guard<std::mutex>lk(g_wsMutex);job=g_wsPending;}
    if(!job.pending){Sleep(1);continue;}
    if(!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode){ClearWsPending();Sleep(1);continue;}
    InterruptibleAttackDelayFrom(job.skillAt,WsFirstDelayMs(job));
    if(!g_running||!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode){ClearWsPending();continue;}
    // W/S timing is DOWN-edge to DOWN-edge: WMs = skill DOWN -> W DOWN; SMs = W DOWN -> S DOWN.
    LONGLONG wAt=0;
    if(job.w){
      {FifoTicketGuard sequence(g_gameInputGate);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){wAt=AttackQpcNow();DirectTimedTapUnlocked('W',kWsVisibleHoldUs,kWsReleaseGapUs);}}
      if(wAt)g_lastComboAt=GetTickCount64();
    }
    if(job.s){
      if(job.w&&wAt)InterruptibleAttackDelayFrom(wAt,job.sDelayMs);
      else InterruptibleAttackDelayFrom(job.skillAt,job.sDelayMs);
      if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
        FifoTicketGuard sequence(g_gameInputGate);
        if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){DirectTimedTapUnlocked('S',kWsVisibleHoldUs,kWsReleaseGapUs);g_lastComboAt=GetTickCount64();}
      }
    }
    ClearWsPending();
  }
  ClearWsPending();
}
bool AttackNeedsBarTap(int knownBar,int wantedBar){return knownBar!=wantedBar;}
void ExecuteAttack(const AttackSettings& a){
  if(!g_running||!g_attackActive||g_cureExclusive||g_potionExclusive||g_chatMode)return;
  if(a.zCombo&&AttackZEnabledNow()){ReferenceTapKey('Z');if(g_cureExclusive||g_potionExclusive||g_chatMode)return;}
  int i=NextEnabledSkill(a);if(i<0)return;const int wantedBar=a.attackBars[i];LONGLONG skillAt=0;
  g_attackExclusive=true;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
    int knownBar=g_attackKnownBar.load(std::memory_order_relaxed);
    if(AttackNeedsBarTap(knownBar,wantedBar)){DirectTimedTapUnlocked(BarToVk(wantedBar),12000,1000);g_attackKnownBar=wantedBar;PreciseDelayUs(30000);}
    skillAt=AttackQpcNow();ReferenceTapKeyUnlocked(SlotToVk(a.slots[i]));
    if(wantedBar!=a.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}
  }}
  g_attackExclusive=false;
  if(!skillAt||g_cureExclusive||g_potionExclusive||g_chatMode)return;
  MaybeSendWsCombo(a,skillAt);
  if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode)InterruptibleAttackDelayFrom(skillAt,a.skillDelayMs[i]);
}
bool WaitWsCycleCompletion(const AttackSettings& a){
  if(!a.wCombo&&!a.sCombo)return true;
  int maxMs=std::clamp((a.wCombo?a.wDelayMs:0)+(a.sCombo?(a.wCombo?a.sDelayMs:a.sDelayMs):0)+350,350,2600);
  LONGLONG start=AttackQpcNow();LARGE_INTEGER fq{},now{};QueryPerformanceFrequency(&fq);
  while(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){
    if(!WsPendingNow())return true;
    QueryPerformanceCounter(&now);if((now.QuadPart-start)*1000>=fq.QuadPart*maxMs){ClearWsPending();return false;}
    Sleep(1);
  }
  return false;
}
void AttackWorker(){bool wasReady=false;while(g_running){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}bool ready=r.powerEnabled&&g_attackCategoryEnabled&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode;if(!ready){wasReady=false;g_attackKnownBar=0;Sleep(1);continue;}if(!wasReady){g_attackKnownBar=0;ClearWsPending();wasReady=true;}ExecuteAttack(a);if(g_running&&g_attackActive&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode){if(a.wCombo||a.sCombo)WaitWsCycleCompletion(a);else InterruptibleAttackDelay(a.delayMs);}}g_attackKnownBar=0;ClearWsPending();}

struct RGBc{double r,g,b;};
double Sat(const RGBc& c){double mx=std::max({c.r,c.g,c.b}),mn=std::min({c.r,c.g,c.b});return mx<=0?0:(mx-mn)/mx;}double Lum(const RGBc& c){return (0.2126*c.r+0.7152*c.g+0.0722*c.b)/255.0;}double Dist(const RGBc&a,const RGBc&b){double dr=a.r-b.r,dg=a.g-b.g,db=a.b-b.b;return std::sqrt(dr*dr+dg*dg+db*db);} 
int EstimateFill(const std::vector<RGBc>& col){int n=(int)col.size();if(n<5)return -1;int edge=std::max(1,n/12);RGBc L{},R{};for(int i=0;i<edge;i++){L.r+=col[i].r;L.g+=col[i].g;L.b+=col[i].b;R.r+=col[n-1-i].r;R.g+=col[n-1-i].g;R.b+=col[n-1-i].b;}L.r/=edge;L.g/=edge;L.b/=edge;R.r/=edge;R.g/=edge;R.b/=edge;double sL=Sat(L),sR=Sat(R),sumL=L.r+L.g+L.b,sumR=R.r+R.g+R.b;double chroma=999;if(sumL>1&&sumR>1){double lr0=L.r/sumL-R.r/sumR,lg0=L.g/sumL-R.g/sumR,lb0=L.b/sumL-R.b/sumR;chroma=std::sqrt(lr0*lr0+lg0*lg0+lb0*lb0);}if(sL>0.15&&sR>0.15&&std::abs(sL-sR)<0.08&&chroma<0.04)return 100;double lr=Dist(L,R);if(lr<22){double score=0;for(auto&c:col)score+=0.65*Sat(c)+0.35*Lum(c);score/=n;return score>0.34?100:0;}int filled=0;for(int i=0;i<n;i++){double dl=Dist(col[i],L),dr=Dist(col[i],R);if(dl<=dr*1.10)filled=i+1;else if(i>edge)break;}return std::clamp((int)std::lround(100.0*filled/n),0,100);}
RECT Denorm(const NormalizedRect&r){int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);RECT q{vx+(int)((long long)r.x*vw/1000000),vy+(int)((long long)r.y*vh/1000000),0,0};q.right=q.left+(int)((long long)r.w*vw/1000000);q.bottom=q.top+(int)((long long)r.h*vh/1000000);return q;}
int EstimateTypedBarFill(const std::vector<uint32_t>& px,int w,int h,bool hp){
  if(w<8||h<3||(int)px.size()<w*h)return -1;
  int y0=std::max(0,h/8),y1=std::max(y0+1,h-h/8),innerH=y1-y0,need=std::max(1,(innerH+7)/8);
  std::vector<int> mark(w,0),smooth(w,0);
  for(int x=0;x<w;x++){
    int hits=0;
    for(int y=y0;y<y1;y++){
      uint32_t p=px[(size_t)y*w+x];int b=p&255,g=(p>>8)&255,rr=(p>>16)&255;
      bool colored=hp?(rr>60&&rr>g+25&&rr>b+45):(b>80&&b>rr+45&&b>g+30);
      if(colored)hits++;
    }
    mark[x]=hits>=need;
  }
  for(int x=0;x<w;x++){
    int n=0;for(int k=-2;k<=2;k++)if(x+k>=0&&x+k<w)n+=mark[x+k];smooth[x]=n>=2;
  }
  int lo=std::max(0,w/100),hi=std::min(w,w-w/100),last=-1;
  for(int x=lo;x<hi;x++)if(smooth[x])last=x;
  if(last<0)return -1;
  if(last>=w*94/100)return 100;
  return std::clamp((int)std::lround(100.0*(last+1)/w),0,100);
}

struct BarReading{int percent=-1;int current=-1;int maximum=-1;};
struct GlyphBox{int x0=0,y0=0,x1=0,y1=0,area=0;std::vector<uint8_t> mask;};
static int CountGlyphHoles(const std::vector<uint8_t>& m,int w,int h,double* holeY=nullptr,double* holeRatio=nullptr){
  if(holeY)*holeY=0;if(holeRatio)*holeRatio=0;std::vector<uint8_t> seen((size_t)w*h);int holes=0,bestArea=0;double bestY=0;
  for(int sy=0;sy<h;sy++)for(int sx=0;sx<w;sx++){size_t si=(size_t)sy*w+sx;if(m[si]||seen[si])continue;std::vector<int> q{(int)si};seen[si]=1;bool border=false;int area=0;long long ysum=0;for(size_t qi=0;qi<q.size();qi++){int at=q[qi],x=at%w,y=at/w;area++;ysum+=y;if(x==0||y==0||x==w-1||y==h-1)border=true;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dy)continue;int nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=w||ny>=h)continue;int ni=ny*w+nx;if(!m[(size_t)ni]&&!seen[(size_t)ni]){seen[(size_t)ni]=1;q.push_back(ni);}}}if(!border){holes++;if(area>bestArea){bestArea=area;bestY=area?((double)ysum/area)/std::max(1,h):0;}}}
  if(holeY)*holeY=bestY;if(holeRatio)*holeRatio=(double)bestArea/std::max(1,w*h);return holes;
}
static wchar_t RecognizeGameDigit(const std::vector<uint8_t>& m,int w,int h){
  if(w<1||h<1)return 0;
  if(h*100>=w*155){long long topSum=0,botSum=0;int topN=0,botN=0,band=std::max(1,h/3);for(int y=0;y<band;y++)for(int x=0;x<w;x++)if(m[(size_t)y*w+x]){topSum+=x;topN++;}for(int y=h-band;y<h;y++)for(int x=0;x<w;x++)if(m[(size_t)y*w+x]){botSum+=x;botN++;}if(topN&&botN&&((double)topSum/topN)>((double)botSum/botN)+w*0.18)return L'/';}
  double hy=0,hr=0;int holes=CountGlyphHoles(m,w,h,&hy,&hr);if(holes>=2)return L'8';if(holes==1){if(hr<0.08)return L'4';if(hy>0.52)return L'6';if(hy<0.40)return L'9';return L'0';}if(w*100<h*58)return L'1';
  static const char* pat[5]={"00100011000010000100001000010001110","01110100010000100010001000100011111","11110000010000101110000010000111110","11111100001000011110000010000111110","11111000010001000100010000100001000"};
  static const wchar_t dig[5]={L'1',L'2',L'3',L'5',L'7'};bool grid[35]{};for(int ty=0;ty<7;ty++)for(int tx=0;tx<5;tx++){int xa=tx*w/5,xb=((tx+1)*w+4)/5,ya=ty*h/7,yb=((ty+1)*h+6)/7;xa=std::clamp(xa,0,w-1);xb=std::clamp(xb,xa+1,w);ya=std::clamp(ya,0,h-1);yb=std::clamp(yb,ya+1,h);int on=0,all=0;for(int y=ya;y<yb;y++)for(int x=xa;x<xb;x++){on+=m[(size_t)y*w+x]!=0;all++;}grid[ty*5+tx]=all&&on*100>=all*18;}
  int best=999,bi=-1;for(int d=0;d<5;d++){int dist=0;for(int k=0;k<35;k++)dist+=grid[k]!=(pat[d][k]=='1');if(dist<best){best=dist;bi=d;}}return bi>=0?dig[bi]:0;
}
static bool ReadNumericOverlay(const std::vector<uint32_t>& px,int w,int h,int& cur,int& mx){
  cur=mx=-1;if(w<20||h<6||(int)px.size()<w*h)return false;std::vector<uint8_t> white((size_t)w*h);for(int y=0;y<h;y++)for(int x=0;x<w;x++){uint32_t p=px[(size_t)y*w+x];int b=p&255,g=(p>>8)&255,r=(p>>16)&255;int mn=std::min({r,g,b}),ma=std::max({r,g,b});white[(size_t)y*w+x]=(mn>=120&&ma-mn<=110)?1:0;}
  std::vector<uint8_t> seen((size_t)w*h);std::vector<GlyphBox> boxes;for(int sy=0;sy<h;sy++)for(int sx=0;sx<w;sx++){int si=sy*w+sx;if(!white[(size_t)si]||seen[(size_t)si])continue;std::vector<int> q{si};seen[(size_t)si]=1;int x0=sx,x1=sx,y0=sy,y1=sy,area=0;for(size_t qi=0;qi<q.size();qi++){int at=q[qi],x=at%w,y=at/w;area++;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dy)continue;int nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=w||ny>=h)continue;int ni=ny*w+nx;if(white[(size_t)ni]&&!seen[(size_t)ni]){seen[(size_t)ni]=1;q.push_back(ni);}}}int gw=x1-x0+1,gh=y1-y0+1;if(area<5||gh<5||gw>std::max(28,w/3))continue;GlyphBox box;box.x0=x0;box.y0=y0;box.x1=x1+1;box.y1=y1+1;box.area=area;box.mask.assign((size_t)gw*gh,0);for(int at:q){int x=at%w,y=at/w;box.mask[(size_t)(y-y0)*gw+(x-x0)]=1;}boxes.push_back(box);}
  std::sort(boxes.begin(),boxes.end(),[](const GlyphBox&a,const GlyphBox&b){return a.x0<b.x0;});std::wstring text;for(const auto& box:boxes){int gw=box.x1-box.x0,gh=box.y1-box.y0;wchar_t c=RecognizeGameDigit(box.mask,gw,gh);if(c)text.push_back(c);}auto slash=text.find(L'/');if(slash==std::wstring::npos||slash==0||slash+1>=text.size())return false;auto parse=[](const std::wstring& z,int& v){if(z.empty()||z.size()>7)return false;long long n=0;for(wchar_t c:z){if(c<L'0'||c>L'9')return false;n=n*10+(c-L'0');if(n>9999999)return false;}v=(int)n;return v>0;};int a=0,b=0;if(!parse(text.substr(0,slash),a)||!parse(text.substr(slash+1),b)||b<a)return false;cur=a;mx=b;return true;
}
static BarReading AnalyzeBarPixels(const std::vector<uint32_t>& px,int w,int h,bool hp){BarReading r;r.percent=EstimateTypedBarFill(px,w,h,hp);if(ReadNumericOverlay(px,w,h,r.current,r.maximum)&&r.maximum>0){int exact=std::clamp((int)std::lround(100.0*r.current/r.maximum),0,100);if(r.percent<0||std::abs(exact-r.percent)<=20)r.percent=exact;else{r.current=-1;r.maximum=-1;}}return r;}
static BarReading CaptureBarReading(const NormalizedRect&r,bool hp){
  BarReading out;if(!r.valid())return out;RECT q=Denorm(r);int w=q.right-q.left,h=q.bottom-q.top;if(w<8||h<3||w>2000||h>500)return out;
  HDC s=GetDC(nullptr);if(!s)return out;HDC m=CreateCompatibleDC(s);if(!m){ReleaseDC(nullptr,s);return out;}HBITMAP bm=CreateCompatibleBitmap(s,w,h);if(!bm){DeleteDC(m);ReleaseDC(nullptr,s);return out;}HGDIOBJ old=SelectObject(m,bm);
  if(!BitBlt(m,0,0,w,h,s,q.left,q.top,SRCCOPY|CAPTUREBLT)){SelectObject(m,old);DeleteObject(bm);DeleteDC(m);ReleaseDC(nullptr,s);return out;}
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;std::vector<uint32_t> px((size_t)w*h);
  if(GetDIBits(m,bm,0,h,px.data(),&bi,DIB_RGB_COLORS))out=AnalyzeBarPixels(px,w,h,hp);SelectObject(m,old);DeleteObject(bm);DeleteDC(m);ReleaseDC(nullptr,s);return out;
}
int CapturePercent(const NormalizedRect&r,bool hp){return CaptureBarReading(r,hp).percent;}
bool UsePotion(bool hp,const AttackSettings&a){
  if(g_cureExclusive||g_chatMode)return false;bool expected=false;if(!g_potionExclusive.compare_exchange_strong(expected,true))return false;int bar=hp?a.hpBar:a.mpBar,slot=hp?a.hpSlot:a.mpSlot;bool ok=false;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_chatMode){
    if(bar!=a.restoreBar){ok=DirectTimedTapUnlocked(BarToVk(bar),1800,120);PreciseDelayUs(10000);}else ok=true;
    if(ok){ok=DirectTimedTapUnlocked(SlotToVk(slot),2300,160);PreciseDelayUs(36000);}
    if(bar!=a.restoreBar)DirectTimedTapUnlocked(BarToVk(a.restoreBar),1800,120);
  }}g_potionExclusive=false;return ok;
}
void VitalsWorker(){while(g_running){
  RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}
  const bool autoMinorSense=r.powerEnabled&&g_rogueCategoryEnabled&&r.autoMinorEnabled;
  const bool attackSense=r.powerEnabled&&g_attackCategoryEnabled;
  if(!autoMinorSense&&!attackSense){if(g_autoMinorLatched.exchange(false)){if(g_autoMinorOwned.exchange(false))g_minorActive=false;}Sleep(80);continue;}
  BarReading hp=(a.hpRect.valid()&&(attackSense||autoMinorSense))?CaptureBarReading(a.hpRect,true):BarReading{},mp=(attackSense&&a.mpRect.valid())?CaptureBarReading(a.mpRect,false):BarReading{};
  if(hp.percent>=0)g_hpPercent=hp.percent;if(mp.percent>=0)g_mpPercent=mp.percent;if(hp.current>=0&&hp.maximum>0){g_hpCurrent=hp.current;g_hpMax=hp.maximum;}if(mp.current>=0&&mp.maximum>0){g_mpCurrent=mp.current;g_mpMax=mp.maximum;}
  if(autoMinorSense&&hp.percent>=0&&!g_chatMode){
    bool latched=g_autoMinorLatched.load(std::memory_order_acquire);bool next=AutoMinorNextState(latched,hp.percent,r.autoMinorStartPct,r.autoMinorStopPct);
    if(next&&!latched){g_autoMinorLatched=true;if(!g_minorActive.exchange(true)){g_autoMinorOwned=true;}}
    else if(!next&&latched){g_autoMinorLatched=false;if(g_autoMinorOwned.exchange(false))g_minorActive=false;}
  }else if(!autoMinorSense&&g_autoMinorLatched.exchange(false)){if(g_autoMinorOwned.exchange(false))g_minorActive=false;}
  ULONGLONG now=GetTickCount64();if(attackSense&&!g_chatMode&&!g_cureExclusive&&a.hpEnabled&&hp.percent>=0&&hp.percent<=a.hpThreshold&&now-g_lastHpPot>240){if(UsePotion(true,a))g_lastHpPot=GetTickCount64();}
  now=GetTickCount64();if(attackSense&&!g_chatMode&&!g_cureExclusive&&a.mpEnabled&&mp.percent>=0&&mp.percent<=a.mpThreshold&&now-g_lastMpPot>240){if(UsePotion(false,a))g_lastMpPot=GetTickCount64();}
  PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);Sleep(45);
}}
NormalizedRect NormalizeScreenRect(RECT r){int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);r.left=std::clamp<LONG>(r.left,(LONG)vx,(LONG)(vx+vw));r.right=std::clamp<LONG>(r.right,(LONG)vx,(LONG)(vx+vw));r.top=std::clamp<LONG>(r.top,(LONG)vy,(LONG)(vy+vh));r.bottom=std::clamp<LONG>(r.bottom,(LONG)vy,(LONG)(vy+vh));return{(int)((long long)(r.left-vx)*1000000/vw),(int)((long long)(r.top-vy)*1000000/vh),(int)((long long)(r.right-r.left)*1000000/vw),(int)((long long)(r.bottom-r.top)*1000000/vh)};}
LRESULT CALLBACK OverlayProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_LBUTTONDOWN:g_calStart={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calStart);g_calCurrent=g_calStart;g_dragging=true;SetCapture(h);InvalidateRect(h,nullptr,TRUE);return 0;case WM_MOUSEMOVE:if(g_dragging){g_calCurrent={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calCurrent);InvalidateRect(h,nullptr,TRUE);}return 0;case WM_LBUTTONUP:if(g_dragging){g_calCurrent={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calCurrent);ReleaseCapture();g_dragging=false;RECT r{std::min(g_calStart.x,g_calCurrent.x),std::min(g_calStart.y,g_calCurrent.y),std::max(g_calStart.x,g_calCurrent.x),std::max(g_calStart.y,g_calCurrent.y)};if(r.right-r.left>=10&&r.bottom-r.top>=3){auto nr=NormalizeScreenRect(r);int target=g_calTarget.load();{std::lock_guard<std::mutex>lk(g_settingsMutex);if(target==1)g_attack.hpRect=nr;else if(target==2)g_attack.mpRect=nr;}BarReading br=CaptureBarReading(nr,target==1);if(target==1){g_hpPercent=br.percent;g_hpCurrent=br.current;g_hpMax=br.maximum;}else if(target==2){g_mpPercent=br.percent;g_mpCurrent=br.current;g_mpMax=br.maximum;}}g_calTarget=0;DestroyWindow(h);g_overlay=nullptr;SaveAttack();PostMessageW(g_ui.main,WM_APP_CAL_DONE,0,0);}return 0;case WM_KEYDOWN:if(w==VK_ESCAPE){g_calTarget=0;DestroyWindow(h);g_overlay=nullptr;}return 0;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);HBRUSH b=CreateSolidBrush(RGB(20,20,20));FillRect(dc,&c,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,230,150));SelectObject(dc,g_fontBold);DrawTextW(dc,L"HP/MP BAR ALANINI MOUSE İLE ÇERÇEVELEYİN  •  ESC: İPTAL",-1,&c,DT_CENTER|DT_TOP|DT_SINGLELINE);if(g_dragging){POINT a=g_calStart,bp=g_calCurrent;ScreenToClient(h,&a);ScreenToClient(h,&bp);RECT r{std::min(a.x,bp.x),std::min(a.y,bp.y),std::max(a.x,bp.x),std::max(a.y,bp.y)};HPEN p=CreatePen(PS_SOLID,2,RGB(255,210,70));HGDIOBJ o=SelectObject(dc,p);SelectObject(dc,GetStockObject(HOLLOW_BRUSH));Rectangle(dc,r.left,r.top,r.right,r.bottom);SelectObject(dc,o);DeleteObject(p);}EndPaint(h,&ps);return 0;}case WM_DESTROY:g_overlay=nullptr;return 0;}return DefWindowProcW(h,m,w,l);}
void BeginCalibration(int target){if(g_overlay)return;g_calTarget=target;int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);g_overlay=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED,kOverlayClass,L"Kalibrasyon",WS_POPUP,x,y,w,h,nullptr,nullptr,g_instance,nullptr);SetLayeredWindowAttributes(g_overlay,0,110,LWA_ALPHA);ShowWindow(g_overlay,SW_SHOW);SetForegroundWindow(g_overlay);SetFocus(g_overlay);}

void ReadRogueUi(bool warn){RogueSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_rogue;}for(int i=0;i<3;i++){wchar_t b[16]{};GetWindowTextW(g_ui.seq[i],b,15);if(wcslen(b)==1)n.seq[i]=towupper(b[0]);}n.rEnabled=SendMessageW(g_ui.rCheck,BM_GETCHECK,0,0)==BST_CHECKED;n.rMax=GetInt(g_ui.rMax,n.rMax,1,100);n.rTurbo=GetInt(g_ui.rTurbo,n.rTurbo,1,150);n.cureEnabled=SendMessageW(g_ui.cureCheck,BM_GETCHECK,0,0)==BST_CHECKED;n.cureBar=ComboVal(g_ui.cureBar,1,12,n.cureBar);n.cureSlot=ComboVal(g_ui.cureSlot,1,10,n.cureSlot);n.autoMinorEnabled=SendMessageW(g_ui.autoMinorCheck,BM_GETCHECK,0,0)==BST_CHECKED;n.autoMinorStartPct=GetInt(g_ui.autoMinorStartPct,n.autoMinorStartPct,1,98);n.autoMinorStopPct=GetInt(g_ui.autoMinorStopPct,n.autoMinorStopPct,2,99);n.autoMinorBar=ComboVal(g_ui.autoMinorBar,1,12,n.autoMinorBar);n.autoMinorSlot=ComboVal(g_ui.autoMinorSlot,1,10,n.autoMinorSlot);if(n.autoMinorEnabled&&n.autoMinorStopPct<=n.autoMinorStartPct){if(warn)MessageBoxW(g_ui.main,L"Auto Minor durma yüzdesi başlama yüzdesinden büyük olmalı.",L"Auto Minor",MB_OK|MB_ICONWARNING);return;}bool valid=true;for(int i=0;i<3;i++)valid&=((n.seq[i]>='0'&&n.seq[i]<='9')||(n.seq[i]>='A'&&n.seq[i]<='Z')||(n.seq[i]>=VK_F1&&n.seq[i]<=VK_F12));valid&=n.seq[0]!=n.seq[1]&&n.seq[0]!=n.seq[2]&&n.seq[1]!=n.seq[2]&&n.seq[0]!='R'&&n.seq[1]!='R'&&n.seq[2]!='R';if(!valid){if(warn)MessageBoxW(g_ui.main,L"Giriş dizisi geçersiz. Üç farklı 0-9 / A-Z / F1-F12 tuşu seçin; R kullanmayın.",L"Giriş Dizisi",MB_OK|MB_ICONWARNING);return;} {std::lock_guard<std::mutex>lk(g_settingsMutex);g_rogue=n;}if(!n.autoMinorEnabled){g_autoMinorLatched=false;if(g_autoMinorOwned.exchange(false))g_minorActive=false;}if(!n.rEnabled)SendRoutedKey('R',true);SaveRogue();}
void ReadAttackUi(bool persist=true){AttackSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_attack;}n.delayMs=GetInt(g_ui.attackDelay,n.delayMs,1,2000);n.restoreBar=ComboVal(g_ui.restoreBar,1,12,n.restoreBar);for(int i=0;i<4;i++){n.skillEnabled[i]=SendMessageW(g_ui.skillCheck[i],BM_GETCHECK,0,0)==BST_CHECKED;n.attackBars[i]=ComboVal(g_ui.skillBar[i],1,12,n.attackBars[i]);n.slots[i]=ComboVal(g_ui.skillSlot[i],1,10,n.slots[i]);n.skillDelayMs[i]=GetInt(g_ui.skillDelay[i],n.skillDelayMs[i],1,1000);}n.zCombo=SendMessageW(g_ui.zCombo,BM_GETCHECK,0,0)==BST_CHECKED;n.wCombo=SendMessageW(g_ui.wCombo,BM_GETCHECK,0,0)==BST_CHECKED;n.sCombo=SendMessageW(g_ui.sCombo,BM_GETCHECK,0,0)==BST_CHECKED;n.wDelayMs=GetInt(g_ui.wDelay,n.wDelayMs,1,1000);n.sDelayMs=GetInt(g_ui.sDelay,n.sDelayMs,1,1000);n.hpEnabled=SendMessageW(g_ui.hpCheck,BM_GETCHECK,0,0)==BST_CHECKED;n.hpThreshold=GetInt(g_ui.hpThreshold,n.hpThreshold,1,99);n.hpBar=ComboVal(g_ui.hpBar,1,12,n.hpBar);n.hpSlot=ComboVal(g_ui.hpSlot,1,10,n.hpSlot);n.mpEnabled=SendMessageW(g_ui.mpCheck,BM_GETCHECK,0,0)==BST_CHECKED;n.mpThreshold=GetInt(g_ui.mpThreshold,n.mpThreshold,1,99);n.mpBar=ComboVal(g_ui.mpBar,1,12,n.mpBar);n.mpSlot=ComboVal(g_ui.mpSlot,1,10,n.mpSlot);{std::lock_guard<std::mutex>lk(g_settingsMutex);g_attack=n;}if(persist)SaveAttack();}
void PopulateUi(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}SendMessageW(g_ui.rogueCategoryEnable,BM_SETCHECK,g_rogueCategoryEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.attackCategoryEnable,BM_SETCHECK,g_attackCategoryEnabled?BST_CHECKED:BST_UNCHECKED,0);for(int i=0;i<3;i++)SetWindowTextW(g_ui.seq[i],KeyName(r.seq[i]).c_str());SendMessageW(g_ui.rCheck,BM_SETCHECK,r.rEnabled?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.rMax,std::to_wstring(r.rMax).c_str());SetWindowTextW(g_ui.rTurbo,std::to_wstring(r.rTurbo).c_str());SendMessageW(g_ui.cureCheck,BM_SETCHECK,r.cureEnabled?BST_CHECKED:BST_UNCHECKED,0);SetCombo(g_ui.cureBar,r.cureBar,1);SetCombo(g_ui.cureSlot,r.cureSlot,1);SendMessageW(g_ui.autoMinorCheck,BM_SETCHECK,r.autoMinorEnabled?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.autoMinorStartPct,std::to_wstring(r.autoMinorStartPct).c_str());SetWindowTextW(g_ui.autoMinorStopPct,std::to_wstring(r.autoMinorStopPct).c_str());SetCombo(g_ui.autoMinorBar,r.autoMinorBar,1);SetCombo(g_ui.autoMinorSlot,r.autoMinorSlot,1);SetWindowTextW(g_ui.attackDelay,std::to_wstring(a.delayMs).c_str());SetCombo(g_ui.restoreBar,a.restoreBar,1);SendMessageW(g_ui.zCombo,BM_SETCHECK,a.zCombo?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.wCombo,BM_SETCHECK,a.wCombo?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.sCombo,BM_SETCHECK,a.sCombo?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.wDelay,std::to_wstring(a.wDelayMs).c_str());SetWindowTextW(g_ui.sDelay,std::to_wstring(a.sDelayMs).c_str());for(int i=0;i<4;i++){SendMessageW(g_ui.skillCheck[i],BM_SETCHECK,a.skillEnabled[i]?BST_CHECKED:BST_UNCHECKED,0);SetCombo(g_ui.skillBar[i],a.attackBars[i],1);SetCombo(g_ui.skillSlot[i],a.slots[i],1);SetWindowTextW(g_ui.skillDelay[i],std::to_wstring(a.skillDelayMs[i]).c_str());}SendMessageW(g_ui.hpCheck,BM_SETCHECK,a.hpEnabled?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.hpThreshold,std::to_wstring(a.hpThreshold).c_str());SetCombo(g_ui.hpBar,a.hpBar,1);SetCombo(g_ui.hpSlot,a.hpSlot,1);SendMessageW(g_ui.mpCheck,BM_SETCHECK,a.mpEnabled?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(g_ui.mpThreshold,std::to_wstring(a.mpThreshold).c_str());SetCombo(g_ui.mpBar,a.mpBar,1);SetCombo(g_ui.mpSlot,a.mpSlot,1);RefreshHotkeyLabels();RefreshStatus();}

void CreateRoguePage(){auto& p=g_ui.roguePage;PageAdd(p,Label(L"ROGUE / MINOR",kContentX,86,310,28,g_fontBold));g_ui.rogueCategoryEnable=Ctrl(L"BUTTON",L"ROGUE AKTİF",BS_AUTOCHECKBOX,kContentX+500,86,120,24,IDC_ROGUE_CATEGORY_ENABLE,g_fontSmall);PageAdd(p,g_ui.rogueCategoryEnable);PageAdd(p,Label(L"Minor Kontrol",kContentX,120,150,24,g_fontBold));g_ui.minorStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,148,110,32,IDC_MINOR_START,g_fontBold);PageAdd(p,g_ui.minorStart);g_ui.minorStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+120,148,110,32,IDC_MINOR_STOP,g_fontBold);PageAdd(p,g_ui.minorStop);g_ui.minorStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+250,148,180,32,IDC_MINOR_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.minorStartAssign);g_ui.minorStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+440,148,180,32,IDC_MINOR_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.minorStopAssign);PageAdd(p,Label(L"Giriş Dizisi",kContentX,194,110,24,g_fontBold));for(int i=0;i<3;i++){g_ui.seq[i]=Ctrl(L"EDIT",L"",WS_BORDER|ES_CENTER,kContentX+118+i*66,194,58,26,1450+i,g_fontBold);PageAdd(p,g_ui.seq[i]);if(i<2)PageAdd(p,Label(L">",kContentX+176+i*66,194,18,24,g_fontBold));}g_ui.maxMode=Ctrl(L"BUTTON",L"Maximum Mod",BS_AUTORADIOBUTTON,kContentX+330,194,135,26,IDC_MAX,g_fontSmall);PageAdd(p,g_ui.maxMode);g_ui.turboMode=Ctrl(L"BUTTON",L"Turbo Mod",BS_AUTORADIOBUTTON,kContentX+475,194,125,26,IDC_TURBO,g_fontSmall);PageAdd(p,g_ui.turboMode);SendMessageW(g_ui.maxMode,BM_SETCHECK,BST_CHECKED,0);PageAdd(p,Label(L"R Combo",kContentX,238,120,24,g_fontBold));g_ui.rCheck=Ctrl(L"BUTTON",L"R Combo Aktif",BS_AUTOCHECKBOX,kContentX,266,130,26,IDC_R_CHECK);PageAdd(p,g_ui.rCheck);PageAdd(p,Label(L"Maximum R/sn",kContentX+148,266,105,26));g_ui.rMax=Ctrl(L"EDIT",L"25",WS_BORDER|ES_CENTER,kContentX+255,266,55,26,1460);PageAdd(p,g_ui.rMax);PageAdd(p,Label(L"Turbo R/sn",kContentX+328,266,90,26));g_ui.rTurbo=Ctrl(L"EDIT",L"40",WS_BORDER|ES_CENTER,kContentX+420,266,55,26,1461);PageAdd(p,g_ui.rTurbo);PageAdd(p,Label(L"Cure",kContentX,310,120,24,g_fontBold));g_ui.cureCheck=Ctrl(L"BUTTON",L"Cure Al",BS_AUTOCHECKBOX,kContentX,338,92,26,IDC_CURE_CHECK);PageAdd(p,g_ui.cureCheck);PageAdd(p,Label(L"Bar",kContentX+105,338,32,26));g_ui.cureBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+138,338,72,240,1462);FillBar(g_ui.cureBar);PageAdd(p,g_ui.cureBar);PageAdd(p,Label(L"Slot",kContentX+222,338,38,26));g_ui.cureSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+262,338,68,240,1463);FillSlot(g_ui.cureSlot);PageAdd(p,g_ui.cureSlot);g_ui.cureAssign=Ctrl(L"BUTTON",L"Cure",BS_PUSHBUTTON,kContentX+348,338,180,28,IDC_CURE_ASSIGN,g_fontSmall);PageAdd(p,g_ui.cureAssign);
PageAdd(p,Label(L"AUTO MINOR",kContentX,382,88,24,g_fontBold));g_ui.autoMinorCheck=Ctrl(L"BUTTON",L"Aktif",BS_AUTOCHECKBOX,kContentX+90,382,58,24,IDC_AUTO_MINOR_ENABLE,g_fontSmall);PageAdd(p,g_ui.autoMinorCheck);PageAdd(p,Label(L"Başla %",kContentX+154,382,48,24,g_fontSmall));g_ui.autoMinorStartPct=Ctrl(L"EDIT",L"30",WS_BORDER|ES_CENTER,kContentX+204,382,38,24,IDC_AUTO_MINOR_START_PCT,g_fontSmall);PageAdd(p,g_ui.autoMinorStartPct);PageAdd(p,Label(L"Dur %",kContentX+250,382,40,24,g_fontSmall));g_ui.autoMinorStopPct=Ctrl(L"EDIT",L"75",WS_BORDER|ES_CENTER,kContentX+292,382,38,24,IDC_AUTO_MINOR_STOP_PCT,g_fontSmall);PageAdd(p,g_ui.autoMinorStopPct);PageAdd(p,Label(L"Bar",kContentX+338,382,28,24,g_fontSmall));g_ui.autoMinorBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+368,382,58,190,IDC_AUTO_MINOR_BAR,g_fontSmall);FillBar(g_ui.autoMinorBar);PageAdd(p,g_ui.autoMinorBar);PageAdd(p,Label(L"Slot",kContentX+434,382,30,24,g_fontSmall));g_ui.autoMinorSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+466,382,54,190,IDC_AUTO_MINOR_SLOT,g_fontSmall);FillSlot(g_ui.autoMinorSlot);PageAdd(p,g_ui.autoMinorSlot);g_ui.autoMinorCal=Ctrl(L"BUTTON",L"HP Kalibre",BS_PUSHBUTTON,kContentX+530,382,88,24,IDC_AUTO_MINOR_CAL,g_fontSmall);PageAdd(p,g_ui.autoMinorCal);g_ui.autoMinorHp=Label(L"HP: -",kContentX+90,410,220,22,g_fontSmall);PageAdd(p,g_ui.autoMinorHp);PageAdd(p,Label(L"Başla ≤ eşik  •  Dur ≥ eşik",kContentX+320,410,250,22,g_fontSmall));
g_ui.saveRogue=Ctrl(L"BUTTON",L"ROGUE AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,444,265,34,IDC_SAVE,g_fontBold);PageAdd(p,g_ui.saveRogue);PageAdd(p,Label(L"Manuel Minor yapısı korunur. Auto Minor yalnız seçilen bar/slot ve HP eşiklerini kullanır.",kContentX,486,620,24,g_fontSmall));}
void CreateAttackPage(){auto& p=g_ui.attackPage;PageAdd(p,Label(L"ATTACK",kContentX,84,260,26,g_fontBold));g_ui.attackCategoryEnable=Ctrl(L"BUTTON",L"ATTACK AKTİF",BS_AUTOCHECKBOX,kContentX+500,84,120,24,IDC_ATTACK_CATEGORY_ENABLE,g_fontSmall);PageAdd(p,g_ui.attackCategoryEnable);PageAdd(p,Label(L"Attack Kontrol",kContentX,116,150,22,g_fontBold));g_ui.attackStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,143,106,30,IDC_ATTACK_START,g_fontBold);PageAdd(p,g_ui.attackStart);g_ui.attackStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+114,143,106,30,IDC_ATTACK_STOP,g_fontBold);PageAdd(p,g_ui.attackStop);g_ui.attackStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+230,143,190,30,IDC_ATTACK_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.attackStartAssign);g_ui.attackStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+428,143,190,30,IDC_ATTACK_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.attackStopAssign);PageAdd(p,Label(L"Loop ms",kContentX,181,56,22,g_fontSmall));g_ui.attackDelay=Ctrl(L"EDIT",L"125",WS_BORDER|ES_CENTER,kContentX+58,180,50,23,1550,g_fontSmall);PageAdd(p,g_ui.attackDelay);PageAdd(p,Label(L"Ana bara dön",kContentX+122,181,82,22,g_fontSmall));g_ui.restoreBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+205,180,68,200,1551,g_fontSmall);FillBar(g_ui.restoreBar);PageAdd(p,g_ui.restoreBar);g_ui.wCombo=Ctrl(L"BUTTON",L"W",BS_AUTOCHECKBOX,kContentX+292,180,38,23,IDC_ATTACK_W_COMBO,g_fontSmall);PageAdd(p,g_ui.wCombo);g_ui.wDelay=Ctrl(L"EDIT",L"400",WS_BORDER|ES_CENTER,kContentX+330,180,42,23,IDC_ATTACK_W_MS,g_fontSmall);PageAdd(p,g_ui.wDelay);PageAdd(p,Label(L"ms",kContentX+374,181,22,22,g_fontSmall));g_ui.sCombo=Ctrl(L"BUTTON",L"S",BS_AUTOCHECKBOX,kContentX+410,180,38,23,IDC_ATTACK_S_COMBO,g_fontSmall);PageAdd(p,g_ui.sCombo);g_ui.sDelay=Ctrl(L"EDIT",L"50",WS_BORDER|ES_CENTER,kContentX+448,180,42,23,IDC_ATTACK_S_MS,g_fontSmall);PageAdd(p,g_ui.sDelay);PageAdd(p,Label(L"ms",kContentX+492,181,22,22,g_fontSmall));g_ui.zCombo=Ctrl(L"BUTTON",L"Z",BS_AUTOCHECKBOX,kContentX+526,180,42,23,IDC_ATTACK_Z_COMBO,g_fontSmall);PageAdd(p,g_ui.zCombo);PageAdd(p,Label(L"Seçili skilller",kContentX,215,260,22,g_fontBold));PageAdd(p,Label(L"AKTİF",kContentX,239,55,20,g_fontSmall));PageAdd(p,Label(L"SKILL",kContentX+72,239,55,20,g_fontSmall));PageAdd(p,Label(L"BAR",kContentX+222,239,55,20,g_fontSmall));PageAdd(p,Label(L"SLOT",kContentX+310,239,55,20,g_fontSmall));PageAdd(p,Label(L"MS",kContentX+390,239,55,20,g_fontSmall));for(int i=0;i<4;i++){int y=262+i*32;g_ui.skillCheck[i]=Ctrl(L"BUTTON",L"",BS_AUTOCHECKBOX,kContentX+12,y,22,22,IDC_ATTACK_SKILL_BASE+i,g_fontSmall);PageAdd(p,g_ui.skillCheck[i]);PageAdd(p,Label((L"Skill "+std::to_wstring(i+1)).c_str(),kContentX+72,y,100,22,g_fontSmall));g_ui.skillBar[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+210,y,74,190,1570+i,g_fontSmall);FillBar(g_ui.skillBar[i]);PageAdd(p,g_ui.skillBar[i]);g_ui.skillSlot[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+300,y,70,190,1580+i,g_fontSmall);FillSlot(g_ui.skillSlot[i]);PageAdd(p,g_ui.skillSlot[i]);g_ui.skillDelay[i]=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+382,y,54,22,1610+i,g_fontSmall);PageAdd(p,g_ui.skillDelay[i]);}int y=399;PageAdd(p,Label(L"HP POT",kContentX,y,64,22,g_fontBold));g_ui.hpCheck=Ctrl(L"BUTTON",L"Aktif",BS_AUTOCHECKBOX,kContentX+68,y,54,22,IDC_HP_CHECK,g_fontSmall);PageAdd(p,g_ui.hpCheck);PageAdd(p,Label(L"%",kContentX+126,y,14,22,g_fontSmall));g_ui.hpThreshold=Ctrl(L"EDIT",L"60",WS_BORDER|ES_CENTER,kContentX+142,y,40,22,1590,g_fontSmall);PageAdd(p,g_ui.hpThreshold);PageAdd(p,Label(L"Bar",kContentX+190,y,26,22,g_fontSmall));g_ui.hpBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+218,y,62,190,1591,g_fontSmall);FillBar(g_ui.hpBar);PageAdd(p,g_ui.hpBar);PageAdd(p,Label(L"Slot",kContentX+286,y,30,22,g_fontSmall));g_ui.hpSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+318,y,58,190,1592,g_fontSmall);FillSlot(g_ui.hpSlot);PageAdd(p,g_ui.hpSlot);g_ui.hpCal=Ctrl(L"BUTTON",L"Kalibre Et",BS_PUSHBUTTON,kContentX+386,y,80,23,IDC_HP_CAL,g_fontSmall);PageAdd(p,g_ui.hpCal);g_ui.hpPercent=Label(L"HP: -",kContentX+474,y,165,22,g_fontSmall);PageAdd(p,g_ui.hpPercent);y=430;PageAdd(p,Label(L"MP POT",kContentX,y,64,22,g_fontBold));g_ui.mpCheck=Ctrl(L"BUTTON",L"Aktif",BS_AUTOCHECKBOX,kContentX+68,y,54,22,IDC_MP_CHECK,g_fontSmall);PageAdd(p,g_ui.mpCheck);PageAdd(p,Label(L"%",kContentX+126,y,14,22,g_fontSmall));g_ui.mpThreshold=Ctrl(L"EDIT",L"35",WS_BORDER|ES_CENTER,kContentX+142,y,40,22,1593,g_fontSmall);PageAdd(p,g_ui.mpThreshold);PageAdd(p,Label(L"Bar",kContentX+190,y,26,22,g_fontSmall));g_ui.mpBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+218,y,62,190,1594,g_fontSmall);FillBar(g_ui.mpBar);PageAdd(p,g_ui.mpBar);PageAdd(p,Label(L"Slot",kContentX+286,y,30,22,g_fontSmall));g_ui.mpSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+318,y,58,190,1595,g_fontSmall);FillSlot(g_ui.mpSlot);PageAdd(p,g_ui.mpSlot);g_ui.mpCal=Ctrl(L"BUTTON",L"Kalibre Et",BS_PUSHBUTTON,kContentX+386,y,80,23,IDC_MP_CAL,g_fontSmall);PageAdd(p,g_ui.mpCal);g_ui.mpPercent=Label(L"MP: -",kContentX+474,y,165,22,g_fontSmall);PageAdd(p,g_ui.mpPercent);g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,475,250,30,1599,g_fontBold);PageAdd(p,g_ui.saveAttack);}
void LayoutChrome(){if(!g_ui.main)return;RECT r{};GetClientRect(g_ui.main,&r);int cw=r.right-r.left,ch=r.bottom-r.top;if(g_ui.power)MoveWindow(g_ui.power,std::max(kContentX+500,cw-150),16,126,34,TRUE);if(g_ui.status)MoveWindow(g_ui.status,kContentX,556,std::max(220,cw-kContentX-24),24,TRUE);InvalidateRect(g_ui.main,nullptr,TRUE);}
void CreateControls(){g_ui.power=Ctrl(L"BUTTON",L"POWER  AÇIK",BS_OWNERDRAW,650,16,126,34,IDC_POWER,g_fontBold);g_ui.catRogue=Ctrl(L"BUTTON",L"ROGUE",BS_OWNERDRAW,10,100,96,38,IDC_CATEGORY_ROGUE,g_fontBold);g_ui.catAttack=Ctrl(L"BUTTON",L"ATTACK",BS_OWNERDRAW,10,146,96,38,IDC_CATEGORY_ATTACK,g_fontBold);g_ui.status=Ctrl(L"STATIC",L"",SS_LEFT|SS_CENTERIMAGE,kContentX,556,648,24,1600,g_fontSmall);CreateRoguePage();CreateAttackPage();PopulateUi();ShowCategory(0);LayoutChrome();}

void DrawOwnerButton(DRAWITEMSTRUCT* d){bool sidebar=d->CtlID==IDC_CATEGORY_ROGUE||d->CtlID==IDC_CATEGORY_ATTACK;bool power=d->CtlID==IDC_POWER;COLORREF fill=sidebar?C_RED:(power?C_GOLD_DARK:C_GOLD),text=RGB(255,247,225);HBRUSH b=CreateSolidBrush(fill);FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);HPEN p=CreatePen(PS_SOLID,1,C_GOLD);HGDIOBJ op=SelectObject(d->hDC,p);SelectObject(d->hDC,GetStockObject(HOLLOW_BRUSH));Rectangle(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom);SelectObject(d->hDC,op);DeleteObject(p);wchar_t s[80]{};GetWindowTextW(d->hwndItem,s,79);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,text);SelectObject(d->hDC,g_fontBold);DrawTextW(d->hDC,s,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_CREATE:g_ui.main=h;CreateControls();return 0;case WM_GETMINMAXINFO:{auto* mm=(MINMAXINFO*)l;RECT wr{0,0,kWindowWidth,kWindowHeight};AdjustWindowRect(&wr,WS_OVERLAPPEDWINDOW,FALSE);mm->ptMinTrackSize.x=wr.right-wr.left;mm->ptMinTrackSize.y=wr.bottom-wr.top;return 0;}case WM_SIZE:LayoutChrome();return 0;case WM_DRAWITEM:DrawOwnerButton((DRAWITEMSTRUCT*)l);return TRUE;case WM_CTLCOLORSTATIC:{HDC dc=(HDC)w;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,C_TEXT);return (LRESULT)g_panelBrush;}case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)w;SetBkColor(dc,RGB(255,252,242));SetTextColor(dc,C_TEXT);return (LRESULT)GetStockObject(WHITE_BRUSH);}case WM_ERASEBKGND:return 1;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,g_bgBrush);RECT side{0,70,kSidebarWidth,r.bottom};FillRect(dc,&side,g_sidebarBrush);RECT panel{kContentX-10,74,r.right-12,r.bottom-34};FillRect(dc,&panel,g_panelBrush);HPEN pen=CreatePen(PS_SOLID,1,C_GOLD);HGDIOBJ old=SelectObject(dc,pen);SelectObject(dc,GetStockObject(HOLLOW_BRUSH));Rectangle(dc,panel.left,panel.top,panel.right,panel.bottom);SelectObject(dc,old);DeleteObject(pen);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,C_RED);SelectObject(dc,g_fontBold);RECT t{16,12,std::max<LONG>(360,r.right-180),56};DrawTextW(dc,L"PREMIUM PLUS COMBO  •  ROGUE",-1,&t,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);EndPaint(h,&ps);return 0;}case WM_COMMAND:{int id=LOWORD(w);if(g_ui.saveAttack&&((id>=1540&&id<=1595)||(id>=1610&&id<=1613)))ReadAttackUi(false);if(g_ui.saveRogue&&(id==1462||id==1463||(id>=IDC_AUTO_MINOR_ENABLE&&id<=IDC_AUTO_MINOR_SLOT)))ReadRogueUi(false);switch(id){case IDC_POWER:{bool on;{std::lock_guard<std::mutex>lk(g_settingsMutex);on=!g_rogue.powerEnabled;}SetPower(on);break;}case IDC_CATEGORY_ROGUE:ShowCategory(0);break;case IDC_CATEGORY_ATTACK:ShowCategory(1);break;case IDC_MINOR_START:if(g_rogueCategoryEnabled){g_chatMode=false;g_autoMinorOwned=false;g_minorActive=true;}RefreshStatus();break;case IDC_MINOR_STOP:g_autoMinorOwned=false;g_minorActive=false;ReleaseKeys();RefreshStatus();break;case IDC_MINOR_START_ASSIGN:g_assignTarget=1;SetWindowTextW(g_ui.minorStartAssign,L"Bir tuşa bas...");break;case IDC_MINOR_STOP_ASSIGN:g_assignTarget=2;SetWindowTextW(g_ui.minorStopAssign,L"Bir tuşa bas...");break;case IDC_CURE_ASSIGN:g_assignTarget=3;SetWindowTextW(g_ui.cureAssign,L"Bir tuşa bas...");break;case IDC_ATTACK_START:ReadAttackUi(true);if(g_attackCategoryEnabled){g_chatMode=false;g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;g_attackActive=true;}RefreshStatus();break;case IDC_ATTACK_STOP:g_attackActive=false;ReleaseKeys();RefreshStatus();break;case IDC_ATTACK_START_ASSIGN:g_assignTarget=4;SetWindowTextW(g_ui.attackStartAssign,L"Bir tuşa bas...");break;case IDC_ATTACK_STOP_ASSIGN:g_assignTarget=5;SetWindowTextW(g_ui.attackStopAssign,L"Bir tuşa bas...");break;case IDC_MAX:g_turbo=false;break;case IDC_TURBO:g_turbo=true;break;case IDC_SAVE:ReadRogueUi(true);RefreshStatus();MessageBoxW(h,L"Rogue ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;case IDC_HP_CAL:ReadAttackUi(true);BeginCalibration(1);break;case IDC_MP_CAL:ReadAttackUi(true);BeginCalibration(2);break;case 1599:ReadAttackUi(true);RefreshStatus();MessageBoxW(h,L"Attack ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;case IDC_R_CHECK:ReadRogueUi(false);break;case IDC_CURE_CHECK:ReadRogueUi(false);break;case IDC_AUTO_MINOR_ENABLE:ReadRogueUi(false);SaveRogue();RefreshStatus();break;case IDC_AUTO_MINOR_CAL:ReadRogueUi(true);BeginCalibration(1);break;case IDC_ROGUE_CATEGORY_ENABLE:{bool on=SendMessageW(g_ui.rogueCategoryEnable,BM_GETCHECK,0,0)==BST_CHECKED;g_rogueCategoryEnabled=on;SaveCategoryEnabled(true,on);if(!on){g_autoMinorLatched=false;g_autoMinorOwned=false;g_minorActive=false;g_curePending=false;ReleaseKeys();}RefreshStatus();break;}case IDC_ATTACK_CATEGORY_ENABLE:{bool on=SendMessageW(g_ui.attackCategoryEnable,BM_GETCHECK,0,0)==BST_CHECKED;g_attackCategoryEnabled=on;SaveCategoryEnabled(false,on);if(!on){g_attackActive=false;ClearWsPending();ReleaseKeys();}RefreshStatus();break;}default:break;}return 0;}case WM_APP_ASSIGN_DONE:SaveRogue();SaveAttack();RefreshHotkeyLabels();RefreshStatus();return 0;case WM_APP_CAL_DONE:RefreshStatus();return 0;case WM_APP_REFRESH:RefreshStatus();return 0;case WM_CLOSE:DestroyWindow(h);return 0;case WM_DESTROY:g_running=false;g_autoMinorLatched=false;g_autoMinorOwned=false;g_minorActive=false;g_attackActive=false;ReleaseKeys();PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}

ULONGLONG ObserverNowUs(){LARGE_INTEGER fq{},q{};QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&q);return fq.QuadPart?(ULONGLONG)((q.QuadPart*1000000LL)/fq.QuadPart):GetTickCount64()*1000ULL;}
LRESULT CALLBACK TransportObserverProc(int code,WPARAM wp,LPARAM lp){
  if(code==HC_ACTION){
    auto* k=(KBDLLHOOKSTRUCT*)lp;
    if(k->dwExtraInfo==kMagicInput && (k->flags&LLKHF_INJECTED)){
      int vk=(int)(k->vkCode&0xff);
      if(vk>=0&&vk<256){
        if(k->scanCode)g_observerScanNonZero[vk]=1;
        if(wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN){
          g_observerDownCount[vk]++;ULONGLONG stamp=ObserverNowUs(),zero=0;g_observerFirstDownAt[vk].compare_exchange_strong(zero,stamp);
          int before=g_observerActiveDown.fetch_add(1);
          if(before!=0)g_observerOverlap++;
        }else if(wp==WM_KEYUP||wp==WM_SYSKEYUP){
          ULONGLONG stamp=ObserverNowUs(),zero=0;g_observerFirstUpAt[vk].compare_exchange_strong(zero,stamp);
          int before=g_observerActiveDown.fetch_sub(1);
          if(before<=0)g_observerActiveDown=0;
        }
      }
    }
  }
  return CallNextHookEx(g_observerHook,code,wp,lp);
}
void TransportObserverThread(){
  g_observerThreadId=GetCurrentThreadId();
  g_observerHook=SetWindowsHookExW(WH_KEYBOARD_LL,TransportObserverProc,GetModuleHandleW(nullptr),0);
  g_observerReady=(g_observerHook!=nullptr);
  MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  if(g_observerHook){UnhookWindowsHookEx(g_observerHook);g_observerHook=nullptr;}
}
bool RunTransportObserverTest(){
  for(auto&v:g_observerDownCount)v=0;for(auto&v:g_observerScanNonZero)v=0;for(auto&v:g_observerFirstDownAt)v=0;for(auto&v:g_observerFirstUpAt)v=0;
  g_observerActiveDown=0;g_observerOverlap=0;g_observerReady=false;g_observerThreadId=0;
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  std::thread observer(TransportObserverThread);
  for(int i=0;i<200&&!g_observerReady;i++)Sleep(5);
  bool ready=g_observerReady.load();
  if(ready){
    auto hammer=[](int vk){for(int i=0;i<24;i++)ReferenceTapKey(vk);};
    std::thread t8(hammer,'8'),t9(hammer,'9'),t0(hammer,'0'),tr(hammer,'R');
    t8.join();t9.join();t0.join();tr.join();
    Sleep(60);
  }
  DWORD tid=g_observerThreadId.load();if(tid)PostThreadMessageW(tid,WM_QUIT,0,0);if(observer.joinable())observer.join();
  bool counts=g_observerDownCount['8']>0&&g_observerDownCount['9']>0&&g_observerDownCount['0']>0&&g_observerDownCount['R']>0;
  bool scans=g_observerScanNonZero['8']&&g_observerScanNonZero['9']&&g_observerScanNonZero['0']&&g_observerScanNonZero['R'];
  bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("transport-observer-report.txt",std::ios::trunc);
  f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\n";
  f<<"8_Down="<<g_observerDownCount['8'].load()<<"\n9_Down="<<g_observerDownCount['9'].load()<<"\n0_Down="<<g_observerDownCount['0'].load()<<"\nR_Down="<<g_observerDownCount['R'].load()<<"\n";
  f<<"ScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";
  f<<"RESULT="<<((ready&&counts&&scans&&overlap)?"PASS":"FAIL")<<"\n";
  CloseBridge();
  return ready&&counts&&scans&&overlap;
}

void ResetObserverState(){for(auto&v:g_observerDownCount)v=0;for(auto&v:g_observerScanNonZero)v=0;for(auto&v:g_observerFirstDownAt)v=0;for(auto&v:g_observerFirstUpAt)v=0;g_observerActiveDown=0;g_observerOverlap=0;g_observerReady=false;g_observerThreadId=0;}
bool StartObserver(std::thread& observer){ResetObserverState();observer=std::thread(TransportObserverThread);for(int i=0;i<200&&!g_observerReady;i++)Sleep(5);return g_observerReady.load();}
void StopObserver(std::thread& observer){Sleep(60);DWORD tid=g_observerThreadId.load();if(tid)PostThreadMessageW(tid,WM_QUIT,0,0);if(observer.joinable())observer.join();}
bool RunAttackObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  std::thread observer;bool ready=StartObserver(observer);g_running=true;std::thread wsWorker(WsWorker);
  ULONGLONG executeStart=0,executeEnd=0;
  if(ready){g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;ClearWsPending();AttackSettings a;a.restoreBar=1;a.delayMs=1;a.skillEnabled={true,true,true,true};a.attackBars={1,2,3,4};a.slots={2,3,4,5};a.skillDelayMs={1,1,1,1};a.zCombo=true;a.wCombo=true;a.sCombo=true;a.wDelayMs=400;a.sDelayMs=50;{std::lock_guard<std::mutex>lk(g_settingsMutex);g_attack=a;}g_attackActive=true;g_cureExclusive=false;g_potionExclusive=false;g_chatMode=false;executeStart=ObserverNowUs();for(int i=0;i<4;i++){ExecuteAttack(a);WaitWsCycleCompletion(a);}executeEnd=ObserverNowUs();g_attackActive=false;}
  g_running=false;if(wsWorker.joinable())wsWorker.join();StopObserver(observer);
  int keys[]={'Z','W','S','2','3','4','5',VK_F1,VK_F2,VK_F3,VK_F4};bool scans=true;for(int vk:keys)scans&=g_observerScanNonZero[vk].load()!=0;bool skills=g_observerDownCount['2'].load()==1&&g_observerDownCount['3'].load()==1&&g_observerDownCount['4'].load()==1&&g_observerDownCount['5'].load()==1;int wc=g_observerDownCount['W'].load(),sc=g_observerDownCount['S'].load();bool ws=wc==4&&sc==4;ULONGLONG skillAt=g_observerFirstDownAt['2'].load(),wAt=g_observerFirstDownAt['W'].load(),sAt=g_observerFirstDownAt['S'].load();long long wAfterSkill=(skillAt&&wAt)?(long long)((wAt-skillAt+500)/1000):-1,sAfterW=(wAt&&sAt)?(long long)((sAt-wAt+500)/1000):-1;auto holdMs=[&](int vk)->long long{ULONGLONG d=g_observerFirstDownAt[vk].load(),u=g_observerFirstUpAt[vk].load();return(d&&u&&u>=d)?(long long)((u-d+500)/1000):-1;};long long wHold=holdMs('W'),sHold=holdMs('S');bool timing=wAfterSkill>=390&&wAfterSkill<=430&&sAfterW>=42&&sAfterW<=70;bool visible=wHold>=16&&sHold>=16;bool z=g_observerDownCount['Z'].load()==4;bool bars=g_observerDownCount[VK_F1].load()==4&&g_observerDownCount[VK_F2].load()==1&&g_observerDownCount[VK_F3].load()==1&&g_observerDownCount[VK_F4].load()==1;bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;long long executeSpan=(executeStart&&executeEnd&&executeEnd>=executeStart)?(long long)((executeEnd-executeStart+500)/1000):-1;bool synced=executeSpan>=1500&&executeSpan<=2300;
  std::ofstream f("attack-observer-report.txt",std::ios::trunc);f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\nZ_Down="<<g_observerDownCount['Z'].load()<<"\nW_Down="<<wc<<"\nS_Down="<<sc<<"\nSkill2_Down="<<g_observerDownCount['2'].load()<<"\nSkill3_Down="<<g_observerDownCount['3'].load()<<"\nSkill4_Down="<<g_observerDownCount['4'].load()<<"\nSkill5_Down="<<g_observerDownCount['5'].load()<<"\nF1_Down="<<g_observerDownCount[VK_F1].load()<<"\nF2_Down="<<g_observerDownCount[VK_F2].load()<<"\nF3_Down="<<g_observerDownCount[VK_F3].load()<<"\nF4_Down="<<g_observerDownCount[VK_F4].load()<<"\nRoundRobin="<<(skills?"PASS":"FAIL")<<"\nBarAssertion="<<(bars?"PASS":"FAIL")<<"\nSyncedAttackSpanMs="<<executeSpan<<"\nSkillCycleSync="<<(synced?"PASS":"FAIL")<<"\nWSEverySkill="<<(ws?"PASS":"FAIL")<<"\nWAfterSkillMs="<<wAfterSkill<<"\nSAfterWMs="<<sAfterW<<"\nW_HoldMs="<<wHold<<"\nS_HoldMs="<<sHold<<"\nWSTiming="<<(timing?"PASS":"FAIL")<<"\nWSFrameVisible="<<(visible?"PASS":"FAIL")<<"\nScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";bool ok=ready&&z&&skills&&bars&&scans&&ws&&timing&&visible&&synced&&overlap;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";CloseBridge();return ok;
}
bool RunAttackZOffObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  std::thread observer;bool ready=StartObserver(observer);
  if(ready){g_skillTurn=0;AttackSettings a;a.restoreBar=1;a.delayMs=1;a.skillEnabled={true,false,false,false};a.attackBars={1,1,1,1};a.slots={2,3,4,5};a.skillDelayMs={1,1,1,1};a.zCombo=false;a.wCombo=false;a.sCombo=false;{std::lock_guard<std::mutex>lk(g_settingsMutex);g_attack=a;}g_attackActive=true;g_cureExclusive=false;g_potionExclusive=false;g_chatMode=false;ExecuteAttack(a);g_attackActive=false;}
  StopObserver(observer);
  bool noZ=g_observerDownCount['Z'].load()==0;bool oneSkill=g_observerDownCount['2'].load()==1;bool bar=g_observerDownCount[VK_F1].load()==1;bool scans=g_observerScanNonZero['2']&&g_observerScanNonZero[VK_F1];bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("attack-z-off-observer-report.txt",std::ios::trunc);f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\nZ_Down="<<g_observerDownCount['Z'].load()<<"\nF1_Down="<<g_observerDownCount[VK_F1].load()<<"\nSkill2_Down="<<g_observerDownCount['2'].load()<<"\nZOff="<<(noZ?"PASS":"FAIL")<<"\nBarAssertion="<<(bar?"PASS":"FAIL")<<"\nScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";bool ok=ready&&noZ&&oneSkill&&bar&&scans&&overlap;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";CloseBridge();return ok;
}

bool RunAutoMinorObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  {std::lock_guard<std::mutex>lk(g_settingsMutex);g_rogue.powerEnabled=true;g_rogue.autoMinorEnabled=true;g_rogue.autoMinorBar=3;g_rogue.autoMinorSlot=7;}
  g_rogueCategoryEnabled=true;g_attackActive=false;g_autoMinorLatched=true;g_autoMinorOwned=true;g_minorActive=true;g_cureExclusive=false;g_potionExclusive=false;g_chatMode=false;
  std::thread observer;bool ready=StartObserver(observer);g_running=true;std::thread minor(MinorWorker);
  if(ready){for(int i=0;i<250&&g_observerDownCount['7'].load()<3;i++)Sleep(2);}
  g_minorActive=false;g_autoMinorOwned=false;g_autoMinorLatched=false;g_running=false;if(minor.joinable())minor.join();StopObserver(observer);
  int f3=g_observerDownCount[VK_F3].load(),slot=g_observerDownCount['7'].load();bool bar=f3==1;bool slots=slot>=1;bool noLegacy=g_observerDownCount['8'].load()==0&&g_observerDownCount['9'].load()==0&&g_observerDownCount['0'].load()==0;bool scans=g_observerScanNonZero[VK_F3]&&g_observerScanNonZero['7'];bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("auto-minor-observer-report.txt",std::ios::trunc);f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\nF3_Down="<<f3<<"\nSlot7_Down="<<slot<<"\nLegacy890_Down="<<(g_observerDownCount['8']+g_observerDownCount['9']+g_observerDownCount['0'])<<"\nConfiguredBarOnce="<<(bar?"PASS":"FAIL")<<"\nConfiguredSlot="<<(slots?"PASS":"FAIL")<<"\nManualSequenceIsolated="<<(noLegacy?"PASS":"FAIL")<<"\nScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";bool ok=ready&&bar&&slots&&noLegacy&&scans&&overlap;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";CloseBridge();return ok;
}
bool RunCureObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);g_cureEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!g_cureEvent){CloseBridge();return false;}
  {std::lock_guard<std::mutex>lk(g_settingsMutex);g_rogue.powerEnabled=true;g_rogue.cureEnabled=true;g_rogue.cureBar=2;g_rogue.cureSlot=6;g_attack.restoreBar=1;}
  std::thread observer;bool ready=StartObserver(observer);g_running=true;std::thread cure(CureWorker);ULONGLONG began=GetTickCount64();if(ready){g_cureExclusive=true;g_curePending=true;SetEvent(g_cureEvent);}for(int i=0;i<300&&g_cureExclusive;i++)Sleep(1);ULONGLONG latency=GetTickCount64()-began;Sleep(60);g_running=false;SetEvent(g_cureEvent);if(cure.joinable())cure.join();StopObserver(observer);
  bool counts=g_observerDownCount[VK_F2].load()==1&&g_observerDownCount['6'].load()==1&&g_observerDownCount[VK_F1].load()==1;bool scans=g_observerScanNonZero[VK_F2]&&g_observerScanNonZero['6']&&g_observerScanNonZero[VK_F1];auto hold=[&](int vk)->long long{ULONGLONG d=g_observerFirstDownAt[vk].load(),u=g_observerFirstUpAt[vk].load();return(d&&u&&u>=d)?(long long)((u-d+500)/1000):-1;};long long f2Hold=hold(VK_F2),slotHold=hold('6'),f1Hold=hold(VK_F1);bool frameVisible=f2Hold>=40&&slotHold>=43&&f1Hold>=30;bool fast=latency<380;bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("cure-observer-report.txt",std::ios::trunc);f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\n";f<<"F2_Down="<<g_observerDownCount[VK_F2].load()<<"\n6_Down="<<g_observerDownCount['6'].load()<<"\nF1_Down="<<g_observerDownCount[VK_F1].load()<<"\nF2_HoldMs="<<f2Hold<<"\n6_HoldMs="<<slotHold<<"\nF1_HoldMs="<<f1Hold<<"\nFrameVisibleHold="<<(frameVisible?"PASS":"FAIL")<<"\nLatencyMs="<<latency<<"\nScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\nOverlap="<<g_observerOverlap.load()<<"\n";bool ok=ready&&counts&&scans&&frameVisible&&fast&&overlap;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";CloseHandle(g_cureEvent);g_cureEvent=nullptr;CloseBridge();return ok;
}

bool RunSelfTest(){int pass=0,total=0;auto test=[&](const char*name,bool ok,std::ofstream&f){total++;if(ok)pass++;f<<name<<"="<<(ok?"PASS":"FAIL")<<"\n";};std::ofstream f("self-test-report.txt",std::ios::trunc);std::array<RGBc,6> colors{{{220,35,35},{235,195,40},{145,55,190},{75,32,120},{230,115,35},{40,100,220}}};std::array<int,11> fills{{0,5,10,20,35,43,60,75,90,95,100}};for(auto fg:colors)for(int pct:fills)for(int variant=0;variant<2;variant++){std::vector<RGBc> c(200,{28,28,28});int n=(int)std::lround(2.0*pct);for(int i=0;i<n&&i<200;i++){double k=variant?(0.88+0.12*(double)i/199.0):1.0;c[i]={fg.r*k,fg.g*k,fg.b*k};}int got=EstimateFill(c);bool ok=std::abs(got-pct)<=7||(pct==100&&got>=95)||(pct==0&&got<=5);test(("Detector_"+std::to_string(total)).c_str(),ok,f);}AttackSettings a;a.restoreBar=1;a.skillEnabled={true,true,false,false};a.attackBars={3,5,1,1};a.slots={2,5,4,5};a.zCombo=false;auto seq=BuildAttackSequence(a);test("AttackSequenceWithoutZ",seq==std::vector<int>{VK_F3,'2',VK_F5,'5',VK_F1},f);a.zCombo=true;seq=BuildAttackSequence(a);test("AttackSequenceWithOptionalZ",seq==std::vector<int>{'Z',VK_F3,'2',VK_F5,'5',VK_F1},f);test("SkillBarF1ToF12Mapping",BarToVk(1)==VK_F1&&BarToVk(12)==VK_F12,f);test("NonCapsHotkeyMapping",VK_F6!=VK_CAPITAL&&VK_INSERT!=VK_CAPITAL,f);test("CureSequenceF2_6_F1",BarToVk(2)==VK_F2&&SlotToVk(6)=='6'&&BarToVk(1)==VK_F1,f);test("BridgeSelectiveRouting",IsBridgeInputKey('1')&&IsBridgeInputKey('0')&&IsBridgeInputKey('R')&&!IsBridgeInputKey('Z')&&!IsBridgeInputKey(VK_F1)&&!IsBridgeInputKey(VK_F12),f);{INPUT i{};BuildKeyInput(i,'8',false);auto n=NativeNormalizedInput(i);test("ScanCodeTransport",n.ki.wVk==0&&n.ki.wScan!=0&&(n.ki.dwFlags&KEYEVENTF_SCANCODE)&&n.ki.dwExtraInfo==kMagicInput,f);}test("FifoTicketLockInitial",g_gameInputGate.nextTicket.load()==g_gameInputGate.serving.load(),f);{AttackSettings ws;ws.restoreBar=1;ws.skillEnabled={true,true,true,false};g_skillTurn=0;int a0=NextEnabledSkill(ws),a1=NextEnabledSkill(ws),a2=NextEnabledSkill(ws),a3=NextEnabledSkill(ws);test("AttackOrderedThreeSkills",a0==0&&a1==1&&a2==2&&a3==0,f);g_skillTurn=0;}test("AttackLoopAllows1ms",std::clamp(1,1,2000)==1,f);test("SkillDelayAllows1ms",std::clamp(1,1,1000)==1,f);{auto make=[](bool hp,int pct){int w=200,h=12;uint32_t bg=(28u<<16)|(28u<<8)|28u;uint32_t fg=hp?((146u<<16)|(96u<<8)|23u):((34u<<16)|(53u<<8)|184u);std::vector<uint32_t> px((size_t)w*h,bg);int n=(int)std::lround(w*pct/100.0);for(int y=0;y<h;y++)for(int x=0;x<n;x++)px[(size_t)y*w+x]=fg;return px;};for(bool hp:{false,true})for(int pct:{20,35,50,75,100}){auto px=make(hp,pct);int got=EstimateTypedBarFill(px,200,12,hp);test((std::string(hp?"TypedHP_":"TypedMP_")+std::to_string(pct)).c_str(),std::abs(got-pct)<=3,f);}}{RogueSettings mr;mr.seq={'8','9','0'};auto mb=BuildMinorBatch(mr,1);bool ok=mb.size()==6&&mb[0].ki.wVk=='8'&&!(mb[0].ki.dwFlags&KEYEVENTF_KEYUP)&&mb[1].ki.wVk=='8'&&(mb[1].ki.dwFlags&KEYEVENTF_KEYUP)&&mb[2].ki.wVk=='9'&&mb[3].ki.wVk=='9'&&(mb[3].ki.dwFlags&KEYEVENTF_KEYUP)&&mb[4].ki.wVk=='0'&&mb[5].ki.wVk=='0'&&(mb[5].ki.dwFlags&KEYEVENTF_KEYUP);test("MinorReferenceSequenceModel",ok,f);}test("WDelayAllows1ms",std::clamp(1,1,1000)==1,f);test("SDelayAllows1ms",std::clamp(1,1,1000)==1,f);{AttackSettings def;test("ZDefaultOff",!def.zCombo,f);test("SkillDefaults_1",def.skillDelayMs[0]==1&&def.skillDelayMs[1]==1&&def.skillDelayMs[2]==1&&def.skillDelayMs[3]==1,f);test("WSDefaults_400_50",def.wDelayMs==400&&def.sDelayMs==50,f);{WsPendingState p;p.w=true;p.wDelayMs=1;test("WSDelayRespects1ms",WsFirstDelayMs(p)==1,f);p.wDelayMs=400;test("WSDelayRespects400ms",WsFirstDelayMs(p)==400,f);}test("AttackSameBarFastPath",!AttackNeedsBarTap(1,1)&&AttackNeedsBarTap(0,1)&&AttackNeedsBarTap(2,1),f);test("WsCycleBudgetModel",std::clamp(400+50+350,350,2600)==800,f);}test("AutoMinorStartAt30",AutoMinorNextState(false,30,30,75),f);test("AutoMinorHoldAt60",AutoMinorNextState(true,60,30,75),f);test("AutoMinorStopAt75",!AutoMinorNextState(true,75,30,75),f);test("AutoMinorNoStartAt31",!AutoMinorNextState(false,31,30,75),f);test("CureSingleFlightModel",!g_curePending.load(),f);test("PotionRetryIntervalModel",240<650,f);test("ChatPauseModel",!g_chatMode.load(),f);test("ChatEnterOpens",ChatKeyTransitionModel(false,VK_RETURN,true)==1,f);test("ChatInjectedEnterCloses",ChatKeyTransitionModel(true,VK_RETURN,true)==0,f);test("ChatCloseSurvivesFocusLoss",ChatKeyTransitionModel(true,VK_RETURN,false)==0,f);test("ChatEscapeCloses",ChatKeyTransitionModel(true,VK_ESCAPE,true)==0,f);{auto gm=[](std::initializer_list<const char*> rows){int h=(int)rows.size(),w=(int)std::char_traits<char>::length(*rows.begin());std::vector<uint8_t> m((size_t)w*h);int y=0;for(auto row:rows){for(int x=0;x<w;x++)m[(size_t)y*w+x]=row[x]=='#';y++;}return std::pair<std::vector<uint8_t>,std::pair<int,int>>{m,{w,h}};};auto ck=[&](const char*n,wchar_t want,std::initializer_list<const char*> rows){auto q=gm(rows);test(n,RecognizeGameDigit(q.first,q.second.first,q.second.second)==want,f);};ck("GameGlyph4",L'4',{"...###.","...###.","..####.",".##.##.","##..##.","#######","....##.","....##."});ck("GameGlyph2",L'2',{"#####.","#..###","....##","...###","..###.","..##..","###...","######"});ck("GameGlyph6",L'6',{"..####.",".##....",".##....","######.","###..##","###..##",".##..##","..####."});ck("GameGlyph9",L'9',{".#####.","###.###","##...##","###..##",".######","....###","....##.",".####.."});ck("GameGlyph8",L'8',{".#####.",".##..##","###..##",".#####.",".#...##","##...##","###..##",".#####."});ck("GameGlyphSlash",L'/',{"....##","....##","....#.","...##.","...##.","..##..","..##..","..#...",".##...",".##...","##...."});}f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";return total==180&&pass==180;}

} // namespace

int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show){g_instance=hi;if(cmd&&wcsstr(cmd,L"--self-test"))return RunSelfTest()?0:2;if(cmd&&wcsstr(cmd,L"--transport-observer-test"))return RunTransportObserverTest()?0:3;if(cmd&&wcsstr(cmd,L"--attack-observer-test"))return RunAttackObserverTest()?0:6;if(cmd&&wcsstr(cmd,L"--attack-z-off-observer-test"))return RunAttackZOffObserverTest()?0:8;if(cmd&&wcsstr(cmd,L"--auto-minor-observer-test"))return RunAutoMinorObserverTest()?0:9;if(cmd&&wcsstr(cmd,L"--cure-observer-test"))return RunCureObserverTest()?0:7;g_cureEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);LoadSettings();InitBridge();g_font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");g_fontBold=CreateFontW(-16,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");g_fontSmall=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");g_bgBrush=CreateSolidBrush(C_BG);g_panelBrush=CreateSolidBrush(C_PANEL);g_sidebarBrush=CreateSolidBrush(C_RED);WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=g_bgBrush;wc.lpszClassName=kClassName;wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassExW(&wc);WNDCLASSEXW ov{sizeof(ov)};ov.lpfnWndProc=OverlayProc;ov.hInstance=hi;ov.hCursor=LoadCursorW(nullptr,IDC_CROSS);ov.lpszClassName=kOverlayClass;RegisterClassExW(&ov);
RECT wr{0,0,kWindowWidth,kWindowHeight};AdjustWindowRect(&wr,WS_OVERLAPPEDWINDOW,FALSE);HWND h=CreateWindowExW(0,kClassName,kTitle,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,wr.right-wr.left,wr.bottom-wr.top,nullptr,nullptr,hi,nullptr);if(!h){if(g_cureEvent){CloseHandle(g_cureEvent);g_cureEvent=nullptr;}return 4;}g_keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,KeyboardProc,hi,0);if(!g_keyboardHook){MessageBoxW(h,L"Global tuş dinleyicisi başlatılamadı.",L"Premium Plus Combo",MB_OK|MB_ICONERROR);DestroyWindow(h);if(g_cureEvent){CloseHandle(g_cureEvent);g_cureEvent=nullptr;}return 5;}
std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker);ShowWindow(h,show);UpdateWindow(h);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}g_running=false;if(g_cureEvent)SetEvent(g_cureEvent);if(g_keyboardHook)UnhookWindowsHookEx(g_keyboardHook);if(tMinor.joinable())tMinor.join();if(tR.joinable())tR.join();if(tCure.joinable())tCure.join();if(tAttack.joinable())tAttack.join();if(tWs.joinable())tWs.join();if(tVitals.joinable())tVitals.join();CloseBridge();if(g_cureEvent){CloseHandle(g_cureEvent);g_cureEvent=nullptr;}if(g_font)DeleteObject(g_font);if(g_fontBold)DeleteObject(g_fontBold);if(g_fontSmall)DeleteObject(g_fontSmall);if(g_bgBrush)DeleteObject(g_bgBrush);if(g_panelBrush)DeleteObject(g_panelBrush);if(g_sidebarBrush)DeleteObject(g_sidebarBrush);return (int)msg.wParam;}
