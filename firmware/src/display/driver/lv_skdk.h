/**
 * @file lv_linux_fb_dev_h
 *
 */

#include "LGFX_SKDK.hpp"

#include <LovyanGFX.hpp>
#ifdef __cplusplus

extern "C"
{
#endif

/*********************
 *      INCLUDES
 *********************/
#pragma once
#include "lvgl.h"

    /*********************
     *      DEFINES
     *********************/

    /**********************
     *      TYPEDEFS
     **********************/

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void lv_skdk_create();

    lv_disp_drv_t *lv_skdk_get_disp_drv();
    LGFX *lv_skdk_get_lcd();

    // Frame statistics since the last call: frames drawn, the total and longest
    // time (ms) LVGL spent rendering and sending one, pixels drawn, and the
    // time spent handing strips to the panel.
    void lv_skdk_take_perf(uint32_t *frames, uint32_t *total_ms, uint32_t *max_ms, uint32_t *pixels, uint32_t *flush_ms);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif