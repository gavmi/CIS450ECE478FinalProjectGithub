/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

//notes
// light_2color_layer_enter_cb function triggers when entering the light layer (selecting the light option with knob panel)
// light_2color_layer_exit_cb function triggers when exiting the light layer (long press on knob while in the light layer)
// when xEventGroupWaitBits is called it waits for a signal before running any subsequent code
// signal is produced by xEventGroupSetBits using the defined PWM_UPDATE and pwm_event_group
// switch case is to play the corresponding audio file based on the updated global variable
// 

#include "lvgl.h"
#include <stdio.h>
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "bsp/esp-bsp.h"

//added functions to get the audio working (all non duplicate includes from app_audio.c and an include of app_audio.h)
#include <time.h>
#include "src/misc/lv_math.h"
#include "settings.h"
#include "app_audio.h"


static bool light_2color_layer_enter_cb(void *layer);
static bool light_2color_layer_exit_cb(void *layer);
static void light_2color_layer_timer_cb(lv_timer_t *tmr);

//my stuff ----------------------------------------------------------------------------------------
uint8_t global_light_pwm; //global variable for the voice thread
#define PWM_UPDATE  (1 << 1) //value to signal when global var global_light_pwm is updated
static EventGroupHandle_t pwm_event_group; //event group for signaling
static TaskHandle_t light_2voice_handle = NULL; //task handle for create & delete
//my stuff ----------------------------------------------------------------------------------------

typedef enum {
    LIGHT_CCK_WARM,
    LIGHT_CCK_COOL,
    LIGHT_CCK_MAX,
} LIGHT_CCK_TYPE;
typedef struct {
    uint8_t light_pwm;
    LIGHT_CCK_TYPE light_cck;
} light_set_attribute_t;
typedef struct {
    const lv_img_dsc_t *img_bg[2];
    const lv_img_dsc_t *img_pwm_25[2];
    const lv_img_dsc_t *img_pwm_50[2];
    const lv_img_dsc_t *img_pwm_75[2];
    const lv_img_dsc_t *img_pwm_100[2];
} ui_light_img_t;
    static lv_obj_t *page;
    static time_out_count time_20ms, time_500ms;
    static lv_obj_t *img_light_bg, *label_pwm_set;
    static lv_obj_t *img_light_pwm_25, *img_light_pwm_50, *img_light_pwm_75, *img_light_pwm_100, *img_light_pwm_0;
    static light_set_attribute_t light_set_conf, light_xor;
    static const ui_light_img_t light_image = {
    {&light_warm_bg,     &light_cool_bg},
    {&light_warm_25,     &light_cool_25},
    {&light_warm_50,     &light_cool_50},
    {&light_warm_75,     &light_cool_75},
    {&light_warm_100,    &light_cool_100},
};

lv_layer_t light_2color_Layer = {
    .lv_obj_name    = "light_2color_Layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = light_2color_layer_enter_cb,
    .exit_cb        = light_2color_layer_exit_cb,
    .timer_cb       = light_2color_layer_timer_cb,
};

//my stuff ----------------------------------------------------------------------------------------
void light_2voice_event(void *parameters) //function to play audio files based on global variable global_light_pwm
{
    while (true) {
        //wait for PWM_UPDATE bit to be set
        EventBits_t bits = xEventGroupWaitBits(pwm_event_group, PWM_UPDATE, pdTRUE, pdFALSE, portMAX_DELAY);
        
        if (bits & PWM_UPDATE) {
            //switch case to play audio
            switch (global_light_pwm) {
                case 0:
                    //play audio file for 0%
                    audio_handle_info(SOUND_TYPE_0);
                    break;
                case 25:
                    //play audio file for 25%
                    audio_handle_info(SOUND_TYPE_25);
                    break;
                case 50:
                    //play audio file for 50%
                    audio_handle_info(SOUND_TYPE_50);
                    break;
                case 75:
                    //play audio file for 75%
                    audio_handle_info(SOUND_TYPE_75);
                    break;
                case 100:
                    //play audio file for 100%
                    audio_handle_info(SOUND_TYPE_100);
                    break;
            }
        }
    }
}
//my stuff ----------------------------------------------------------------------------------------

static void light_2color_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_group_set_editing(lv_group_get_default(), true);
    } else if (LV_EVENT_KEY == code) {
        uint32_t key = lv_event_get_key(e);
        if (is_time_out(&time_500ms)) {
            if (LV_KEY_RIGHT == key) {
                if (light_set_conf.light_pwm < 100) {
                    light_set_conf.light_pwm += 25;
                }
            } else if (LV_KEY_LEFT == key) {
                if (light_set_conf.light_pwm > 0) {
                    light_set_conf.light_pwm -= 25;
                }
            }
        }

        //my stuff ----------------------------------------------------------------------------------------
        global_light_pwm = light_set_conf.light_pwm; //sets global variable to current light pwm value
        xEventGroupSetBits(pwm_event_group, PWM_UPDATE); //signals the xEventGroupWaitBits in light_2voice_event
        //my stuff ----------------------------------------------------------------------------------------


    } else if (LV_EVENT_CLICKED == code) {
        light_set_conf.light_cck = \
                                   (LIGHT_CCK_WARM == light_set_conf.light_cck) ? (LIGHT_CCK_COOL) : (LIGHT_CCK_WARM);
    } else if (LV_EVENT_LONG_PRESSED == code) {
        lv_indev_wait_release(lv_indev_get_next(NULL));
        ui_remove_all_objs_from_encoder_group();
        lv_func_goto_layer(&menu_layer);
    }
}


