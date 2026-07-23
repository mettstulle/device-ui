# Mein MUI Node (ESP32-S3-N16R8 + SX1262 + ILI9488/HR2046)

Custom Meshtastic MUI board using:

- ESP32-S3 DevKitC-1 N16R8 (16 MB flash, 8 MB OPI PSRAM)
- DX-LR20 / E22-style SX1262 LoRa (`900M22S`) on SPI2
- KMRTM35018-SPI 3.5" ILI9488 TFT + HR2046 (XPT2046-compatible) on SPI3

## Critical: do not use GPIO 39-42 for the TFT

On ESP32-S3 those pins are USB-JTAG (`MTCK/MTDO/MTDI/MTMS`). With USB connected, SPI there often wedges the `tft` task (`task_wdt`, `CPU 0: tft`).

## Firmware `variants/esp32s3/mein-mui-node/variant.h`

```cpp
#define USE_SX1262

#define LORA_SCK 12
#define LORA_MISO 13
#define LORA_MOSI 11
#define LORA_CS 10

#define SX126X_CS LORA_CS
#define SX126X_RESET 9
#define SX126X_DIO1 8
#define SX126X_BUSY 7
#define SX126X_RXEN 6
#define SX126X_TXEN 14
#define SX126X_MAX_POWER 22
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif

#define BUTTON_PIN 0
#define I2C_SDA 17
#define I2C_SCL 18
```

## Firmware `platformio.ini`

Your firmware tree is **develop / pioarduino (Arduino 3.x)**: Bluetooth uses the
framework `BLE` library + IDF NimBLE (`BLE2904.h`), **not** `h2zero/NimBLE-Arduino`.

If `esp_bt.h` / `host/ble_uuid.h` are missing while compiling `libraries/BLE/...`,
the board build is not picking up Meshtastic's `custom_sdkconfig` BT bits. For the
first MUI bring-up, **exclude Bluetooth** so display/LoRa can be tested over USB
serial; re-enable BLE later after a working image.

```ini
[env:mein-mui-node]
extends = esp32s3_base
board = my-esp32s3-diy-oled
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.f_flash = 80000000L
upload_protocol = esptool
custom_meshtastic_has_mui = true

build_unflags =
  ${esp32s3_base.build_unflags}
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_MODE=0
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DARDUINO_USB_CDC_ON_BOOT

build_flags =
  ${esp32s3_base.build_flags}
  -I variants/esp32s3/mein-mui-node
  -DPRIVATE_HW=1
  -DMEIN_MUI_NODE=1
  -DLGFX_DRIVER=LGFX_MEIN_MUI_NODE
  -DBOARD_HAS_PSRAM=1
  -DARDUINO_USB_CDC_ON_BOOT=0
  -DHAS_SCREEN=0
  -DHAS_TFT=1
  -DHAS_MUI=1
  -DUSE_PACKET_API=1
  -DUSE_LOG_DEBUG=1
  -DLOG_DEBUG_INC=\"DebugConfiguration.h\"
  -DVIEW_320x240
  -DDISPLAY_SET_RESOLUTION=1
  -DRAM_SIZE=6144
  -DLV_CACHE_DEF_SIZE=2097152
  -DMESHTASTIC_EXCLUDE_WEBSERVER=1
  -DMESHTASTIC_EXCLUDE_CANNEDMESSAGES=1
  -DMESHTASTIC_EXCLUDE_PAXCOUNTER=1
  -DMESHTASTIC_EXCLUDE_BLUETOOTH=1
  -DRADIOLIB_SPI_PARANOID=0
  -DMEIN_MUI_NO_TOUCH=1
  -DLGFX_PIN_SCK=21
  -DLGFX_PIN_MOSI=16
  -DLGFX_PIN_MISO=4
  -DLGFX_PIN_DC=5
  -DLGFX_PIN_CS=2
  -DLGFX_PIN_RST=38
  -DLGFX_PIN_BL=1
  -DLGFX_TOUCH_CS=15
  -DLGFX_TOUCH_INT=-1
  -DLGFX_SPI_FREQUENCY=10000000
  -DLGFX_OFFSET_ROTATION=1

lib_deps =
  ${esp32s3_base.lib_deps}
  ; DO NOT use ${device-ui_base.lib_deps} — official device-ui wins on name clash
  https://github.com/mettstulle/device-ui/archive/2444b5ba114e38ee9520870bcf057a46171fee2e.zip
  lovyan03/LovyanGFX@1.2.24
  ; DO NOT add h2zero/NimBLE-Arduino on develop/pioarduino

lib_ignore =
  ${esp32s3_base.lib_ignore}
  libpax
  BLE
  ESP32 BLE Arduino
```

With `-DMESHTASTIC_EXCLUDE_BLUETOOTH=1`, also ignore `BLE` so the framework library
is not compiled (it needs `esp_bt.h`). Phone pairing will not work until BT is
re-enabled; use USB serial / CLI for now.

```powershell
Remove-Item -Recurse -Force .pio\build\mein-mui-node -ErrorAction SilentlyContinue
pio run -e mein-mui-node
```

