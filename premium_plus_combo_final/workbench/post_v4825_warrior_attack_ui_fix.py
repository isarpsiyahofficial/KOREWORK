import pathlib,re,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
orig=s

def one(old,new,label):
    global s
    n=s.count(old)
    if n!=1:
        raise SystemExit(f'{label} count={n}')
    s=s.replace(old,new,1)

one('constexpr int IDC_ATTACK_EXTRA_REMOVE = 1707;','constexpr int IDC_ATTACK_EXTRA_REMOVE = 1707;\nconstexpr int kMaxAttackExtraUi = 5;\nconstexpr int IDC_ATTACK_EXTRA_ROW_BASE = 2200; // enabled/bar/slot/ms per dynamic Attack skill row','attack constants')

one('HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackCategoryEnable{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},rAttack{},wDelay{},sDelay{},attackRandom{},attackExtraBar{},attackExtraSlot{},attackExtraMs{},attackExtraAdd{},attackExtraList{},attackExtraRemove{};',
    'HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackCategoryEnable{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},rAttack{},wDelay{},sDelay{},attackRandom{},attackExtraBar{},attackExtraSlot{},attackExtraMs{},attackExtraAdd{},attackExtraList{},attackExtraRemove{};\n  std::array<HWND,kMaxAttackExtraUi> attackExtraCheck{},attackExtraRowBar{},attackExtraRowSlot{},attackExtraRowMs{},attackExtraRowLabel{};',
    'attack ui arrays')

one('int n=ClampD(ReadDword(k,L"ExtraSkillCount",0),0,24);g_attack.extraSkills.clear();',
    'int n=ClampD(ReadDword(k,L"ExtraSkillCount",0),0,kMaxAttackExtraUi);g_attack.extraSkills.clear();',
    'load extra clamp')

s=s.replace('WriteDword(k,L"ExtraSkillCount",(DWORD)std::min<size_t>(24,g_attack.extraSkills.size()));','WriteDword(k,L"ExtraSkillCount",(DWORD)std::min<size_t>(kMaxAttackExtraUi,g_attack.extraSkills.size()));',1)
s=s.replace('for(size_t i=0;i<g_attack.extraSkills.size()&&i<24;i++){','for(size_t i=0;i<g_attack.extraSkills.size()&&i<kMaxAttackExtraUi;i++){',1)

start=s.index('void RefreshAttackExtraList(){')
end=s.index('void RefreshMobLists(){',start)
new='''void RefreshAttackExtraRows(){
  AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);a=g_attack;}
  const bool visible=g_currentCategory.load()==3;
  for(int i=0;i<kMaxAttackExtraUi;i++){
    const bool exists=i<(int)a.extraSkills.size();
    HWND hs[]={g_ui.attackExtraRowLabel[i],g_ui.attackExtraCheck[i],g_ui.attackExtraRowBar[i],g_ui.attackExtraRowSlot[i],g_ui.attackExtraRowMs[i]};
    for(HWND h:hs)if(h)ShowWindow(h,visible&&exists?SW_SHOW:SW_HIDE);
    if(exists){const auto&e=a.extraSkills[i];SetWindowTextW(g_ui.attackExtraRowLabel[i],(L"Skill "+std::to_wstring(i+5)).c_str());SendMessageW(g_ui.attackExtraCheck[i],BM_SETCHECK,e.enabled?BST_CHECKED:BST_UNCHECKED,0);SetCombo(g_ui.attackExtraRowBar[i],e.bar,1);SetCombo(g_ui.attackExtraRowSlot[i],e.slot,1);SetWindowTextW(g_ui.attackExtraRowMs[i],std::to_wstring(e.delayMs).c_str());}
  }
  if(g_ui.attackExtraRemove)EnableWindow(g_ui.attackExtraRemove,!a.extraSkills.empty());
  if(g_ui.attackExtraAdd)EnableWindow(g_ui.attackExtraAdd,a.extraSkills.size()<kMaxAttackExtraUi);
}
'''
s=s[:start]+new+s[end:]

