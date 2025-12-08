/*
 * Carousel Widget for displaying weather locations and printer status
 * Supports horizontal scrolling through multiple cards/slides
 */

#ifndef CAROUSEL_WIDGET_HPP
#define CAROUSEL_WIDGET_HPP

#include "lvgl/lvgl.h"
#include <vector>
#include <string>
#include "esp_log.h"
#include "i18n/lang.hpp"  // Internationalization support

// Font declarations
LV_FONT_DECLARE(font_fa_weather_42)
LV_FONT_DECLARE(font_fa_printer_42)
LV_FONT_DECLARE(font_fa_printer_24)

// Printer icon definitions (UTF-8 encoded FontAwesome)
#define ICON_PRINTER_PLAY         "\xEF\x81\x8B"      // f04b - Play (Printing)
#define ICON_PRINTER_PAUSE        "\xEF\x81\x8C"      // f04c - Pause (Paused)
#define ICON_PRINTER_STOP         "\xEF\x81\x8D"      // f04d - Stop (Idle/Stopped)
#define ICON_PRINTER_CHECK        "\xEF\x80\x8C"      // f00c - Check (Complete/Ready)
#define ICON_PRINTER_WARNING      "\xEF\x81\xB1"      // f071 - Warning Triangle (Error)
#define ICON_PRINTER_THERMOMETER  "\xEF\x8B\x89"      // f2c9 - Thermometer Half
#define ICON_PRINTER_FIRE         "\xEF\x81\xAD"      // f06d - Fire (Heating/Bed)

// Slide types
enum carousel_slide_type_t {
    SLIDE_TYPE_WEATHER = 0,
    SLIDE_TYPE_PRINTER = 1,
    SLIDE_TYPE_OTHER = 2
};

// Slide data structure
struct carousel_slide_t {
    std::string title;           // Location name or printer name
    std::string subtitle;        // Time, city, country or status
    std::string value1;          // Temperature or progress
    std::string value2;          // Weather/status description
    std::string value3;          // Additional info (temp range, humidity, wind)
    std::string value4;          // Extra info line
    std::string snapshot_path;   // Path to camera snapshot image (LVGL format: S:/path or F:/path)
    lv_color_t bg_color;         // Background color
    uint32_t icon_code;          // Font icon code (if used)
    carousel_slide_type_t type;  // Slide type for icon font selection
    int printer_index;           // BambuMonitor printer index (for snapshot lookup)
    
    carousel_slide_t() : bg_color(lv_color_hex(0x2a2a2a)), icon_code(0), type(SLIDE_TYPE_OTHER), printer_index(-1) {}
};

// Carousel callback types
typedef void (*carousel_slide_changed_t)(int current_slide);

class CarouselWidget {
public:
    lv_obj_t *container;
    std::vector<carousel_slide_t> slides;
    int current_slide;
    carousel_slide_changed_t on_slide_changed;
    int width;   // Saved dimensions
    int height;  // Saved dimensions
    
    // Slide objects
    std::vector<lv_obj_t*> slide_panels;
    std::vector<lv_obj_t*> slide_labels;
    lv_obj_t *page_indicator;  // Shows current page (dots)
    lv_obj_t *scroll_container;
    
    CarouselWidget(lv_obj_t *parent, int width, int height)
        : container(nullptr), current_slide(0), on_slide_changed(nullptr),
          page_indicator(nullptr), scroll_container(nullptr)
    {
        create_carousel(parent, width, height);
    }
    
    ~CarouselWidget() {
        slide_panels.clear();
        slide_labels.clear();
        slides.clear();
    }
    
    void create_carousel(lv_obj_t *parent, int width, int height);
    void add_slide(const carousel_slide_t &slide);
    void update_slides();
    void update_slide_labels(int index);
    void show_slide(int index);
    void next_slide();
    void prev_slide();
    int get_current_slide() { return current_slide; }
    int get_slide_count() { return slides.size(); }
    
private:
    void create_page_indicator();
    void update_page_indicator();
    static void scroll_event_cb(lv_event_t *e);
};

/*
 * Implementation
 */

