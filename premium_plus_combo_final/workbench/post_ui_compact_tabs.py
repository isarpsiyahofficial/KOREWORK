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
    a,b=span(sig);s=s[:a]+new+s[b:]

def once(old,new,label):
    global s
    c=s.count(old)
    if c!=1: raise RuntimeError(f'{label}: expected 1 got {c}')
    s=s.replace(old,new,1)

once('constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue";','constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue | v4.8.15";','title')
once('constexpr int kWindowWidth = 800;','constexpr int kWindowWidth = 760;','width')
once('constexpr int kWindowHeight = 700;','constexpr int kWindowHeight = 620;','height')
once('constexpr int IDC_MOB_SAVE = 1755;','''constexpr int IDC_MOB_SAVE = 1755;
constexpr int IDC_MOB_TAB_GENERAL = 1760;
constexpr int IDC_MOB_TAB_PRIEST = 1761;''','mob tab ids')
once('std::vector<HWND> roguePage,attackPage,mobPage;','std::vector<HWND> roguePage,attackPage,mobPage,mobGeneralPage,mobPriestPage;','subpage vectors')
once('HWND mobHealEnable{},mobHealPct{},mobHealBar{},mobHealSlot{},mobHpCal{},mobSave{},mobPosStatus{};','HWND mobHealEnable{},mobHealPct{},mobHealBar{},mobHealSlot{},mobHpCal{},mobSave{},mobPosStatus{},mobTabGeneral{},mobTabPriest{};','subpage hwnds')
once('std::vector<ULONGLONG> g_scrollNextDue;','''std::vector<ULONGLONG> g_scrollNextDue;
std::atomic<int> g_currentCategory{0};
std::atomic<int> g_mobSubTab{0};''','ui state')

replace_fn('void ShowCategory(int category)',r'''void ShowMobSubCategory(int tab){
  g_mobSubTab=tab;
  const bool mobVisible=g_currentCategory.load()==2;
  for(HWND h:g_ui.mobGeneralPage)if(IsWindow(h))ShowWindow(h,(mobVisible&&tab==0)?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.mobPriestPage)if(IsWindow(h))ShowWindow(h,(mobVisible&&tab==1)?SW_SHOW:SW_HIDE);
  if(g_ui.mobTabGeneral)SetWindowTextW(g_ui.mobTabGeneral,tab==0?L"GENEL  •":L"GENEL");
  if(g_ui.mobTabPriest)SetWindowTextW(g_ui.mobTabPriest,tab==1?L"PRIEST  •":L"PRIEST");
}
void ShowCategory(int category){
  g_currentCategory=category;
  bool rogue=category==0,attack=category==1,mob=category==2;
  for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.attackPage)ShowWindow(h,attack?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.mobPage)ShowWindow(h,mob?SW_SHOW:SW_HIDE);
  ShowMobSubCategory(g_mobSubTab.load());
  InvalidateRect(g_ui.main,nullptr,TRUE);
}''')

replace_fn('void RefreshAttackExtraList()',r'''void RefreshAttackExtraList(){
  if(!g_ui.attackExtraList)return;
  SendMessageW(g_ui.attackExtraList,LB_RESETCONTENT,0,0);
  AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);a=g_attack;}
  for(size_t i=0;i<a.extraSkills.size();i++){
    const auto&e=a.extraSkills[i];
    wchar_t row[160]{};
    wsprintfW(row,L"Skill %-2d        F%-2d            %-2d             %d ms",(int)i+5,e.bar,e.slot,e.delayMs);
    SendMessageW(g_ui.attackExtraList,LB_ADDSTRING,0,(LPARAM)row);
  }
}''')

