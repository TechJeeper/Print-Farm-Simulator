#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PERSIST_VERSION 1
#define KEY_VERSION     1
#define KEY_MONEY       2
#define KEY_REP         3
#define KEY_OWNED       4
#define KEY_STAFF       5
#define KEY_EXOTIC      6
#define KEY_DAY         7
#define KEY_CONTRACT    8
#define KEY_PRINTERS    9

static Farm s_farm;
static AppTimer *s_timer = NULL;

static const char *JOB_NAMES[] = {
  "Benchy", "Phone stand", "Dragon", "Cosplay", "Gear set",
  "Keycap", "Planter", "D20", "Cable clip", "Figurine"
};
static const uint8_t JOB_NAME_COUNT = 10;

const char *filament_name(FilamentKind k) {
  switch (k) {
    case Filament_PLA: return "PLA";
    case Filament_PETG: return "PETG";
    case Filament_ABS: return "ABS";
    case Filament_TPU: return "TPU";
    case Filament_Exotic: return "XTC";
    default: return "?";
  }
}

const char *tier_name(PrinterTier t) {
  switch (t) {
    case PrinterTier_Basic: return "Ender";
    case PrinterTier_CoreXY: return "CoreXY";
    case PrinterTier_Industrial: return "Indust.";
    default: return "?";
  }
}

const char *status_name(PrinterStatus s) {
  switch (s) {
    case PrinterStatus_Offline: return "OFF";
    case PrinterStatus_Idle: return "idle";
    case PrinterStatus_Printing: return "prn";
    case PrinterStatus_Done: return "DONE";
    case PrinterStatus_Jam: return "JAM";
    case PrinterStatus_Filament: return "FIL";
    case PrinterStatus_Bed: return "BED";
    case PrinterStatus_Hotend: return "HOT";
    default: return "?";
  }
}

uint16_t shop_price(ShopItem item, const Farm *f) {
  switch (item) {
    case ShopItem_BuyPrinter: {
      // Garage unlocks at 3, shop at 6. Prices scale with owned.
      if (f->owned_count >= FARM_MAX_PRINTERS) return 0;
      return (uint16_t)(400 + f->owned_count * 350);
    }
    case ShopItem_UpgradeTier:
      return 600;
    case ShopItem_HireStaff:
      if (f->staff_level >= 2) return 0;
      return (uint16_t)(500 + f->staff_level * 700);
    case ShopItem_BuyExotic:
      return f->exotic_unlocked ? 0 : 1200;
    default:
      return 0;
  }
}

const char *shop_label(ShopItem item) {
  switch (item) {
    case ShopItem_BuyPrinter: return "Buy printer";
    case ShopItem_UpgradeTier: return "Upgrade tier";
    case ShopItem_HireStaff: return "Hire staff";
    case ShopItem_BuyExotic: return "Unlock exotic";
    default: return "";
  }
}

static uint32_t now_s(void) {
  return (uint32_t)time(NULL);
}

static uint32_t day_stamp_now(void) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  if (!tm) return 0;
  return (uint32_t)((tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday);
}

static void set_banner(const char *msg) {
  strncpy(s_farm.banner, msg, sizeof(s_farm.banner) - 1);
  s_farm.banner[sizeof(s_farm.banner) - 1] = '\0';
  s_farm.banner_ms = 2200;
}

static bool vibe_ok(void) {
  if (!s_farm.vibe_enabled) return false;
  if (!s_farm.quiet_hours) return true;
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  if (!tm) return true;
  // Quiet 22:00–07:00
  return !(tm->tm_hour >= 22 || tm->tm_hour < 7);
}

static void vibe_soft(void) {
  if (vibe_ok()) vibes_short_pulse();
}

static void vibe_crisis(void) {
  if (vibe_ok()) vibes_double_pulse();
}

static uint8_t max_filament_kind(void) {
  if (s_farm.exotic_unlocked) return Filament_Exotic;
  if (s_farm.reputation >= 600) return Filament_TPU;
  if (s_farm.reputation >= 350) return Filament_ABS;
  if (s_farm.reputation >= 150) return Filament_PETG;
  return Filament_PLA;
}

