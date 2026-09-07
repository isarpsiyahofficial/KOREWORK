# PremiumPlusCombo v4.8.25 FINAL — Final Delivery and Validation Report

Date: 2026-09-07
Repository: isarpsiyahofficial/KOREWORK
Development branch: premiumplus-v4825-mob-ui-runtime-fix
Validated workflow run: 34134296984
Validated workflow head: 63e6bbd5a8604fb8b28acb1d643b78591ef75498
Artifact ID: 10023373349
Artifact name: PremiumPlus-v4.8.25-FINAL
Artifact archive SHA-256: 6A3B86DE29FC11C7F5091784C7A3C89AD254726E8900EA182E01D6B7CC4BB0A4

## Final binaries

EXE: PremiumPlusCombo-v4.8.25-FINAL.exe
EXE size: 611840 bytes
EXE SHA-256: B630A8250FA4C8F77CCC44626813639BEBABD74954614ABFFCC5CE200C79F7EA
Final generated source SHA-256: BA9573FE685ADC7771E6537C9C9B3E0FA4909322B746E8B361E6FC8B62E34896
Validated v4.8.24 source checkpoint SHA-256: F709BFE06C77F3BAE5AED78F20C99ECDE70B5B4995C351C6B2D1AF7435F082C6
Known-good v4.8.11 source SHA-256: 3AECB38864D0C248F926C9F15A8FF7F5BE4A1636ACAB966ADC40DA350C142A42
Known-good original v4.8.11 EXE SHA-256: E9B5C7ECD2933EEA64EF096494CF428E2D7F410D43CB32628C7459640B85C936

## Final CI gates

- Stable runtime core preservation: PASS
- Legacy self-test: 180/180 PASS
- MOB model test: 20/20 PASS
- Warrior model test: 26/26 PASS
- MOB target selector scenario model: 48/48 PASS
- Real Windows SendInput mouse selection harness: 9/9 PASS
- v4.8.25 MOB/UI live-regression scenario model: 20/20 PASS
- Responsive layout model at 100%, 125%, 150%, 200%: PASS
- Exact import surface vs known-good v4.8.11: PASS
- Control imports: 163
- Final imports: 163
- Extra imports: none
- Missing imports: none
- PE section count / DLL characteristics compatibility gate: PASS
- Defender diagnostic command: completed, but the hosted runner reported the file scan as skipped; therefore no 'Defender clean' claim is made.

## v4.8.25 live-regression fixes

### Sidebar and Priest

The PRIEST category had been omitted from the owner-draw sidebar category set and therefore inherited the gold/orange action-button rendering. v4.8.25 explicitly includes IDC_CATEGORY_PRIEST in the normal sidebar style group. PRIEST now has the same category visual treatment as ROGUE, WARRIOR, ATTACK and MOB ATTACK.

### MOB ATTACK tab isolation

The v4.8.24 RefreshMobLists path could re-show skill row controls even when the HEDEF tab was active. This caused HEDEF, SKILL and status/save controls to visually overlap. v4.8.25 makes row visibility depend on both the active top-level MOB category and the active MOB sub-tab, then calls ShowMobSubCategory to enforce final visibility. HEDEF, SKILL and SCROLL controls are therefore isolated from one another.

### MOB ATTACK layout and responsive behavior

The MOB page was re-spaced so that:
- top controls, farm/range controls and sub-tabs occupy separate rows;
- seven target records fit before the target status/save area;
- eight skill rows fit before the skill controls/save area;
- the scroll list remains above the common save area.

A responsive child-control layout layer records the logical creation rectangles and rescales the controls with the client size using the already imported MoveWindow/GetClientRect family. No new Windows API import was introduced. Scenario checks cover 100%, 125%, 150% and 200% scaling.

### Thin red / anti-aliased mob nameplates

The previous mob-learning filter was too strict for thin anti-aliased red text such as Troll nameplates. v4.8.25 lowers and splits the red-dominance rule while preserving a red-over-green/blue requirement. The mask builder also accepts thinner red occupancy cells. Neutral gray is explicitly rejected by the scenario suite.

### Mouse-first target acquisition and Z fallback

The preferred target-acquisition path remains visual mouse selection. If the learned red nameplate cannot be acquired because of overlap, occlusion, player positioning or transient visibility loss, a bounded Z fallback can cycle possible game targets.

Z never authorizes an attack by itself. After either mouse selection or Z fallback, the program requires both:
1. visible target HP; and
2. target header/name verification against one of the enabled learned target records.

Only after this closed-loop confirmation is the MOB target marked confirmed. MOB R chase and MOB skill execution both require that confirmed state.

### Wrong-target prevention

A visual candidate, mouse click, Z selection, target HP alone or visual score alone is not sufficient to authorize R/skills. Header verification remains fail-closed. The selector scenario suite includes false positive visual matches, wrong-first-target retry, no-target-HP, overlap, disappearance and reacquisition cases.

### Compile-order correction

The first v4.8.25 compile attempt exposed a declaration-order error: TryMobZFallback was inserted before the later ReferenceTapKey definition. A dedicated micro-patch adds only a forward declaration for the existing FIFO-routed ReferenceTapKey implementation. The transport/input implementation itself was not changed.

## Preserved runtime architecture

The v4.8.25 static audit protects existing runtime bodies and/or established behavior for the prior working modules, including the known-good input transport and the accumulated Rogue/Attack/Warrior/MOB functionality. The release continues to use the same v4.8.11-compatible Windows import surface.

The project remains a non-invasive keyboard/mouse/image-recognition automation layer. No packet modification, DLL injection, anti-cheat bypass, memory patching, cooldown bypass or stat/damage bypass was introduced in this release.

## Historical feature chain preserved through v4.8.25

The release includes the prior accumulated work: Rogue Minor/Cure, Attack and W/S logic, HP/MP visual calibration, dynamic Attack skills, MOB skills/scrolls, R chase, separate Priest category, Warrior 28-slot equipment right-click toggle, inventory calibration/fallback, single KALKAN-SILAH hotkey, Descent and optional post-Descent bar restore, automatic timed skills, Battle Cry visual verification/retry behavior, Attack R Attack option, farm/range controls, maximum seven MOB target records, partial/exact target-name modes and target verification before MOB attack.

## Defender note

The GitHub Windows runner executed MpCmdRun and produced:

Scan starting...
Scan finished.
Scanning ... PremiumPlusCombo-v4.8.25-FINAL.exe was skipped.

This is recorded as UNAVAILABLE_OR_SKIPPED rather than NO_THREATS. The release does, however, pass the exact v4.8.11 import-surface comparison (163/163, EXTRA=[], MISSING=[]), which was retained specifically because the known-good v4.8.11 binary did not exhibit the earlier production-shape problem.

## Validation boundary

CI validates build integrity, regression behavior, model scenarios and real Windows SendInput mouse dispatch. It cannot reproduce every Knight Online/private-client rendering state, camera angle, frame delay or server-specific UI asset. Live game testing remains authoritative for those environmental variables. The code is designed to fail closed when target verification is insufficient instead of knowingly starting MOB R/skills on an unconfirmed target.
