import pathlib,re,sys
WB=pathlib.Path(__file__).resolve().parent
old=(WB/'premiumplus_v4824_final.cpp').read_text(encoding='utf-8')
new=(WB/'premiumplus_v4825_final.cpp').read_text(encoding='utf-8')

def body(s,sig):
    p=s.find(sig)
    if p<0: raise SystemExit('MISSING '+sig)
    q=s.find('{',p);d=0
    for i in range(q,len(s)):
        if s[i]=='{':d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[p:i+1]
    raise SystemExit('UNCLOSED '+sig)

# These runtime areas must remain byte-for-byte identical to v4.8.24.
# Minor/ATTACK/W-S are intentionally excluded only because the side-input
# arbitration patch changes those three workers surgically.
protected=[
 'void CureWorker()','void RWorker()','void VitalsWorker()','void MobSkillWorker()',
 'void MobChaseWorker()','void MobScrollWorker()','void MobPriestWorker()',
 'void WarriorWorker()','void WarriorEchoWorker()','void WarriorBattleCryWorker()',
 'bool WarriorRightClickSlot','bool WarriorResolveInventory','void CreateWarriorPage()',
 'void CreateRoguePage()','void CreateAttackPage()','void CreatePriestPage()'
]
for sig in protected:
    if body(old,sig)!=body(new,sig): raise SystemExit('PROTECTED_FUNCTION_CHANGED '+sig)
print('PROTECTED_RUNTIME_CORE=PASS')

minor=body(new,'void MinorWorker()')
attack=body(new,'void AttackWorker()')
execute=body(new,'void ExecuteAttack(const AttackSettings& a)')
ws=body(new,'void WsWorker()')
maybe_ws=body(new,'void MaybeSendWsCombo(')
wait_ws=body(new,'bool WaitWsCycleCompletion(const AttackSettings& a)')
potion=body(new,'bool UsePotion(bool hp,const AttackSettings&a)')
wnd=body(new,'LRESULT CALLBACK WndProc(')

checks={
 'VERSION_4825':'Premium Plus Combo | v4.8.25' in new,
 'PRIEST_SIDEBAR_NORMAL':'IDC_CATEGORY_PRIEST||d->CtlID==IDC_CATEGORY_ATTACK' in new,
 'MOB_TAB_VISIBILITY_GUARD':'mobVisible&&tab==1&&rowExists' in new,
 'MOB_Z_FALLBACK':'bool TryMobZFallback' in new and "ReferenceTapKey('Z')" in body(new,'bool TryMobZFallback'),
 'MOB_FAIL_CLOSED_CONFIRM':'TargetHpBarVisible(game)&&HeaderMatchesTarget' in body(new,'bool ConfirmMobCandidate'),
 'MOB_TARGET_WORKER_CONFIRM_GATE':'g_mobTargetConfirmed=true' in body(new,'void MobTargetWorker()'),
 'MOB_R_ONLY_AFTER_CONFIRM':'g_mobTargetConfirmed.load()' in body(new,'void MobChaseWorker()'),
 'MOB_SKILL_ONLY_AFTER_CONFIRM':'g_mobTargetConfirmed.load()' in body(new,'void MobSkillWorker()'),
 'THIN_RED_SUPPORT':'c*12>=n' in body(new,'MobMaskSig MakeMobMaskSig'),
 'MOB_TEST_20':'return total==20&&pass==20;' in body(new,'bool RunMobModelTest()'),
 'RESPONSIVE_BASE_RECTS':'g_uiBaseRects.push_back' in new,
 'RESPONSIVE_SCALE_X':'g_layoutScaleX=std::max(1.0' in body(new,'void LayoutChrome()'),
 'RESPONSIVE_SCALE_Y':'g_layoutScaleY=std::max(1.0' in body(new,'void LayoutChrome()'),
 'RESPONSIVE_CHILD_MOVE':'MoveWindow(e.hwnd' in body(new,'void LayoutChrome()'),

 # Side-input concurrency gates.
 'ATTACK_WORKER_NOT_BLOCKED_BY_POTION':'g_potionExclusive' not in attack,
 'ATTACK_EXECUTE_NOT_ABORTED_BY_POTION':'g_potionExclusive' not in execute,
 'WS_WORKER_NOT_CANCELLED_BY_POTION':'g_potionExclusive' not in ws and 'g_potionExclusive' not in maybe_ws and 'g_potionExclusive' not in wait_ws,
 'ATTACK_PRIORITY_PUBLISHED_BEFORE_Z':execute.find('g_attackExclusive=true;')>=0 and execute.find('g_attackExclusive=true;')<execute.find("ReferenceTapKey('Z')"),
 'WS_PRIORITY_PUBLISHED':'g_wsPriority=true;' in maybe_ws,
 'AUTO_MINOR_YIELDS_TO_ATTACK':'g_attackExclusive.load' in minor and 'g_wsPriority.load' in minor,
 'POTION_YIELDS_TO_ATTACK':'AttackSideInputReserved()' in potion,
 'POTION_RECHECKS_LIVE_TOGGLE':'PotionEnabledNow(hp)' in potion and potion.count('PotionEnabledNow(hp)')>=2,
 'HP_MP_TOGGLE_PERSISTS':'persistToggle=(id==IDC_HP_CHECK||id==IDC_MP_CHECK)' in wnd and 'ReadAttackUi(persistToggle)' in wnd,
}
for k,v in checks.items():
    print(k+'=' + ('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)
cm=body(new,'void CreateMobPage()')
for bad in ['kContentX,494,560,22','kContentX,500,244,26']:
    if bad in cm: raise SystemExit('OLD_OVERLAP_COORD_REMAINS '+bad)
print('OLD_MOB_OVERLAP_COORDS_REMOVED=PASS')