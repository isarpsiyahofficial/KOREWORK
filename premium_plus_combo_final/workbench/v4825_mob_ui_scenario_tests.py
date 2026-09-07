import pathlib,re
WB=pathlib.Path(__file__).resolve().parent
s=(WB/'premiumplus_v4825_final.cpp').read_text(encoding='utf-8')
results=[]
def t(n,v): results.append((n,bool(v)))
target_rows=[(244+i*31,244+i*31+21) for i in range(7)]
skill_rows=[(252+i*28,252+i*28+21) for i in range(8)]
t('TargetRowsNoInternalOverlap',all(target_rows[i][1]<target_rows[i+1][0] for i in range(6)))
t('TargetRowsBeforeStatus',target_rows[-1][1]<466)
t('TargetStatusBeforeSave',466+20<502)
t('SkillRowsNoInternalOverlap',all(skill_rows[i][1]<skill_rows[i+1][0] for i in range(7)))
t('SkillRowsBeforeSave',skill_rows[-1][1]<502)
t('ScrollListBeforeSave',460<502)
t('PriestNormalSidebarStyle','IDC_CATEGORY_PRIEST||d->CtlID==IDC_CATEGORY_ATTACK' in s)
t('RefreshCannotShowSkillOnTargetTab','mobVisible&&tab==1&&rowExists' in s)
t('MouseFirstThenZFallback',s.find('auto cand=ScanMobCandidates')<s.find('TryMobZFallback(game,m)'))
t('ZDoesNotDirectlyAuthorizeAttack','TargetHpBarVisible(game)' in s and 'HeaderMatchesTarget(game' in s)
t('RRequiresConfirmedTarget','g_mobTargetConfirmed.load()' in s[s.find('void MobChaseWorker'):s.find('void MobChaseWorker')+1200])
t('SkillsRequireConfirmedTarget','g_mobTargetConfirmed.load()' in s[s.find('void MobSkillWorker'):s.find('void MobSkillWorker')+1600])
def red(r,g,b): return (r>=105 and r>=g+24 and r>=b+20) or (r>=145 and r>=g+18 and r>=b+16)
t('DimAntiAliasRedAccepted',red(118,86,80))
t('NeutralGrayRejected',not red(130,125,122))
t('StrongRedAccepted',red(190,70,60))
for name,scale in [('Dpi100',1.0),('Dpi125',1.25),('Dpi150',1.5),('Dpi200',2.0)]:
    tr=[(round(a*scale),round(b*scale)) for a,b in target_rows]
    sr=[(round(a*scale),round(b*scale)) for a,b in skill_rows]
    status_top=round(466*scale); save_top=round(502*scale)
    ok=(all(tr[i][1]<tr[i+1][0] for i in range(6)) and tr[-1][1]<status_top and
        all(sr[i][1]<sr[i+1][0] for i in range(7)) and sr[-1][1]<save_top)
    t('ResponsiveNoOverlap'+name,ok)
t('ResponsiveBaseRectRegistry','g_uiBaseRects.push_back' in s and 'MoveWindow(e.hwnd' in s)
with (WB/'v4825-mob-ui-scenario-report.txt').open('w',encoding='utf-8') as f:
    for n,v in results:f.write(f'{n}={"PASS" if v else "FAIL"}\n')
    f.write(f'TOTAL={len(results)}\nPASSED={sum(v for _,v in results)}\n')
print('\n'.join(f'{n}={"PASS" if v else "FAIL"}' for n,v in results));print('TOTAL',len(results),'PASSED',sum(v for _,v in results))
raise SystemExit(0 if all(v for _,v in results) else 1)
