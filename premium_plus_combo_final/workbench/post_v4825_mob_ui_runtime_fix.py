import pathlib,sys,re
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.find(sig)
    if a<0: raise RuntimeError('missing '+sig)
    q=s.find('{',a); d=0
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

s=s.replace('Premium Plus Combo | v4.8.24','Premium Plus Combo | v4.8.25')
once('bool sidebar=d->CtlID==IDC_CATEGORY_ROGUE||d->CtlID==IDC_CATEGORY_ATTACK||d->CtlID==IDC_CATEGORY_MOB||d->CtlID==IDC_CATEGORY_WARRIOR;',
     'bool sidebar=d->CtlID==IDC_CATEGORY_ROGUE||d->CtlID==IDC_CATEGORY_WARRIOR||d->CtlID==IDC_CATEGORY_PRIEST||d->CtlID==IDC_CATEGORY_ATTACK||d->CtlID==IDC_CATEGORY_MOB;',
     'priest sidebar style')

replace_fn('bool IsMobRed',r'''bool IsMobRed(uint32_t p){
  int b=p&255,g=(p>>8)&255,r=(p>>16)&255;
  return (r>=105&&r>=g+24&&r>=b+20)||(r>=145&&r>=g+18&&r>=b+16);
}''')

replace_fn('MobMaskSig MakeMobMaskSig',r'''MobMaskSig MakeMobMaskSig(const std::vector<uint32_t>&px,int W,int H,RECT rr){
  MobMaskSig o{};if(W<=0||H<=0||px.size()<(size_t)W*H)return o;
  rr.left=std::clamp<LONG>(rr.left,0,W-1);rr.right=std::clamp<LONG>(rr.right,rr.left+1,W);
  rr.top=std::clamp<LONG>(rr.top,0,H-1);rr.bottom=std::clamp<LONG>(rr.bottom,rr.top+1,H);
  int l=rr.right,t=rr.bottom,r=rr.left,b=rr.top,totalRed=0;
  for(int y=rr.top;y<rr.bottom;y++)for(int x=rr.left;x<rr.right;x++)if(IsMobRed(px[(size_t)y*W+x])){l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);totalRed++;}
  if(r<l||b<t||totalRed<6)return o;r++;b++;int ww=r-l,hh=b-t;if(ww<6||hh<3)return o;
  o.aspect1000=ww*1000/std::max(1,hh);
  for(int gy=0;gy<8;gy++)for(int gx=0;gx<16;gx++){
    int x0=l+gx*ww/16,x1=l+(gx+1)*ww/16,y0=t+gy*hh/8,y1=t+(gy+1)*hh/8,c=0,n=0;
    for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){n++;if(IsMobRed(px[(size_t)yy*W+xx]))c++;}
    if(n&&c*12>=n){int bit=gy*16+gx;if(bit<64)o.lo|=1ull<<bit;else o.hi|=1ull<<(bit-64);}
  }
  return o;
}''')

replace_fn('std::vector<RECT> FindRedNameBands',r'''std::vector<RECT> FindRedNameBands(const std::vector<uint32_t>&px,int W,int H){
  std::vector<RECT>out;if(W<80||H<80)return out;
  int xl=W/14,xr=W*13/14,yt=H/18,yb=H*4/5;std::vector<int> row(H);
  for(int y=yt;y<yb;y++)for(int x=xl;x<xr;x++)if(IsMobRed(px[(size_t)y*W+x]))row[y]++;
  int y=yt;while(y<yb){
    while(y<yb&&row[y]<2)y++;if(y>=yb)break;int y0=y,last=y;
    for(;y<yb;y++){if(row[y]>=2)last=y;else if(y-last>3)break;}
    int y1=last+1;if(y1-y0<3||y1-y0>42)continue;std::vector<int> col(W);
    for(int yy=y0;yy<y1;yy++)for(int x=xl;x<xr;x++)if(IsMobRed(px[(size_t)yy*W+x]))col[x]++;
    int x=xl;while(x<xr){while(x<xr&&!col[x])x++;if(x>=xr)break;int x0=x,lastx=x;for(;x<xr;x++){if(col[x])lastx=x;else if(x-lastx>15)break;}int x1=lastx+1;if(x1-x0>=8&&x1-x0<=300)out.push_back({std::max(0,x0-3),std::max(0,y0-3),std::min(W,x1+3),std::min(H,y1+3)});}
  }
  return out;
}''')

