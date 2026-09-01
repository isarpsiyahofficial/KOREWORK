from pathlib import Path
import hashlib,sys
p=Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
def rep(a,b):
    global s
    if a not in s:
        raise SystemExit('missing transform token: '+a[:120])
    s=s.replace(a,b,1)
rep('constexpr int IDC_ATTACK_Z_COMBO = 1548;','constexpr int IDC_ATTACK_Z_COMBO = 1548;\nconstexpr int IDC_ROGUE_CATEGORY_ENABLE = 1620;\nconstexpr int IDC_ATTACK_CATEGORY_ENABLE = 1621;\nconstexpr int IDC_ATTACK_RESTORE_ENABLE = 1622;')
rep('  bool powerEnabled=true;\n};\nstruct AttackSettings {','  bool powerEnabled=true;\n  bool categoryEnabled=true;\n};\nstruct AttackSettings {')
rep('  int restoreBar=1;\n  std::array<bool,4> skillEnabled','  int restoreBar=1;\n  bool restoreMainBar=true;\n  bool categoryEnabled=true;\n  std::array<bool,4> skillEnabled')
rep('  HWND minorStart{},minorStop{},minorStartAssign{},minorStopAssign{};','  HWND minorStart{},minorStop{},minorStartAssign{},minorStopAssign{},rogueCategoryEnable{};')
rep('  HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackDelay{},restoreBar{},zCombo{},wCombo{},sCombo{},wDelay{},sDelay{};','  HWND attackStart{},attackStop{},attackStartAssign{},attackStopAssign{},attackDelay{},restoreBar{},restoreMainBar{},attackCategoryEnable{},zCombo{},wCombo{},sCombo{},wDelay{},sDelay{};')
rep('    g_rogue.powerEnabled=ReadDword(k,L"CapsToggleDefaultV3",1)!=0; RegCloseKey(k);','    g_rogue.powerEnabled=ReadDword(k,L"CapsToggleDefaultV3",1)!=0; g_rogue.categoryEnabled=ReadDword(k,L"CategoryEnabled",1)!=0; RegCloseKey(k);')
rep('    g_attack.delayMs=ClampD(ReadDword(k,L"DelayMs",125),1,2000); g_attack.restoreBar=ClampD(ReadDword(k,L"RestoreBar",1),1,12);','    g_attack.delayMs=ClampD(ReadDword(k,L"DelayMs",125),1,2000); g_attack.restoreBar=ClampD(ReadDword(k,L"RestoreBar",1),1,12); g_attack.restoreMainBar=ReadDword(k,L"RestoreMainBar",1)!=0; g_attack.categoryEnabled=ReadDword(k,L"CategoryEnabled",1)!=0;')
rep('WriteDword(k,L"CapsToggleDefaultV3",g_rogue.powerEnabled);RegCloseKey(k); }','WriteDword(k,L"CapsToggleDefaultV3",g_rogue.powerEnabled);WriteDword(k,L"CategoryEnabled",g_rogue.categoryEnabled);RegCloseKey(k); }')
rep('WriteDword(k,L"DelayMs",g_attack.delayMs);WriteDword(k,L"RestoreBar",g_attack.restoreBar);','WriteDword(k,L"DelayMs",g_attack.delayMs);WriteDword(k,L"RestoreBar",g_attack.restoreBar);WriteDword(k,L"RestoreMainBar",g_attack.restoreMainBar);WriteDword(k,L"CategoryEnabled",g_attack.categoryEnabled);')
for a,b in [
('  if(vk==r.startHotkey&&vk==r.stopHotkey){','  if(r.categoryEnabled&&vk==r.startHotkey&&vk==r.stopHotkey){'),
('  if(vk==r.startHotkey){','  if(r.categoryEnabled&&vk==r.startHotkey){'),
('  if(vk==r.stopHotkey){','  if(r.categoryEnabled&&vk==r.stopHotkey){'),
('  if(vk==a.startHotkey&&vk==a.stopHotkey){','  if(a.categoryEnabled&&vk==a.startHotkey&&vk==a.stopHotkey){'),
('  if(vk==a.startHotkey){','  if(a.categoryEnabled&&vk==a.startHotkey){'),
('  if(vk==a.stopHotkey){','  if(a.categoryEnabled&&vk==a.stopHotkey){'),
('  if(r.cureEnabled&&vk==r.cureHotkey){','  if(r.categoryEnabled&&r.cureEnabled&&vk==r.cureHotkey){')]: rep(a,b)
rep('if(!r.powerEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode)','if(!r.powerEnabled||!r.categoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode)')
rep('if(!fresh.powerEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode)','if(!fresh.powerEnabled||!fresh.categoryEnabled||!g_minorActive||g_cureExclusive||g_potionExclusive||g_chatMode)')
rep('if(!r.powerEnabled||!g_minorActive||!r.rEnabled||g_cureExclusive','if(!r.powerEnabled||!r.categoryEnabled||!g_minorActive||!r.rEnabled||g_cureExclusive')
rep('if(!fresh.powerEnabled||!g_minorActive||!fresh.rEnabled||g_cureExclusive','if(!fresh.powerEnabled||!fresh.categoryEnabled||!g_minorActive||!fresh.rEnabled||g_cureExclusive')
rep('  if(!r.powerEnabled||!r.cureEnabled){g_cureExclusive=false;continue;}','  if(!r.powerEnabled||!r.categoryEnabled||!r.cureEnabled){g_cureExclusive=false;continue;}')
rep('bool ready=r.powerEnabled&&g_attackActive&&!g_cureExclusive','bool ready=r.powerEnabled&&a.categoryEnabled&&g_attackActive&&!g_cureExclusive')
rep('    if(wantedBar!=a.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}','    if(a.restoreMainBar&&wantedBar!=a.restoreBar){PreciseDelayUs(4000);DirectTimedTapUnlocked(BarToVk(a.restoreBar),10000,1000);g_attackKnownBar=a.restoreBar;}')
rep('void ReadAttackUi(bool persist=true){AttackSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_attack;}n.delayMs=GetInt(g_ui.attackDelay,n.delayMs,1,2000);n.restoreBar=ComboVal(g_ui.restoreBar,1,12,n.restoreBar);','void ReadAttackUi(bool persist=true){AttackSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_attack;}n.categoryEnabled=SendMessageW(g_ui.attackCategoryEnable,BM_GETCHECK,0,0)==BST_CHECKED;n.delayMs=GetInt(g_ui.attackDelay,n.delayMs,1,2000);n.restoreMainBar=SendMessageW(g_ui.restoreMainBar,BM_GETCHECK,0,0)==BST_CHECKED;n.restoreBar=ComboVal(g_ui.restoreBar,1,12,n.restoreBar);')
rep('void ReadRogueUi(bool warn){RogueSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_rogue;}for(int i=0;i<3;i++)','void ReadRogueUi(bool warn){RogueSettings n;{std::lock_guard<std::mutex>lk(g_settingsMutex);n=g_rogue;}n.categoryEnabled=SendMessageW(g_ui.rogueCategoryEnable,BM_GETCHECK,0,0)==BST_CHECKED;for(int i=0;i<3;i++)')
rep('void PopulateUi(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}for(int i=0;i<3;i++)','void PopulateUi(){RogueSettings r;AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;a=g_attack;}SendMessageW(g_ui.rogueCategoryEnable,BM_SETCHECK,r.categoryEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.attackCategoryEnable,BM_SETCHECK,a.categoryEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(g_ui.restoreMainBar,BM_SETCHECK,a.restoreMainBar?BST_CHECKED:BST_UNCHECKED,0);for(int i=0;i<3;i++)')
rep('PageAdd(p,Label(L"ROGUE / MINOR",kContentX,86,310,28,g_fontBold));PageAdd(p,Label(L"Minor Kontrol"','PageAdd(p,Label(L"ROGUE / MINOR",kContentX,86,310,28,g_fontBold));g_ui.rogueCategoryEnable=Ctrl(L"BUTTON",L"ROGUE AKTİF",BS_AUTOCHECKBOX,kContentX+500,86,120,24,IDC_ROGUE_CATEGORY_ENABLE,g_fontSmall);PageAdd(p,g_ui.rogueCategoryEnable);PageAdd(p,Label(L"Minor Kontrol"')
rep('PageAdd(p,Label(L"ATTACK",kContentX,84,260,26,g_fontBold));PageAdd(p,Label(L"Attack Kontrol"','PageAdd(p,Label(L"ATTACK",kContentX,84,260,26,g_fontBold));g_ui.attackCategoryEnable=Ctrl(L"BUTTON",L"ATTACK AKTİF",BS_AUTOCHECKBOX,kContentX+500,84,120,24,IDC_ATTACK_CATEGORY_ENABLE,g_fontSmall);PageAdd(p,g_ui.attackCategoryEnable);PageAdd(p,Label(L"Attack Kontrol"')
rep('PageAdd(p,Label(L"Ana bara dön",kContentX+122,181,82,22,g_fontSmall));g_ui.restoreBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+205,180,68,200,1551,g_fontSmall);','g_ui.restoreMainBar=Ctrl(L"BUTTON",L"Ana bara dön",BS_AUTOCHECKBOX,kContentX+118,180,100,23,IDC_ATTACK_RESTORE_ENABLE,g_fontSmall);PageAdd(p,g_ui.restoreMainBar);g_ui.restoreBar=Ctrl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kContentX+220,180,58,200,1551,g_fontSmall);')
rep('if(g_ui.saveAttack&&((id>=1540&&id<=1595)||(id>=1610&&id<=1613)))ReadAttackUi(false);','if(g_ui.saveAttack&&((id>=1540&&id<=1595)||(id>=1610&&id<=1613)||id==IDC_ATTACK_CATEGORY_ENABLE||id==IDC_ATTACK_RESTORE_ENABLE))ReadAttackUi(false);')
rep('if(g_ui.saveRogue&&(id==1462||id==1463))ReadRogueUi(false);','if(g_ui.saveRogue&&(id==1462||id==1463||id==IDC_ROGUE_CATEGORY_ENABLE))ReadRogueUi(false);')
rep('case IDC_MINOR_START:g_chatMode=false;g_minorActive=true;RefreshStatus();break;','case IDC_MINOR_START:{RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}if(r.categoryEnabled){g_chatMode=false;g_minorActive=true;}RefreshStatus();break;}')
rep('case IDC_ATTACK_START:ReadAttackUi(true);g_chatMode=false;g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;g_attackActive=true;RefreshStatus();break;','case IDC_ATTACK_START:{ReadAttackUi(true);AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);a=g_attack;}if(a.categoryEnabled){g_chatMode=false;g_wsTurn=0;g_skillTurn=0;g_lastComboAt=0;g_attackActive=true;}RefreshStatus();break;}')
rep('case IDC_CURE_CHECK:ReadRogueUi(false);break;default:break;','case IDC_CURE_CHECK:ReadRogueUi(false);break;case IDC_ROGUE_CATEGORY_ENABLE:ReadRogueUi(false);{RogueSettings r;{std::lock_guard<std::mutex>lk(g_settingsMutex);r=g_rogue;}if(!r.categoryEnabled){g_minorActive=false;g_curePending=false;ReleaseKeys();}}SaveRogue();RefreshStatus();break;case IDC_ATTACK_CATEGORY_ENABLE:ReadAttackUi(true);{AttackSettings a;{std::lock_guard<std::mutex>lk(g_settingsMutex);a=g_attack;}if(!a.categoryEnabled){g_attackActive=false;ClearWsPending();ReleaseKeys();}}RefreshStatus();break;case IDC_ATTACK_RESTORE_ENABLE:ReadAttackUi(true);RefreshStatus();break;default:break;')
p.write_text(s,encoding='utf-8',newline='\n')
sha=hashlib.sha256(p.read_bytes()).hexdigest()
print(sha)
if sha!='63f36cd8fff25b574b178064fa421e36c9d4bcfa4f400947da03f8a978d9ae12':
    raise SystemExit('unexpected final sha '+sha)
