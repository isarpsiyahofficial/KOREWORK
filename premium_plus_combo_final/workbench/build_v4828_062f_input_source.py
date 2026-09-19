import hashlib,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
base=WB/'premiumplus_v4825_final.cpp'
out=WB/'premiumplus_v4828_062f_input.cpp'
subprocess.check_call([sys.executable,str(WB/'build_v4825_final_source.py')],cwd=ROOT)
if not base.exists(): raise SystemExit('BASE_SOURCE_MISSING')
out.write_bytes(base.read_bytes())
subprocess.check_call([sys.executable,str(WB/'post_v4828_062f_input_restore.py'),str(out)],cwd=ROOT)
h=hashlib.sha256(out.read_bytes()).hexdigest().upper()
(WB/'v4828-062f-source-sha.txt').write_text(h+'\n',encoding='utf-8')
print('V4828_SOURCE_SHA256='+h)
