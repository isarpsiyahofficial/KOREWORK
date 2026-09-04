import pathlib,sys

path=pathlib.Path(sys.argv[1])
s=path.read_text(encoding='utf-8')


def replace_function(text, sig, replacement):
    p=text.find(sig)
    if p<0: raise RuntimeError('missing '+sig)
    q=text.find('{',p)
    if q<0: raise RuntimeError('missing body '+sig)
    d=0
    end=None
    for i in range(q,len(text)):
        c=text[i]
        if c=='{': d+=1
        elif c=='}':
            d-=1
            if d==0:
                end=i+1
                break
    if end is None: raise RuntimeError('unclosed '+sig)
    return text[:p]+replacement+text[end:]

# Version only; all untouched working modules remain byte-for-byte identical.
s=s.replace('Premium Plus Combo | v4.8.22','Premium Plus Combo | v4.8.23')

s=replace_function(s,'bool DetectBattleCryPixels',r'''bool DetectBattleCryPixels(const std::vector<uint32_t>&px,int W,int H,double*bestScore=nullptr){
  double best=-1.0;
  if(W<20||H<20||(int)px.size()<W*H){if(bestScore)*bestScore=best;return false;}
  // Two live Battle Cry captures (34px and 32px) are kept as fine visual
  // signatures. The old 8x8 template remains a cheap first-stage filter.
  static const uint8_t kBattleCryFineA[289]={
    36,46,44,81,21,46,31,66,34,19,16,19,61,57,27,67,27,
    36,52,56,41,32,32,38,30,17,18,17,14,18,27,38,18,84,
    38,50,85,25,58,70,60,26,58,24,54,46,40,26,65,50,57,
    95,132,236,90,169,113,145,56,130,182,70,73,174,72,127,124,133,
    92,149,183,83,107,181,118,34,178,229,70,169,228,94,116,142,90,
    84,154,121,105,228,176,55,11,76,100,54,42,168,16,152,159,86,
    83,164,137,89,100,191,10,54,86,142,98,22,206,30,205,151,50,
    82,146,157,67,171,129,30,60,23,156,59,20,168,49,210,80,239,
    94,125,146,184,47,170,100,97,36,84,45,102,67,187,74,160,215,
    94,123,99,134,143,94,131,44,105,28,48,140,60,66,96,178,182,
    89,113,72,59,134,150,78,14,115,110,92,114,2,94,152,162,45,
    93,110,42,83,73,134,63,15,118,154,167,134,61,92,109,60,52,
    90,106,27,80,74,35,130,100,133,154,144,180,132,38,82,47,17,
    87,89,30,32,55,34,72,102,132,81,54,138,111,38,49,11,18,
    60,49,39,52,46,41,46,57,83,85,112,84,59,48,45,23,35,
    60,44,51,82,72,69,80,80,72,64,92,104,66,64,62,34,36,
    48,43,43,57,90,72,85,76,77,72,66,109,71,48,57,48,46
  };
  static const uint8_t kBattleCryFineB[289]={
    30,48,76,16,13,14,48,33,5,12,16,12,20,47,18,28,84,
    31,65,63,46,31,77,36,13,68,20,33,33,27,8,73,18,71,
    52,134,87,128,96,90,61,89,98,101,75,33,112,79,59,106,80,
    82,230,128,173,100,173,60,112,162,219,85,170,190,86,145,107,135,
    88,224,31,119,186,132,55,30,190,137,43,80,199,36,146,188,70,
    96,216,26,194,183,106,2,39,166,142,57,34,196,19,159,119,58,
    95,202,53,148,108,114,15,63,54,142,100,15,189,38,205,156,158,
    90,144,225,43,173,135,24,71,8,143,38,64,118,104,184,72,244,
    82,128,188,160,20,214,16,136,3,50,58,108,56,213,16,210,194,
    86,130,119,131,135,80,92,130,24,28,48,140,60,66,96,178,182,
    84,90,74,119,113,155,21,76,92,110,92,114,2,94,152,162,45,
    84,98,36,98,109,88,46,53,138,154,167,134,61,92,109,60,52,
    82,72,57,86,50,50,148,69,221,154,144,180,132,38,82,47,17,
    77,66,21,58,39,52,55,170,91,81,54,138,111,38,49,11,18,
    54,39,50,47,47,41,49,65,98,85,112,84,59,48,45,23,35,
    39,47,64,85,66,74,79,83,64,64,92,104,66,64,62,34,36,
    52,38,44,78,81,78,84,75,74,72,66,109,71,48,57,48,46
  };
  const int S=W+1;
  std::vector<uint64_t> ii((size_t)(W+1)*(H+1));
  for(int y=0;y<H;y++){
    uint64_t row=0;
    for(int x=0;x<W;x++){
      row+=BattleGray(px[(size_t)y*W+x]);
      ii[(size_t)(y+1)*S+x+1]=ii[(size_t)y*S+x+1]+row;
    }
  }
  auto sumRect=[&](int l,int t,int r,int b)->uint64_t{
    return ii[(size_t)b*S+r]-ii[(size_t)t*S+r]-ii[(size_t)b*S+l]+ii[(size_t)t*S+l];
  };
  auto prep=[&](const uint8_t* t,int n,double& mean,double& energy){
    mean=0;for(int i=0;i<n;i++)mean+=t[i];mean/=n;energy=0;
    for(int i=0;i<n;i++){double d=(double)t[i]-mean;energy+=d*d;}
  };
  double tm8=0,te8=0;prep(kBattleCryTemplate8,64,tm8,te8);
  double tmA=0,teA=0,tmB=0,teB=0;prep(kBattleCryFineA,289,tmA,teA);prep(kBattleCryFineB,289,tmB,teB);
  auto ncc=[&](const double* p,int n,double pm,const uint8_t* t,double tm,double te)->double{
    double dot=0,pe=0;for(int i=0;i<n;i++){double a=p[i]-pm,b=(double)t[i]-tm;dot+=a*b;pe+=a*a;}
    return pe>1&&te>1?dot/std::sqrt(pe*te):-1.0;
  };
  int maxS=std::min({64,W,H}),minS=std::min(maxS,20);
  int xyStep=(W*H>120000)?3:2;
  for(int size=minS;size<=maxS;size+=2){
    for(int y=0;y+size<=H;y+=xyStep)for(int x=0;x+size<=W;x+=xyStep){
      double c8[64]{},m8=0;
      for(int ty=0;ty<8;ty++)for(int tx=0;tx<8;tx++){
        int x0=x+tx*size/8,x1=x+(tx+1)*size/8,y0=y+ty*size/8,y1=y+(ty+1)*size/8;
        int n=std::max(1,(x1-x0)*(y1-y0));double v=(double)sumRect(x0,y0,x1,y1)/(double)n;
        c8[ty*8+tx]=v;m8+=v;
      }
      m8/=64.0;double coarse=ncc(c8,64,m8,kBattleCryTemplate8,tm8,te8);
      if(coarse<0.40)continue;
      double c17[289]{},m17=0;
      for(int ty=0;ty<17;ty++)for(int tx=0;tx<17;tx++){
        int x0=x+tx*size/17,x1=x+(tx+1)*size/17,y0=y+ty*size/17,y1=y+(ty+1)*size/17;
        int n=std::max(1,(x1-x0)*(y1-y0));double v=(double)sumRect(x0,y0,x1,y1)/(double)n;
        c17[ty*17+tx]=v;m17+=v;
      }
      m17/=289.0;
      double fine=std::max(ncc(c17,289,m17,kBattleCryFineA,tmA,teA),ncc(c17,289,m17,kBattleCryFineB,tmB,teB));
      if(fine>best)best=fine;
      if(best>=0.90){if(bestScore)*bestScore=best;return true;}
    }
  }
  if(bestScore)*bestScore=best;
  // The fine signatures separate the supplied live icon captures from unrelated
  // UI frames, allowing a tolerant threshold without the old false recast loop.
  return best>=0.60;
}''')
s=replace_function(s,'bool BattleCryVisible',r'''bool BattleCryVisible(const WarriorSettings&w,double*score=nullptr){
  if(!w.battleCryRect.valid()){if(score)*score=-1;return false;}
  RECT r=Denorm(w.battleCryRect);
  // Buffs can shift or wrap to the next row as other scrolls are added. Search a
  // guarded margin around the user's calibrated buff area instead of one frozen box.
  int ww=std::max<LONG>(1,r.right-r.left),hh=std::max<LONG>(1,r.bottom-r.top);
  int padX=std::clamp(ww/5,20,80),padY=std::clamp(hh,36,90);
  int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
  r.left=std::max<LONG>(r.left-padX,vx);r.right=std::min<LONG>(r.right+padX,vx+vw);
  r.top=std::max<LONG>(r.top-padY,vy);r.bottom=std::min<LONG>(r.bottom+padY,vy+vh);
  std::vector<uint32_t>px;int W=0,H=0;
  if(!CaptureScreenRectPixels(r,px,W,H)){if(score)*score=-1;return false;}
  return DetectBattleCryPixels(px,W,H,score);
}''')

