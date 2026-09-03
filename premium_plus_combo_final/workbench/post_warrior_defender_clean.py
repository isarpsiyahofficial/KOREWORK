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

# Keep Warrior functionality but avoid cursor-position APIs.
replace_fn('bool WarriorRightClickSlot(HWND game,int slot)', r'''bool WarriorRightClickSlot(HWND game,int slot){
  if(!game||GetForegroundWindow()!=game)return false;
  InventoryGrid grid{};if(!WarriorResolveInventory(game,grid))return false;
  if(GetForegroundWindow()!=game)return false;
  POINT pt=WarriorSlotCellCenter(grid,slot);
  const int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if(vw<2||vh<2)return false;
  INPUT mv{},d{},u{};
  mv.type=INPUT_MOUSE;
  mv.mi.dx=(LONG)std::clamp<long long>(((long long)(pt.x-vx)*65535)/(vw-1),0,65535);
  mv.mi.dy=(LONG)std::clamp<long long>(((long long)(pt.y-vy)*65535)/(vh-1),0,65535);
  mv.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
  mv.mi.dwExtraInfo=kMagicInput;
  d.type=INPUT_MOUSE;d.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;d.mi.dwExtraInfo=kMagicInput;
  u=d;u.mi.dwFlags=MOUSEEVENTF_RIGHTUP;
  FifoTicketGuard gate(g_gameInputGate);
  if(SendInput(1,&mv,sizeof(INPUT))!=1)return false;
  PreciseDelayUs(1200);
  if(SendInput(1,&d,sizeof(INPUT))!=1)return false;
  PreciseDelayUs(4200);
  if(SendInput(1,&u,sizeof(INPUT))!=1)return false;
  return true;
}''')

# The optional MOB position mapping has no producer. Keep it fail-closed without
# stripping any of the known-good self-test/observer code from the executable.
replace_fn('bool ReadPosition(double&x,double&z)', 'bool ReadPosition(double&,double&){return false;}')
s=s.replace('ClosePositionBridge();CloseBridge();','CloseBridge();',1)

# Preserve the compact UI but do not add Windows APIs absent from the known-good
# v4.8.11 binary. Font propagation is done through HWNDs already tracked by Ui.
replace_fn('void RebuildUiFonts(UINT dpi)', r'''void RebuildUiFonts(UINT dpi){
  if(!dpi)dpi=96;
  HFONT on=g_font,ob=g_fontBold,os=g_fontSmall;
  auto scaled=[&](int px){return -std::max(1,(px*(int)dpi+48)/96);};
  HFONT nn=CreateFontW(scaled(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  HFONT nb=CreateFontW(scaled(14),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  HFONT ns=CreateFontW(scaled(11),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,TURKISH_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
  if(!nn||!nb||!ns){if(nn)DeleteObject(nn);if(nb)DeleteObject(nb);if(ns)DeleteObject(ns);return;}
  auto apply=[&](HWND h){
    if(!h||!IsWindow(h))return;
    HFONT f=(HFONT)SendMessageW(h,WM_GETFONT,0,0);
    HFONT n=(f==ob)?nb:((f==os)?ns:nn);
    SendMessageW(h,WM_SETFONT,(WPARAM)n,TRUE);
  };
  if(g_ui.main){
    apply(g_ui.power);apply(g_ui.catRogue);apply(g_ui.catAttack);apply(g_ui.catMob);apply(g_ui.catWarrior);apply(g_ui.status);
    for(HWND h:g_ui.roguePage)apply(h);
    for(HWND h:g_ui.attackPage)apply(h);
    for(HWND h:g_ui.mobPage)apply(h);
    for(HWND h:g_ui.warriorPage)apply(h);
  }
  g_font=nn;g_fontBold=nb;g_fontSmall=ns;
  if(on)DeleteObject(on);if(ob)DeleteObject(ob);if(os)DeleteObject(os);
}''')

# MoveWindow is already part of the known-good executable. It replaces the new
# SetWindowPos dependency while honoring the WM_DPICHANGED suggested rectangle.
old='if(rr)SetWindowPos(h,nullptr,rr->left,rr->top,rr->right-rr->left,rr->bottom-rr->top,SWP_NOZORDER|SWP_NOACTIVATE);'
if s.count(old)!=1: raise RuntimeError('WM_DPICHANGED SetWindowPos anchor missing')
s=s.replace(old,'if(rr)MoveWindow(h,rr->left,rr->top,rr->right-rr->left,rr->bottom-rr->top,TRUE);',1)

# Start with compact 96-DPI fonts. Per-monitor DPI changes still rescale through
# WM_DPICHANGED without importing GetDpiForSystem.
old='RebuildUiFonts(GetDpiForSystem());'
if s.count(old)!=1: raise RuntimeError('GetDpiForSystem anchor missing')
s=s.replace(old,'RebuildUiFonts(96);',1)

s=s.replace('constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.16";','constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.17";',1)

# Hard guards.
a,b=span('bool WarriorRightClickSlot'); body=s[a:b]
for bad in ('GetCursorPos','SetCursorPos','MOUSEEVENTF_LEFTDOWN','MOUSEEVENTF_LEFTUP','mouse_event('):
    if bad in body: raise RuntimeError('forbidden Warrior API/action remains: '+bad)
for good in ('MOUSEEVENTF_MOVE','MOUSEEVENTF_ABSOLUTE','MOUSEEVENTF_RIGHTDOWN','MOUSEEVENTF_RIGHTUP','SendInput'):
    if good not in body: raise RuntimeError('required Warrior action missing: '+good)
if 'bool ReadPosition(double&,double&){return false;}' not in s:
    raise RuntimeError('fail-closed position stub missing')
for bad in ('EnumChildWindows(','GetDpiForSystem(','SetWindowPos(','MulDiv('):
    if bad in s: raise RuntimeError('known-good import compatibility violation: '+bad)

p.write_text(s,encoding='utf-8',newline='\n')
print('WARRIOR_DEFENDER_CLEAN_PATCH=PASS')
print('UNUSED_POSITION_MAPPING_REMOVED=PASS')
print('KNOWN_GOOD_IMPORT_COMPAT_UI=PASS')