static FilamentKind random_filament(void) {
  uint8_t maxk = max_filament_kind();
  return (FilamentKind)(rand() % (maxk + 1));
}

static uint16_t base_pay_for(FilamentKind need, bool contract) {
  uint16_t pay = 40;
  switch (need) {
    case Filament_PLA: pay = 50; break;
    case Filament_PETG: pay = 80; break;
    case Filament_ABS: pay = 110; break;
    case Filament_TPU: pay = 150; break;
    case Filament_Exotic: pay = 220; break;
  }
  if (contract) pay = (uint16_t)(pay * 3);
  // Reputation bonus
  pay = (uint16_t)(pay + s_farm.reputation / 40);
  return pay;
}

static uint32_t duration_for(PrinterTier tier, FilamentKind need, bool contract) {
  uint32_t secs = 90;
  switch (need) {
    case Filament_PLA: secs = 75; break;
    case Filament_PETG: secs = 100; break;
    case Filament_ABS: secs = 120; break;
    case Filament_TPU: secs = 140; break;
    case Filament_Exotic: secs = 160; break;
  }
  if (tier == PrinterTier_CoreXY) secs = (secs * 70) / 100;
  if (tier == PrinterTier_Industrial) secs = (secs * 50) / 100;
  if (contract) secs *= 4; // overnight-ish when idle; still playable in session
  if (secs < 30) secs = 30;
  return secs;
}

static void start_job_on(Printer *p, bool contract) {
  FilamentKind need = random_filament();
  const char *name = JOB_NAMES[rand() % JOB_NAME_COUNT];
  strncpy(p->job_name, name, FARM_JOB_NAME_LEN - 1);
  p->job_name[FARM_JOB_NAME_LEN - 1] = '\0';
  p->job_need = need;
  // Keep currently loaded filament; matching pays more (applied by caller or below).
  p->job_pay = base_pay_for(need, contract);
  if (p->filament == need) {
    p->job_pay = (uint16_t)(p->job_pay + 15);
  }
  p->job_duration_s = duration_for(p->tier, need, contract);
  p->job_started_at = now_s();
  p->progress_pct = 0;
  p->jam_mash = 0;
  p->bed_hold_ms = 0;
  p->is_contract = contract;
  p->status = PrinterStatus_Printing;
}

static void scrap_job(Printer *p) {
  if (s_farm.reputation > 15) s_farm.reputation = (uint16_t)(s_farm.reputation - 15);
  else s_farm.reputation = 0;
  if (p->is_contract) s_farm.contract_active = false;
  p->status = PrinterStatus_Idle;
  p->job_name[0] = '\0';
  p->progress_pct = 0;
  p->jam_mash = 0;
  p->bed_hold_ms = 0;
  p->is_contract = false;
  set_banner("Job scrapped");
}

static void collect_done(Printer *p) {
  s_farm.money += p->job_pay;
  uint16_t rep_gain = (uint16_t)(8 + p->tier * 4 + (p->is_contract ? 25 : 0));
  if (s_farm.reputation + rep_gain > 1000) s_farm.reputation = 1000;
  else s_farm.reputation = (uint16_t)(s_farm.reputation + rep_gain);
  if (p->is_contract) s_farm.contract_active = false;
  char buf[28];
  snprintf(buf, sizeof(buf), "+$%u rep", (unsigned)p->job_pay);
  set_banner(buf);
  vibe_soft();
  p->status = PrinterStatus_Idle;
  p->job_name[0] = '\0';
  p->progress_pct = 0;
  p->is_contract = false;
}

static uint8_t crisis_chance_pct(PrinterTier tier) {
  // Per tick (~0.5s) chance while printing. Scaled by settings.
  uint8_t base = 2;
  if (s_farm.crisis_freq == 0) base = 1;
  if (s_farm.crisis_freq == 2) base = 4;
  if (tier == PrinterTier_CoreXY && base > 1) base--;
  if (tier == PrinterTier_Industrial && base > 1) base = (uint8_t)(base > 2 ? base - 2 : 1);
  return base;
}

