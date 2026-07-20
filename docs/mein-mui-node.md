# Mein MUI Node (ESP32-S3-N16R8 + LLCC68 + ILI9488/XPT2046)

Custom Meshtastic MUI board using:

- ESP32-S3 DevKit N16R8 (16 MB flash, 8 MB OPI PSRAM)
- LLCC68 LoRa (RadioLib SX126x-compatible) on SPI2
- ILI9488 TFT + XPT2046 resistive touch on SPI3

## Why the UI stuck on the boot logo

The Meshtastic logo + version string **is already MUI**. Leaving that screen means the full UI never becomes visible, usually because:

1. **SPI host conflict** — LoRa uses SPI2 (`FSPI`). If the display is also configured on `SPI2_HOST` with different pins, LoRa init rebinds the peripheral and the TFT freezes on the last frame (logo), while the radio keeps working in the log.
2. **Touch MISO = -1** — For shared-bus XPT2046, the display bus `pin_miso` must be the touch `T_DO` pin (GPIO 41). `-1` breaks touch and can destabilize the shared bus.
3. **I2C on GPIO 8/9** — ESP32-S3 Arduino defaults Wire to SDA=8 / SCL=9, but those pins are LLCC68 `DIO1` / `RESET`. Remap I2C away from them.
4. **Wrong LGFX driver selection** — Prefer only `-DMEIN_MUI_NODE=1`. That forces `LGFX_MEIN_MUI_NODE` on SPI3. Leftover `LGFX_DRIVER_TEMPLATE` / `LGFX_GENERIC` flags are ignored when `MEIN_MUI_NODE` is set, but should still be removed from `platformio.ini`.

## Firmware `variants/esp32s3/mein-mui-node/variant.h`

```cpp
// ===== LoRa (LLCC68 via RadioLib SX1262 class) =====
#define LORA_SCK 12
#define LORA_MISO 13
#define LORA_MOSI 11
#define SX126X_CS 10
#define SX126X_RESET 9
#define SX126X_DIO1 8
#define SX126X_BUSY 7
#define SX126X_RXEN 6
#define SX126X_TXEN 14
#define SX126X_MAX_POWER 22

// Keep LoRa SPI frequency independent from the TFT bus.
// Do NOT set a global SPI_FREQUENCY that the TFT also overrides.

#define BUTTON_PIN 0

// Move I2C off LoRa IRQ/RESET (ESP32-S3 default Wire is 8/9)
#define I2C_SDA 17
#define I2C_SCL 18
#define HAS_AXP192 0
#define HAS_AXP2101 0
```

## Firmware `platformio.ini` environment

```ini
[env:mein-mui-node]
extends = esp32s3_base
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.f_flash = 80000000L
custom_meshtastic_has_mui = true

; Note: every continuation line under build_flags must stay indented.
; Avoid bare "-D FOO" and ";" comments inside the multiline value (breaks PlatformIO on Windows).
build_flags =
  ${esp32s3_base.build_flags}
  -I variants/esp32s3/mein-mui-node
  -DMEIN_MUI_NODE=1
  -DBOARD_HAS_PSRAM=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DHAS_SCREEN=0
  -DHAS_TFT=1
  -DHAS_MUI=1
  -DUSE_PACKET_API=1
  -DUSE_LOG_DEBUG=1
  -DLOG_DEBUG_INC=\"DebugConfiguration.h\"
  -DVIEW_320x240=1
  -DDISPLAY_SET_RESOLUTION=1
  -DLGFX_DRIVER=LGFX_MEIN_MUI_NODE
  -DLGFX_CFG_DMA_CH=1
  -DLGFX_PIN_SCK=39
  -DLGFX_PIN_MOSI=40
  -DLGFX_PIN_MISO=41
  -DLGFX_PIN_DC=42
  -DLGFX_PIN_CS=2
  -DLGFX_PIN_RST=38
  -DLGFX_PIN_BL=1
  -DLGFX_TOUCH_CS=15
  -DLGFX_TOUCH_INT=5
  -DSPI_FREQUENCY=20000000
  -DLGFX_OFFSET_ROTATION=1

lib_deps =
  ${esp32s3_base.lib_deps}
  lovyan03/LovyanGFX@1.2.24
```

Point `lib_deps` device-ui at a revision that contains `LGFX_MEIN_MUI_NODE.h`.

## Pin map (from the DevKit pinout)

| Function | GPIO | Notes |
|----------|------|-------|
| LoRa SCK/MISO/MOSI/CS | 12/13/11/10 | SPI2 |
| LoRa RESET/DIO1/BUSY | 9/8/7 | keep free from I2C/TFT |
| LoRa RXEN/TXEN | 6/14 | external RF switch |
| TFT SCK/MOSI/MISO | 39/40/41 | SPI3, MISO = touch DO |
| TFT DC/CS/RST/BL | 42/2/38/1 | |
| Touch CS/INT | 15/5 | shared SPI3 |
| User button | 0 | BOOT |
| I2C SDA/SCL | 17/18 | bypass away from 8/9 |

## Checklist after flashing

1. Log shows LoRa init **and** lines like `TFTView_320x240 init` / `setupUIConfig`.
2. Boot logo disappears within a few seconds and the home screen appears.
3. Touch works after on-screen calibration (raw 0..4095 until calibrated).
4. If the image is rotated wrong, try `-D LGFX_OFFSET_ROTATION=0` (or 2/3).
