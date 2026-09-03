import pathlib, re, sys

src = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2])
s = src.read_text(encoding='utf-8')

# Production EXE must not expose or retain command-line self-test/observer entry points.
# Those paths stay in the separately-built test executable, where CI still runs them.
test_switches = [
    ('--self-test', 'RunSelfTest()?0:2'),
    ('--transport-observer-test', 'RunTransportObserverTest()?0:3'),
    ('--attack-observer-test', 'RunAttackObserverTest()?0:6'),
    ('--attack-z-off-observer-test', 'RunAttackZOffObserverTest()?0:8'),
    ('--auto-minor-observer-test', 'RunAutoMinorObserverTest()?0:9'),
    ('--cure-observer-test', 'RunCureObserverTest()?0:7'),
    ('--mob-model-test', 'RunMobModelTest()?0:10'),
]
for sw, ret in test_switches:
    exact = f'if(cmd&&wcsstr(cmd,L"{sw}"))return {ret};'
    count = s.count(exact)
    if count != 1:
        raise RuntimeError(f'production strip: {sw} expected once, got {count}')
    s = s.replace(exact, '', 1)

# The optional second position mapping currently has no producer in this repository.
# Keep the UI/range model fail-closed, but do not ship an unused OpenFileMapping path
# in the production binary. The existing v4.8.11 game-input bridge is untouched.
start = s.find('bool ReadPosition(double&x,double&z){')
if start < 0:
    raise RuntimeError('ReadPosition not found')
brace = s.find('{', start)
depth = 0
end = None
for i in range(brace, len(s)):
    if s[i] == '{': depth += 1
    elif s[i] == '}':
        depth -= 1
        if depth == 0:
            end = i + 1
            break
if end is None:
    raise RuntimeError('ReadPosition unclosed')
s = s[:start] + 'bool ReadPosition(double&,double&){return false;}' + s[end:]

# Remove the sole production cleanup call to the unused optional position mapping.
s = s.replace('ClosePositionBridge();CloseBridge();', 'CloseBridge();', 1)

# Build identity only; runtime logic remains the same.
s = s.replace('constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue";',
              'constexpr wchar_t kTitle[] = L"Premium Plus Combo - Rogue | v4.8.14";', 1)

# Guardrails: no test dispatch should remain in production source.
for sw, _ in test_switches:
    if sw in s:
        raise RuntimeError('test switch remains in production source: ' + sw)
if 'bool ReadPosition(double&,double&){return false;}' not in s:
    raise RuntimeError('fail-closed position stub missing')

out.write_text(s, encoding='utf-8', newline='\n')
print('PRODUCTION_CLEAN=PASS')
print('OUTPUT=' + str(out))
