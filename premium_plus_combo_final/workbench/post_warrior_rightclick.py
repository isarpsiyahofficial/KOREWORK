import pathlib,sys
p=pathlib.Path(sys.argv[1]); s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.index(sig); q=s.index('{',a); d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise RuntimeError('unclosed '+sig)

def once(old,new,label):
    global s
    c=s.count(old)
    if c!=1: raise RuntimeError(f'{label}: expected 1 got {c}')
    s=s.replace(old,new,1)

def replace_fn(sig,new):
    global s
    a,b=span(sig); s=s[:a]+new+s[b:]

once('constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue | v4.8.15";', 'constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.16";', 'title')
once('constexpr wchar_t kMobRegistry[] = L"Software\\\\PremiumPlusCombo\\\\MobAttackV1";', 'constexpr wchar_t kMobRegistry[] = L"Software\\\\PremiumPlusCombo\\\\MobAttackV1";\nconstexpr wchar_t kWarriorRegistry[] = L"Software\\\\PremiumPlusCombo\\\\WarriorV1";', 'registry')
once('constexpr int IDC_MOB_TAB_PRIEST = 1761;', '''constexpr int IDC_MOB_TAB_PRIEST = 1761;
constexpr int IDC_CATEGORY_WARRIOR = 1800;
constexpr int IDC_WARRIOR_ENABLE = 1801;
constexpr int IDC_WARRIOR_SHIELD_SLOT = 1802;
constexpr int IDC_WARRIOR_SHIELD_KEY = 1803;
constexpr int IDC_WARRIOR_WEAPON_SLOT = 1804;
constexpr int IDC_WARRIOR_WEAPON_KEY = 1805;
constexpr int IDC_WARRIOR_SAVE = 1806;''', 'warrior ids')

once('struct Ui {', '''struct WarriorSettings {
  bool enabled=false;
  int shieldSlot=1;
  int shieldHotkey='X';
  int weaponSlot=2;
  int weaponHotkey='C';
};

struct InventoryGrid {
  bool valid=false;
  int left=0,top=0,cell=0,clientW=0,clientH=0;
  POINT screenOrigin{};
  double score=0;
};

struct Ui {''', 'warrior structs')
once('HWND main{},power{},catRogue{},catAttack{},catMob{},status{};', 'HWND main{},power{},catRogue{},catAttack{},catMob{},catWarrior{},status{};', 'ui cat warrior')
once('std::vector<HWND> roguePage,attackPage,mobPage,mobGeneralPage,mobPriestPage;', 'std::vector<HWND> roguePage,attackPage,mobPage,mobGeneralPage,mobPriestPage,warriorPage;', 'warrior page')
once('HWND mobHealEnable{},mobHealPct{},mobHealBar{},mobHealSlot{},mobHpCal{},mobSave{},mobPosStatus{},mobTabGeneral{},mobTabPriest{};', 'HWND mobHealEnable{},mobHealPct{},mobHealBar{},mobHealSlot{},mobHpCal{},mobSave{},mobPosStatus{},mobTabGeneral{},mobTabPriest{};\n  HWND warriorEnable{},warriorShieldSlot{},warriorShieldKey{},warriorWeaponSlot{},warriorWeaponKey{},warriorSave{},warriorStatus{};', 'warrior ui fields')
once('MobSettings g_mob;', 'MobSettings g_mob;\nWarriorSettings g_warrior;', 'warrior global settings')
once('std::atomic<int> g_mobSubTab{0};', '''std::atomic<int> g_mobSubTab{0};
std::atomic<int> g_warriorRequest{0}; // 1 shield, 2 weapon
std::atomic<ULONG_PTR> g_warriorRequestWindow{0};
std::atomic<int> g_warriorLastResult{0}; // 0 ready,1 click ok,2 grid fail,3 focus lost
HANDLE g_warriorEvent{};''', 'warrior runtime globals')

