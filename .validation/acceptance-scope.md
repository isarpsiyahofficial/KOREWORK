# Complete Offline Fire Drake Acceptance Scope

This branch is not accepted or mergeable until every mandatory gate below passes on the same commit.

- [ ] Linux Release build and complete package
- [ ] Windows Release build and complete package
- [ ] SQL-free KOPACK generated from pinned OpenKO monster, drop, skill and class data
- [ ] Real encrypted Item_Org catalog compiled into KOPACK
- [ ] Runtime rejects missing/corrupt external KOPACK instead of silently accepting fallback
- [ ] Real SMD, N3 character, skeleton, animation, texture and equipment probes
- [ ] Package contains more than 100 MB of pinned KO assets
- [ ] Three independent local character slots
- [ ] Warrior, Rogue, Mage and Priest base stats and skills
- [ ] Per-character save/load, inventory, equipment, stat points and upgrades
- [ ] Local quest, merchant, healer and SMD warp interaction state
- [ ] No login server, game server, AI server, SQL server, localhost service or runtime socket dependency

The validation marker in `.validation/complete-offline-package.txt` is authoritative only when `validated_commit` equals the branch head.
