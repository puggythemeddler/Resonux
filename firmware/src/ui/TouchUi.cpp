#include "ui/TouchUi.h"
#include <Arduino.h>

#ifdef ENABLE_TOUCHUI

#include <lvgl.h>
#include <string.h>
#include "display/DisplayFactory.h"
#include "esp_heap_caps.h"
#include "runtime/App.h"
#include "theme/ThemeEngine.h"

namespace {

const int kMaxThemeButtons = 40;

// --- LVGL display+input plumbing ----------------------------------------------
lv_color_t* gBuf = nullptr;
lv_disp_draw_buf_t gDrawBuf;
lv_disp_drv_t gDispDrv;
lv_indev_drv_t gIndevDrv;

volatile int  gPtX = 0;
volatile int  gPtY = 0;
volatile bool gPtPressed = false;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  DisplayManager::instance().flushArea(
      area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area),
      (const uint16_t*)color_p);
  lv_disp_flush_ready(drv);
}

void readCb(lv_indev_drv_t*, lv_indev_data_t* data) {
  data->point.x = (lv_coord_t)gPtX;
  data->point.y = (lv_coord_t)gPtY;
  data->state = gPtPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace

struct TouchUi::Impl {
  DisplayManager* mgr = nullptr;
  lv_obj_t* themeLabel = nullptr;   // Now playing active theme
  lv_obj_t* beatLabel = nullptr;
  lv_obj_t* bars[8] = {};
  lv_obj_t* brightValue = nullptr;  // System tab readout
  lv_obj_t* brightSlider = nullptr;
  lv_obj_t* themeButtons[kMaxThemeButtons] = {};
  const char* themeIds[kMaxThemeButtons] = {};
  int themeCount = 0;
  AudioFrame frame;
  uint32_t lastTick = 0;
};

TouchUi& TouchUi::instance() {
  static TouchUi ui;
  return ui;
}

static void themeBtnCb(lv_event_t* e) {
  const char* id = (const char*)lv_event_get_user_data(e);
  if (id) App::instance().setTheme(id);
}

static void brightnessCb(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  uint8_t v = (uint8_t)lv_slider_get_value(slider);
  App::instance().setMasterBrightness(v);
}

static lv_obj_t* makeTab(lv_obj_t* tv, const char* title) {
  lv_obj_t* t = lv_tabview_add_tab(tv, title);
  lv_obj_set_style_pad_all(t, 8, 0);
  return t;
}

void TouchUi::begin(DisplayManager& mgr) {
  if (_impl) return;
  DisplayConfig cfg = mgr.cfg();
  if (!cfg.enabled) return;

  DisplayDriver* dd = createDisplayDriver(cfg);
  if (!dd) return;  // unsupported/unknown panel in config
  TouchDriver* td = createTouchDriver(cfg);  // may be null

  // Re-arm the manager now that a real driver exists.
  mgr.begin(cfg, dd, td);
  if (!mgr.enabled() || !dd->isReady()) {
    // drop the manager back to no-op rather than boot-loop on dead hardware
    mgr.begin(cfg, nullptr, nullptr);
    delete dd;
    delete td;
    return;
  }

  lv_init();

  int w = mgr.panelW();
  int h = mgr.panelH();
  size_t total = (size_t)w * 12;   // ~12 rows of draw buffer
  gBuf = (lv_color_t*)heap_caps_malloc(total * sizeof(lv_color_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!gBuf) gBuf = (lv_color_t*)malloc(total * sizeof(lv_color_t));
  if (!gBuf) return;

  lv_disp_draw_buf_init(&gDrawBuf, gBuf, nullptr, total);
  lv_disp_drv_init(&gDispDrv);
  gDispDrv.hor_res = w;
  gDispDrv.ver_res = h;
  gDispDrv.flush_cb = flushCb;
  gDispDrv.draw_buf = &gDrawBuf;
  lv_disp_drv_register(&gDispDrv);

  lv_indev_drv_init(&gIndevDrv);
  gIndevDrv.type = LV_INDEV_TYPE_POINTER;
  gIndevDrv.read_cb = readCb;
  lv_indev_drv_register(&gIndevDrv);

  _impl = new Impl();
  _impl->mgr = &mgr;
  _impl->lastTick = millis();

  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0F17), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t* tv = lv_tabview_create(scr, LV_DIR_TOP, 44);
  lv_obj_set_style_bg_color(tv, lv_color_hex(0x0B0F17), 0);
  lv_obj_t* tabNow = makeTab(tv, "Now");
  lv_obj_t* tabThemes = makeTab(tv, "Themes");
  lv_obj_t* tabSys = makeTab(tv, "System");

  // --- Now playing -----------------------------------------------------------
  _impl->themeLabel = lv_label_create(tabNow);
  lv_obj_set_width(_impl->themeLabel, LV_PCT(100));
  lv_obj_set_style_text_align(_impl->themeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(_impl->themeLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(_impl->themeLabel, lv_color_hex(0xFF7A18), 0);
  lv_obj_set_style_pad_top(_impl->themeLabel, 10, 0);

  lv_obj_t* barRow = lv_obj_create(tabNow);
  lv_obj_set_width(barRow, LV_PCT(100));
  lv_obj_set_height(barRow, 220);
  lv_obj_set_style_bg_opa(barRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(barRow, 0, 0);
  lv_obj_set_style_pad_all(barRow, 8, 0);
  lv_obj_set_flex_flow(barRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(barRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(barRow, 4, 0);
  for (int i = 0; i < 8; ++i) {
    lv_obj_t* b = lv_bar_create(barRow);
    lv_obj_set_size(b, 22, 180);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1C2A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xFF7A18), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    _impl->bars[i] = b;
  }

  _impl->beatLabel = lv_label_create(tabNow);
  lv_obj_set_width(_impl->beatLabel, LV_PCT(100));
  lv_obj_set_style_text_align(_impl->beatLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(_impl->beatLabel, lv_color_hex(0x22D3EE), 0);
  lv_obj_set_style_text_font(_impl->beatLabel, &lv_font_montserrat_14, 0);

  // --- Themes ----------------------------------------------------------------
  lv_obj_t* themeList = lv_obj_create(tabThemes);
  lv_obj_set_width(themeList, LV_PCT(100));
  lv_obj_set_height(themeList, LV_PCT(100));
  lv_obj_set_style_bg_opa(themeList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(themeList, 0, 0);
  lv_obj_set_flex_flow(themeList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(themeList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(themeList, 6, 0);
  lv_obj_set_scroll_dir(themeList, LV_DIR_VER);

  ThemeEngine& te = ThemeEngine::instance();
  int n = te.count();
  if (n > kMaxThemeButtons) n = kMaxThemeButtons;
  _impl->themeCount = n;
  for (int i = 0; i < n; ++i) {
    const ThemeDef* def = te.get(i);
    if (!def) continue;
    lv_obj_t* btn = lv_btn_create(themeList);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 44);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x141B26), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF7A18), LV_STATE_CHECKED);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, themeBtnCb, LV_EVENT_CLICKED, (void*)def->id);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, def->name);
    lv_obj_center(lbl);
    _impl->themeButtons[i] = btn;
    _impl->themeIds[i] = def->id;
  }

  // --- System ----------------------------------------------------------------
  lv_obj_t* sLbl = lv_label_create(tabSys);
  lv_label_set_text(sLbl, "Master brightness");
  lv_obj_set_style_text_color(sLbl, lv_color_hex(0x8899AA), 0);
  lv_obj_set_style_text_font(sLbl, &lv_font_montserrat_14, 0);
  lv_obj_align(sLbl, LV_ALIGN_TOP_MID, 0, 10);

  _impl->brightSlider = lv_slider_create(tabSys);
  lv_obj_set_width(_impl->brightSlider, LV_PCT(85));
  lv_obj_align(_impl->brightSlider, LV_ALIGN_TOP_MID, 0, 44);
  lv_slider_set_range(_impl->brightSlider, 0, 255);
  lv_slider_set_value(_impl->brightSlider, App::instance().masterBrightness(),
                      LV_ANIM_OFF);
  lv_obj_add_event_cb(_impl->brightSlider, brightnessCb, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  _impl->brightValue = lv_label_create(tabSys);
  lv_obj_set_style_text_font(_impl->brightValue, &lv_font_montserrat_20, 0);
  lv_obj_align(_impl->brightValue, LV_ALIGN_TOP_MID, 0, 96);

  lv_obj_t* sysNote = lv_label_create(tabSys);
  lv_label_set_text(sysNote, "Screen timeout does not stop audio or LEDs");
  lv_obj_set_style_text_color(sysNote, lv_color_hex(0x556677), 0);
  lv_obj_set_style_text_font(sysNote, &lv_font_montserrat_12, 0);
  lv_obj_align(sysNote, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void TouchUi::tick(uint32_t nowMs) {
  if (!_impl) return;
  uint32_t dt = nowMs - _impl->lastTick;
  if (dt && dt < 1000) lv_tick_inc(dt);
  _impl->lastTick = nowMs;
}

void TouchUi::render() {
  if (!_impl) return;
  Impl& I = *_impl;

  // ---- refresh dynamic widgets ----
  const AudioFrame& f = I.frame;
  for (int i = 0; i < 8 && i < (int)f.bandCount; ++i) {
    int v = (int)(f.bands[i] * 100.0f);
    if (v > 100) v = 100;
    if (I.bars[i]) lv_bar_set_value(I.bars[i], v, LV_ANIM_OFF);
  }
  const char* activeId = ThemeEngine::instance().activeId();
  const ThemeDef* active = activeId ? ThemeEngine::instance().find(activeId) : nullptr;
  if (I.themeLabel) {
    lv_label_set_text(I.themeLabel, active ? active->name : "—");
  }
  if (I.beatLabel) {
    lv_label_set_text(I.beatLabel, f.beat ? "BEAT" : " ");
  }
  for (int i = 0; i < I.themeCount; ++i) {
    lv_obj_t* b = I.themeButtons[i];
    if (!b) continue;
    if (I.themeIds[i] && activeId && strcmp(I.themeIds[i], activeId) == 0) {
      lv_obj_add_state(b, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(b, LV_STATE_CHECKED);
    }
  }
  uint8_t mb = App::instance().masterBrightness();
  if (I.brightValue) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d / 255", (int)mb);
    lv_label_set_text(I.brightValue, buf);
  }
  if (I.brightSlider && (uint8_t)lv_slider_get_value(I.brightSlider) != mb) {
    lv_slider_set_value(I.brightSlider, mb, LV_ANIM_OFF);
  }

  lv_timer_handler();
}

void TouchUi::handleTouch(const TouchPoint& tp) {
  if (!_impl || !_impl->mgr) return;
  int px = 0, py = 0;
  _impl->mgr->mapPoint(tp.x >= 0 ? tp.x : 0, tp.y >= 0 ? tp.y : 0, px, py);
  gPtX = px;
  gPtY = py;
  gPtPressed = tp.touched;
}

void TouchUi::updateAudio(const AudioFrame& f) {
  if (!_impl) return;
  _impl->frame = f;
}

#else  // !ENABLE_TOUCHUI — compile to no-ops so the default build never links LVGL

TouchUi& TouchUi::instance() {
  static TouchUi ui;
  return ui;
}
void TouchUi::begin(DisplayManager&) {}
void TouchUi::tick(uint32_t) {}
void TouchUi::render() {}
void TouchUi::handleTouch(const TouchPoint&) {}
void TouchUi::updateAudio(const AudioFrame&) {}

#endif  // ENABLE_TOUCHUI