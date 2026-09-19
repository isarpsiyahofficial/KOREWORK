#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <fstream>
#include <thread>
#include <vector>
#include <algorithm>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
struct Trial{bool down=false,release=false;};
int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
  WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName=L"PPC_DINPUT_SWEEP";if(!RegisterClassW(&wc))return 2;
  HWND h=CreateWindowW(wc.lpszClassName,L"DirectInput Hold Sweep",WS_OVERLAPPEDWINDOW,50,50,320,180,nullptr,nullptr,hi,nullptr);if(!h)return 3;
  ShowWindow(h,SW_SHOW);SetForegroundWindow(h);SetActiveWindow(h);SetFocus(h);
  IDirectInput8W* di=nullptr;IDirectInputDevice8W* kb=nullptr;if(FAILED(DirectInput8Create(hi,DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&di,nullptr))||!di)return 4;
  if(FAILED(di->CreateDevice(GUID_SysKeyboard,&kb,nullptr))||!kb){di->Release();return 5;}
  kb->SetDataFormat(&c_dfDIKeyboard);kb->SetCooperativeLevel(h,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND|DISCL_NOWINKEY);HRESULT ac=kb->Acquire();if(FAILED(ac)&&ac!=S_FALSE)return 6;
  UINT scan=MapVirtualKeyW('8',MAPVK_VK_TO_VSC_EX)&0xffu;
  auto pump=[&](){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}};
  auto one=[&](int holdMs,int pollMs,int phase)->Trial{
    Trial r{};BYTE st[256]{};
    ULONGLONG quiet=GetTickCount64()+50;while(GetTickCount64()<quiet){pump();kb->GetDeviceState(sizeof(st),st);Sleep(2);}
    std::thread send([&]{Sleep(80+phase);INPUT d{},u{};d.type=u.type=INPUT_KEYBOARD;d.ki.wScan=u.ki.wScan=(WORD)scan;d.ki.dwFlags=KEYEVENTF_SCANCODE;u.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;SendInput(1,&d,sizeof(INPUT));Sleep(holdMs);SendInput(1,&u,sizeof(INPUT));});
    ULONGLONG end=GetTickCount64()+80+phase+holdMs+pollMs*4+60;
    while(GetTickCount64()<end){
      pump();HRESULT g=kb->GetDeviceState(sizeof(st),st);if(g==DIERR_INPUTLOST||g==DIERR_NOTACQUIRED){kb->Acquire();Sleep(pollMs);continue;}
      if(SUCCEEDED(g)){bool d=(st[scan]&0x80)!=0;if(d)r.down=true;if(r.down&&!d)r.release=true;}
      Sleep(pollMs);
    }
    send.join();return r;
  };
  std::vector<int> holds{10,12,15,18,20,24,28,32,36,40,45,50,60,75,100};
  std::vector<int> polls{16,33};
  std::ofstream f("directinput-hold-sweep-report.txt",std::ios::trunc);
  int min16=-1,min33=-1;
  for(int poll:polls){
    for(int hold:holds){
      int pass=0,rel=0,tries=6;
      for(int i=0;i<tries;i++){int phase=(i*poll)/tries;Trial t=one(hold,poll,phase);if(t.down)pass++;if(t.release)rel++;}
      f<<"Poll"<<poll<<"_Hold"<<hold<<"_Down="<<pass<<"/"<<tries<<" Release="<<rel<<"/"<<tries<<"\n";
      if(pass==tries&&rel==tries){if(poll==16&&min16<0)min16=hold;if(poll==33&&min33<0)min33=hold;}
    }
  }
  f<<"MinAllPassPoll16Ms="<<min16<<"\nMinAllPassPoll33Ms="<<min33<<"\n";
  bool ok=min16>0&&min33>0;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";
  kb->Unacquire();kb->Release();di->Release();DestroyWindow(h);return ok?0:10;
}
