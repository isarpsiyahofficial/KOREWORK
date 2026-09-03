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

once('constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.18";', 'constexpr wchar_t kTitle[] = L"Premium Plus Combo | v4.8.19";', 'title')

# HP/MP: preserve the existing pixel-analysis model, but collect a bounded sparse raster
# using GetPixel instead of bulk BitBlt/GetDIBits desktop capture.
replace_fn('static BarReading CaptureBarReading(const NormalizedRect&r,bool hp)', r'''static BarReading CaptureBarReading(const NormalizedRect&r,bool hp){
  BarReading out;if(!r.valid())return out;RECT q=Denorm(r);int w=q.right-q.left,h=q.bottom-q.top;if(w<8||h<3||w>2400||h>600)return out;
  const int sw=std::clamp(w,8,420),sh=std::clamp(h,3,64);std::vector<uint32_t> px((size_t)sw*sh);
  HDC dc=GetDC(nullptr);if(!dc)return out;int bad=0;
  for(int y=0;y<sh;y++){int sy=q.top+(int)(((long long)(2*y+1)*h)/(2*sh));sy=std::clamp(sy,q.top,q.bottom-1);for(int x=0;x<sw;x++){int sx=q.left+(int)(((long long)(2*x+1)*w)/(2*sw));sx=std::clamp(sx,q.left,q.right-1);COLORREF c=GetPixel(dc,sx,sy);if(c==CLR_INVALID){bad++;c=RGB(0,0,0);}px[(size_t)y*sw+x]=(uint32_t)GetBValue(c)|((uint32_t)GetGValue(c)<<8)|((uint32_t)GetRValue(c)<<16);}}
  ReleaseDC(nullptr,dc);if(bad>(sw*sh)/8)return out;return AnalyzeBarPixels(px,sw,sh,hp);
}''')

# Keep the old helper name only as a harmless stub so no bulk screen-grab code survives.
replace_fn('bool CaptureGameClient(HWND game,std::vector<uint32_t>&px,int&w,int&h,POINT&origin)', r'''bool CaptureGameClient(HWND,std::vector<uint32_t>&px,int&w,int&h,POINT&origin){px.clear();w=h=0;origin={};return false;}''')

