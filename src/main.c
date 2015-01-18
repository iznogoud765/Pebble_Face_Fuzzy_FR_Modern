#include <pebble.h>
  
#include "french_time.h"

#define ANIMATION_DURATION 800
#define LINE_BUFFER_SIZE 50
#define WINDOW_NAME "fuzzy_french_plus"

static Window *s_main_window;

static GFont s_time_font;
static GFont s_time_font_big;

typedef struct {
  TextLayer *layer[2];
  //PropertyAnimation layer_animation[2];
} TextLine;

typedef struct {
  char line1[LINE_BUFFER_SIZE];
  char line2[LINE_BUFFER_SIZE];
  char line3[LINE_BUFFER_SIZE];
  char topbar[LINE_BUFFER_SIZE];
  char bottombar[LINE_BUFFER_SIZE];
} TheTime;

TextLine line1;
TextLine line2;
TextLine line3;
TextLine topbar;
TextLine bottombar;

static TheTime cur_time;
static TheTime new_time;

static bool busy_animating_in = false;
static bool busy_animating_out = false;
const int line1_y = 10;
const int line2_y = 68;
const int line3_y = 100;



void animationInStoppedHandler(struct Animation *animation, bool finished, void *context) {
  busy_animating_in = false;
  // reset cur_time
  cur_time = new_time;
}

void animationOutStoppedHandler(struct Animation *animation, bool finished, void *context) {
  // reset out layer to x=144
  TextLayer *outside = (TextLayer *)context;
  GRect rect = layer_get_frame(text_layer_get_layer(outside));
  if (rect.origin.y == line2_y) rect.origin.x = -144;
  else rect.origin.x = 144;
  layer_set_frame(text_layer_get_layer(outside), rect);

  busy_animating_out = false;
}

void updateLayer(TextLine *animating_line, int line) {

  TextLayer *inside, *outside;
  GRect rect = layer_get_frame(text_layer_get_layer(animating_line->layer[0]));

  inside = (rect.origin.x == 0) ? animating_line->layer[0] : animating_line->layer[1];
  outside = (inside == animating_line->layer[0]) ? animating_line->layer[1] : animating_line->layer[0];

  GRect in_rect = layer_get_frame(text_layer_get_layer(outside));
  GRect out_rect = layer_get_frame(text_layer_get_layer(inside));

  if (line == 2) {
    in_rect.origin.x += 144;
    out_rect.origin.x += 144;
  } else {
    in_rect.origin.x -= 144;
    out_rect.origin.x -= 144;
  }

 // animate out current layer
  busy_animating_out = true;
  PropertyAnimation *animate_out;
  animate_out = property_animation_create_layer_frame(text_layer_get_layer(inside), NULL, &out_rect);
  animation_set_duration((Animation*)animate_out, ANIMATION_DURATION);
  animation_set_curve((Animation*)animate_out, AnimationCurveEaseOut);
  animation_set_handlers((Animation*)animate_out, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler)animationOutStoppedHandler
  }, (void *)inside);
  animation_schedule((Animation*)animate_out);

  if (line==1){
    text_layer_set_text(outside, new_time.line1);
    text_layer_set_text(inside, cur_time.line1);
  }
  if (line==2){
    text_layer_set_text(outside, new_time.line2);
    text_layer_set_text(inside, cur_time.line2);
  }
  if (line==3){
    text_layer_set_text(outside, new_time.line3);
    text_layer_set_text(inside, cur_time.line3);
  }

  // animate in new layer
  busy_animating_in = true;
  PropertyAnimation *animate_in;
  animate_in = property_animation_create_layer_frame(text_layer_get_layer(outside), NULL, &in_rect);
  animation_set_duration((Animation*)animate_in, ANIMATION_DURATION);
  animation_set_curve((Animation*)animate_in, AnimationCurveEaseOut);
  animation_set_handlers((Animation*)animate_in, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler)animationInStoppedHandler
  }, (void *)outside);
  animation_schedule((Animation*)animate_in);
}

void update_watch(struct tm* t) {
  // Let's get the new text date
  info_lines(t, new_time.topbar, new_time.bottombar);

  // Let's update the top bar
  //if(strcmp(new_time.topbar, cur_time.topbar) != 0) text_layer_set_text(topbar.layer[0], new_time.topbar);
  // Let's update the bottom bar
  text_layer_set_text(bottombar.layer[0], new_time.bottombar);

  // Let's get the new text time
  fuzzy_time(t, new_time.line1, new_time.line2, new_time.line3);

  // update hour only if changed
  if(strcmp(new_time.line1, cur_time.line1) != 0) updateLayer(&line1, 1);
  // update min1 only if changed
  if(strcmp(new_time.line2, cur_time.line2) != 0) updateLayer(&line2, 2);
  // update min2 only if changed happens on
  if(strcmp(new_time.line3, cur_time.line3) != 0) updateLayer(&line3, 3);

  // vibrate at o'clock from 8 to 24
//  if(t->tm_min == 0 && t->tm_sec == 0 && t->tm_hour >= 8 && t->tm_hour <= 24 ) vibes_double_pulse();
//  if(t->tm_min == 59 && t->tm_sec == 57 && t->tm_hour >= 7 && t->tm_hour <= 23 ) vibes_short_pulse();
}

static void battery_handler(BatteryChargeState charge_state) {
  static char s_battery_buffer[16];

//  if (busy_animating_out || busy_animating_in) return;

  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%/", charge_state.charge_percent);
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(topbar.layer[0], s_battery_buffer);
}

static void bt_handler(bool connected) {
  static char s_bt_buffer[10];

//  if (busy_animating_out || busy_animating_in) return;

  if (connected) {
    strcpy(s_bt_buffer, "*");
  } else {
    strcpy(s_bt_buffer, "");
  }
  text_layer_set_text(topbar.layer[1], s_bt_buffer);
}

