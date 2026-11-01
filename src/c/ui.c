#include "ui.h"
#include "game.h"

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_redraw = NULL;

static GBitmap *s_bmp_title = NULL;
static GBitmap *s_bmp_printer[3] = {NULL, NULL, NULL};
static GBitmap *s_bmp_spool[5] = {NULL, NULL, NULL, NULL, NULL};
static GBitmap *s_bmp_status_ok = NULL;
static GBitmap *s_bmp_status_jam = NULL;
static GBitmap *s_bmp_status_heat = NULL;
static GBitmap *s_bmp_status_warn = NULL;
static GBitmap *s_bmp_fail_spaghetti = NULL;
static GBitmap *s_bmp_fail_flame = NULL;
static GBitmap *s_bmp_fail_empty = NULL;

#if defined(PBL_COLOR)
#define C_BG       GColorBlack
#define C_PANEL    GColorDarkGray
#define C_TEXT     GColorWhite
#define C_DIM      GColorLightGray
#define C_ACCENT   GColorCyan
#define C_OK       GColorBrightGreen
#define C_WARN     GColorChromeYellow
#define C_BAD      GColorRed
#define C_BAR_BG   GColorDarkGray
#define C_BAR_FG   GColorBrightGreen
#define C_FOCUS    GColorVividCerulean
#else
#define C_BG       GColorBlack
#define C_PANEL    GColorBlack
#define C_TEXT     GColorWhite
#define C_DIM      GColorWhite
#define C_ACCENT   GColorWhite
#define C_OK       GColorWhite
#define C_WARN     GColorWhite
#define C_BAD      GColorWhite
#define C_BAR_BG   GColorBlack
#define C_BAR_FG   GColorWhite
#define C_FOCUS    GColorWhite
#endif

static void redraw_cb(void *ctx) {
  (void)ctx;
  s_redraw = NULL;
  if (s_canvas) layer_mark_dirty(s_canvas);
  s_redraw = app_timer_register(200, redraw_cb, NULL);
}

static void draw_bmp(GContext *ctx, GBitmap *bmp, GPoint origin) {
  if (!bmp) return;
  GRect b = gbitmap_get_bounds(bmp);
  GRect dest = GRect(origin.x, origin.y, b.size.w, b.size.h);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, bmp, dest);
}

static void draw_bmp_centered(GContext *ctx, GBitmap *bmp, GPoint center) {
  if (!bmp) return;
  GRect b = gbitmap_get_bounds(bmp);
  draw_bmp(ctx, bmp, GPoint(center.x - b.size.w / 2, center.y - b.size.h / 2));
}

static GColor status_color(PrinterStatus s) {
  switch (s) {
    case PrinterStatus_Done: return C_OK;
    case PrinterStatus_Printing: return C_ACCENT;
    case PrinterStatus_Idle: return C_DIM;
    case PrinterStatus_Jam:
    case PrinterStatus_Hotend: return C_BAD;
    case PrinterStatus_Filament:
    case PrinterStatus_Bed: return C_WARN;
    default: return C_DIM;
  }
}

static GBitmap *status_glyph(PrinterStatus s) {
  switch (s) {
    case PrinterStatus_Done:
    case PrinterStatus_Idle: return s_bmp_status_ok;
    case PrinterStatus_Jam: return s_bmp_status_jam;
    case PrinterStatus_Hotend: return s_bmp_status_heat;
    case PrinterStatus_Filament:
    case PrinterStatus_Bed:
    case PrinterStatus_Printing: return s_bmp_status_warn;
    default: return s_bmp_status_warn;
  }
}

static void format_money(char *buf, size_t n, uint32_t money) {
  if (money >= 10000) {
    snprintf(buf, n, "$%luk", (unsigned long)(money / 1000));
  } else {
    snprintf(buf, n, "$%lu", (unsigned long)money);
  }
}

static void draw_progress_bar(GContext *ctx, GRect r, uint8_t pct) {
  graphics_context_set_fill_color(ctx, C_BAR_BG);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
  if (pct > 100) pct = 100;
  int16_t w = (int16_t)((r.size.w * pct) / 100);
  if (w > 0) {
    graphics_context_set_fill_color(ctx, C_BAR_FG);
    graphics_fill_rect(ctx, GRect(r.origin.x, r.origin.y, w, r.size.h), 0, GCornerNone);
  }
  graphics_context_set_stroke_color(ctx, C_DIM);
  graphics_draw_rect(ctx, r);
}

