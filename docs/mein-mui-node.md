# Mein MUI Node (ESP32-S3-N16R8 + SX1262 + ILI9488/HR2046)

Custom Meshtastic MUI board using:

- ESP32-S3 DevKitC-1 N16R8 (16 MB flash, 8 MB OPI PSRAM)
- Waveshare **Core1262-868M** (SX1262 + TCXO + RF-Switch) on SPI2
  - (older bring-up used DX-LR20 / E22-style `900M22S` — pin GPIOs stay the same)
- KMRTM35018-SPI 3.5" ILI9488 TFT + HR2046 (XPT2046-compatible) on SPI3

## Critical: do not use GPIO 39-42 for the TFT

On ESP32-S3 those pins are USB-JTAG (`MTCK/MTDO/MTDI/MTMS`). With USB connected, SPI there often wedges the `tft` task (`task_wdt`, `CPU 0: tft`).

### WiFi vs. CLI (MUI / COLOR-Display)

Mit MUI setzt die Firmware `display.displaymode = COLOR`. Dann startet **kein** TCP-API-Server auf Port **4403** (und oft auch kein Webserver) — absichtlich, siehe `WiFiAPClient.cpp` / T-Deck-Verhalten.

Folge: Node hat eine **IP** (Karten/NTP über WLAN funktionieren), aber:

```text
meshtastic --host 192.168.x.x --info
→ WinError 10061 Verbindung verweigert
```

**CLI/Config** für MUI-Nodes über **USB-Serial** (oder später BLE, wenn im Build aktiv):

```powershell
meshtastic --port COMx --info
meshtastic --port COMx --set device.tzdef "CET-1CEST,M3.5.0,M10.5.0/3"
meshtastic --port COMx --ch-set module_settings.position_precision 32 --ch-index 0
```

`MESHTASTIC_EXCLUDE_WEBSERVER=1` im Env entfernt zusätzlich die HTTP-UI — unabhängig davon fehlt 4403 bei COLOR ohnehin.

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
#define SX126X_RXEN 6   // FW-RXEN → Modul-TXEN (Kreuz); optional → 39 siehe Pin-Hinweis unten
#define SX126X_TXEN 14  // FW-TXEN → Modul-RXEN (Kreuz)
#define SX126X_MAX_POWER 22
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
// Core1262 has on-board TCXO (DIO3). Do NOT keep TCXO_OPTIONAL.

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif

#define BUTTON_PIN 0
#define I2C_SDA 17
#define I2C_SCL 18
```

Region in Meshtastic: **EU_868** (Core1262-868M).

### GPIO 1 / 6 und Flash — Recherche-Hinweis

Auf dem **klassischen ESP32** sind GPIO 6–11 die externe Flash-SPI — die darf man nicht nutzen. Beim **ESP32-S3** (dein N16R8) liegt Flash/PSRAM intern auf **GPIO 26–32** (bzw. 33–37 bei Octal); die sind am WROOM-Modul **nicht herausgeführt**. GPIO 1 und 6 sind laut Espressif normale, nutzbare GPIOs.

Trotzdem kann dein Befund praktisch relevant sein:

| Pin heute | Funktion | Risiko |
|-----------|----------|--------|
| **GPIO 6** | FW `SX126X_RXEN` → **Modul-TXEN** | Schaltet bei **jedem TX** um — genau wenn das Modul mit voller Leistung (~22 dBm) sendet |
| **GPIO 1** | TFT Backlight | Weniger kritisch (statisch), liegt aber oft nah an anderen Leitungen auf dem DevKit |

Starke LoRa-HF in Antennen-/Modulnähe zum ESP kann Flash-/LittleFS-Zugriffe stören (Brownout, Bitfehler) — das passt zur fehlenden `/prefs/uiconfig.proto`, auch wenn 1/6 keine Flash-Pins des S3 sind.

**Maßnahmen (Priorität):**

1. Antenne fest, gut getrennt vom ESP/DevKit; kurze IPEX–SMA; solides gemeinsames GND (kein loser GND wie früher beim busyTx).
2. Testweise `SX126X_MAX_POWER` senken (z. B. 14–16) und prüfen, ob Prefs-Schreiben stabiler wird.
3. Optional umverdrahten / in `variant.h` + `platformio.ini` anpassen — RF-Schalter und BL **aus dem LoRa-Cluster 6–14** heraus:

| Signal | Alt | Neu (Vorschlag) | Hinweis |
|--------|-----|-----------------|--------|
| Modul-**TXEN** (FW `SX126X_RXEN`) | GPIO **6** | GPIO **39** | Nur digitales Schalten, kein SPI — USB-JTAG dann nicht parallel nutzen |
| TFT **BL** | GPIO **1** | GPIO **41** | `-DLGFX_PIN_BL=41` |
| Modul-RXEN (FW `SX126X_TXEN`) | GPIO **14** | kann bleiben | weniger kritisch als TXEN |

```cpp
#define SX126X_RXEN 39   // statt 6 — Modul-TXEN-Pad
#define SX126X_TXEN 14   // unverändert, Modul-RXEN-Pad
```

```ini
  -DLGFX_PIN_BL=41
