# iwd P2P (Wi-Fi Direct) Extensions

## 1.1 Wi-Fi Display (WFD/Miracast) Session Management
- [x] Create `src/p2p-display.c` and `src/p2p-display.h`
- [x] WFD session state machine and D-Bus interface
- [x] Hook WFD IEs into P2P frames in `src/p2p.c`

## 1.2 Full P2P Group Owner (GO) Mode
- [x] Enhance GO negotiation (intent 0-15 + tiebreaker) — `p2p_resolve_go_intent()`
- [x] GO already uses `l_dhcp_server` via `src/ap.c`
- [x] `net.connman.iwd.p2p.Group` D-Bus interface — Disconnect/Frequency/Persistent

## 1.3 Persistent P2P Groups
- [x] Create `src/p2p-storage.c` and `src/p2p-storage.h`
- [x] Credential save/load for persistent groups
- [x] Wire reinvocation into `src/p2p.c` (TODO stubs in place)

## 1.4 P2P Concurrency (STA + P2P)
- [x] `p2p_check_concurrency()` stub (TODO: parse NL80211_ATTR_INTERFACE_COMBINATIONS)
- [x] Concurrency check integrated into GO start flow

## 1.5 Build System
- [x] `configure.ac` — `--enable-p2p-display`
- [x] `Makefile.am` — new source files and tests

## 1.6 Unit Tests
- [x] `unit/test-p2p-ie.c` — WFD IE round-trip (5 tests)
- [x] `unit/test-p2p-storage.c` — persistent group I/O (5 tests)
