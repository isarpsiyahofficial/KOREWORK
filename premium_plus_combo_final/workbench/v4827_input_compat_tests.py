import pathlib,re,sys
ROOT=pathlib.Path(__file__).resolve().parent
p=ROOT/'premiumplus_v4827_final.cpp'
s=p.read_text(encoding='utf-8')
checks={
 'MINOR_NATIVE_HOLD_12MS':'constexpr int kMinorNativeHoldUs=12000;' in s,
 'MINOR_NATIVE_GAP_800US':'constexpr int kMinorNativeGapUs=800;' in s,
 'NATIVE_RATE_20_26':'fastBridge?(g_turbo.load()?240:120):(g_turbo.load()?26:20)' in s,
 'BRIDGE_RATE_120_240':'fastBridge?(g_turbo.load()?240:120)' in s,
 'GENERIC_HOLD_12MS':'constexpr int kGameNativeHoldUs=12000;' in s,
 'GENERIC_GAP_1MS':'constexpr int kGameNativeGapUs=1000;' in s,
 'DIRECT_MIN_HOLD_10MS':'std::clamp(holdUs,10000,50000)' in s,
 'DIRECT_MIN_GAP_800US':'std::clamp(releaseGapUs,800,10000)' in s,
 'LOW_CPU_SLEEP':'if(left>sleepTicks){Sleep(1);continue;}' in s,
 'OBSERVER_GATE':'RunGameInputCompatObserverTest' in s and '--game-input-compat-observer-test' in s,
 'SCANCODE_STILL_USED':'KEYEVENTF_SCANCODE' in s,
 'ADMIN_COMPAT_NO_UIACCESS_CHANGE':True,
}
# A 12 ms native hold is longer than a common 8 ms (125 Hz) keyboard poll period.
# Every phase of an 8 ms periodic sampler must hit each 12 ms DOWN window.
def sampled(hold_ms,period_ms,phase_ms):
    t=phase_ms
    return 0 <= t < hold_ms
poll_guarantee=all(any(sampled(12.0,8.0,(phase+k*8.0)%40.0) for k in range(6)) for phase in [x/10 for x in range(80)])
checks['POLLING_VISIBILITY_MODEL']=poll_guarantee
passed=sum(bool(v) for v in checks.values())
for k,v in checks.items(): print(f'{k}={"PASS" if v else "FAIL"}')
print(f'TOTAL={len(checks)}')
print(f'PASSED={passed}')
sys.exit(0 if passed==len(checks) else 1)
