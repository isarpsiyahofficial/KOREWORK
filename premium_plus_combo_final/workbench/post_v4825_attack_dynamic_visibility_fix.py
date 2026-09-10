import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
old='void ShowCategory(int category){g_currentCategory=category;bool rogue=category==0,warrior=category==1,priest=category==2,attack=category==3,mob=category==4;for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);for(HWND h:g_ui.warriorPage)ShowWindow(h,warrior?SW_SHOW:SW_HIDE);for(HWND h:g_ui.priestPage)ShowWindow(h,priest?SW_SHOW:SW_HIDE);for(HWND h:g_ui.attackPage)ShowWindow(h,attack?SW_SHOW:SW_HIDE);for(HWND h:g_ui.mobPage)ShowWindow(h,mob?SW_SHOW:SW_HIDE);ShowMobSubCategory(g_mobSubTab.load());InvalidateRect(g_ui.main,nullptr,TRUE);}'
new='void RefreshAttackExtraRows();\nvoid ShowCategory(int category){g_currentCategory=category;bool rogue=category==0,warrior=category==1,priest=category==2,attack=category==3,mob=category==4;for(HWND h:g_ui.roguePage)ShowWindow(h,rogue?SW_SHOW:SW_HIDE);for(HWND h:g_ui.warriorPage)ShowWindow(h,warrior?SW_SHOW:SW_HIDE);for(HWND h:g_ui.priestPage)ShowWindow(h,priest?SW_SHOW:SW_HIDE);for(HWND h:g_ui.attackPage)ShowWindow(h,attack?SW_SHOW:SW_HIDE);for(HWND h:g_ui.mobPage)ShowWindow(h,mob?SW_SHOW:SW_HIDE);RefreshAttackExtraRows();ShowMobSubCategory(g_mobSubTab.load());InvalidateRect(g_ui.main,nullptr,TRUE);}'
if s.count(old)!=1: raise SystemExit(f'ShowCategory marker count={s.count(old)}')
s=s.replace(old,new,1)
old2='g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,528,224,26,1599,g_fontBold);'
new2='g_ui.saveAttack=Ctrl(L"BUTTON",L"ATTACK AYARLARINI KAYDET",BS_PUSHBUTTON,kContentX,518,224,24,1599,g_fontBold);'
if s.count(old2)!=1: raise SystemExit(f'Attack save marker count={s.count(old2)}')
s=s.replace(old2,new2,1)
p.write_text(s,encoding='utf-8')
print('V4825_ATTACK_DYNAMIC_VISIBILITY_FIX=APPLIED')
