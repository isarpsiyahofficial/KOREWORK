import pathlib,sys
p=pathlib.Path(sys.argv[1]);s=p.read_text(encoding='utf-8')

old='void Font(HWND h,HFONT f=nullptr){SendMessageW(h,WM_SETFONT,(WPARAM)(f?f:g_font),TRUE);} HWND Ctrl(const wchar_t* cls,const wchar_t* txt,DWORD style,int x,int y,int w,int h,int id,HFONT f=nullptr){HWND c=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,g_ui.main,(HMENU)(INT_PTR)id,g_instance,nullptr);Font(c,f);return c;} HWND Label(const wchar_t* txt,int x,int y,int w,int h,HFONT f=nullptr){return Ctrl(L"STATIC",txt,SS_LEFT|SS_CENTERIMAGE,x,y,w,h,0,f);} void PageAdd(std::vector<HWND>& p,HWND h){p.push_back(h);} '
new=r'''struct UiBaseRect{HWND hwnd{};int x{},y{},w{},h{};};
std::vector<UiBaseRect> g_uiBaseRects;
double g_layoutScaleX=1.0,g_layoutScaleY=1.0;
void Font(HWND h,HFONT f=nullptr){SendMessageW(h,WM_SETFONT,(WPARAM)(f?f:g_font),TRUE);}
HWND Ctrl(const wchar_t* cls,const wchar_t* txt,DWORD style,int x,int y,int w,int h,int id,HFONT f=nullptr){
  HWND c=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,g_ui.main,(HMENU)(INT_PTR)id,g_instance,nullptr);
  Font(c,f);if(c)g_uiBaseRects.push_back({c,x,y,w,h});return c;
}
HWND Label(const wchar_t* txt,int x,int y,int w,int h,HFONT f=nullptr){return Ctrl(L"STATIC",txt,SS_LEFT|SS_CENTERIMAGE,x,y,w,h,0,f);}
void PageAdd(std::vector<HWND>& p,HWND h){p.push_back(h);} '''
if old not in s: raise SystemExit('Ctrl/Label anchor missing')
s=s.replace(old,new,1)

old_layout='void LayoutChrome(){if(!g_ui.main)return;RECT r{};GetClientRect(g_ui.main,&r);int cw=r.right-r.left,ch=r.bottom-r.top;if(g_ui.power)MoveWindow(g_ui.power,std::max(kContentX+470,cw-140),14,116,30,TRUE);if(g_ui.status)MoveWindow(g_ui.status,kContentX,std::max(546,ch-36),std::max(220,cw-kContentX-20),20,TRUE);InvalidateRect(g_ui.main,nullptr,TRUE);}'
new_layout=r'''void LayoutChrome(){
  if(!g_ui.main)return;RECT r{};GetClientRect(g_ui.main,&r);int cw=std::max(1L,r.right-r.left),ch=std::max(1L,r.bottom-r.top);
  // The window cannot be resized below the 760x620 logical baseline. When
  // PerMonitorV2 or the user enlarges it, every child control is scaled from
  // its creation-time logical rectangle; fonts are independently rebuilt by DPI.
  g_layoutScaleX=std::max(1.0,(double)cw/(double)kWindowWidth);
  g_layoutScaleY=std::max(1.0,(double)ch/(double)kWindowHeight);
  for(const auto&e:g_uiBaseRects){
    if(!e.hwnd||!IsWindow(e.hwnd))continue;
    int x=(int)std::lround(e.x*g_layoutScaleX),y=(int)std::lround(e.y*g_layoutScaleY);
    int w=std::max(1,(int)std::lround(e.w*g_layoutScaleX)),h=std::max(1,(int)std::lround(e.h*g_layoutScaleY));
    MoveWindow(e.hwnd,x,y,w,h,TRUE);
  }
  InvalidateRect(g_ui.main,nullptr,TRUE);
}'''
if old_layout not in s: raise SystemExit('LayoutChrome anchor missing')
s=s.replace(old_layout,new_layout,1)

repls={
'RECT side{0,70,kSidebarWidth,r.bottom};':'RECT side{0,(LONG)std::lround(70*g_layoutScaleY),(LONG)std::lround(kSidebarWidth*g_layoutScaleX),r.bottom};',
'RECT panel{kContentX-10,74,r.right-12,r.bottom-34};':'RECT panel{(LONG)std::lround((kContentX-10)*g_layoutScaleX),(LONG)std::lround(74*g_layoutScaleY),r.right-(LONG)std::lround(12*g_layoutScaleX),r.bottom-(LONG)std::lround(34*g_layoutScaleY)};',
'RECT t{16,12,std::max<LONG>(360,r.right-180),56};':'RECT t{(LONG)std::lround(16*g_layoutScaleX),(LONG)std::lround(12*g_layoutScaleY),std::max<LONG>((LONG)std::lround(360*g_layoutScaleX),r.right-(LONG)std::lround(180*g_layoutScaleX)),(LONG)std::lround(56*g_layoutScaleY)};'
}
for a,b in repls.items():
    if a not in s: raise SystemExit('paint anchor missing: '+a)
    s=s.replace(a,b,1)
for needle in ['g_uiBaseRects.push_back','g_layoutScaleX=std::max(1.0','MoveWindow(e.hwnd','kWindowWidth','kWindowHeight']:
    assert needle in s,needle
p.write_text(s,encoding='utf-8',newline='\n')
print('V4825_RESPONSIVE_LAYOUT=PASS')
