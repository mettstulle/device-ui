#pragma once

/**
 * @file LGFX_MEIN_MUI_NODE.h
 * @brief ESP32-S3-N16R8 + LLCC68 (LoRa) + ILI9488 TFT + XPT2046 touch
 *
 * SPI layout (critical):
 *  - LoRa (RadioLib) uses the default Arduino SPI host = SPI2_HOST / FSPI
 *  - Display + touch MUST use SPI3_HOST on separate pins to avoid bus contention
 *  - Bus MISO must be the XPT2046 T_DO pin (not -1), even if the panel is write-only
 *
 * Build flags (firmware platformio.ini):
 *  -D MEIN_MUI_NODE
 *  -D LGFX_DRIVER=LGFX_MEIN_MUI_NODE
 *  Do NOT also enable LGFX_DRIVER_TEMPLATE / LGFX_GENERIC for this board.
 */

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>

#ifndef SPI_FREQUENCY
// ILI9488 is typically stable around 20 MHz; raise carefully if needed.
#define SPI_FREQUENCY 20000000
#endif
#ifndef LGFX_PIN_SCK
#define LGFX_PIN_SCK 39
#endif
#ifndef LGFX_PIN_MOSI
#define LGFX_PIN_MOSI 40
#endif
#ifndef LGFX_PIN_MISO
#define LGFX_PIN_MISO 41 // XPT2046 T_DO — required for shared-bus touch
#endif
#ifndef LGFX_PIN_DC
#define LGFX_PIN_DC 42
#endif
#ifndef LGFX_PIN_CS
#define LGFX_PIN_CS 2
#endif
#ifndef LGFX_PIN_RST
#define LGFX_PIN_RST 38
#endif
#ifndef LGFX_PIN_BL
#define LGFX_PIN_BL 1
#endif

// Prefer LGFX_TOUCH_* names when provided via build_flags; fall back to legacy names.
#if defined(LGFX_TOUCH_CS)
#define MEIN_TOUCH_CS LGFX_TOUCH_CS
#elif defined(TOUCH_CS_PIN)
#define MEIN_TOUCH_CS TOUCH_CS_PIN
#else
#define MEIN_TOUCH_CS 15
#endif

#if defined(LGFX_TOUCH_INT)
#define MEIN_TOUCH_INT LGFX_TOUCH_INT
#elif defined(TOUCH_INT_PIN)
#define MEIN_TOUCH_INT TOUCH_INT_PIN
#else
#define MEIN_TOUCH_INT 5
#endif

#ifndef LGFX_TOUCH_SPI_FREQ
#define LGFX_TOUCH_SPI_FREQ 2500000
#endif

#ifndef LGFX_OFFSET_ROTATION
#define LGFX_OFFSET_ROTATION 1 // landscape; try 0/2/3 if the image is rotated wrong
#endif

#ifndef LGFX_TOUCH_OFFSET_ROTATION
#define LGFX_TOUCH_OFFSET_ROTATION LGFX_OFFSET_ROTATION
#endif

class LGFX_MEIN_MUI_NODE : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;
    uint8_t brightness = 200;

  public:
    // Logical landscape size after offset_rotation
    const uint32_t screenWidth = 480;
    const uint32_t screenHeight = 320;

    bool hasButton(void) { return true; }

    void setBrightness(uint8_t brightness)
    {
        _light_instance.setBrightness(brightness);
        this->brightness = brightness;
    }

    uint8_t getBrightness(void) const { return brightness; }

    LGFX_MEIN_MUI_NODE(void)
    {
        { // Display SPI on SPI3 — keeps SPI2 free for LoRa (LLCC68)
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = SPI_FREQUENCY;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
#ifdef LGFX_CFG_DMA_CH
            cfg.dma_channel = LGFX_CFG_DMA_CH;
#else
            cfg.dma_channel = SPI_DMA_CH_AUTO;
#endif
            cfg.pin_sclk = LGFX_PIN_SCK;
            cfg.pin_mosi = LGFX_PIN_MOSI;
            cfg.pin_miso = LGFX_PIN_MISO; // must be T_DO for XPT2046 shared bus
            cfg.pin_dc = LGFX_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // ILI9488 panel (native portrait 320x480, rotated to landscape)
            auto cfg = _panel_instance.config();
            cfg.pin_cs = LGFX_PIN_CS;
            cfg.pin_rst = LGFX_PIN_RST;
            cfg.pin_busy = -1;

            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = LGFX_OFFSET_ROTATION;
            cfg.readable = (LGFX_PIN_MISO >= 0);
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }

        { // Backlight PWM
            auto cfg = _light_instance.config();
            cfg.pin_bl = LGFX_PIN_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        { // XPT2046 on the same SPI3 bus (different CS)
            auto cfg = _touch_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.freq = LGFX_TOUCH_SPI_FREQ;
            cfg.pin_cs = MEIN_TOUCH_CS;
            cfg.pin_int = MEIN_TOUCH_INT;
            cfg.pin_sclk = LGFX_PIN_SCK;
            cfg.pin_mosi = LGFX_PIN_MOSI;
            cfg.pin_miso = LGFX_PIN_MISO;
            cfg.bus_shared = true;
            cfg.x_min = 0;
            cfg.x_max = 4095;
            cfg.y_min = 0;
            cfg.y_max = 4095;
            cfg.offset_rotation = LGFX_TOUCH_OFFSET_ROTATION;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