static void trigger_crisis(Printer *p) {
  int roll = rand() % 100;
  if (roll < 40) p->status = PrinterStatus_Jam;
  else if (roll < 70) p->status = PrinterStatus_Filament;
  else if (roll < 88) p->status = PrinterStatus_Bed;
  else p->status = PrinterStatus_Hotend;
  p->jam_mash = 0;
  p->bed_hold_ms = 0;
  s_farm.filament_pick = p->job_need;
  vibe_crisis();
}

static void staff_try_fix(void) {
  if (s_farm.staff_level == 0) return;
  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    Printer *p = &s_farm.printers[i];
    if (!p->owned) continue;
    // Junior clears filament only; senior also clears jam & bed
    if (p->status == PrinterStatus_Filament) {
      p->filament = p->job_need;
      p->status = PrinterStatus_Printing;
      p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
      set_banner("Staff reloaded");
      return; // one fix per tick
    }
    if (s_farm.staff_level >= 2) {
      if (p->status == PrinterStatus_Jam || p->status == PrinterStatus_Bed) {
        p->status = PrinterStatus_Printing;
        p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
        p->jam_mash = 0;
        p->bed_hold_ms = 0;
        set_banner("Staff fixed");
        return;
      }
    }
  }
}

static void update_printer_progress(Printer *p) {
  if (!p->owned) return;
  if (p->status != PrinterStatus_Printing) return;

  uint32_t elapsed = now_s() - p->job_started_at;
  if (elapsed >= p->job_duration_s) {
    p->progress_pct = 100;
    p->status = PrinterStatus_Done;
    vibe_soft();
    return;
  }
  p->progress_pct = (uint8_t)((elapsed * 100) / p->job_duration_s);

  // Crisis roll
  if ((rand() % 100) < crisis_chance_pct(p->tier)) {
    // Don't crisis in first 10%
    if (p->progress_pct >= 10 && p->progress_pct < 95) {
      trigger_crisis(p);
    }
  }
}

static void ensure_capacity_unlock(void) {
  // Progression fantasy: bed=1, garage=3, shop=6 via buying.
  // Starting owned_count=1 is Bed; buying grows farm.
  (void)0;
}

static void save_state(void) {
  persist_write_int(KEY_VERSION, PERSIST_VERSION);
  persist_write_int(KEY_MONEY, (int)s_farm.money);
  persist_write_int(KEY_REP, (int)s_farm.reputation);
  persist_write_int(KEY_OWNED, (int)s_farm.owned_count);
  persist_write_int(KEY_STAFF, (int)s_farm.staff_level);
  persist_write_bool(KEY_EXOTIC, s_farm.exotic_unlocked);
  persist_write_int(KEY_DAY, (int)s_farm.day_stamp);
  persist_write_bool(KEY_CONTRACT, s_farm.contract_claimed);
  persist_write_data(KEY_PRINTERS, s_farm.printers, sizeof(s_farm.printers));
}

static void load_state(void) {
  if (!persist_exists(KEY_VERSION) || persist_read_int(KEY_VERSION) != PERSIST_VERSION) {
    return;
  }
  s_farm.money = (uint32_t)persist_read_int(KEY_MONEY);
  s_farm.reputation = (uint16_t)persist_read_int(KEY_REP);
  s_farm.owned_count = (uint8_t)persist_read_int(KEY_OWNED);
  s_farm.staff_level = (uint8_t)persist_read_int(KEY_STAFF);
  s_farm.exotic_unlocked = persist_read_bool(KEY_EXOTIC);
  s_farm.day_stamp = (uint32_t)persist_read_int(KEY_DAY);
  s_farm.contract_claimed = persist_read_bool(KEY_CONTRACT);
  if (persist_exists(KEY_PRINTERS)) {
    persist_read_data(KEY_PRINTERS, s_farm.printers, sizeof(s_farm.printers));
  }
  if (s_farm.owned_count < 1) s_farm.owned_count = 1;
  if (s_farm.owned_count > FARM_MAX_PRINTERS) s_farm.owned_count = FARM_MAX_PRINTERS;
  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    s_farm.printers[i].owned = (i < s_farm.owned_count) ? 1 : 0;
  }
}

