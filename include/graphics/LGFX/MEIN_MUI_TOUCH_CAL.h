#pragma once

#include <cstdint>

/**
 * Baked-in XPT2046 calibration for Mein MUI (KMRTM35018 / HR2046).
 * Captured 2026-08-13 with LGFX_OFFSET_ROTATION=3 / LGFX_TOUCH_OFFSET_ROTATION=3.
 * Keep all call sites in sync via this header.
 */
#if defined(MEIN_MUI_NODE)
inline constexpr uint16_t MEIN_MUI_TOUCH_CAL[8] = {3817, 3895, 267, 3898, 3790, 282, 322, 295};
#endif
