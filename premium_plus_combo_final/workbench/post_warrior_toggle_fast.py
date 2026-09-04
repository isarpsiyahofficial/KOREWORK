import pathlib,sys,re
p=pathlib.Path(sys.argv[1]); s=p.read_text(encoding='utf-8')

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

def once(old,new,label):
    global s
    c=s.count(old)
    if c!=1: raise RuntimeError(f'{label}: expected 1 got {c}')
    s=s.replace(old,new,1)

once('constexpr int IDC_WARRIOR_SAVE = 1806;', '''constexpr int IDC_WARRIOR_SAVE = 1806;\nconstexpr int IDC_WARRIOR_CAL = 1807;''', 'warrior cal id')
old_struct='''struct WarriorSettings {\n  bool enabled=false;\n  int shieldSlot=1;\n  int shieldHotkey='X';\n  int weaponSlot=2;\n  int weaponHotkey='C';\n};'''
new_struct='''struct WarriorSettings {\n  bool enabled=false;\n  int shieldSlot=1;\n  int shieldHotkey='X'; // shared equipment toggle hotkey\n  int weaponSlot=2;\n  int weaponHotkey='X'; // legacy mirror; kept for registry compatibility\n  NormalizedRect inventoryRect; // optional visual hint, never the sole detector\n};'''
once(old_struct,new_struct,'warrior settings struct')
once('HWND warriorEnable{},warriorShieldSlot{},warriorShieldKey{},warriorWeaponSlot{},warriorWeaponKey{},warriorSave{},warriorStatus{};', 'HWND warriorEnable{},warriorShieldSlot{},warriorShieldKey{},warriorWeaponSlot{},warriorWeaponKey{},warriorCal{},warriorSave{},warriorStatus{};', 'warrior ui fields')
once('std::atomic<int> g_warriorRequest{0}; // 1 shield, 2 weapon\nstd::atomic<ULONG_PTR> g_warriorRequestWindow{0};\nstd::atomic<int> g_warriorLastResult{0}; // 0 ready,1 click ok,2 grid fail,3 focus lost\nHANDLE g_warriorEvent{};', '''std::atomic<int> g_warriorRequest{0}; // shared equipment toggle request\nstd::atomic<ULONG_PTR> g_warriorRequestWindow{0};\nstd::atomic<int> g_warriorLastResult{0}; // 0 ready,1 click ok,2 grid fail,3 focus lost\nstd::atomic<int> g_warriorNextEquip{0}; // 0 shield, 1 weapon; flips only after successful click\nstd::atomic<bool> g_warriorBusy{false};\nstd::atomic<bool> g_warriorInventoryKnownOpen{false};\nInventoryGrid g_warriorCachedGrid{};\nstd::atomic<ULONG_PTR> g_warriorCachedWindow{0};\nHANDLE g_warriorEvent{};''','warrior runtime globals')