# Warrior: read only the right/lower inventory search area with sparse GetPixel samples.
# Detect the 8 vertical + 5 horizontal periodic grid borders (7x4 cells), then convert back
# to real screen coordinates. No full-window screenshot is created.
replace_fn('bool FindInventoryGrid(HWND game,InventoryGrid&grid)', r'''bool FindInventoryGrid(HWND game,InventoryGrid&grid){
  grid={};if(!game||!IsWindow(game))return false;RECT cr{};if(!GetClientRect(game,&cr))return false;POINT origin{0,0};if(!ClientToScreen(game,&origin))return false;
  const int W=cr.right-cr.left,H=cr.bottom-cr.top;if(W<640||H<420)return false;
  const int cropL=W*70/100,cropR=W*995/1000,cropT=H*48/100,cropB=H*86/100;if(cropR-cropL<220||cropB-cropT<150)return false;
  int step=2;while(((cropR-cropL+step-1)/step)*((cropB-cropT+step-1)/step)>42000&&step<5)step++;
  const int sw=(cropR-cropL+step-1)/step,sh=(cropB-cropT+step-1)/step;if(sw<80||sh<60)return false;
  std::vector<uint8_t> g((size_t)sw*sh);HDC dc=GetDC(nullptr);if(!dc)return false;int bad=0;
  for(int y=0;y<sh;y++){int py=origin.y+std::min(cropB-1,cropT+y*step);for(int x=0;x<sw;x++){int px=origin.x+std::min(cropR-1,cropL+x*step);COLORREF c=GetPixel(dc,px,py);if(c==CLR_INVALID){bad++;c=RGB(0,0,0);}int rr=GetRValue(c),gg=GetGValue(c),bb=GetBValue(c);g[(size_t)y*sw+x]=(uint8_t)((rr*77+gg*150+bb*29)>>8);}}
  ReleaseDC(nullptr,dc);if(bad>(sw*sh)/12)return false;
  std::vector<double> vx((size_t)sw-1),hy((size_t)sh-1),vs(vx.size()),hs(hy.size());
  for(int x=1;x<sw;x++){double sum=0;for(int y=0;y<sh;y++)sum+=std::abs((int)g[(size_t)y*sw+x]-(int)g[(size_t)y*sw+x-1]);vx[(size_t)x-1]=sum/sh;}
  for(int y=1;y<sh;y++){double sum=0;for(int x=0;x<sw;x++)sum+=std::abs((int)g[(size_t)y*sw+x]-(int)g[(size_t)(y-1)*sw+x]);hy[(size_t)y-1]=sum/sw;}
  for(size_t i=0;i<vx.size();i++){double m=vx[i];if(i)m=std::max(m,vx[i-1]);if(i+1<vx.size())m=std::max(m,vx[i+1]);vs[i]=m;}
  for(size_t i=0;i<hy.size();i++){double m=hy[i];if(i)m=std::max(m,hy[i-1]);if(i+1<hy.size())m=std::max(m,hy[i+1]);hs[i]=m;}
  double meanV=0,meanH=0;for(double v:vs)meanV+=v;for(double v:hs)meanH+=v;meanV/=std::max<size_t>(1,vs.size());meanH/=std::max<size_t>(1,hs.size());
  double best=-1;int bestX=0,bestY=0,bestCell=0;
  for(int cellPx=32;cellPx<=90;cellPx++){int cs=std::max(4,(cellPx+step/2)/step);if(7*cs+2>=(int)vs.size()||4*cs+2>=(int)hs.size())continue;
    double bx=-1,by=-1;int bxi=0,byi=0;
    for(int x=1;x+7*cs<(int)vs.size()-1;x++){double sc=0;for(int j=0;j<8;j++)sc+=vs[(size_t)(x+j*cs)];sc/=8.0;if(sc>bx){bx=sc;bxi=x;}}
    for(int y=1;y+4*cs<(int)hs.size()-1;y++){double sc=0;for(int j=0;j<5;j++)sc+=hs[(size_t)(y+j*cs)];sc/=5.0;if(sc>by){by=sc;byi=y;}}
    double score=bx/(meanV+1.0)+by/(meanH+1.0);if(score>best){best=score;bestX=bxi;bestY=byi;bestCell=cs;}
  }
  if(best<2.65||bestCell<=0)return false;int left=cropL+bestX*step,top=cropT+bestY*step,cell=bestCell*step;
  if(left<W*71/100||left+7*cell>W*999/1000||top<H*49/100||top+4*cell>H*88/100)return false;
  grid.valid=true;grid.left=left;grid.top=top;grid.cell=cell;grid.clientW=W;grid.clientH=H;grid.screenOrigin=origin;grid.score=best;return true;
}''')

# One strong periodic detection is enough. If inventory is closed, open it once and retry
# at bounded intervals. This keeps the requested response time while avoiding repeated full scans.
replace_fn('bool WarriorResolveInventory(HWND game,InventoryGrid&grid)', r'''bool WarriorResolveInventory(HWND game,InventoryGrid&grid){
  if(FindInventoryGrid(game,grid))return true;if(GetForegroundWindow()!=game)return false;ReferenceTapKey('I');ULONGLONG deadline=GetTickCount64()+340;Sleep(38);
  while(g_running&&GetTickCount64()<deadline){if(FindInventoryGrid(game,grid))return true;Sleep(24);}return false;
}''')

# Source-level guards: production runtime must no longer contain bulk desktop capture APIs.
for bad in ('BitBlt(','GetDIBits(','CreateCompatibleBitmap(','CreateCompatibleDC('):
    if bad in s: raise RuntimeError('bulk capture API still present: '+bad)
if 'GetPixel(dc' not in s: raise RuntimeError('sparse pixel reader missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('SPARSE_PIXEL_CAPTURE=PASS')
