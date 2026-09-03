import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.index(sig);q=s.index('{',a);d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise RuntimeError('unclosed '+sig)

# User-visible wording: MOB chase is R-driven. W remains only in the legacy optional ATTACK W-combo.
s=s.replace('koordinat gelene kadar W-kovalama güvenli biçimde kapalı kalır.','koordinat gelene kadar range sınırı doğrulanamaz; MOB kovalama R ile çalışır.',1)

# Inject an isolated chase primitive. It never sends W. If a valid anchor/position feed exists,
# it respects the farm leash before issuing another R pulse. Without a position feed it still
# allows the normal in-game R target-follow behavior rather than inventing pixel/metre math.
anchor='void MobScrollWorker()'
assert s.count(anchor)==1
chase=r'''int MobChaseKey(){return 'R';}
bool MobChaseLeashAllows(const MobSettings&m,double*distance=nullptr){
  if(!m.anchorValid){if(distance)*distance=-1;return true;}
  double x=0,z=0;if(!ReadPosition(x,z)){if(distance)*distance=-1;return true;}
  double d=MobAnchorDistance(m,x,z);if(distance)*distance=d;return d<=m.leashRange;
}
void MobChaseWorker(){
  ULONGLONG lastPulse=0;
  while(g_running){
    RogueSettings r;MobSettings m;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;m=g_mob;}
    bool ready=r.powerEnabled&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode;
    if(!ready){lastPulse=0;Sleep(30);continue;}
    double distance=-1;if(!MobChaseLeashAllows(m,&distance)){Sleep(50);continue;}
    ULONGLONG now=GetTickCount64();
    if(lastPulse&&now-lastPulse<650){Sleep(20);continue;}
    {
      FifoTicketGuard gate(g_gameInputGate);
      if(g_running&&g_mobActive&&m.generalEnabled&&!g_cureExclusive&&!g_potionExclusive&&!g_chatMode)
        ReferenceTapKeyUnlocked(MobChaseKey());
    }
    lastPulse=GetTickCount64();
    Sleep(25);
  }
}
'''
s=s.replace(anchor,chase+anchor,1)

# Start/join chase worker without changing any legacy worker.
a,b=span('int APIENTRY wWinMain');main=s[a:b]
old='std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker),tMobSkill(MobSkillWorker),tMobScroll(MobScrollWorker),tMobPriest(MobPriestWorker);'
new='std::thread tMinor(MinorWorker),tR(RWorker),tCure(CureWorker),tAttack(AttackWorker),tWs(WsWorker),tVitals(VitalsWorker),tMobSkill(MobSkillWorker),tMobChase(MobChaseWorker),tMobScroll(MobScrollWorker),tMobPriest(MobPriestWorker);'
assert main.count(old)==1
main=main.replace(old,new,1)
oldj='if(tMobSkill.joinable())tMobSkill.join();if(tMobScroll.joinable())tMobScroll.join();if(tMobPriest.joinable())tMobPriest.join();'
newj='if(tMobSkill.joinable())tMobSkill.join();if(tMobChase.joinable())tMobChase.join();if(tMobScroll.joinable())tMobScroll.join();if(tMobPriest.joinable())tMobPriest.join();'
assert main.count(oldj)==1
main=main.replace(oldj,newj,1)
s=s[:a]+main+s[b:]

# Extend executable model tests: chase command must be R and never W.
a,b=span('bool RunMobModelTest()');test=s[a:b]
needle='  f<<"TOTAL="<<total<<"\\nPASSED="<<pass<<"\\n";return total==6&&pass==6;'
extra='  t("MobChaseUsesR",MobChaseKey()==\'R\');\n  t("MobChaseNeverUsesW",MobChaseKey()!=\'W\');\n  f<<"TOTAL="<<total<<"\\nPASSED="<<pass<<"\\n";return total==8&&pass==8;'
assert test.count(needle)==1
test=test.replace(needle,extra,1)
s=s[:a]+test+s[b:]

# Static guard: the actual chase worker itself must not contain a W key command.
a,b=span('void MobChaseWorker()');body=s[a:b]
if "'W'" in body or 'VK_W' in body:
    raise RuntimeError('MOB chase must never issue W')
if "ReferenceTapKeyUnlocked(MobChaseKey())" not in body:
    raise RuntimeError('MOB chase R command missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('R_CHASE_PATCH=PASS')
