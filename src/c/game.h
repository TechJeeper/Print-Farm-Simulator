#pragma once

#include <pebble.h>

#define FARM_MAX_PRINTERS     6
#define FARM_TICK_MS          500
#define FARM_JAM_MASH_NEED    5
#define FARM_BED_HOLD_MS      1800
#define FARM_FILAMENT_KINDS   5
#define FARM_JOB_NAME_LEN     16

typedef enum {
  Screen_Title = 0,
  Screen_Farm,
  Screen_Detail,
  Screen_Shop,
} FarmScreen;

typedef enum {
  PrinterStatus_Offline = 0,
  PrinterStatus_Idle,
  PrinterStatus_Printing,
  PrinterStatus_Done,
  PrinterStatus_Jam,
  PrinterStatus_Filament,
  PrinterStatus_Bed,
  PrinterStatus_Hotend,
} PrinterStatus;

typedef enum {
  PrinterTier_Basic = 0,
  PrinterTier_CoreXY,
  PrinterTier_Industrial,
} PrinterTier;

typedef enum {
  Filament_PLA = 0,
  Filament_PETG,
  Filament_ABS,
  Filament_TPU,
  Filament_Exotic,
} FilamentKind;

typedef enum {
  ShopItem_BuyPrinter = 0,
  ShopItem_UpgradeTier,
  ShopItem_HireStaff,
  ShopItem_BuyExotic,
  ShopItem_Count,
} ShopItem;

typedef struct __attribute__((packed)) {
  uint8_t owned;
  uint8_t tier;              // PrinterTier
  uint8_t status;            // PrinterStatus
  uint8_t filament;          // FilamentKind
  uint8_t job_need;          // required filament for current job
  char job_name[FARM_JOB_NAME_LEN];
  uint16_t job_pay;          // payout on collect
  uint32_t job_duration_s;   // total print seconds
  uint32_t job_started_at;   // wall-clock start
  uint8_t progress_pct;      // 0–100 cached
  uint8_t jam_mash;          // spaghetti mash counter
  uint16_t bed_hold_ms;      // re-level hold progress
  uint8_t is_contract;       // daily/overnight contract
} Printer;

typedef struct {
  FarmScreen screen;
  Printer printers[FARM_MAX_PRINTERS];
  uint8_t owned_count;
  uint8_t focus;             // farm list focus (0..owned_count) ; owned_count == SHOP row
  uint8_t detail_idx;
  uint8_t shop_focus;
  FilamentKind filament_pick; // for filament crisis picker
  uint32_t money;
  uint16_t reputation;       // 0–1000
  uint8_t staff_level;       // 0 none, 1 junior, 2 senior
  bool exotic_unlocked;
  uint32_t day_stamp;        // YYYYMMDD-ish from time for daily contract
  bool contract_claimed;     // claimed today's overnight contract
  bool contract_active;
  char banner[28];
  uint16_t banner_ms;
  bool quiet_hours;          // from settings mirror
  uint8_t crisis_freq;       // 0 low, 1 normal, 2 high
  bool vibe_enabled;
} Farm;

void game_init(void);
void game_deinit(void);
void game_tick(void);
const Farm *game_get(void);

void game_set_vibe_enabled(bool on);
void game_set_crisis_freq(uint8_t freq);
void game_set_quiet_hours(bool on);

void game_enter_farm(void);
void game_focus_up(void);
void game_focus_down(void);
void game_select(void);
void game_back(void);
void game_detail_up(void);
void game_detail_down(void);
void game_shop_up(void);
void game_shop_down(void);

const char *filament_name(FilamentKind k);
const char *tier_name(PrinterTier t);
const char *status_name(PrinterStatus s);
uint16_t shop_price(ShopItem item, const Farm *f);
const char *shop_label(ShopItem item);
