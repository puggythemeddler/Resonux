/**
 * @file lv_conf.h
 * Configuration file for v8.3. Made for the Resonux touchscreen UI.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_COLOR_16_SWAP 0
#define LV_COLOR_SCREEN_TRANSP 0

#define LV_DPI_DEF 130

#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4

#define LV_USE_THORVG_INTERNAL 0

/* 1 = Max performance (mask) 0 = Min memory consumption */
#define LV_DRAW_SW_COMPLEX 1

/* 1 = double buffering, 0 = single */
#define LV_DISP_DEF_REFR_PERIOD 33

#define LV_INDEV_DEF_READ_PERIOD 33
#define LV_INDEV_DEF_TAP_TIME 200
#define LV_INDEV_DEF_LONG_PRESS_TIME 400
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME 100

#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_ASM 0

#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_GD32_IPA 0
#define LV_USE_GPU_ARM2D 0
#define LV_GPU_ARM2D_AUTO_SCALING 1

#define LV_USE_OS 0

/* Mem */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (80 * 1024)
#define LV_MEM_BUF_MAX_NUM 16

#define LV_MEMCPY_MEMSET_STD 1

#define LV_MEM_POOL_INCLUDE <stdint.h>

#define LV_MEM_POOL_ALLOC(x) malloc(x)

/* Log */
#define LV_USE_LOG 0
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0
#define LV_LOG_TRACE_MEM 1
#define LV_LOG_TRACE_TIMER 1
#define LV_LOG_TRACE_INDEV 1
#define LV_LOG_TRACE_DISP 1
#define LV_LOG_TRACE_EVENT 1
#define LV_LOG_TRACE_ANIM 1
#define LV_LOG_TRACE_CACHE 1

/* Asserts */
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

#define LV_USE_TICK_CUSTOM 0

#define LV_SPRINTF_CUSTOM 0

#define LV_USE_USER_DATA 1

#define LV_USE_ATTRIBUTE_FAST_MEM 0

#define LV_USE_DISPLAY 1
#define LV_USE_TIMER 1

/* Fonts */
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0
#define LV_FONT_CUSTOM 0

#ifndef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#endif

/* Symbols */
#define LV_SYMBOL_AUDIO 0
#define LV_SYMBOL_VIDEO 0
#define LV_SYMBOL_LIST 1
#define LV_SYMBOL_OK 1
#define LV_SYMBOL_CLOSE 1
#define LV_SYMBOL_POWER 1
#define LV_SYMBOL_SETTINGS 1
#define LV_SYMBOL_HOME 1
#define LV_SYMBOL_DOWNLOAD 0
#define LV_SYMBOL_DRIVE 0
#define LV_SYMBOL_REFRESH 1
#define LV_SYMBOL_MUTE 0
#define LV_SYMBOL_VOLUME_MAX 1
#define LV_SYMBOL_VOLUME_MID 0
#define LV_SYMBOL_VOLUME_MIN 0
#define LV_SYMBOL_EDIT 0
#define LV_SYMBOL_PREV 1
#define LV_SYMBOL_PLAY 1
#define LV_SYMBOL_PAUSE 1
#define LV_SYMBOL_NEXT 1
#define LV_SYMBOL_EJECT 0
#define LV_SYMBOL_LEFT 1
#define LV_SYMBOL_RIGHT 1
#define LV_SYMBOL_PLUS 1
#define LV_SYMBOL_MINUS 1
#define LV_SYMBOL_EYE_OPEN 0
#define LV_SYMBOL_EYE_CLOSE 0
#define LV_SYMBOL_WARNING 0
#define LV_SYMBOL_SHUFFLE 0
#define LV_SYMBOL_UP 1
#define LV_SYMBOL_DOWN 1
#define LV_SYMBOL_LOOP 0
#define LV_SYMBOL_DIRECTORY 0
#define LV_SYMBOL_UPLOAD 0
#define LV_SYMBOL_CALL 0
#define LV_SYMBOL_CUT 0
#define LV_SYMBOL_COPY 0
#define LV_SYMBOL_PASTE 0
#define LV_SYMBOL_SAVE 0
#define LV_SYMBOL_BELL 0
#define LV_SYMBOL_KEYBOARD 0
#define LV_SYMBOL_GPS 0
#define LV_SYMBOL_WIFI 1
#define LV_SYMBOL_BATTERY_FULL 0
#define LV_SYMBOL_BATTERY_3 0
#define LV_SYMBOL_BATTERY_2 0
#define LV_SYMBOL_BATTERY_1 0
#define LV_SYMBOL_BATTERY_EMPTY 0
#define LV_SYMBOL_USB 0
#define LV_SYMBOL_BLUETOOTH 0
#define LV_SYMBOL_NEW_LINE 0
#define LV_SYMBOL_GLOBE 0
#define LV_SYMBOL_KEY 0
#define LV_SYMBOL_IMG 0
#define LV_SYMBOL_BACKSPACE 0
#define LV_SYMBOL_TINT 1
#define LV_SYMBOL_BRIGHTNESS 1

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0

/* Widgets */
#define LV_USE_ARC 0
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMG 0
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 0
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0

#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LIST 1
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_SPAN 0
#define LV_USE_LAYOUT 1
#define LV_USE_GRID 1

/* Images */
#define LV_USE_IMG_DECODE 0

/* Decorations */
#define LV_USE_RADIUS 1
#define LV_USE_SHADOW 1
#define LV_USE_TEXT_AA 1
#define LV_USE_OPA_SCALE 1

/* Graphics */
#define LV_USE_GPU_NONE 0

#define LV_USE_TRANSFORM 0

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif  /* LV_CONF_H */