import pathlib,sys
p=pathlib.Path(sys.argv[1]);s=p.read_text(encoding='utf-8')
def one(a,b,n):
    global s
    c=s.count(a)
    if c!=1: raise SystemExit(f'{n}: expected 1 got {c}')
    s=s.replace(a,b,1)

# Restore the exact live-working 062F native pulse contract.
one('constexpr int kMinorNativeHoldUs=300;','constexpr int kMinorNativeHoldUs=1000;','minor hold')
one('constexpr int kMinorNativeGapUs=30;','constexpr int kMinorNativeGapUs=75;','minor gap')

# Guard the generic transport: exact 062F single-event fallback contract.
required=[
 'PreciseDelayUs(1000);\n      INPUT second=NativeNormalizedInput(inputs[done+1]);',
 'PreciseDelayUs(75);',
 'NativeNormalizedInput',
 'KEYEVENTF_SCANCODE',
 'FifoTicketGuard',
]
for x in required:
    if x not in s: raise SystemExit('062F transport guard missing: '+x[:80])

# Do not alter 120/240 manual cadence; this is the known live-working reference behavior.
if 'g_turbo.load()?240:120' not in s: raise SystemExit('minor cadence guard missing')

p.write_text(s,encoding='utf-8',newline='\n')
print('V4828_062F_EXACT_TRANSPORT=APPLIED')
