#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <array>
#include <fstream>
#include <string>
#include <algorithm>

struct Zone { RECT r; int id; const wchar_t* name; };
static std::array<Zone,7> g_zones{{
  {{40,80,180,140},1,L"Pride"},{{220,80,360,140},2,L"Gluttony"},{{400,80,540,140},3,L"Wrath"},
  {{40,200,180,260},4,L"Sloth"},{{220,200,360,260},5,L"Lust"},{{400,200,540,260},6,L"Envy"},{{580,200,720,260},7,L"Greed"}
}};
static volatile LONG g_selected=0;

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_LBUTTONDOWN){
    POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};int sel=0;
    for(auto &z:g_zones)if(PtInRect(&z.r,p)){sel=z.id;break;}
    InterlockedExchange(&g_selected,sel);InvalidateRect(h,nullptr,TRUE);return 0;
  }
  if(m==WM_PAINT){
    PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);SetBkMode(dc,TRANSPARENT);
    HBRUSH bg=CreateSolidBrush(RGB(30,30,30));FillRect(dc,&ps.rcPaint,bg);DeleteObject(bg);
    for(auto &z:g_zones){Rectangle(dc,z.r.left,z.r.top,z.r.right,z.r.bottom);SetTextColor(dc,RGB(230,30,30));TextOutW(dc,z.r.left+8,z.r.top+18,z.name,(int)wcslen(z.name));}
    EndPaint(h,&ps);return 0;
  }
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
  return DefWindowProcW(h,m,w,l);
}

static void Pump(DWORD ms){
  DWORD end=GetTickCount()+ms;MSG msg{};
  while((LONG)(GetTickCount()-end)<0){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(1);}
}

static bool ClickScreen(int sx,int sy){
  int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
  INPUT a[3]{};
  a[0].type=INPUT_MOUSE;a[0].mi.dx=(LONG)(((long long)(sx-vx)*65535)/std::max(1,vw-1));a[0].mi.dy=(LONG)(((long long)(sy-vy)*65535)/std::max(1,vh-1));a[0].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
  a[1].type=INPUT_MOUSE;a[1].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;a[2].type=INPUT_MOUSE;a[2].mi.dwFlags=MOUSEEVENTF_LEFTUP;
  return SendInput(3,a,sizeof(INPUT))==3;
}

static bool ClickZone(HWND h,int id){
  for(auto &z:g_zones)if(z.id==id){
    POINT p{(z.r.left+z.r.right)/2,(z.r.top+z.r.bottom)/2};ClientToScreen(h,&p);InterlockedExchange(&g_selected,0);
    if(!ClickScreen(p.x,p.y))return false;Pump(90);return InterlockedCompareExchange(&g_selected,0,0)==id;
  }
  return false;
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
  WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName=L"MobSelectorManualClickTest";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
  HWND h=CreateWindowW(wc.lpszClassName,L"Mob selector manual click scenario",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,820,420,nullptr,nullptr,hi,nullptr);if(!h)return 2;
  SetForegroundWindow(h);Pump(150);
  std::ofstream f("mob-selector-manual-click-report.txt",std::ios::trunc);int pass=0,total=0;
  auto T=[&](const char*n,bool ok){total++;if(ok)pass++;f<<n<<"="<<(ok?"PASS":"FAIL")<<"\n";};
  const char* names[7]={"Pride","Gluttony","Wrath","Sloth","Lust","Envy","Greed"};
  for(int i=1;i<=7;i++){std::string n="ManualClick_"+std::string(names[i-1]);T(n.c_str(),ClickZone(h,i));}
  bool wrong=ClickZone(h,2);bool wrongObserved=InterlockedCompareExchange(&g_selected,0,0)==2;bool right=ClickZone(h,1);
  T("WrongFirstThenCorrectRetry",wrong&&wrongObserved&&right&&InterlockedCompareExchange(&g_selected,0,0)==1);
  POINT q{770,330};ClientToScreen(h,&q);InterlockedExchange(&g_selected,0);bool sent=ClickScreen(q.x,q.y);Pump(80);
  T("MissDoesNotSelectMob",sent&&InterlockedCompareExchange(&g_selected,0,0)==0);
  f<<"TOTAL="<<total<<"\nPASSED="<<pass<<"\n";DestroyWindow(h);Pump(20);return pass==total?0:1;
}