replace_fn('void CreateAttackPage()',r'''void CreateAttackPage(){
  auto& p=g_ui.attackPage;
  PageAdd(p,Label(L"ATTACK",kContentX,82,180,22,g_fontBold));
  g_ui.attackCategoryEnable=Ctrl(L"BUTTON",L"ATTACK AKTİF",BS_AUTOCHECKBOX,kContentX+492,82,112,21,IDC_ATTACK_CATEGORY_ENABLE,g_fontSmall);PageAdd(p,g_ui.attackCategoryEnable);
  PageAdd(p,Label(L"Kontrol",kContentX,108,70,20,g_fontBold));
  g_ui.attackStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,128,88,26,IDC_ATTACK_START,g_fontBold);PageAdd(p,g_ui.attackStart);
  g_ui.attackStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+94,128,88,26,IDC_ATTACK_STOP,g_fontBold);PageAdd(p,g_ui.attackStop);
  g_ui.attackStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+192,128,136,26,IDC_ATTACK_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.attackStartAssign);
  g_ui.attackStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+334,128,136,26,IDC_ATTACK_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.attackStopAssign);
  PageAdd(p,Label(L"Loop",kContentX+478,130,30,20,g_fontSmall));g_ui.attackDelay=Ctrl(L"EDIT",L"125",WS_BORDER|ES_CENTER,kContentX+510,128,44,23,1550,g_fontSmall);PageAdd(p,g_ui.attackDelay);PageAdd(p,Label(L"ms",kContentX+556,130,22,20,g_fontSmall));
  PageAdd(p,Label(L"Ana bar",kContentX,166,52,20,g_fontSmall));g_ui.restoreBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+55,164,56,180,1551,g_fontSmall);FillBar(g_ui.restoreBar);PageAdd(p,g_ui.restoreBar);
  g_ui.wCombo=Ctrl(L"BUTTON",L"W",BS_AUTOCHECKBOX,kContentX+126,164,34,22,IDC_ATTACK_W_COMBO,g_fontSmall);PageAdd(p,g_ui.wCombo);g_ui.wDelay=Ctrl(L"EDIT",L"400",WS_BORDER|ES_CENTER,kContentX+160,164,40,22,IDC_ATTACK_W_MS,g_fontSmall);PageAdd(p,g_ui.wDelay);PageAdd(p,Label(L"ms",kContentX+202,166,20,20,g_fontSmall));
  g_ui.sCombo=Ctrl(L"BUTTON",L"S",BS_AUTOCHECKBOX,kContentX+230,164,34,22,IDC_ATTACK_S_COMBO,g_fontSmall);PageAdd(p,g_ui.sCombo);g_ui.sDelay=Ctrl(L"EDIT",L"50",WS_BORDER|ES_CENTER,kContentX+264,164,40,22,IDC_ATTACK_S_MS,g_fontSmall);PageAdd(p,g_ui.sDelay);PageAdd(p,Label(L"ms",kContentX+306,166,20,20,g_fontSmall));
  g_ui.zCombo=Ctrl(L"BUTTON",L"Z",BS_AUTOCHECKBOX,kContentX+334,164,34,22,IDC_ATTACK_Z_COMBO,g_fontSmall);PageAdd(p,g_ui.zCombo);
  g_ui.attackRandom=Ctrl(L"BUTTON",L"Random kullan",BS_AUTOCHECKBOX,kContentX+398,164,110,22,IDC_ATTACK_RANDOM,g_fontSmall);PageAdd(p,g_ui.attackRandom);
  PageAdd(p,Label(L"SKILLLER",kContentX,198,90,20,g_fontBold));
  PageAdd(p,Label(L"AKTİF",kContentX+4,218,42,18,g_fontSmall));PageAdd(p,Label(L"SKILL",kContentX+66,218,50,18,g_fontSmall));PageAdd(p,Label(L"BAR",kContentX+206,218,42,18,g_fontSmall));PageAdd(p,Label(L"SLOT",kContentX+286,218,42,18,g_fontSmall));PageAdd(p,Label(L"MS",kContentX+360,218,42,18,g_fontSmall));
  for(int i=0;i<4;i++){
    int y=238+i*24;
    g_ui.skillCheck[i]=Ctrl(L"BUTTON",L"",BS_AUTOCHECKBOX,kContentX+10,y,20,20,IDC_ATTACK_SKILL_BASE+i,g_fontSmall);PageAdd(p,g_ui.skillCheck[i]);
    PageAdd(p,Label((L"Skill "+std::to_wstring(i+1)).c_str(),kContentX+66,y,92,20,g_fontSmall));
    g_ui.skillBar[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+196,y,58,165,1570+i,g_fontSmall);FillBar(g_ui.skillBar[i]);PageAdd(p,g_ui.skillBar[i]);
    g_ui.skillSlot[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+276,y,56,165,1580+i,g_fontSmall);FillSlot(g_ui.skillSlot[i]);PageAdd(p,g_ui.skillSlot[i]);
    g_ui.skillDelay[i]=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+350,y,54,20,1610+i,g_fontSmall);PageAdd(p,g_ui.skillDelay[i]);
  }
  g_ui.attackExtraList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX+4,336,430,74,IDC_ATTACK_EXTRA_LIST,g_fontSmall);PageAdd(p,g_ui.attackExtraList);
  PageAdd(p,Label(L"Yeni skill",kContentX+4,416,58,20,g_fontBold));PageAdd(p,Label(L"Bar",kContentX+68,416,24,20,g_fontSmall));
  g_ui.attackExtraBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+94,414,54,165,IDC_ATTACK_EXTRA_BAR,g_fontSmall);FillBar(g_ui.attackExtraBar);PageAdd(p,g_ui.attackExtraBar);
  PageAdd(p,Label(L"Slot",kContentX+156,416,28,20,g_fontSmall));g_ui.attackExtraSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+186,414,52,165,IDC_ATTACK_EXTRA_SLOT,g_fontSmall);FillSlot(g_ui.attackExtraSlot);PageAdd(p,g_ui.attackExtraSlot);
  PageAdd(p,Label(L"ms",kContentX+246,416,20,20,g_fontSmall));g_ui.attackExtraMs=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+268,414,42,20,IDC_ATTACK_EXTRA_MS,g_fontSmall);PageAdd(p,g_ui.attackExtraMs);
  g_ui.attackExtraAdd=Ctrl(L"BUTTON",L"+ Skill Ekle",BS_PUSHBUTTON,kContentX+320,412,90,24,IDC_ATTACK_EXTRA_ADD,g_fontSmall);PageAdd(p,g_ui.attackExtraAdd);
  g_ui.attackExtraRemove=Ctrl(L"BUTTON",L"Seçileni Sil",BS_PUSHBUTTON,kContentX+416,412,96,24,IDC_ATTACK_EXTRA_REMOVE,g_fontSmall);PageAdd(p,g_ui.attackExtraRemove);
  int y=450;PageAdd(p,Label(L"HP POT",kContentX,y,54,20,g_fontBold));g_ui.hpCheck=Ctrl(L"BUTTON",L"Aktif",BS_AUTOCHECKBOX,kContentX+58,y,48,20,IDC_HP_CHECK,g_fontSmall);PageAdd(p,g_ui.hpCheck);PageAdd(p,Label(L"%",kContentX+108,y,12,20,g_fontSmall));g_ui.hpThreshold=Ctrl(L"EDIT",L"60",WS_BORDER|ES_CENTER,kContentX+122,y,36,20,1590,g_fontSmall);PageAdd(p,g_ui.hpThreshold);PageAdd(p,Label(L"Bar",kContentX+166,y,24,20,g_fontSmall));g_ui.hpBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+192,y,54,160,1591,g_fontSmall);FillBar(g_ui.hpBar);PageAdd(p,g_ui.hpBar);PageAdd(p,Label(L"Slot",kContentX+252,y,28,20,g_fontSmall));g_ui.hpSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+282,y,52,160,1592,g_fontSmall);FillSlot(g_ui.hpSlot);PageAdd(p,g_ui.hpSlot);g_ui.hpCal=Ctrl(L"BUTTON",L"Kalibre",BS_PUSHBUTTON,kContentX+342,y-1,72,22,IDC_HP_CAL,g_fontSmall);PageAdd(p,g_ui.hpCal);g_ui.hpPercent=Label(L"HP: -",kContentX+422,y,155,20,g_fontSmall);PageAdd(p,g_ui.hpPercent);
  y=478;PageAdd(p,Label(L"MP POT",kContentX,y,54,20,g_fontBold));g_ui.mpCheck=Ctrl(L"BUTTON",L"Aktif",BS_AUTOCHECKBOX,kContentX+58,y,48,20,IDC_MP_CHECK,g_fontSmall);PageAdd(p,g_ui.mpCheck);PageAdd(p,Label(L"%",kContentX+108,y,12,20,g_fontSmall));g_ui.mpThreshold=Ctrl(L"EDIT",L"35",WS_BORDER|ES_CENTER,kContentX+122,y,36,20,1593,g_fontSmall);PageAdd(p,g_ui.mpThreshold);PageAdd(p,Label(L"Bar",kContentX+166,y,24,20,g_fontSmall));g_ui.mpBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+192,y,54,160,1594,g_fontSmall);FillBar(g_ui.mpBar);PageAdd(p,g_ui.mpBar);PageAdd(p,Label(L"Slot",kContentX+252,y,28,20,g_fontSmall));g_ui.mpSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+282,y,52,160,1595,g_fontSmall);FillSlot(g_ui.mpSlot);PageAdd(p,g_ui.mpSlot);g_ui.mpCal=Ctrl(L"BUTTON",L"Kalibre",BS_PUSHBUTTON,kContentX+342,y-1,72,22,IDC_MP_CAL,g_fontSmall);PageAdd(p,g_ui.mpCal);g_ui.mpPercent=Label(L"MP: -",kContentX+422,y,155,20,g_fontSmall);PageAdd(p,g_ui.mpPercent);
  g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,510,224,26,1599,g_fontBold);PageAdd(p,g_ui.saveAttack);
}''')