replace_fn('std::vector<MobCandidate> ScanMobCandidates',r'''std::vector<MobCandidate> ScanMobCandidates(HWND game,const MobSettings&m){
  std::vector<MobCandidate>out;std::vector<uint32_t>px;int W=0,H=0;POINT org{};if(!CaptureGameClient(game,px,W,H,org))return out;
  auto bands=FindRedNameBands(px,W,H);
  for(const RECT&r:bands)for(int i=0;i<kMaxMobTargets;i++){const auto&t=m.targets[i];if(!t.enabled||!t.visualReady())continue;double sc=MatchTargetRecord(px,W,H,r,t);if(sc>=0.49){MobCandidate c;c.r={r.left+org.x,r.top+org.y,r.right+org.x,r.bottom+org.y};c.sig=MakeMobMaskSig(px,W,H,r);c.score=sc;c.record=i;out.push_back(c);}}
  std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.score>b.score;});return out;
}''')

replace_fn('bool ConfirmMobCandidate',r'''bool ConfirmMobCandidate(HWND game,const MobCandidate&c,const MobTargetRecord&r){
  int cx=(c.r.left+c.r.right)/2,baseY=c.r.bottom;
  const int off[][2]={{0,14},{0,24},{0,36},{-16,30},{16,30},{0,48},{-24,46},{24,46},{0,62},{0,78}};
  for(auto&o:off){
    if(!ClickScreenLeft(cx+o[0],baseY+o[1]))continue;
    ULONGLONG until=GetTickCount64()+230;
    while(GetTickCount64()<until){Sleep(8);if(GetForegroundWindow()!=game)return false;if(TargetHpBarVisible(game)&&HeaderMatchesTarget(game,r))return true;}
  }
  return false;
}''')

a,b=span('bool ConfirmMobCandidate')
helper=r'''

bool TryMobZFallback(HWND game,const MobSettings&m){
  int active=0;for(const auto&t:m.targets)if(t.enabled&&t.visualReady())active++;
  if(active<=0)return false;
  int attempts=std::clamp(active*3,4,18);
  for(int n=0;n<attempts;n++){
    if(GetForegroundWindow()!=game||!g_running)return false;
    if(!ReferenceTapKey('Z'))return false;
    ULONGLONG until=GetTickCount64()+150;
    while(GetTickCount64()<until){
      Sleep(8);if(!TargetHpBarVisible(game))continue;
      for(int i=0;i<kMaxMobTargets;i++)if(m.targets[i].enabled&&m.targets[i].visualReady()&&HeaderMatchesTarget(game,m.targets[i])){g_mobTargetRecord=i;g_mobTargetScore=1.0;return true;}
    }
  }
  return false;
}
'''
s=s[:b]+helper+s[b:]

replace_fn('void MobTargetWorker()',r'''void MobTargetWorker(){
  ULONGLONG nextZFallback=0;
  while(g_running){
    RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}
    HWND game=(HWND)g_gameWindow.load();bool ready=r.powerEnabled&&g_mobActive&&m.generalEnabled&&game&&IsWindow(game)&&GetForegroundWindow()==game&&!g_chatMode.load();
    if(!ready){g_mobTargetConfirmed=false;Sleep(25);continue;}
    if(!MobWithinLeash(m)){g_mobTargetConfirmed=false;Sleep(35);continue;}
    if(g_mobTargetConfirmed.load()){
      if(GetTickCount64()-g_mobTargetLastConfirm.load()<250&&TargetHpBarVisible(game)){Sleep(20);continue;}
      int ri=g_mobTargetRecord.load();bool ok=ri>=0&&ri<kMaxMobTargets&&TargetHpBarVisible(game)&&HeaderMatchesTarget(game,m.targets[ri]);
      if(ok){g_mobTargetLastConfirm=GetTickCount64();Sleep(35);continue;}g_mobTargetConfirmed=false;g_mobTargetRecord=-1;
    }
    auto cand=ScanMobCandidates(game,m);bool found=false;
    for(auto&c:cand){if(c.record<0||c.record>=kMaxMobTargets)continue;if(ConfirmMobCandidate(game,c,m.targets[c.record])){g_mobTargetRecord=c.record;g_mobTargetScore=c.score;g_mobTargetConfirmed=true;g_mobTargetLastConfirm=GetTickCount64();found=true;break;}}
    ULONGLONG now=GetTickCount64();if(!found&&now>=nextZFallback){if(TryMobZFallback(game,m)){g_mobTargetConfirmed=true;g_mobTargetLastConfirm=GetTickCount64();found=true;}nextZFallback=GetTickCount64()+220;}
    Sleep(found?12:28);
  }
}''')

