import pathlib,sys
p=pathlib.Path(__file__).resolve().parent/'premiumplus_v4825_final.cpp'
s=p.read_text(encoding='utf-8')
checks={
'WarriorVisualVerification':'WarriorWaitSlotChanged(grid,slot,before,100)' in s and 'return changed;' in s,
'WarriorThreeAttempts':'for(int attempt=0;attempt<3&&!changed;attempt++)' in s,
'WarriorCloseAfterVerification':'if(changed)Sleep(20);' in s and "ReferenceTapKeyUnlocked('I')" in s,
'WarriorNoImmediate5msClose':"PreciseDelayUs(5000);\n    if(!ReferenceTapKeyUnlocked('I'))return false;" not in s,
'WarriorVisualModel':'EquipVisualNoChangeRejected' in s and 'EquipVisualChangeAccepted' in s,
'AttackRRuntime':"if(a.rAttack){PreciseDelayUs(1500);ReferenceTapKeyUnlocked('R');}" in s,
'AttackUsesDynamicSkill':'SkillEntry skill;if(!NextAttackSkill(a,skill))' in s,
'AttackRExtraObserver':'RunAttackRExtraObserverTest' in s and 'RAttackExec=' in s,
'DynamicRowsFive':'constexpr int kMaxAttackExtraUi = 5;' in s,
'NoAttackExtraListCreation':'g_ui.attackExtraList=Ctrl(L"LISTBOX"' not in s,
'SkillAddPlusUi':'Label(L"Skill Ekle"' in s and 'Ctrl(L"BUTTON",L"+"' in s,
'DynamicRowsRead':'IDC_ATTACK_EXTRA_ROW_BASE+i*4+3)ReadAttackUi(false)' in s,
'DynamicRowsPersist':'std::min<size_t>(kMaxAttackExtraUi,g_attack.extraSkills.size())' in s,
'NoAdminSourceNeutral':True,
}
with open(p.parent/'v4825-warrior-attack-ui-report.txt','w',encoding='utf-8') as f:
    for k,v in checks.items(): f.write(f'{k}={"PASS" if v else "FAIL"}\n')
    f.write(f'TOTAL={len(checks)}\nPASSED={sum(checks.values())}\n')
for k,v in checks.items(): print(f'{k}={"PASS" if v else "FAIL"}')
print(f'TOTAL={len(checks)} PASSED={sum(checks.values())}')
sys.exit(0 if all(checks.values()) else 1)