static void init_new_farm(void) {
  memset(&s_farm, 0, sizeof(s_farm));
  s_farm.screen = Screen_Title;
  s_farm.money = 200;
  s_farm.reputation = 0;
  s_farm.owned_count = 1;
  s_farm.vibe_enabled = true;
  s_farm.crisis_freq = 1;
  s_farm.printers[0].owned = true;
  s_farm.printers[0].tier = PrinterTier_Basic;
  s_farm.printers[0].status = PrinterStatus_Idle;
  s_farm.printers[0].filament = Filament_PLA;
  s_farm.day_stamp = day_stamp_now();
}

static void offer_daily_contract_if_needed(void) {
  uint32_t today = day_stamp_now();
  if (today != s_farm.day_stamp) {
    s_farm.day_stamp = today;
    s_farm.contract_claimed = false;
    s_farm.contract_active = false;
  }
}

static void assign_contract_to_idle(void) {
  if (s_farm.contract_claimed || s_farm.contract_active) return;
  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    Printer *p = &s_farm.printers[i];
    if (p->owned && p->status == PrinterStatus_Idle) {
      start_job_on(p, true);
      p->filament = p->job_need; // overnight ships with correct filament
      s_farm.contract_active = true;
      s_farm.contract_claimed = true;
      set_banner("Overnight job!");
      return;
    }
  }
}

static void timer_cb(void *ctx);

static void schedule_tick(void) {
  s_timer = app_timer_register(FARM_TICK_MS, timer_cb, NULL);
}

static void timer_cb(void *ctx) {
  (void)ctx;
  s_timer = NULL;
  game_tick();
  schedule_tick();
}

void game_init(void) {
  srand((unsigned)time(NULL));
  init_new_farm();
  load_state();
  offer_daily_contract_if_needed();
  // Advance wall-clock progress for any mid-print jobs
  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    update_printer_progress(&s_farm.printers[i]);
  }
  schedule_tick();
}

void game_deinit(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  save_state();
}

void game_tick(void) {
  if (s_farm.banner_ms > FARM_TICK_MS) {
    s_farm.banner_ms = (uint16_t)(s_farm.banner_ms - FARM_TICK_MS);
  } else if (s_farm.banner_ms > 0) {
    s_farm.banner_ms = 0;
    s_farm.banner[0] = '\0';
  }

  offer_daily_contract_if_needed();

  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    update_printer_progress(&s_farm.printers[i]);
  }
  staff_try_fix();

  // Auto-offer daily contract once per day when farm open
  if (s_farm.screen != Screen_Title) {
    assign_contract_to_idle();
  }

  // Periodic save
  static uint8_t save_div;
  if (++save_div >= 20) { // ~10s
    save_div = 0;
    save_state();
  }
}

const Farm *game_get(void) {
  return &s_farm;
}

void game_set_vibe_enabled(bool on) { s_farm.vibe_enabled = on; }
void game_set_crisis_freq(uint8_t freq) { s_farm.crisis_freq = freq > 2 ? 1 : freq; }
void game_set_quiet_hours(bool on) { s_farm.quiet_hours = on; }

void game_enter_farm(void) {
  s_farm.screen = Screen_Farm;
  s_farm.focus = 0;
  assign_contract_to_idle();
  set_banner("Farm online");
}

static uint8_t farm_rows(void) {
  // printers + shop row
  return (uint8_t)(s_farm.owned_count + 1);
}

void game_focus_up(void) {
  if (s_farm.screen != Screen_Farm) return;
  if (s_farm.focus == 0) s_farm.focus = (uint8_t)(farm_rows() - 1);
  else s_farm.focus--;
}

void game_focus_down(void) {
  if (s_farm.screen != Screen_Farm) return;
  s_farm.focus++;
  if (s_farm.focus >= farm_rows()) s_farm.focus = 0;
}

