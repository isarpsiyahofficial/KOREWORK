from __future__ import annotations
from dataclasses import dataclass, field
import math,re,sys

BUILTIN = ["Pride","Gluttony","Wrath","Sloth","Lust","Envy","Greed"]

def norm(s:str)->str:
    s=s.casefold().strip()
    s=re.sub(r"[^a-z0-9]+"," ",s)
    return " ".join(s.split())

def name_match(query:str, observed:str, exact:bool)->bool:
    q,o=norm(query),norm(observed)
    if not q or not o:return False
    if exact:return o==q
    return o==q or o.startswith(q+" ") or (" "+q+" ") in (" "+o+" ")

@dataclass
class Rule:
    label:str
    exact:bool=False
    name_enabled:bool=True
    visual_enabled:bool=True
    max_range:float=48.0
    visual_threshold:float=.66

@dataclass
class Candidate:
    cid:int
    observed_name:str
    x:float
    z:float
    visual_scores:list[float]=field(default_factory=list)
    nameplate_visible:bool=True
    body_visible:bool=True
    body_occlusion:float=0.0
    click_results:list[tuple[str,bool]]=field(default_factory=list)
    visual_scale:float=1.0
    brightness:float=1.0

@dataclass
class Trace:
    clicks:list[tuple[int,int]]=field(default_factory=list)
    rejected:list[int]=field(default_factory=list)
    confirmed:int|None=None
    r_count:int=0
    skills_started:bool=False
    reason:str=""

def distance(c:Candidate, ax=0.0, az=0.0): return math.hypot(c.x-ax,c.z-az)

def candidate_score(rule:Rule,c:Candidate)->float:
    if distance(c)>rule.max_range:return -1
    name_ok = name_match(rule.label,c.observed_name,rule.exact) if rule.name_enabled else True
    if rule.name_enabled and not name_ok:return -1
    vis=max(c.visual_scores, default=0.0)
    vis-=min(.10, abs(c.visual_scale-1.0)*.12)
    vis-=min(.08, abs(c.brightness-1.0)*.08)
    if rule.visual_enabled and vis<rule.visual_threshold:
        if not (c.nameplate_visible and rule.name_enabled and name_ok): return -1
        vis=max(vis,.70)
    return .62*vis + .38*(1.0 if name_ok else .0)

def selected_name_matches(rule:Rule, selected_name:str)->bool:
    return name_match(rule.label, selected_name, rule.exact) if rule.name_enabled else bool(norm(selected_name))

def select(rule:Rule,cands:list[Candidate],anchor=(0.,0.),max_offsets=4)->Trace:
    tr=Trace(); rejected=set()
    while True:
        ranked=[]
        for c in cands:
            if c.cid in rejected:continue
            sc=candidate_score(rule,c)
            if sc>=0:ranked.append((sc,-distance(c,*anchor),c))
        if not ranked:
            tr.reason="no-safe-candidate";return tr
        ranked.sort(key=lambda x:(x[0],x[1]),reverse=True)
        c=ranked[0][2]
        for oi in range(min(max_offsets,max(1,len(c.click_results)))):
            tr.clicks.append((c.cid,oi))
            sel,hp = c.click_results[oi] if oi<len(c.click_results) else ("",False)
            if hp and selected_name_matches(rule,sel):
                tr.confirmed=c.cid;tr.r_count=1;tr.skills_started=True;tr.reason="confirmed";return tr
        rejected.add(c.cid);tr.rejected.append(c.cid)

pass_n=0;total=0
out=[]
def T(name,cond):
    global pass_n,total
    total+=1;pass_n+=bool(cond);out.append(f"{name}={'PASS' if cond else 'FAIL'}")

def C(cid,name,x=10,z=10,vis=(.9,),click=None,**kw):
    return Candidate(cid,name,x,z,list(vis),click_results=click or [(name,True)],**kw)

T('NormalizeCase', name_match('pRiDe','PRIDE',True))
T('ContainsPrideRandom', name_match('Pride','Pride Random',False))
T('ExactRejectsPrideRandom', not name_match('Pride','Pride Random',True))
T('ExactPride', name_match('Pride','Pride',True))
T('NoSubstringInsideWord', not name_match('Pride','Surpride',False))