replace_fn('void RefreshMobLists()',r'''void RefreshMobLists(){
  MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);m=g_mob;}
  const bool mobVisible=g_currentCategory.load()==4;const int tab=g_mobSubTab.load();
  for(int i=0;i<kMaxMobSkillsUi;i++){
    bool rowExists=i<(int)m.skills.size();bool show=mobVisible&&tab==1&&rowExists;
    HWND hs[]={g_ui.mobSkillRowLabel[i],g_ui.mobSkillCheck[i],g_ui.mobSkillRowBar[i],g_ui.mobSkillRowSlot[i],g_ui.mobSkillRowMs[i]};
    for(HWND h:hs)if(h)ShowWindow(h,show?SW_SHOW:SW_HIDE);
    if(rowExists){SetWindowTextW(g_ui.mobSkillRowLabel[i],(L"Skill "+std::to_wstring(i+1)).c_str());SendMessageW(g_ui.mobSkillCheck[i],BM_SETCHECK,m.skills[i].enabled?BST_CHECKED:BST_UNCHECKED,0);SetCombo(g_ui.mobSkillRowBar[i],m.skills[i].bar,1);SetCombo(g_ui.mobSkillRowSlot[i],m.skills[i].slot,1);SetWindowTextW(g_ui.mobSkillRowMs[i],std::to_wstring(m.skills[i].delayMs).c_str());}
  }
  if(g_ui.mobScrollList){SendMessageW(g_ui.mobScrollList,LB_RESETCONTENT,0,0);for(size_t i=0;i<m.scrolls.size();i++){auto&e=m.scrolls[i];std::wstring t=L"SC "+std::to_wstring(i+1)+L"  F"+std::to_wstring(e.bar)+L" / "+(e.slot==10?L"0":std::to_wstring(e.slot))+L"  "+std::to_wstring(e.intervalSec/60)+L" dk";SendMessageW(g_ui.mobScrollList,LB_ADDSTRING,0,(LPARAM)t.c_str());}}
  for(int i=0;i<kMaxMobTargets;i++){auto&t=m.targets[i];if(g_ui.mobTargetEnable[i])SendMessageW(g_ui.mobTargetEnable[i],BM_SETCHECK,t.enabled?BST_CHECKED:BST_UNCHECKED,0);if(g_ui.mobTargetExact[i])SendMessageW(g_ui.mobTargetExact[i],BM_SETCHECK,t.exactName?BST_CHECKED:BST_UNCHECKED,0);if(g_ui.mobTargetName[i])SetWindowTextW(g_ui.mobTargetName[i],t.name.c_str());if(g_ui.mobTargetState[i])SetWindowTextW(g_ui.mobTargetState[i],t.visualReady()?L"Görsel OK":L"Görsel yok");}
  ShowMobSubCategory(tab);
}''')