void ui_light_2color_init(lv_obj_t *parent)
{
    light_xor.light_pwm = 0xFF;
    light_xor.light_cck = LIGHT_CCK_MAX;
    light_set_conf.light_pwm = 50;
    //my stuff ----------------------------------------------------------------------------------------
    audio_handle_info(SOUND_TYPE_50); //plays the audio for 50% light when entering light layer since thats its begining value
    //my stuff ----------------------------------------------------------------------------------------
    light_set_conf.light_cck = LIGHT_CCK_WARM;
    page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(page);
    img_light_bg = lv_img_create(page);
    lv_img_set_src(img_light_bg, &light_warm_bg);
    lv_obj_align(img_light_bg, LV_ALIGN_CENTER, 0, 0);
    label_pwm_set = lv_label_create(page);
    lv_obj_set_style_text_font(label_pwm_set, &HelveticaNeue_Regular_24, 0);
    if (light_set_conf.light_pwm) {
        lv_label_set_text_fmt(label_pwm_set, "%d%%", light_set_conf.light_pwm);
    } else {
        lv_label_set_text(label_pwm_set, "--");
    }
    lv_obj_align(label_pwm_set, LV_ALIGN_CENTER, 0, 65);
    img_light_pwm_0 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_0, &light_close_status);
    lv_obj_add_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(img_light_pwm_0, LV_ALIGN_TOP_MID, 0, 0);
    img_light_pwm_25 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_25, &light_warm_25);
    lv_obj_align(img_light_pwm_25, LV_ALIGN_TOP_MID, 0, 0);
    img_light_pwm_50 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_50, &light_warm_50);
    lv_obj_align(img_light_pwm_50, LV_ALIGN_TOP_MID, 0, 0);
    img_light_pwm_75 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_75, &light_warm_75);
    lv_obj_add_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(img_light_pwm_75, LV_ALIGN_TOP_MID, 0, 0);
    img_light_pwm_100 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_100, &light_warm_100);
    lv_obj_add_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(img_light_pwm_100, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_CLICKED, NULL);
    ui_add_obj_to_encoder_group(page);
}


static bool light_2color_layer_enter_cb(void *layer)
{
    bool ret = false;

    LV_LOG_USER("");
    lv_layer_t *create_layer = layer;
    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);
        ui_light_2color_init(create_layer->lv_obj_layer);
        set_time_out(&time_20ms, 20);
        set_time_out(&time_500ms, 200);
    }


    //my stuff ----------------------------------------------------------------------------------------
    pwm_event_group = xEventGroupCreate(); //initialize pwm_event_group
    xTaskCreate(light_2voice_event, "PlayAudioTask", 2048, NULL, 2, &light_2voice_handle); //create task light_2voice_event
    //stack size 2048
    //priority 5

    //my stuff ----------------------------------------------------------------------------------------

    return ret;
}


static bool light_2color_layer_exit_cb(void *layer)
{
    LV_LOG_USER("");
    bsp_led_rgb_set(0x00, 0x00, 0x00);


    //my stuff ----------------------------------------------------------------------------------------
    vTaskDelete(light_2voice_handle); //deletes task when exiting the light layer
    vEventGroupDelete(pwm_event_group); //deletes event group when exiting the light layer
    //my stuff ----------------------------------------------------------------------------------------


    return true;
}


static void light_2color_layer_timer_cb(lv_timer_t *tmr)
{
    uint32_t RGB_color = 0xFF;
    feed_clock_time();

    if (is_time_out(&time_20ms)) {
        if ((light_set_conf.light_pwm ^ light_xor.light_pwm) || (light_set_conf.light_cck ^ light_xor.light_cck)) {
            light_xor.light_pwm = light_set_conf.light_pwm;
            light_xor.light_cck = light_set_conf.light_cck;
            if (LIGHT_CCK_COOL == light_xor.light_cck) {
                RGB_color = (0xFF * light_xor.light_pwm / 100) << 16 | (0xFF * light_xor.light_pwm / 100) << 8 | (0xFF * light_xor.light_pwm / 100) << 0;
            } else {
                RGB_color = (0xFF * light_xor.light_pwm / 100) << 16 | (0xFF * light_xor.light_pwm / 100) << 8 | (0x33 * light_xor.light_pwm / 100) << 0;
            }
            bsp_led_rgb_set((RGB_color >> 16) & 0xFF, (RGB_color >> 8) & 0xFF, (RGB_color >> 0) & 0xFF);
            lv_obj_add_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_50, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_25, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN);
            if (light_set_conf.light_pwm) {
                lv_label_set_text_fmt(label_pwm_set, "%d%%", light_set_conf.light_pwm);
            } else {
                lv_label_set_text(label_pwm_set, "--");
            }

            uint8_t cck_set = (uint8_t)light_xor.light_cck;

            switch (light_xor.light_pwm) {
            case 100:
                lv_obj_clear_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_100, light_image.img_pwm_100[cck_set]);
            case 75:
                lv_obj_clear_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_75, light_image.img_pwm_75[cck_set]);
            case 50:
                lv_obj_clear_flag(img_light_pwm_50, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_50, light_image.img_pwm_50[cck_set]);
            case 25:
                lv_obj_clear_flag(img_light_pwm_25, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_25, light_image.img_pwm_25[cck_set]);
                lv_img_set_src(img_light_bg, light_image.img_bg[cck_set]);
                break;
            case 0:
                lv_obj_clear_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_bg, &light_close_bg);
                break;
            default:
                break;
            }
        }
    }
}