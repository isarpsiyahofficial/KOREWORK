import pathlib,subprocess,sys,re
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
subprocess.check_call([sys.executable,str(WB/'build_v4824_final_source.py')],cwd=ROOT)
s=(WB/'premiumplus_v4824_final.cpp').read_text(encoding='utf-8')
out=[]

def fn(sig):
    p=s.find(sig)
    if p<0:
        out.append(f'@@ MISSING {sig}\n')
        return
    q=s.find('{',p); d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0:
                out.append(f'\n@@ FUNCTION {sig}\n'+s[p:i+1]+'\n@@ END\n')
                return

def around(token,span=5000):
    p=s.find(token)
    out.append(f'\n@@ TOKEN {token!r} pos={p}\n')
    if p>=0: out.append(s[max(0,p-span):min(len(s),p+span)]+'\n')

for sig in [
    'void CreateMobPage()',
    'bool ConfirmMobCandidate',
    'void MobTargetWorker()',
    'void MobSkillWorker()',
    'void MobChaseWorker()',
    'LRESULT CALLBACK WndProc',
]: fn(sig)
for token in [
    'yeterli kırmızı mob isim görüntüsü',
    'Görsel Tanıt',
    'Görsel yok',
    'Görsel OK',
    'IDC_CATEGORY_PRIEST',
    'MobVisual',
    'mobVisual',
    'DetectMob',
    'TrainMob',
    'CaptureMob',
    'Hedef aranıyor',
]: around(token,3500)
for pat in ['red','nameplate','visual','Visual','mobTarget','MobTarget']:
    out.append(f'\n@@ MATCHES {pat}\n')
    for m in re.finditer(pat,s,re.I if pat=='red' else 0):
        a=s.rfind('\n',0,m.start()); b=s.find('\n',m.start())
        line=s[a+1:b if b>=0 else len(s)]
        if len(line)<500: out.append(line+'\n')
(WB/'v4824-mob-dump.txt').write_text(''.join(out),encoding='utf-8')
print('DUMP_WRITTEN=PASS')
