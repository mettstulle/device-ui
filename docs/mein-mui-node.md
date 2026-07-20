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

Use Meshtastic's DIY N16R8 board (`boards/my-esp32s3-diy-oled.json`), not stock
`esp32-s3-devkitc-1`. Ignore **both** Arduino BLE names, exclude `libpax`, and
pin NimBLE explicitly.

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

build_flags =
  ${esp32s3_base.build_flags}
  -I variants/esp32s3/mein-mui-node
  -DPRIVATE_HW=1
  -DMEIN_MUI_NODE=1
  -DLGFX_DRIVER=LGFX_MEIN_MUI_NODE
  -DBOARD_HAS_PSRAM=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DARDUINO_USB_MODE=0
  -DHAS_SCREEN=0
  -DHAS_TFT=1
  -DHAS_MUI=1
  -DUSE_PACKET_API=1
  -DUSE_LOG_DEBUG=1
  -DLOG_DEBUG_INC=\"DebugConfiguration.h\"
  -DVIEW_320x240
  -DDISPLAY_SET_RESOLUTION=1
  -DMESHTASTIC_EXCLUDE_PAXCOUNTER=1
  -DLGFX_PIN_SCK=21
  -DLGFX_PIN_MOSI=16
  -DLGFX_PIN_MISO=4
  -DLGFX_PIN_DC=5
  -DLGFX_PIN_CS=2
  -DLGFX_PIN_RST=38
  -DLGFX_PIN_BL=1
  -DLGFX_TOUCH_CS=15
  -DLGFX_SPI_FREQUENCY=10000000
  -DLGFX_OFFSET_ROTATION=1

lib_deps =
  ${esp32s3_base.lib_deps}
  ; DO NOT use ${device-ui_base.lib_deps} — official device-ui wins on name clash
  https://github.com/mettstulle/device-ui/archive/2444b5ba114e38ee9520870bcf057a46171fee2e.zip
  lovyan03/LovyanGFX@1.2.24
  h2zero/NimBLE-Arduino@1.4.3

; Framework dir is "BLE"; package name is "ESP32 BLE Arduino". Ignore BOTH.
lib_ignore =
  ${esp32s3_base.lib_ignore}
  ESP32 BLE Arduino
  BLE
  libpax
```

### Critical: Arduino BLE still compiling (`libraries/BLE/...`)

If the log still shows `framework-arduinoespressif32/libraries/BLE/BLEAddress.cpp`,
`lib_ignore` missed the framework folder name. You need **both** `ESP32 BLE Arduino`
and `BLE`, plus explicit `h2zero/NimBLE-Arduino@1.4.3` in `lib_deps`.

Also set `board = my-esp32s3-diy-oled` (N16R8 DevKit clone already in firmware).

```powershell
Remove-Item -Recurse -Force .pio\libdeps\mein-mui-node, .pio\build\mein-mui-node -ErrorAction SilentlyContinue
pio pkg install -e mein-mui-node
Get-ChildItem .pio\libdeps\mein-mui-node | Select-String -Pattern "NimBLE|libpax|device-ui"
pio run -e mein-mui-node
```

Expect `NimBLE-Arduino` present, no `libpax`, and **no** compile steps under `libraries/BLE/`.

### Critical: force PlatformIO to actually install the fork

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

### LoRa DX-PJ27 / DX-LR20

| Modul | ESP32 |
|-------|-------|
| VCC | 3V3 |
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
| VCC | 3V3 |
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

## Expected log markers (UART)

```
MEIN_MUI: LGFX begin SCK=21 MOSI=16 MISO=4 DC=5 CS=2 RST=38
MEIN_MUI: LGFX init done
MEIN_MUI: LGFX fillScreen done
```

If it still hangs after `begin` but before `init done`, re-check TFT wiring.
If `fillScreen done` appears then WDT fires, try `-DMEIN_MUI_NO_TOUCH=1` as a test.