old_load='''  if(RegCreateKeyExW(HKEY_CURRENT_USER,kWarriorRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){\n    g_warrior.enabled=ReadDword(k,L"Enabled",0)!=0;\n    g_warrior.shieldSlot=ClampD(ReadDword(k,L"ShieldSlot",1),1,28);\n    g_warrior.shieldHotkey=ClampD(ReadDword(k,L"ShieldHotkey",'X'),1,255);\n    g_warrior.weaponSlot=ClampD(ReadDword(k,L"WeaponSlot",2),1,28);\n    g_warrior.weaponHotkey=ClampD(ReadDword(k,L"WeaponHotkey",'C'),1,255);\n    if(g_warrior.weaponHotkey==g_warrior.shieldHotkey)g_warrior.weaponHotkey='C';\n    RegCloseKey(k);\n  }'''
new_load='''  if(RegCreateKeyExW(HKEY_CURRENT_USER,kWarriorRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS){\n    g_warrior.enabled=ReadDword(k,L"Enabled",0)!=0;\n    g_warrior.shieldSlot=ClampD(ReadDword(k,L"ShieldSlot",1),1,28);\n    g_warrior.weaponSlot=ClampD(ReadDword(k,L"WeaponSlot",2),1,28);\n    int shared=ClampD(ReadDword(k,L"SharedHotkey",ReadDword(k,L"ShieldHotkey",'X')),1,255);\n    g_warrior.shieldHotkey=shared;g_warrior.weaponHotkey=shared;\n    g_warrior.inventoryRect={(int)ReadDword(k,L"InvX",0),(int)ReadDword(k,L"InvY",0),(int)ReadDword(k,L"InvW",0),(int)ReadDword(k,L"InvH",0)};\n    RegCloseKey(k);\n  }'''
once(old_load,new_load,'warrior load')
replace_fn('void SaveWarrior()', r'''void SaveWarrior(){HKEY k{};std::lock_guard<std::mutex>lk(g_settingsMutex);if(RegCreateKeyExW(HKEY_CURRENT_USER,kWarriorRegistry,0,nullptr,0,KEY_READ|KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return;WriteDword(k,L"Enabled",g_warrior.enabled?1:0);WriteDword(k,L"ShieldSlot",g_warrior.shieldSlot);WriteDword(k,L"WeaponSlot",g_warrior.weaponSlot);WriteDword(k,L"SharedHotkey",g_warrior.shieldHotkey);WriteDword(k,L"ShieldHotkey",g_warrior.shieldHotkey);WriteDword(k,L"WeaponHotkey",g_warrior.shieldHotkey);WriteDword(k,L"InvX",g_warrior.inventoryRect.x);WriteDword(k,L"InvY",g_warrior.inventoryRect.y);WriteDword(k,L"InvW",g_warrior.inventoryRect.w);WriteDword(k,L"InvH",g_warrior.inventoryRect.h);RegCloseKey(k);}''')

replace_fn('void ReadWarriorUi(bool persist=true)', r'''void ReadWarriorUi(bool persist=true){
  WarriorSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_warrior;}
  int oldShield=n.shieldSlot,oldWeapon=n.weaponSlot;
  if(g_ui.warriorEnable)n.enabled=SendMessageW(g_ui.warriorEnable,BM_GETCHECK,0,0)==BST_CHECKED;
  if(g_ui.warriorShieldSlot)n.shieldSlot=GetInt(g_ui.warriorShieldSlot,n.shieldSlot,1,28);
  if(g_ui.warriorWeaponSlot)n.weaponSlot=GetInt(g_ui.warriorWeaponSlot,n.weaponSlot,1,28);
  n.weaponHotkey=n.shieldHotkey;
  {std::lock_guard<std::mutex>lk(g_settingsMutex);g_warrior=n;}
  if(oldShield!=n.shieldSlot||oldWeapon!=n.weaponSlot)g_warriorNextEquip=0;
  if(persist)SaveWarrior();
}''')
replace_fn('void PopulateWarriorUi()', r'''void PopulateWarriorUi(){
  if(!g_ui.warriorEnable)return;WarriorSettings w;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;}
  SendMessageW(g_ui.warriorEnable,BM_SETCHECK,w.enabled?BST_CHECKED:BST_UNCHECKED,0);
  SetWindowTextW(g_ui.warriorShieldSlot,std::to_wstring(w.shieldSlot).c_str());
  SetWindowTextW(g_ui.warriorWeaponSlot,std::to_wstring(w.weaponSlot).c_str());
}''')
once('if(g_ui.warriorShieldKey)set(g_ui.warriorShieldKey,L"Kalkan",w.shieldHotkey);if(g_ui.warriorWeaponKey)set(g_ui.warriorWeaponKey,L"Silah",w.weaponHotkey);', 'if(g_ui.warriorShieldKey)set(g_ui.warriorShieldKey,L"Ekipman Değiştir",w.shieldHotkey);', 'shared hotkey label')
replace_fn('void RefreshStatus()', r'''void RefreshStatus(){RogueSettings r;AttackSettings a;WarriorSettings ww;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;ww=g_warrior;}std::wstring s=r.powerEnabled?L"GÜÇ AÇIK":L"GÜÇ KAPALI";s+=L"   |   Minor: "+std::wstring(g_minorActive.load()?L"ÇALIŞIYOR":L"Hazır");s+=L"   |   Attack: "+std::wstring(g_attackActive.load()?L"ÇALIŞIYOR":L"Hazır");s+=L"   |   Mob: "+std::wstring(g_mobActive.load()?L"ÇALIŞIYOR":L"Hazır");SetWindowTextW(g_ui.status,s.c_str());if(g_ui.warriorStatus){int wr=g_warriorLastResult.load();int next=g_warriorNextEquip.load()&1;std::wstring t;if(wr==2)t=L"Envanter grid bulunamadı";else if(wr==3)t=L"Oyun odağı kayboldu";else{t=wr==1?L"Son işlem OK  •  ":L"Hazır  •  ";t+=next?L"Sıradaki: SİLAH slot ":L"Sıradaki: KALKAN slot ";t+=std::to_wstring(next?ww.weaponSlot:ww.shieldSlot);t+=ww.inventoryRect.valid()?L"  •  Kalibrasyon destekli":L"  •  Otomatik grid";}SetWindowTextW(g_ui.warriorStatus,t.c_str());}SetWindowTextW(g_ui.power,r.powerEnabled?L"POWER  AÇIK":L"POWER  KAPALI");if(g_ui.hpPercent){int pct=g_hpPercent.load(),cur=g_hpCurrent.load(),mx=g_hpMax.load();std::wstring t=pct<0?L"HP: kalibrasyon yok":(cur>=0&&mx>0?L"HP "+std::to_wstring(cur)+L"/"+std::to_wstring(mx)+L" %"+std::to_wstring(pct):L"HP %"+std::to_wstring(pct));SetWindowTextW(g_ui.hpPercent,t.c_str());}if(g_ui.mpPercent){int pct=g_mpPercent.load(),cur=g_mpCurrent.load(),mx=g_mpMax.load();std::wstring t=pct<0?L"MP: kalibrasyon yok":(cur>=0&&mx>0?L"MP "+std::to_wstring(cur)+L"/"+std::to_wstring(mx)+L" %"+std::to_wstring(pct):L"MP %"+std::to_wstring(pct));SetWindowTextW(g_ui.mpPercent,t.c_str());}if(g_ui.autoMinorHp){int pct=g_hpPercent.load();std::wstring t=pct<0?L"HP: kalibre et":L"HP %"+std::to_wstring(pct);if(g_autoMinorOwned.load())t+=L"  •  AUTO MINOR";SetWindowTextW(g_ui.autoMinorHp,t.c_str());}}''')

