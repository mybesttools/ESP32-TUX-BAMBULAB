/*
MIT License

Copyright (c) 2022 Sukesh Ashok Kumar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ota.h"
#include "widgets/tux_panel.h"
#include "widgets/carousel_widget.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include "OpenWeatherMap.hpp"
#include "apps/weather/weathericons.h"
#include "events/gui_events.hpp"
#include <esp_partition.h>
#include "SettingsConfig.hpp"
#include "BambuMonitor.hpp"
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cJSON.h>  // For file-based weather polling
#include <sys/stat.h>  // For stat() to check file metadata

LV_IMG_DECLARE(dev_bg)
//LV_IMG_DECLARE(tux_logo)

// Forward declarations
extern SettingsConfig *cfg;

// LV_FONT_DECLARE(font_7seg_64)
// LV_FONT_DECLARE(font_7seg_60)
// LV_FONT_DECLARE(font_7seg_58)
LV_FONT_DECLARE(font_7seg_56)

//LV_FONT_DECLARE(font_robotomono_12)
LV_FONT_DECLARE(font_robotomono_13)

LV_FONT_DECLARE(font_fa_14)
#define FA_SYMBOL_BLE "\xEF\x8A\x94"      // 0xf294
#define FA_SYMBOL_SETTINGS "\xEF\x80\x93" // 0xf0ad

/*********************
 *      DEFINES
 *********************/
#define HEADER_HEIGHT 60
#define FOOTER_HEIGHT 30

/******************
 *  LV DEFINES
 ******************/
static const lv_font_t *font_normal;
static const lv_font_t *font_symbol;
static const lv_font_t *font_fa;
static const lv_font_t *font_xl;
static const lv_font_t *font_xxl;
static const lv_font_t *font_large;

static lv_obj_t *panel_header;
static lv_obj_t *panel_title;
static lv_obj_t *panel_status; // Status icons in the header
static lv_obj_t *content_container;
static lv_obj_t *screen_container;
static lv_obj_t *qr_status_container;

// TUX Panels
static lv_obj_t *tux_clock_weather;

static lv_obj_t *island_wifi;
static lv_obj_t *island_ota;
static lv_obj_t *island_devinfo;
static lv_obj_t *prov_qr;

// Carousel widget for multi-location weather and printer display
static CarouselWidget *carousel_widget = nullptr;

// Slide countries map by INDEX (lazy initialization to avoid static init issues)
static std::map<int, std::string> *slide_country_by_index_ptr = nullptr;

// UI IPC queue for marshaling data into the LVGL task context
enum class ui_ipc_type : uint8_t {
    TIME = 0,
};

struct ui_ipc_msg_t {
    ui_ipc_type type;
    struct tm time_payload;
};

static QueueHandle_t ui_ipc_queue = nullptr;
static lv_timer_t *ui_ipc_timer = nullptr;
static lv_timer_t *weather_poll_timer = nullptr;  // File-based weather polling timer

// Global buffers for datetime callback (avoid stack corruption)
static char g_time_buf[32] = {0};
static char g_ampm_buf[16] = {0};
static char g_date_buf[128] = {0};
static char g_current_time[128] = {0};
static char g_subtitle_buf[300] = {0};  // 128 + 128 + separator + null

// Forward declarations for UI IPC helpers
bool ui_ipc_post_time(const struct tm &dtinfo);
void ui_ipc_init();

// Forward declaration for file-based weather polling
static void weather_poll_timer_cb(lv_timer_t *timer);
static void poll_weather_files();
static void weather_poll_init();

// Forward declaration for file-based printer polling
static void printer_poll_timer_cb(lv_timer_t *timer);
static void poll_printer_files();
static void printer_poll_init();

static lv_obj_t *label_title;
static lv_obj_t *label_message;
static lv_obj_t *lbl_version;
static lv_obj_t *lbl_update_status;
static lv_obj_t *lbl_scan_status;

static lv_obj_t *lbl_device_info;

static lv_obj_t *icon_storage;
static lv_obj_t *icon_wifi;
static lv_obj_t *icon_ble;
static lv_obj_t *icon_battery;

/* Date/Time */
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_ampm;
static lv_obj_t *lbl_date;

/* Weather */
static lv_obj_t *lbl_weathericon;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_hl;

static lv_obj_t *lbl_wifi_status;
static lv_obj_t *lbl_webui_url;  // Web UI URL label

static lv_coord_t screen_h;
static lv_coord_t screen_w;

/******************
 *  LVL STYLES
 ******************/
static lv_style_t style_content_bg;

static lv_style_t style_message;
static lv_style_t style_title;
static lv_style_t style_iconstatus;
static lv_style_t style_storage;
static lv_style_t style_wifi;
static lv_style_t style_ble;
static lv_style_t style_battery;

static lv_style_t style_ui_island;

static lv_style_t style_glow;

/******************
 *  FOOTER AUTO-HIDE
 ******************/
static lv_obj_t *panel_footer_global = NULL;
static bool footer_visible = false;
static lv_obj_t *content_container_global = NULL;  // For resizing when footer toggles

/******************
 *  SLIDESHOW MODE
 ******************/
static bool slideshow_enabled = true;  // Carousel navigation - keep enabled
static uint32_t slideshow_slide_idx = 0;
static lv_timer_t *slideshow_timer = NULL;
#define SLIDESHOW_SLIDE_DURATION_MS 8000  // 8 seconds per slide

/******************
 *  LVL ANIMATION
 ******************/
static lv_anim_t anim_labelscroll;

void anim_move_left_x(lv_obj_t * TargetObject, int start_x, int end_x, int delay);
void tux_anim_callback_set_x(lv_anim_t * a, int32_t v);

void anim_move_left_y(lv_obj_t * TargetObject, int start_y, int end_y, int delay);
void tux_anim_callback_set_y(lv_anim_t * a, int32_t v);

void anim_fade_in(lv_obj_t * TargetObject, int delay);
void anim_fade_out(lv_obj_t * TargetObject, int delay);

void tux_anim_callback_set_opacity(lv_anim_t * a, int32_t v);
/******************
 *  LVL FUNCS & EVENTS
 ******************/
static void create_page_home(lv_obj_t *parent);
static void create_page_settings(lv_obj_t *parent);
static void create_page_updates(lv_obj_t *parent);
static void create_page_remote(lv_obj_t *parent);
static void create_page_bambu(lv_obj_t *parent);

// Forward declarations for Bambu callbacks
static void bambu_status_cb(void * s, lv_msg_t * m);
static void bambu_progress_cb(void * s, lv_msg_t * m);
static void bambu_temps_cb(void * s, lv_msg_t * m);

// Home page islands
static void tux_panel_clock_weather(lv_obj_t *parent);
static void tux_panel_config(lv_obj_t *parent);

// Setting page islands
static void tux_panel_devinfo(lv_obj_t *parent);
static void tux_panel_ota(lv_obj_t *parent);
static void tux_panel_wifi(lv_obj_t *parent);

static void create_header(lv_obj_t *parent);
static void create_footer(lv_obj_t *parent);

static void footer_message(const char *fmt, ...);
static void create_splash_screen();
static void switch_theme(bool dark);
static void qrcode_ui(lv_obj_t *parent);
static void show_ui();

static const char* get_firmware_version();

static void rotate_event_handler(lv_event_t *e);
static void theme_switch_event_handler(lv_event_t *e);
static void espwifi_event_handler(lv_event_t* e);
static void weather_location_event_handler(lv_event_t *e);
static void printer_config_event_handler(lv_event_t *e);
//static void espble_event_handler(lv_event_t *e);
static void checkupdates_event_handler(lv_event_t *e);
static void home_clicked_eventhandler(lv_event_t *e);
static void status_clicked_eventhandler(lv_event_t *e);
static void footer_button_event_handler(lv_event_t *e);
static void screen_touch_event_handler(lv_event_t *e);

// static void new_theme_apply_cb(lv_theme_t * th, lv_obj_t * obj);

/* MSG Events */
void datetime_event_cb(lv_event_t * e);
// weather_event_cb REMOVED - replaced by file-based polling

static void status_change_cb(void * s, lv_msg_t *m);
static void lv_update_battery(uint batval);
static void set_weather_icon(string weatherIcon);

static int current_page = 0;