one('void AddAttackExtra(){SkillEntry e;e.bar=ComboVal(g_ui.attackExtraBar,1,12,1);e.slot=ComboVal(g_ui.attackExtraSlot,1,10,1);e.delayMs=GetInt(g_ui.attackExtraMs,1,1,1000);{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_attack.extraSkills.size()<24)g_attack.extraSkills.push_back(e);}SaveAttack();RefreshAttackExtraList();}\nvoid RemoveAttackExtra(){int i=(int)SendMessageW(g_ui.attackExtraList,LB_GETCURSEL,0,0);if(i<0)return;{std::lock_guard<std::mutex>lk(g_settingsMutex);if(i<(int)g_attack.extraSkills.size())g_attack.extraSkills.erase(g_attack.extraSkills.begin()+i);}SaveAttack();RefreshAttackExtraList();}',
'''void AddAttackExtra(){{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_attack.extraSkills.size()>=kMaxAttackExtraUi)return;g_attack.extraSkills.push_back({true,1,1,1});}SaveAttack();RefreshAttackExtraRows();}
void RemoveAttackExtra(){{std::lock_guard<std::mutex>lk(g_settingsMutex);if(g_attack.extraSkills.empty())return;g_attack.extraSkills.pop_back();}SaveAttack();RefreshAttackExtraRows();}''',
'attack add remove')

needle='for(int i=0;i<4;i++){n.skillEnabled[i]=SendMessageW(g_ui.skillCheck[i],BM_GETCHECK,0,0)==BST_CHECKED;n.attackBars[i]=ComboVal(g_ui.skillBar[i],1,12,n.attackBars[i]);n.slots[i]=ComboVal(g_ui.skillSlot[i],1,10,n.slots[i]);n.skillDelayMs[i]=GetInt(g_ui.skillDelay[i],n.skillDelayMs[i],1,1000);}n.zCombo='
repl='for(int i=0;i<4;i++){n.skillEnabled[i]=SendMessageW(g_ui.skillCheck[i],BM_GETCHECK,0,0)==BST_CHECKED;n.attackBars[i]=ComboVal(g_ui.skillBar[i],1,12,n.attackBars[i]);n.slots[i]=ComboVal(g_ui.skillSlot[i],1,10,n.slots[i]);n.skillDelayMs[i]=GetInt(g_ui.skillDelay[i],n.skillDelayMs[i],1,1000);}for(int i=0;i<(int)n.extraSkills.size()&&i<kMaxAttackExtraUi;i++){if(g_ui.attackExtraCheck[i])n.extraSkills[i].enabled=SendMessageW(g_ui.attackExtraCheck[i],BM_GETCHECK,0,0)==BST_CHECKED;if(g_ui.attackExtraRowBar[i])n.extraSkills[i].bar=ComboVal(g_ui.attackExtraRowBar[i],1,12,n.extraSkills[i].bar);if(g_ui.attackExtraRowSlot[i])n.extraSkills[i].slot=ComboVal(g_ui.attackExtraRowSlot[i],1,10,n.extraSkills[i].slot);if(g_ui.attackExtraRowMs[i])n.extraSkills[i].delayMs=GetInt(g_ui.attackExtraRowMs[i],n.extraSkills[i].delayMs,1,1000);}n.zCombo='
one(needle,repl,'read attack extras')

s=s.replace('RefreshAttackExtraList();PopulateMobUi();','RefreshAttackExtraRows();PopulateMobUi();',1)

old='''  int i=NextEnabledSkill(a);if(i<0){g_attackExclusive=false;return;}const int wantedBar=a.attackBars[i];LONGLONG skillAt=0;
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
'''
new='''  SkillEntry skill;if(!NextAttackSkill(a,skill)){g_attackExclusive=false;return;}const int wantedBar=skill.bar;LONGLONG skillAt=0;
  {FifoTicketGuard sequence(g_gameInputGate);if(!g_cureExclusive&&!g_chatMode){
    int knownBar=g_attackKnownBar.load(std::memory_order_relaxed);
    if(AttackNeedsBarTap(knownBar,wantedBar)){DirectTimedTapUnlocked(BarToVk(wantedBar),12000,1000);g_attackKnownBar=wantedBar;PreciseDelayUs(30000);}
    skillAt=AttackQpcNow();ReferenceTapKeyUnlocked(SlotToVk(skill.slot));
    if(a.rAttack){PreciseDelayUs(1500);ReferenceTapKeyUnlocked('R');}
    if(wantedBar!=a.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}
  }}
  if(!skillAt||g_cureExclusive||g_chatMode){g_attackExclusive=false;return;}
  MaybeSendWsCombo(a,skillAt);
  g_attackExclusive=false;
  if(g_running&&g_attackActive&&!g_cureExclusive&&!g_chatMode)InterruptibleAttackDelayFrom(skillAt,skill.delayMs);
'''
one(old,new,'execute attack')