a,b=span('void LoadSettings()'); fn=s[a:b]
insert='''\n  if(RegCreateKeyExW(HKEY_CURRENT_USER,kWarriorRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){
    g_warrior.enabled=ReadDword(k,L"Enabled",0)!=0;
    g_warrior.shieldSlot=ClampD(ReadDword(k,L"ShieldSlot",1),1,28);
    g_warrior.shieldHotkey=ClampD(ReadDword(k,L"ShieldHotkey",'X'),1,255);
    g_warrior.weaponSlot=ClampD(ReadDword(k,L"WeaponSlot",2),1,28);
    g_warrior.weaponHotkey=ClampD(ReadDword(k,L"WeaponHotkey",'C'),1,255);
    if(g_warrior.weaponHotkey==g_warrior.shieldHotkey)g_warrior.weaponHotkey='C';
    RegCloseKey(k);
  }
'''
fn=fn[:-1]+insert+'}'
s=s[:a]+fn+s[b:]
once('\nbool InitPositionBridge(){', '''
void SaveWarrior(){HKEY k{};std::lock_guard<std::mutex>lk(g_settingsMutex);if(RegCreateKeyExW(HKEY_CURRENT_USER,kWarriorRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return;WriteDword(k,L"Enabled",g_warrior.enabled?1:0);WriteDword(k,L"ShieldSlot",g_warrior.shieldSlot);WriteDword(k,L"ShieldHotkey",g_warrior.shieldHotkey);WriteDword(k,L"WeaponSlot",g_warrior.weaponSlot);WriteDword(k,L"WeaponHotkey",g_warrior.weaponHotkey);RegCloseKey(k);}

bool InitPositionBridge(){''','save warrior')

replace_fn('void ShowCategory(int category)',r'''void ShowCategory(int category){
  g_currentCategory=category;
  bool rogue=category==0,attack=category==1,mob=category==2,warrior=category==3;
  for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.attackPage)ShowWindow(h,attack?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.mobPage)ShowWindow(h,mob?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.warriorPage)ShowWindow(h,warrior?SW_SHOW:SW_HIDE);
  ShowMobSubCategory(g_mobSubTab.load());
  InvalidateRect(g_ui.main,nullptr,TRUE);
}''')

once('void ReadRogueUi(bool warn){', r'''void ReadWarriorUi(bool persist=true){
  WarriorSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_warrior;}
  if(g_ui.warriorEnable)n.enabled=SendMessageW(g_ui.warriorEnable,BM_GETCHECK,0,0)==BST_CHECKED;
  if(g_ui.warriorShieldSlot)n.shieldSlot=GetInt(g_ui.warriorShieldSlot,n.shieldSlot,1,28);
  if(g_ui.warriorWeaponSlot)n.weaponSlot=GetInt(g_ui.warriorWeaponSlot,n.weaponSlot,1,28);
  {std::lock_guard<std::mutex>lk(g_settingsMutex);g_warrior=n;}
  if(persist)SaveWarrior();
}
void PopulateWarriorUi(){
  if(!g_ui.warriorEnable)return;WarriorSettings w;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;}
  SendMessageW(g_ui.warriorEnable,BM_SETCHECK,w.enabled?BST_CHECKED:BST_UNCHECKED,0);
  SetWindowTextW(g_ui.warriorShieldSlot,std::to_wstring(w.shieldSlot).c_str());
  SetWindowTextW(g_ui.warriorWeaponSlot,std::to_wstring(w.weaponSlot).c_str());
}

void ReadRogueUi(bool warn){''','warrior ui helpers')

a,b=span('void RefreshHotkeyLabels()'); fn=s[a:b]
fn=fn.replace('RogueSettings r;AttackSettings a;MobSettings m;','RogueSettings r;AttackSettings a;MobSettings m;WarriorSettings w;')
fn=fn.replace('r=g_rogue;a=g_attack;m=g_mob;','r=g_rogue;a=g_attack;m=g_mob;w=g_warrior;')
fn=fn.replace('if(g_ui.mobStopAssign)set(g_ui.mobStopAssign,L"Kapatma",m.stopHotkey);','if(g_ui.mobStopAssign)set(g_ui.mobStopAssign,L"Kapatma",m.stopHotkey);if(g_ui.warriorShieldKey)set(g_ui.warriorShieldKey,L"Kalkan",w.shieldHotkey);if(g_ui.warriorWeaponKey)set(g_ui.warriorWeaponKey,L"Silah",w.weaponHotkey);')
s=s[:a]+fn+s[b:]
a,b=span('void RefreshStatus()'); fn=s[a:b]
fn=fn.replace('SetWindowTextW(g_ui.status,s.c_str());', 'SetWindowTextW(g_ui.status,s.c_str());if(g_ui.warriorStatus){int wr=g_warriorLastResult.load();const wchar_t* t=wr==1?L"Son işlem: SAĞ TIK OK":wr==2?L"Envanter 7x4 bulunamadı":wr==3?L"Oyun odağı kayboldu":L"Hazır • 28 slot otomatik algılama";SetWindowTextW(g_ui.warriorStatus,t);}')
s=s[:a]+fn+s[b:]