void lv_setup_styles()
{
    font_symbol = &lv_font_montserrat_14;
    font_normal = &lv_font_montserrat_14;
    font_large = &lv_font_montserrat_16;
    font_xl = &lv_font_montserrat_24;
    font_xxl = &lv_font_montserrat_32;
    font_fa = &font_fa_14;

    screen_h = lv_obj_get_height(lv_scr_act());
    screen_w = lv_obj_get_width(lv_scr_act());

    /* CONTENT CONTAINER BACKGROUND */
    lv_style_init(&style_content_bg);
    lv_style_set_bg_opa(&style_content_bg, LV_OPA_50);
    lv_style_set_radius(&style_content_bg, 0);

// Enabling wallpaper image slows down scrolling perf etc...
#if 0  // DISABLED - use gradient to save memory for MQTT
    // Image Background
    // CF_INDEXED_8_BIT for smaller size - resolution 480x480
    // NOTE: Dynamic loading bg from SPIFF makes screen perf bad
    if (lv_fs_is_ready('F')) { // NO SD CARD load default
        ESP_LOGW(TAG,"Loading - F:/bg/dev_bg9.bin");
        lv_style_set_bg_img_src(&style_content_bg, "F:/bg/dev_bg9.bin");    
    } else {
        ESP_LOGW(TAG,"Loading - from firmware");
        lv_style_set_bg_img_src(&style_content_bg, &dev_bg);
    }
    //lv_style_set_bg_img_src(&style_content_bg, &dev_bg);
    // lv_style_set_bg_img_opa(&style_content_bg,LV_OPA_50);
#else
    ESP_LOGW(TAG,"Using Gradient (background image disabled to save memory)");
    // Gradient Background
    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_VER;
    grad.stops_count = 2;
    grad.stops[0].color = lv_color_make(31,32,34) ;
    grad.stops[1].color = lv_palette_main(LV_PALETTE_BLUE);
    grad.stops[0].frac = 150;
    grad.stops[1].frac = 190;
    lv_style_set_bg_grad(&style_content_bg, &grad);
#endif

    // DASHBOARD TITLE
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, font_large);
    lv_style_set_align(&style_title, LV_ALIGN_LEFT_MID);
    lv_style_set_pad_left(&style_title, 15);
    lv_style_set_border_width(&style_title, 0);
    lv_style_set_size(&style_title, LV_SIZE_CONTENT);

    // HEADER STATUS ICON PANEL
    lv_style_init(&style_iconstatus);
    lv_style_set_size(&style_iconstatus, LV_SIZE_CONTENT);
    lv_style_set_pad_all(&style_iconstatus, 0);
    lv_style_set_border_width(&style_iconstatus, 0);
    lv_style_set_align(&style_iconstatus, LV_ALIGN_RIGHT_MID);
    lv_style_set_pad_right(&style_iconstatus, 15);

    lv_style_set_layout(&style_iconstatus, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&style_iconstatus, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_main_place(&style_iconstatus, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_iconstatus, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_style_set_pad_row(&style_iconstatus, 3);

    // BATTERY
    lv_style_init(&style_battery);
    lv_style_set_text_font(&style_battery, font_symbol);
    lv_style_set_align(&style_battery, LV_ALIGN_RIGHT_MID);
    lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_RED));

    // SD CARD
    lv_style_init(&style_storage);
    lv_style_set_text_font(&style_storage, font_symbol);
    lv_style_set_align(&style_storage, LV_ALIGN_RIGHT_MID);

    // WIFI
    lv_style_init(&style_wifi);
    lv_style_set_text_font(&style_wifi, font_symbol);
    lv_style_set_align(&style_wifi, LV_ALIGN_RIGHT_MID);

    // BLE
    lv_style_init(&style_ble);
    lv_style_set_text_font(&style_ble, font_fa);
    lv_style_set_align(&style_ble, LV_ALIGN_RIGHT_MID);

    // FOOTER MESSAGE & ANIMATION
    lv_anim_init(&anim_labelscroll);
    lv_anim_set_delay(&anim_labelscroll, 1000);        // Wait 1 second to start the first scroll
    lv_anim_set_repeat_delay(&anim_labelscroll, 3000); // Repeat the scroll 3 seconds after the label scrolls back to the initial position

    lv_style_init(&style_message);
    lv_style_set_anim(&style_message, &anim_labelscroll); // Set animation for the style
    // lv_style_set_text_color(&style_message, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_opa(&style_message, LV_OPA_COVER);
    lv_style_set_text_font(&style_message, font_normal);
    lv_style_set_align(&style_message, LV_ALIGN_LEFT_MID);
    lv_style_set_pad_left(&style_message, 15);
    lv_style_set_pad_right(&style_message, 15);

    // UI ISLANDS
    lv_style_init(&style_ui_island);
    lv_style_set_bg_color(&style_ui_island, bg_theme_color);
    lv_style_set_bg_opa(&style_ui_island, LV_OPA_80);
    lv_style_set_border_color(&style_ui_island, bg_theme_color);
    //lv_style_set_border_opa(&style_ui_island, LV_OPA_80);
    lv_style_set_border_width(&style_ui_island, 1);
    lv_style_set_radius(&style_ui_island, 10);

    // FOOTER NAV BUTTONS
    lv_style_init(&style_glow);
    lv_style_set_bg_opa(&style_glow, LV_OPA_COVER);
    lv_style_set_border_width(&style_glow,0);
    lv_style_set_bg_color(&style_glow, lv_palette_main(LV_PALETTE_RED));

    /*Add a shadow*/
    // lv_style_set_shadow_width(&style_glow, 10);
    // lv_style_set_shadow_color(&style_glow, lv_palette_main(LV_PALETTE_RED));
    // lv_style_set_shadow_ofs_x(&style_glow, 5);
    // lv_style_set_shadow_ofs_y(&style_glow, 5);    
}

static void create_header(lv_obj_t *parent)
{
    // HEADER PANEL
    panel_header = lv_obj_create(parent);
    lv_obj_set_size(panel_header, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_set_style_pad_all(panel_header, 0, 0);
    lv_obj_set_style_radius(panel_header, 0, 0);
    lv_obj_set_align(panel_header, LV_ALIGN_TOP_MID);
    lv_obj_set_scrollbar_mode(panel_header, LV_SCROLLBAR_MODE_OFF);

    // HEADER TITLE PANEL
    panel_title = lv_obj_create(panel_header);
    lv_obj_add_style(panel_title, &style_title, 0);
    lv_obj_set_scrollbar_mode(panel_title, LV_SCROLLBAR_MODE_OFF);

    // HEADER TITLE
    label_title = lv_label_create(panel_title);
    lv_label_set_text(label_title, LV_SYMBOL_HOME " BAMBULAB MONITOR");

    // HEADER STATUS ICON PANEL
    panel_status = lv_obj_create(panel_header);
    lv_obj_add_style(panel_status, &style_iconstatus, 0);
    lv_obj_set_scrollbar_mode(panel_status, LV_SCROLLBAR_MODE_OFF);

    // BLE (hidden if not configured)
    icon_ble = lv_label_create(panel_status);
    lv_label_set_text(icon_ble, FA_SYMBOL_BLE);
    lv_obj_add_style(icon_ble, &style_ble, 0);
    #ifndef CONFIG_TUX_HAVE_BLUETOOTH
    lv_obj_add_flag(icon_ble, LV_OBJ_FLAG_HIDDEN);
    #endif

    // WIFI
    icon_wifi = lv_label_create(panel_status);
    lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_add_style(icon_wifi, &style_wifi, 0);

    // SD CARD (will be hidden if not present)
    icon_storage = lv_label_create(panel_status);
    lv_label_set_text(icon_storage, LV_SYMBOL_SD_CARD);
    lv_obj_add_style(icon_storage, &style_storage, 0);
    lv_obj_add_flag(icon_storage, LV_OBJ_FLAG_HIDDEN);

    // BATTERY (hidden if not configured)
    icon_battery = lv_label_create(panel_status);
    lv_label_set_text(icon_battery, LV_SYMBOL_CHARGE);
    lv_obj_add_style(icon_battery, &style_battery, 0);
    #ifndef CONFIG_TUX_HAVE_BATTERY
    lv_obj_add_flag(icon_battery, LV_OBJ_FLAG_HIDDEN);
    #endif

    // lv_obj_add_event_cb(panel_title, home_clicked_eventhandler, LV_EVENT_CLICKED, NULL);
    // lv_obj_add_event_cb(panel_status, status_clicked_eventhandler, LV_EVENT_CLICKED, NULL);
}

static void screen_touch_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED && panel_footer_global) {
        lv_indev_t * indev = lv_indev_get_act();
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        // If touching bottom 60px of screen, toggle footer
        if (point.y > screen_h - 60) {
            if (footer_visible) {
                // Hide footer - position just below visible screen
                lv_obj_set_y(panel_footer_global, screen_h - HEADER_HEIGHT);
                footer_visible = false;
                // Content stays same height - footer is accessible by scrolling down
            } else {
                // Show footer - slide it into view
                lv_obj_set_y(panel_footer_global, screen_h - HEADER_HEIGHT - FOOTER_HEIGHT);
                footer_visible = true;
                // Content stays same height
            }
        }
    }
}

