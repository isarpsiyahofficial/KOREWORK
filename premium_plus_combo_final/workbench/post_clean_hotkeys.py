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

def replace_fn(sig,new):
    global s
    a,b=span(sig); s=s[:a]+new+s[b:]

def once(old,new,label):
    global s
    c=s.count(old)
    if c!=1: raise RuntimeError(f'{label}: expected 1 got {c}')
    s=s.replace(old,new,1)

# Version identity.
once('constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.17";', 'constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.18";', 'title')
once('constexpr UINT WM_APP_CAL_DONE = WM_APP + 3;', 'constexpr UINT WM_APP_CAL_DONE = WM_APP + 3;\nconstexpr int kOperationalHotkeyBase = 0x5100;', 'hotkey id base')

# Production runtime no longer keeps a WH_KEYBOARD_LL hook handle. Observer hooks used only
# by the diagnostic executable remain separate and are dead-stripped from production.
once('HHOOK g_keyboardHook{};', 'std::array<bool,256> g_registeredHotkeys{};\nstd::atomic<int> g_hotkeyBindFailures{0};', 'runtime hook state')

# Replace the invasive global keyboard hook with standard RegisterHotKey dispatch plus
# focused-window key assignment. This monitors only configured operational keys.
replace_fn('LRESULT CALLBACK KeyboardProc', r'''void UnregisterOperationalHotkeys(){
  if(!g_ui.main)return;
  for(int vk=1;vk<256;vk++)if(g_registeredHotkeys[(size_t)vk]){
    UnregisterHotKey(g_ui.main,kOperationalHotkeyBase+vk);
    g_registeredHotkeys[(size_t)vk]=false;
  }
  g_hotkeyBindFailures=0;
}

bool OperationalHotkeySupported(int vk){
  return vk>0&&vk<256&&vk!=VK_LBUTTON&&vk!=VK_RBUTTON&&vk!=VK_F12;
}

void RebindOperationalHotkeys(){
  if(!g_ui.main)return;
  UnregisterOperationalHotkeys();
  RogueSettings r;AttackSettings a;MobSettings m;WarriorSettings w;
  {std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;w=g_warrior;}
  if(!r.powerEnabled)return;
  std::array<bool,256>wanted{};
  auto add=[&](int vk){if(OperationalHotkeySupported(vk))wanted[(size_t)vk]=true;};
  if(g_rogueCategoryEnabled.load()){add(r.startHotkey);add(r.stopHotkey);if(r.cureEnabled)add(r.cureHotkey);}
  if(g_attackCategoryEnabled.load()){add(a.startHotkey);add(a.stopHotkey);}
  if(m.generalEnabled||m.priestEnabled){add(m.startHotkey);add(m.stopHotkey);}
  if(w.enabled){add(w.shieldHotkey);add(w.weaponHotkey);}
  int failures=0;
  for(int vk=1;vk<256;vk++)if(wanted[(size_t)vk]){
    if(RegisterHotKey(g_ui.main,kOperationalHotkeyBase+vk,MOD_NOREPEAT,(UINT)vk))g_registeredHotkeys[(size_t)vk]=true;
    else failures++;
  }
  g_hotkeyBindFailures=failures;
}

void BeginHotkeyAssignment(int target,HWND button){
  g_assignTarget=target;
  UnregisterOperationalHotkeys();
  if(button)SetWindowTextW(button,L"Bir tuşa bas...");
  if(g_ui.main)SetFocus(g_ui.main);
}

bool AssignHotkeyFromUi(int vk){
  int t=g_assignTarget.load();if(!t)return false;
  if(vk==VK_ESCAPE){g_assignTarget=0;RebindOperationalHotkeys();RefreshHotkeyLabels();return true;}
  if(!OperationalHotkeySupported(vk)){MessageBeep(MB_ICONWARNING);return true;}
  RogueSettings cr;AttackSettings ca;MobSettings cm;WarriorSettings cw;
  {std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;cm=g_mob;cw=g_warrior;}
  bool allowed=true;
  if(t<=5)allowed=!ConflictWithRogue(vk,t);
  else{
    allowed=vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey&&vk!=cm.startHotkey&&vk!=cm.stopHotkey;
    if(t==8)allowed=allowed&&vk!=cw.weaponHotkey;
    if(t==9)allowed=allowed&&vk!=cw.shieldHotkey;
  }
  if(!allowed){MessageBeep(MB_ICONWARNING);return true;}
  {
    std::lock_guard<std::mutex>lk(g_settingsMutex);
    if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;
    else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;
    else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;
    else if(t==8)g_warrior.shieldHotkey=vk;else if(t==9)g_warrior.weaponHotkey=vk;
  }
  g_assignTarget=0;
  RebindOperationalHotkeys();
  PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);
  return true;
}

void HandleOperationalHotkey(int vk){
  if(vk<=0||vk>=256)return;
  HWND fg=GetForegroundWindow();
  if(!fg||IsOurUiWindow(fg))return;
  RogueSettings r;AttackSettings a;MobSettings m;WarriorSettings ww;
  {std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;m=g_mob;ww=g_warrior;}
  if(!r.powerEnabled||g_chatMode.load())return;

  if(ww.enabled&&(vk==ww.shieldHotkey||vk==ww.weaponHotkey)){
    RememberGameWindow();g_warriorRequestWindow=(ULONG_PTR)fg;g_warriorRequest=(vk==ww.shieldHotkey)?1:2;
    if(g_warriorEvent)SetEvent(g_warriorEvent);return;
  }
  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey&&vk==m.stopHotkey){
    RememberGameWindow();bool on=!g_mobActive.load();g_mobActive=on;if(on){g_mobSkillTurn=0;g_lastMobRandom=-1;ResetMobTimelines();}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;
  }
  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey){RememberGameWindow();if(!g_mobActive.exchange(true)){g_mobSkillTurn=0;g_lastMobRandom=-1;ResetMobTimelines();}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if((m.generalEnabled||m.priestEnabled)&&vk==m.stopHotkey){g_mobActive=false;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}

  if(g_rogueCategoryEnabled&&vk==r.startHotkey){RememberGameWindow();g_autoMinorOwned=false;g_minorActive=true;PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if(g_rogueCategoryEnabled&&vk==r.stopHotkey){g_autoMinorOwned=false;g_minorActive=false;ReleaseKeys();PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if(g_attackCategoryEnabled&&vk==a.startHotkey&&vk==a.stopHotkey){RememberGameWindow();bool on=!g_attackActive.load();g_attackActive=on;if(on){g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if(g_attackCategoryEnabled&&vk==a.startHotkey){RememberGameWindow();if(!g_attackActive.exchange(true)){g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;}PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if(g_attackCategoryEnabled&&vk==a.stopHotkey){g_attackActive=false;ReleaseKeys();PostMessageW(g_ui.main,WM_APP_REFRESH,0,0);return;}
  if(g_rogueCategoryEnabled&&r.cureEnabled&&vk==r.cureHotkey){RememberGameWindow();bool expected=false;if(g_cureExclusive.compare_exchange_strong(expected,true)){g_curePending=true;if(g_cureEvent)SetEvent(g_cureEvent);}return;}
}''')