static void main_window_load(Window *window) {
  // Load GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DOMESTIC_BOLD_SUBSET_36));
  s_time_font_big = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DOMESTIC_BOLD_SUBSET_48));
  
  // Init the text layers used to show the time

  // line1
  line1.layer[0] = text_layer_create(GRect(0, line1_y, 144, 60));
//  text_layer_set_text_color(line1.layer[0], GColorWhite);
  text_layer_set_background_color(line1.layer[0], GColorClear);
//  text_layer_set_font(line1.layer[0], fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_font(line1.layer[0], s_time_font_big);
  text_layer_set_text_alignment(line1.layer[0], GTextAlignmentLeft);

  line1.layer[1] = text_layer_create(GRect(144, line1_y, 144, 60));
//  text_layer_set_text_color(line1.layer[1], GColorWhite);
  text_layer_set_background_color(line1.layer[1], GColorClear);
  text_layer_set_font(line1.layer[1], s_time_font_big);
  text_layer_set_text_alignment(line1.layer[1], GTextAlignmentLeft);

  // line2
  line2.layer[0] = text_layer_create(GRect(0, line2_y, 144, 50));
//  text_layer_set_text_color(line2.layer[0], GColorWhite);
  text_layer_set_background_color(line2.layer[0], GColorClear);
//  text_layer_set_font(line2.layer[0], fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_font(line2.layer[0], s_time_font);
  text_layer_set_text_alignment(line2.layer[0], GTextAlignmentLeft);

  line2.layer[1] = text_layer_create(GRect(-144, line2_y, 144, 50));
//  text_layer_set_text_color(line2.layer[1], GColorWhite);
  text_layer_set_background_color(line2.layer[1], GColorClear);
  text_layer_set_font(line2.layer[1], s_time_font);
  text_layer_set_text_alignment(line2.layer[1], GTextAlignmentLeft);

  // line3
  line3.layer[0] = text_layer_create(GRect(0, line3_y, 144, 50));
//  text_layer_set_text_color(line3.layer[0], GColorWhite);
  text_layer_set_background_color(line3.layer[0], GColorClear);
  text_layer_set_font(line3.layer[0], s_time_font);
  text_layer_set_text_alignment(line3.layer[0], GTextAlignmentLeft);

  line3.layer[1] = text_layer_create(GRect(144, line3_y, 144, 50));
//  text_layer_set_text_color(line3.layer[1], GColorWhite);
  text_layer_set_background_color(line3.layer[1], GColorClear);
  text_layer_set_font(line3.layer[1], s_time_font);
  text_layer_set_text_alignment(line3.layer[1], GTextAlignmentLeft);

  // top text
  topbar.layer[0] = text_layer_create(GRect(0, 0, 100, 18));
//  text_layer_set_text_color(topbar.layer[0], GColorWhite);
  text_layer_set_background_color(topbar.layer[0], GColorClear);
  text_layer_set_font(topbar.layer[0], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(topbar.layer[0], GTextAlignmentLeft);

  topbar.layer[1] = text_layer_create(GRect(100, 0, 44, 18));
//  text_layer_set_text_color(topbar.layer[0], GColorWhite);
  text_layer_set_background_color(topbar.layer[0], GColorClear);
  text_layer_set_font(topbar.layer[1], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(topbar.layer[1], GTextAlignmentRight);

  // bottom text
  bottombar.layer[0] = text_layer_create(GRect(0, 150, 144, 18));
//  text_layer_set_text_color(bottombar.layer[0], GColorWhite);
  text_layer_set_background_color(bottombar.layer[0], GColorClear);
  text_layer_set_font(bottombar.layer[0], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(bottombar.layer[0], GTextAlignmentCenter);

  // Ensures time is displayed immediately (will break if NULL tick event accessed).
  // (This is why it's a good idea to have a separate routine to do the update itself.)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_watch(t);
  
  BatteryChargeState charge_state = battery_state_service_peek();
  battery_handler(charge_state);
  
  bt_handler(bluetooth_connection_service_peek());

  Layer *root_layer = window_get_root_layer(window);
	
  layer_add_child(root_layer, text_layer_get_layer(line3.layer[0]));
  layer_add_child(root_layer, text_layer_get_layer(line3.layer[1]));
  layer_add_child(root_layer, text_layer_get_layer(line2.layer[0]));
  layer_add_child(root_layer, text_layer_get_layer(line2.layer[1]));
  layer_add_child(root_layer, text_layer_get_layer(line1.layer[0]));
  layer_add_child(root_layer, text_layer_get_layer(line1.layer[1]));
  layer_add_child(root_layer, text_layer_get_layer(topbar.layer[0]));
  layer_add_child(root_layer, text_layer_get_layer(topbar.layer[1]));
  layer_add_child(root_layer, text_layer_get_layer(bottombar.layer[0]));
}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(line1.layer[0]);
  text_layer_destroy(line1.layer[1]);
  text_layer_destroy(line2.layer[0]);
  text_layer_destroy(line2.layer[1]);
  text_layer_destroy(line3.layer[0]);
  text_layer_destroy(line3.layer[1]);
  text_layer_destroy(bottombar.layer[0]);
  text_layer_destroy(topbar.layer[0]);
  
  // Unload GFont
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_time_font_big);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (busy_animating_out || busy_animating_in) return;

  update_watch(tick_time);
}
  
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
//  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  // Register with BatteryService
  battery_state_service_subscribe(battery_handler);
  // Register BluetoothService
  bluetooth_connection_service_subscribe(bt_handler);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