a,b=span('void PopulateUi()'); fn=s[a:b]
fn=fn.replace('PopulateMobUi();RefreshHotkeyLabels();', 'PopulateMobUi();PopulateWarriorUi();RefreshHotkeyLabels();')
s=s[:a]+fn+s[b:]

once('void CreateRoguePage(){', r'''static uint8_t WarriorGray(uint32_t p){int b=p&255,g=(p>>8)&255,r=(p>>16)&255;return (uint8_t)((r*77+g*150+b*29)>>8);}
static uint64_t SumIntegral(const std::vector<uint64_t>&q,int W,int H,int l,int t,int r,int b){l=std::clamp(l,0,W);r=std::clamp(r,0,W);t=std::clamp(t,0,H);b=std::clamp(b,0,H);if(r<=l||b<=t)return 0;const int S=W+1;return q[(size_t)b*S+r]-q[(size_t)t*S+r]-q[(size_t)b*S+l]+q[(size_t)t*S+l];}
static double MeanIntegral(const std::vector<uint64_t>&q,int W,int H,int l,int t,int r,int b){int ww=std::max(0,std::min(W,r)-std::max(0,l)),hh=std::max(0,std::min(H,b)-std::max(0,t));if(!ww||!hh)return 0;return (double)SumIntegral(q,W,H,l,t,r,b)/(double)(ww*hh);}
static double MedianSmall(double* a,int n){std::sort(a,a+n);return a[n/2];}
bool DetectInventoryGridPixels(const std::vector<uint32_t>&px,int W,int H,InventoryGrid&out){
  out={};if(W<640||H<420||(int)px.size()<W*H)return false;
  std::vector<uint8_t> g((size_t)W*H);for(size_t i=0;i<g.size();i++)g[i]=WarriorGray(px[i]);
  std::vector<uint64_t> ix((size_t)(W+1)*(H+1)),iy(ix.size());
  for(int y=0;y<H;y++){uint64_t rx=0,ry=0;for(int x=0;x<W;x++){int at=y*W+x;int vx=x?std::abs((int)g[at]-(int)g[at-1]):0;int vy=y?std::abs((int)g[at]-(int)g[at-W]):0;rx+=vx;ry+=vy;size_t o=(size_t)(y+1)*(W+1)+x+1;ix[o]=ix[o-(W+1)]+rx;iy[o]=iy[o-(W+1)]+ry;}}
  const int sMin=std::max(24,H*45/1000),sMax=std::min(120,H*90/1000);double best=-1;InventoryGrid bg{};
  for(int cell=sMin;cell<=sMax;cell++){
    int gw=7*cell,gh=4*cell;if(gw>W*46/100||gh>H*42/100)continue;
    int m0=std::max(6,W*5/1000),m1=std::max(m0+3,W*80/1000);
    for(int margin=m0;margin<=m1;margin+=3){int left=W-margin-gw;if(left<W*52/100||left<2)continue;
      for(int top=H*50/100;top<=H*70/100;top+=3){if(top+gh>=H*93/100)continue;double vv[8],hh[5],va=0,ha=0;for(int j=0;j<8;j++){int x=left+j*cell;vv[j]=MeanIntegral(ix,W,H,x-1,top,x+2,top+gh+1);va+=vv[j];}for(int i=0;i<5;i++){int y=top+i*cell;hh[i]=MeanIntegral(iy,W,H,left,y-1,left+gw+1,y+2);ha+=hh[i];}va/=8;ha/=5;double vm[8],hm[5];std::copy(vv,vv+8,vm);std::copy(hh,hh+5,hm);double vmed=MedianSmall(vm,8),hmed=MedianSmall(hm,5);double score=va+ha;if(vmed<18||hmed<20||score<50)continue;if(score>best){best=score;bg.valid=true;bg.left=left;bg.top=top;bg.cell=cell;bg.clientW=W;bg.clientH=H;bg.score=score;}}
    }
  }
  if(!bg.valid)return false;out=bg;return true;
}
bool CaptureGameClient(HWND game,std::vector<uint32_t>&px,int&w,int&h,POINT&origin){
  px.clear();w=h=0;origin={};if(!game||!IsWindow(game))return false;RECT cr{};if(!GetClientRect(game,&cr))return false;POINT p{0,0};if(!ClientToScreen(game,&p))return false;w=cr.right-cr.left;h=cr.bottom-cr.top;if(w<100||h<100)return false;origin=p;
  HDC scr=GetDC(nullptr);if(!scr)return false;HDC mem=CreateCompatibleDC(scr);HBITMAP bm=CreateCompatibleBitmap(scr,w,h);if(!mem||!bm){if(bm)DeleteObject(bm);if(mem)DeleteDC(mem);ReleaseDC(nullptr,scr);return false;}HGDIOBJ old=SelectObject(mem,bm);bool ok=BitBlt(mem,0,0,w,h,scr,p.x,p.y,SRCCOPY|CAPTUREBLT)!=0;px.resize((size_t)w*h);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;if(!ok||!GetDIBits(mem,bm,0,h,px.data(),&bi,DIB_RGB_COLORS))ok=false;SelectObject(mem,old);DeleteObject(bm);DeleteDC(mem);ReleaseDC(nullptr,scr);if(!ok)px.clear();return ok;
}
bool FindInventoryGrid(HWND game,InventoryGrid&grid){std::vector<uint32_t>px;int w=0,h=0;POINT o{};if(!CaptureGameClient(game,px,w,h,o))return false;if(!DetectInventoryGridPixels(px,w,h,grid))return false;grid.screenOrigin=o;return true;}
POINT WarriorSlotCellCenter(const InventoryGrid&g,int slot){slot=std::clamp(slot,1,28)-1;int col=slot%7,row=slot/7;return {g.screenOrigin.x+g.left+col*g.cell+g.cell/2,g.screenOrigin.y+g.top+row*g.cell+g.cell/2};}
bool WarriorGridStable(const InventoryGrid&a,const InventoryGrid&b){return a.valid&&b.valid&&std::abs(a.left-b.left)<=3&&std::abs(a.top-b.top)<=3&&std::abs(a.cell-b.cell)<=2;}
bool WarriorResolveInventory(HWND game,InventoryGrid&grid){
  InventoryGrid a{},b{};if(FindInventoryGrid(game,a)){Sleep(3);if(FindInventoryGrid(game,b)&&WarriorGridStable(a,b)){grid=b;return true;}}
  if(GetForegroundWindow()!=game)return false;ReferenceTapKey('I');ULONGLONG deadline=GetTickCount64()+320;InventoryGrid prev{};int stable=0;while(g_running&&GetTickCount64()<deadline){Sleep(6);InventoryGrid cur{};if(FindInventoryGrid(game,cur)){stable=WarriorGridStable(prev,cur)?stable+1:1;prev=cur;if(stable>=2){grid=cur;return true;}}else stable=0;}return false;
}
bool WarriorRightClickSlot(HWND game,int slot){
  if(!game||GetForegroundWindow()!=game)return false;InventoryGrid grid{};if(!WarriorResolveInventory(game,grid))return false;if(GetForegroundWindow()!=game)return false;POINT pt=WarriorSlotCellCenter(grid,slot);POINT old{};GetCursorPos(&old);if(!SetCursorPos(pt.x,pt.y))return false;PreciseDelayUs(1200);INPUT d{},u{};d.type=INPUT_MOUSE;d.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;d.mi.dwExtraInfo=kMagicInput;u=d;u.mi.dwFlags=MOUSEEVENTF_RIGHTUP;{
    FifoTicketGuard gate(g_gameInputGate);if(SendInput(1,&d,sizeof(INPUT))!=1){SetCursorPos(old.x,old.y);return false;}PreciseDelayUs(4500);if(SendInput(1,&u,sizeof(INPUT))!=1){SetCursorPos(old.x,old.y);return false;}}
  PreciseDelayUs(800);SetCursorPos(old.x,old.y);return true;
}
void WarriorWorker(){while(g_running){if(!g_warriorEvent){Sleep(10);continue;}DWORD wr=WaitForSingleObject(g_warriorEvent,50);if(wr!=WAIT_OBJECT_0)continue;int req=g_warriorRequest.exchange(0);HWND game=(HWND)g_warriorRequestWindow.exchange(0);WarriorSettings w;RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;r=g_rogue;}if(!req||!w.enabled||!r.powerEnabled)continue;if(!game||GetForegroundWindow()!=game){g_warriorLastResult=3;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);continue;}int slot=req==1?w.shieldSlot:w.weaponSlot;bool ok=WarriorRightClickSlot(game,slot);g_warriorLastResult=ok?1:(GetForegroundWindow()==game?2:3);PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);}}

void CreateWarriorPage(){auto&p=g_ui.warriorPage;PageAdd(p,Label(L"WARRIOR",kContentX,82,160,22,g_fontBold));g_ui.warriorEnable=Ctrl(L"BUTTON",L"WARRIOR AKTİF",BS_AUTOCHECKBOX,kContentX+460,82,120,22,IDC_WARRIOR_ENABLE,g_fontSmall);PageAdd(p,g_ui.warriorEnable);PageAdd(p,Label(L"Kalkan / silah hızlı değişim",kContentX,114,250,20,g_fontBold));PageAdd(p,Label(L"Envanter 28 slot: 7 sütun × 4 satır otomatik görüntü algılama. Sabit ekran koordinatı kullanılmaz.",kContentX,140,570,36,g_fontSmall));
  PageAdd(p,Label(L"KALKAN",kContentX,194,70,20,g_fontBold));PageAdd(p,Label(L"Slot (1-28)",kContentX+88,194,70,20,g_fontSmall));g_ui.warriorShieldSlot=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+162,192,46,22,IDC_WARRIOR_SHIELD_SLOT,g_fontSmall);PageAdd(p,g_ui.warriorShieldSlot);g_ui.warriorShieldKey=Ctrl(L"BUTTON",L"Kalkan: X",BS_PUSHBUTTON,kContentX+226,190,142,26,IDC_WARRIOR_SHIELD_KEY,g_fontSmall);PageAdd(p,g_ui.warriorShieldKey);
  PageAdd(p,Label(L"SİLAH",kContentX,232,70,20,g_fontBold));PageAdd(p,Label(L"Slot (1-28)",kContentX+88,232,70,20,g_fontSmall));g_ui.warriorWeaponSlot=Ctrl(L"EDIT",L"2",WS_BORDER|ES_CENTER,kContentX+162,230,46,22,IDC_WARRIOR_WEAPON_SLOT,g_fontSmall);PageAdd(p,g_ui.warriorWeaponSlot);g_ui.warriorWeaponKey=Ctrl(L"BUTTON",L"Silah: C",BS_PUSHBUTTON,kContentX+226,228,142,26,IDC_WARRIOR_WEAPON_KEY,g_fontSmall);PageAdd(p,g_ui.warriorWeaponKey);
  PageAdd(p,Label(L"Çalışma: Hotkey → envanter açıksa I YOK → seçilen kutunun merkezine SAĞ TIK. Kapalıysa I bir kez → grid görünür görünmez SAĞ TIK.",kContentX,282,570,44,g_fontSmall));g_ui.warriorStatus=Label(L"Hazır • 28 slot otomatik algılama",kContentX,342,440,22,g_fontSmall);PageAdd(p,g_ui.warriorStatus);g_ui.warriorSave=Ctrl(L"BUTTON",L"WARRIOR AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,382,238,28,IDC_WARRIOR_SAVE,g_fontBold);PageAdd(p,g_ui.warriorSave);PopulateWarriorUi();}

void CreateRoguePage(){''','warrior engine/ui')