static void draw_title(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, C_TEXT);
  if (s_bmp_title) {
    GRect tb = gbitmap_get_bounds(s_bmp_title);
    int16_t tw = tb.size.w;
    if (tw > bounds.size.w - 4) tw = bounds.size.w - 4;
    GRect dest = GRect((bounds.size.w - tw) / 2, 10, tw, tb.size.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_bmp_title, dest);
  } else {
    graphics_draw_text(ctx, "PRINT FARM", fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                       GRect(2, 24, bounds.size.w - 4, 32), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, C_DIM);
  graphics_draw_text(ctx, "Manage printers\nfrom your wrist",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(4, bounds.size.h / 2, bounds.size.w - 8, 40),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, C_TEXT);
  graphics_draw_text(ctx, "SELECT to open", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, bounds.size.h - 36, bounds.size.w - 8, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_farm(GContext *ctx, GRect bounds, const Farm *f) {
  char line[32];
  graphics_context_set_text_color(ctx, C_TEXT);

  format_money(line, sizeof(line), f->money);
  graphics_draw_text(ctx, "PRINT FARM", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(2, 0, bounds.size.w / 2, 16), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(bounds.size.w / 2, 0, bounds.size.w / 2 - 2, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  snprintf(line, sizeof(line), "Rep %u  Staff %u", (unsigned)f->reputation,
           (unsigned)f->staff_level);
  graphics_context_set_text_color(ctx, C_DIM);
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, 14, bounds.size.w - 4, 14), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  const int16_t row_h = 22;
  const int16_t list_top = 30;
  uint8_t rows = (uint8_t)(f->owned_count + 1);
  for (uint8_t i = 0; i < rows; i++) {
    int16_t y = (int16_t)(list_top + i * row_h);
    if (y + row_h > bounds.size.h - 22) break;

    bool focused = (i == f->focus);
    if (focused) {
      graphics_context_set_fill_color(ctx, C_PANEL);
#if defined(PBL_COLOR)
      graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, row_h - 1), 0, GCornerNone);
#else
      graphics_context_set_stroke_color(ctx, C_FOCUS);
      graphics_draw_rect(ctx, GRect(0, y, bounds.size.w, row_h - 1));
#endif
    }

    if (i < f->owned_count) {
      const Printer *p = &f->printers[i];
      GBitmap *pb = s_bmp_printer[p->tier];
      if (pb) draw_bmp(ctx, pb, GPoint(2, y + 1));
      else {
        graphics_context_set_fill_color(ctx, C_ACCENT);
        graphics_fill_rect(ctx, GRect(4, y + 4, 14, 14), 0, GCornerNone);
      }

      GBitmap *sg = status_glyph(p->status);
      if (sg) draw_bmp(ctx, sg, GPoint(36, y + 3));

      graphics_context_set_text_color(ctx, status_color(p->status));
      snprintf(line, sizeof(line), "P%u %s", (unsigned)(i + 1), status_name(p->status));
      graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(54, y, 50, 16), GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft, NULL);

      if (p->status == PrinterStatus_Printing || p->status == PrinterStatus_Done ||
          p->status == PrinterStatus_Jam || p->status == PrinterStatus_Filament ||
          p->status == PrinterStatus_Bed || p->status == PrinterStatus_Hotend) {
        draw_progress_bar(ctx, GRect(104, y + 6, bounds.size.w - 108, 8), p->progress_pct);
      } else {
        GBitmap *sp = s_bmp_spool[p->filament];
        if (sp) draw_bmp(ctx, sp, GPoint(bounds.size.w - 18, y + 3));
      }
    } else {
      graphics_context_set_text_color(ctx, C_ACCENT);
      graphics_draw_text(ctx, focused ? "> SHOP" : "  SHOP",
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(4, y, bounds.size.w - 8, 20),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  graphics_context_set_text_color(ctx, C_DIM);
  const char *hint = f->banner[0] ? f->banner : "UP/DN  SEL open";
  graphics_draw_text(ctx, hint, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, bounds.size.h - 18, bounds.size.w - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_detail(GContext *ctx, GRect bounds, const Farm *f) {
  const Printer *p = &f->printers[f->detail_idx];
  char line[36];

  graphics_context_set_text_color(ctx, C_TEXT);
  snprintf(line, sizeof(line), "PRINTER %u  %s", (unsigned)(f->detail_idx + 1),
           tier_name(p->tier));
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(2, 0, bounds.size.w - 4, 16), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  GBitmap *pb = s_bmp_printer[p->tier];
  if (pb) draw_bmp_centered(ctx, pb, GPoint(bounds.size.w / 2, 40));
  else {
    graphics_context_set_fill_color(ctx, C_ACCENT);
    graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 16, 24, 32, 32), 0, GCornerNone);
  }

  // Fail overlay
  if (p->status == PrinterStatus_Jam && s_bmp_fail_spaghetti) {
    draw_bmp_centered(ctx, s_bmp_fail_spaghetti, GPoint(bounds.size.w / 2 + 18, 48));
  } else if (p->status == PrinterStatus_Hotend && s_bmp_fail_flame) {
    draw_bmp_centered(ctx, s_bmp_fail_flame, GPoint(bounds.size.w / 2 + 18, 48));
  } else if (p->status == PrinterStatus_Filament && s_bmp_fail_empty) {
    draw_bmp_centered(ctx, s_bmp_fail_empty, GPoint(bounds.size.w / 2 + 18, 48));
  }

  graphics_context_set_text_color(ctx, status_color(p->status));
  if (p->job_name[0] && p->status != PrinterStatus_Idle) {
    snprintf(line, sizeof(line), "%s  need %s", p->job_name, filament_name(p->job_need));
  } else {
    snprintf(line, sizeof(line), "%s", status_name(p->status));
  }
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, 60, bounds.size.w - 4, 16), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  draw_progress_bar(ctx, GRect(10, 78, bounds.size.w - 20, 10), p->progress_pct);

  graphics_context_set_text_color(ctx, C_TEXT);
  const char *action = "";
  switch (p->status) {
    case PrinterStatus_Idle:
      snprintf(line, sizeof(line), "Load %s  SEL start", filament_name(p->filament));
      action = line;
      break;
    case PrinterStatus_Done:
      snprintf(line, sizeof(line), "SEL collect $%u", (unsigned)p->job_pay);
      action = line;
      break;
    case PrinterStatus_Jam:
      action = "SEL mash cut  DN scrap";
      break;
    case PrinterStatus_Filament:
      snprintf(line, sizeof(line), "Pick %s  SEL load", filament_name(f->filament_pick));
      action = line;
      break;
    case PrinterStatus_Bed:
      action = "SEL re-level  DN scrap";
      break;
    case PrinterStatus_Hotend:
      action = "SEL power-cycle  DN scrap";
      break;
    case PrinterStatus_Printing:
      snprintf(line, sizeof(line), "%u%%  pay $%u", (unsigned)p->progress_pct,
               (unsigned)p->job_pay);
      action = line;
      break;
    default:
      action = "BACK";
      break;
  }
  graphics_draw_text(ctx, action, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, 94, bounds.size.w - 4, 32), GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, C_DIM);
  const char *hint = f->banner[0] ? f->banner : "BACK: farm";
  graphics_draw_text(ctx, hint, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, bounds.size.h - 18, bounds.size.w - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Spool preview
  FilamentKind show = (p->status == PrinterStatus_Filament) ? f->filament_pick : p->filament;
  if (s_bmp_spool[show]) {
    draw_bmp(ctx, s_bmp_spool[show], GPoint(bounds.size.w - 20, 2));
  }
}

static void draw_shop(GContext *ctx, GRect bounds, const Farm *f) {
  char line[36];
  graphics_context_set_text_color(ctx, C_TEXT);
  graphics_draw_text(ctx, "SHOP", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(2, 0, 60, 20), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  format_money(line, sizeof(line), f->money);
  graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(60, 2, bounds.size.w - 64, 16), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  for (uint8_t i = 0; i < ShopItem_Count; i++) {
    int16_t y = (int16_t)(24 + i * 28);
    bool focused = (i == f->shop_focus);
    if (focused) {
      graphics_context_set_fill_color(ctx, C_PANEL);
      graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, 26), 0, GCornerNone);
    }
    uint16_t price = shop_price((ShopItem)i, f);
    graphics_context_set_text_color(ctx, focused ? C_ACCENT : C_TEXT);
    graphics_draw_text(ctx, shop_label((ShopItem)i),
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(4, y, bounds.size.w - 8, 14),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, C_DIM);
    switch ((ShopItem)i) {
      case ShopItem_BuyPrinter:
        snprintf(line, sizeof(line), "%u/%u  $%u", (unsigned)f->owned_count,
                 (unsigned)FARM_MAX_PRINTERS, (unsigned)price);
        break;
      case ShopItem_UpgradeTier:
        snprintf(line, sizeof(line), "+speed  $%u", (unsigned)price);
        break;
      case ShopItem_HireStaff:
        snprintf(line, sizeof(line), "Lv%u  $%u", (unsigned)f->staff_level,
                 (unsigned)price);
        break;
      case ShopItem_BuyExotic:
        snprintf(line, sizeof(line), price ? "glow  $%u" : "owned",
                 (unsigned)price);
        break;
      default:
        line[0] = '\0';
        break;
    }
    graphics_draw_text(ctx, line, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(4, y + 12, bounds.size.w - 8, 12),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  graphics_context_set_text_color(ctx, C_DIM);
  const char *hint = f->banner[0] ? f->banner : "SEL buy  BACK";
  graphics_draw_text(ctx, hint, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, bounds.size.h - 18, bounds.size.w - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  const Farm *f = game_get();
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, C_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  switch (f->screen) {
    case Screen_Title: draw_title(ctx, bounds); break;
    case Screen_Farm: draw_farm(ctx, bounds, f); break;
    case Screen_Detail: draw_detail(ctx, bounds, f); break;
    case Screen_Shop: draw_shop(ctx, bounds, f); break;
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  game_select();
  ui_mark_dirty();
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  const Farm *f = game_get();
  if (f->screen == Screen_Farm) game_focus_up();
  else if (f->screen == Screen_Detail) game_detail_up();
  else if (f->screen == Screen_Shop) game_shop_up();
  ui_mark_dirty();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  const Farm *f = game_get();
  if (f->screen == Screen_Farm) game_focus_down();
  else if (f->screen == Screen_Detail) game_detail_down();
  else if (f->screen == Screen_Shop) game_shop_down();
  else if (f->screen == Screen_Title) game_select();
  ui_mark_dirty();
}

static void back_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  const Farm *f = game_get();
  if (f->screen == Screen_Title) {
    window_stack_pop(true);
    return;
  }
  game_back();
  ui_mark_dirty();
}

static void click_config(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 0, 0, true, select_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  s_bmp_title = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TITLE);
  s_bmp_printer[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PRINTER_BASIC);
  s_bmp_printer[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PRINTER_COREXY);
  s_bmp_printer[2] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PRINTER_INDUSTRIAL);
  s_bmp_spool[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPOOL_PLA);
  s_bmp_spool[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPOOL_PETG);
  s_bmp_spool[2] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPOOL_ABS);
  s_bmp_spool[3] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPOOL_TPU);
  s_bmp_spool[4] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SPOOL_EXOTIC);
  s_bmp_status_ok = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_OK);
  s_bmp_status_jam = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_JAM);
  s_bmp_status_heat = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_HEAT);
  s_bmp_status_warn = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_WARN);
  s_bmp_fail_spaghetti = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FAIL_SPAGHETTI);
  s_bmp_fail_flame = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FAIL_FLAME);
  s_bmp_fail_empty = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FAIL_EMPTY);

  s_redraw = app_timer_register(200, redraw_cb, NULL);
}

static void destroy_bmp(GBitmap **b) {
  if (*b) { gbitmap_destroy(*b); *b = NULL; }
}

static void window_unload(Window *window) {
  (void)window;
  if (s_redraw) {
    app_timer_cancel(s_redraw);
    s_redraw = NULL;
  }
  destroy_bmp(&s_bmp_title);
  for (int i = 0; i < 3; i++) destroy_bmp(&s_bmp_printer[i]);
  for (int i = 0; i < 5; i++) destroy_bmp(&s_bmp_spool[i]);
  destroy_bmp(&s_bmp_status_ok);
  destroy_bmp(&s_bmp_status_jam);
  destroy_bmp(&s_bmp_status_heat);
  destroy_bmp(&s_bmp_status_warn);
  destroy_bmp(&s_bmp_fail_spaghetti);
  destroy_bmp(&s_bmp_fail_flame);
  destroy_bmp(&s_bmp_fail_empty);
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

void ui_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, C_BG);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
}

void ui_deinit(void) {
  window_destroy(s_window);
  s_window = NULL;
}

void ui_mark_dirty(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}
