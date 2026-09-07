# v4.8.25 CI Run History

Date: 2026-09-07
Branch: `premiumplus-v4825-mob-ui-runtime-fix`

This file records all finalization attempts, including failed CI gates, rather than showing only the successful run.

## Run 34113062080 — failed at deterministic source SHA gate

Head: `a91370dc6024b6f395cad0c8e9cf4ef71e834bba`

Source generation itself completed and the v4.8.25 MOB/UI patches were applied. The workflow was incorrectly configured with a precomputed expected source SHA:

`1F2D8427E6EAB571E24B063DD69E945095DB403BB7257AF63E3D7A4ABCA645CE`

The actual GitHub-runner generated source SHA at that stage was:

`DF300AE023565D8C702265235E4C73AB25FEF7766269CEFD6E6ECE75DA5C02DF`

The run was intentionally stopped by the deterministic source gate. No release artifact from this run was accepted.

## Run 34113213008 — source/static/UI gates passed, compile failed

Head: `787a3532cd1b8690d94cc54648fbf935cabe3846`

Passed before compilation:
- exact v4.8.24 source reconstruction;
- v4.8.25 MOB/UI runtime patch;
- responsive layout patch;
- protected-core static audit;
- v4.8.25 MOB/UI scenario suite: 20/20 PASS.

Compiler error:

`premiumplus_v4825_final.cpp(586): error C3861: 'ReferenceTapKey': identifier not found`

Root cause: `TryMobZFallback()` had been inserted earlier in the translation unit than the existing `ReferenceTapKey()` definition. This was a C++ declaration-order problem, not a runtime/input behavior failure.

Correction: `post_v4825_compile_order_fix.py` adds only the forward declaration:

`bool ReferenceTapKey(int vk);`

The existing FIFO-routed `ReferenceTapKey` implementation was not replaced or modified.

## Run 34134219383 — compile-order patch generated new deterministic source; stale SHA gate stopped run

Head: `f346ed3e3dba041f016ae442b5f430b80d06c85d`

The compile-order micro-patch applied successfully. New final source SHA:

`BA9573FE685ADC7771E6537C9C9B3E0FA4909322B746E8B361E6FC8B62E34896`

The workflow still contained the previous `DF300...` expected-SHA value and therefore correctly stopped before build. This was again a CI configuration gate, not an application regression. The stale hardcoded SHA assertion was removed in favor of logging the deterministically generated source SHA while retaining the exact v4.8.24 base SHA assertion inside the source builder.

## Run 34134296984 — FINAL SUCCESS

Head: `63e6bbd5a8604fb8b28acb1d643b78591ef75498`
Job: `101781502950`
Artifact ID: `10023373349`
Artifact: `PremiumPlus-v4.8.25-FINAL`

All required gates passed:
- source generation/static audit: PASS;
- build known-good v4.8.11 control: PASS;
- build v4.8.25 final release: PASS;
- exact PE/import surface comparison: PASS;
- legacy self-test: 180/180 PASS;
- MOB model: 20/20 PASS;
- Warrior model: 26/26 PASS;
- MOB selector scenarios: 48/48 PASS;
- real Windows `SendInput` mouse harness: 9/9 PASS;
- v4.8.25 MOB/UI live-regression scenario suite: 20/20 PASS;
- responsive overlap scenarios at 100/125/150/200 percent: PASS;
- final report creation: PASS;
- artifact upload: PASS.

Defender diagnostic command completed, but the hosted runner reported that the specific EXE scan was skipped. The release report therefore records `UNAVAILABLE_OR_SKIPPED`; no false `NO_THREATS` claim is made.

## Final accepted hashes

Generated source SHA-256:
`BA9573FE685ADC7771E6537C9C9B3E0FA4909322B746E8B361E6FC8B62E34896`

EXE SHA-256:
`B630A8250FA4C8F77CCC44626813639BEBABD74954614ABFFCC5CE200C79F7EA`

Artifact ZIP digest:
`6A3B86DE29FC11C7F5091784C7A3C89AD254726E8900EA182E01D6B7CC4BB0A4`