s=replace_function(s,'bool WarriorResolveVisibleInventoryFast',r'''bool WarriorResolveVisibleInventoryFast(HWND game,InventoryGrid&grid){
  if(!game||GetForegroundWindow()!=game)return false;
  // IMPORTANT: the fast path may only reuse a grid that was already resolved by
  // the robust 7x4 detector. v4.8.22 also guessed a grid directly from the raw
  // calibration rectangle; a few pixels of border/margin then shifted every slot.
  InventoryGrid cached=g_warriorCachedGrid;
  if(g_warriorCachedWindow.load()!=(ULONG_PTR)game||!cached.valid)return false;
  int margin=6;
  RECT sr{cached.screenOrigin.x+cached.left-margin,cached.screenOrigin.y+cached.top-margin,
          cached.screenOrigin.x+cached.left+7*cached.cell+margin,cached.screenOrigin.y+cached.top+4*cached.cell+margin};
  std::vector<uint32_t>px;int W=0,H=0;
  if(!CaptureScreenRectPixels(sr,px,W,H))return false;
  InventoryGrid q{};q.valid=true;q.left=margin;q.top=margin;q.cell=cached.cell;q.clientW=W;q.clientH=H;q.screenOrigin={sr.left,sr.top};
  double sc=WarriorGridEdgeScore(px,W,H,q);
  if(sc<8.0)return false;
  q.score=sc;grid=q;return true;
}''')

