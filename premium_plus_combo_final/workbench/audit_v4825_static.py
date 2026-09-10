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

# Deliberate hotfix scope: ExecuteAttack/Attack UI, WarriorRightClickSlot and the
# idle scheduling shell of Minor/R/Attack/W-S/MobSkill/WarriorEcho may change.
# Their active input/cadence semantics are checked explicitly below. All other
# runtime workers/pages remain byte-for-byte protected.
protected=[
 'void CureWorker()','void VitalsWorker()',
 'void MobChaseWorker()','void MobScrollWorker()','void MobPriestWorker()',
 'void WarriorWorker()','void WarriorBattleCryWorker()',
 'bool WarriorResolveInventory','void CreateWarriorPage()',
 'void CreateRoguePage()','void CreatePriestPage()'
]
for sig in protected:
    if body(old,sig)!=body(new,sig): raise SystemExit('PROTECTED_FUNCTION_CHANGED '+sig)
print('PROTECTED_RUNTIME_CORE=PASS')

minor=body(new,'void MinorWorker()')
rworker=body(new,'void RWorker()')
attack=body(new,'void AttackWorker()')
execute=body(new,'void ExecuteAttack(const AttackSettings& a)')
ws=body(new,'void WsWorker()')
mobskill=body(new,'void MobSkillWorker()')
echo=body(new,'void WarriorEchoWorker()')
maybe_ws=body(new,'void MaybeSendWsCombo(')
wait_ws=body(new,'bool WaitWsCycleCompletion(const AttackSettings& a)')
potion=body(new,'bool UsePotion(bool hp,const AttackSettings&a)')
wnd=body(new,'LRESULT CALLBACK WndProc(')
pulse_wait=body(new,'void MinorPulseWaitUntil(')
cycle_wait=body(new,'void MinorCycleWaitUntil(')
sender=body(new,'UINT MinorSendInputsLowCpu(')
timing_test=body(new,'bool RunMinorTimingTest()')
warrior_equip=body(new,'bool WarriorRightClickSlot')
attack_page=body(new,'void CreateAttackPage()')
injected=minor+pulse_wait+cycle_wait+sender+timing_test
forbidden=['CreateWaitableTimerExW','CreateWaitableTimerW','SetWaitableTimer','GetThreadTimes','GetCurrentThread']