replace_fn('void CreateMobPage()',r'''void CreateMobPage(){
  auto& p=g_ui.mobPage;
  auto general=[&](HWND h){PageAdd(p,h);g_ui.mobGeneralPage.push_back(h);};
  auto priest=[&](HWND h){PageAdd(p,h);g_ui.mobPriestPage.push_back(h);};
  PageAdd(p,Label(L"MOB ATTACK",kContentX,82,180,22,g_fontBold));
  PageAdd(p,Label(L"Ortak kontrol",kContentX,108,90,20,g_fontBold));
  g_ui.mobStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,128,82,26,IDC_MOB_START,g_fontBold);PageAdd(p,g_ui.mobStart);
  g_ui.mobStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+88,128,82,26,IDC_MOB_STOP,g_fontBold);PageAdd(p,g_ui.mobStop);
  g_ui.mobStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+180,128,130,26,IDC_MOB_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStartAssign);
  g_ui.mobStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+316,128,130,26,IDC_MOB_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStopAssign);
  g_ui.mobTabGeneral=Ctrl(L"BUTTON",L"GENEL  •",BS_PUSHBUTTON,kContentX,166,112,26,IDC_MOB_TAB_GENERAL,g_fontBold);PageAdd(p,g_ui.mobTabGeneral);
  g_ui.mobTabPriest=Ctrl(L"BUTTON",L"PRIEST",BS_PUSHBUTTON,kContentX+118,166,112,26,IDC_MOB_TAB_PRIEST,g_fontBold);PageAdd(p,g_ui.mobTabPriest);
  g_ui.mobGeneralEnable=Ctrl(L"BUTTON",L"GENEL AKTİF",BS_AUTOCHECKBOX,kContentX+250,168,104,22,IDC_MOB_GENERAL_ENABLE,g_fontSmall);general(g_ui.mobGeneralEnable);
  HWND h=Label(L"Range",kContentX,204,36,20,g_fontSmall);general(h);g_ui.mobRange=Ctrl(L"EDIT",L"48",WS_BORDER|ES_CENTER,kContentX+40,202,38,21,IDC_MOB_RANGE,g_fontSmall);general(g_ui.mobRange);
  g_ui.mobAnchor=Ctrl(L"BUTTON",L"ANKOR AL",BS_PUSHBUTTON,kContentX+86,200,76,24,IDC_MOB_ANCHOR,g_fontSmall);general(g_ui.mobAnchor);g_ui.mobPosStatus=Label(L"POS: bekleniyor",kContentX+170,203,130,20,g_fontSmall);general(g_ui.mobPosStatus);
  g_ui.mobRandom=Ctrl(L"BUTTON",L"Random kullan",BS_AUTOCHECKBOX,kContentX+330,202,108,22,IDC_MOB_RANDOM,g_fontSmall);general(g_ui.mobRandom);
  h=Label(L"SKILL",kContentX,236,60,20,g_fontBold);general(h);h=Label(L"Bar",kContentX+70,236,24,20,g_fontSmall);general(h);g_ui.mobSkillBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+96,234,54,165,IDC_MOB_SKILL_BAR,g_fontSmall);FillBar(g_ui.mobSkillBar);general(g_ui.mobSkillBar);h=Label(L"Slot",kContentX+158,236,28,20,g_fontSmall);general(h);g_ui.mobSkillSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+188,234,52,165,IDC_MOB_SKILL_SLOT,g_fontSmall);FillSlot(g_ui.mobSkillSlot);general(g_ui.mobSkillSlot);h=Label(L"ms",kContentX+248,236,20,20,g_fontSmall);general(h);g_ui.mobSkillMs=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+270,234,42,20,IDC_MOB_SKILL_MS,g_fontSmall);general(g_ui.mobSkillMs);g_ui.mobSkillAdd=Ctrl(L"BUTTON",L"+ Skill Ekle",BS_PUSHBUTTON,kContentX+322,232,90,24,IDC_MOB_SKILL_ADD,g_fontSmall);general(g_ui.mobSkillAdd);g_ui.mobSkillRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+418,232,42,24,IDC_MOB_SKILL_REMOVE,g_fontSmall);general(g_ui.mobSkillRemove);g_ui.mobSkillList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX,262,462,82,IDC_MOB_SKILL_LIST,g_fontSmall);general(g_ui.mobSkillList);
  h=Label(L"SCROLL",kContentX,356,70,20,g_fontBold);general(h);h=Label(L"Bar",kContentX+70,356,24,20,g_fontSmall);general(h);g_ui.mobScrollBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+96,354,54,165,IDC_MOB_SCROLL_BAR,g_fontSmall);FillBar(g_ui.mobScrollBar);general(g_ui.mobScrollBar);h=Label(L"Slot",kContentX+158,356,28,20,g_fontSmall);general(h);g_ui.mobScrollSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+188,354,52,165,IDC_MOB_SCROLL_SLOT,g_fontSmall);FillSlot(g_ui.mobScrollSlot);general(g_ui.mobScrollSlot);h=Label(L"dk",kContentX+248,356,20,20,g_fontSmall);general(h);g_ui.mobScrollMin=Ctrl(L"EDIT",L"30",WS_BORDER|ES_CENTER,kContentX+270,354,42,20,IDC_MOB_SCROLL_MIN,g_fontSmall);general(g_ui.mobScrollMin);g_ui.mobScrollAdd=Ctrl(L"BUTTON",L"+ Scroll Ekle",BS_PUSHBUTTON,kContentX+322,352,96,24,IDC_MOB_SCROLL_ADD,g_fontSmall);general(g_ui.mobScrollAdd);g_ui.mobScrollRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+424,352,42,24,IDC_MOB_SCROLL_REMOVE,g_fontSmall);general(g_ui.mobScrollRemove);g_ui.mobScrollList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX,382,466,86,IDC_MOB_SCROLL_LIST,g_fontSmall);general(g_ui.mobScrollList);
  g_ui.mobPriestEnable=Ctrl(L"BUTTON",L"PRIEST AKTİF",BS_AUTOCHECKBOX,kContentX+250,168,108,22,IDC_MOB_PRIEST_ENABLE,g_fontSmall);priest(g_ui.mobPriestEnable);
  h=Label(L"HEAL",kContentX,210,70,20,g_fontBold);priest(h);g_ui.mobHealEnable=Ctrl(L"BUTTON",L"HP eşik altına düşünce 1 kez Heal",BS_AUTOCHECKBOX,kContentX,236,220,22,IDC_MOB_HEAL_ENABLE,g_fontSmall);priest(g_ui.mobHealEnable);h=Label(L"%",kContentX+230,238,14,20,g_fontSmall);priest(h);g_ui.mobHealPct=Ctrl(L"EDIT",L"40",WS_BORDER|ES_CENTER,kContentX+246,236,38,20,IDC_MOB_HEAL_PCT,g_fontSmall);priest(g_ui.mobHealPct);h=Label(L"Bar",kContentX+296,238,24,20,g_fontSmall);priest(h);g_ui.mobHealBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+322,236,54,165,IDC_MOB_HEAL_BAR,g_fontSmall);FillBar(g_ui.mobHealBar);priest(g_ui.mobHealBar);h=Label(L"Slot",kContentX+384,238,28,20,g_fontSmall);priest(h);g_ui.mobHealSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+414,236,52,165,IDC_MOB_HEAL_SLOT,g_fontSmall);FillSlot(g_ui.mobHealSlot);priest(g_ui.mobHealSlot);g_ui.mobHpCal=Ctrl(L"BUTTON",L"HP Kalibre",BS_PUSHBUTTON,kContentX,272,86,24,IDC_MOB_HP_CAL,g_fontSmall);priest(g_ui.mobHpCal);
  h=Label(L"Heal eşik davranışı: eşik geçilince tek heal atar; HP tekrar yükselmeden spam yapmaz.",kContentX,310,500,38,g_fontSmall);priest(h);
  g_ui.mobSave=Ctrl(L"BUTTON",L"MOB ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,494,244,26,IDC_MOB_SAVE,g_fontBold);PageAdd(p,g_ui.mobSave);
  PopulateMobUi();ShowMobSubCategory(0);
}''')

