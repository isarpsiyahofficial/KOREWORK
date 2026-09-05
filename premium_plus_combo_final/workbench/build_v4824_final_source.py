import pathlib,hashlib,subprocess,sys,base64,lzma
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
base=WB/'makro2_v4811_exact.cpp'
v23=WB/'premiumplus_v4823_exact.cpp'
out=WB/'premiumplus_v4824_final.cpp'
def run(script,*args):
    cmd=[sys.executable,str(WB/script),*map(str,args)]
    print('RUN',' '.join(cmd)); subprocess.check_call(cmd,cwd=ROOT)
h=hashlib.sha256(base.read_bytes().replace(b'\r\n',b'\n')).hexdigest().upper()
assert h=='3AECB38864D0C248F926C9F15A8FF7F5BE4A1636ACAB966ADC40DA350C142A42',h
run('apply_mob_dynamic.py',base,v23)
run('post_mob_dynamic_fix.py',v23)
run('post_mob_r_chase.py',v23)
run('post_ui_compact_tabs.py',v23)
run('post_warrior_rightclick.py',v23)
run('post_warrior_defender_clean.py',v23)
s=v23.read_text(encoding='utf-8').replace('Premium Plus Combo | v4.8.17','Premium Plus Combo | v4.8.19')
v23.write_text(s,encoding='utf-8',newline='\n')
run('post_warrior_toggle_fast.py',v23)
run('post_warrior_descent_echo.py',v23)
run('post_warrior_v4822.py',v23)
s=v23.read_text(encoding='utf-8'); old='return total==25&&pass==25;'; assert old in s
v23.write_text(s.replace(old,'return total==26&&pass==26;',1),encoding='utf-8',newline='\n')
run('post_warrior_v4823_livefix.py',v23)
h23=hashlib.sha256(v23.read_bytes()).hexdigest().upper(); print('V4823_EXACT_SOURCE_SHA256='+h23)
assert h23=='D21B1D1A93CFDAB31F15D21340BD02C7E2FA15C3D99117DAA7A0E9CB10338305',h23
out.write_bytes(v23.read_bytes())
a=(WB/'v4824_diff_payload_1.txt').read_text().strip(); b=(WB/'v4824_diff_payload_2.txt').read_text().strip()
patch=ROOT/'v4824.patch'; patch.write_bytes(lzma.decompress(base64.b64decode(a+b)))
subprocess.check_call(['git','apply','--whitespace=nowarn',str(patch)],cwd=ROOT)
run('post_warrior_v4823_battlecry_coarse_confirm.py',out)
hpre=hashlib.sha256(out.read_bytes()).hexdigest().upper(); print('V4824_PRE_TESTCOUNT_SHA256='+hpre)
assert hpre=='7BD6A6DE3D2138F089708797757196CD382286B7526B68259209C0255500CFE8',hpre
run('post_v4824_mob_test_count_fix.py',out)
hfinal=hashlib.sha256(out.read_bytes()).hexdigest().upper(); print('V4824_FINAL_SOURCE_SHA256='+hfinal)
(WB/'v4824-final-source-sha.txt').write_text(hfinal+'\n',encoding='utf-8')