checks={
 'VERSION_4825':'Premium Plus Combo | v4.8.25' in new,
 'PRIEST_SIDEBAR_NORMAL':'IDC_CATEGORY_PRIEST||d->CtlID==IDC_CATEGORY_ATTACK' in new,
 'MOB_TAB_VISIBILITY_GUARD':'mobVisible&&tab==1&&rowExists' in new,
 'MOB_Z_FALLBACK':'bool TryMobZFallback' in new and "ReferenceTapKey('Z')" in body(new,'bool TryMobZFallback'),
 'MOB_FAIL_CLOSED_CONFIRM':'TargetHpBarVisible(game)&&HeaderMatchesTarget' in body(new,'bool ConfirmMobCandidate'),
 'MOB_TARGET_WORKER_CONFIRM_GATE':'g_mobTargetConfirmed=true' in body(new,'void MobTargetWorker()'),
 'MOB_R_ONLY_AFTER_CONFIRM':'g_mobTargetConfirmed.load()' in body(new,'void MobChaseWorker()'),
 'MOB_SKILL_ONLY_AFTER_CONFIRM':'g_mobTargetConfirmed.load()' in mobskill,
 'THIN_RED_SUPPORT':'c*12>=n' in body(new,'MobMaskSig MakeMobMaskSig'),
 'MOB_TEST_20':'return total==20&&pass==20;' in body(new,'bool RunMobModelTest()'),
 'RESPONSIVE_BASE_RECTS':'g_uiBaseRects.push_back' in new,
 'RESPONSIVE_SCALE_X':'g_layoutScaleX=std::max(1.0' in body(new,'void LayoutChrome()'),
 'RESPONSIVE_SCALE_Y':'g_layoutScaleY=std::max(1.0' in body(new,'void LayoutChrome()'),
 'RESPONSIVE_CHILD_MOVE':'MoveWindow(e.hwnd' in body(new,'void LayoutChrome()'),

 'ATTACK_WORKER_NOT_BLOCKED_BY_POTION':'g_potionExclusive' not in attack,
 'ATTACK_EXECUTE_NOT_ABORTED_BY_POTION':'g_potionExclusive' not in execute,
 'WS_WORKER_NOT_CANCELLED_BY_POTION':'g_potionExclusive' not in ws and 'g_potionExclusive' not in maybe_ws and 'g_potionExclusive' not in wait_ws,
 'ATTACK_PRIORITY_PUBLISHED_BEFORE_Z':execute.find('g_attackExclusive=true;')>=0 and execute.find('g_attackExclusive=true;')<execute.find("ReferenceTapKey('Z')"),
 'WS_PRIORITY_PUBLISHED':'g_wsPriority=true;' in maybe_ws,
 'AUTO_MINOR_YIELDS_TO_ATTACK':'g_attackExclusive.load' in minor and 'g_wsPriority.load' in minor,
 'POTION_YIELDS_TO_ATTACK':'AttackSideInputReserved()' in potion,
 'POTION_RECHECKS_LIVE_TOGGLE':'PotionEnabledNow(hp)' in potion and potion.count('PotionEnabledNow(hp)')>=2,
 'HP_MP_TOGGLE_PERSISTS':'case IDC_HP_CHECK:ReadAttackUi(true);break;' in wnd and 'case IDC_MP_CHECK:ReadAttackUi(true);break;' in wnd,

 'MINOR_RATE_120_240':'g_turbo.load()?240:120' in minor,
 'MINOR_CYCLE_SLEEP':'Sleep(1)' in cycle_wait and 'freq/2000' in cycle_wait,
 'MINOR_PULSE_COOPERATIVE_YIELD':'Sleep(0)' in pulse_wait and 'freq/12500' in pulse_wait,
 'MINOR_OUTER_WAIT':'MinorCycleWaitUntil(nextTick' in minor,
 'MINOR_MANUAL_LOWCPU_PATH':'MinorSendInputsLowCpu(manualBatch.data()' in minor,
 'MINOR_BATCH_CACHED':'cachedSeq!=fresh.seq' in minor and 'std::array<INPUT,6>' in minor,
 'MINOR_PAIR_LEVEL_FIFO':'FifoTicketGuard sequence(g_gameInputGate);' in sender and 'FifoTicketGuard sequence(g_gameInputGate);\n      MinorSendInputsLowCpu' not in minor,
 'MINOR_NATIVE_PULSE_300US':'kMinorNativeHoldUs=300' in new,
 'MINOR_NATIVE_GAP_30US':'kMinorNativeGapUs=30' in new,
 'MINOR_GENERIC_TRANSPORT_UNCHANGED':'void PreciseDelayUs(int microseconds)' in new and 'UINT ReferenceSendInputsUnlocked' in new,
 'MINOR_PATCH_NO_NEW_APIS':all(x not in injected for x in forbidden),
 'MINOR_TIMING_TEST_MODE':'--minor-timing-test' in new and 'MeasuredHz=' in timing_test and 'minor-timing-report.txt' in timing_test,

 # CPU-only worker allowances: active behavior remains explicitly pinned.
 'MINOR_TIMER_DEFERRED':'bool timer1ms=false' in minor and minor.find('timeBeginPeriod(1)')>minor.find('if(!featureReady)'),
 'MINOR_IDLE_BACKOFF':'Sleep(20);continue;' in minor,
 'R_TIMER_DEFERRED':'bool timer1ms=false' in rworker and rworker.find('timeBeginPeriod(1)')>rworker.find('if(!featureReady)'),
 'R_IDLE_BACKOFF':'Sleep(20);continue;' in rworker,
 'R_ACTIVE_CORE_PRESERVED':"ReferenceTapKey('R')" in rworker and 'g_turbo.load()?r.rTurbo:r.rMax' in rworker and '1000/rate' in rworker and 'g_rPauseUntil' in rworker,
 'ATTACK_IDLE_BACKOFF':'Sleep(15);continue;' in attack,
 'WS_IDLE_BACKOFF':'Sleep(g_attackActive.load()?1:15)' in ws,
 'WS_ACTIVE_CORE_PRESERVED':'DirectTimedTapUnlocked(\'W\',kWsVisibleHoldUs,kWsReleaseGapUs)' in ws and 'DirectTimedTapUnlocked(\'S\',kWsVisibleHoldUs,kWsReleaseGapUs)' in ws and 'InterruptibleAttackDelayFrom' in ws,
 'MOB_SKILL_IDLE_BACKOFF':'if(!ready){was=false;Sleep(20);continue;}' in mobskill,
 'MOB_SKILL_ACTIVE_CORE_PRESERVED':'DirectTimedTapUnlocked(BarToVk(e.bar),7000,500)' in mobskill and 'DirectTimedTapUnlocked(SlotToVk(e.slot),7000,500)' in mobskill and 'std::clamp(e.delayMs,1,1000)' in mobskill,
 'ECHO_TIMER_DEFERRED':'bool timer1ms=false' in echo and echo.find('timeBeginPeriod(1)')>echo.find('if(!ready)'),
 'ECHO_IDLE_BACKOFF':'Sleep(20);continue;' in echo,
 'ECHO_ACTIVE_CORE_PRESERVED':'WarriorTimedIntervalMs(w,i)' in echo and 'WarriorCastBarSlot(game,bar,slot)' in echo and 'g_echoFireCount.fetch_add(1)' in echo,

 'WARRIOR_EQUIP_RETRIES':'for(int attempt=0;attempt<3&&!changed;attempt++)' in warrior_equip,
 'WARRIOR_EQUIP_VISUAL_CONFIRM':'WarriorWaitSlotChanged(grid,slot,before,100)' in warrior_equip and 'return changed;' in warrior_equip,
 'WARRIOR_EQUIP_CLOSE_AFTER_CONFIRM':'if(changed)Sleep(20);' in warrior_equip and "ReferenceTapKeyUnlocked('I')" in warrior_equip,
 'ATTACK_R_RUNTIME':"if(a.rAttack){PreciseDelayUs(1500);ReferenceTapKeyUnlocked('R');}" in execute,
 'ATTACK_DYNAMIC_RUNTIME':'SkillEntry skill;if(!NextAttackSkill(a,skill))' in execute,
 'ATTACK_DYNAMIC_ROWS':'kMaxAttackExtraUi' in attack_page and 'attackExtraRowBar' in attack_page and 'attackExtraRowSlot' in attack_page,
 'ATTACK_NO_LISTBOX':'attackExtraList=Ctrl(L"LISTBOX"' not in attack_page,
 'ATTACK_PLUS_UI':'Label(L"Skill Ekle"' in attack_page and 'Ctrl(L"BUTTON",L"+"' in attack_page,
}
for k,v in checks.items():
    print(k+'=' + ('PASS' if v else 'FAIL'))
    if not v: raise SystemExit(k)
cm=body(new,'void CreateMobPage()')
for bad in ['kContentX,494,560,22','kContentX,500,244,26']:
    if bad in cm: raise SystemExit('OLD_OVERLAP_COORD_REMAINS '+bad)
print('OLD_MOB_OVERLAP_COORDS_REMOVED=PASS')
