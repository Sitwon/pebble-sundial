#include <pebble.h>
#include "dial.h"
#include "render.h"
#include "solar.h"
#include "sundial.h"

static Window *s_window;
static Layer *s_layer;

static Dial s_dial;
static SolarState s_solar;
static Vec3q16 s_sun;
static Location s_loc = {514779, -15, 0, false};  /* Greenwich until phase 6 */

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

static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  render_draw(ctx, bounds, &s_dial, &s_solar, s_sun, s_loc.valid);
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

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
