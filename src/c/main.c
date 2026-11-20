#include <pebble.h>
#include "dial.h"
#include "location.h"
#include "persist.h"
#include "render.h"
#include "solar.h"
#include "sundial.h"

static Window *s_window;
static Layer *s_layer;

static Dial s_dial;
static SolarState s_solar;
static Vec3q16 s_sun;
static Location s_loc = {514779, -15, 0, false};  /* default Greenwich */

static void redraw(void) {
  solar_state(time(NULL), s_loc.lon_e4, &s_solar);
  dial_init(&s_dial, s_loc.lat_e4);
  s_sun = sun_vector(&s_solar, s_loc.lat_e4);
  layer_mark_dirty(s_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units) {
  (void)tick_time;
  (void)units;
  redraw();
}

static void request_location(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_LOCATION, 1);
    app_message_outbox_send();
  }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  Tuple *lat = dict_find(iter, MESSAGE_KEY_LAT_E4);
  Tuple *lon = dict_find(iter, MESSAGE_KEY_LON_E4);
  Tuple *src = dict_find(iter, MESSAGE_KEY_LOCATION_SOURCE);
  if (!lat || !lon) return;
  if (!location_valid(lat->value->int32, lon->value->int32)) return;

  s_loc.lat_e4 = lat->value->int32;
  s_loc.lon_e4 = lon->value->int32;
  s_loc.source = src ? src->value->uint8 : 0;
  s_loc.valid = true;
  persist_location_save(&s_loc);
  redraw();
}

static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  render_draw(ctx, bounds, &s_dial, &s_solar, s_sun,
              s_loc.valid && dial_in_band(s_loc.lat_e4));
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, draw);
  layer_add_child(root, s_layer);
  redraw();
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
}

static void bluetooth_handler(bool connected) {
  if (connected) request_location();
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  /* Load persisted location; fall back to Greenwich default. */
  if (!persist_location_load(&s_loc)) {
    s_loc.lat_e4 = 514779;
    s_loc.lon_e4 = -15;
    s_loc.source = 0;
    s_loc.valid = false;
  }

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = bluetooth_handler,
  });

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
  request_location();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