static void buy_printer(void) {
  uint16_t price = shop_price(ShopItem_BuyPrinter, &s_farm);
  if (price == 0) {
    set_banner("Farm full");
    return;
  }
  // Cap by progression: need rep for >3
  if (s_farm.owned_count >= 3 && s_farm.reputation < 200) {
    set_banner("Need more rep");
    return;
  }
  if (s_farm.owned_count >= 5 && s_farm.reputation < 500) {
    set_banner("Need more rep");
    return;
  }
  if (s_farm.money < price) {
    set_banner("Can't afford");
    return;
  }
  s_farm.money -= price;
  uint8_t idx = s_farm.owned_count;
  s_farm.printers[idx].owned = true;
  s_farm.printers[idx].tier = PrinterTier_Basic;
  s_farm.printers[idx].status = PrinterStatus_Idle;
  s_farm.printers[idx].filament = Filament_PLA;
  s_farm.owned_count++;
  ensure_capacity_unlock();
  set_banner("Printer added");
}

static void upgrade_focused_or_cheapest(void) {
  uint16_t price = shop_price(ShopItem_UpgradeTier, &s_farm);
  // Upgrade the lowest-tier owned printer
  int8_t target = -1;
  PrinterTier lowest = PrinterTier_Industrial;
  for (uint8_t i = 0; i < FARM_MAX_PRINTERS; i++) {
    if (!s_farm.printers[i].owned) continue;
    if (s_farm.printers[i].tier < lowest) {
      lowest = s_farm.printers[i].tier;
      target = (int8_t)i;
    }
  }
  if (target < 0 || lowest >= PrinterTier_Industrial) {
    set_banner("Max tier");
    return;
  }
  if (s_farm.money < price) {
    set_banner("Can't afford");
    return;
  }
  s_farm.money -= price;
  s_farm.printers[target].tier = (PrinterTier)(lowest + 1);
  char buf[28];
  snprintf(buf, sizeof(buf), "P%u -> %s", (unsigned)(target + 1),
           tier_name(s_farm.printers[target].tier));
  set_banner(buf);
}

static void hire_staff(void) {
  uint16_t price = shop_price(ShopItem_HireStaff, &s_farm);
  if (price == 0) {
    set_banner("Staff max");
    return;
  }
  if (s_farm.money < price) {
    set_banner("Can't afford");
    return;
  }
  s_farm.money -= price;
  s_farm.staff_level++;
  set_banner(s_farm.staff_level == 1 ? "Junior hired" : "Senior hired");
}

static void unlock_exotic(void) {
  uint16_t price = shop_price(ShopItem_BuyExotic, &s_farm);
  if (price == 0) {
    set_banner("Have exotic");
    return;
  }
  if (s_farm.reputation < 400) {
    set_banner("Need 400 rep");
    return;
  }
  if (s_farm.money < price) {
    set_banner("Can't afford");
    return;
  }
  s_farm.money -= price;
  s_farm.exotic_unlocked = true;
  set_banner("Exotic unlocked");
}

static void shop_buy(void) {
  switch ((ShopItem)s_farm.shop_focus) {
    case ShopItem_BuyPrinter: buy_printer(); break;
    case ShopItem_UpgradeTier: upgrade_focused_or_cheapest(); break;
    case ShopItem_HireStaff: hire_staff(); break;
    case ShopItem_BuyExotic: unlock_exotic(); break;
    default: break;
  }
  save_state();
}

void game_shop_up(void) {
  if (s_farm.shop_focus == 0) s_farm.shop_focus = (uint8_t)(ShopItem_Count - 1);
  else s_farm.shop_focus--;
}

void game_shop_down(void) {
  s_farm.shop_focus++;
  if (s_farm.shop_focus >= ShopItem_Count) s_farm.shop_focus = 0;
}

void game_detail_up(void) {
  Printer *p = &s_farm.printers[s_farm.detail_idx];
  if (p->status == PrinterStatus_Filament) {
    uint8_t maxk = max_filament_kind();
    if (s_farm.filament_pick == 0) s_farm.filament_pick = (FilamentKind)maxk;
    else s_farm.filament_pick = (FilamentKind)(s_farm.filament_pick - 1);
  } else if (p->status == PrinterStatus_Idle) {
    // Cycle filament loaded for next job preference
    uint8_t maxk = max_filament_kind();
    if (p->filament == 0) p->filament = (FilamentKind)maxk;
    else p->filament = (FilamentKind)(p->filament - 1);
  }
}

