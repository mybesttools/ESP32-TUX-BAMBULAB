/*
 * Carousel Widget for displaying weather locations and printer status
 * Supports horizontal scrolling through multiple cards/slides
 */

#ifndef CAROUSEL_WIDGET_HPP
#define CAROUSEL_WIDGET_HPP

#include "lvgl/lvgl.h"
#include <vector>
#include <string>

// Slide data structure
struct carousel_slide_t {
    std::string title;           // Location name or printer name
    std::string subtitle;        // City, country or status
    std::string value1;          // Temperature or progress
    std::string value2;          // Weather/status description
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
    // Main carousel container
    container = lv_obj_create(parent);
    lv_obj_set_size(container, width, height);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    
    // Scrollable container for slides
    scroll_container = lv_obj_create(container);
    lv_obj_set_size(scroll_container, width, height - 50);  // Leave space for indicator
    lv_obj_set_pos(scroll_container, 0, 0);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_HOR);
    lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 0, 0);
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_OFF);
    
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
    
    // Create slide panel
    lv_obj_t *slide_panel = lv_obj_create(scroll_container);
    lv_obj_set_size(slide_panel, lv_obj_get_width(scroll_container), lv_obj_get_height(scroll_container));
    lv_obj_set_style_bg_color(slide_panel, slide.bg_color, 0);
    lv_obj_set_style_border_width(slide_panel, 0, 0);
    lv_obj_set_style_pad_all(slide_panel, 20, 0);
    
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
    // Clear existing slides
    for (auto panel : slide_panels) {
        lv_obj_del(panel);
    }
    slide_panels.clear();
    slide_labels.clear();
    
    // Recreate all slides
    for (const auto &slide : slides) {
        add_slide(slide);
    }
    
    current_slide = 0;
    update_page_indicator();
}

void CarouselWidget::show_slide(int index)
{
    if (index < 0 || index >= (int)slides.size()) {
        return;
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
    int next_index = (current_slide + 1) % slides.size();
    show_slide(next_index);
}

void CarouselWidget::prev_slide()
{
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
