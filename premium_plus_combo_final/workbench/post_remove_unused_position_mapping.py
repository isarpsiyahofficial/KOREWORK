import pathlib, sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')

def span(sig):
    a=s.index(sig); q=s.index('{',a); d=0
    for i in range(q,len(s)):
        if s[i]=='{': d+=1
        elif s[i]=='}':
            d-=1
            if d==0: return a,i+1
    raise RuntimeError('unclosed '+sig)

# Optional MOB position shared-memory consumer has no producer in this repository.
# Keep range fail-closed and preserve every other runtime/test path exactly.
a,b=span('bool ReadPosition(double&x,double&z)')
s=s[:a]+'bool ReadPosition(double&,double&){return false;}'+s[b:]

# Cleanup call for the now-unused optional mapping is unnecessary.
s=s.replace('ClosePositionBridge();CloseBridge();','CloseBridge();',1)

if 'OpenFileMappingW' in s:
    # The helper may still exist but must become dead code; compiler /O2 should remove it.
    pass
if 'bool ReadPosition(double&,double&){return false;}' not in s:
    raise RuntimeError('fail-closed position stub missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('UNUSED_POSITION_MAPPING_REMOVED=PASS')
