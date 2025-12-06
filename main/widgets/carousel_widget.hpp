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

// Weather icon font declaration
LV_FONT_DECLARE(font_fa_weather_42)

// Slide data structure
struct carousel_slide_t {
    std::string title;           // Location name or printer name
    std::string subtitle;        // Time, city, country or status
    std::string value1;          // Temperature or progress
    std::string value2;          // Weather/status description
    std::string value3;          // Additional info (temp range, humidity, wind)
    std::string value4;          // Extra info line
    lv_color_t bg_color;         // Background color
    uint32_t icon_code;          // Font icon code (if used)
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
    lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0x2e2e2e), 0);  // Slightly lighter for debugging
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 0, 0);
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(scroll_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_style_bg_opa(scroll_container, LV_OPA_COVER, 0);  // Ensure scroll container is visible
    
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 20, 20);
    
    // Subtitle
    lv_obj_t *subtitle = lv_label_create(slide_panel);
    lv_label_set_text(subtitle, slide.subtitle.c_str());
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(subtitle, 20, 50);
    
    // Main value (temperature, progress, etc.)
    lv_obj_t *value1 = lv_label_create(slide_panel);
    lv_label_set_text(value1, slide.value1.c_str());
    lv_obj_set_style_text_font(value1, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(value1, lv_color_hex(0xffa500), 0);
    lv_obj_align(value1, LV_ALIGN_CENTER, 0, -30);
    
    // Secondary value (description)
    lv_obj_t *value2 = lv_label_create(slide_panel);
    lv_label_set_text(value2, slide.value2.c_str());
    lv_obj_set_style_text_font(value2, &lv_font_montserrat_16, 0);
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
        ESP_LOGW("CarouselWidget", "Creating panel %d for: %s", i, slide.title.c_str());
        
        // Create slide panel - use saved dimensions
        lv_obj_t *slide_panel = lv_obj_create(scroll_container);
        lv_obj_set_size(slide_panel, width, height - 50);  // Use saved dimensions
        lv_obj_set_style_bg_color(slide_panel, slide.bg_color, 0);
        lv_obj_set_style_border_width(slide_panel, 0, 0);
        lv_obj_set_style_pad_all(slide_panel, 20, 0);
        lv_obj_set_flex_grow(slide_panel, 0);  // Don't grow, keep exact size
        
        // Title (Location)
        lv_obj_t *title = lv_label_create(slide_panel);
        lv_label_set_text(title, slide.title.c_str());
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_pos(title, 10, 15);
        
        // Subtitle (Time, Date, Location details)
        lv_obj_t *subtitle = lv_label_create(slide_panel);
        lv_label_set_text(subtitle, slide.subtitle.c_str());
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_pos(subtitle, 10, 45);
        
        // Main value (Current temperature)
        lv_obj_t *value1 = lv_label_create(slide_panel);
        lv_label_set_text(value1, slide.value1.c_str());
        lv_obj_set_style_text_font(value1, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(value1, lv_color_hex(0xffa500), 0);
        lv_obj_set_pos(value1, 10, 70);
        
        // Secondary value (Weather description)
        lv_obj_t *value2 = lv_label_create(slide_panel);
        lv_label_set_text(value2, slide.value2.c_str());
        lv_obj_set_style_text_font(value2, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(value2, lv_color_hex(0xcccccc), 0);
        lv_obj_set_pos(value2, 10, 115);
        
        // Value 3 (Temp range, humidity) - always created for consistent child index
        lv_obj_t *value3 = lv_label_create(slide_panel);
        lv_label_set_text(value3, slide.value3.empty() ? "" : slide.value3.c_str());
        lv_obj_set_style_text_font(value3, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(value3, lv_color_hex(0x88ccff), 0);
        lv_obj_set_pos(value3, 10, 140);
        
        // Value 4 (Wind, pressure) - always created for consistent child index
        lv_obj_t *value4 = lv_label_create(slide_panel);
        lv_label_set_text(value4, slide.value4.empty() ? "" : slide.value4.c_str());
        lv_obj_set_style_text_font(value4, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(value4, lv_color_hex(0x88ccff), 0);
        lv_obj_set_pos(value4, 10, 165);
        
        // Weather icon (child 6) - positioned on the right side
        lv_obj_t *icon = lv_label_create(slide_panel);
        lv_label_set_text(icon, LV_SYMBOL_IMAGE);  // Placeholder until weather updates
        lv_obj_set_style_text_font(icon, &font_fa_weather_42, 0);
        lv_obj_set_style_text_color(icon, lv_color_make(241, 235, 156), 0);  // Warm yellow
        lv_obj_set_pos(icon, width - 100, 60);  // Position on right side
        
        slide_panels.push_back(slide_panel);
        slide_labels.push_back(title);
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
    
    // Get all children of the panel
    // Child 0: title, Child 1: subtitle, Child 2: value1, Child 3: value2
    uint32_t child_count = lv_obj_get_child_cnt(panel);
    
    if (child_count >= 1) {
        lv_obj_t *title = lv_obj_get_child(panel, 0);
        lv_label_set_text(title, slides[index].title.c_str());
    }
    
    if (child_count >= 2) {
        lv_obj_t *subtitle = lv_obj_get_child(panel, 1);
        lv_label_set_text(subtitle, slides[index].subtitle.c_str());
    }
    
    if (child_count >= 3) {
        lv_obj_t *value1 = lv_obj_get_child(panel, 2);
        lv_label_set_text(value1, slides[index].value1.c_str());
    }
    
    if (child_count >= 4) {
        lv_obj_t *value2 = lv_obj_get_child(panel, 3);
        lv_label_set_text(value2, slides[index].value2.c_str());
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