font_code=r'''struct UiFontSwapCtx{HFONT on{},ob{},os{},nn{},nb{},ns{};};
BOOL CALLBACK SwapUiFontProc(HWND h,LPARAM lp){auto*c=(UiFontSwapCtx*)lp;HFONT f=(HFONT)SendMessageW(h,WM_GETFONT,0,0);HFONT n=f==c->ob?c->nb:(f==c->os?c->ns:(f==c->on?c->nn:nullptr));if(n)SendMessageW(h,WM_SETFONT,(WPARAM)n,TRUE);return TRUE;}
void RebuildUiFonts(UINT dpi){
  if(!dpi)dpi=96;HFONT on=g_font,ob=g_fontBold,os=g_fontSmall;
  HFONT nn=CreateFontW(-MulDiv(13,(int)dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  HFONT nb=CreateFontW(-MulDiv(14,(int)dpi,96),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  HFONT ns=CreateFontW(-MulDiv(11,(int)dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  if(g_ui.main){UiFontSwapCtx c{on,ob,os,nn,nb,ns};EnumChildWindows(g_ui.main,SwapUiFontProc,(LPARAM)&c);}
  g_font=nn;g_fontBold=nb;g_fontSmall=ns;if(on)DeleteObject(on);if(ob)DeleteObject(ob);if(os)DeleteObject(os);
}
'''
insert='void DrawOwnerButton(DRAWITEMSTRUCT* d)'
assert s.count(insert)==1
s=s.replace(insert,font_code+'\n'+insert,1)