void CarouselWidget::create_carousel(lv_obj_t *parent, int width, int height)
{
    ESP_LOGW("CarouselWidget", "Creating carousel: width=%d, height=%d", width, height);
    
    // Main carousel container
    container = lv_obj_create(parent);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(container, width, height);
    // Don't set position here - it will be set after creation in gui.hpp
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);  // Ensure container is visible
    
    ESP_LOGW("CarouselWidget", "Container created at position: x=%d, y=%d, size: %dx%d", 
             lv_obj_get_x(container), lv_obj_get_y(container),
             lv_obj_get_width(container), lv_obj_get_height(container));
    
    // Scrollable container for slides
    scroll_container = lv_obj_create(container);
    lv_obj_set_size(scroll_container, width, height - 50);  // Use passed width, not queried size
    lv_obj_set_pos(scroll_container, 0, 0);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_HOR);
    lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0x1e1e1e), 0);  // Match container bg
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_radius(scroll_container, 0, 0);  // No rounded corners
    lv_obj_set_style_pad_all(scroll_container, 0, 0);
    lv_obj_set_style_pad_gap(scroll_container, 0, 0);  // No gap between slides
    lv_obj_set_style_pad_row(scroll_container, 0, 0);  // No row gap
    lv_obj_set_style_pad_column(scroll_container, 0, 0);  // No column gap
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroll_container, LV_SCROLL_SNAP_START);  // Snap to start for clean alignment
    lv_obj_set_style_bg_opa(scroll_container, LV_OPA_COVER, 0);  // Ensure scroll container is visible
    lv_obj_clear_flag(scroll_container, LV_OBJ_FLAG_SCROLL_ELASTIC);  // Disable elastic scroll
    
    // Save dimensions for later use
    this->width = width;
    this->height = height;
    
    ESP_LOGW("CarouselWidget", "Scroll container created, size: %dx%d (using passed dimensions)", width, height - 50);
    
    // Set flex layout for horizontal scrolling
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Create page indicator at bottom
    page_indicator = lv_obj_create(container);
    lv_obj_set_size(page_indicator, width, 40);
    lv_obj_set_pos(page_indicator, 0, height - 40);
    lv_obj_set_style_bg_color(page_indicator, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(page_indicator, 0, 0);
    lv_obj_set_flex_flow(page_indicator, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(page_indicator, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(page_indicator, 0, 0);
    
    // Add scroll event listener
    lv_obj_add_event_cb(scroll_container, scroll_event_cb, LV_EVENT_SCROLL, (void*)this);
}

void CarouselWidget::add_slide(const carousel_slide_t &slide)
{
    if (slides.size() >= 10) {  // Limit to 10 slides
        return;
    }
    
    slides.push_back(slide);
    ESP_LOGW("CarouselWidget", "Adding slide %d: %s", slides.size(), slide.title.c_str());
    
    // Create slide panel
    lv_obj_t *slide_panel = lv_obj_create(scroll_container);
    int panel_width = lv_obj_get_width(scroll_container);
    int panel_height = lv_obj_get_height(scroll_container);
    lv_obj_set_size(slide_panel, panel_width, panel_height);
    lv_obj_set_style_bg_color(slide_panel, slide.bg_color, 0);
    lv_obj_set_style_border_width(slide_panel, 0, 0);
    lv_obj_set_style_pad_all(slide_panel, 20, 0);
    lv_obj_set_flex_grow(slide_panel, 0);  // Don't grow, keep exact size
    
    // Title
    lv_obj_t *title = lv_label_create(slide_panel);
    lv_label_set_text(title, slide.title.c_str());
    lv_obj_set_style_text_font(title, &font_montserrat_int_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 20, 20);
    
    // Subtitle
    lv_obj_t *subtitle = lv_label_create(slide_panel);
    lv_label_set_text(subtitle, slide.subtitle.c_str());
    lv_obj_set_style_text_font(subtitle, &font_montserrat_int_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(subtitle, 20, 50);
    
    // Main value (temperature, progress, etc.)
    lv_obj_t *value1 = lv_label_create(slide_panel);
    lv_label_set_text(value1, slide.value1.c_str());
    lv_obj_set_style_text_font(value1, &font_montserrat_int_32, 0);
    lv_obj_set_style_text_color(value1, lv_color_hex(0xffa500), 0);
    lv_obj_align(value1, LV_ALIGN_CENTER, 0, -30);
    
    // Secondary value (description)
    lv_obj_t *value2 = lv_label_create(slide_panel);
    lv_label_set_text(value2, slide.value2.c_str());
    lv_obj_set_style_text_font(value2, &font_montserrat_int_16, 0);
    lv_obj_set_style_text_color(value2, lv_color_hex(0xcccccc), 0);
    lv_obj_align(value2, LV_ALIGN_CENTER, 0, 40);
    
    slide_panels.push_back(slide_panel);
    slide_labels.push_back(title);
    
    update_page_indicator();
}

void CarouselWidget::update_slides()
{
    ESP_LOGW("CarouselWidget", "update_slides() called, slides.size=%d, slide_panels.size=%d", slides.size(), slide_panels.size());
    
    // Clear existing visual panels
    for (auto panel : slide_panels) {
        lv_obj_del(panel);
    }
    slide_panels.clear();
    slide_labels.clear();
    
    // Recreate visual panels from slides data
    for (size_t i = 0; i < slides.size(); i++) {
        const auto &slide = slides[i];
        ESP_LOGW("CarouselWidget", "Creating panel %d for: %s (type=%d)", i, slide.title.c_str(), slide.type);
        
        // Create slide panel - use saved dimensions, identical for all slide types
        lv_obj_t *slide_panel = lv_obj_create(scroll_container);
        lv_obj_set_size(slide_panel, width, height - 50);  // Use saved dimensions
        lv_obj_set_style_bg_color(slide_panel, slide.bg_color, 0);
        lv_obj_set_style_border_width(slide_panel, 0, 0);
        lv_obj_set_style_radius(slide_panel, 0, 0);  // No rounded corners
        lv_obj_set_style_pad_all(slide_panel, 0, 0);  // No padding - position content explicitly
        lv_obj_set_flex_grow(slide_panel, 0);  // Don't grow, keep exact size
        lv_obj_clear_flag(slide_panel, LV_OBJ_FLAG_SCROLLABLE);  // Slide panels don't scroll
        
        if (slide.type == SLIDE_TYPE_PRINTER) {
            // ============ PRINTER SLIDE LAYOUT ============
            // Child order: 0=title, 1=subtitle, 2=value1(progress), 3=nozzle_icon, 4=value2(nozzle temp),
            //              5=bed_icon, 6=value3(bed+layer), 7=value4(file), 8=status_icon(top-right)
            
            // Child 0: Title (Printer name)
            lv_obj_t *title = lv_label_create(slide_panel);
            lv_label_set_text(title, slide.title.c_str());
            lv_obj_set_style_text_font(title, &font_montserrat_int_24, 0);
            lv_obj_set_style_text_color(title, lv_color_white(), 0);
            lv_obj_set_pos(title, 10, 5);
            
            // Child 1: Subtitle (State + time remaining)
            lv_obj_t *subtitle = lv_label_create(slide_panel);
            lv_label_set_text(subtitle, slide.subtitle.c_str());
            lv_obj_set_style_text_font(subtitle, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
            lv_obj_set_pos(subtitle, 10, 38);
            
            // Child 2: Progress (large)
            lv_obj_t *value1 = lv_label_create(slide_panel);
            lv_label_set_text(value1, slide.value1.c_str());
            lv_obj_set_style_text_font(value1, &font_montserrat_int_32, 0);
            lv_obj_set_style_text_color(value1, lv_color_hex(0x00cc00), 0);  // Green for progress
            lv_obj_set_pos(value1, 10, 62);
            
            // Child 3: Nozzle icon (tint/droplet - represents melted filament)
            lv_obj_t *nozzle_icon = lv_label_create(slide_panel);
            lv_obj_set_style_text_font(nozzle_icon, &font_fa_printer_42, 0);
            lv_obj_set_style_text_color(nozzle_icon, lv_color_hex(0xff6600), 0);  // Orange
            lv_label_set_text(nozzle_icon, "\xEF\x81\x83");  // f043 tint (droplet)
            lv_obj_set_pos(nozzle_icon, 10, 105);
            
            // Child 4: Nozzle temperature text (value2)
            lv_obj_t *value2 = lv_label_create(slide_panel);
            lv_label_set_text(value2, slide.value2.c_str());
            lv_obj_set_style_text_font(value2, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value2, lv_color_hex(0xcccccc), 0);
            lv_obj_set_pos(value2, 55, 115);
            
            // Child 5: Bed icon (thermometer - represents heated bed)
            lv_obj_t *bed_icon = lv_label_create(slide_panel);
            lv_obj_set_style_text_font(bed_icon, &font_fa_printer_42, 0);
            lv_obj_set_style_text_color(bed_icon, lv_color_hex(0xff3300), 0);  // Red-orange
            lv_label_set_text(bed_icon, "\xEF\x8B\x89");  // f2c9 thermometer
            lv_obj_set_pos(bed_icon, 200, 105);
            
            // Child 6: Bed temp + Layer progress (value3)
            lv_obj_t *value3 = lv_label_create(slide_panel);
            lv_label_set_text(value3, slide.value3.empty() ? "" : slide.value3.c_str());
            lv_obj_set_style_text_font(value3, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value3, lv_color_hex(0x88ccff), 0);
            lv_obj_set_pos(value3, 10, 155);
            
            // Child 7: File name (value4)
            lv_obj_t *value4 = lv_label_create(slide_panel);
            lv_label_set_text(value4, slide.value4.empty() ? "" : slide.value4.c_str());
            lv_obj_set_style_text_font(value4, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value4, lv_color_hex(0x888888), 0);
            lv_obj_set_pos(value4, 10, 180);
            
            // Child 8: Main status icon (right side, same position as weather icon)
            lv_obj_t *status_icon = lv_label_create(slide_panel);
            lv_obj_set_style_text_font(status_icon, &font_fa_printer_42, 0);
            lv_obj_set_style_text_color(status_icon, lv_color_hex(0x888888), 0);  // Default gray
            lv_label_set_text(status_icon, "\xEF\x80\x91");  // f011 power-off (idle)
            lv_obj_set_pos(status_icon, width - 100, 60);  // Same position as weather icon
            
            // Child 9: Camera snapshot container (bottom-right corner)
            lv_obj_t *snapshot_img = lv_obj_create(slide_panel);
            lv_obj_set_size(snapshot_img, 200, 150);  // 4:3 aspect ratio
            lv_obj_set_pos(snapshot_img, width - 210, height - 190);  // Bottom right with margin
            lv_obj_set_style_bg_color(snapshot_img, lv_color_hex(0x303030), 0);
            lv_obj_set_style_bg_opa(snapshot_img, LV_OPA_100, 0);
            lv_obj_set_style_radius(snapshot_img, 4, 0);
            lv_obj_set_style_border_width(snapshot_img, 1, 0);
            lv_obj_set_style_border_color(snapshot_img, lv_color_hex(0x505050), 0);
            lv_obj_set_style_pad_all(snapshot_img, 0, 0);
            lv_obj_clear_flag(snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
            
            // "No Data" label inside container
            lv_obj_t *no_data_lbl = lv_label_create(snapshot_img);
            lv_label_set_text(no_data_lbl, "No Camera");
            lv_obj_set_style_text_font(no_data_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(no_data_lbl, lv_color_hex(0x808080), 0);
            lv_obj_center(no_data_lbl);
            
            // Actual image (child of container, initially hidden)
            lv_obj_t *snapshot_actual = lv_img_create(snapshot_img);
            lv_obj_set_size(snapshot_actual, 200, 150);
            lv_obj_set_pos(snapshot_actual, 0, 0);
            lv_obj_add_flag(snapshot_actual, LV_OBJ_FLAG_HIDDEN);
            // Image source set later via update_slide_labels() when snapshot is available
            
        } else {
            // ============ WEATHER/DEFAULT SLIDE LAYOUT ============
            // Title (Location)
            lv_obj_t *title = lv_label_create(slide_panel);
            lv_label_set_text(title, slide.title.c_str());
            lv_obj_set_style_text_font(title, &font_montserrat_int_24, 0);
            lv_obj_set_style_text_color(title, lv_color_white(), 0);
            lv_obj_set_pos(title, 10, 10);
            
            // Subtitle (Time, Date, Location details)
            lv_obj_t *subtitle = lv_label_create(slide_panel);
            lv_label_set_text(subtitle, slide.subtitle.c_str());
            lv_obj_set_style_text_font(subtitle, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
            lv_obj_set_pos(subtitle, 10, 45);
            
            // Main value (Current temperature)
            lv_obj_t *value1 = lv_label_create(slide_panel);
            lv_label_set_text(value1, slide.value1.c_str());
            lv_obj_set_style_text_font(value1, &font_montserrat_int_32, 0);
            lv_obj_set_style_text_color(value1, lv_color_hex(0xffa500), 0);
            lv_obj_set_pos(value1, 10, 70);
            
            // Secondary value (Weather description)
            lv_obj_t *value2 = lv_label_create(slide_panel);
            lv_label_set_text(value2, slide.value2.c_str());
            lv_obj_set_style_text_font(value2, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value2, lv_color_hex(0xcccccc), 0);
            lv_obj_set_pos(value2, 10, 115);
            
            // Value 3 (Temp range, humidity)
            lv_obj_t *value3 = lv_label_create(slide_panel);
            lv_label_set_text(value3, slide.value3.empty() ? "" : slide.value3.c_str());
            lv_obj_set_style_text_font(value3, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value3, lv_color_hex(0x88ccff), 0);
            lv_obj_set_pos(value3, 10, 145);
            
            // Value 4 (Wind, pressure)
            lv_obj_t *value4 = lv_label_create(slide_panel);
            lv_label_set_text(value4, slide.value4.empty() ? "" : slide.value4.c_str());
            lv_obj_set_style_text_font(value4, &font_montserrat_int_16, 0);
            lv_obj_set_style_text_color(value4, lv_color_hex(0x88ccff), 0);
            lv_obj_set_pos(value4, 10, 175);
            
            // Weather icon (child 6) - positioned on the right side
            lv_obj_t *icon = lv_label_create(slide_panel);
            lv_label_set_text(icon, LV_SYMBOL_IMAGE);  // Placeholder until weather updates
            lv_obj_set_style_text_font(icon, &font_fa_weather_42, 0);
            lv_obj_set_style_text_color(icon, lv_color_make(241, 235, 156), 0);  // Warm yellow
            lv_obj_set_pos(icon, width - 100, 60);  // Position on right side
        }
        
        slide_panels.push_back(slide_panel);
        slide_labels.push_back(lv_obj_get_child(slide_panel, 0));  // title
    }
    
    current_slide = 0;
    update_page_indicator();
    
    ESP_LOGW("CarouselWidget", "update_slides() complete, %d panels created", slide_panels.size());
}

void CarouselWidget::update_slide_labels(int index)
{
    // Update the text labels of a specific slide with current data from slides vector
    if (index < 0 || index >= (int)slides.size() || index >= (int)slide_panels.size()) {
        return;
    }
    
    lv_obj_t *panel = slide_panels[index];
    if (!panel) return;
    
    const auto &slide = slides[index];
    uint32_t child_count = lv_obj_get_child_cnt(panel);
    
    if (slide.type == SLIDE_TYPE_PRINTER) {
        // Printer layout child indices (9 children):
        // 0=title, 1=subtitle, 2=value1(progress), 3=nozzle_icon, 4=value2(nozzle temp),
        // 5=bed_icon, 6=value3(bed+layer), 7=value4(file), 8=status_icon(top-right)
        
        if (child_count >= 1) {
            lv_obj_t *title = lv_obj_get_child(panel, 0);
            lv_label_set_text(title, slide.title.c_str());
        }
        if (child_count >= 2) {
            lv_obj_t *subtitle = lv_obj_get_child(panel, 1);
            lv_label_set_text(subtitle, slide.subtitle.c_str());
        }
        if (child_count >= 3) {
            lv_obj_t *value1 = lv_obj_get_child(panel, 2);
            lv_label_set_text(value1, slide.value1.c_str());
        }
        // Child 3 is nozzle_icon - no text update needed
        if (child_count >= 5) {
            lv_obj_t *value2 = lv_obj_get_child(panel, 4);
            lv_label_set_text(value2, slide.value2.c_str());
        }
        // Child 5 is bed_icon - no text update needed
        if (child_count >= 7) {
            lv_obj_t *value3 = lv_obj_get_child(panel, 6);
            lv_label_set_text(value3, slide.value3.c_str());
        }
        if (child_count >= 8) {
            lv_obj_t *value4 = lv_obj_get_child(panel, 7);
            lv_label_set_text(value4, slide.value4.c_str());
        }
        // Child 8 is status_icon - update based on state in subtitle
        if (child_count >= 9) {
            lv_obj_t *status_icon = lv_obj_get_child(panel, 8);
            // Set icon and color based on state in subtitle (check all language translations)
            bool is_running = (slide.subtitle.find(TR(STR_RUNNING)) != std::string::npos ||
                              slide.subtitle.find(TR(STR_PRINTING)) != std::string::npos ||
                              slide.subtitle.find("RUNNING") != std::string::npos ||
                              slide.subtitle.find("PRINTING") != std::string::npos);
            bool is_paused = (slide.subtitle.find(TR(STR_PAUSED)) != std::string::npos ||
                             slide.subtitle.find("PAUSE") != std::string::npos);
            bool is_error = (slide.subtitle.find(TR(STR_ERROR)) != std::string::npos ||
                            slide.subtitle.find(TR(STR_FAILED)) != std::string::npos ||
                            slide.subtitle.find("ERROR") != std::string::npos ||
                            slide.subtitle.find("FAILED") != std::string::npos);
            bool is_finished = (slide.subtitle.find(TR(STR_FINISHED)) != std::string::npos ||
                               slide.subtitle.find("FINISH") != std::string::npos);
            
            if (is_running) {
                lv_label_set_text(status_icon, "\xEF\x80\x93");  // f013 cog (working)
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0x00cc00), 0);  // Green
            } else if (is_paused) {
                lv_label_set_text(status_icon, "\xEF\x81\x8C");  // f04c pause
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0xffaa00), 0);  // Amber
            } else if (is_error) {
                lv_label_set_text(status_icon, "\xEF\x81\xB1");  // f071 warning
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0xff3333), 0);  // Red
            } else if (is_finished) {
                lv_label_set_text(status_icon, "\xEF\x80\x8C");  // f00c check
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0x00aaff), 0);  // Blue
            } else {
                lv_label_set_text(status_icon, "\xEF\x80\x91");  // f011 power-off (idle)
                lv_obj_set_style_text_color(status_icon, lv_color_hex(0x888888), 0);  // Gray
            }
        }
        // Child 9 is snapshot container - update if path is available
        if (child_count >= 10) {
            lv_obj_t *snapshot_container = lv_obj_get_child(panel, 9);
            if (snapshot_container) {
                // Container has: child 0 = "No Camera" label, child 1 = actual image
                lv_obj_t *no_data_lbl = lv_obj_get_child(snapshot_container, 0);
                lv_obj_t *snapshot_actual = lv_obj_get_child(snapshot_container, 1);
                
                if (!slide.snapshot_path.empty() && snapshot_actual) {
                    lv_img_set_src(snapshot_actual, slide.snapshot_path.c_str());
                    lv_obj_clear_flag(snapshot_actual, LV_OBJ_FLAG_HIDDEN);
                    if (no_data_lbl) lv_obj_add_flag(no_data_lbl, LV_OBJ_FLAG_HIDDEN);
                    ESP_LOGD("CarouselWidget", "Set snapshot image: %s", slide.snapshot_path.c_str());
                } else {
                    // No snapshot - show "No Camera" label
                    if (snapshot_actual) lv_obj_add_flag(snapshot_actual, LV_OBJ_FLAG_HIDDEN);
                    if (no_data_lbl) lv_obj_clear_flag(no_data_lbl, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    } else {
        // Weather/default layout child indices (7 children):
        // 0=title, 1=subtitle, 2=value1, 3=value2, 4=value3, 5=value4, 6=icon
        if (child_count >= 1) {
            lv_obj_t *title = lv_obj_get_child(panel, 0);
            lv_label_set_text(title, slide.title.c_str());
        }
        if (child_count >= 2) {
            lv_obj_t *subtitle = lv_obj_get_child(panel, 1);
            lv_label_set_text(subtitle, slide.subtitle.c_str());
        }
        if (child_count >= 3) {
            lv_obj_t *value1 = lv_obj_get_child(panel, 2);
            lv_label_set_text(value1, slide.value1.c_str());
        }
        if (child_count >= 4) {
            lv_obj_t *value2 = lv_obj_get_child(panel, 3);
            lv_label_set_text(value2, slide.value2.c_str());
        }
        if (child_count >= 5) {
            lv_obj_t *value3 = lv_obj_get_child(panel, 4);
            lv_label_set_text(value3, slide.value3.c_str());
        }
        if (child_count >= 6) {
            lv_obj_t *value4 = lv_obj_get_child(panel, 5);
            lv_label_set_text(value4, slide.value4.c_str());
        }
    }
}

void CarouselWidget::show_slide(int index)
{
    if (index < 0 || index >= (int)slides.size()) {
        return;
    }
    
    if (!scroll_container || !lv_obj_is_valid(scroll_container)) {
        return;  // Safety check: scroll container must be valid
    }
    
    current_slide = index;
    
    // Calculate scroll position
    int scroll_x = index * lv_obj_get_width(scroll_container);
    lv_obj_scroll_to_x(scroll_container, scroll_x, LV_ANIM_ON);
    
    update_page_indicator();
    
    if (on_slide_changed) {
        on_slide_changed(current_slide);
    }
}

void CarouselWidget::next_slide()
{
    if (slides.size() == 0) return;  // No slides to navigate
    int next_index = (current_slide + 1) % slides.size();
    show_slide(next_index);
}

void CarouselWidget::prev_slide()
{
    if (slides.size() == 0) return;  // No slides to navigate
    int prev_index = (current_slide - 1 + (int)slides.size()) % slides.size();
    show_slide(prev_index);
}

void CarouselWidget::create_page_indicator()
{
    // Clear existing indicators
    lv_obj_clean(page_indicator);
    
    int dot_count = slides.size();
    if (dot_count == 0) return;
    
    // Calculate spacing
    int total_width = (dot_count * 12) + ((dot_count - 1) * 8);
    int start_x = (lv_obj_get_width(page_indicator) - total_width) / 2;
    
    // Create dots for each slide
    for (int i = 0; i < dot_count; i++) {
        lv_obj_t *dot = lv_obj_create(page_indicator);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_pos(dot, start_x + i * 20, 14);
        lv_obj_set_style_radius(dot, 6, 0);  // Make it circular
        lv_obj_set_style_border_width(dot, 0, 0);
        
        if (i == current_slide) {
            lv_obj_set_style_bg_color(dot, lv_color_hex(0xffa500), 0);  // Orange
        } else {
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x666666), 0);  // Gray
        }
    }
}

void CarouselWidget::update_page_indicator()
{
    create_page_indicator();
}

void CarouselWidget::scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    CarouselWidget *carousel = (CarouselWidget *)lv_event_get_user_data(e);
    
    if (!carousel) return;
    
    // Calculate which slide is visible
    int scroll_x = lv_obj_get_scroll_x(obj);
    int slide_width = lv_obj_get_width(obj);
    int new_slide = (scroll_x + slide_width / 2) / slide_width;
    
    if (new_slide != carousel->current_slide && new_slide < (int)carousel->slides.size()) {
        carousel->current_slide = new_slide;
        carousel->update_page_indicator();
        
        if (carousel->on_slide_changed) {
            carousel->on_slide_changed(new_slide);
        }
    }
}

#endif // CAROUSEL_WIDGET_HPP
