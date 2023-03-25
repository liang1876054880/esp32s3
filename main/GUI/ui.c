// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: SquareLine_Project

#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_Screen1;
lv_obj_t * ui_Button2;
lv_obj_t * ui_Button1;
lv_obj_t * ui_Slider2;
lv_obj_t * ui_Keyboard2;
lv_obj_t * ui_TextArea2;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////

///////////////////// SCREENS ////////////////////
void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Button2 = lv_btn_create(ui_Screen1);
    lv_obj_set_width(ui_Button2, 100);
    lv_obj_set_height(ui_Button2, 50);
    lv_obj_set_x(ui_Button2, -55);
    lv_obj_set_y(ui_Button2, 3);
    lv_obj_set_align(ui_Button2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Button1 = lv_btn_create(ui_Screen1);
    lv_obj_set_width(ui_Button1, 100);
    lv_obj_set_height(ui_Button1, 50);
    lv_obj_set_x(ui_Button1, 58);
    lv_obj_set_y(ui_Button1, 1);
    lv_obj_set_align(ui_Button1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button1, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Slider2 = lv_slider_create(ui_Screen1);
    lv_obj_set_width(ui_Slider2, 207);
    lv_obj_set_height(ui_Slider2, 10);
    lv_obj_set_x(ui_Slider2, 4);
    lv_obj_set_y(ui_Slider2, -54);
    lv_obj_set_align(ui_Slider2, LV_ALIGN_CENTER);

    ui_Keyboard2 = lv_keyboard_create(ui_Screen1);
    lv_obj_set_width(ui_Keyboard2, 300);
    lv_obj_set_height(ui_Keyboard2, 120);
    lv_obj_set_x(ui_Keyboard2, 2);
    lv_obj_set_y(ui_Keyboard2, 160);
    lv_obj_set_align(ui_Keyboard2, LV_ALIGN_CENTER);

    ui_TextArea2 = lv_textarea_create(ui_Screen1);
    lv_obj_set_width(ui_TextArea2, 260);
    lv_obj_set_height(ui_TextArea2, 70);
    lv_obj_set_x(ui_TextArea2, 3);
    lv_obj_set_y(ui_TextArea2, -161);
    lv_obj_set_align(ui_TextArea2, LV_ALIGN_CENTER);
    lv_textarea_set_placeholder_text(ui_TextArea2, "Placeholder...");

    lv_keyboard_set_textarea(ui_Keyboard2, ui_TextArea2);

}

void ui_init(void)
{
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_Screen1_screen_init();
    lv_disp_load_scr(ui_Screen1);
}
