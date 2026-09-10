#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#pragma comment(lib,"winmm.lib")

static void WaitUntil(LONGLONG target,LONGLONG freq){
  LARGE_INTEGER now{};
  const LONGLONG spinTicks=std::max<LONGLONG>(1,freq/10000);
  const LONGLONG sleepFloor=std::max<LONGLONG>(spinTicks+1,freq/2500);
  for(;;){
    QueryPerformanceCounter(&now);LONGLONG left=target-now.QuadPart;if(left<=0)break;
    if(left>sleepFloor){Sleep(1);continue;}
    if(left>spinTicks){Sleep(0);continue;}
    YieldProcessor();
  }
}
static void DelayUs(int us,LONGLONG freq){
  if(us<=0)return;
  if(us>=900){
    LARGE_INTEGER begin{},now{};QueryPerformanceCounter(&begin);Sleep(1);
    const LONGLONG target=begin.QuadPart+std::max<LONGLONG>(1,(freq*(LONGLONG)us)/1000000LL);
    QueryPerformanceCounter(&now);if(now.QuadPart<target)WaitUntil(target,freq);return;
  }
  LARGE_INTEGER now{};QueryPerformanceCounter(&now);
  WaitUntil(now.QuadPart+std::max<LONGLONG>(1,(freq*(LONGLONG)us)/1000000LL),freq);
}
static unsigned long long Ft64(const FILETIME& f){ULARGE_INTEGER u{};u.LowPart=f.dwLowDateTime;u.HighPart=f.dwHighDateTime;return u.QuadPart;}

int main(){
  timeBeginPeriod(1);
  LARGE_INTEGER fq{},start{},now{};QueryPerformanceFrequency(&fq);
  FILETIME c0{},e0{},k0{},u0{},c1{},e1{},k1{},u1{};
  GetThreadTimes(GetCurrentThread(),&c0,&e0,&k0,&u0);
  QueryPerformanceCounter(&start);
  const int rate=240,cycles=480;const LONGLONG step=std::max<LONGLONG>(1,fq.QuadPart/rate);LONGLONG next=start.QuadPart;
  for(int i=0;i<cycles;i++){
    for(int k=0;k<3;k++){DelayUs(1000,fq.QuadPart);DelayUs(75,fq.QuadPart);}
    next+=step;QueryPerformanceCounter(&now);
    if(now.QuadPart<next)WaitUntil(next,fq.QuadPart);else if(now.QuadPart-next>step*2)next=now.QuadPart;
  }
  QueryPerformanceCounter(&now);GetThreadTimes(GetCurrentThread(),&c1,&e1,&k1,&u1);timeEndPeriod(1);
  double wallMs=1000.0*(double)(now.QuadPart-start.QuadPart)/(double)fq.QuadPart;
  double cpuMs=(double)((Ft64(k1)-Ft64(k0))+(Ft64(u1)-Ft64(u0)))/10000.0;
  double hz=wallMs>0?cycles*1000.0/wallMs:0.0;double cpuPct=wallMs>0?100.0*cpuMs/wallMs:100.0;
  bool cadence=hz>=225.0&&hz<=255.0;bool lowCpu=cpuPct<=30.0;
  std::ofstream f("minor-cpu-benchmark-report.txt",std::ios::trunc);
  f<<"TargetHz=240\nMeasuredHz="<<hz<<"\nWallMs="<<wallMs<<"\nThreadCpuMs="<<cpuMs<<"\nThreadCpuPct="<<cpuPct<<"\nCadence="<<(cadence?"PASS":"FAIL")<<"\nLowCpu="<<(lowCpu?"PASS":"FAIL")<<"\nRESULT="<<((cadence&&lowCpu)?"PASS":"FAIL")<<"\n";
  std::cout<<"MeasuredHz="<<hz<<" ThreadCpuPct="<<cpuPct<<"\n";
  return cadence&&lowCpu?0:1;
}
