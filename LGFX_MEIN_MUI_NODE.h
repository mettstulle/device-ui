#pragma once

#define LGFX_USE_V1
#include "util/ILog.h"
#include <LovyanGFX.hpp>

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000   // ILI9488 läuft meist stabiler bei 40MHz als bei 80MHz
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
            cfg.pin_sclk    = 39;
            cfg.pin_mosi    = 40;
            cfg.pin_miso    = 41;
            cfg.pin_dc      = 42;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // ===== Display-Panel (ILI9488) =====
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 2;
            cfg.pin_rst  = -1;   // kein eigener Reset-Pin verdrahtet -> Software-Reset
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
            cfg.pin_bl      = 1;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        { // ===== Touch (XPT2046, resistiv, SPI – am selben Bus wie das Display) =====
            auto cfg = _touch_instance.config();
            cfg.pin_cs   = 4;
            cfg.pin_int  = 5;
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