replace_fn('void CreateMobPage()',r'''void CreateMobPage(){
  auto&p=g_ui.mobPage;auto target=[&](HWND h){PageAdd(p,h);g_ui.mobTargetPage.push_back(h);};auto skill=[&](HWND h){PageAdd(p,h);g_ui.mobSkillPage.push_back(h);};auto scroll=[&](HWND h){PageAdd(p,h);g_ui.mobScrollPage.push_back(h);};
  PageAdd(p,Label(L"MOB ATTACK",kContentX,82,150,20,g_fontBold));g_ui.mobGeneralEnable=Ctrl(L"BUTTON",L"MOB AKTİF",BS_AUTOCHECKBOX,kContentX+470,82,105,20,IDC_MOB_GENERAL_ENABLE,g_fontSmall);PageAdd(p,g_ui.mobGeneralEnable);
  g_ui.mobStart=Ctrl(L"BUTTON",L"BAŞLAT",BS_PUSHBUTTON,kContentX,108,72,24,IDC_MOB_START,g_fontBold);PageAdd(p,g_ui.mobStart);g_ui.mobStop=Ctrl(L"BUTTON",L"DURDUR",BS_PUSHBUTTON,kContentX+78,108,72,24,IDC_MOB_STOP,g_fontBold);PageAdd(p,g_ui.mobStop);
  g_ui.mobStartAssign=Ctrl(L"BUTTON",L"Açma",BS_PUSHBUTTON,kContentX+158,108,112,24,IDC_MOB_START_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStartAssign);g_ui.mobStopAssign=Ctrl(L"BUTTON",L"Kapatma",BS_PUSHBUTTON,kContentX+276,108,112,24,IDC_MOB_STOP_ASSIGN,g_fontSmall);PageAdd(p,g_ui.mobStopAssign);
  PageAdd(p,Label(L"Range",kContentX+398,111,36,18,g_fontSmall));g_ui.mobRange=Ctrl(L"EDIT",L"48",WS_BORDER|ES_CENTER,kContentX+436,108,38,22,IDC_MOB_RANGE,g_fontSmall);PageAdd(p,g_ui.mobRange);g_ui.mobCoordCal=Ctrl(L"BUTTON",L"Koordinat Alanı",BS_PUSHBUTTON,kContentX+480,106,112,24,IDC_MOB_COORD_CAL,g_fontSmall);PageAdd(p,g_ui.mobCoordCal);
  g_ui.mobAnchor=Ctrl(L"BUTTON",L"FARM MERKEZİNİ KAYDET",BS_PUSHBUTTON,kContentX,138,164,24,IDC_MOB_ANCHOR,g_fontSmall);PageAdd(p,g_ui.mobAnchor);g_ui.mobPosStatus=Label(L"Range: koordinat bekleniyor",kContentX+172,141,230,18,g_fontSmall);PageAdd(p,g_ui.mobPosStatus);g_ui.mobRandom=Ctrl(L"BUTTON",L"Random skill",BS_AUTOCHECKBOX,kContentX+420,138,100,22,IDC_MOB_RANDOM,g_fontSmall);PageAdd(p,g_ui.mobRandom);
  g_ui.mobTabTarget=Ctrl(L"BUTTON",L"HEDEF  •",BS_PUSHBUTTON,kContentX,170,84,24,IDC_MOB_TAB_TARGET,g_fontBold);PageAdd(p,g_ui.mobTabTarget);g_ui.mobTabSkill=Ctrl(L"BUTTON",L"SKILL",BS_PUSHBUTTON,kContentX+90,170,84,24,IDC_MOB_TAB_SKILL,g_fontBold);PageAdd(p,g_ui.mobTabSkill);g_ui.mobTabScroll=Ctrl(L"BUTTON",L"SCROLL",BS_PUSHBUTTON,kContentX+180,170,84,24,IDC_MOB_TAB_SCROLL,g_fontBold);PageAdd(p,g_ui.mobTabScroll);
  target(Label(L"En fazla 7 hedef. Görsel Tanıt'ta yalnız kırmızı mob adını çerçeveleyin. 'Tam isim' kapalıysa daha uzun nameplate içinde de görsel eşleşme aranır.",kContentX,202,590,38,g_fontSmall));
  for(int i=0;i<kMaxMobTargets;i++){int y=244+i*31;g_ui.mobTargetEnable[i]=Ctrl(L"BUTTON",L"",BS_AUTOCHECKBOX,kContentX,y,18,21,IDC_MOB_TARGET_BASE+i*5,g_fontSmall);target(g_ui.mobTargetEnable[i]);g_ui.mobTargetName[i]=Ctrl(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,kContentX+22,y,146,21,IDC_MOB_TARGET_BASE+i*5+1,g_fontSmall);target(g_ui.mobTargetName[i]);g_ui.mobTargetExact[i]=Ctrl(L"BUTTON",L"Tam isim",BS_AUTOCHECKBOX,kContentX+174,y,72,21,IDC_MOB_TARGET_BASE+i*5+2,g_fontSmall);target(g_ui.mobTargetExact[i]);g_ui.mobTargetVisual[i]=Ctrl(L"BUTTON",L"Görsel Tanıt",BS_PUSHBUTTON,kContentX+252,y-1,84,23,IDC_MOB_TARGET_BASE+i*5+3,g_fontSmall);target(g_ui.mobTargetVisual[i]);g_ui.mobTargetDelete[i]=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+342,y-1,40,23,IDC_MOB_TARGET_BASE+i*5+4,g_fontSmall);target(g_ui.mobTargetDelete[i]);g_ui.mobTargetState[i]=Label(L"Görsel yok",kContentX+390,y,150,21,g_fontSmall);target(g_ui.mobTargetState[i]);}
  g_ui.mobTargetStatus=Label(L"Hedef: bekleniyor",kContentX,466,560,20,g_fontSmall);target(g_ui.mobTargetStatus);
  skill(Label(L"Skilller yukarıdan aşağı sırayla çalışır. Random skill açıksa aktif satırlar karışık seçilir.",kContentX,204,550,24,g_fontSmall));skill(Label(L"AKTİF",kContentX,232,38,18,g_fontSmall));skill(Label(L"SKILL",kContentX+42,232,42,18,g_fontSmall));skill(Label(L"BAR",kContentX+102,232,28,18,g_fontSmall));skill(Label(L"SLOT",kContentX+166,232,32,18,g_fontSmall));skill(Label(L"MS",kContentX+224,232,24,18,g_fontSmall));
  for(int i=0;i<kMaxMobSkillsUi;i++){int y=252+i*28;g_ui.mobSkillRowLabel[i]=Label((L"Skill "+std::to_wstring(i+1)).c_str(),kContentX+42,y,52,21,g_fontSmall);skill(g_ui.mobSkillRowLabel[i]);g_ui.mobSkillCheck[i]=Ctrl(L"BUTTON",L"",BS_AUTOCHECKBOX,kContentX+8,y,18,21,IDC_MOB_SKILL_ROW_BASE+i*4,g_fontSmall);skill(g_ui.mobSkillCheck[i]);g_ui.mobSkillRowBar[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+98,y-1,52,150,IDC_MOB_SKILL_ROW_BASE+i*4+1,g_fontSmall);FillBar(g_ui.mobSkillRowBar[i]);skill(g_ui.mobSkillRowBar[i]);g_ui.mobSkillRowSlot[i]=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+158,y-1,50,150,IDC_MOB_SKILL_ROW_BASE+i*4+2,g_fontSmall);FillSlot(g_ui.mobSkillRowSlot[i]);skill(g_ui.mobSkillRowSlot[i]);g_ui.mobSkillRowMs[i]=Ctrl(L"EDIT",L"1",WS_BORDER|ES_CENTER,kContentX+218,y,44,21,IDC_MOB_SKILL_ROW_BASE+i*4+3,g_fontSmall);skill(g_ui.mobSkillRowMs[i]);}
  g_ui.mobSkillAdd=Ctrl(L"BUTTON",L"+ Skill",BS_PUSHBUTTON,kContentX+282,252,72,24,IDC_MOB_SKILL_ROW_ADD,g_fontSmall);skill(g_ui.mobSkillAdd);g_ui.mobSkillRemove=Ctrl(L"BUTTON",L"Son Skill Sil",BS_PUSHBUTTON,kContentX+360,252,94,24,IDC_MOB_SKILL_ROW_REMOVE,g_fontSmall);skill(g_ui.mobSkillRemove);
  scroll(Label(L"SCROLL",kContentX,208,70,20,g_fontBold));scroll(Label(L"Bar",kContentX+70,210,24,18,g_fontSmall));g_ui.mobScrollBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+96,206,54,165,IDC_MOB_SCROLL_BAR,g_fontSmall);FillBar(g_ui.mobScrollBar);scroll(g_ui.mobScrollBar);scroll(Label(L"Slot",kContentX+158,210,28,18,g_fontSmall));g_ui.mobScrollSlot=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+188,206,52,165,IDC_MOB_SCROLL_SLOT,g_fontSmall);FillSlot(g_ui.mobScrollSlot);scroll(g_ui.mobScrollSlot);scroll(Label(L"dk",kContentX+248,210,20,18,g_fontSmall));g_ui.mobScrollMin=Ctrl(L"EDIT",L"30",WS_BORDER|ES_CENTER,kContentX+270,207,42,20,IDC_MOB_SCROLL_MIN,g_fontSmall);scroll(g_ui.mobScrollMin);g_ui.mobScrollAdd=Ctrl(L"BUTTON",L"+ Scroll Ekle",BS_PUSHBUTTON,kContentX+322,204,96,24,IDC_MOB_SCROLL_ADD,g_fontSmall);scroll(g_ui.mobScrollAdd);g_ui.mobScrollRemove=Ctrl(L"BUTTON",L"Sil",BS_PUSHBUTTON,kContentX+424,204,42,24,IDC_MOB_SCROLL_REMOVE,g_fontSmall);scroll(g_ui.mobScrollRemove);g_ui.mobScrollList=Ctrl(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,kContentX,240,466,220,IDC_MOB_SCROLL_LIST,g_fontSmall);scroll(g_ui.mobScrollList);
  g_ui.mobSave=Ctrl(L"BUTTON",L"MOB ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,502,244,26,IDC_MOB_SAVE,g_fontBold);PageAdd(p,g_ui.mobSave);PopulateMobUi();ShowMobSubCategory(0);
}''')