start=s.index('bool WarriorRightClickSlot(HWND game,int slot){')
end=s.index('int WarriorToggleSlot(',start)
new='''double WarriorVisualChangeRatio(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b){if(a.empty()||a.size()!=b.size())return 1.0;size_t changed=0;for(size_t i=0;i<a.size();i++){uint32_t x=a[i],y=b[i];int db=std::abs((int)(x&255)-(int)(y&255)),dg=std::abs((int)((x>>8)&255)-(int)((y>>8)&255)),dr=std::abs((int)((x>>16)&255)-(int)((y>>16)&255));if(std::max({dr,dg,db})>=28)changed++;}return (double)changed/(double)a.size();}
bool WarriorCaptureSlotVisual(const InventoryGrid&g,int slot,std::vector<uint32_t>&px){POINT c=WarriorSlotCellCenter(g,slot);int half=std::max(6,g.cell/3);RECT r{c.x-half,c.y-half,c.x+half,c.y+half};int W=0,H=0;return CaptureScreenRectPixels(r,px,W,H)&&W>4&&H>4;}
bool WarriorWaitSlotChanged(const InventoryGrid&g,int slot,const std::vector<uint32_t>&before,int timeoutMs){ULONGLONG deadline=GetTickCount64()+timeoutMs;while(g_running&&GetTickCount64()<deadline){Sleep(8);std::vector<uint32_t>after;if(WarriorCaptureSlotVisual(g,slot,after)&&WarriorVisualChangeRatio(before,after)>=0.06)return true;}return false;}
bool WarriorRightClickSlot(HWND game,int slot){
  if(!game||GetForegroundWindow()!=game)return false;LARGE_INTEGER fq{},t0{},t1{};QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&t0);
  slot=std::clamp(slot,1,28);InventoryGrid grid{};if(!WarriorResolveInventory(game,grid)||GetForegroundWindow()!=game)return false;
  POINT pt=WarriorSlotCellCenter(grid,slot);const int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);if(vw<2||vh<2)return false;
  INPUT mv{},d{},u{};mv.type=INPUT_MOUSE;mv.mi.dx=(LONG)std::clamp<long long>(((long long)(pt.x-vx)*65535)/(vw-1),0,65535);mv.mi.dy=(LONG)std::clamp<long long>(((long long)(pt.y-vy)*65535)/(vh-1),0,65535);mv.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;mv.mi.dwExtraInfo=kMagicInput;d.type=INPUT_MOUSE;d.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;d.mi.dwExtraInfo=kMagicInput;u=d;u.mi.dwFlags=MOUSEEVENTF_RIGHTUP;
  bool changed=false;
  {FifoTicketGuard gate(g_gameInputGate);if(SendInput(1,&mv,sizeof(INPUT))!=1)return false;PreciseDelayUs(12000);}
  std::vector<uint32_t>before;if(!WarriorCaptureSlotVisual(grid,slot,before)){FifoTicketGuard gate(g_gameInputGate);if(GetForegroundWindow()==game)ReferenceTapKeyUnlocked('I');g_warriorInventoryKnownOpen=false;return false;}
  for(int attempt=0;attempt<3&&!changed;attempt++){
    if(GetForegroundWindow()!=game)break;
    {FifoTicketGuard gate(g_gameInputGate);if(SendInput(1,&d,sizeof(INPUT))!=1)break;PreciseDelayUs(18000);if(SendInput(1,&u,sizeof(INPUT))!=1)break;}
    changed=WarriorWaitSlotChanged(grid,slot,before,100);
    if(!changed)Sleep(12);
  }
  if(changed)Sleep(20);
  {FifoTicketGuard gate(g_gameInputGate);if(GetForegroundWindow()==game)ReferenceTapKeyUnlocked('I');}
  g_warriorInventoryKnownOpen=false;QueryPerformanceCounter(&t1);if(fq.QuadPart>0)g_warriorLastEquipUs=(ULONGLONG)((t1.QuadPart-t0.QuadPart)*1000000LL/fq.QuadPart);return changed;
}
'''
s=s[:start]+new+s[end:]

s=s.replace('if(wr==2)t=L"Envanter grid bulunamadı";else if(wr==3)t=L"Oyun odağı kayboldu";else{',
            'if(wr==2)t=L"Envanter grid bulunamadı / item değişimi doğrulanamadı";else if(wr==3)t=L"Oyun odağı kayboldu";else{',1)

