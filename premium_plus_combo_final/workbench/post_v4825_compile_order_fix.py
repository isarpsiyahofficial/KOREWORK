import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
needle='\n\nbool TryMobZFallback(HWND game,const MobSettings&m){'
if s.count(needle)!=1:
    raise RuntimeError(f'TryMobZFallback insertion point count={s.count(needle)}')
# v4.8.25 inserts TryMobZFallback before the later ReferenceTapKey definition.
# A forward declaration preserves the exact existing FIFO-routed ReferenceTapKey
# implementation; no transport/runtime behavior is changed here.
s=s.replace(needle,'\n\nbool ReferenceTapKey(int vk);\n\nbool TryMobZFallback(HWND game,const MobSettings&m){',1)
p.write_text(s,encoding='utf-8')
print('V4825_COMPILE_ORDER_FIX=PASS')