s=replace_function(s,'bool WarriorResolveInventory',r'''bool WarriorResolveInventory(HWND game,InventoryGrid&grid){
  if(!game||GetForegroundWindow()!=game)return false;
  // First reuse a previously VERIFIED grid; otherwise use the exact v4.8.21
  // full-client/calibration-search detector. Do not infer slot geometry directly
  // from the user's rectangle.
  if(WarriorResolveVisibleInventoryFast(game,grid)){g_warriorInventoryKnownOpen=true;return true;}
  int pre=g_warriorInventoryKnownOpen.load()?3:2;
  for(int i=0;i<pre;i++){
    if(WarriorResolveVisibleInventory(game,grid)){g_warriorInventoryKnownOpen=true;return true;}
    if(i+1<pre)Sleep(4);
  }
  if(GetForegroundWindow()!=game)return false;
  {FifoTicketGuard gate(g_gameInputGate);if(!ReferenceTapKeyUnlocked('I'))return false;}
  g_warriorInventoryKnownOpen=false;
  ULONGLONG deadline=GetTickCount64()+280;
  while(g_running&&GetTickCount64()<deadline){
    Sleep(4);if(GetForegroundWindow()!=game)return false;
    if(WarriorResolveVisibleInventoryFast(game,grid)||WarriorResolveVisibleInventory(game,grid)){
      g_warriorInventoryKnownOpen=true;return true;
    }
  }
  return false;
}''')

s=replace_function(s,'void WarriorBattleCryWorker',r'''void WarriorBattleCryWorker(){
  int miss=0;ULONGLONG retryAt=0,lastSeenAt=0,lastCastAt=0;unsigned rev0=0;
  while(g_running){
    WarriorSettings w;RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);w=g_warrior;r=g_rogue;}
    HWND game=(HWND)g_gameWindow.load();unsigned rev=g_battleCryRevision.load();
    bool ready=w.enabled&&w.battleCryEnabled&&w.battleCryRect.valid()&&r.powerEnabled&&game&&IsWindow(game)&&GetForegroundWindow()==game&&!g_chatMode.load();
    if(!ready){miss=0;retryAt=0;lastSeenAt=0;g_battleCryState=0;Sleep(25);continue;}
    if(rev!=rev0){rev0=rev;miss=0;retryAt=0;lastSeenAt=0;}
    double score=-1;bool present=BattleCryVisible(w,&score);g_battleCryLastScore=score;ULONGLONG now=GetTickCount64();
    if(score<0){miss=0;Sleep(50);continue;}
    if(present){g_battleCryState=1;miss=0;retryAt=0;lastSeenAt=now;Sleep(85);continue;}
    // A single animated/dim frame must never cause a recast. Once Battle Cry was
    // seen, require continuous absence for 900 ms before considering it expired.
    if(lastSeenAt&&now-lastSeenAt<900){g_battleCryState=1;miss=0;Sleep(70);continue;}
    if(++miss<5){Sleep(55);continue;}
    g_battleCryState=2;
    if(now<retryAt){Sleep((DWORD)std::min<ULONGLONG>(35,retryAt-now));continue;}
    // Also cap retries if the game temporarily refuses the cast.
    if(lastCastAt&&now-lastCastAt<700){Sleep(40);continue;}
    bool sent=WarriorCastBarSlot(game,w.battleCryBar,w.battleCrySlot);lastCastAt=GetTickCount64();
    if(!sent){retryAt=GetTickCount64()+180;Sleep(20);continue;}
    bool confirmed=false;ULONGLONG verifyUntil=GetTickCount64()+1200;
    while(g_running&&GetTickCount64()<verifyUntil){
      Sleep(35);if(GetForegroundWindow()!=game)break;
      WarriorSettings cur;{std::lock_guard<std::mutex>lk(g_settingsMutex);cur=g_warrior;}
      double sc=-1;if(BattleCryVisible(cur,&sc)){g_battleCryLastScore=sc;confirmed=true;lastSeenAt=GetTickCount64();break;}
    }
    if(confirmed){g_battleCryState=1;miss=0;retryAt=0;}
    else{g_battleCryState=2;miss=0;retryAt=GetTickCount64()+250;}
  }
}''')

# Tight source guards: only the intended logic is allowed to move.
for required in [
    'Premium Plus Combo | v4.8.23','return best>=0.60','padY=std::clamp(hh,36,90)',
    'fast path may only reuse a grid','GetTickCount64()+280','now-lastSeenAt<900',
    'GetTickCount64()+1200'
]:
    if required not in s: raise RuntimeError('guard missing '+required)

path.write_text(s,encoding='utf-8',newline='\n')
print('WARRIOR_V4823_LIVEFIX=PASS')
