import pathlib,sys
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
old="""      m8/=64.0;double coarse=ncc(c8,64,m8,kBattleCryTemplate8,tm8,te8);
      if(coarse<0.40)continue;"""
new="""      m8/=64.0;double coarse=ncc(c8,64,m8,kBattleCryTemplate8,tm8,te8);
      // Nearly exact legacy signature is conclusive; live variants continue to fine matching.
      if(coarse>=0.94){if(bestScore)*bestScore=coarse;return true;}
      if(coarse<0.40)continue;"""
if old not in s: raise RuntimeError('Battle Cry coarse insertion point missing')
s=s.replace(old,new,1)
if 'coarse>=0.94' not in s or 'kBattleCryFineA' not in s or 'return best>=0.60' not in s:
    raise RuntimeError('Battle Cry dual-stage guard failed')
p.write_text(s,encoding='utf-8',newline='\n')
print('BATTLECRY_COARSE_CONFIRM=PASS')
