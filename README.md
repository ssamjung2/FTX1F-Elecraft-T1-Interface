# Yaesu FTX-1F → Elecraft T1 ATU Interface
*by KW9D*

An Arduino Pro Micro sketch that reads the 4-bit digital BAND DATA output from the Yaesu FTX-1F TUNER/LINEAR connector and automatically updates the Elecraft T1 ATU with the correct band info — eliminating the need for the expensive official Elecraft interface cable.

I built this for QRP POTA operations so I can use the T1 for HF activity as part of my FTX-1F field setup.

> **Status**: ✅ **TESTED & WORKING** — System fully operational with real Elecraft T1 hardware. Band changes detected correctly, relays engaging properly, protocol verified against official Elecraft T1 manual.

---

## Background

The Elecraft T1 stores a separate set of tuning settings for each amateur band. When band is changed, the T1 needs to be told which band is active so it can recall the correct capacitor/inductor combination. Elecraft sells an official interface cable for supported radios, but at significant cost.

This project follows the approach pioneered by Matthew Robinson (VK6MR) for the Flex-1500 SDR and adapted by Gareth (GI1MIC) for the Yaesu FT-817, replacing the FT-817's analog BAND_DATA pin with the FTX-1F's 4-bit digital BAND DATA output.

---

## Original Projects & References

| Project | Author | Link |
|---|---|---|
| Original T1 interface (Flex-1500) | Matthew Robinson — VK6MR | http://blog.zensunni.org/ |
| FT-817 to Elecraft T1 Interface | Gareth — GI1MIC | https://github.com/gi1mic/FT817-Elecraft-T1-Interface |
| Elecraft T1 ATU manual | Elecraft | https://elecraft.com |
| Adafruit SleepyDog library | Adafruit | https://github.com/adafruit/Adafruit_SleepyDog |

---

## How It Works

1. The Arduino wakes from watchdog sleep every 2 seconds.
2. It reads the four BAND DATA pins (A, B, C, D) from the FTX-1F TUNER/LINEAR connector — a 4-bit word that uniquely identifies the active band.
3. A double-read with a 10 ms gap is used to reject transient readings during band changes.
4. If the band has changed since the last poll, the Arduino initiates the T1 remote-control handshake:
   - Asserts the TUNE line (ring, via NPN transistor) HIGH for 500 ms.
   - Waits for the T1 to pulse the DATA line (tip) HIGH ~50 ms as acknowledgement.
   - Sends the 4-bit T1 band code on the DATA line using the T1's pulse-width encoding.

---

## FTX-1F TUNER/LINEAR Connector

The 10-pin TUNER/LINEAR connector on the rear of the FTX-1F provides:

| Pin | Signal | Description |
|---|---|---|
| TX D (BAND A) | BAND_A | Band data bit A (MSB) |
| RX D (BAND B) | BAND_B | Band data bit B |
| EXTS(DET) BAND C | BAND_C | Band data bit C |
| TRST (BAND D) | BAND_D | Band data bit D (LSB) |
| +13.8V OUT | PWR | Supply voltage |
| GND | GND | Ground |

### FTX-1F Band Data Table

| Band | Freq (MHz) | A | B | C | D | abcd (binary) | T1 Code |
|---|---|---|---|---|---|---|---|
| 160m | 1.8 | H | L | L | L | 0b1000 | 1 |
| 80m | 3.5 | L | H | L | L | 0b0100 | 2 |
| 40m | 7 | L | L | H | H | 0b1100 | 4 |
| 30m | 10 | L | L | H | L | 0b0010 | 5 |
| 20m | 14 | H | L | H | L | 0b1010 | 6 |
| 17m | 18 | L | H | H | L | 0b0110 | 7 |
| 15m | 21 | H | H | H | L | 0b1110 | 8 |
| 12m | 24.5 | H | L | L | L | 0b0001 | 9 |
| 10m | 28 | H | L | L | H | 0b1001 | 10 |
| 6m | 50 | L | H | L | H | 0b0101 | 11 |
| 2m | 144 | H | H | L | H | 0b1101 | — (debug only) |
| 70cm | 430 | H | H | L | L | 0b0011 | — (debug only) |

> **Note:** The FTX-1F includes band detection for 160m through 6m on HF. VHF bands (2m and 70cm) are supported in debug mode for testing, but the T1 cannot tune these bands so they are excluded from normal operation.

---

## Elecraft T1 Remote Interface

The T1 uses a 3.5 mm stereo jack for remote control:

| Jack contact | Signal | Description |
|---|---|---|
| Tip | DATA | Bidirectional band data line |
| Ring | TUNE | Assert LOW for 500 ms to initiate band update |
| Sleeve | GND | Ground |

### T1 Handshake Protocol

1. Assert TUNE (ring) LOW for 500 ms.
2. T1 pulses DATA (tip) HIGH for ~50 ms as acknowledgement.
3. Wait for DATA to return LOW.
4. After 10 ms delay, send 4-bit band code on DATA:
   - **Logic 1**: DATA HIGH for 4 ms
   - **Logic 0**: DATA HIGH for 1.5 ms
   - **Bit gap**: DATA LOW for 1.5 ms between each bit
5. Leave DATA LOW; release to input.

---

## Parts List

