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

# Keep Warrior functionality but avoid adding cursor APIs that were absent from the clean v4.8.15 binary.
# Mouse movement and right-click both use the already-existing SendInput transport.
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

s=s.replace('constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.16";','constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.17";',1)

# Hard guards: final Warrior source must not use cursor-position APIs, drag, or left-click.
a,b=span('bool WarriorRightClickSlot'); body=s[a:b]
for bad in ('GetCursorPos','SetCursorPos','MOUSEEVENTF_LEFTDOWN','MOUSEEVENTF_LEFTUP','mouse_event('):
    if bad in body: raise RuntimeError('forbidden Warrior API/action remains: '+bad)
for good in ('MOUSEEVENTF_MOVE','MOUSEEVENTF_ABSOLUTE','MOUSEEVENTF_RIGHTDOWN','MOUSEEVENTF_RIGHTUP','SendInput'):
    if good not in body: raise RuntimeError('required Warrior action missing: '+good)

p.write_text(s,encoding='utf-8',newline='\n')
print('WARRIOR_DEFENDER_CLEAN_PATCH=PASS')
