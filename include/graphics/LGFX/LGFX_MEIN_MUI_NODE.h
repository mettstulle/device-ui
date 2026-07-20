#pragma once

/**
 * @file LGFX_MEIN_MUI_NODE.h
 * @brief ESP32-S3-N16R8 + SX1262/LLCC68 (LoRa) + ILI9488 TFT + XPT2046/HR2046 touch
 *
 * SPI layout (critical):
 *  - LoRa (RadioLib) uses the default Arduino SPI host = SPI2_HOST / FSPI
 *  - Display + touch MUST use SPI3_HOST on separate pins to avoid bus contention
 *  - Do NOT use GPIO 39-42 for the TFT: those are ESP32-S3 USB-JTAG pins and can
 *    wedge the tft task when USB is connected (task_wdt, CPU 0: tft).
 *  - Bus MISO must be the touch T_OUT pin (HR2046/XPT2046), not -1
 *
 * Build flags (firmware platformio.ini):
 *  -DMEIN_MUI_NODE=1
 *  -DLGFX_DRIVER=LGFX_MEIN_MUI_NODE
 */

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>

// Prefer a display-specific clock so LoRa can keep its own SPI_FREQUENCY in variant.h.
#ifndef LGFX_SPI_FREQUENCY
#ifdef SPI_FREQUENCY
#define LGFX_SPI_FREQUENCY SPI_FREQUENCY
#else
#define LGFX_SPI_FREQUENCY 10000000
#endif
#endif

// Defaults avoid USB-JTAG pins (39-42) on ESP32-S3.
#ifndef LGFX_PIN_SCK
#define LGFX_PIN_SCK 21
#endif
#ifndef LGFX_PIN_MOSI
#define LGFX_PIN_MOSI 16
#endif
#ifndef LGFX_PIN_MISO
#define LGFX_PIN_MISO 4 // touch T_OUT — required for shared-bus touch
#endif
#ifndef LGFX_PIN_DC
#define LGFX_PIN_DC 5
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

#if defined(LGFX_TOUCH_CS)
#define MEIN_TOUCH_CS LGFX_TOUCH_CS
#elif defined(TOUCH_CS_PIN)
#define MEIN_TOUCH_CS TOUCH_CS_PIN
#else
#define MEIN_TOUCH_CS 15
#endif

// Force polling touch unless explicitly opted in.
#ifdef MEIN_TOUCH_USE_INT
#if defined(LGFX_TOUCH_INT)
#define MEIN_TOUCH_INT LGFX_TOUCH_INT
#elif defined(TOUCH_INT_PIN)
#define MEIN_TOUCH_INT TOUCH_INT_PIN
#else
#define MEIN_TOUCH_INT 5
#endif
#else
#define MEIN_TOUCH_INT -1
#endif

#ifndef LGFX_TOUCH_SPI_FREQ
#define LGFX_TOUCH_SPI_FREQ 2500000
#endif

#ifndef LGFX_PANEL_READABLE
#define LGFX_PANEL_READABLE false
#endif

#ifdef MEIN_LGFX_USE_DMA
#ifdef LGFX_CFG_DMA_CH
#define MEIN_LGFX_DMA_CH LGFX_CFG_DMA_CH
#else
#define MEIN_LGFX_DMA_CH SPI_DMA_CH_AUTO
#endif
#else
#define MEIN_LGFX_DMA_CH 0
#endif

#ifndef LGFX_OFFSET_ROTATION
#define LGFX_OFFSET_ROTATION 1
#endif

#ifndef LGFX_TOUCH_OFFSET_ROTATION
#define LGFX_TOUCH_OFFSET_ROTATION LGFX_OFFSET_ROTATION
#endif

class LGFX_MEIN_MUI_NODE : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
#ifndef MEIN_MUI_NO_TOUCH
    lgfx::Touch_XPT2046 _touch_instance;
#endif
    uint8_t brightness = 200;

  public:
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
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = LGFX_SPI_FREQUENCY;
            cfg.freq_read = 8000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = MEIN_LGFX_DMA_CH;
            cfg.pin_sclk = LGFX_PIN_SCK;
            cfg.pin_mosi = LGFX_PIN_MOSI;
            cfg.pin_miso = LGFX_PIN_MISO;
            cfg.pin_dc = LGFX_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = LGFX_PIN_CS;
            cfg.pin_rst = LGFX_PIN_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = LGFX_OFFSET_ROTATION;
            cfg.readable = LGFX_PANEL_READABLE;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = LGFX_PIN_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

#ifndef MEIN_MUI_NO_TOUCH
        {
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
#endif

        setPanel(&_panel_instance);
    }
};