replace_fn('void LayoutChrome()',r'''void LayoutChrome(){if(!g_ui.main)return;RECT r{};GetClientRect(g_ui.main,&r);int cw=r.right-r.left,ch=r.bottom-r.top;if(g_ui.power)MoveWindow(g_ui.power,std::max(kContentX+470,cw-140),14,116,30,TRUE);if(g_ui.status)MoveWindow(g_ui.status,kContentX,std::max(546,ch-36),std::max(220,cw-kContentX-20),20,TRUE);InvalidateRect(g_ui.main,nullptr,TRUE);}''')

a,b=span('LRESULT CALLBACK WndProc');fn=s[a:b]
fn=fn.replace('case WM_SIZE:LayoutChrome();return 0;','case WM_DPICHANGED:{RECT* rr=(RECT*)l;if(rr)SetWindowPos(h,nullptr,rr->left,rr->top,rr->right-rr->left,rr->bottom-rr->top,SWP_NOZORDER|SWP_NOACTIVATE);RebuildUiFonts(HIWORD(w));LayoutChrome();return 0;}case WM_SIZE:LayoutChrome();return 0;',1)
fn=fn.replace('case IDC_CATEGORY_MOB:ShowCategory(2);break;','case IDC_CATEGORY_MOB:ShowCategory(2);break;case IDC_MOB_TAB_GENERAL:ShowMobSubCategory(0);break;case IDC_MOB_TAB_PRIEST:ShowMobSubCategory(1);break;',1)
s=s[:a]+fn+s[b:]