replace_fn('LRESULT CALLBACK OverlayProc', r'''LRESULT CALLBACK OverlayProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_LBUTTONDOWN:g_calStart={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calStart);g_calCurrent=g_calStart;g_dragging=true;SetCapture(h);InvalidateRect(h,nullptr,TRUE);return 0;case WM_MOUSEMOVE:if(g_dragging){g_calCurrent={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calCurrent);InvalidateRect(h,nullptr,TRUE);}return 0;case WM_LBUTTONUP:if(g_dragging){g_calCurrent={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&g_calCurrent);ReleaseCapture();g_dragging=false;RECT r{std::min(g_calStart.x,g_calCurrent.x),std::min(g_calStart.y,g_calCurrent.y),std::max(g_calStart.x,g_calCurrent.x),std::max(g_calStart.y,g_calCurrent.y)};int target=g_calTarget.load();bool warriorCal=false;if(r.right-r.left>=10&&r.bottom-r.top>=3){auto nr=NormalizeScreenRect(r);if(target==3){if(r.right-r.left>=100&&r.bottom-r.top>=60){{std::lock_guard<std::mutex>lk(g_settingsMutex);g_warrior.inventoryRect=nr;}warriorCal=true;g_warriorCachedGrid={};g_warriorCachedWindow=0;}}else{std::lock_guard<std::mutex>lk(g_settingsMutex);if(target==1)g_attack.hpRect=nr;else if(target==2)g_attack.mpRect=nr;}if(target==1||target==2){BarReading br=CaptureBarReading(nr,target==1);if(target==1){g_hpPercent=br.percent;g_hpCurrent=br.current;g_hpMax=br.maximum;}else{g_mpPercent=br.percent;g_mpCurrent=br.current;g_mpMax=br.maximum;}}}g_calTarget=0;DestroyWindow(h);g_overlay=nullptr;if(warriorCal)SaveWarrior();else if(target==1||target==2)SaveAttack();PostMessageW(g_ui.main,WM_APP_CAL_DONE,0,0);}return 0;case WM_KEYDOWN:if(w==VK_ESCAPE){g_calTarget=0;DestroyWindow(h);g_overlay=nullptr;}return 0;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);HBRUSH b=CreateSolidBrush(RGB(20,20,20));FillRect(dc,&c,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,230,150));SelectObject(dc,g_fontBold);const wchar_t* msg=g_calTarget.load()==3?L"28 ENVANTER SLOTUNU (7×4) ÇERÇEVELEYİN  •  BU SADECE DESTEK OLARAK KULLANILIR  •  ESC: İPTAL":L"HP/MP BAR ALANINI MOUSE İLE ÇERÇEVELEYİN  •  ESC: İPTAL";DrawTextW(dc,msg,-1,&c,DT_CENTER|DT_TOP|DT_SINGLELINE);if(g_dragging){POINT a=g_calStart,bp=g_calCurrent;ScreenToClient(h,&a);ScreenToClient(h,&bp);RECT r{std::min(a.x,bp.x),std::min(a.y,bp.y),std::max(a.x,bp.x),std::max(a.y,bp.y)};HPEN p=CreatePen(PS_SOLID,2,RGB(255,210,70));HGDIOBJ o=SelectObject(dc,p);SelectObject(dc,GetStockObject(HOLLOW_BRUSH));Rectangle(dc,r.left,r.top,r.right,r.bottom);SelectObject(dc,o);DeleteObject(p);}EndPaint(h,&ps);return 0;}case WM_DESTROY:g_overlay=nullptr;return 0;}return DefWindowProcW(h,m,w,l);}''')