### Later: re-enable Bluetooth

1. Remove `-DMESHTASTIC_EXCLUDE_BLUETOOTH=1`
2. Remove `BLE` / `ESP32 BLE Arduino` from `lib_ignore`
3. Full clean so `custom_sdkconfig` regenerates: `pio run -e mein-mui-node -t fullclean`
4. Rebuild. If `esp_bt.h` is still missing, BT include paths are still wrong for this
   board — compare with a known-good env like `heltec-v3` on the same firmware tree.

### Touch calibration (XPT2046 / HR2046)

Wake-on-touch can work while menu hits miss — that means detection works but
coordinates are wrong. Calibrate once:

```ini
  -DMEIN_MUI_ENABLE_TOUCH=1
  -DCALIBRATE_TOUCH=1
  -DLGFX_TOUCH_SPI_FREQ=1000000
```

Flash, then tap the arrow tips on screen. UART prints:

```
Touchscreen calibration parameters: {a, b, c, d, e, f, g, h}
```

Paste those eight values into `LGFXDriver.h` under `#elif defined(MEIN_MUI_NODE)`,
rebuild with `-DCALIBRATE_TOUCH=0` (keeps the `#ifdef CALIBRATE_TOUCH` path so
`setTouchCalibrate()` still runs), and menu hits should line up.

Current panel values (KMRTM35018 / HR2046, rotation offset 1):

```cpp
uint16_t parameters[8] = {242, 240, 3888, 231, 247, 3876, 3787, 3861};
```

If axes feel swapped/mirrored, try `-DLGFX_TOUCH_OFFSET_ROTATION=0` (or 2/3)
while keeping panel `-DLGFX_OFFSET_ROTATION=1`.

### Powersave + touch (XPT2046 shared SPI)

After display timeout, MEIN_MUI no longer calls `lgfx->sleep()` / disables the
LVGL touch indev. That path broke icon hits after wake on shared-SPI resistive
panels. Wake by tapping the blank lock overlay; backlight restores and
calibration is re-applied.

`custom_meshtastic_has_mui = true` only sets flasher metadata. It does **not** pull `device-ui`.

You must list the fork ZIP in `lib_deps` (instead of `${device-ui_base.lib_deps}`).

If PowerShell says `meshtastic-device-ui\...\LGFXDriver.h` is missing while PIO prints `Already up-to-date`, the env never got the library (or the libdeps folder is stale). Force a reinstall:

```powershell
# from C:\Users\Roy\firmware
Remove-Item -Recurse -Force .pio\libdeps\mein-mui-node -ErrorAction SilentlyContinue
pio pkg install -e mein-mui-node
Get-ChildItem .pio\libdeps\mein-mui-node | Select-Object Name
Select-String -Path ".pio\libdeps\mein-mui-node\meshtastic-device-ui\include\graphics\driver\LGFXDriver.h" -Pattern "MEIN_MUI_NO_TOUCH"
```

Expected: a folder named `meshtastic-device-ui` (from `library.json` `name`), and the Select-String hits.
If the folder is still missing, print resolved deps:

```powershell
pio pkg list -e mein-mui-node
```

and confirm `meshtastic-device-ui` appears with the `mettstulle` / `45cf1d3` source URL.

### If AES.h / Fsm.h / OLEDDisplay.h are missing

Those come from `${esp32s3_base.lib_deps}` (`Crypto`, `arduino-fsm`, `esp8266-oled-ssd1306`). OLED is still required even with `HAS_SCREEN=0`.

1. Confirm `lib_deps` still starts with `${esp32s3_base.lib_deps}` (do not replace the whole list with only LovyanGFX + device-ui).
2. Reinstall and verify:

```powershell
Remove-Item -Recurse -Force .pio\libdeps\mein-mui-node, .pio\build\mein-mui-node -ErrorAction SilentlyContinue
pio pkg install -e mein-mui-node
pio pkg list -e mein-mui-node
Get-ChildItem .pio\libdeps\mein-mui-node | Select-Object Name
```

You should see folders similar to `Crypto`, `meshtastic-arduino-fsm` (or `arduino-fsm`), `meshtastic-esp8266-oled-ssd1306` (or `esp8266-oled-ssd1306`), `NimBLE-Arduino`, and `meshtastic-device-ui`.

## Wiring

### Power (LiPo 1S + Pololu S7V8F3)

Akku (~100×60×11 mm) passt flächig zum Display (~98×58 mm). Der Pololu ist **Buck-Boost 3,3 V** — nicht den Roh-Akku an `3V3` legen.

Zusätzlich brauchst du noch ein **USB-Lademodul** (z. B. TP4056 mit Schutz / besser mit Power-Path). Der S7V8F3 lädt nicht.

```
USB ──► Lade-IC ──► LiPo(+) ──┬──► Pololu VIN
                              │
                              └──► MAX17048 CELL / VIN
LiPo(−) / GND ───────────────────── gemeinsames GND
Pololu VOUT (3.3V) ──► ESP32 3V3, LoRa VCC, Display VCC,
                       MAX17048 VDD, L76K VCC
Pololu GND ─────────── GND
```

