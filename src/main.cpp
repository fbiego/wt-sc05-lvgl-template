/*
   MIT License

  Copyright (c) 2024 Felix Biego

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

  ______________  _____
  ___  __/___  /_ ___(_)_____ _______ _______
  __  /_  __  __ \__  / _  _ \__  __ `/_  __ \
  _  __/  _  /_/ /_  /  /  __/_  /_/ / / /_/ /
  /_/     /_.___/ /_/   \___/ _\__, /  \____/
                              /____/

*/

#include <Arduino.h>
#include <lvgl.h>
#include "main.h"
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Timber.h>

#ifdef USE_UI
#include "ui/ui.h"
#endif

#define SCR 8

#define GFX_BL 45
const int freq = 1000;
const int ledChannel = 7;
const int resolution = 8;

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    39 /* DE */, 38 /*VSYNC */, 5 /*HSYNC */, 9 /* PCLK */,
    10 /* R0 */, 11 /* R1 */, 12 /* R2 */, 13 /* R3 */, 14 /* R4 */,
    21 /* G0 */, 0 /* G1 */, 46 /* G2 */, 3 /* G3 */, 8 /* G4 */, 18 /* G5 */,
    17 /* B0 */, 16 /* B1 */, 15 /* B2 */, 7 /* B3 */, 6 /* B4 */,
    0 /* hsync_polarity */, 0 /* hsync_front_porch */, 210 /* hsync_pulse_width */, 30 /* hsync_back_porch */,
    0 /* vsync_polarity */, 0 /* vsync_front_porch */, 22 /* vsync_pulse_width */, 13 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 16000000 /* prefer_speed */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    800 /* width */, 481 /*height */, rgbpanel);

TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);


static const uint32_t screenWidth = 800;
static const uint32_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static lv_color_t disp_draw_buf[screenWidth * SCR];
static lv_color_t disp_draw_buf2[screenWidth * SCR];


/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  tp.read();
  if (!tp.isTouched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;
    /*Set the coordinates*/
    data->point.x = tp.points[0].x;
    data->point.y = tp.points[0].y;
  }
}

void logCallback(Level level, unsigned long time, String message)
{
  Serial0.print(message);

  switch (level)
  {
  case ERROR:
    // maybe save only errors to local storage
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial0.begin(115200);
  Timber.setLogCallback(logCallback);

  gfx->begin();
  gfx->fillScreen(BLACK);

  // #ifdef GFX_BL
  //   pinMode(GFX_BL, OUTPUT);
  //   digitalWrite(GFX_BL, HIGH);
  // #endif

  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(GFX_BL, ledChannel);
  ledcWrite(ledChannel, int(2.55 * 95));

  tp.begin();
  tp.setRotation(ROTATION_INVERTED);

  lv_init();

  Timber.i("Width: %d\tHeight: %d", screenWidth, screenHeight);

  if (!disp_draw_buf)
  {
    Timber.e("LVGL disp_draw_buf allocate failed!");
  }
  else
  {

    Timber.i("Display buffer size: ");

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, disp_draw_buf2, screenWidth * SCR);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

#ifdef USE_UI
    ui_init();

#else

    lv_obj_t *label1 = lv_label_create(lv_scr_act());
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 100);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label1, screenWidth - 30);
    lv_label_set_text(label1, "Hello there! You have not included UI files, add you UI files and "
                              "uncomment this line\n'//#define USE_UI' in include/main.h\n"
                              "You should be able to move the slider below");

    /*Create a slider below the label*/
    lv_obj_t *slider1 = lv_slider_create(lv_scr_act());
    lv_obj_set_width(slider1, screenWidth - 40);
    lv_obj_align_to(slider1, label1, LV_ALIGN_OUT_BOTTOM_MID, 0, 50);
#endif
    Timber.i("Setup done");
  }

}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);

}

