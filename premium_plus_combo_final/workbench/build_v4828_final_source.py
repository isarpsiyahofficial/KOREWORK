import pathlib,hashlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
base=WB/'premiumplus_v4825_final.cpp'
out=WB/'premiumplus_v4828_final.cpp'
subprocess.check_call([sys.executable,str(WB/'build_v4825_final_source.py')],cwd=ROOT)
out.write_bytes(base.read_bytes())
subprocess.check_call([sys.executable,str(WB/'post_v4828_062f_exact_transport.py'),str(out)],cwd=ROOT)
h=hashlib.sha256(out.read_bytes()).hexdigest().upper()
(WB/'v4828-final-source-sha.txt').write_text(h+'\n',encoding='utf-8')
print('V4828_FINAL_SOURCE_SHA256='+h)
