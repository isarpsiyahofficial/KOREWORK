import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

old="int t=g_assignTarget.load();if(t&&!injected){if(vk!=VK_LBUTTON&&vk!=VK_RBUTTON&&!ConflictWithRogue(vk,t)){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}"
new="int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else if(t==6||t==7)allowed=allowed&&vk!='R'&&vk!=r.startHotkey&&vk!=r.stopHotkey&&vk!=r.cureHotkey&&vk!=a.startHotkey&&vk!=a.stopHotkey;if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}"
# The original block occurs before the r/a/m snapshot, so use globals for conflict tests there.
new="int t=g_assignTarget.load();if(t&&!injected){bool allowed=vk!=VK_LBUTTON&&vk!=VK_RBUTTON;if(t<=5)allowed=allowed&&!ConflictWithRogue(vk,t);else if(t==6||t==7){RogueSettings cr;AttackSettings ca;{std::lock_guard<std::mutex>lk(g_settingsMutex);cr=g_rogue;ca=g_attack;}allowed=allowed&&vk!='R'&&vk!=cr.startHotkey&&vk!=cr.stopHotkey&&vk!=cr.cureHotkey&&vk!=ca.startHotkey&&vk!=ca.stopHotkey;}if(allowed){std::lock_guard<std::mutex>lk(g_settingsMutex);if(t==1)g_rogue.startHotkey=vk;else if(t==2)g_rogue.stopHotkey=vk;else if(t==3)g_rogue.cureHotkey=vk;else if(t==4)g_attack.startHotkey=vk;else if(t==5)g_attack.stopHotkey=vk;else if(t==6)g_mob.startHotkey=vk;else if(t==7)g_mob.stopHotkey=vk;g_assignTarget=0;PostMessageW(g_ui.main,WM_APP_ASSIGN_DONE,t,vk);}return 1;}"
assert s.count(old)==1, s.count(old)
s=s.replace(old,new,1)

bad_start="  if(target==6||target==7){"
next_marker="  if((m.generalEnabled||m.priestEnabled)&&vk==m.startHotkey&&vk==m.stopHotkey)"
a=s.find(bad_start)
b=s.find(next_marker,a)
assert a>=0 and b>a
s=s[:a]+s[b:]

anchor="LRESULT CALLBACK KeyboardProc(int code,WPARAM wp,LPARAM lp){"
assert s.count(anchor)==1
s=s.replace(anchor,"void ResetMobTimelines();\n"+anchor,1)

# Preserve the legacy selector used by the 180-test suite while the new selector includes extras/random.
pr="unsigned NextPseudoRandom(unsigned n,std::atomic<int>&last)"
if "int NextEnabledSkill(const AttackSettings& a)" not in s:
    assert s.count(pr)==1
    compat="int NextEnabledSkill(const AttackSettings& a){int enabled[4]{},n=0;for(int i=0;i<4;i++)if(a.skillEnabled[i])enabled[n++]=i;if(!n)return -1;unsigned turn=g_skillTurn.fetch_add(1,std::memory_order_relaxed);return enabled[turn%(unsigned)n];}\n"
    s=s.replace(pr,compat+pr,1)

p.write_text(s,encoding='utf-8',newline='\n')
print('POST_FIX=PASS')