| Pololu S7V8F3 | Anschluss |
|---------------|-----------|
| VIN | LiPo + (nach Schalter/PCM, parallel zur Lade-IC-BAT-Seite) |
| GND | LiPo − / System-GND |
| VOUT | System-**3V3** (ESP `3V3`-Pin) |
| EN | an VIN lassen (immer an) oder Schalter nach GND = aus |

Hinweise:

- Polarität des JST **vor dem Stecken** mit Multimeter prüfen.
- Beim Entwickeln mit USB-Kabel: Pololu-**VOUT** vom ESP `3V3` trennen (oder nur Akku-Betrieb), sonst Backfeed/Regler-Kampf.
- Optional Schalter zwischen LiPo + und Pololu VIN.

### LoRa DX-PJ27 / DX-LR20

| Modul | ESP32 |
|-------|-------|
| VCC | 3V3 (von Pololu) |
| GND | GND |
| NSS | 10 |
| NRST | 9 |
| MOSI | 11 |
| SCK | 12 |
| MISO | 13 |
| DIO1 | 8 |
| BUSY | 7 |
| RXEN | 6 |
| TXEN | 14 |
| DIO2 | nc |

### Display KMRTM35018-SPI

| Display | ESP32 |
|---------|-------|
| VCC | 3V3 (von Pololu) |
| GND | GND |
| CS | 2 |
| RESET | 38 |
| D/C | 5 |
| SDI | 16 |
| SCK | 21 |
| LED | 1 |
| SDO | nc |
| T_CLK | 21 |
| T_CS | 15 |
| T_DIN | 16 |
| T_OUT | 4 |
| T_IRQ | nc |

### MAX17048 (I2C Fuel Gauge)

Am bestehenden I2C-Bus (wie in `variant.h`: SDA 17 / SCL 18). Adresse typisch `0x36`.

| MAX17048 | Anschluss |
|----------|-----------|
| VDD | 3V3 (von Pololu) |
| GND | GND |
| SDA | GPIO **17** |
| SCL | GPIO **18** |
| CELL / VIN (Sense) | **LiPo +** (Zellenspannung, nicht die 3,3 V-Schiene) |
| ALRT | nc (optional später) |

Pull-ups: oft schon auf dem Breakout; sonst 4,7 kΩ SDA/SCL → 3V3.

### Waveshare L76K GPS (UART)

Waveshare-Demo nutzt oft GPIO 16/17 — die sind bei uns **Display/I2C**. Deshalb UART auf **47/48**.

| L76K | ESP32 / Power |
|------|----------------|
| VCC | 3V3 (Modul akzeptiert 2,7–5 V; 3V3 reicht) |
| GND | GND |
| TX | GPIO **47** (ESP RX ← GPS TX) |
| RX | GPIO **48** (ESP TX → GPS RX) |
| PPS | nc (optional Status-LED/GPIO) |

Antenne auf dem Modul aufschrauben; erster Fix braucht freie Sicht zum Himmel.

Firmware-Skizze (`variant.h` / `platformio.ini`):

```cpp
#define HAS_GPS 1
#define GPS_RX_PIN 47
#define GPS_TX_PIN 48
#define GPS_BAUDRATE 9600
```

(Pin-Namen in Meshtastic: `GPS_RX_PIN` = MCU empfängt von GPS-TX.)

### Passiver Piezo-Buzzer

| Buzzer | Anschluss |
|--------|-----------|
| + / Signal | GPIO **3** (PWM / `PIN_BUZZER`) |
| − / GND | GND |

Optional ~100–220 Ω in Reihe. Lautstärke zu gering → NPN/MOSFET + 5 V/3V3-Versorgung (Piezo dann nicht direkt am GPIO).

```cpp
#define PIN_BUZZER 3
```

### Belegte vs. freie GPIOs (Kurz)

| GPIO | Funktion |
|------|----------|
| 0 | Boot-Taste |
| 1 | TFT BL |
| 2 | TFT CS |
| 3 | **Buzzer** |
| 4 | Touch MISO |
| 5 | TFT DC |
| 6–14 | LoRa |
| 15 | Touch CS |
| 16 | TFT MOSI |
| 17 / 18 | I2C (MAX17048) |
| 21 | TFT SCK |
| 38 | TFT RST |
| 47 / 48 | **GPS UART** |
| 19 / 20 | USB — nicht nutzen |
| 35–37 | OPI-PSRAM — nicht nutzen |
| 39–42 | USB-JTAG — nicht für SPI |
| 43 / 44 | UART0 Konsole — frei lassen |

## Expected log markers (UART)

```
MEIN_MUI: LGFX begin SCK=21 MOSI=16 MISO=4 DC=5 CS=2 RST=38
MEIN_MUI: LGFX init done
MEIN_MUI: LGFX fillScreen done
```

If it still hangs after `begin` but before `init done`, re-check TFT wiring.
If `fillScreen done` appears then WDT fires, try `-DMEIN_MUI_NO_TOUCH=1` as a test.