# Assignment buttons put focus on the main window and temporarily release registered hotkeys,
# so the selected key is read through normal WM_KEYDOWN rather than a global keyboard monitor.
assignment_replacements={
'case IDC_MINOR_START_ASSIGN:g_assignTarget=1;SetWindowTextW(g_ui.minorStartAssign,L"Bir tuşa bas...");break;':'case IDC_MINOR_START_ASSIGN:BeginHotkeyAssignment(1,g_ui.minorStartAssign);break;',
'case IDC_MINOR_STOP_ASSIGN:g_assignTarget=2;SetWindowTextW(g_ui.minorStopAssign,L"Bir tuşa bas...");break;':'case IDC_MINOR_STOP_ASSIGN:BeginHotkeyAssignment(2,g_ui.minorStopAssign);break;',
'case IDC_CURE_ASSIGN:g_assignTarget=3;SetWindowTextW(g_ui.cureAssign,L"Bir tuşa bas...");break;':'case IDC_CURE_ASSIGN:BeginHotkeyAssignment(3,g_ui.cureAssign);break;',
'case IDC_ATTACK_START_ASSIGN:g_assignTarget=4;SetWindowTextW(g_ui.attackStartAssign,L"Bir tuşa bas...");break;':'case IDC_ATTACK_START_ASSIGN:BeginHotkeyAssignment(4,g_ui.attackStartAssign);break;',
'case IDC_ATTACK_STOP_ASSIGN:g_assignTarget=5;SetWindowTextW(g_ui.attackStopAssign,L"Bir tuşa bas...");break;':'case IDC_ATTACK_STOP_ASSIGN:BeginHotkeyAssignment(5,g_ui.attackStopAssign);break;',
'case IDC_MOB_START_ASSIGN:g_assignTarget=6;SetWindowTextW(g_ui.mobStartAssign,L"Bir tuşa bas...");break;':'case IDC_MOB_START_ASSIGN:BeginHotkeyAssignment(6,g_ui.mobStartAssign);break;',
'case IDC_MOB_STOP_ASSIGN:g_assignTarget=7;SetWindowTextW(g_ui.mobStopAssign,L"Bir tuşa bas...");break;':'case IDC_MOB_STOP_ASSIGN:BeginHotkeyAssignment(7,g_ui.mobStopAssign);break;',
'case IDC_WARRIOR_SHIELD_KEY:g_assignTarget=8;SetWindowTextW(g_ui.warriorShieldKey,L"Bir tuşa bas...");break;':'case IDC_WARRIOR_SHIELD_KEY:BeginHotkeyAssignment(8,g_ui.warriorShieldKey);break;',
'case IDC_WARRIOR_WEAPON_KEY:g_assignTarget=9;SetWindowTextW(g_ui.warriorWeaponKey,L"Bir tuşa bas...");break;':'case IDC_WARRIOR_WEAPON_KEY:BeginHotkeyAssignment(9,g_ui.warriorWeaponKey);break;'
}
for old,new in assignment_replacements.items(): once(old,new,'assignment '+old[:30])

