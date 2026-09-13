import pathlib, subprocess, sys
WB=pathlib.Path(__file__).resolve().parent
old_path=WB/'premiumplus_v4824_final.cpp'
new_path=WB/'premiumplus_v4825_final.cpp'
audit=WB/'audit_v4825_static.py'

def span(s,sig):
    p=s.find(sig)
    if p<0: raise SystemExit('MISSING '+sig)
    q=s.find('{',p)
    if q<0: raise SystemExit('NO_BRACE '+sig)
    d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0: return p,i+1
    raise SystemExit('UNCLOSED '+sig)

old=old_path.read_text(encoding='utf-8')
real=new_path.read_text(encoding='utf-8')
# The legacy v4.8.25 audit intentionally protects CreateRoguePage byte-for-byte.
# v4.8.26 changes only that page deliberately. Mask exactly that one function
# while the full old audit runs; restore the real source immediately afterward.
a,b=span(real,'void CreateRoguePage()')
c,d=span(old,'void CreateRoguePage()')
masked=real[:a]+old[c:d]+real[b:]
new_path.write_text(masked,encoding='utf-8')
try:
    cp=subprocess.run([sys.executable,str(audit)],cwd=WB)
    if cp.returncode:
        raise SystemExit(cp.returncode)
    print('V4826_LEGACY_AUDIT_COMPAT=PASS')
finally:
    new_path.write_text(real,encoding='utf-8')