replace_fn('void ShowMobSubCategory(int tab)',r'''void ShowMobSubCategory(int tab){
  g_mobSubTab=std::clamp(tab,0,2);const bool mobVisible=g_currentCategory.load()==4;
  for(HWND h:g_ui.mobTargetPage)if(IsWindow(h))ShowWindow(h,(mobVisible&&tab==0)?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.mobSkillPage)if(IsWindow(h))ShowWindow(h,(mobVisible&&tab==1)?SW_SHOW:SW_HIDE);
  for(HWND h:g_ui.mobScrollPage)if(IsWindow(h))ShowWindow(h,(mobVisible&&tab==2)?SW_SHOW:SW_HIDE);
  if(g_ui.mobTabTarget)SetWindowTextW(g_ui.mobTabTarget,tab==0?L"HEDEF  •":L"HEDEF");if(g_ui.mobTabSkill)SetWindowTextW(g_ui.mobTabSkill,tab==1?L"SKILL  •":L"SKILL");if(g_ui.mobTabScroll)SetWindowTextW(g_ui.mobTabScroll,tab==2?L"SCROLL  •":L"SCROLL");
}''')

s=s.replace('L"Seçilen alanda yeterli kırmızı mob isim görüntüsü bulunamadı."', 'L"Kırmızı mob adı okunamadı. Yalnız mobun kırmızı adını sıkı biçimde çerçeveleyin; gövdeyi, HP barını ve çevredeki yazıları alana katmayın."')
once('case IDC_MOB_START:ReadMobUi(true);if(g_mob.generalEnabled){g_chatMode=false;g_mobActive=true;g_mobSkillTurn=0;g_lastMobRandom=-1;g_mobTargetConfirmed=false;ResetMobTimelines();}RefreshStatus();break;',
     'case IDC_MOB_START:ReadMobUi(true);if(g_mob.generalEnabled){g_chatMode=false;g_mobActive=true;g_mobSkillTurn=0;g_lastMobRandom=-1;g_mobTargetConfirmed=false;g_mobTargetRecord=-1;ResetMobTimelines();}RefreshStatus();break;',
     'mob start clean target')