r=Rule('Pride')
t=select(r,[C(1,'Pride')]);T('ClearPrideSelected',t.confirmed==1 and t.r_count==1 and t.skills_started)
t=select(r,[C(1,'Pride',body_visible=False,body_occlusion=1.0,vis=(.2,),nameplate_visible=True)]);T('BodyFullyHiddenNameplateStillSelects',t.confirmed==1)
t=select(r,[C(1,'Pride',body_visible=False,body_occlusion=1.0,vis=(.2,),nameplate_visible=False)]);T('NothingVisibleNoBlindClick',t.confirmed is None and t.r_count==0)

c=C(1,'Pride',vis=(.9,),click=[('SomePlayer',True),('Pride',True)])
t=select(r,[c]);T('PlayerInsideMobRetryOffset',t.confirmed==1 and len(t.clicks)==2 and t.r_count==1)
T('NoRBeforeConfirm',t.confirmed==1 and t.r_count==1)

bad=C(1,'Pride',vis=(.97,),click=[('Gluttony',True),('Gluttony',True)])
good=C(2,'Pride',vis=(.88,),click=[('Pride',True)])
t=select(r,[bad,good]);T('WrongTargetRejectedThenCorrect',t.confirmed==2 and 1 in t.rejected and t.r_count==1)

c=C(1,'Pride',click=[('Pride',False),('Pride',False)])
t=select(r,[c]);T('NoTargetHpNoAttack',t.confirmed is None and t.r_count==0 and not t.skills_started)

T('Range48Accepted',select(Rule('Pride',max_range=48),[C(1,'Pride',x=48,z=0)]).confirmed==1)
T('Range49Rejected',select(Rule('Pride',max_range=48),[C(1,'Pride',x=49,z=0)]).confirmed is None)

for i,n in enumerate(BUILTIN,1):
    t=select(Rule(n),[C(i,n,vis=(.91,))])
    T('Builtin_'+n,t.confirmed==i)

for view in range(8):
    scores=[.15]*8;scores[view]=.89
    t=select(Rule('Pride'),[C(1,'Pride',vis=tuple(scores))])
    T(f'MultiViewAngle{view}',t.confirmed==1)

for k,(sc,br) in enumerate([(0.75,.70),(.85,1.25),(1.0,.55),(1.15,1.35),(1.3,.85)]):
    t=select(r,[C(1,'Pride',vis=(.90,),visual_scale=sc,brightness=br)])
    T(f'ScaleBrightness{k}',t.confirmed==1)

c1=C(1,'Pride Random',vis=(.95,),click=[('Pride Random',True)])
c2=C(2,'Pride',vis=(.87,),click=[('Pride',True)])
t=select(Rule('Pride',exact=True),[c1,c2]);T('ExactModeChoosesPlainPride',t.confirmed==2)
t=select(Rule('Pride',exact=False),[c1,c2]);T('ContainsModeAllowsPrideRandom',t.confirmed==1)

T('VisualFalsePositiveBlockedByName',select(r,[C(1,'Wrath',vis=(.99,))]).confirmed is None)
rv=Rule('Pride',name_enabled=False,visual_enabled=True)
t=select(rv,[C(1,'',vis=(.92,),click=[('',False),('Pride',True)])]);T('VisualOnlyNeedsConfirmHeader',t.confirmed==1 and len(t.clicks)==2)

c1=C(1,'Pride',vis=(.96,),click=[('Player',True),('Player',True),('Player',True),('Player',True)])
c2=C(2,'Pride',vis=(.90,),click=[('Pride',True)])
t=select(r,[c1,c2]);T('OverlapExhaustFirstThenSecond',t.confirmed==2 and t.rejected==[1])
T('DisappearDoesNotAttack',select(r,[]).r_count==0)
T('ReacquireThenAttack',select(r,[C(3,'Pride')]).confirmed==3)

for i,n in enumerate(BUILTIN,1):
    t=select(Rule(n),[C(i,n,vis=(.1,),body_visible=False,body_occlusion=1.0,nameplate_visible=True)])
    T('Occluded_'+n,t.confirmed==i)

out += [f'TOTAL={total}',f'PASSED={pass_n}']
print('\n'.join(out))
open('mob-selector-scenario-report.txt','w',encoding='utf-8').write('\n'.join(out)+'\n')
sys.exit(0 if total==pass_n else 1)
