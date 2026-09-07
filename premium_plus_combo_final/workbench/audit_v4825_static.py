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

protected=[
 'void MinorWorker()','void CureWorker()','void RWorker()','void AttackWorker()',
 'void WsWorker()','void VitalsWorker()','void MobSkillWorker()',
 'void MobChaseWorker()','void MobScrollWorker()','void MobPriestWorker()',
 'void WarriorWorker()','void WarriorEchoWorker()','void WarriorBattleCryWorker()',
 'bool WarriorRightClickSlot','bool WarriorResolveInventory','void CreateWarriorPage()',
 'void CreateRoguePage()','void CreateAttackPage()','void CreatePriestPage()'
]
for sig in protected:
    if body(old,sig)!=body(new,sig): raise SystemExit('PROTECTED_FUNCTION_CHANGED '+sig)
print('PROTECTED_RUNTIME_CORE=PASS')
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
}
for k,v in checks.items():
    print(k+'=' + ('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)
cm=body(new,'void CreateMobPage()')
for bad in ['kContentX,494,560,22','kContentX,500,244,26']:
    if bad in cm: raise SystemExit('OLD_OVERLAP_COORD_REMAINS '+bad)
print('OLD_MOB_OVERLAP_COORDS_REMOVED=PASS')