a,b=span('void CreateControls()'); fn=s[a:b]
fn=fn.replace('g_ui.catMob=Ctrl(L"BUTTON",L"MOB ATTACK",BS_OWNERDRAW,10,180,96,34,IDC_CATEGORY_MOB,g_fontSmall);', 'g_ui.catMob=Ctrl(L"BUTTON",L"MOB ATTACK",BS_OWNERDRAW,10,180,96,34,IDC_CATEGORY_MOB,g_fontSmall);g_ui.catWarrior=Ctrl(L"BUTTON",L"WARRIOR",BS_OWNERDRAW,10,222,96,34,IDC_CATEGORY_WARRIOR,g_fontBold);')
fn=fn.replace('CreateRoguePage();CreateAttackPage();CreateMobPage();PopulateUi();', 'CreateRoguePage();CreateAttackPage();CreateMobPage();CreateWarriorPage();PopulateUi();')
s=s[:a]+fn+s[b:]

a,b=span('void DrawOwnerButton(DRAWITEMSTRUCT* d)'); fn=s[a:b]
fn=fn.replace('d->CtlID==IDC_CATEGORY_MOB', 'd->CtlID==IDC_CATEGORY_MOB||d->CtlID==IDC_CATEGORY_WARRIOR')
s=s[:a]+fn+s[b:]