void game_detail_down(void) {
  Printer *p = &s_farm.printers[s_farm.detail_idx];
  if (p->status == PrinterStatus_Filament) {
    uint8_t maxk = max_filament_kind();
    s_farm.filament_pick = (FilamentKind)((s_farm.filament_pick + 1) % (maxk + 1));
  } else if (p->status == PrinterStatus_Idle) {
    uint8_t maxk = max_filament_kind();
    p->filament = (FilamentKind)((p->filament + 1) % (maxk + 1));
  } else if (p->status == PrinterStatus_Jam || p->status == PrinterStatus_Bed ||
             p->status == PrinterStatus_Hotend) {
    scrap_job(p);
  }
}

static void detail_select(void) {
  Printer *p = &s_farm.printers[s_farm.detail_idx];
  switch (p->status) {
    case PrinterStatus_Idle:
      start_job_on(p, false);
      if (p->filament != p->job_need) {
        // Mismatch: lower pay, slightly riskier (pay already set in start_job_on)
        p->job_pay = (uint16_t)(p->job_pay > 25 ? p->job_pay - 25 : 10);
      }
      set_banner(p->job_name);
      break;
    case PrinterStatus_Done:
      collect_done(p);
      break;
    case PrinterStatus_Jam:
      p->jam_mash++;
      if (p->jam_mash >= FARM_JAM_MASH_NEED) {
        p->status = PrinterStatus_Printing;
        p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
        p->jam_mash = 0;
        set_banner("Cleared jam");
      } else {
        char buf[28];
        snprintf(buf, sizeof(buf), "Cut %u/%u", (unsigned)p->jam_mash,
                 (unsigned)FARM_JAM_MASH_NEED);
        set_banner(buf);
      }
      break;
    case PrinterStatus_Filament:
      p->filament = s_farm.filament_pick;
      if (p->filament == p->job_need) {
        p->status = PrinterStatus_Printing;
        p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
        set_banner("Filament loaded");
      } else {
        set_banner("Wrong filament!");
        // slight rep ding and stay in crisis
        if (s_farm.reputation > 5) s_farm.reputation -= 5;
      }
      break;
    case PrinterStatus_Bed:
      p->bed_hold_ms = (uint16_t)(p->bed_hold_ms + 350);
      if (p->bed_hold_ms >= FARM_BED_HOLD_MS) {
        p->status = PrinterStatus_Printing;
        p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
        p->bed_hold_ms = 0;
        set_banner("Bed leveled");
      } else {
        char buf[28];
        snprintf(buf, sizeof(buf), "Level %u%%",
                 (unsigned)((p->bed_hold_ms * 100) / FARM_BED_HOLD_MS));
        set_banner(buf);
      }
      break;
    case PrinterStatus_Hotend:
      // Power cycle
      p->status = PrinterStatus_Printing;
      p->job_started_at = now_s() - (p->job_duration_s * p->progress_pct) / 100;
      set_banner("Power cycled");
      break;
    case PrinterStatus_Printing:
      set_banner("Printing…");
      break;
    default:
      break;
  }
}

void game_select(void) {
  switch (s_farm.screen) {
    case Screen_Title:
      game_enter_farm();
      break;
    case Screen_Farm:
      if (s_farm.focus >= s_farm.owned_count) {
        s_farm.screen = Screen_Shop;
        s_farm.shop_focus = 0;
      } else {
        s_farm.detail_idx = s_farm.focus;
        s_farm.filament_pick = s_farm.printers[s_farm.detail_idx].job_need;
        s_farm.screen = Screen_Detail;
      }
      break;
    case Screen_Detail:
      detail_select();
      break;
    case Screen_Shop:
      shop_buy();
      break;
  }
}

void game_back(void) {
  switch (s_farm.screen) {
    case Screen_Detail:
    case Screen_Shop:
      s_farm.screen = Screen_Farm;
      save_state();
      break;
    case Screen_Farm:
      s_farm.screen = Screen_Title;
      save_state();
      break;
    default:
      break;
  }
}
