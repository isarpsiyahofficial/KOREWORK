# v4.8.25 Minor CPU root cause — 2026-09-10

Verified against the deterministic v4.8.25 source chain.

- Manual Minor sends three DOWN/UP key pairs per cycle.
- Native fallback transport intentionally holds every key DOWN for ~1000 us and adds a ~75 us release gap.
- `PreciseDelayUs()` implements those waits with `QueryPerformanceCounter` plus `SwitchToThread` / `YieldProcessor`, i.e. active CPU spinning.
- Minor runs at 120 cycles/s in Maximum and 240 cycles/s in Turbo.
- Therefore the 3 x ~1 ms key holds alone account for ~360 ms/s active-spin in Maximum and ~720 ms/s active-spin in Turbo, before release gaps, scheduler spin, transport, locks, UI and other workers.
- `MinorWorker()` also busy-yields during the final sub-millisecond part before each cycle.

Fix constraint: preserve the same 120/240 target cadence and key DOWN/UP visibility while replacing long active-spin waits with a high-resolution waitable-timer + very short final spin. Do not alter Attack, Cure, MOB, Warrior, Priest or UI behavior.