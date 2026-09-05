import pathlib,sys,re
p=pathlib.Path(sys.argv[1])
s=p.read_text(encoding='utf-8')
patterns=[
    ('return total==15&&pass==15;','return total==16&&pass==16;'),
    ('return total == 15 && pass == 15;','return total == 16 && pass == 16;')
]
changed=0
for a,b in patterns:
    if a in s:
        s=s.replace(a,b,1); changed+=1; break
if changed!=1:
    raise RuntimeError('Expected stale 15/15 MOB model return condition not found exactly once')
if 'MobSkillRowsEight' not in s:
    raise RuntimeError('v4.8.24 MOB test set marker missing')
p.write_text(s,encoding='utf-8',newline='\n')
print('V4824_MOB_MODEL_TEST_COUNT_16=PASS')