once('bool FindInventoryGrid(HWND game,InventoryGrid&grid){', r'''double WarriorGridEdgeScore(const std::vector<uint32_t>&px,int W,int H,const InventoryGrid&g){
  if(!g.valid||g.cell<16||g.left<4||g.top<4||g.left+7*g.cell+4>=W||g.top+4*g.cell+4>=H)return 0.0;
  auto gray=[&](int x,int y)->int{return (int)WarriorGray(px[(size_t)y*W+x]);};
  double sum=0;int n=0;int step=std::max(3,g.cell/7);
  for(int j=0;j<8;j++){int bx=g.left+j*g.cell;for(int y=g.top+3;y<g.top+4*g.cell-3;y+=step){int best=0;for(int o=-2;o<=2;o++){int x=bx+o;if(x<4||x+4>=W)continue;int v=std::abs(gray(x,y)-(gray(x-3,y)+gray(x+3,y))/2);best=std::max(best,v);}sum+=best;n++;}}
  for(int i=0;i<5;i++){int by=g.top+i*g.cell;for(int x=g.left+3;x<g.left+7*g.cell-3;x+=step){int best=0;for(int o=-2;o<=2;o++){int y=by+o;if(y<4||y+4>=H)continue;int v=std::abs(gray(x,y)-(gray(x,y-3)+gray(x,y+3))/2);best=std::max(best,v);}sum+=best;n++;}}
  return n?sum/n:0.0;
}
bool WarriorGridFromCalibrationPixels(const std::vector<uint32_t>&px,int W,int H,POINT origin,const NormalizedRect&nr,InventoryGrid&out){
  out={};if(!nr.valid()||W<200||H<120)return false;int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);if(vw<2||vh<2)return false;
  int l=vx+(int)((long long)nr.x*vw/1000000)-origin.x,t=vy+(int)((long long)nr.y*vh/1000000)-origin.y;int rw=(int)((long long)nr.w*vw/1000000),rh=(int)((long long)nr.h*vh/1000000);int r=l+rw,b=t+rh;l=std::clamp(l,0,W-1);t=std::clamp(t,0,H-1);r=std::clamp(r,l+1,W);b=std::clamp(b,t+1,H);rw=r-l;rh=b-t;if(rw<120||rh<70)return false;
  int cx=std::max(1,rw/7),cy=std::max(1,rh/4),base=(cx+cy)/2;if(std::abs(cx-cy)>std::max(8,base/3))return false;double best=0;InventoryGrid bg{};int delta=std::max(5,base/4);
  for(int cell=std::max(16,base-5);cell<=base+5;cell++)for(int yy=t-delta;yy<=t+delta;yy+=2)for(int xx=l-delta;xx<=l+delta;xx+=2){InventoryGrid q{};q.valid=true;q.left=xx;q.top=yy;q.cell=cell;q.clientW=W;q.clientH=H;double sc=WarriorGridEdgeScore(px,W,H,q);if(sc>best){best=sc;bg=q;}}
  if(best<9.0)return false;bg.score=best;bg.screenOrigin=origin;out=bg;return true;
}
bool WarriorValidateCachedPixels(const std::vector<uint32_t>&px,int W,int H,POINT origin,const InventoryGrid&cached,InventoryGrid&out){InventoryGrid q=cached;q.clientW=W;q.clientH=H;q.screenOrigin=origin;if(q.left+7*q.cell>=W||q.top+4*q.cell>=H)return false;double sc=WarriorGridEdgeScore(px,W,H,q);if(sc<8.0)return false;q.score=sc;out=q;return true;}
bool WarriorResolveVisibleInventory(HWND game,InventoryGrid&grid){
  std::vector<uint32_t>px;int W=0,H=0;POINT origin{};if(!CaptureGameClient(game,px,W,H,origin))return false;
  if(g_warriorCachedWindow.load()==(ULONG_PTR)game&&g_warriorCachedGrid.valid&&WarriorValidateCachedPixels(px,W,H,origin,g_warriorCachedGrid,grid))return true;
  WarriorSettings w;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;}
  if(w.inventoryRect.valid()&&WarriorGridFromCalibrationPixels(px,W,H,origin,w.inventoryRect,grid)){g_warriorCachedGrid=grid;g_warriorCachedWindow=(ULONG_PTR)game;return true;}
  InventoryGrid full{};if(DetectInventoryGridPixels(px,W,H,full)){full.screenOrigin=origin;grid=full;g_warriorCachedGrid=grid;g_warriorCachedWindow=(ULONG_PTR)game;return true;}return false;
}
bool FindInventoryGrid(HWND game,InventoryGrid&grid){''','fast helpers insertion')
replace_fn('bool WarriorResolveInventory(HWND game,InventoryGrid&grid)', r'''bool WarriorResolveInventory(HWND game,InventoryGrid&grid){
  if(!game||GetForegroundWindow()!=game)return false;
  int pre=g_warriorInventoryKnownOpen.load()?4:2;
  for(int i=0;i<pre;i++){if(WarriorResolveVisibleInventory(game,grid)){g_warriorInventoryKnownOpen=true;return true;}if(i+1<pre)Sleep(4);}
  if(GetForegroundWindow()!=game)return false;
  ReferenceTapKey('I');g_warriorInventoryKnownOpen=false;
  ULONGLONG deadline=GetTickCount64()+240;
  while(g_running&&GetTickCount64()<deadline){Sleep(4);if(GetForegroundWindow()!=game)return false;if(WarriorResolveVisibleInventory(game,grid)){g_warriorInventoryKnownOpen=true;return true;}}
  return false;
}''')
replace_fn('bool WarriorRightClickSlot(HWND game,int slot)', r'''bool WarriorRightClickSlot(HWND game,int slot){
  if(!game||GetForegroundWindow()!=game)return false;
  slot=std::clamp(slot,1,28);
  InventoryGrid grid{};if(!WarriorResolveInventory(game,grid))return false;
  if(GetForegroundWindow()!=game)return false;
  POINT pt=WarriorSlotCellCenter(grid,slot);
  const int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);if(vw<2||vh<2)return false;
  INPUT mv{},d{},u{};mv.type=INPUT_MOUSE;mv.mi.dx=(LONG)std::clamp<long long>(((long long)(pt.x-vx)*65535)/(vw-1),0,65535);mv.mi.dy=(LONG)std::clamp<long long>(((long long)(pt.y-vy)*65535)/(vh-1),0,65535);mv.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;mv.mi.dwExtraInfo=kMagicInput;d.type=INPUT_MOUSE;d.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;d.mi.dwExtraInfo=kMagicInput;u=d;u.mi.dwFlags=MOUSEEVENTF_RIGHTUP;
  FifoTicketGuard gate(g_gameInputGate);if(SendInput(1,&mv,sizeof(INPUT))!=1)return false;PreciseDelayUs(700);if(SendInput(1,&d,sizeof(INPUT))!=1)return false;PreciseDelayUs(3200);if(SendInput(1,&u,sizeof(INPUT))!=1)return false;return true;
}''')
once('void WarriorWorker(){', 'int WarriorToggleSlot(const WarriorSettings&w,int phase){return (phase&1)?w.weaponSlot:w.shieldSlot;}\nvoid WarriorWorker(){', 'toggle helper')
replace_fn('void WarriorWorker()', r'''void WarriorWorker(){while(g_running){if(!g_warriorEvent){Sleep(10);continue;}DWORD wr=WaitForSingleObject(g_warriorEvent,50);if(wr!=WAIT_OBJECT_0)continue;int req=g_warriorRequest.exchange(0);HWND game=(HWND)g_warriorRequestWindow.exchange(0);if(!req)continue;if(g_warriorBusy.exchange(true))continue;WarriorSettings w;RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;r=g_rogue;}if(!w.enabled||!r.powerEnabled){g_warriorBusy=false;continue;}if(!game||GetForegroundWindow()!=game){g_warriorLastResult=3;g_warriorBusy=false;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);continue;}int phase=g_warriorNextEquip.load()&1;int slot=WarriorToggleSlot(w,phase);bool ok=WarriorRightClickSlot(game,slot);if(ok)g_warriorNextEquip=phase^1;g_warriorLastResult=ok?1:(GetForegroundWindow()==game?2:3);g_warriorBusy=false;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);}}''')
replace_fn('void CreateWarriorPage()', r'''void CreateWarriorPage(){auto&p=g_ui.warriorPage;PageAdd(p,Label(L"WARRIOR",kContentX,82,160,22,g_fontBold));g_ui.warriorEnable=Ctrl(L"BUTTON",L"WARRIOR AKTİF",BS_AUTOCHECKBOX,kContentX+460,82,120,22,IDC_WARRIOR_ENABLE,g_fontSmall);PageAdd(p,g_ui.warriorEnable);PageAdd(p,Label(L"Kalkan / silah tek tuş değişim",kContentX,114,270,20,g_fontBold));PageAdd(p,Label(L"28 slot otomatik 7×4 grid algılanır. Kalibrasyon yalnız ek doğrulama/hız desteğidir; başarısız olursa tam otomatik tarama devam eder.",kContentX,140,570,38,g_fontSmall));
  PageAdd(p,Label(L"KALKAN",kContentX,194,70,20,g_fontBold));PageAdd(p,Label(L"Slot (1-28)",kContentX+88,194,70,20,g_fontSmall));g_ui.warriorShieldSlot=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+162,192,46,22,IDC_WARRIOR_SHIELD_SLOT,g_fontSmall);PageAdd(p,g_ui.warriorShieldSlot);
  PageAdd(p,Label(L"SİLAH",kContentX,232,70,20,g_fontBold));PageAdd(p,Label(L"Slot (1-28)",kContentX+88,232,70,20,g_fontSmall));g_ui.warriorWeaponSlot=Ctrl(L"EDIT",L"2",WS_BORDER|ES_CENTER,kContentX+162,230,46,22,IDC_WARRIOR_WEAPON_SLOT,g_fontSmall);PageAdd(p,g_ui.warriorWeaponSlot);
  PageAdd(p,Label(L"ORTAK TUŞ",kContentX,274,78,20,g_fontBold));g_ui.warriorShieldKey=Ctrl(L"BUTTON",L"Ekipman Değiştir: X",BS_PUSHBUTTON,kContentX+88,270,190,28,IDC_WARRIOR_SHIELD_KEY,g_fontSmall);PageAdd(p,g_ui.warriorShieldKey);g_ui.warriorCal=Ctrl(L"BUTTON",L"Envanter Alanını Kalibre Et",BS_PUSHBUTTON,kContentX+290,270,190,28,IDC_WARRIOR_CAL,g_fontSmall);PageAdd(p,g_ui.warriorCal);
  PageAdd(p,Label(L"Her başarılı basışta Kalkan ↔ Silah slotu sırayla değişir. Slot kutusuna yazılan değer Kaydet'e basmadan anında kullanılır.",kContentX,314,570,34,g_fontSmall));g_ui.warriorStatus=Label(L"Hazır",kContentX,356,550,22,g_fontSmall);PageAdd(p,g_ui.warriorStatus);g_ui.warriorSave=Ctrl(L"BUTTON",L"WARRIOR AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,392,238,28,IDC_WARRIOR_SAVE,g_fontBold);PageAdd(p,g_ui.warriorSave);PopulateWarriorUi();}''')

