#include "settings.h"
#include "game.h"

#include <stdlib.h>

static int32_t tuple_to_int(Tuple *t) {
  if (!t) return -1;
  if (t->type == TUPLE_INT) return t->value->int32;
  if (t->type == TUPLE_UINT) return (int32_t)t->value->uint32;
  if (t->type == TUPLE_CSTRING) return (int32_t)atoi(t->value->cstring);
  return -1;
}

static void inbox_received(DictionaryIterator *it, void *context) {
  (void)context;

  Tuple *vibe = dict_find(it, MESSAGE_KEY_VibeEnabled);
  if (vibe) {
    int32_t v = tuple_to_int(vibe);
    if (v >= 0) game_set_vibe_enabled(v != 0);
  }

  Tuple *freq = dict_find(it, MESSAGE_KEY_CrisisFrequency);
  if (freq) {
    int32_t v = tuple_to_int(freq);
    if (v >= 0) game_set_crisis_freq((uint8_t)v);
  }

  Tuple *quiet = dict_find(it, MESSAGE_KEY_QuietHours);
  if (quiet) {
    int32_t v = tuple_to_int(quiet);
    if (v >= 0) game_set_quiet_hours(v != 0);
  }
}

void settings_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 64);
}

void settings_deinit(void) {
}
