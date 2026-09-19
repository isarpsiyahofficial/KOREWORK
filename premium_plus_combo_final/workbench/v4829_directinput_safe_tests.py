from pathlib import Path
import sys
WB=Path(__file__).resolve().parent
s=(WB/'premiumplus_v4829_directinput_safe.cpp').read_text(encoding='utf-8')

def fn(sig):
    a=s.index(sig);q=s.index('{',a);d=0
    for i in range(q,len(s)):
        if s[i]=='{':d+=1
        elif s[i]=='}':
            d-=1
            if d==0:return s[a:i+1]
    raise RuntimeError(sig)

transport=fn('UINT ReferenceSendInputsUnlocked(')
direct=fn('bool DirectTimedTapUnlocked(')
minor=fn('void MinorWorker()')
status=fn('void RefreshStatus()')
timing=fn('bool RunMinorTimingTest()')
checks={
 'MeasuredHold50ms':'constexpr int kDirectInputSafeHoldUs=50000;' in s,
 'MeasuredGap1ms':'constexpr int kDirectInputSafeGapUs=1000;' in s,
 'BridgeStillFirst':'BridgeReceiverLive()&&PublishBridgeInputsUnlocked(inputs,count)' in transport,
 'NativePairHold50ms':'PreciseDelayUs(kDirectInputSafeHoldUs)' in transport,
 'NativePairGap1ms':'PreciseDelayUs(kDirectInputSafeGapUs)' in transport,
 'DirectTimedMinimum50ms':'std::clamp(holdUs,kDirectInputSafeHoldUs,80000)' in direct,
 'ManualNativeOneCycle':'if(!BridgeReceiverLive())' in minor and 'BuildMinorBatch(fresh,1)' in minor,
 'NativeNoImpossibleBurstBacklog':'BuildMinorBatch(fresh,1)' in minor,
 'BridgeBurstPreserved':'g_turbo.load()?240:120' in minor and 'std::clamp<LONGLONG>(due-emitted,1,8)' in minor,
 'AutoMinorPreserved':'g_autoMinorOwned.load' in minor and 'autoMinorBar' in minor,
 'RouteVisible':'NATIVE SAFE' in status and 'BRIDGE' in status,
 'TimingReportMeasuredIdentity':'NativeHoldUs=' in timing and 'NativeTotalTapRateApprox=' in timing,
 'ScanCodeTransportPreserved':'KEYEVENTF_SCANCODE' in s,
 'NoV4825ShortPulseInManual':'MinorSendInputsLowCpu(manualBatch' not in minor,
}
passed=sum(checks.values())
for k,v in checks.items():print(f'{k}={"PASS" if v else "FAIL"}')
print(f'TOTAL={len(checks)}\nPASSED={passed}')
sys.exit(0 if passed==len(checks) else 1)