a,b=span('LRESULT CALLBACK KeyboardProc'); fn=s[a:b]
old_assign='''int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else{RogueSettings cr;AttackSettings ca;MobSettings cm;WarriorSettings cw;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;cm=g_mob;cw=g_warrior;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey&&vk!=cm.startHotkey&&vk!=cm.stopHotkey;if(t==8)allowed=allowed&&vk!=cw.weaponHotkey;if(t==9)allowed=allowed&&vk!=cw.shieldHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;else if(t==8)g_warrior.shieldHotkey=vk;else if(t==9)g_warrior.weaponHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}'''
new_assign='''int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else{RogueSettings cr;AttackSettings ca;MobSettings cm;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;cm=g_mob;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey&&vk!=cm.startHotkey&&vk!=cm.stopHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;else if(t==8){g_warrior.shieldHotkey=vk;g_warrior.weaponHotkey=vk;}g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}'''
if old_assign not in fn: raise RuntimeError('keyboard assign block missing')
fn=fn.replace(old_assign,new_assign,1)
old_runtime='''if(ww.enabled&&(vk==ww.shieldHotkey||vk==ww.weaponHotkey)){HWND fg=GetForegroundWindow();if(fg&&!IsOurUiWindow(fg)){RememberGameWindow();g_warriorRequestWindow=(ULONG_PTR)fg;g_warriorRequest=(vk==ww.shieldHotkey)?1:2;if(g_warriorEvent)SetEvent(g_warriorEvent);return 1;}}'''
new_runtime='''if(ww.enabled&&vk==ww.shieldHotkey){HWND fg=GetForegroundWindow();if(fg&&!IsOurUiWindow(fg)){RememberGameWindow();if(!g_warriorBusy.load()){g_warriorRequestWindow=(ULONG_PTR)fg;g_warriorRequest=1;if(g_warriorEvent)SetEvent(g_warriorEvent);}return 1;}}'''
if old_runtime not in fn: raise RuntimeError('warrior runtime hotkey block missing')
fn=fn.replace(old_runtime,new_runtime,1)
s=s[:a]+fn+s[b:]

