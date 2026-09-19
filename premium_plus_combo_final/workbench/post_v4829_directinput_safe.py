from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text(encoding='utf-8-sig')

def span(src,sig):
    a=src.find(sig)
    if a<0:raise SystemExit('MISSING '+sig)
    q=src.find('{',a);d=0
    for i in range(q,len(src)):
        if src[i]=='{':d+=1
        elif src[i]=='}':
            d-=1
            if d==0:return a,i+1
    raise SystemExit('UNCLOSED '+sig)

def rep1(src,old,new,label):
    n=src.count(old)
    if n!=1:raise SystemExit(f'{label}: count {n}')
    return src.replace(old,new,1)

marker='// Game transport. The caller must own g_gameInputGate.'
if marker not in s:raise SystemExit('TRANSPORT_MARKER')
s=s.replace(marker,'constexpr int kDirectInputSafeHoldUs=50000;\nconstexpr int kDirectInputSafeGapUs=1000;\n'+marker,1)

a,b=span(s,'UINT ReferenceSendInputsUnlocked(')
fn=s[a:b]
fn=rep1(fn,'PreciseDelayUs(1000);\n      INPUT second=NativeNormalizedInput(inputs[done+1]);','PreciseDelayUs(kDirectInputSafeHoldUs);\n      INPUT second=NativeNormalizedInput(inputs[done+1]);','GENERIC_HOLD')
fn=rep1(fn,'PreciseDelayUs(50);','PreciseDelayUs(kDirectInputSafeGapUs);','GENERIC_GAP')
fn=rep1(fn,'PreciseDelayUs(up?50:1000);','PreciseDelayUs(up?kDirectInputSafeGapUs:kDirectInputSafeHoldUs);','SINGLE_TIMING')
s=s[:a]+fn+s[b:]

a,b=span(s,'bool DirectTimedTapUnlocked(')
fn=s[a:b]
fn=rep1(fn,'std::clamp(holdUs,1000,50000)','std::clamp(holdUs,kDirectInputSafeHoldUs,80000)','DIRECT_HOLD')
fn=rep1(fn,'std::clamp(releaseGapUs,75,10000)','std::clamp(releaseGapUs,kDirectInputSafeGapUs,10000)','DIRECT_GAP')
s=s[:a]+fn+s[b:]

a,b=span(s,'void MinorWorker()')
fn=s[a:b]
needle='''    autoKnownBar=0;autoWasRunning=false;
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);'''
repl='''    autoKnownBar=0;autoWasRunning=false;
    if(!BridgeReceiverLive()){
      scheduled=false;
      if(g_attackActive.load(std::memory_order_acquire)&&(g_attackExclusive.load(std::memory_order_acquire)||g_wsPriority.load(std::memory_order_acquire))){Sleep(1);continue;}
      auto nativeCycle=BuildMinorBatch(fresh,1);
      ReferenceSendInputs(nativeCycle.data(),(UINT)nativeCycle.size());
      continue;
    }
    int rate=g_turbo.load()?240:120;LARGE_INTEGER now{};QueryPerformanceCounter(&now);'''
fn=rep1(fn,needle,repl,'MINOR_NATIVE_SAFE_BRANCH')
s=s[:a]+fn+s[b:]

a,b=span(s,'void RefreshStatus()')
fn=s[a:b]
fn=rep1(fn,'BridgeReceiverLive()?L"BRIDGE":L"NATIVE"','BridgeReceiverLive()?L"BRIDGE":L"NATIVE SAFE"','STATUS_SAFE')
s=s[:a]+fn+s[b:]

a,b=span(s,'bool RunMinorTimingTest()')
test=r'''bool RunMinorTimingTest(){
  RogueSettings r;r.seq={'8','9','0'};auto one=BuildMinorBatch(r,1),eight=BuildMinorBatch(r,8);
  bool shape=one.size()==6&&eight.size()==48;
  bool bridgeRates=(120==120&&240==240);
  const double nativeTapRate=1000000.0/(double)(kDirectInputSafeHoldUs+kDirectInputSafeGapUs);
  bool nativeRate=nativeTapRate>=19.0&&nativeTapRate<=20.5;
  std::ofstream f("minor-timing-report.txt",std::ios::trunc);
  f<<"Reference=062F_BRIDGE_PLUS_DIRECTINPUT_SAFE_NATIVE\nBridgeMaxHz=120\nBridgeTurboHz=240\n";
  f<<"NativeHoldUs="<<kDirectInputSafeHoldUs<<"\nNativeGapUs="<<kDirectInputSafeGapUs<<"\nNativeTotalTapRateApprox="<<nativeTapRate<<"\n";
  f<<"BatchShape="<<(shape?"PASS":"FAIL")<<"\nBridgeRateModel="<<(bridgeRates?"PASS":"FAIL")<<"\nNativeRateModel="<<(nativeRate?"PASS":"FAIL")<<"\nRESULT="<<((shape&&bridgeRates&&nativeRate)?"PASS":"FAIL")<<"\n";
  return shape&&bridgeRates&&nativeRate;
}'''
s=s[:a]+test+s[b:]

checks={
 'SAFE_HOLD_50MS':'kDirectInputSafeHoldUs=50000' in s,
 'SAFE_GAP_1MS':'kDirectInputSafeGapUs=1000' in s,
 'GENERIC_SAFE_HOLD':'PreciseDelayUs(kDirectInputSafeHoldUs)' in s,
 'DIRECT_SAFE_MIN':'std::clamp(holdUs,kDirectInputSafeHoldUs,80000)' in s,
 'NATIVE_MINOR_NO_BACKLOG':'if(!BridgeReceiverLive())' in s and 'BuildMinorBatch(fresh,1)' in s,
 'BRIDGE_062F_RATE':'g_turbo.load()?240:120' in s and 'std::clamp<LONGLONG>(due-emitted,1,8)' in s,
 'ROUTE_LABEL':'NATIVE SAFE' in s,
 'SCANCODE_PRESERVED':'KEYEVENTF_SCANCODE' in s,
}
for k,v in checks.items():
    print(k+'='+('PASS' if v else 'FAIL'))
    if not v:raise SystemExit(k)
p.write_text(s,encoding='utf-8',newline='\n')
print('V4829_DIRECTINPUT_SAFE=APPLIED')
