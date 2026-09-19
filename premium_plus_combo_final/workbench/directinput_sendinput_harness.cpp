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

LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
  return DefWindowProcW(h,m,w,l);
}
int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
  WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName=L"PPC_DINPUT_HARNESS";
  if(!RegisterClassW(&wc))return 2;
  HWND h=CreateWindowW(wc.lpszClassName,L"DirectInput Harness",WS_OVERLAPPEDWINDOW,50,50,320,180,nullptr,nullptr,hi,nullptr);
  if(!h)return 3;
  ShowWindow(h,SW_SHOW);SetForegroundWindow(h);SetActiveWindow(h);SetFocus(h);
  IDirectInput8W* di=nullptr;IDirectInputDevice8W* kb=nullptr;
  HRESULT hr=DirectInput8Create(hi,DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&di,nullptr);
  if(FAILED(hr)||!di)return 4;
  hr=di->CreateDevice(GUID_SysKeyboard,&kb,nullptr);if(FAILED(hr)||!kb){di->Release();return 5;}
  kb->SetDataFormat(&c_dfDIKeyboard);
  kb->SetCooperativeLevel(h,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND|DISCL_NOWINKEY);
  hr=kb->Acquire();
  bool acquired=SUCCEEDED(hr)||hr==S_FALSE;
  UINT scan=MapVirtualKeyW('8',MAPVK_VK_TO_VSC_EX)&0xFFu;
  std::thread sender([&]{
    Sleep(250);
    INPUT d{},u{};d.type=u.type=INPUT_KEYBOARD;d.ki.wScan=u.ki.wScan=(WORD)scan;d.ki.dwFlags=KEYEVENTF_SCANCODE;u.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
    SendInput(1,&d,sizeof(INPUT));Sleep(15);SendInput(1,&u,sizeof(INPUT));
  });
  bool sawDown=false,sawRelease=false;BYTE state[256]{};ULONGLONG end=GetTickCount64()+2000;
  while(GetTickCount64()<end){
    MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    HRESULT g=kb->GetDeviceState(sizeof(state),state);
    if(g==DIERR_INPUTLOST||g==DIERR_NOTACQUIRED){kb->Acquire();Sleep(1);continue;}
    if(SUCCEEDED(g)){
      bool down=(state[scan]&0x80)!=0;
      if(down)sawDown=true;
      if(sawDown&&!down)sawRelease=true;
    }
    Sleep(1);
  }
  sender.join();
  std::ofstream f("directinput-sendinput-report.txt",std::ios::trunc);
  f<<"Acquired="<<(acquired?"PASS":"FAIL")<<"\n";
  f<<"ScanCode="<<scan<<"\n";
  f<<"SyntheticDownSeenByDirectInput="<<(sawDown?"PASS":"FAIL")<<"\n";
  f<<"SyntheticReleaseSeenByDirectInput="<<(sawRelease?"PASS":"FAIL")<<"\n";
  bool ok=acquired&&sawDown&&sawRelease;f<<"RESULT="<<(ok?"PASS":"FAIL")<<"\n";
  kb->Unacquire();kb->Release();di->Release();DestroyWindow(h);
  return ok?0:10;
}