```

Nach dem Umbau: LoRa-Init (`SX126x init result 0`), TX-Test, dann UI-Einstellungen + UART auf `Save /prefs/uiconfig.proto` / Dateiliste prüfen.

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
  https://github.com/mettstulle/device-ui/archive/0ca2499bfbc53e80352f1557c5eedc301afd8093.zip
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

Mein MUI applies baked-in cal from `MEIN_MUI_TOUCH_CAL.h` automatically (no
`-DCALIBRATE_TOUCH=…` needed). **Do not** use `-DCALIBRATE_TOUCH=0` / `=1` —
evaluating that define in `#if` can fail the build with
`user-defined literal in preprocessor expression`.

Normal build flags:

```ini
  -DMEIN_MUI_ENABLE_TOUCH=1
  -DLGFX_TOUCH_SPI_FREQ=1000000
  -DLGFX_OFFSET_ROTATION=3
  -DLGFX_TOUCH_OFFSET_ROTATION=3
```

Remove `-DMEIN_MUI_NO_TOUCH=1` and any `-DCALIBRATE_TOUCH=…` if present.

To force interactive recalibration once:

```ini
  -DFORCE_CALIBRATE_TOUCH
```

Flash, tap the arrow tips, UART prints `Touchscreen calibration parameters: {…}`.
Paste the eight values into `include/graphics/LGFX/MEIN_MUI_TOUCH_CAL.h`, remove
`-DFORCE_CALIBRATE_TOUCH`, rebuild.

Powersave re-applies the baked-in cal; `IGNORE_CALIBRATION_DATA=1` remains optional.

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
USB ──► Lade-IC ──► LiPo(+) ──► MAX17048 JST1
                                    │
                              MAX17048 JST2 ──► Pololu VIN
LiPo(−) / GND ───────────────────── gemeinsames GND
Pololu VOUT (3.3V) ──► ESP32 3V3, LoRa/Display VCC,
                       MAX17048 VIN (Logik), L76K VCC