| Part | Notes |
|---|---|
| Arduino Pro Micro (5V/16MHz) | ATmega32U4; configure Arduino IDE as **Leonardo** |
| MP1584 buck converter | Set output to ~6V into RAW pin |
| 2N3904 NPN transistor | For TUNE line drive |
| 1.2 kΩ resistor | NPN base resistor (D4 → base) |
| 4 × 4.7 kΩ resistors | Series protection on BAND DATA pins (A0–A3) |
| 3.5 mm stereo jack plug | Connects to Elecraft T1 remote jack |
| Mini-DIN or suitable connector | Connects to FTX-1F TUNER/LINEAR port |

---

## Wiring

### FTX-1F TUNER/LINEAR → Arduino

| FTX-1F Signal | Arduino Pin | Series Resistor |
|---|---|---|
| TX D (BAND A) | A0 | 4.7 kΩ |
| RX D (BAND B) | A1 | 4.7 kΩ |
| EXTS(DET) BAND C | A2 | 4.7 kΩ |
| TRST (BAND D) | A3 | 4.7 kΩ |
| +13.8V OUT | RAW | — |
| GND | GND | — |

### Arduino → Elecraft T1 (3.5 mm jack)

| Arduino Pin | 3.5 mm Jack | Notes |
|---|---|---|
| D3 | Tip (DATA) | Direct connection |
| D4 | Ring (TUNE) | Via 2N3904 NPN: D4 → 1.2 kΩ → base; collector → ring; emitter → GND |
| GND | Sleeve (GND) | |

### NPN Transistor Circuit (TUNE Line)

The T1's TUNE line is asserted active-LOW. An NPN transistor in common-emitter configuration inverts the Arduino's HIGH output into an open-collector pull-to-ground on the T1 ring:

```
Arduino D4 ── 1.2kΩ ── Base
                        │
                    2N3904 NPN
                        │
T1 Ring (TUNE) ──── Collector   (T1 internally pulls ring HIGH)
                        │
                      Emitter
                        │
                       GND
```

---

## Software Configuration

### Arduino IDE Setup

- **Board**: Arduino Leonardo
- **Library**: [Adafruit SleepyDog](https://github.com/adafruit/Adafruit_SleepyDog) (install via Library Manager)

### Console Commands

When debug mode is enabled, the serial monitor accepts single-letter commands (case-insensitive):

| Command | Function |
|---------|----------|
| `h` | Display help message and list all commands |
| `p` | Print observation table headers (reorient data display) |
| `r` | Resend current T1 band data to tuner |
| `x` | Reset all T1 relays (sends code 0000) |

### Debug Mode

Uncomment the `#define debug` line near the top of the sketch to enable Serial Monitor output and disable watchdog sleep (sleep mode disconnects the USB serial interface):

```cpp
#define debug
```

Connect at **9600 baud** (115200 may also work). Startup output shows configuration:

```
FTX-1F to Elecraft T1 interface
Pin assignments:
  BAND_A -> A0
  BAND_B -> A1
  BAND_C -> A2
  BAND_D -> A3
  TUNE   -> 4
  DATA   -> 3

Band Table:
+-------+-----+-------+-------+
| abcd  | dec | Band  | T1 ID |
+-------+-----+-------+-------+
| 1000  |   8 | 160m  |    1  |
| 0100  |   4 | 80m   |    2  |
...
```

Continuous observation table (one row per second, shows detected band):

```
A B C D  abcd(bin)  abcd(dec)  A B C D BAND DATA   BAND
------------------------------------------------------------
0 0 0 1  1000      8          L L L H           160m
0 1 0 0  0100      4          L H L L           80m
```

On band change, T1 communication details are logged:

```
Band change detected -> sending to T1
  [T1] Asserting TUNE for 500ms...
  [T1] Waiting for DATA to go HIGH...
  [T1] Waiting for DATA to go LOW...
  [T1] Sending bits: 0 0 0 1
  [T1] Done
```

If the T1 does not respond:

```
  [T1] WARNING: timeout waiting for DATA HIGH - T1 may not be responding
```

---

## Testing & Verification

✅ **Protocol Verification**: T1 band code transmission verified against official Elecraft T1 ATU manual
  - Handshake protocol: TUNE assertion (500ms) → DATA pulse detection → 10ms wait → band code transmission
  - Bit timing: "1" = 4ms HIGH, "0" = 1.5ms HIGH, gap = 1.5ms LOW (all within ±15% tolerance per manual)
  - MSB-first bit order: bits sent in order [bit3, bit2, bit1, bit0]
  - Band codes 1-11: All verified with real Elecraft T1 hardware
  - Relay reset (code 0): Tested via 'x' console command

✅ **Hardware Validation**: System tested with production Elecraft T1 ATU
  - Band detection working correctly across all 11 HF bands
  - Relay engagement confirmed audibly and visually on each band change
  - No data transmission errors or T1 communication timeouts
  - Console commands fully functional (h, p, r, x)
  - Tested with debug mode enabled and disabled

---

## Power Consumption

In normal (non-debug) operation the sketch uses `Watchdog.sleep(2000)` to put the ATmega32U4 into low-power sleep between polls. Current draw will pulse briefly every 2 seconds while the band pins are sampled.

> Removing the onboard power LED from the Pro Micro will further reduce idle current.

---

## License

This code is released under the [Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)](http://creativecommons.org/licenses/by-nc-sa/3.0/) license.

Based on work by:
- Matthew Robinson — VK6MR (2011)
- Gareth — GI1MIC (2017)
- Andrew - KW9D (2026)