replace_fn('void CreateControls()',r'''void CreateControls(){g_ui.power=Ctrl(L"BUTTON",L"POWER  AÇIK",BS_OWNERDRAW,620,14,116,30,IDC_POWER,g_fontBold);g_ui.catRogue=Ctrl(L"BUTTON",L"ROGUE",BS_OWNERDRAW,10,96,96,34,IDC_CATEGORY_ROGUE,g_fontBold);g_ui.catAttack=Ctrl(L"BUTTON",L"ATTACK",BS_OWNERDRAW,10,138,96,34,IDC_CATEGORY_ATTACK,g_fontBold);g_ui.catMob=Ctrl(L"BUTTON",L"MOB ATTACK",BS_OWNERDRAW,10,180,96,34,IDC_CATEGORY_MOB,g_fontSmall);g_ui.status=Ctrl(L"STATIC",L"",SS_LEFT|SS_CENTERIMAGE,kContentX,546,600,20,1600,g_fontSmall);CreateRoguePage();CreateAttackPage();CreateMobPage();PopulateUi();ShowCategory(0);LayoutChrome();}''')

a,b=span('int APIENTRY wWinMain');main=s[a:b]
old='g_font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");g_fontBold=CreateFontW(-16,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");g_fontSmall=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");'
assert main.count(old)==1
main=main.replace(old,'RebuildUiFonts(GetDpiForSystem());',1)
s=s[:a]+main+s[b:]

required=['IDC_MOB_TAB_GENERAL','IDC_MOB_TAB_PRIEST','ShowMobSubCategory','Skill %-2d','RebuildUiFonts','case IDC_MOB_TAB_GENERAL','ReferenceTapKeyUnlocked(MobChaseKey())']
for x in required:
    if x not in s: raise RuntimeError('missing '+x)
if s.count('void MinorWorker()')!=1 or s.count('void CureWorker()')!=1 or s.count('void AttackWorker()')!=1:
    raise RuntimeError('core worker structure changed unexpectedly')

p.write_text(s,encoding='utf-8',newline='\n')
print('UI_COMPACT_TABS=PASS')