# WndProc receives only configured WM_HOTKEY events. Key assignment is local to the app window.
a,b=span('LRESULT CALLBACK WndProc'); fn=s[a:b]
needle='switch(m){case WM_CREATE:'
if needle not in fn: raise RuntimeError('WndProc switch anchor')
insert='''switch(m){case WM_KEYDOWN:case WM_SYSKEYDOWN:if(g_assignTarget.load()){if((((LPARAM)l)&(1LL<<30))==0)AssignHotkeyFromUi((int)(w&0xff));return 0;}break;case WM_HOTKEY:if((int)w>=kOperationalHotkeyBase&&(int)w<kOperationalHotkeyBase+256){HandleOperationalHotkey((int)(HIWORD(l)&0xff));return 0;}break;case WM_CREATE:'''
fn=fn.replace(needle,insert,1)
# Rebind whenever a runtime enable state changes.
fn=fn.replace('case IDC_POWER:{bool on;{std::lock_guard<std::mutex>lk(g_settingsMutex);on=!g_rogue.powerEnabled;}SetPower(on);break;}', 'case IDC_POWER:{bool on;{std::lock_guard<std::mutex>lk(g_settingsMutex);on=!g_rogue.powerEnabled;}SetPower(on);RebindOperationalHotkeys();break;}',1)
fn=fn.replace('case IDC_CURE_CHECK:ReadRogueUi(false);break;', 'case IDC_CURE_CHECK:ReadRogueUi(false);RebindOperationalHotkeys();break;',1)
# category enable branches already contain full state handling; add a rebind just before break.
fn=fn.replace('RefreshStatus();break;}case IDC_ATTACK_CATEGORY_ENABLE:', 'RefreshStatus();RebindOperationalHotkeys();break;}case IDC_ATTACK_CATEGORY_ENABLE:',1)
fn=fn.replace('RefreshStatus();break;}default:break;}return 0;}case WM_APP_ASSIGN_DONE:', 'RefreshStatus();RebindOperationalHotkeys();break;}default:break;}return 0;}case WM_APP_ASSIGN_DONE:',1)
# MOB combined enable branch and Warrior enable branch.
fn=fn.replace('ReadMobUi(true);if(!g_mob.generalEnabled&&!g_mob.priestEnabled)g_mobActive=false;RefreshStatus();break;', 'ReadMobUi(true);if(!g_mob.generalEnabled&&!g_mob.priestEnabled)g_mobActive=false;RefreshStatus();RebindOperationalHotkeys();break;',1)
fn=fn.replace('case IDC_WARRIOR_ENABLE:ReadWarriorUi(true);RefreshStatus();break;', 'case IDC_WARRIOR_ENABLE:ReadWarriorUi(true);RefreshStatus();RebindOperationalHotkeys();break;',1)
s=s[:a]+fn+s[b:]

# Status makes registration failures visible instead of silently swallowing a configured key.
a,b=span('void RefreshStatus()'); fn=s[a:b]
statusneedle='SetWindowTextW(g_ui.status,s.c_str());'
if statusneedle not in fn: raise RuntimeError('status anchor')
fn=fn.replace(statusneedle,'if(g_hotkeyBindFailures.load()>0)s+=L"   |   Hotkey kayıt hatası: "+std::to_wstring(g_hotkeyBindFailures.load());'+statusneedle,1)
s=s[:a]+fn+s[b:]

# Remove the runtime SetWindowsHookExW installation and cleanup. Keep diagnostic observer hooks.
a,b=span('int APIENTRY wWinMain'); fn=s[a:b]
hs=fn.find('g_keyboardHook=SetWindowsHookExW(')
ts=fn.find('std::thread tMinor',hs)
if hs<0 or ts<0: raise RuntimeError('runtime hook startup anchor')
fn=fn[:hs]+'RebindOperationalHotkeys();'+fn[ts:]
fn=fn.replace('if(g_keyboardHook)UnhookWindowsHookEx(g_keyboardHook);','UnregisterOperationalHotkeys();',1)
s=s[:a]+fn+s[b:]

# Source guardrails for the production runtime architecture.
if 'LRESULT CALLBACK KeyboardProc' in s: raise RuntimeError('runtime KeyboardProc remains')
a,b=span('int APIENTRY wWinMain'); main=s[a:b]
for bad in ('SetWindowsHookExW(WH_KEYBOARD_LL,KeyboardProc','g_keyboardHook'):
    if bad in main: raise RuntimeError('runtime low-level hook remains: '+bad)
for good in ('RebindOperationalHotkeys()','UnregisterOperationalHotkeys()'):
    if good not in main: raise RuntimeError('runtime hotkey lifecycle missing: '+good)
if 'RegisterHotKey(g_ui.main' not in s or 'UnregisterHotKey(g_ui.main' not in s: raise RuntimeError('standard hotkey API missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('CLEAN_HOTKEY_ARCHITECTURE=PASS')
