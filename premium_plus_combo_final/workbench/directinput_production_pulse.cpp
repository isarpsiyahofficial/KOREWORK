#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <fstream>
#include <thread>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
struct Trial{bool down=false,release=false;};
int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
  WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName=L"PPC_DINPUT_PRODUCTION";if(!RegisterClassW(&wc))return 2;
  HWND h=CreateWindowW(wc.lpszClassName,L"DirectInput Production Pulse",WS_OVERLAPPEDWINDOW,50,50,320,180,nullptr,nullptr,hi,nullptr);if(!h)return 3;
  ShowWindow(h,SW_SHOW);SetForegroundWindow(h);SetActiveWindow(h);SetFocus(h);
  IDirectInput8W* di=nullptr;IDirectInputDevice8W* kb=nullptr;if(FAILED(DirectInput8Create(hi,DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&di,nullptr))||!di)return 4;
  if(FAILED(di->CreateDevice(GUID_SysKeyboard,&kb,nullptr))||!kb){di->Release();return 5;}
  kb->SetDataFormat(&c_dfDIKeyboard);kb->SetCooperativeLevel(h,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND|DISCL_NOWINKEY);HRESULT ac=kb->Acquire();bool acquired=SUCCEEDED(ac)||ac==S_FALSE;
  UINT scan=MapVirtualKeyW('8',MAPVK_VK_TO_VSC_EX)&0xffu;
  auto pump=[&](){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}};
  auto one=[&](int pollMs,int phase)->Trial{
    Trial r{};BYTE st[256]{};ULONGLONG quiet=GetTickCount64()+45;while(GetTickCount64()<quiet){pump();kb->GetDeviceState(sizeof(st),st);Sleep(2);}
    std::thread send([&]{Sleep(70+phase);INPUT d{},u{};d.type=u.type=INPUT_KEYBOARD;d.ki.wScan=u.ki.wScan=(WORD)scan;d.ki.dwFlags=KEYEVENTF_SCANCODE;u.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;SendInput(1,&d,sizeof(INPUT));Sleep(50);SendInput(1,&u,sizeof(INPUT));});
    ULONGLONG end=GetTickCount64()+70+phase+50+pollMs*4+50;
    while(GetTickCount64()<end){pump();HRESULT g=kb->GetDeviceState(sizeof(st),st);if(g==DIERR_INPUTLOST||g==DIERR_NOTACQUIRED){kb->Acquire();Sleep(pollMs);continue;}if(SUCCEEDED(g)){bool d=(st[scan]&0x80)!=0;if(d)r.down=true;if(r.down&&!d)r.release=true;}Sleep(pollMs);}
    send.join();return r;
  };
  int pass16=0,pass33=0,tries=6;
  for(int i=0;i<tries;i++){auto t=one(16,(i*16)/tries);if(t.down&&t.release)pass16++;}
  for(int i=0;i<tries;i++){auto t=one(33,(i*33)/tries);if(t.down&&t.release)pass33++;}
  std::ofstream f("directinput-production-pulse-report.txt",std::ios::trunc);
  f<<"Acquired="<<(acquired?"PASS":"FAIL")<<"\nHoldMs=50\nPoll16Pass="<<pass16<<"/"<<tries<<"\nPoll33Pass="<<pass33<<"/"<<tries<<"\n";
  bool ok=acquired&&pass16==tries&&pass33==tries;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";
  kb->Unacquire();kb->Release();di->Release();DestroyWindow(h);return ok?0:10;
}