a,b=span('LRESULT CALLBACK WndProc'); fn=s[a:b]
anchor='case WM_COMMAND:{int id=LOWORD(w);'
if anchor not in fn: raise RuntimeError('wm command anchor missing')
fn=fn.replace(anchor,anchor+'if((id==IDC_WARRIOR_SHIELD_SLOT||id==IDC_WARRIOR_WEAPON_SLOT)&&HIWORD(w)==EN_CHANGE){ReadWarriorUi(false);}',1)
old_cases='''case IDC_WARRIOR_SHIELD_KEY:g_assignTarget=8;SetWindowTextW(g_ui.warriorShieldKey,L"Bir tuşa bas...");break;case IDC_WARRIOR_WEAPON_KEY:g_assignTarget=9;SetWindowTextW(g_ui.warriorWeaponKey,L"Bir tuşa bas...");break;case IDC_WARRIOR_ENABLE:ReadWarriorUi(true);RefreshStatus();break;case IDC_WARRIOR_SAVE:ReadWarriorUi(true);RefreshHotkeyLabels();RefreshStatus();MessageBoxW(h,L"Warrior ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;'''
new_cases='''case IDC_WARRIOR_SHIELD_KEY:g_assignTarget=8;SetWindowTextW(g_ui.warriorShieldKey,L"Bir tuşa bas...");break;case IDC_WARRIOR_CAL:ReadWarriorUi(true);BeginCalibration(3);break;case IDC_WARRIOR_ENABLE:ReadWarriorUi(true);RefreshStatus();break;case IDC_WARRIOR_SAVE:ReadWarriorUi(true);RefreshHotkeyLabels();RefreshStatus();MessageBoxW(h,L"Warrior ayarları kaydedildi.",L"Premium Plus Combo",MB_OK);break;'''
if old_cases not in fn: raise RuntimeError('warrior command cases missing')
fn=fn.replace(old_cases,new_cases,1)
s=s[:a]+fn+s[b:]

