import pathlib,hashlib
WB=pathlib.Path(__file__).resolve().parent
old=(WB/'premiumplus_v4823_exact.cpp').read_text(encoding='utf-8')
new=(WB/'premiumplus_v4824_final.cpp').read_text(encoding='utf-8')
def fn(t,sig):
    a=t.index(sig);q=t.index('{',a);d=0
    for i in range(q,len(t)):
        if t[i]=='{':d+=1
        elif t[i]=='}':
            d-=1
            if d==0:return t[a:i+1]
    raise RuntimeError(sig)
protected=['void MinorWorker()','void RWorker()','void CureWorker()','void WsWorker()','void AttackWorker()','void VitalsWorker()','void MobScrollWorker()']
for sig in protected:
    a,b=fn(old,sig),fn(new,sig)
    if a!=b:raise SystemExit('STABLE_CORE_CHANGED='+sig)
    print('UNCHANGED='+sig+' '+hashlib.sha256(a.encode()).hexdigest())
for bad in ['CreateRemoteThread','WriteProcessMemory','VirtualAllocEx','OpenProcess(','WinHttpOpen','InternetOpenW','WSAStartup']:
    if bad in new:raise SystemExit('FORBIDDEN='+bad)
rc=fn(new,'bool WarriorRightClickSlot')
assert 'MOUSEEVENTF_RIGHTDOWN' in rc and 'MOUSEEVENTF_RIGHTUP' in rc
assert 'MOUSEEVENTF_LEFTDOWN' not in rc and 'MOUSEEVENTF_LEFTUP' not in rc
bc=fn(new,'void WarriorBattleCryWorker');assert 'while(g_running)' in bc and 'BattleCryVisible' in bc
vis=fn(new,'bool BattleCryVisible');assert 'Denorm(w.battleCryRect)' in vis and 'CaptureScreenRectPixels' in vis
assert 'coarse>=0.94' in fn(new,'bool DetectBattleCryPixels')
conf=fn(new,'bool ConfirmMobCandidate')
assert 'TargetHpBarVisible(game)&&HeaderMatchesTarget(game,r)' in conf
assert 'c.score>=' not in conf.replace(' ','')
assert 'g_mobTargetConfirmed' in fn(new,'void MobSkillWorker()')
assert 'g_mobTargetConfirmed' in fn(new,'void MobChaseWorker()')
for x in ['constexpr int kMaxMobTargets=7;','std::array<MobTargetRecord,kMaxMobTargets>','FARM MERKEZİNİ KAYDET','IDC_CATEGORY_PRIEST','PRIEST AYARLARINI KAYDET','R Attack','kMaxMobSkillsUi=8','return total==16&&pass==16;']:
    assert x in new,x
print('STABLE_CORE_PRESERVED=PASS')
print('RIGHTCLICK_ONLY=PASS')
print('BATTLECRY_RETRY_UNTIL_VISIBLE=PASS')
print('STRICT_TARGET_HP_AND_HEADER=PASS')
print('MOB_MODEL_COUNT_16=PASS')
