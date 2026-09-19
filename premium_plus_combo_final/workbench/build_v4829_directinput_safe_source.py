import hashlib,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
base=WB/'premiumplus_v4828_062f_input.cpp'
out=WB/'premiumplus_v4829_directinput_safe.cpp'
subprocess.check_call([sys.executable,str(WB/'build_v4828_062f_input_source.py')],cwd=ROOT)
if not base.exists(): raise SystemExit('V4828_SOURCE_MISSING')
out.write_bytes(base.read_bytes())
subprocess.check_call([sys.executable,str(WB/'post_v4829_directinput_safe.py'),str(out)],cwd=ROOT)
h=hashlib.sha256(out.read_bytes()).hexdigest().upper()
(WB/'v4829-source-sha.txt').write_text(h+'\n',encoding='utf-8')
print('V4829_SOURCE_SHA256='+h)