old='''  g_ui.attackExtraList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX+4,336,430,74,IDC_ATTACK_EXTRA_LIST,g_fontSmall);PageAdd(p,g_ui.attackExtraList);
  PageAdd(p,Label(L"Yeni skill",kContentX+4,416,58,20,g_fontBold));PageAdd(p,Label(L"Bar",kContentX+68,416,24,20,g_fontSmall));
  g_ui.attackExtraBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+94,414,54,165,IDC_ATTACK_EXTRA_BAR,g_fontSmall);FillBar(g_ui.attackExtraBar);PageAdd(p,g_ui.attackExtraBar);
  PageAdd(p,Label(L"Slot",kContentX+156,416,28,20,g_fontSmall));g_ui.attackExtraSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+186,414,52,165,IDC_ATTACK_EXTRA_SLOT,g_fontSmall);FillSlot(g_ui.attackExtraSlot);PageAdd(p,g_ui.attackExtraSlot);
  PageAdd(p,Label(L"ms",kContentX+246,416,20,20,g_fontSmall));g_ui.attackExtraMs=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+268,414,42,20,IDC_ATTACK_EXTRA_MS,g_fontSmall);PageAdd(p,g_ui.attackExtraMs);
  g_ui.attackExtraAdd=Ctrl(L"BUTTON",L"+ Skill Ekle",BS_PUSHBUTTON,kContentX+320,412,90,24,IDC_ATTACK_EXTRA_ADD,g_fontSmall);PageAdd(p,g_ui.attackExtraAdd);
  g_ui.attackExtraRemove=Ctrl(L"BUTTON",L"Seçileni Sil",BS_PUSHBUTTON,kContentX+416,412,96,24,IDC_ATTACK_EXTRA_REMOVE,g_fontSmall);PageAdd(p,g_ui.attackExtraRemove);
  int y=450;'''
new='''  for(int i=0;i<kMaxAttackExtraUi;i++){
    int y=336+i*20,base=IDC_ATTACK_EXTRA_ROW_BASE+i*4;
    g_ui.attackExtraCheck[i]=Ctrl(L"BUTTON",L"",BS_AUTOCHECKBOX,kContentX+10,y,20,18,base,g_fontSmall);PageAdd(p,g_ui.attackExtraCheck[i]);
    g_ui.attackExtraRowLabel[i]=Label((L"Skill "+std::to_wstring(i+5)).c_str(),kContentX+66,y,92,18,g_fontSmall);PageAdd(p,g_ui.attackExtraRowLabel[i]);
    g_ui.attackExtraRowBar[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+196,y,58,150,base+1,g_fontSmall);FillBar(g_ui.attackExtraRowBar[i]);PageAdd(p,g_ui.attackExtraRowBar[i]);
    g_ui.attackExtraRowSlot[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+276,y,56,150,base+2,g_fontSmall);FillSlot(g_ui.attackExtraRowSlot[i]);PageAdd(p,g_ui.attackExtraRowSlot[i]);
    g_ui.attackExtraRowMs[i]=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+350,y,54,18,base+3,g_fontSmall);PageAdd(p,g_ui.attackExtraRowMs[i]);
  }
  PageAdd(p,Label(L"Skill Ekle",kContentX+4,442,62,20,g_fontBold));g_ui.attackExtraAdd=Ctrl(L"BUTTON",L"+",BS_PUSHBUTTON,kContentX+70,438,34,24,IDC_ATTACK_EXTRA_ADD,g_fontBold);PageAdd(p,g_ui.attackExtraAdd);g_ui.attackExtraRemove=Ctrl(L"BUTTON",L"-",BS_PUSHBUTTON,kContentX+110,438,34,24,IDC_ATTACK_EXTRA_REMOVE,g_fontBold);PageAdd(p,g_ui.attackExtraRemove);
  int y=468;'''
one(old,new,'attack extra ui')
s=s.replace('y=478;PageAdd(p,Label(L"MP POT"','y=496;PageAdd(p,Label(L"MP POT"',1)
s=s.replace('g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,510,224,26,1599,g_fontBold);','g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,528,224,26,1599,g_fontBold);',1)

needle='for(int i=0;i<kMaxMobSkillsUi;i++)if(id>=IDC_MOB_SKILL_ROW_BASE+i*4&&id<=IDC_MOB_SKILL_ROW_BASE+i*4+3)ReadMobUi(false);'
repl='for(int i=0;i<kMaxAttackExtraUi;i++)if(id>=IDC_ATTACK_EXTRA_ROW_BASE+i*4&&id<=IDC_ATTACK_EXTRA_ROW_BASE+i*4+3)ReadAttackUi(false);'+needle
one(needle,repl,'wm dynamic attack read')