a,b=span('LRESULT CALLBACK KeyboardProc'); fn=s[a:b]
old='''int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else if(t==6||t==7){RogueSettings cr;AttackSettings ca;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}'''
new='''int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else{RogueSettings cr;AttackSettings ca;MobSettings cm;WarriorSettings cw;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;cm=g_mob;cw=g_warrior;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey&&vk!=cm.startHotkey&&vk!=cm.stopHotkey;if(t==8)allowed=allowed&&vk!=cw.weaponHotkey;if(t==9)allowed=allowed&&vk!=cw.shieldHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;else if(t==8)g_warrior.shieldHotkey=vk;else if(t==9)g_warrior.weaponHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}'''
if old not in fn: raise RuntimeError('keyboard assignment anchor missing')
fn=fn.replace(old,new,1)
fn=fn.replace('RogueSettings r;AttackSettings a;MobSettings m;', 'RogueSettings r;AttackSettings a;MobSettings m;WarriorSettings ww;',1).replace('r=g_rogue;a=g_attack;m=g_mob;', 'r=g_rogue;a=g_attack;m=g_mob;ww=g_warrior;',1)
anchor='''if(g_chatMode.load())return CallNextHookEx(g_keyboardHook,code,wp,lp);'''
war='''if(g_chatMode.load())return CallNextHookEx(g_keyboardHook,code,wp,lp);
  if(ww.enabled&&(vk==ww.shieldHotkey||vk==ww.weaponHotkey)){HWND fg=GetForegroundWindow();if(fg&&!IsOurUiWindow(fg)){RememberGameWindow();g_warriorRequestWindow=(ULONG_PTR)fg;g_warriorRequest=(vk==ww.shieldHotkey)?1:2;if(g_warriorEvent)SetEvent(g_warriorEvent);return 1;}}'''