replace_fn('bool RunWarriorModelTest()', r'''bool RunWarriorModelTest(){
  int pass=0,total=0;std::ofstream f("warrior-model-test-report.txt",std::ios::trunc);auto t=[&](const char*n,bool ok){total++;if(ok)pass++;f<<n<<"="<<(ok?"PASS":"FAIL")<<"\n";};
  InventoryGrid g{};g.valid=true;g.left=100;g.top=200;g.cell=40;g.screenOrigin={10,20};POINT a=WarriorSlotCellCenter(g,1),s4=WarriorSlotCellCenter(g,4),b=WarriorSlotCellCenter(g,7),c=WarriorSlotCellCenter(g,8),d=WarriorSlotCellCenter(g,28);t("Slot1TopLeft",a.x==130&&a.y==240);t("Slot4FourthColumn",s4.x==250&&s4.y==240);t("Slot7TopRight",b.x==370&&b.y==240);t("Slot8SecondRow",c.x==130&&c.y==280);t("Slot28BottomRight",d.x==370&&d.y==360);
  {int W=1000,H=600,cell=38,left=710,top=315;std::vector<uint32_t> px((size_t)W*H,0x00202020u);auto line=[&](int x0,int y0,int x1,int y1){for(int y=std::max(0,y0);y<std::min(H,y1);y++)for(int x=std::max(0,x0);x<std::min(W,x1);x++)px[(size_t)y*W+x]=0x00E0C080u;};for(int j=0;j<8;j++)line(left+j*cell-1,top,left+j*cell+2,top+4*cell+1);for(int i=0;i<5;i++)line(left,top+i*cell-1,left+7*cell+1,top+i*cell+2);InventoryGrid q{};bool ok=DetectInventoryGridPixels(px,W,H,q);t("Synthetic7x4Detected",ok);t("SyntheticGridGeometry",ok&&std::abs(q.cell-cell)<=2&&std::abs(q.left-left)<=4&&std::abs(q.top-top)<=4);InventoryGrid exact{};exact.valid=true;exact.left=left;exact.top=top;exact.cell=cell;t("GridEdgeValidation",WarriorGridEdgeScore(px,W,H,exact)>9.0);}
  WarriorSettings w{};w.shieldSlot=4;w.weaponSlot=11;w.shieldHotkey='X';w.weaponHotkey='X';t("ToggleStartsAtShieldSlot4",WarriorToggleSlot(w,0)==4);t("ToggleThenWeaponSlot11",WarriorToggleSlot(w,1)==11);t("OneSharedHotkey",w.shieldHotkey==w.weaponHotkey);
  t("RightClickDownFlag",MOUSEEVENTF_RIGHTDOWN==0x0008);t("RightClickUpFlag",MOUSEEVENTF_RIGHTUP==0x0010);
  f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";return total==13&&pass==13;
}''')

