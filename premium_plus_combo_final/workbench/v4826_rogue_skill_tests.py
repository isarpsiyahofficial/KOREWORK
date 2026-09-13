import pathlib,re,sys
WB=pathlib.Path(__file__).resolve().parent
p=WB/'premiumplus_v4825_final.cpp'
s=p.read_text(encoding='utf-8')

def body(sig):
    a=s.find(sig)
    if a<0: raise SystemExit('MISSING '+sig)
    q=s.find('{',a); d=0
    for i in range(q,len(s)):
        if s[i]=='{':d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[a:i+1]
    raise SystemExit('UNCLOSED '+sig)

page=body('void CreateRoguePage()')
worker=body('void RogueActionWorker()')
mana=body('bool RogueManaSwapAction(')
cast=body('bool RogueCastQuickSkill(')
click=body('bool RogueRightClickInventorySlot(')
attack=body('void AttackWorker()')
execute=body('void ExecuteAttack(const AttackSettings& a)')
overlay=body('LRESULT CALLBACK OverlayProc(')
keyboard=body('LRESULT CALLBACK KeyboardProc(')
load=body('void LoadSettings()')
save=body('void SaveRogue()')
main=body('int APIENTRY wWinMain(')
model=body('bool RunRogueV4826ModelTest()')

checks={
 'QuickLabels':all(x in page for x in ['L"800 DEF"','L"400 DEF"','L"Bıçak At"','L"M20 Bas"']),
 'QuickRowsVertical':'for(int i=0;i<4;i++){int y=260+i*26' in page,
 'QuickRowsBarSlotHotkey':all(x in page for x in ['rogueQuickBar[i]','rogueQuickSlot[i]','rogueQuickKey[i]']),
 'KnifeM20RNote':'R Attack açıksa 1 sn bekletir' in page and page.count('i>=2')>=1,
 'CustomSection':'YENİ SKILL / TUŞ ATAMASI' in page and all(x in page for x in ['rogueCustomBar','rogueCustomSlot','rogueCustomMs','rogueCustomKey']),
 'CustomMsRange':'GetInt(g_ui.rogueCustomMs,n.customMs,1,5000)' in s,
 'ManaSection':'MANA SİLME / BOW SWAP' in page and all(x in page for x in ['rogueManaBowSlot','rogueManaBar','rogueManaSlot','rogueManaKey','rogueManaCal']),
 'ResponsiveRegistration':page.count('PageAdd(p,g_ui.rogueQuickBar[i])')==1 and 'PageAdd(p,g_ui.rogueCustomKey)' in page and 'PageAdd(p,g_ui.rogueManaCal)' in page,
 'LayoutBelowGlobalStatus':all(int(y)<546 for y in re.findall(r'kContentX(?:\+\d+)?,(\d+),',page) if y.isdigit()),
 'EventDrivenWorker':'WaitForSingleObject(g_rogueActionEvent,50)' in worker and 'timeBeginPeriod' not in worker,
 'QueueBound':'g_rogueActionQueue.size()<16' in s,
 'KnifeM20CooldownPredicate':'a.rAttack&&(type==RogueKnife||type==RogueM20)' in s,
 'KnifeM20Pause1000':'g_attackPauseUntil=GetTickCount64()+1000' in worker,
 'AttackOuterPause':'GetTickCount64()<g_attackPauseUntil.load()' in attack,
 'AttackFifoPauseRecheck':'FifoTicketGuard sequence(g_gameInputGate);if(GetTickCount64()<g_attackPauseUntil.load())' in execute,
 'QuickRestoreBar':'if(bar!=restoreBar)' in cast and 'g_attackKnownBar=restoreBar' in cast,
 'ManaOpenDetectFirst':'if(RogueDetectVisibleInventory(game,grid))return true' in body('bool RogueResolveInventory('),
 'ManaSameInventorySlotTwice':mana.count('RogueRightClickInventorySlot(game,grid,r.manaBowSlot)')==2,
 'ManaPreservesPreOpenState':mana.count("if(opened&&GetForegroundWindow()==game)")>=2,
 'ManaVisualVerification':'WarriorCaptureSlotVisual(grid,slot,before)' in click and 'WarriorWaitSlotChanged(grid,slot,before,85)' in click,
 'ManaPauseIsolation':'g_attackPauseUntil=GetTickCount64()+900' in mana and 'g_rPauseUntil=GetTickCount64()+900' in mana,
 'ManaCalibrationTarget6':'target==6' in overlay and 'g_rogue.manaInventoryRect=nr' in overlay and 'BeginCalibration(6)' in s,
 'PersistenceQuick':'Quick%dBar' in load and 'Quick%dHotkey' in load and 'Quick%dBar' in save and 'Quick%dHotkey' in save,
 'PersistenceCustomMana':all(x in load+save for x in ['CustomMs','CustomHotkey','ManaBowSlot','ManaSkillBar','ManaSkillSlot','ManaHotkey','ManaInvX','ManaInvY','ManaInvW','ManaInvH']),
 'DedicatedAssignTargets':all(x in keyboard+s for x in ['g_assignTarget=10+i','g_assignTarget=14','g_assignTarget=15']),
 'PageUpDownNames':'VK_PRIOR' in s and 'VK_NEXT' in s and 'PAGE UP' in s and 'PAGE DOWN' in s,
 'WorkerStartedJoined':'tRogueAction(RogueActionWorker)' in main and 'if(tRogueAction.joinable())tRogueAction.join()' in main,
 'ModelTestCli':'--rogue-v4826-model-test' in main and 'return total==12&&pass==12;' in model,
 'NoNewTimerInRogue':'timeBeginPeriod' not in worker+mana+cast+click,
}
passn=0
out=[]
for k,v in checks.items():
    out.append(f'{k}={"PASS" if v else "FAIL"}')
    if v: passn+=1
out += [f'TOTAL={len(checks)}',f'PASSED={passn}']
(WB/'v4826-rogue-skill-report.txt').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('\n'.join(out))
if passn!=len(checks): sys.exit(1)
