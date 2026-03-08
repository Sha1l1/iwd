# iwd P2P (Wi-Fi Direct) Extensions — Implementation Plan

## Overview

Extend iwd's existing P2P support with WFD/Miracast session management, full GO mode, persistent groups, P2P Service Discovery (Bonjour/UPnP), and STA+P2P concurrency. Adds ~8 new source files and ~3 unit test files.

**iwd source:** `C:\Users\win10\Downloads\iwed\ell\iwd`

> [!NOTE]
> Ethernet support is skipped — already implemented as the EAD daemon in `wired/`.

---

## Proposed Changes

### 1. Wi-Fi Display (Miracast) — Session Management

#### [NEW] [p2p-display.h](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p-display.h)
#### [NEW] [p2p-display.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p-display.c)
- `net.connman.iwd.p2p.Display` D-Bus interface
- WFD session lifecycle (StartSession/StopSession, timeouts)
- Hooks into existing `p2p_build_wfd_ie()` / `p2p_extract_wfd_properties()`

#### [MODIFY] [p2p.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p.c)
- Connect WFD display module to scan/frame exchange paths

---

### 2. Full P2P Group Owner (GO) Mode

#### [MODIFY] [p2p.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p.c)
- `struct p2p_group` with `l_dhcp_server`, client list, passphrase
- Full GO intent negotiation (0–15 + tiebreaker)
- `net.connman.iwd.p2p.Group` D-Bus interface

---

### 3. Persistent P2P Groups

#### [NEW] [p2p-storage.h](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p-storage.h)
#### [NEW] [p2p-storage.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p-storage.c)
- Save/load persistent group credentials via `l_settings` to `/var/lib/iwd/p2p/`
- INI format: `[PersistentGroup]` with SSID, Passphrase, DeviceAddress, Role

#### [MODIFY] [p2p.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p.c)
- Persistent group reinvocation in connection flow

---

### 4. P2P Concurrency (STA + P2P)

#### [MODIFY] [p2p.c](file:///C:/Users/win10/Downloads/iwed/ell/iwd/src/p2p.c)
- `p2p_check_concurrency()` — parse `NL80211_ATTR_INTERFACE_COMBINATIONS`
- Dedicated P2P virtual interface management

---

### 5. Build System & iwctl

#### [MODIFY] [configure.ac](file:///C:/Users/win10/Downloads/iwed/ell/iwd/configure.ac)
- `--enable-p2p-display` flag

#### [MODIFY] [Makefile.am](file:///C:/Users/win10/Downloads/iwed/ell/iwd/Makefile.am)
- New source files + unit test targets

---

## Verification Plan

### Unit Tests (new)
| Test | What it validates |
|------|------------------|
| `unit/test-p2p-ie.c` | WFD IE encode/decode round-trip |

| `unit/test-p2p-storage.c` | Persistent group credential I/O |

### Existing Tests
- `unit/test-p2p` (40KB) must continue to pass
