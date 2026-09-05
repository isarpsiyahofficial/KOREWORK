import pathlib,struct,hashlib
WB=pathlib.Path(__file__).resolve().parent
control=WB/'PremiumPlusCombo-v4.8.11-CONTROL.exe'
release=WB/'PremiumPlusCombo-v4.8.24-FINAL.exe'
def pe(path):
    b=path.read_bytes();po=struct.unpack_from('<I',b,0x3c)[0];co=po+4;n=struct.unpack_from('<H',b,co+2)[0];op=co+20;dc=struct.unpack_from('<H',b,op+0x46)[0];return b,co,op,n,dc
def rva(b,co,op,x):
    n=struct.unpack_from('<H',b,co+2)[0];sz=struct.unpack_from('<H',b,co+16)[0];sec=co+20+sz
    for i in range(n):
        o=sec+i*40;vsz,va,rs,raw=struct.unpack_from('<IIII',b,o+8)
        if va<=x<va+max(vsz,rs):return raw+x-va
    raise ValueError(x)
def cs(b,o):
    e=b.find(b'\0',o);return b[o:e].decode('ascii','replace')
def imp(path):
    b,co,op,n,dc=pe(path);ir,_=struct.unpack_from('<II',b,op+0x78);out=set();d=rva(b,co,op,ir)
    while True:
        oft,ts,fc,nr,ft=struct.unpack_from('<IIIII',b,d)
        if not(oft or nr or ft):break
        dll=cs(b,rva(b,co,op,nr)).lower();to=rva(b,co,op,oft or ft);j=0
        while True:
            v=struct.unpack_from('<Q',b,to+j*8)[0]
            if not v:break
            name='#'+str(v&0xffff) if v&(1<<63) else cs(b,rva(b,co,op,v)+2)
            out.add(dll+'!'+name);j+=1
        d+=20
    return out
cb,cc,co,cn,cd=pe(control);rb,rc,ro,rn,rd=pe(release)
assert len(cb)==419328 and cn==7 and cd==0x8160
assert rn==7 and rd==0x8160
ci,ri=imp(control),imp(release)
extra=sorted(ri-ci);missing=sorted(ci-ri)
(WB/'import-audit.txt').write_text(f'CONTROL_IMPORTS={len(ci)}\nFINAL_IMPORTS={len(ri)}\nEXTRA={extra}\nMISSING={missing}\n',encoding='utf-8')
if ci!=ri:raise SystemExit(f'IMPORT_SURFACE_CHANGED extra={extra!r} missing={missing!r}')
print('EXACT_IMPORT_SURFACE_MATCH=PASS')
print('FINAL_EXE_SHA256='+hashlib.sha256(rb).hexdigest().upper())
