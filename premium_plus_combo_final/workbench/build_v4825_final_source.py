import pathlib,hashlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
WB=ROOT/'premium_plus_combo_final'/'workbench'
v24=WB/'premiumplus_v4824_final.cpp'
out=WB/'premiumplus_v4825_final.cpp'
subprocess.check_call([sys.executable,str(WB/'build_v4824_final_source.py')],cwd=ROOT)
h24=hashlib.sha256(v24.read_bytes()).hexdigest().upper()
print('V4824_SOURCE_SHA256='+h24)
# Exact validated v4.8.24 source checkpoint.
assert h24=='F709BFE06C77F3BAE5AED78F20C99ECDE70B5B4995C351C6B2D1AF7435F082C6',h24
out.write_bytes(v24.read_bytes())
subprocess.check_call([sys.executable,str(WB/'post_v4825_mob_ui_runtime_fix.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_responsive_layout.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_compile_order_fix.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_attack_sideinput_fix.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_minor_cpu_fix.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_warrior_attack_ui_fix.py'),str(out)],cwd=ROOT)
subprocess.check_call([sys.executable,str(WB/'post_v4825_attack_dynamic_visibility_fix.py'),str(out)],cwd=ROOT)
h=hashlib.sha256(out.read_bytes()).hexdigest().upper()
(WB/'v4825-final-source-sha.txt').write_text(h+'\n',encoding='utf-8')
print('V4825_FINAL_SOURCE_SHA256='+h)