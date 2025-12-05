/*
 * LVGL Compatibility Header
 * 
 * LVGL v8.3.3 already includes messaging and QRCode functionality.
 * This header ensures fonts are declared and available.
 */

#ifndef LVGL_V9_STUBS_HPP
#define LVGL_V9_STUBS_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* ============================================================================
 * LVGL Messaging System
 * Already provided by LVGL v8.3.3 - functions are available from lv_msg.h
 * ============================================================================ */

// All messaging functions are declared in LVGL:
// - lv_msg_subscribe()
// - lv_msg_subscribe_obj()  
// - lv_msg_send()
// - lv_msg_get_id()
// - lv_msg_get_payload()
// - lv_msg_get_user_data()
// - lv_event_get_msg()

/* ============================================================================
 * LVGL QRCode Widget - provided by LVGL library
 * ============================================================================ */

// All QRCode functions are declared in LVGL:
// - lv_qrcode_create()
// - lv_qrcode_update()

/* ============================================================================
 * LVGL QRCode Widget - provided by LVGL library
 * ============================================================================ */

/* ============================================================================
 * Font Declarations (ensure they exist)
 * ============================================================================ */

// Declare montserrat fonts - these should exist in LVGL v8.3.3
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif  // LVGL_V9_STUBS_HPP