if anchor not in fn: raise RuntimeError('warrior hotkey insertion anchor')
fn=fn.replace(anchor,war,1)
s=s[:a]+fn+s[b:]

a,b=span('LRESULT CALLBACK WndProc'); fn=s[a:b]
fn=fn.replace('case IDC_CATEGORY_MOB:ShowCategory(2);break;', 'case IDC_CATEGORY_MOB:ShowCategory(2);break;case IDC_CATEGORY_WARRIOR:ShowCategory(3);break;',1)
fn=fn.replace('case IDC_MOB_TAB_PRIEST:ShowMobSubCategory(1);break;', 'case IDC_MOB_TAB_PRIEST:ShowMobSubCategory(1);break;case IDC_WARRIOR_SHIELD_KEY:g_assignTarget=8;SetWindowTextW(g_ui.warriorShieldKey,L"Bir tuşa bas...");break;case IDC_WARRIOR_WEAPON_KEY:g_assignTarget=9;SetWindowTextW(g_ui.warriorWeaponKey,L"Bir tuşa bas...");break;case IDC_WARRIOR_ENABLE:ReadWarriorUi(true);RefreshStatus();break;case IDC_WARRIOR_SAVE:ReadWarriorUi(true);RefreshHotkeyLabels();RefreshStatus();MessageBoxW(h,L"Warrior ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;',1)
fn=fn.replace('case WM_APP_ASSIGN_DONE:SaveRogue();SaveAttack();SaveMob();RefreshHotkeyLabels();', 'case WM_APP_ASSIGN_DONE:SaveRogue();SaveAttack();SaveMob();SaveWarrior();RefreshHotkeyLabels();',1)
s=s[:a]+fn+s[b:]

a,b=span('int APIENTRY wWinMain'); fn=s[a:b]
fn=fn.replace('g_cureEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);LoadSettings();', 'g_cureEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);g_warriorEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);LoadSettings();',1)
old='std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker),tMobSkill(MobSkillWorker),tMobChase(MobChaseWorker),tMobScroll(MobScrollWorker),tMobPriest(MobPriestWorker);'
new=old[:-1]+',tWarrior(WarriorWorker);'
if old not in fn: raise RuntimeError('thread anchor')
fn=fn.replace(old,new,1)
oldj='if(tMobPriest.joinable())tMobPriest.join();'
fn=fn.replace(oldj,oldj+'if(tWarrior.joinable())tWarrior.join();',1)
fn=fn.replace('if(g_cureEvent){CloseHandle(g_cureEvent);g_cureEvent=nullptr;}if(g_font)', 'if(g_cureEvent){CloseHandle(g_cureEvent);g_cureEvent=nullptr;}if(g_warriorEvent){CloseHandle(g_warriorEvent);g_warriorEvent=nullptr;}if(g_font)',1)
s=s[:a]+fn+s[b:]

ns='\n} // namespace\n'
if s.count(ns)!=1: raise RuntimeError('namespace close')
test=r'''
bool RunWarriorModelTest(){
  int pass=0,total=0;std::ofstream f("warrior-model-test-report.txt",std::ios::trunc);auto t=[&](const char*n,bool ok){total++;if(ok)pass++;f<<n<<"="<<(ok?"PASS":"FAIL")<<"\n";};
  InventoryGrid g{};g.valid=true;g.left=100;g.top=200;g.cell=40;g.screenOrigin={10,20};POINT a=WarriorSlotCellCenter(g,1),b=WarriorSlotCellCenter(g,7),c=WarriorSlotCellCenter(g,8),d=WarriorSlotCellCenter(g,28);t("Slot1TopLeft",a.x==130&&a.y==240);t("Slot7TopRight",b.x==370&&b.y==240);t("Slot8SecondRow",c.x==130&&c.y==280);t("Slot28BottomRight",d.x==370&&d.y==360);
  {int W=1000,H=600,cell=38,left=710,top=315;std::vector<uint32_t> px((size_t)W*H,0x00202020u);auto line=[&](int x0,int y0,int x1,int y1){for(int y=std::max(0,y0);y<std::min(H,y1);y++)for(int x=std::max(0,x0);x<std::min(W,x1);x++)px[(size_t)y*W+x]=0x00E0C080u;};for(int j=0;j<8;j++)line(left+j*cell-1,top,left+j*cell+2,top+4*cell+1);for(int i=0;i<5;i++)line(left,top+i*cell-1,left+7*cell+1,top+i*cell+2);InventoryGrid q{};bool ok=DetectInventoryGridPixels(px,W,H,q);t("Synthetic7x4Detected",ok);t("SyntheticGridGeometry",ok&&std::abs(q.cell-cell)<=2&&std::abs(q.left-left)<=4&&std::abs(q.top-top)<=4);}
  t("RightClickDownFlag",MOUSEEVENTF_RIGHTDOWN==0x0008);t("RightClickUpFlag",MOUSEEVENTF_RIGHTUP==0x0010);
  f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";return total==8&&pass==8;
}
'''
s=s.replace(ns,'\n'+test+ns,1)
main='int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show){g_instance=hi;'
if s.count(main)!=1: raise RuntimeError('main dispatcher anchor')
s=s.replace(main,main+'if(cmd&&wcsstr(cmd,L"--warrior-model-test"))return RunWarriorModelTest()?0:11;',1)

a,b=span('bool WarriorRightClickSlot'); wb=s[a:b]
if 'MOUSEEVENTF_RIGHTDOWN' not in wb or 'MOUSEEVENTF_RIGHTUP' not in wb: raise RuntimeError('right click flags missing')
for bad in ('MOUSEEVENTF_LEFTDOWN','MOUSEEVENTF_LEFTUP','mouse_event('):
    if bad in wb: raise RuntimeError('forbidden warrior mouse action '+bad)
if "ReferenceTapKey('I')" not in s: raise RuntimeError('inventory I path missing')
if '7*cell' not in s or '4*cell' not in s: raise RuntimeError('7x4 detector missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('WARRIOR_RIGHTCLICK_PATCH=PASS')