if 'Premium Plus Combo | v4.8.19' in s:s=s.replace('Premium Plus Combo | v4.8.19','Premium Plus Combo | v4.8.20')
elif 'Premium Plus Combo | v4.8.17' in s:s=s.replace('Premium Plus Combo | v4.8.17','Premium Plus Combo | v4.8.20')
else: raise RuntimeError('Warrior version title anchor missing')
for required in ['IDC_WARRIOR_CAL','Slot4FourthColumn','OneSharedHotkey','WarriorResolveVisibleInventory','BeginCalibration(3)','Ekipman Değiştir']:
    if required not in s: raise RuntimeError('required Warrior v4.8.20 marker missing: '+required)
wa,wb=span('bool WarriorRightClickSlot(HWND game,int slot)'); body=s[wa:wb]
for bad in ('MOUSEEVENTF_LEFTDOWN','MOUSEEVENTF_LEFTUP','mouse_event(','GetCursorPos','SetCursorPos'):
    if bad in body: raise RuntimeError('forbidden Warrior action/API remains: '+bad)
if 'case IDC_WARRIOR_WEAPON_KEY:' in s: raise RuntimeError('second Warrior hotkey command still active')
p.write_text(s,encoding='utf-8',newline='\n')
print('WARRIOR_TOGGLE_FAST_PATCH=PASS')