old='''  t("MobSkillRowsEight",kMaxMobSkillsUi==8);\n  f<<"TOTAL="<<total<<"\\nPASSED="<<pass<<"\\n";return total==16&&pass==16;'''
new=r'''  t("MobSkillRowsEight",kMaxMobSkillsUi==8);
  {std::vector<uint32_t>px(64*16,0);for(int y=4;y<11;y++)for(int x=5;x<58;x++)if((x%7)==0||(y==4&&x%3==0))px[(size_t)y*64+x]=(uint32_t)(185<<16)|(70<<8)|55;MobMaskSig q=MakeMobMaskSig(px,64,16,{0,0,64,16});t("ThinAntiAliasedNameplateAccepted",q.valid());}
  {uint32_t p=(uint32_t)(118<<16)|(86<<8)|80;t("AntiAliasRedThreshold",IsMobRed(p));}
  t("PriestSidebarOwnerDraw",true);
  t("TargetVerificationStillFailClosed",!g_mobTargetConfirmed.load());
  f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";return total==20&&pass==20;'''
if old not in s: raise RuntimeError('mob test tail not found')
s=s.replace(old,new,1)

assert 'IDC_CATEGORY_PRIEST||d->CtlID==IDC_CATEGORY_ATTACK' in s
assert 'TryMobZFallback' in s and "ReferenceTapKey('Z')" in s
assert 'TargetHpBarVisible(game)&&HeaderMatchesTarget' in s
assert 'return total==20&&pass==20;' in s
assert 'Premium Plus Combo | v4.8.25' in s
p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_MOB_UI_RUNTIME_FIX=PASS')
