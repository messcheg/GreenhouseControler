# ESP8266 Irrigation Controller

A Wi‑Fi–enabled irrigation controller built on an \*\*ESP8266\*\*, designed for reliable long‑term operation with minimal external dependencies.  

The system combines robust time handling, seasonal scheduling, and a mobile‑friendly web interface to control a single irrigation valve or relay.

---

## Features

### Time Management

- *\*Primary source:\** NTP (`pool.ntp.org`, `time.nist.gov`)
- *\*Fallbacks:\**
 1. Last known time restored from LittleFS, corrected using `millis()`
 2. Firmware build time (`__DATE__`, `__TIME__`)

- *\*Timezone aware:\** CET / CEST via `configTime()`
- Periodic time persistence (every 5 minutes) to survive power loss
- Safe handling of `millis()` rollover

---
### Scheduling

- Up to *\*10 schedule slots\**
- Each slot includes:
 - Hour \& minute
 - Action (ON or OFF)
- Optional seasonal date range
 - Per‑slot active / inactive switch

#### Seasonal Date Logic

Supports both:
- *\*Normal ranges\**  
  Example: `Mar 15 → Sep 30`
- *\*Inverted “active‑outside‑gap” ranges\**  
  Example: `Apr 20 → Apr 5`  
  (Active year‑round *except* Apr 6–19)

Boundary dates are inclusive and work correctly across year boundaries without relying on day‑of‑year calculations.

---
### Control Model

- *\*State‑based scheduling\** (not edge‑triggered)
- At any moment, the controller computes the correct output state
- Correct behavior after reboot or time correction
- Schedule is sorted by time
- If no slot applies yet today, logic falls back to *\*yesterday’s last active slot\**
- GPIO writes are guarded to prevent unnecessary relay toggling

---
