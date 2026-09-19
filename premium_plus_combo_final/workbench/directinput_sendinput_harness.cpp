#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <fstream>
#include <functional>
#include <thread>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
  return DefWindowProcW(h,m,w,l);
}
struct Result{bool down=false,up=false;};
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
  hr=kb->Acquire();bool acquired=SUCCEEDED(hr)||hr==S_FALSE;

  auto pump=[&](){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}};
  auto observe=[&](int vk,const std::function<void()>& send)->Result{
    Result r{};UINT scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC_EX)&0xFFu;BYTE state[256]{};
    std::thread t([&]{Sleep(150);send();});
    ULONGLONG end=GetTickCount64()+1200;
    while(GetTickCount64()<end){
      pump();HRESULT g=kb->GetDeviceState(sizeof(state),state);
      if(g==DIERR_INPUTLOST||g==DIERR_NOTACQUIRED){kb->Acquire();Sleep(1);continue;}
      if(SUCCEEDED(g)){
        bool down=(state[scan]&0x80)!=0;if(down)r.down=true;if(r.down&&!down)r.up=true;
      }
      Sleep(1);
    }
    t.join();return r;
  };
  auto sendScan=[&]{
    UINT sc=MapVirtualKeyW('8',MAPVK_VK_TO_VSC_EX);INPUT d{},u{};d.type=u.type=INPUT_KEYBOARD;d.ki.wScan=u.ki.wScan=(WORD)(sc&0xff);d.ki.dwFlags=KEYEVENTF_SCANCODE;u.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;if(sc&0xff00){d.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;u.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;}SendInput(1,&d,sizeof(INPUT));Sleep(100);SendInput(1,&u,sizeof(INPUT));
  };
  auto sendVk=[&]{
    INPUT d{},u{};d.type=u.type=INPUT_KEYBOARD;d.ki.wVk=u.ki.wVk='9';u.ki.dwFlags=KEYEVENTF_KEYUP;SendInput(1,&d,sizeof(INPUT));Sleep(100);SendInput(1,&u,sizeof(INPUT));
  };
  auto sendLegacy=[&]{
    UINT sc=MapVirtualKeyW('0',MAPVK_VK_TO_VSC_EX);keybd_event('0',(BYTE)(sc&0xff),0,0);Sleep(100);keybd_event('0',(BYTE)(sc&0xff),KEYEVENTF_KEYUP,0);
  };
  auto postMsg=[&]{
    PostMessageW(h,WM_KEYDOWN,'7',1);Sleep(100);PostMessageW(h,WM_KEYUP,'7',1|((LPARAM)1<<30)|((LPARAM)1<<31));
  };

  Result scan=observe('8',sendScan);
  Result vk=observe('9',sendVk);
  Result legacy=observe('0',sendLegacy);
  Result msg=observe('7',postMsg);

  std::ofstream f("directinput-sendinput-report.txt",std::ios::trunc);
  f<<"Acquired="<<(acquired?"PASS":"FAIL")<<"\n";
  f<<"SendInputScan_Down="<<(scan.down?"PASS":"FAIL")<<"\nSendInputScan_Release="<<(scan.up?"PASS":"FAIL")<<"\n";
  f<<"SendInputVK_Down="<<(vk.down?"PASS":"FAIL")<<"\nSendInputVK_Release="<<(vk.up?"PASS":"FAIL")<<"\n";
  f<<"KeybdEvent_Down="<<(legacy.down?"PASS":"FAIL")<<"\nKeybdEvent_Release="<<(legacy.up?"PASS":"FAIL")<<"\n";
  f<<"PostMessage_Down="<<(msg.down?"PASS":"FAIL")<<"\nPostMessage_Release="<<(msg.up?"PASS":"FAIL")<<"\n";
  bool any=scan.down&&scan.up || vk.down&&vk.up || legacy.down&&legacy.up;
  f<<"AnyStandardKeyboardInjectionVisible="<<(any?"PASS":"FAIL")<<"\n";
  f<<"RESULT="<<((acquired&&any)?"PASS":"FAIL")<<"\n";
  kb->Unacquire();kb->Release();di->Release();DestroyWindow(h);
  return acquired&&any?0:10;
}