static void create_footer(lv_obj_t *parent)
{
    lv_obj_t *panel_footer = lv_obj_create(parent);
    lv_obj_set_size(panel_footer, LV_PCT(100), FOOTER_HEIGHT);
    // lv_obj_set_style_bg_color(panel_footer, bg_theme_color, 0);
    lv_obj_set_style_pad_all(panel_footer, 0, 0);
    lv_obj_set_style_radius(panel_footer, 0, 0);
    lv_obj_set_align(panel_footer, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_scrollbar_mode(panel_footer, LV_SCROLLBAR_MODE_OFF);
    
    // Store global reference for toggle
    panel_footer_global = panel_footer;
    
    // Position just below content area - accessible by scrolling down
    lv_obj_set_y(panel_footer, screen_h - HEADER_HEIGHT);
    footer_visible = false;

/*
    // Create Footer label and animate if text is longer
    label_message = lv_label_create(panel_footer); // full screen as the parent
    lv_obj_set_width(label_message, LV_PCT(100));
    lv_label_set_long_mode(label_message, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_add_style(label_message, &style_message, LV_STATE_DEFAULT);
    lv_obj_set_style_align(label_message,LV_ALIGN_BOTTOM_LEFT,0);

    // Show LVGL version in the footer
    footer_message("A Touch UX Template using LVGL v%d.%d.%d", lv_version_major(), lv_version_minor(), lv_version_patch());
*/

    /* REPLACE STATUS BAR WITH BUTTON PANEL FOR NAVIGATION */
    // Button order: HOME | BAMBU (PRINTER) | SETTINGS | UPDATE
    // Icons: Home | Cube (3D model) | Cog | Download
    static const char * btnm_map[] = {LV_SYMBOL_HOME, "\xEF\x97\xB3", FA_SYMBOL_SETTINGS, LV_SYMBOL_DOWNLOAD,  NULL};
    lv_obj_t * footerButtons = lv_btnmatrix_create(panel_footer);
    lv_btnmatrix_set_map(footerButtons, btnm_map);
    lv_obj_set_style_text_font(footerButtons,&lv_font_montserrat_16,LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(footerButtons,LV_OPA_TRANSP,0);
    lv_obj_set_size(footerButtons,LV_PCT(100),LV_PCT(100));
    lv_obj_set_style_border_width(footerButtons,0,LV_PART_MAIN | LV_PART_ITEMS);
    lv_btnmatrix_set_btn_ctrl_all(footerButtons, LV_BTNMATRIX_CTRL_CHECKABLE);
    
    lv_btnmatrix_set_one_checked(footerButtons, true);   // only 1 button can be checked
    lv_btnmatrix_set_btn_ctrl(footerButtons,0,LV_BTNMATRIX_CTRL_CHECKED);

    // Very important but weird behavior
    lv_obj_set_height(footerButtons,FOOTER_HEIGHT+20);    
    lv_obj_set_style_radius(footerButtons,0,LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(footerButtons,LV_OPA_TRANSP,LV_PART_ITEMS);
    lv_obj_add_style(footerButtons, &style_glow,LV_PART_ITEMS | LV_BTNMATRIX_CTRL_CHECKED); // selected

    lv_obj_align(footerButtons, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(footerButtons, footer_button_event_handler, LV_EVENT_ALL, NULL); 
    
}

static void tux_panel_clock_weather(lv_obj_t *parent)
{
    tux_clock_weather = tux_panel_create(parent, "", 130);
    lv_obj_add_style(tux_clock_weather, &style_ui_island, 0);

    lv_obj_t *cont_panel = tux_panel_get_content(tux_clock_weather);
    lv_obj_set_flex_flow(tux_clock_weather, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tux_clock_weather, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ************ Date/Time panel
    lv_obj_t *cont_datetime = lv_obj_create(cont_panel);
    lv_obj_set_size(cont_datetime,180,120);
    lv_obj_set_flex_flow(cont_datetime, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scrollbar_mode(cont_datetime, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(cont_datetime,LV_ALIGN_LEFT_MID,0,0);
    lv_obj_set_style_bg_opa(cont_datetime,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_opa(cont_datetime,LV_OPA_TRANSP,0);
    //lv_obj_set_style_pad_gap(cont_datetime,10,0);
    lv_obj_set_style_pad_top(cont_datetime,20,0);

    // MSG - MSG_TIME_CHANGED - EVENT
    lv_obj_add_event_cb(cont_datetime, datetime_event_cb, LV_EVENT_MSG_RECEIVED, NULL);
    lv_msg_subscribe_obj(MSG_TIME_CHANGED, cont_datetime, NULL);

    // Time
    lbl_time = lv_label_create(cont_datetime);
    lv_obj_set_style_align(lbl_time, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_style_text_font(lbl_time, &font_7seg_56, 0);
    lv_label_set_text(lbl_time, "00:00");

    // AM/PM
    lbl_ampm = lv_label_create(cont_datetime);
    lv_obj_set_style_align(lbl_ampm, LV_ALIGN_TOP_LEFT, 0);
    lv_label_set_text(lbl_ampm, "AM");

    // Date
    lbl_date = lv_label_create(cont_datetime);
    lv_obj_set_style_align(lbl_date, LV_ALIGN_BOTTOM_MID, 0);
    lv_obj_set_style_text_font(lbl_date, font_large, 0);
    lv_obj_set_height(lbl_date,30);
    lv_label_set_text(lbl_date, "waiting for update");

    // ************ Weather panel (panel widen with weekly forecast in landscape)
    lv_obj_t *cont_weather = lv_obj_create(cont_panel);
    lv_obj_set_size(cont_weather,100,115);
    lv_obj_set_flex_flow(cont_weather, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont_weather, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(cont_weather, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align_to(cont_weather,cont_datetime,LV_ALIGN_OUT_RIGHT_MID,0,0);
    lv_obj_set_style_bg_opa(cont_weather,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_opa(cont_weather,LV_OPA_TRANSP,0);

    // MSG - MSG_WEATHER_CHANGED - EVENT
    // Note: weather_event_cb now only handles carousel; old cont_weather subscription disabled
    // lv_obj_add_event_cb(cont_weather, weather_event_cb, LV_EVENT_MSG_RECEIVED, NULL);
    // lv_msg_subscribe_obj(MSG_WEATHER_CHANGED, cont_weather, NULL);

    // This only for landscape
    // lv_obj_t *lbl_unit = lv_label_create(cont_weather);
    // lv_obj_set_style_text_font(lbl_unit, font_normal, 0);
    // lv_label_set_text(lbl_unit, "Light rain");

    // Weather icons
    lbl_weathericon = lv_label_create(cont_weather);
    lv_obj_set_style_text_font(lbl_weathericon, &font_fa_weather_42, 0);
    // "F:/weather/cloud-sun-rain.bin");//10d@2x.bin"
    lv_label_set_text(lbl_weathericon, FA_WEATHER_SUN);
    lv_obj_set_style_text_color(lbl_weathericon,lv_palette_main(LV_PALETTE_ORANGE),0);

    // Temperature
    lbl_temp = lv_label_create(cont_weather);
    //lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_font(lbl_temp, font_xl, 0);
    lv_obj_set_style_align(lbl_temp, LV_ALIGN_BOTTOM_MID, 0);
    lv_label_set_text(lbl_temp, "0°C");

    lbl_hl = lv_label_create(cont_weather);
    lv_obj_set_style_text_font(lbl_hl, font_normal, 0);
    lv_obj_set_style_align(lbl_hl, LV_ALIGN_BOTTOM_MID, 0);
    lv_label_set_text(lbl_hl, "H:0° L:0°");
}

static lv_obj_t * slider_label;
static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    lv_label_set_text_fmt(slider_label,"Brightness : %d",(int)lv_slider_get_value(slider));
    lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lcd.setBrightness((int)lv_slider_get_value(slider));
}

static void tux_panel_config(lv_obj_t *parent)
{
    /******** CONFIG & TESTING ********/
    lv_obj_t *island_2 = tux_panel_create(parent, LV_SYMBOL_EDIT " CONFIG", 200);
    lv_obj_add_style(island_2, &style_ui_island, 0);

    // Get Content Area to add UI elements
    lv_obj_t *cont_2 = tux_panel_get_content(island_2);

    lv_obj_set_flex_flow(cont_2, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(cont_2, 10, 0);
    //lv_obj_set_style_pad_column(cont_2, 5, 0);
    lv_obj_set_flex_align(cont_2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

    // Screen Brightness
    /*Create a label below the slider*/
    slider_label = lv_label_create(cont_2);
    lv_label_set_text_fmt(slider_label, "Brightness : %d", lcd.getBrightness());   

    lv_obj_t * slider = lv_slider_create(cont_2);
    lv_obj_center(slider);
    lv_obj_set_size(slider, LV_PCT(90), 20);
    lv_slider_set_range(slider, 50 , 255);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_TOP_MID, 0, 30);
    lv_bar_set_value(slider,lcd.getBrightness(),LV_ANIM_ON);

    // THEME Selection
    lv_obj_t *label = lv_label_create(cont_2);
    lv_label_set_text(label, "Theme : Dark");
    //lv_obj_set_size(label, LV_PCT(90), 20);
    lv_obj_align_to(label,slider,LV_ALIGN_OUT_TOP_MID,0,15);
    //lv_obj_center(slider);
    
    lv_obj_t *sw = lv_switch_create(cont_2);
    lv_obj_add_event_cb(sw, theme_switch_event_handler, LV_EVENT_ALL, label);
    lv_obj_align_to(label, sw, LV_ALIGN_OUT_TOP_MID, 0, 20);
    //lv_obj_align(sw,LV_ALIGN_RIGHT_MID,0,0);

    // Weather Location Selection
    lv_obj_t *weather_label = lv_label_create(cont_2);
    lv_label_set_text(weather_label, "Weather Location:");
    lv_obj_align_to(weather_label, sw, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    
    lv_obj_t *weather_dropdown = lv_dropdown_create(cont_2);
    lv_dropdown_set_options(weather_dropdown, "Kleve, Germany\nAmsterdam, Netherlands");
    lv_obj_set_size(weather_dropdown, LV_PCT(85), 30);
    lv_obj_align_to(weather_dropdown, weather_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_add_event_cb(weather_dropdown, weather_location_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Printer Management Button
    lv_obj_t *btn_printer = lv_btn_create(cont_2);
    lv_obj_align(btn_printer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(btn_printer, LV_SIZE_CONTENT, 30);
    lv_obj_add_event_cb(btn_printer, printer_config_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_printer = lv_label_create(btn_printer);
    lv_label_set_text(lbl_printer, "🖨️ Add Printer");
    lv_obj_center(lbl_printer);
    lv_obj_align_to(btn_printer, weather_dropdown, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);

    // Rotate to Portait/Landscape
    lv_obj_t *btn2 = lv_btn_create(cont_2);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(btn2, LV_SIZE_CONTENT, 30);
    lv_obj_add_event_cb(btn2, rotate_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_t *lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, "Rotate to Landscape");
    //lv_obj_center(lbl2);
    lv_obj_align_to(btn2, btn_printer, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

// Provision WIFI
static void tux_panel_wifi(lv_obj_t *parent)
{
    /******** PROVISION WIFI ********/
    island_wifi = tux_panel_create(parent, LV_SYMBOL_WIFI " WIFI STATUS", 270);
    lv_obj_add_style(island_wifi, &style_ui_island, 0);
    // tux_panel_set_title_color(island_wifi, lv_palette_main(LV_PALETTE_BLUE));

    // Get Content Area to add UI elements
    lv_obj_t *cont_1 = tux_panel_get_content(island_wifi);
    lv_obj_set_flex_flow(cont_1, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(cont_1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_wifi_status = lv_label_create(cont_1);
    lv_obj_set_size(lbl_wifi_status, LV_SIZE_CONTENT, 30);
    lv_obj_align(lbl_wifi_status, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(lbl_wifi_status, "Waiting for IP");

    // Web UI URL label
    lbl_webui_url = lv_label_create(cont_1);
    lv_obj_set_size(lbl_webui_url, LV_PCT(90), LV_SIZE_CONTENT);
    lv_label_set_long_mode(lbl_webui_url, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_webui_url, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl_webui_url, "Web UI: Waiting for IP...");

    // Check for Updates button
    lv_obj_t *btn_unprov = lv_btn_create(cont_1);
    lv_obj_set_size(btn_unprov, LV_SIZE_CONTENT, 40);
    lv_obj_align(btn_unprov, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *lbl2 = lv_label_create(btn_unprov);
    lv_label_set_text(lbl2, "Reset Wi-Fi Settings");
    lv_obj_center(lbl2);    

    /* ESP QR CODE inserted here */
    qr_status_container = lv_obj_create(cont_1);
    lv_obj_set_size(qr_status_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(qr_status_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_ver(qr_status_container, 3, 0);
    lv_obj_set_style_border_width(qr_status_container, 0, 0);
    lv_obj_set_flex_flow(qr_status_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(qr_status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_add_event_cb(btn_unprov, espwifi_event_handler, LV_EVENT_CLICKED, NULL);

    /* QR CODE */
    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_GREY, 4);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

    prov_qr = lv_qrcode_create(qr_status_container, 100, fg_color, bg_color);

    /* Set data - format of BLE provisioning data */
    // {"ver":"v1","name":"TUX_4AA440","pop":"abcd1234","transport":"ble"}
    const char *qrdata = "https://github.com/sukesh-ak/ESP32-TUX";
    lv_qrcode_update(prov_qr, qrdata, strlen(qrdata));

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(prov_qr, bg_color, 0);
    lv_obj_set_style_border_width(prov_qr, 5, 0);

    lbl_scan_status = lv_label_create(qr_status_container);
    lv_obj_set_size(lbl_scan_status, LV_SIZE_CONTENT, 30);
    lv_label_set_text(lbl_scan_status, "Scan to learn about ESP32-TUX");

}

static void tux_panel_ota(lv_obj_t *parent)
{
    /******** OTA UPDATES ********/
    island_ota = tux_panel_create(parent, LV_SYMBOL_DOWNLOAD " OTA UPDATES", 180);
    lv_obj_add_style(island_ota, &style_ui_island, 0);

    // Get Content Area to add UI elements
    lv_obj_t *cont_ota = tux_panel_get_content(island_ota);
    lv_obj_set_flex_flow(cont_ota, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_ota, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Current Firmware version
    lbl_version = lv_label_create(cont_ota);
    lv_obj_set_size(lbl_version, LV_SIZE_CONTENT, 30);
    lv_obj_align(lbl_version, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text_fmt(lbl_version, "Firmware Version %s",get_firmware_version());

    // Check for Updates button
    lv_obj_t *btn_checkupdates = lv_btn_create(cont_ota);
    lv_obj_set_size(btn_checkupdates, LV_SIZE_CONTENT, 40);
    lv_obj_align(btn_checkupdates, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *lbl2 = lv_label_create(btn_checkupdates);
    lv_label_set_text(lbl2, "Check for Updates");
    lv_obj_center(lbl2);
    lv_obj_add_event_cb(btn_checkupdates, checkupdates_event_handler, LV_EVENT_ALL, NULL);

    lv_obj_t *esp_updatestatus = lv_obj_create(cont_ota);
    lv_obj_set_size(esp_updatestatus, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(esp_updatestatus, LV_OPA_10, 0);
    lv_obj_set_style_border_width(esp_updatestatus, 0, 0);

    lbl_update_status = lv_label_create(esp_updatestatus);
    lv_obj_set_style_text_color(lbl_update_status, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(lbl_update_status, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(lbl_update_status, "Click to check for updates");
}

static void tux_panel_devinfo(lv_obj_t *parent)
{
    island_devinfo = tux_panel_create(parent, LV_SYMBOL_TINT " DEVICE INFO", 200);
    lv_obj_add_style(island_devinfo, &style_ui_island, 0);

    // Get Content Area to add UI elements
    lv_obj_t *cont_devinfo = tux_panel_get_content(island_devinfo);

    lbl_device_info = lv_label_create(cont_devinfo);
    // Monoaspace font for alignment
    lv_obj_set_style_text_font(lbl_device_info,&font_robotomono_13,0); 
}

static void create_page_remote(lv_obj_t *parent)
{

    static lv_style_t style;
    lv_style_init(&style);

    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 10);
    lv_style_set_bg_opa(&style, LV_OPA_80);
    // lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 1));

    /*Add a shadow*/
    lv_style_set_shadow_width(&style, 55);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_obj_t * island_remote = tux_panel_create(parent, LV_SYMBOL_KEYBOARD " REMOTE", LV_PCT(100));
    lv_obj_add_style(island_remote, &style_ui_island, 0);

    // Get Content Area to add UI elements
    lv_obj_t *cont_remote = tux_panel_get_content(island_remote);

    lv_obj_set_flex_flow(cont_remote, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont_remote, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_remote, 10, 0);
    lv_obj_set_style_pad_row(cont_remote, 10, 0);

    uint32_t i;
    for(i = 0; i <12; i++) {
        lv_obj_t * obj = lv_btn_create(cont_remote);
        lv_obj_add_style(obj, &style, LV_STATE_PRESSED);
        lv_obj_set_size(obj, 80, 80);
        
        lv_obj_t * label = lv_label_create(obj);
        lv_label_set_text_fmt(label, "%" LV_PRIu32, i);
        lv_obj_center(label);
    }

}

static void slideshow_timer_cb(lv_timer_t * timer)
{
    // Auto-advance carousel every 8 seconds
    if (!slideshow_enabled) return;
    
    // Advance to next slide in carousel
    if (carousel_widget && current_page == 0 && carousel_widget->slides.size() > 0) {  // Only if on home page with slides
        carousel_widget->next_slide();
    }
}

// Forward declarations
static void update_carousel_slides();

static void create_page_home(lv_obj_t *parent)
{
    /* HOME PAGE - CAROUSEL MODE */
    // Create carousel widget to display weather locations and printer status
    if (!carousel_widget) {
        // Hide header on home page for full-screen carousel
        if (panel_header) {
            lv_obj_add_flag(panel_header, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Use full screen height - carousel fills entire screen
        carousel_widget = new CarouselWidget(parent, screen_w, screen_h);
        // Position at top of screen
        lv_obj_set_size(carousel_widget->container, LV_PCT(100), screen_h);
        lv_obj_align(carousel_widget->container, LV_ALIGN_TOP_LEFT, 0, 0);
        
        // Subscribe carousel to time messages only
        lv_obj_add_event_cb(carousel_widget->container, datetime_event_cb, LV_EVENT_MSG_RECEIVED, NULL);
        lv_msg_subscribe_obj(MSG_TIME_CHANGED, carousel_widget->container, NULL);
        
        // Weather updates via file polling - no event subscription needed
        // weather_event_cb removed - poll_weather_files() reads from /spiffs/weather/<city>.json
        
        update_carousel_slides();
        
        // Start weather and printer file polling timers
        weather_poll_init();
        printer_poll_init();
    }
}

static void update_carousel_slides()
{
    if (!carousel_widget) return;
    
    ESP_LOGW(TAG, "update_carousel_slides() starting");
    
    // Clear existing slides
    carousel_widget->slides.clear();
    
    // Add weather location slides with more descriptive info
    if (cfg) {
        int weather_count = cfg->get_weather_location_count();
        for (int i = 0; i < weather_count; i++) {
            weather_location_t loc = cfg->get_weather_location(i);
            if (loc.enabled) {
                carousel_slide_t slide;
                // Use city name as title, country as subtitle with time placeholder
                slide.title = loc.city.empty() ? loc.name : loc.city;
                slide.subtitle = "--:-- • " + loc.country;
                slide.value1 = "--°C";  // Will be updated by weather callback
                slide.value2 = "Loading weather...";  // Will be updated by weather callback
                slide.value3 = "H: --° L: --° • Humidity: --%";  // Temp range and humidity
                slide.value4 = "Wind: -- m/s • Pressure: -- hPa";  // Wind and pressure
                slide.bg_color = lv_color_hex(0x1e3a5f);  // Blue weather theme
                if (!slide_country_by_index_ptr) slide_country_by_index_ptr = new std::map<int, std::string>();
                int slide_index = carousel_widget ? carousel_widget->slides.size() : 0;
                (*slide_country_by_index_ptr)[slide_index] = loc.country;  // Store country by index
                carousel_widget->add_slide(slide);
            }
        }
    }
    
    // Add printer status slides (only if online - check SPIFFS cache files)
    if (cfg) {
        int printer_count = cfg->get_printer_count();
        time_t now = time(NULL);
        const time_t ONLINE_THRESHOLD = 60;  // Printer is "online" if updated within 60 seconds
        
        for (int i = 0; i < printer_count; i++) {
            printer_config_t printer = cfg->get_printer(i);
            if (!printer.enabled) continue;
            
            // Check if printer has recent data (is online)
            std::string filepath = "/spiffs/printer/" + printer.serial + ".json";
            FILE *f = fopen(filepath.c_str(), "r");
            bool is_online = false;
            
            if (f) {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (fsize > 0 && fsize <= 20480) {
                    char *json_buf = (char*)malloc(fsize + 1);
                    if (json_buf) {
                        size_t read_size = fread(json_buf, 1, fsize, f);
                        json_buf[read_size] = '\0';
                        
                        cJSON *root = cJSON_Parse(json_buf);
                        free(json_buf);
                        
                        if (root) {
                            cJSON *last_update = cJSON_GetObjectItem(root, "last_update");
                            if (last_update) {
                                time_t update_time = (time_t)last_update->valuedouble;
                                is_online = (now - update_time) < ONLINE_THRESHOLD;
                            }
                            cJSON_Delete(root);
                        }
                    }
                }
                fclose(f);
            }
            
            // Only add printer to carousel if it's online
            if (is_online) {
                carousel_slide_t slide;
                slide.title = printer.name;
                slide.subtitle = "Status: Idle";  // Will be updated by printer callback
                slide.value1 = "0%";  // Progress - will be updated
                slide.value2 = "Nozzle: 0°C";  // Will be updated
                slide.bg_color = lv_color_hex(0x3a1e2f);  // Purple printer theme
                carousel_widget->add_slide(slide);
                ESP_LOGI(TAG, "Added online printer %s to carousel", printer.name.c_str());
            } else {
                ESP_LOGD(TAG, "Printer %s offline or no data, skipping carousel",
                         printer.name.c_str());
            }
        }
    }
    
    // If only the time slide exists, add a welcome message
    if (carousel_widget->slides.size() == 1) {
        carousel_slide_t placeholder;
        placeholder.title = "Welcome to TUX";
        placeholder.subtitle = "Add locations & printers";
        placeholder.value1 = "to see more info";
        placeholder.value2 = "";
        placeholder.bg_color = lv_color_hex(0x2a2a2a);
        carousel_widget->add_slide(placeholder);
    }
    
    carousel_widget->update_slides();
    
    // Force layout update to make carousel visible immediately
    lv_obj_update_layout(carousel_widget->container);
}

static void create_page_settings(lv_obj_t *parent)
{
    /* SETTINGS PAGE PANELS */
    tux_panel_wifi(parent);
    tux_panel_config(parent);
}

static void create_page_updates(lv_obj_t *parent)
{
    /* OTA UPDATES PAGE PANELS */
    tux_panel_ota(parent);
    tux_panel_devinfo(parent);    
}

static void create_page_bambu(lv_obj_t *parent)
{
    /* BAMBU MONITOR PAGE PANELS */
    lv_obj_t *panel = tux_panel_create(parent, "🖨️ PRINTER", LV_SIZE_CONTENT);
    lv_obj_add_style(panel, &style_ui_island, 0);
    
    lv_obj_t *cont = tux_panel_get_content(panel);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    
    // Status label
    lv_obj_t *lbl_status = lv_label_create(cont);
    lv_label_set_text(lbl_status, "Status: Offline");
    
    // Progress label
    lv_obj_t *lbl_progress = lv_label_create(cont);
    lv_label_set_text(lbl_progress, "Progress: --");
    
    // Temperature label
    lv_obj_t *lbl_temps = lv_label_create(cont);
    lv_label_set_text(lbl_temps, "Bed: -- Nozzle: --");
    
    // Test Query Button
    lv_obj_t *btn_query = lv_btn_create(cont);
    lv_obj_set_size(btn_query, 200, 50);
    lv_obj_t *lbl_query = lv_label_create(btn_query);
    lv_label_set_text(lbl_query, "Send Query");
    lv_obj_center(lbl_query);
    lv_obj_add_event_cb(btn_query, [](lv_event_t *e) {
        ESP_LOGI("GUI", "Query button clicked - sending MQTT query");
        esp_err_t ret = bambu_send_query();
        if (ret == ESP_OK) {
            ESP_LOGI("GUI", "Query sent successfully");
        } else {
            ESP_LOGE("GUI", "Failed to send query");
        }
    }, LV_EVENT_CLICKED, NULL);
    
    // Subscribe to Bambu messages
    lv_msg_subscribe(MSG_BAMBU_STATUS, bambu_status_cb, lbl_status);
    lv_msg_subscribe(MSG_BAMBU_PROGRESS, bambu_progress_cb, lbl_progress);
    lv_msg_subscribe(MSG_BAMBU_TEMPS, bambu_temps_cb, lbl_temps);
}

static void create_splash_screen()
{
    lv_obj_t * splash_screen = lv_scr_act();
    lv_obj_set_style_bg_color(splash_screen, lv_color_black(),0);
    
    // Create a container for logo and text
    lv_obj_t *splash_container = lv_obj_create(splash_screen);
    lv_obj_set_size(splash_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(splash_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(splash_container, 0, 0);
    lv_obj_set_flex_flow(splash_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(splash_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Logo animation
    lv_obj_t * splash_img = lv_img_create(splash_container);
    lv_img_set_src(splash_img, "F:/bg/tux-logo.bin");
    lv_obj_set_style_pad_bottom(splash_img, 20, 0);
    
    // MyBestTools text
    lv_obj_t * splash_text = lv_label_create(splash_container);
    lv_label_set_text(splash_text, "MyBestTools");
    lv_obj_set_style_text_font(splash_text, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(splash_text, lv_color_white(), 0);

    //lv_scr_load_anim(splash_screen, LV_SCR_LOAD_ANIM_FADE_IN, 5000, 10, true);
    lv_scr_load(splash_screen);
}

static void show_ui()
{
    // screen_container is the root of the UX
    screen_container = lv_obj_create(NULL);

    lv_obj_set_size(screen_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(screen_container, 0, 0);
    lv_obj_align(screen_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(screen_container, 0, 0);
    lv_obj_set_scrollbar_mode(screen_container, LV_SCROLLBAR_MODE_OFF);

    // Gradient / Image Background for screen container
    lv_obj_add_style(screen_container, &style_content_bg, 0);

    // HEADER & FOOTER
    create_header(screen_container);
    create_footer(screen_container);

    // CONTENT CONTAINER 
    content_container = lv_obj_create(screen_container);
    content_container_global = content_container;  // Store global reference
    // Use full screen height - header is hidden on home page
    lv_obj_set_size(content_container, screen_w, screen_h);
    lv_obj_align(content_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content_container, 0, 0);  // Remove default padding
    
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);

    // Show Home Page
    create_page_home(content_container);
    //create_page_settings(content_container);
    //create_page_remote(content_container);

    // Initialize slideshow timer (presentation mode - auto cycles through slides)
    if (slideshow_enabled) {
        slideshow_timer = lv_timer_create(slideshow_timer_cb, SLIDESHOW_SLIDE_DURATION_MS, NULL);
        ESP_LOGI(TAG, "Slideshow mode enabled - auto-cycling every %d ms", SLIDESHOW_SLIDE_DURATION_MS);
    }

    // Load main screen with animation
    //lv_scr_load(screen_container);
    lv_scr_load_anim(screen_container, LV_SCR_LOAD_ANIM_FADE_IN, 1000,100, true);

    // Status subscribers
    lv_msg_subscribe(MSG_WIFI_PROV_MODE, status_change_cb, NULL);    
    lv_msg_subscribe(MSG_WIFI_CONNECTED, status_change_cb, NULL);    
    lv_msg_subscribe(MSG_WIFI_DISCONNECTED, status_change_cb, NULL);    
    lv_msg_subscribe(MSG_OTA_STATUS, status_change_cb, NULL);    
    lv_msg_subscribe(MSG_SDCARD_STATUS, status_change_cb, NULL);  
    lv_msg_subscribe(MSG_BATTERY_STATUS, status_change_cb, NULL);  
    lv_msg_subscribe(MSG_DEVICE_INFO, status_change_cb, NULL);      

    // Send default page load notification => HOME
    lv_msg_send(MSG_PAGE_HOME,NULL);
}

static void rotate_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);

    if (code == LV_EVENT_CLICKED)
    {
        lvgl_acquire();

        if (lv_disp_get_rotation(disp) == LV_DISP_ROT_270)
            lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);
        else
            lv_disp_set_rotation(disp, (lv_disp_rot_t)(lv_disp_get_rotation(disp) + 1));

        if (LV_HOR_RES > LV_VER_RES)
            lv_label_set_text(label, "Rotate to Portrait");
        else
            lv_label_set_text(label, "Rotate to Landscape");

        lvgl_release();

        // Update
        screen_h = lv_obj_get_height(lv_scr_act());
        screen_w = lv_obj_get_width(lv_scr_act());
        lv_obj_set_size(content_container, screen_w, screen_h - HEADER_HEIGHT - FOOTER_HEIGHT);

        // footer_message("%d,%d",screen_h,screen_w);
    }
}

static void theme_switch_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *udata = (lv_obj_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        LV_LOG_USER("State: %s\n", lv_obj_has_state(obj, LV_STATE_CHECKED) ? "On" : "Off");
        if (lv_obj_has_state(obj, LV_STATE_CHECKED))
        {
            switch_theme(false);
            lv_label_set_text(udata, "Theme : Light");

            // Pass the new theme info
            // ESP_ERROR_CHECK(esp_event_post(TUX_EVENTS, TUX_EVENT_THEME_CHANGED, NULL,NULL, portMAX_DELAY));
        }
        else
        {
            switch_theme(true);
            lv_label_set_text(udata, "Theme : Dark");
            
            // Pass the new theme info
            // ESP_ERROR_CHECK(esp_event_post(TUX_EVENTS, TUX_EVENT_THEME_CHANGED, NULL,NULL, portMAX_DELAY));
        }
    }
}

static void weather_location_event_handler(lv_event_t *e)
{
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    // Send message with selected index - will be handled by main.cpp
    lv_msg_send(MSG_WEATHER_LOCATION_CHANGED, (void*)(uintptr_t)selected);
    ESP_LOGI("GUI", "Weather location changed to index: %d", selected);
}

static void printer_config_event_handler(lv_event_t *e)
{
    // Placeholder for printer configuration dialog
    // In a full implementation, this would open a modal with fields to add printer details:
    // - Printer Name (text input)
    // - IP Address (text input)
    // - API Token (text input)
    // - Enabled/Disabled toggle
    
    ESP_LOGI("GUI", "Add Printer clicked - TODO: Show printer configuration dialog");
    lv_msg_send(MSG_PRINTER_CONFIG, NULL);
}

static void footer_message(const char *fmt, ...)
{
    char buffer[200];
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    lv_label_set_text(label_message, buffer);
    va_end(args);
}

static void home_clicked_eventhandler(lv_event_t *e)
{
    // footer_message("Home clicked!");
    //  Clean the content container first
    lv_obj_clean(content_container);
    create_page_home(content_container);
}

static void status_clicked_eventhandler(lv_event_t *e)
{
    // footer_message("Status icons touched but this is a very long message to show scroll animation!");
    //  Clean the content container first
    lv_obj_clean(content_container);
    create_page_settings(content_container);
    //create_page_remote(content_container);
}

void switch_theme(bool dark)
{
    if (dark)
    {
        theme_current = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_GREEN),
                                              1, /*Light or dark mode*/
                                              &lv_font_montserrat_14);
        bg_theme_color = lv_palette_darken(LV_PALETTE_GREY, 5);
        lv_disp_set_theme(disp, theme_current);
        //bg_theme_color = theme_current->flags & LV_USE_THEME_DEFAULT ? lv_palette_darken(LV_PALETTE_GREY, 5) : lv_palette_lighten(LV_PALETTE_GREY, 2);
        // lv_theme_set_apply_cb(theme_current, new_theme_apply_cb);

        lv_style_set_bg_color(&style_ui_island, bg_theme_color);
        //lv_style_set_bg_opa(&style_ui_island, LV_OPA_80);

        ESP_LOGI(TAG,"Dark theme set");
    }
    else
    {
        theme_current = lv_theme_default_init(disp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              0, /*Light or dark mode*/
                                              &lv_font_montserrat_14);
        //bg_theme_color = lv_palette_lighten(LV_PALETTE_GREY, 5);    // #BFBFBD
        // bg_theme_color = lv_color_make(0,0,255); 
        bg_theme_color = lv_color_hex(0xBFBFBD); //383837


        lv_disp_set_theme(disp, theme_current);
        // lv_theme_set_apply_cb(theme_current, new_theme_apply_cb);
        lv_style_set_bg_color(&style_ui_island, bg_theme_color);
        ESP_LOGI(TAG,"Light theme set");        

    }
}

// /*Will be called when the styles of the base theme are already added
//   to add new styles*/
// static void new_theme_apply_cb(lv_theme_t * th, lv_obj_t * obj)
// {
//     LV_UNUSED(th);

//     if(lv_obj_check_type(obj, &tux_panel_class)) {
//         lv_obj_add_style(obj, &style_ui_island, 0);
//         //lv_style_set_bg_color(&style_ui_island,theme_current->color_primary);
//     }

// }

static void espwifi_event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        bool provisioned = false;
        ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
        if (provisioned) {
            wifi_prov_mgr_reset_provisioning();     // reset wifi
            
            // Reset device to start provisioning
            lv_label_set_text(lbl_wifi_status, "Wi-Fi Disconnected!");
            lv_obj_set_style_text_color(lbl_wifi_status, lv_palette_main(LV_PALETTE_YELLOW), 0);
            lv_label_set_text(lbl_scan_status, "Restart device to provision WiFi.");
            lv_obj_add_state( btn, LV_STATE_DISABLED );  /* Disable */
        }
    }
}

inline void checkupdates_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        /*Get the first child of the button which is the label and change its text*/
        // Maybe disable the button and enable once completed
        //lv_obj_t *label = lv_obj_get_child(btn, 0);
        //lv_label_set_text_fmt(label, "Checking for updates...");
        LV_LOG_USER("Clicked");
        lv_msg_send(MSG_OTA_INITIATE,NULL);
    }
}

static const char* get_firmware_version()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    static std::string cached_version;

    if (cached_version.empty() &&
        esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        cached_version = running_app_info.version;
    }

    if (cached_version.empty()) cached_version = "0.0.0";

    return cached_version.c_str();
}

static void update_time_ui_from_tm(const struct tm *dtinfo)
{
    if (!dtinfo || !carousel_widget) return;
    if (!carousel_widget->slide_panels.size()) return;

    // Format time using GLOBAL buffers (avoid any stack issues)
    strftime(g_time_buf, sizeof(g_time_buf), "%I:%M", dtinfo);
    strftime(g_ampm_buf, sizeof(g_ampm_buf), "%p", dtinfo);
    strftime(g_date_buf, sizeof(g_date_buf), "%a, %e %b", dtinfo);
    snprintf(g_current_time, sizeof(g_current_time), "%s %s", g_time_buf, g_ampm_buf);

    // Pre-format the subtitle BEFORE accessing widgets
    snprintf(g_subtitle_buf, sizeof(g_subtitle_buf), "%s • %s", g_current_time, g_date_buf);

    ESP_LOGD(TAG, "update_time_ui_from_tm: %s (panels=%d)",
             g_subtitle_buf, carousel_widget->slide_panels.size());

    // Update ALL slide panels
    for (size_t i = 0; i < carousel_widget->slide_panels.size(); i++) {
        lv_obj_t *panel = carousel_widget->slide_panels[i];

        // Robust NULL checks
        if (!panel || !lv_obj_is_valid(panel)) {
            ESP_LOGE(TAG, "Panel %d is NULL!", (int)i);
            continue;
        }

        uint32_t child_cnt = lv_obj_get_child_cnt(panel);
        if (child_cnt < 2) {
            ESP_LOGE(TAG, "Panel %d has only %ld children!", (int)i, child_cnt);
            continue;
        }

        lv_obj_t *subtitle_label = lv_obj_get_child(panel, 1);
        if (!subtitle_label || !lv_obj_is_valid(subtitle_label)) {
            ESP_LOGE(TAG, "Panel %d child(1) is NULL!", (int)i);
            continue;
        }

        ESP_LOGD(TAG, "Updating panel %d", (int)i);

        // Use set_text_static since buffer is global and persistent
        lv_label_set_text_static(subtitle_label, g_subtitle_buf);
    }
}

static void ui_ipc_timer_cb(lv_timer_t *timer)
{
    if (!ui_ipc_queue) return;

    ui_ipc_msg_t msg = {};
    while (xQueueReceive(ui_ipc_queue, &msg, 0) == pdTRUE) {
        switch (msg.type) {
            case ui_ipc_type::TIME:
                update_time_ui_from_tm(&msg.time_payload);
                break;
            default:
                break;
        }
    }
}

void ui_ipc_init()
{
    if (!ui_ipc_queue) {
        ui_ipc_queue = xQueueCreate(8, sizeof(ui_ipc_msg_t));
    }

    if (ui_ipc_queue && !ui_ipc_timer) {
        ui_ipc_timer = lv_timer_create(ui_ipc_timer_cb, 30, NULL);
    }
}

bool ui_ipc_post_time(const struct tm &dtinfo)
{
    if (!ui_ipc_queue) return false;

    ui_ipc_msg_t msg = {};
    msg.type = ui_ipc_type::TIME;
    msg.time_payload = dtinfo;
    return xQueueSendToBack(ui_ipc_queue, &msg, 0) == pdPASS;
}

// ============= FILE-BASED WEATHER POLLING =============
// Reads weather JSON files from /spiffs/weather/<city>.json
// Updates carousel panels directly - no event callbacks

// Get weather icon string from OWM icon code (e.g., "01d", "10n")
static const char* get_weather_icon_string(const char* owm_icon)
{
    if (!owm_icon || strlen(owm_icon) < 2) return FA_WEATHER_CLOUD;

    // Extract icon number (first 2 chars)
    char code[3] = {owm_icon[0], owm_icon[1], '\0'};

    if (strcmp(code, "01") == 0) return FA_WEATHER_SUN;       // clear sky
    if (strcmp(code, "02") == 0) return FA_WEATHER_CLOUD_SUN; // few clouds
    if (strcmp(code, "03") == 0) return FA_WEATHER_CLOUD;     // scattered clouds
    if (strcmp(code, "04") == 0) return FA_WEATHER_CLOUD;     // broken clouds
    if (strcmp(code, "09") == 0) return FA_WEATHER_CLOUD_RAIN; // shower rain
    if (strcmp(code, "10") == 0) return FA_WEATHER_CLOUD_RAIN; // rain
    if (strcmp(code, "11") == 0) return FA_WEATHER_CLOUD_BOLT; // thunderstorm
    if (strcmp(code, "13") == 0) return FA_WEATHER_SNOWFLAKES; // snow
    if (strcmp(code, "50") == 0) return FA_WEATHER_DROPLET;    // mist

    return FA_WEATHER_CLOUD;  // default
}

// Get weather icon color based on day/night
static lv_color_t get_weather_icon_color(const char* owm_icon)
{
    if (!owm_icon) return lv_palette_main(LV_PALETTE_BLUE_GREY);

    if (strchr(owm_icon, 'd')) {
        return lv_color_make(241, 235, 156);  // Day: warm yellow
    } else {
        return lv_palette_main(LV_PALETTE_BLUE_GREY);  // Night: blue-grey
    }
}

static void poll_weather_files()
{
    if (!carousel_widget || carousel_widget->slide_panels.empty()) return;
    if (!cfg) return;

    int weather_count = cfg->get_weather_location_count();
    for (int i = 0; i < weather_count && i < (int)carousel_widget->slide_panels.size(); i++) {
        weather_location_t loc = cfg->get_weather_location(i);
        if (!loc.enabled) continue;

        // Build filename: /spiffs/weather/<city_lowercase>.json
        std::string safe_name = loc.city;
        for (char &c : safe_name) {
            if (c == ' ') c = '_';
            else c = tolower(c);
        }
        std::string filepath = "/spiffs/weather/" + safe_name + ".json";

        // Read the JSON file
        FILE *f = fopen(filepath.c_str(), "r");
        if (!f) {
            ESP_LOGD(TAG, "Weather file not found: %s", filepath.c_str());
            continue;
        }

        // Read file contents
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fsize <= 0 || fsize > 2048) {
            fclose(f);
            continue;
        }

        char *json_buf = (char*)malloc(fsize + 1);
        if (!json_buf) {
            fclose(f);
            continue;
        }

        size_t read_size = fread(json_buf, 1, fsize, f);
        fclose(f);
        json_buf[read_size] = '\0';

        // Parse JSON
        cJSON *root = cJSON_Parse(json_buf);
        free(json_buf);

        if (!root) {
            ESP_LOGW(TAG, "Failed to parse weather JSON: %s", filepath.c_str());
            continue;
        }

        // Extract weather data
        cJSON *name_item = cJSON_GetObjectItem(root, "name");
        cJSON *main_obj = cJSON_GetObjectItem(root, "main");
        cJSON *weather_arr = cJSON_GetObjectItem(root, "weather");

        if (!name_item || !main_obj || !weather_arr) {
            cJSON_Delete(root);
            continue;
        }

        const char *city_name = name_item->valuestring;
        float temp = cJSON_GetObjectItem(main_obj, "temp")->valuedouble;
        float temp_high = cJSON_GetObjectItem(main_obj, "temp_max")->valuedouble;
        float temp_low = cJSON_GetObjectItem(main_obj, "temp_min")->valuedouble;
        int humidity = cJSON_GetObjectItem(main_obj, "humidity")->valueint;
        int pressure = cJSON_GetObjectItem(main_obj, "pressure")->valueint;

        cJSON *weather_item = cJSON_GetArrayItem(weather_arr, 0);
        const char *description = weather_item ? cJSON_GetObjectItem(weather_item, "description")->valuestring : "N/A";

        // Update the carousel panel
        lv_obj_t *panel = carousel_widget->slide_panels[i];
        if (!panel || !lv_obj_is_valid(panel)) {
            cJSON_Delete(root);
            continue;
        }

        uint32_t child_cnt = lv_obj_get_child_cnt(panel);
        if (child_cnt < 7) {  // Need 7 children: title, subtitle, value1-4, icon
            cJSON_Delete(root);
            continue;
        }

        // Update labels: children 2-5 are value labels, child 6 is icon
        lv_obj_t *value1 = lv_obj_get_child(panel, 2);
        lv_obj_t *value2 = lv_obj_get_child(panel, 3);
        lv_obj_t *value3 = lv_obj_get_child(panel, 4);
        lv_obj_t *value4 = lv_obj_get_child(panel, 5);
        lv_obj_t *icon_lbl = lv_obj_get_child(panel, 6);

        static char temp_buf[32];
        static char range_buf[128];
        static char pressure_buf[64];

        if (value1 && lv_obj_is_valid(value1)) {
            snprintf(temp_buf, sizeof(temp_buf), "%.1f°C", temp);
            lv_label_set_text(value1, temp_buf);
        }

        if (value2 && lv_obj_is_valid(value2) && description) {
            lv_label_set_text(value2, description);
        }

        if (value3 && lv_obj_is_valid(value3)) {
            snprintf(range_buf, sizeof(range_buf), "H: %.1f° L: %.1f° • Humidity: %d%%",
                     temp_high, temp_low, humidity);
            lv_label_set_text(value3, range_buf);
        }

        if (value4 && lv_obj_is_valid(value4)) {
            snprintf(pressure_buf, sizeof(pressure_buf), "Pressure: %d hPa", pressure);
            lv_label_set_text(value4, pressure_buf);
        }

        // Update weather icon (child 6)
        if (icon_lbl && lv_obj_is_valid(icon_lbl)) {
            cJSON *icon_item = cJSON_GetObjectItem(weather_item, "icon");
            const char *icon_code = icon_item ? icon_item->valuestring : "01d";
            const char *icon_str = get_weather_icon_string(icon_code);
            lv_color_t icon_color = get_weather_icon_color(icon_code);
            lv_label_set_text(icon_lbl, icon_str);
            lv_obj_set_style_text_color(icon_lbl, icon_color, 0);
        }

        ESP_LOGD(TAG, "Updated panel %d from file: %s (%.1f°C)", i, city_name, temp);
        cJSON_Delete(root);
    }
}

static void weather_poll_timer_cb(lv_timer_t *timer)
{
    poll_weather_files();
}

void weather_poll_init()
{
    if (!weather_poll_timer) {
        // Poll weather files every 5 seconds
        weather_poll_timer = lv_timer_create(weather_poll_timer_cb, 5000, NULL);
        ESP_LOGI(TAG, "Weather file polling timer started (5s interval)");
    }
}
// ============= END FILE-BASED WEATHER POLLING =============

// ============= PRINTER FILE-BASED POLLING =============
static lv_timer_t *printer_poll_timer = nullptr;
static int last_online_printer_count = -1;  // Track changes in online printer count

static void poll_printer_files()
{
    ESP_LOGI(TAG, "poll_printer_files() called, carousel_widget=%p", carousel_widget);
    
    if (!carousel_widget) {
        ESP_LOGW(TAG, "Carousel not initialized yet");
        return;  // Carousel not initialized yet
    }

    if (!cfg) {
        ESP_LOGW(TAG, "Settings config not initialized");
        return;
    }

    // Use global cfg pointer (already loaded)
    int printer_count = cfg->get_printer_count();
    ESP_LOGI(TAG, "Found %d configured printer(s)", printer_count);
    
    if (printer_count == 0) {
        return;  // No printers configured
    }

    // Count how many printers are currently online
    time_t now = time(NULL);
    const time_t ONLINE_THRESHOLD = 60;
    int online_count = 0;
    
    for (int i = 0; i < printer_count; i++) {
        printer_config_t printer = cfg->get_printer(i);
        std::string filepath = "/spiffs/printer/" + printer.serial + ".json";
        ESP_LOGI(TAG, "Checking printer %d: %s (file: %s)", i, printer.serial.c_str(), filepath.c_str());
        
        // Use stat to get file metadata instead of opening
        struct stat file_stat;
        if (stat(filepath.c_str(), &file_stat) != 0) {
            ESP_LOGI(TAG, "File not found or inaccessible: %s (errno=%d: %s)", 
                     filepath.c_str(), errno, strerror(errno));
            continue;
        }
        
        // Check file size from stat
        if (file_stat.st_size <= 0) {
            ESP_LOGD(TAG, "File %s is empty (size=%ld), skipping", filepath.c_str(), (long)file_stat.st_size);
            continue;
        }
        
        if (file_stat.st_size > 20480) {
            ESP_LOGW(TAG, "File %s too large: %ld bytes", filepath.c_str(), (long)file_stat.st_size);
            continue;
        }
        
        ESP_LOGI(TAG, "File size: %ld bytes, mtime: %ld", (long)file_stat.st_size, (long)file_stat.st_mtime);
        
        // Now open and read the file
        FILE *f = fopen(filepath.c_str(), "r");
        if (!f) {
            ESP_LOGW(TAG, "Failed to open file: %s", filepath.c_str());
            continue;
        }
        
        char *json_buf = (char*)malloc(file_stat.st_size + 1);
        if (!json_buf) {
            ESP_LOGW(TAG, "Failed to allocate %ld bytes for JSON", (long)file_stat.st_size);
            fclose(f);
            continue;
        }
        
        size_t read_size = fread(json_buf, 1, file_stat.st_size, f);
        fclose(f);
        
        if (read_size != (size_t)file_stat.st_size) {
            ESP_LOGW(TAG, "Read size mismatch: expected %ld, got %zu", (long)file_stat.st_size, read_size);
            free(json_buf);
            continue;
        };
        json_buf[read_size] = '\0';
        
        cJSON *root = cJSON_Parse(json_buf);
        if (!root) {
            ESP_LOGW(TAG, "Failed to parse JSON for %s", printer.serial.c_str());
            free(json_buf);
            continue;
        }
        free(json_buf);
        
        cJSON *last_update = cJSON_GetObjectItem(root, "last_update");
        if (last_update) {
            time_t update_time = (time_t)last_update->valuedouble;
            time_t age = now - update_time;
            ESP_LOGI(TAG, "Printer %s: last_update=%lld, age=%lld seconds, threshold=%lld",
                     printer.serial.c_str(), (long long)update_time, (long long)age, (long long)ONLINE_THRESHOLD);
            if (age < ONLINE_THRESHOLD) {
                ESP_LOGI(TAG, "Printer %s is ONLINE", printer.serial.c_str());
                online_count++;
            } else {
                ESP_LOGW(TAG, "Printer %s is OFFLINE (age=%lld > threshold=%lld)", 
                         printer.serial.c_str(), (long long)age, (long long)ONLINE_THRESHOLD);
            }
        } else {
            ESP_LOGW(TAG, "Printer %s: no last_update field found in JSON", printer.serial.c_str());
        }
        
        cJSON_Delete(root);
    }
    
    // Rebuild carousel if online printer count changed
    if (online_count != last_online_printer_count) {
        ESP_LOGI(TAG, "Printer online status changed: %d -> %d printers online, rebuilding carousel",
                 last_online_printer_count, online_count);
        last_online_printer_count = online_count;
        update_carousel_slides();  // This rebuilds the entire carousel
        return;  // Carousel rebuilt, slides will update on next poll
    }
    
    // Online count unchanged - just update existing printer slide labels
    for (size_t i = 1; i < carousel_widget->slides.size(); i++) {
        carousel_slide_t &slide = carousel_widget->slides[i];
        
        // Check if this is a printer slide
        if (slide.bg_color.full != lv_color_hex(0x3a1e2f).full) {
            continue;  // Not a printer slide
        }

        // Find matching printer config by comparing slide title to printer name
        printer_config_t printer;
        bool found = false;
        for (int j = 0; j < printer_count; j++) {
            printer_config_t p = cfg->get_printer(j);
            if (p.name == slide.title) {
                printer = p;
                found = true;
                break;
            }
        }
        
        if (!found) continue;
        
        std::string filepath = "/spiffs/printer/" + printer.serial + ".json";
        FILE *f = fopen(filepath.c_str(), "r");
        if (!f) {
            ESP_LOGD(TAG, "Printer file not found: %s", filepath.c_str());
            continue;
        }

        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fsize <= 0 || fsize > 20480) {
            fclose(f);
            continue;
        }

        char *json_buf = (char*)malloc(fsize + 1);
        if (!json_buf) {
            fclose(f);
            continue;
        }

        size_t read_size = fread(json_buf, 1, fsize, f);
        fclose(f);
        json_buf[read_size] = '\0';

        cJSON *root = cJSON_Parse(json_buf);
        free(json_buf);

        if (!root) {
            ESP_LOGW(TAG, "Failed to parse printer JSON: %s", filepath.c_str());
            continue;
        }

        // Extract printer data
        cJSON *nozzle_temp = cJSON_GetObjectItem(root, "nozzle_temper");
        cJSON *bed_temp = cJSON_GetObjectItem(root, "bed_temper");
        cJSON *progress = cJSON_GetObjectItem(root, "mc_percent");
        cJSON *gcode_state = cJSON_GetObjectItem(root, "gcode_state");
        cJSON *last_update = cJSON_GetObjectItem(root, "last_update");

        int nozzle = nozzle_temp ? (int)nozzle_temp->valuedouble : 0;
        int bed = bed_temp ? (int)bed_temp->valuedouble : 0;
        int prog = progress ? (int)progress->valuedouble : 0;
        const char *state = (gcode_state && gcode_state->valuestring) ? gcode_state->valuestring : "IDLE";

        // Check if printer is online
        time_t update_time = last_update ? (time_t)last_update->valuedouble : 0;
        bool is_online = (now - update_time) < ONLINE_THRESHOLD;

        // Update carousel slide data
        static char subtitle_buf[64];
        static char value1_buf[32];
        static char value2_buf[64];
        static char value3_buf[64];

        if (is_online) {
            snprintf(subtitle_buf, sizeof(subtitle_buf), "Status: %s", state);
            snprintf(value1_buf, sizeof(value1_buf), "%d%%", prog);
            snprintf(value2_buf, sizeof(value2_buf), "Nozzle: %d°C", nozzle);
            snprintf(value3_buf, sizeof(value3_buf), "Bed: %d°C", bed);
        } else {
            snprintf(subtitle_buf, sizeof(subtitle_buf), "Status: Offline");
            snprintf(value1_buf, sizeof(value1_buf), "--");
            snprintf(value2_buf, sizeof(value2_buf), "Last seen %ld sec ago", (long)(now - update_time));
            value3_buf[0] = '\0';  // Empty string
        }

        slide.subtitle = subtitle_buf;
        slide.value1 = value1_buf;
        slide.value2 = value2_buf;
        slide.value3 = value3_buf;

        // Update the actual UI labels
        carousel_widget->update_slide_labels(i);

        ESP_LOGI(TAG, "Updated printer %s: %s, %d%%, nozzle=%d°C, bed=%d°C",
                 printer.name.c_str(), state, prog, nozzle, bed);

        cJSON_Delete(root);
    }
}

static void printer_poll_timer_cb(lv_timer_t *timer)
{
    poll_printer_files();
}

void printer_poll_init()
{
    if (!printer_poll_timer) {
        // Poll printer files every 5 seconds
        printer_poll_timer = lv_timer_create(printer_poll_timer_cb, 5000, NULL);
        ESP_LOGI(TAG, "Printer file polling timer started (5s interval)");
    }
}
// ============= END PRINTER FILE-BASED POLLING =============

void datetime_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_msg_t * m = lv_event_get_msg(e);
    
    if (code != LV_EVENT_MSG_RECEIVED) return;
    
    struct tm *dtinfo = (struct tm*)lv_msg_get_payload(m);
    if (!dtinfo) return;

    // Push into IPC queue so updates are serialized in LVGL timer
    ui_ipc_post_time(*dtinfo);
}

// weather_event_cb REMOVED - replaced by file-based polling via poll_weather_files()

static void footer_button_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        uint32_t page_id = lv_btnmatrix_get_selected_btn(obj);
        const char * txt = lv_btnmatrix_get_btn_text(obj, page_id);
        ESP_LOGI("FOOTER", "Button %ld pressed: %s", page_id, txt);

        // Do not refresh the page if its not changed
        if (current_page != page_id) current_page = page_id;
        else return;    // Skip if no page change

        // HOME (Button 0)
        if (page_id==0)  {
            lv_obj_clean(content_container);
            create_page_home(content_container);
            anim_move_left_x(content_container,screen_w,0,200);
            lv_msg_send(MSG_PAGE_HOME,NULL);
        }
        // BAMBU MONITOR / PRINTER (Button 1)
        else if (page_id == 1) {
            lv_obj_clean(content_container);
            create_page_bambu(content_container);
            anim_move_left_x(content_container,screen_w,0,200);
            lv_msg_send(MSG_PAGE_BAMBU,NULL);
        }
        // SETTINGS (Button 2)
        else if (page_id == 2) {
            lv_obj_clean(content_container);
            create_page_settings(content_container);
            anim_move_left_x(content_container,screen_w,0,200);
            lv_msg_send(MSG_PAGE_SETTINGS,NULL);
        }
        // OTA UPDATES (Button 3)
        else if (page_id == 3) {
            lv_obj_clean(content_container);
            create_page_updates(content_container);
            anim_move_left_x(content_container,screen_w,0,200);
            lv_msg_send(MSG_PAGE_OTA,NULL);
        }
    }
}

static void status_change_cb(void * s, lv_msg_t *m)
{
    LV_UNUSED(s);
    unsigned int msg_id = lv_msg_get_id(m);
    const char * msg_payload = (const char *)lv_msg_get_payload(m);
    const char * msg_data = (const char *)lv_msg_get_user_data(m);

    switch (msg_id)
    {
        case MSG_WIFI_PROV_MODE:
        {
            ESP_LOGW(TAG,"[%d] MSG_WIFI_PROV_MODE",msg_id);
            // Update QR code for PROV and wifi symbol to red?
            lv_style_set_text_color(&style_wifi, lv_palette_main(LV_PALETTE_GREY));
            lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);

            char qr_data[150] = {0};
            snprintf(qr_data,sizeof(qr_data),"%s",(const char*)lv_msg_get_payload(m));
            lv_qrcode_update(prov_qr, qr_data, strlen(qr_data));
            lv_label_set_text(lbl_scan_status, "Install 'ESP SoftAP Prov' App & Scan");
        }
        break;
        case MSG_WIFI_CONNECTED:
        {
            ESP_LOGW(TAG,"[%d] MSG_WIFI_CONNECTED",msg_id);
            lv_style_set_text_color(&style_wifi, lv_palette_main(LV_PALETTE_BLUE));
            lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);

            if (lv_msg_get_payload(m) != NULL) {
                char ip_data[20]={0};
                // IP address in the payload so display
                snprintf(ip_data,sizeof(ip_data),"%s",(const char*)lv_msg_get_payload(m));
                lv_label_set_text_fmt(lbl_wifi_status, "IP Address: %s",ip_data);
            }
        }
        break;
        case MSG_WIFI_DISCONNECTED:
        {
            ESP_LOGW(TAG,"[%d] MSG_WIFI_DISCONNECTED",msg_id);
            lv_style_set_text_color(&style_wifi, lv_palette_main(LV_PALETTE_GREY));
            lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);
        }
        break;
        case MSG_OTA_STATUS:
        {
            ESP_LOGW(TAG,"[%d] MSG_OTA_STATUS",msg_id);
            // Shows different status during OTA update
            char ota_data[150] = {0};
            snprintf(ota_data,sizeof(ota_data),"%s",(const char*)lv_msg_get_payload(m));
            lv_label_set_text(lbl_update_status, ota_data);
        }
        break;
        case MSG_SDCARD_STATUS:
        {
            bool sd_status = *(bool *)lv_msg_get_payload(m);
            ESP_LOGW(TAG,"[%d] MSG_SDCARD_STATUS %d",msg_id,sd_status);
            if (sd_status) {
                lv_style_set_text_color(&style_storage, lv_palette_main(LV_PALETTE_GREEN));
                lv_obj_clear_flag(icon_storage, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_style_set_text_color(&style_storage, lv_palette_main(LV_PALETTE_RED));
                lv_obj_add_flag(icon_storage, LV_OBJ_FLAG_HIDDEN);
            }
        }
        break;
        case MSG_BATTERY_STATUS:
        {
            int battery_val = *(int *)lv_msg_get_payload(m);
            //ESP_LOGW(TAG,"[%d] MSG_BATTERY_STATUS %d",msg_id,battery_val);
            lv_update_battery(battery_val);
        }
        break;
        case MSG_DEVICE_INFO:
        {
            ESP_LOGW(TAG,"[%d] MSG_DEVICE_INFO",msg_id);
            char devinfo_data[300] = {0};
            snprintf(devinfo_data,sizeof(devinfo_data),"%s",(const char*)lv_msg_get_payload(m));
            lv_label_set_text(lbl_device_info,devinfo_data);
        }
        break;
    }
}

static void lv_update_battery(uint batval)
{
    if (batval < 20)
    {
        lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_RED));
        lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_EMPTY);
    }
    else if (batval < 50)
    {
        lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_RED));
        lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_1);
    }
    else if (batval < 70)
    {
        lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_DEEP_ORANGE));
        lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_2);
    }
    else if (batval < 90)
    {
        lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_GREEN));
        lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_3);
    }
    else
    {
        lv_style_set_text_color(&style_battery, lv_palette_main(LV_PALETTE_GREEN));
        lv_label_set_text(icon_battery, LV_SYMBOL_BATTERY_FULL);
    }
}

/********************** ANIMATIONS *********************/
void anim_move_left_x(lv_obj_t * TargetObject, int start_x, int end_x, int delay)
{
    lv_anim_t property_anim;
    lv_anim_init(&property_anim);
    lv_anim_set_time(&property_anim, 200);
    lv_anim_set_user_data(&property_anim, TargetObject);
    lv_anim_set_custom_exec_cb(&property_anim, tux_anim_callback_set_x);
    lv_anim_set_values(&property_anim, start_x, end_x);
    lv_anim_set_path_cb(&property_anim, lv_anim_path_overshoot);
    lv_anim_set_delay(&property_anim, delay + 0);
    lv_anim_set_playback_time(&property_anim, 0);
    lv_anim_set_playback_delay(&property_anim, 0);
    lv_anim_set_repeat_count(&property_anim, 0);
    lv_anim_set_repeat_delay(&property_anim, 0);
    lv_anim_set_early_apply(&property_anim, true);
    lv_anim_start(&property_anim);
}

void tux_anim_callback_set_x(lv_anim_t * a, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)a->user_data, v);
}

void anim_move_left_y(lv_obj_t * TargetObject, int start_y, int end_y, int delay)
{
    lv_anim_t property_anim;
    lv_anim_init(&property_anim);
    lv_anim_set_time(&property_anim, 300);
    lv_anim_set_user_data(&property_anim, TargetObject);
    lv_anim_set_custom_exec_cb(&property_anim, tux_anim_callback_set_y);
    lv_anim_set_values(&property_anim, start_y, end_y);
    lv_anim_set_path_cb(&property_anim, lv_anim_path_overshoot);
    lv_anim_set_delay(&property_anim, delay + 0);
    lv_anim_set_playback_time(&property_anim, 0);
    lv_anim_set_playback_delay(&property_anim, 0);
    lv_anim_set_repeat_count(&property_anim, 0);
    lv_anim_set_repeat_delay(&property_anim, 0);
    lv_anim_set_early_apply(&property_anim, true);
    lv_anim_start(&property_anim);
}

void tux_anim_callback_set_y(lv_anim_t * a, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)a->user_data, v);
}

void anim_fade_in(lv_obj_t * TargetObject, int delay)
{
    lv_anim_t property_anim;
    lv_anim_init(&property_anim);
    lv_anim_set_time(&property_anim, 3000);
    lv_anim_set_user_data(&property_anim, TargetObject);
    lv_anim_set_custom_exec_cb(&property_anim, tux_anim_callback_set_opacity);
    lv_anim_set_values(&property_anim, 0, 255);
    lv_anim_set_path_cb(&property_anim, lv_anim_path_ease_out);
    lv_anim_set_delay(&property_anim, delay + 0);
    lv_anim_set_playback_time(&property_anim, 0);
    lv_anim_set_playback_delay(&property_anim, 0);
    lv_anim_set_repeat_count(&property_anim, 0);
    lv_anim_set_repeat_delay(&property_anim, 0);
    lv_anim_set_early_apply(&property_anim, false);
    lv_anim_start(&property_anim);

}
void anim_fade_out(lv_obj_t * TargetObject, int delay)
{
    lv_anim_t property_anim;
    lv_anim_init(&property_anim);
    lv_anim_set_time(&property_anim, 1000);
    lv_anim_set_user_data(&property_anim, TargetObject);
    lv_anim_set_custom_exec_cb(&property_anim, tux_anim_callback_set_opacity);
    lv_anim_set_values(&property_anim, 255, 0);
    lv_anim_set_path_cb(&property_anim, lv_anim_path_ease_in_out);
    lv_anim_set_delay(&property_anim, delay + 0);
    lv_anim_set_playback_time(&property_anim, 0);
    lv_anim_set_playback_delay(&property_anim, 0);
    lv_anim_set_repeat_count(&property_anim, 0);
    lv_anim_set_repeat_delay(&property_anim, 0);
    lv_anim_set_early_apply(&property_anim, false);
    lv_anim_start(&property_anim);

}
void tux_anim_callback_set_opacity(lv_anim_t * a, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)a->user_data, v, 0);
}

static void set_weather_icon(string weatherIcon)
{
    /* 
        https://openweathermap.org/weather-conditions#Weather-Condition-Codes-2
        d = day / n = night
        01 - clear sky
        02 - few clouds
        03 - scattered clouds
        04 - broken clouds
        09 - shower rain
        10 - rain
        11 - thunderstorm
        13 - snow
        50 - mist
    */
    // lv_color_make(red, green, blue);

    if (weatherIcon.find('d') != std::string::npos) {
        // set daytime color
        // color = whitesmoke = lv_color_make(245, 245, 245)
        // Ideally it should change for each weather - light blue for rain etc...
        lv_obj_set_style_text_color(lbl_weathericon,lv_color_make(241, 235, 156),0); 
    } else {
        // set night time color
        lv_obj_set_style_text_color(lbl_weathericon,lv_palette_main(LV_PALETTE_BLUE_GREY),0);
    }

    if (weatherIcon.find("50") != std::string::npos) {     // mist - need icon
        lv_label_set_text(lbl_weathericon,FA_WEATHER_DROPLET);
        return;
    }
    
    if (weatherIcon.find("13") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_SNOWFLAKES);
        return;
    }    

    if (weatherIcon.find("11") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD_SHOWERS_HEAVY);
        return;
    }    

    if (weatherIcon.find("10") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD_RAIN);
        return;
    }    

    if (weatherIcon.find("09") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD_RAIN);
        return;
    }    

    if (weatherIcon.find("04") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD);
        return;
    }   

    if (weatherIcon.find("03") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD);
        return;
    }

    if (weatherIcon.find("02") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD);
        return;
    }

    if (weatherIcon.find("01") != std::string::npos) {     
        lv_label_set_text(lbl_weathericon,FA_WEATHER_SUN);
        return;
    }


    // default
    lv_label_set_text(lbl_weathericon,FA_WEATHER_CLOUD_SHOWERS_HEAVY);
    lv_obj_set_style_text_color(lbl_weathericon,lv_palette_main(LV_PALETTE_BLUE_GREY),0); 
}

// Bambu Monitor event callbacks
static void bambu_status_cb(void * s, lv_msg_t * m)
{
    lv_obj_t * lbl_status = (lv_obj_t *)s;
    if (!lv_obj_is_valid(lbl_status)) return;
    
    const char * status = (const char *)lv_msg_get_payload(m);
    if (status) {
        lv_label_set_text_fmt(lbl_status, "Status: %s", status);
    }
}

static void bambu_progress_cb(void * s, lv_msg_t * m)
{
    lv_obj_t * lbl_progress = (lv_obj_t *)s;
    if (!lv_obj_is_valid(lbl_progress)) return;
    
    uint8_t * progress = (uint8_t *)lv_msg_get_payload(m);
    if (progress) {
        lv_label_set_text_fmt(lbl_progress, "Progress: %d%%", *progress);
    }
}

static void bambu_temps_cb(void * s, lv_msg_t * m)
{
    lv_obj_t * lbl_temps = (lv_obj_t *)s;
    if (!lv_obj_is_valid(lbl_temps)) return;
    
    const char * temps = (const char *)lv_msg_get_payload(m);
    if (temps) {
        lv_label_set_text_fmt(lbl_temps, "%s", temps);
    }
}