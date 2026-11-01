#include <pebble.h>
#include "game.h"
#include "settings.h"
#include "ui.h"

static void init(void) {
  game_init();
  settings_init();
  ui_init();
}

static void deinit(void) {
  ui_deinit();
  settings_deinit();
  game_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
