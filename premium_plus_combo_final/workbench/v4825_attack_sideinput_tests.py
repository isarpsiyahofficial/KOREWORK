import pathlib,sys

WB=pathlib.Path(__file__).resolve().parent
SRC=WB/'premiumplus_v4825_final.cpp'
s=SRC.read_text(encoding='utf-8')


def body(sig):
    p=s.find(sig)
    if p<0: raise SystemExit('MISSING '+sig)
    q=s.find('{',p);d=0
    for i in range(q,len(s)):
        if s[i]=='{':d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[p:i+1]
    raise SystemExit('UNCLOSED '+sig)

minor=body('void MinorWorker()')
attack=body('void AttackWorker()')
execute=body('void ExecuteAttack(const AttackSettings& a)')
ws=body('void WsWorker()')
maybe=body('void MaybeSendWsCombo(')
waitws=body('bool WaitWsCycleCompletion(const AttackSettings& a)')
pot=body('bool UsePotion(bool hp,const AttackSettings&a)')
wnd=body('LRESULT CALLBACK WndProc(')

results=[]
def t(name,ok):
    results.append((name,bool(ok)))

# Source-level gates tied to the reported live regression.
t('AttackWorkerPotionIndependent','g_potionExclusive' not in attack)
t('ExecuteAttackPotionIndependent','g_potionExclusive' not in execute)
t('WsTimingPotionIndependent','g_potionExclusive' not in ws and 'g_potionExclusive' not in maybe and 'g_potionExclusive' not in waitws)
t('AttackCriticalBeforeZ',execute.find('g_attackExclusive=true;')>=0 and execute.find('g_attackExclusive=true;')<execute.find("ReferenceTapKey('Z')"))
t('WsReservationPublished','g_wsPriority=true;' in maybe)
t('AutoMinorAttackYield','g_attackExclusive.load' in minor and 'g_wsPriority.load' in minor and 'continue;' in minor)
t('PotionAttackYield','AttackSideInputReserved()' in pot)
t('PotionLiveDisableRecheck',pot.count('PotionEnabledNow(hp)')>=2)
t('HpMpToggleImmediatePersist','persistToggle=(id==IDC_HP_CHECK||id==IDC_MP_CHECK)' in wnd and 'ReadAttackUi(persistToggle)' in wnd)

# Small deterministic arbitration model: ATTACK owns the critical burst and W/S
# deadline; side-input retries instead of queueing ahead. When ATTACK is idle,
# pot/Auto Minor may run normally.
def side_allowed(attack_active,attack_critical,ws_reserved):
    return not (attack_active and (attack_critical or ws_reserved))

t('ModelPotBlockedDuringAttackBurst',not side_allowed(True,True,False))
t('ModelAutoMinorBlockedDuringWsDeadline',not side_allowed(True,False,True))
t('ModelSideInputRunsInAttackGap',side_allowed(True,False,False))
t('ModelSideInputRunsWhenAttackOff',side_allowed(False,True,True))

passed=sum(ok for _,ok in results)
out=WB/'v4825-attack-sideinput-report.txt'
with out.open('w',encoding='utf-8') as f:
    for name,ok in results:
        f.write(f'{name}={"PASS" if ok else "FAIL"}\n')
    f.write(f'TOTAL={len(results)}\nPASSED={passed}\n')
print(out.read_text(encoding='utf-8'),end='')
if passed!=len(results):
    raise SystemExit(1)
