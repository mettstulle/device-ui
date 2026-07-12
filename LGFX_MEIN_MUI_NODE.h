#pragma once

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif
#ifndef LGFX_PIN_SCK
#define LGFX_PIN_SCK 39
#endif
#ifndef LGFX_PIN_MOSI
#define LGFX_PIN_MOSI 40
#endif
#ifndef LGFX_PIN_MISO
#define LGFX_PIN_MISO 41
#endif
#ifndef LGFX_PIN_DC
#define LGFX_PIN_DC 42
#endif
#ifndef LGFX_PIN_CS
#define LGFX_PIN_CS 2
#endif
#ifndef LGFX_PIN_RST
#define LGFX_PIN_RST -1
#endif
#ifndef LGFX_PIN_BL
#define LGFX_PIN_BL 1
#endif
#ifndef TOUCH_CS_PIN
#define TOUCH_CS_PIN 4
#endif
#ifndef TOUCH_INT_PIN
#define TOUCH_INT_PIN 5
#endif

class LGFX_MEIN_MUI_NODE : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;
    uint8_t brightness = 200;

  public:
    const uint32_t screenWidth  = 480;
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
        { // ===== SPI-Bus fürs Display (eigener Bus, getrennt vom LoRa-SPI) =====
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = SPI_FREQUENCY;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = LGFX_PIN_SCK;
            cfg.pin_mosi    = LGFX_PIN_MOSI;
            cfg.pin_miso    = LGFX_PIN_MISO;
            cfg.pin_dc      = LGFX_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // ===== Display-Panel (ILI9488) =====
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = LGFX_PIN_CS;
            cfg.pin_rst  = LGFX_PIN_RST;
            cfg.pin_busy = -1;

            cfg.panel_width   = screenHeight; // LovyanGFX erwartet die native (Portrait-)Maße hier
            cfg.panel_height  = screenWidth;
            cfg.offset_x      = 0;
            cfg.offset_y      = 0;
            cfg.offset_rotation = 1;  // 1 = Landscape; bei falscher Ausrichtung 0/2/3 durchprobieren
            cfg.readable      = true;
            cfg.invert        = false; // bei Farbfehlern (z. B. Blau/Rot vertauscht wirkende Bilder) auf true testen
            cfg.rgb_order     = false; // bei vertauschten Farben auf true testen
            cfg.dlen_16bit    = false;
            cfg.bus_shared    = true;
            _panel_instance.config(cfg);
        }

        { // ===== Backlight =====
            auto cfg = _light_instance.config();
            cfg.pin_bl      = LGFX_PIN_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        { // ===== Touch (XPT2046, resistiv, SPI – am selben Bus wie das Display) =====
            auto cfg = _touch_instance.config();
            cfg.pin_cs   = TOUCH_CS_PIN;
            cfg.pin_int  = TOUCH_INT_PIN;
            cfg.bus_shared = true;
            cfg.spi_host = SPI2_HOST;
            cfg.freq     = 2500000;
            cfg.x_min = 0; cfg.x_max = 4095;   // Rohwerte; nach Kalibrierung in Schritt 6 ggf. anpassen
            cfg.y_min = 0; cfg.y_max = 4095;
            cfg.offset_rotation = 0;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