Pololu GND ─────────── GND
```
(JST1/JST2 sind auf Dual-JST-Modulen parallel — tauschbar.)

| Pololu S7V8F3 | Anschluss |
|---------------|-----------|
| VIN | LiPo + (nach Schalter/PCM, parallel zur Lade-IC-BAT-Seite) |
| GND | LiPo − / System-GND |
| VOUT | System-**3V3** (ESP `3V3`-Pin) |
| SHDN | offen lassen oder an VIN (an). Auf GND = aus |

Hinweise:

- Polarität des JST **vor dem Stecken** mit Multimeter prüfen.
- Beim Entwickeln mit USB-Kabel: Pololu-**VOUT** vom ESP `3V3` trennen (oder nur Akku-Betrieb), sonst Backfeed/Regler-Kampf.
- Optional Schalter zwischen LiPo + und Pololu VIN.

### LoRa Waveshare Core1262-868M

ESP-GPIOs bleiben wie beim alten DX-LR20. **Wichtig:** Waveshare beschriftet RXEN/TXEN anders als RadioLib/Meshtastic — die beiden Leitungen sind **über Kreuz** verdrahtet.

| Core1262 Pad | ESP32 / Power | Hinweis |
|--------------|---------------|---------|
| 3V3 | 3V3 (Pololu) | nur 3,3 V |
| GND | GND | mind. einen GND, besser mehrere |
| CS | **10** | NSS |
| CLK | **12** | SCK |
| MOSI | **11** | |
| MISO | **13** | |
| RESET | **9** | |
| BUSY | **7** | |
| DIO1 | **8** | IRQ |
| **TXEN** | GPIO **6** (`SX126X_RXEN`) | Kreuz: FW-RXEN → Modul-TXEN |
| **RXEN** | GPIO **14** (`SX126X_TXEN`) | Kreuz: FW-TXEN → Modul-RXEN |
| DIO2 | nc | unbenutzt bei MCU-RF-Switch |
| ANT | Antenne 868 MHz | |

Firmware: `TCXO_OPTIONAL` entfernen (Modul hat TCXO). `SX126X_DIO3_TCXO_VOLTAGE 1.8` behalten. Max 22 dBm.

Wenn TX/RX vertauscht wirken (kein Empfang / schwacher TX): die beiden EN-Leitungen nochmal prüfen (Kreuzung).

#### `busyTx` / Critical Error 8 / Reboot nach ~60 s

```
WARN  Can not send yet, busyTx
ERROR Hardware Failure! busyTx for more than 60s
ERROR Record critical error 8 … RadioLibInterface.cpp
INFO  Rebooting
```

Bedeutung: Firmware hat TX gestartet, aber **kein TX-done-IRQ** vom Radio (typisch **DIO1**). Das ist Hardware/Verdrahtung, kein UI-Problem — oft nach Neuverdrahten/Zusammenbau.

Prüfen (Durchgang / Wackelkontakt):

| Signal | ESP | Core1262 |
|--------|-----|----------|
| **DIO1** | **8** | DIO1 — zuerst prüfen |
| BUSY | 7 | BUSY |
| CS / CLK / MOSI / MISO | 10 / 12 / 11 / 13 | CS / CLK / MOSI / MISO |
| RESET | 9 | RESET |
| TXEN-Pad | **6** (`SX126X_RXEN`) | Kreuzung |
| RXEN-Pad | **14** (`SX126X_TXEN`) | Kreuzung |

Bootlog: `SX126x init result 0`, `Use MCU pin 6 as RXEN and pin 14 as TXEN`. Antenne aufgeschraubt. 3V3/GND zum Modul fest.

`rxGood≥1` bei gleichzeitigem `busyTx` → Empfang geht oft noch, TX-IRQ (DIO1) fehlt — DIO1-Leitung priorisieren.

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

Typische Dual-JST-Module (z. B. Adafruit): die **beiden JST-Buchsen sind parallel**. LiPo an die eine, Last/Ladepfad (→ Pololu VIN) an die andere — Reihenfolge egal. Polarität `+`/`−` auf dem PCB beachten.

| MAX17048 | Anschluss |
|----------|-----------|
| **VIN** (Logik / Breakout) | **3V3** vom Pololu (gleiche Logik wie ESP) |
| GND | System-GND |
| SDA | GPIO **17** |
| SCL | GPIO **18** |
| JST 1 | **LiPo + / −** (Zelle) |
| JST 2 | weiter zu **Pololu VIN** (und ggf. SM5308-BAT-Seite) |
| ALRT | nc (optional später) |

Hinweise:

- Zellenspannung läuft **nur** über die JSTs (bzw. `Bat`-Pad), **nicht** über den VIN-Pin. VIN ≠ Zell-Sense.
- Auf Adafruit-Boards speist der Chip **standardmäßig aus der Zelle** (`Bat→VDD`); ohne (oder mit zu flacher) Zelle antwortet er oft **nicht** auf I2C. Optionaler Jumper `Vin/VDD/Bat` auf der Unterseite: nur ändern, wenn du den Chip bewusst aus VIN speisen willst.
- Pull-ups: oft schon auf dem Breakout; sonst 4,7 kΩ SDA/SCL → 3V3.

#### Steckersymbol / Log: `max17048Init … not ready yet` + `USB power=1`

Deine Zeilen:

```
DEBUG | Power::max17048Init lipo sensor is not ready yet
INFO  | PowerFSM init, USB power=1
```

bedeuten:

1. **`not ready yet` (t≈0)** — `Power::setup()` läuft **vor** dem I2C-Scan. Die Sensor-Map ist noch leer → Init schlägt immer zuerst fehl. Das allein sagt noch nichts über die Verdrahtung.
2. **`USB power=1`** — ohne `BATTERY_PIN` / AXP nimmt PowerFSM **immer** „extern versorgt“ an. Ohne funktionierenden Fuel-Gauge als `batteryLevel` bleibt `hasBattery=false` → Telemetrie `battery_level=101`, `voltage=0` → **Steckersymbol**.

Danach im **vollen** Bootlog prüfen:

- `Starting Bus with (SDA) 17 and (SCL) 18`
- `Scan for i2c devices` → `N I2C devices found` (nicht `No I2C devices found`)
- `MAX17048` / Adresse **`0x36`**
- später idealerweise `Battery: … batMv=…` mit **batMv > 0**

Fehlt `0x36` komplett: Hardware/I2C (Pull-ups, SDA/SCL, Zelle am JST, Chip antwortet nur mit Zelle).

Wird `0x36` gefunden, aber weiterhin Stecker: bekannte Firmware-Reihenfolge — Power übernimmt den MAX17048 **nicht nachträglich**. Fix: Chip **vor** `power->setup()` eintragen (in `src/main.cpp`, direkt nach dem **ersten** `Wire.begin(...)`, vor `power = new Power()`):

```cpp
// Mein-MUI: Power::setup() runs before the full I2C scan; seed MAX17048 early.
// KEIN zweites Wire.begin() — pioarduino/Arduino-3 I2C-NG verträgt das nicht
// (Log: "Bus already started" → i2cWrite ESP_ERR_INVALID_STATE).
Wire.beginTransmission(0x36);
if (Wire.endTransmission() == 0) {
    nodeTelemetrySensorsMap[meshtastic_TelemetrySensorType_MAX17048] = {0x36, &Wire};
    LOG_INFO("Early probe: MAX17048 at 0x36 for Power");
} else {
    LOG_WARN("Early probe: no MAX17048 at 0x36");
}
```

Erwarteter UART-Ausschnitt:

```text
INFO  | Starting Bus with (SDA) 17 and (SCL) 18
INFO  | Early probe: MAX17048 at 0x36 for Power
```

Wenn danach `Wire.cpp begin(): Bus already started in Master Mode` und
`i2cWrite(): … ESP_ERR_INVALID_STATE` kommt: irgendwo wird **`Wire.begin()` ein zweites Mal** aufgerufen (Probe selbst darf das nicht; oft zusätzliches `Wire.begin(SDA,SCL)` in der eigenen Patch-Zeile oder in Power/Sensor-Init). Zweites `begin` entfernen — Probe nur mit `beginTransmission`/`endTransmission`.

Erwartung nach sauberem Rebuild: `Power::max17048Init lipo sensor is ready`, dann echte `%` / Volt statt Stecker. `USB power=1` kann beim Boot noch kurz erscheinen; mit erkanntem Akku sollte die UI auf Batterie wechseln (bei starkem Laden ggf. Blitz).

`variant.h` weiter: `I2C_SDA 17`, `I2C_SCL 18`; kein `MESHTASTIC_EXCLUDE_I2C`. `Adafruit_MAX1704X` steckt bereits in der zentralen `platformio.ini` der Firmware.

### UI-Einstellungen verschwinden nach Reboot (Sprache, Timeout, Banner)

UART-Befund auf Mein MUI (auch **ohne** Pin-Umbau / ohne reduzierte Sendestärke — Stand weiterhin):

```text
ERROR | Could not open / read /prefs/uiconfig.proto
WARN  | [DeviceUI] invalid uiconfig version 0, reset UI settings to default
INFO  | [Router] Storing device-ui config
```

Im Dateilisting fehlt `/prefs/uiconfig.proto` (andere `/prefs/*.proto` sind da). Die MUI schickt `store_ui_config` (Admin payload 46), aber die Datei überlebt den Neustart nicht → Sprache wieder Englisch, Timeout/Banner zurück auf Defaults.

**Das ist unabhängig von GPIO 1/6 / TX-Power**, solange der Firmware-Store kaputt/unvollständig ist. Pin-/Leistungs-Experimente sind optional (HF-Störung); zuerst den Store-Patch flashen.

**Root cause auf Mein MUI:** In älteren Firmware-Ständen steckt der Save hinter `#if HAS_SCREEN`. Das Env setzt `-DHAS_SCREEN=0` und `-DHAS_TFT=1` → der Save wird **wegkompiliert**. UART zeigt trotzdem `Storing device-ui config` (Log liegt im `switch` davor), aber nie `Save /prefs/uiconfig.proto`.

**device-ui (dieses Repo):** erzwingt `version=1` bei jedem Store, schreibt Touch-Cal zusammen mit Defaults (kein zweiter Halb-Store), und wartet **10 s** statt 3 s vor Sprach-Reboot.

**Firmware (Pflicht auf dem PC unter `C:\Users\Roy\firmware`):** `handleStoreDeviceUIConfig` **ohne** `#if HAS_SCREEN`, RAM aktualisieren, stale `.tmp` löschen:

```cpp
// src/modules/AdminModule.cpp — komplette Funktion ersetzen, keinen alten Rest stehen lassen
void AdminModule::handleStoreDeviceUIConfig(const meshtastic_DeviceUIConfig &uicfg)
{
    uiconfig = uicfg;
#ifdef FSCom
    spiLock->lock();
    FSCom.mkdir("/prefs");
    if (FSCom.exists("/prefs/uiconfig.proto.tmp")) {
        LOG_DEBUG("Remove stale /prefs/uiconfig.proto.tmp");
        FSCom.remove("/prefs/uiconfig.proto.tmp");
    }
    spiLock->unlock();
#endif
    if (!nodeDB->saveProto("/prefs/uiconfig.proto", meshtastic_DeviceUIConfig_size, &meshtastic_DeviceUIConfig_msg,
                           &uiconfig)) {
        LOG_ERROR("Failed to persist /prefs/uiconfig.proto");
    }
}
```

Direkt danach muss `handleSetHamMode` (oder die nächste Funktion) kommen — **kein** zweites `{ #if HAS_SCREEN … }`-Fragment.
Optional upstream nachziehen: SafeFile stale-`.tmp`-Fix ([firmware#11428](https://github.com/meshtastic/firmware/pull/11428)) und `saveProto`-Fehlerpropagation ([firmware#11429](https://github.com/meshtastic/firmware/pull/11429)).

Nach Firmware-Rebuild im UART prüfen:

1. Nach Einstellungsänderung: `Save /prefs/uiconfig.proto` (ohne `Can't write prefs` / `Failed to persist`)
2. Nach Reboot in der Dateiliste: `/prefs/uiconfig.proto (… Bytes)`
3. `setupUIConfig version 1` — nicht `invalid uiconfig version 0`

Wenn `uiconfig.proto.tmp` existiert und Saves scheitern: Node einmal mit dem Patch flashen (löscht `.tmp`) oder LittleFS neu formatieren / Factory-Reset.

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

### SD-Karte für Offline-Karten (SPI-Adapter)

MUI lädt Kacheln von der SD unter `/maps/<style>/<z>/<x>/<y>.png` (256×256, ideal 8-bit). Ohne SD nur WLAN-Download (bei COLOR oft eingeschränkt). Mit SD: Karten ad-hoc offline.

**Amazon SPI-MicroSD-Reader** (z. B. [B0F2DPSQ5R](https://www.amazon.de/dp/B0F2DPSQ5R) — „SPI Reader Micro SD TF“): **ja, prinzipiell geeignet** (CS/SCK/MOSI/MISO/VCC/GND). Achtung:

- ESP32-S3-Logik nur **3,3 V**. Viele billige Module wollen **5 V** an VCC (AMS1117) — dann VCC=5 V ok, aber SPI-Pegel prüfen; ohne Pegelwandler **keine 5 V an GPIO**. Sicherer: Modul ausdrücklich mit 3,3 V betreiben oder eines mit Level-Shifter.
- Karte: MicroSDHC, **FAT32 oder exFAT**, Partitionstabelle **MBR** (sdcard.org-Formatter).
- **Nicht** am LoRa-SPI (11–13) anschließen.

**Empfohlene Verdrahtung Mein MUI** — **eigene Soft-SPI-Pins** (nicht Display teilen):

Auf dem ESP32-S3 ist SPI2 = LoRa und SPI3 = Display/Touch. `SDCARD_USE_SPI1` (HSPI=SPI3) **kollidiert** mit dem TFT → `GO_IDLE_STATE failed`, `duplicate addApbChangeCallback`, oft Reboot. Deshalb SD per **Bitbang (Soft-SPI)** auf freien GPIOs.

| SD-Modul | ESP32-S3 |
|----------|----------|
| GND | GND |
| VCC | 3V3 |
| CS | GPIO **41** |
| CLK (SCK) | GPIO **39** |
| MOSI | GPIO **40** |
| MISO | GPIO **42** |

`variant.h` — Pins + Soft-SPI (**kein** `#define HAS_SDCARD` hier, sonst Warnung „redefined“ mit `platformio.ini`):

```cpp
#define SDCARD_CS 41
#define SPI_SCK 39
#define SPI_MOSI 40
#define SPI_MISO 42
#define SDCARD_USE_SOFT_SPI
#define SD_SPI_FREQUENCY 1000000U
```

`platformio.ini` `build_flags` (wie Elecrow Crowpanel Soft-SPI):

```ini
  -DHAS_SDCARD
  -DSDCARD_USE_SOFT_SPI
  -DSPI_DRIVER_SELECT=2
```

`SPI_DRIVER_SELECT=2` ist **pflicht** für SdFat `SoftSpiDriver` — ohne das scheitert der Build an `SdCard.cpp` (`Error 1`). Nicht zusätzlich `#define HAS_SDCARD` in `variant.h` setzen (nur `-DHAS_SDCARD` in den Flags).

Firmware-`setupSDCard()` wird mit Soft-SPI übersprungen (kein frühes `No SD_MMC…`); die Erkennung läuft über DeviceUI/SdFat. Im Home-Screen SD-Icon prüfen / UART `SdCard init successful`.

**Diese UART-Fehler = noch falsche SD-Config (Display-SPI / `SDCARD_USE_SPI1`):**

```
sdCommand(): token error … 0xa0
ff_sd_initialize(): GO_IDLE_STATE failed
sdcard_mount(): f_mount failed: (3)
No SD_MMC card detected
addApbChangeCallback(): duplicate …
```

danach oft automatischer Neustart. Ursache: Firmware-Arduino-SD auf SPI3 (Display). Fix: Soft-SPI wie oben (Pins umverdrahten + `SDCARD_USE_SOFT_SPI` + `SPI_DRIVER_SELECT=2`, **kein** `SDCARD_USE_SPI1`). Buzzer bleibt GPIO **3** (nicht 45).

**Build `SoftSpiDriver does not name a type` / `HAS_SDCARD` redefined:**

1. In `variant.h` Zeile mit `#define HAS_SDCARD` **löschen** (nur Pins + `SDCARD_USE_SOFT_SPI` behalten).
2. In `platformio.ini` exakt:
   ```ini
     -DHAS_SDCARD
     -DSDCARD_USE_SOFT_SPI
     -DSPI_DRIVER_SELECT=2
   ```
3. Clean-Rebuild (SdFat-Cache sonst noch mit SELECT=0):
   ```
   platformio run -t clean -e mein-mui-node
   platformio run --target upload --environment mein-mui-node
   ```

**Kacheln aufspielen:** Starter-Zips aus `device-ui/maps/` oder [Oxed Map Tile Downloader](https://download.tiles.coalition.space/) → `/maps/<style>/…`. Bei leeren Tiles Zoom ≤ 6.

Optional WLAN-Cache via `.url` im Style-Ordner.

### Belegte vs. freie GPIOs (Kurz)

| GPIO | Funktion |
|------|----------|
| 0 | Boot-Taste |
| 1 | TFT BL *(optional → 41, siehe GPIO-1/6-Hinweis)* |
| 2 | TFT CS |
| 3 | **Buzzer** |
| 4 | Touch MISO |
| 5 | TFT DC |
| 6 | LoRa FW-RXEN→Modul-TXEN *(optional → 39)* |
| 7–13 | LoRa BUSY/DIO1/RST/SPI |
| 14 | LoRa FW-TXEN→Modul-RXEN |
| 15 | Touch CS |
| 16 | TFT MOSI |
| 17 / 18 | I2C (MAX17048) |
| 21 | TFT SCK |
| 38 | TFT RST |
| 39 | **SD Soft-SPI SCK** |
| 40 | **SD Soft-SPI MOSI** |
| 41 | **SD CS** |
| 42 | **SD Soft-SPI MISO** |
| 47 / 48 | **GPS UART** |
| 19 / 20 | USB — nicht nutzen |
| 26–32 | intern Flash (S3) — nicht herausgeführt |
| 35–37 | OPI-PSRAM — nicht nutzen |
| 40 / 42 | USB-JTAG — kein SPI; OK für langsame GPIO falls nötig |
| 43 / 44 | UART0 Konsole — frei lassen |
| 45 / 46 | Strapping — meiden |

## Expected log markers (UART)

```
MEIN_MUI: LGFX begin SCK=21 MOSI=16 MISO=4 DC=5 CS=2 RST=38
MEIN_MUI: LGFX init done
MEIN_MUI: LGFX fillScreen done
```

If it still hangs after `begin` but before `init done`, re-check TFT wiring.
If `fillScreen done` appears then WDT fires, try `-DMEIN_MUI_NO_TOUCH=1` as a test.
