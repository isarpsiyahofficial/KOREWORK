from pathlib import Path
import sys
s=Path(__file__).with_name('premiumplus_v4828_final.cpp').read_text(encoding='utf-8')
checks={
'MinorHold1000':'constexpr int kMinorNativeHoldUs=1000;' in s,
'MinorGap75':'constexpr int kMinorNativeGapUs=75;' in s,
'GenericDown1000':'PreciseDelayUs(1000);\n      INPUT second=NativeNormalizedInput(inputs[done+1]);' in s,
'GenericUpGap75':'PreciseDelayUs(75);' in s,
'ScanCode':'KEYEVENTF_SCANCODE' in s,
'Fifo':'FifoTicketGuard' in s,
'Cadence120240':'g_turbo.load()?240:120' in s,
'BridgeSelective':'IsBridgeInputKey' in s,
}
for k,v in checks.items(): print(f'{k}={"PASS" if v else "FAIL"}')
print(f'TOTAL={len(checks)}\nPASSED={sum(checks.values())}')
sys.exit(0 if all(checks.values()) else 1)