s=s.replace('case IDC_ATTACK_EXTRA_ADD:ReadAttackUi(false);AddAttackExtra();break;','case IDC_ATTACK_EXTRA_ADD:ReadAttackUi(false);AddAttackExtra();break;',1)
s=s.replace('case IDC_ATTACK_EXTRA_REMOVE:RemoveAttackExtra();break;','case IDC_ATTACK_EXTRA_REMOVE:ReadAttackUi(false);RemoveAttackExtra();break;',1)

one('t("BattleCryRequiresCalibratedRect",!w.battleCryRect.valid());t("EquipCloseAfterClickSource",true);f<<"TOTAL="<<total<<"\\nPASSED="<<pass<<"\\n";return total==26&&pass==26;}',
    't("BattleCryRequiresCalibratedRect",!w.battleCryRect.valid());{std::vector<uint32_t>a(100,0x00101010u),b=a;t("EquipVisualNoChangeRejected",WarriorVisualChangeRatio(a,b)<0.06);for(int i=0;i<10;i++)b[i]=0x00F0F0F0u;t("EquipVisualChangeAccepted",WarriorVisualChangeRatio(a,b)>=0.06);}t("EquipCloseAfterClickSource",true);f<<"TOTAL="<<total<<"\\nPASSED="<<pass<<"\\n";return total==28&&pass==28;}',
    'warrior model verification tests')

insert=s.index('bool RunAttackObserverTest(){')
fn='''bool RunAttackRExtraObserverTest(){
  InitBridge();if(g_bridge)InterlockedExchange64(&g_bridge->gameHeartbeatMs,0);
  std::thread observer;bool ready=StartObserver(observer);
  if(ready){g_skillTurn=0;AttackSettings a;a.restoreBar=1;a.delayMs=1;a.skillEnabled={false,false,false,false};a.extraSkills.push_back({true,6,7,1});a.rAttack=true;a.zCombo=false;a.wCombo=false;a.sCombo=false;{std::lock_guard<std::mutex>lk(g_settingsMutex);g_attack=a;}g_attackActive=true;g_cureExclusive=false;g_potionExclusive=false;g_chatMode=false;ExecuteAttack(a);g_attackActive=false;}
  StopObserver(observer);
  bool extra=g_observerDownCount['7'].load()==1&&g_observerDownCount[VK_F6].load()==1;bool r=g_observerDownCount['R'].load()==1;bool restore=g_observerDownCount[VK_F1].load()==1;bool scans=g_observerScanNonZero['7']&&g_observerScanNonZero['R']&&g_observerScanNonZero[VK_F6]&&g_observerScanNonZero[VK_F1];bool overlap=g_observerOverlap.load()==0&&g_observerActiveDown.load()==0;
  std::ofstream f("attack-r-extra-observer-report.txt",std::ios::trunc);f<<"ObserverReady="<<(ready?"PASS":"FAIL")<<"\\nF6_Down="<<g_observerDownCount[VK_F6].load()<<"\\nSlot7_Down="<<g_observerDownCount['7'].load()<<"\\nR_Down="<<g_observerDownCount['R'].load()<<"\\nF1_Down="<<g_observerDownCount[VK_F1].load()<<"\\nDynamicSkillExec="<<(extra?"PASS":"FAIL")<<"\\nRAttackExec="<<(r?"PASS":"FAIL")<<"\\nRestoreBar="<<(restore?"PASS":"FAIL")<<"\\nScanCodesNonZero="<<(scans?"PASS":"FAIL")<<"\\nOverlap="<<g_observerOverlap.load()<<"\\n";bool ok=ready&&extra&&r&&restore&&scans&&overlap;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\\n";CloseBridge();return ok;
}
'''
s=s[:insert]+fn+s[insert:]

one('if(cmd&&wcsstr(cmd,L"--attack-observer-test"))return RunAttackObserverTest()?0:6;',
    'if(cmd&&wcsstr(cmd,L"--attack-r-extra-observer-test"))return RunAttackRExtraObserverTest()?0:12;if(cmd&&wcsstr(cmd,L"--attack-observer-test"))return RunAttackObserverTest()?0:6;',
    'wmain observer mode')

if s==orig: raise SystemExit('no changes')
p.write_text(s,encoding='utf-8')
print('V4825_WARRIOR_ATTACK_UI_FIX=APPLIED')
