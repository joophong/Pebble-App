/* author: John Lewis, Steven Parad, and Joopyo Hong */

#include <pebble.h>
#include <pebble_fonts.h>
  
static Window *window;
static TextLayer *temp_layer;
static char msg[500];
static int mode = 0;
static int number_of_modes = 5;
static int standby = 0;
static int last_request = 0;
static int try_count = 0;

static const int CELSIUS = 0;
static const int FAHRENHEIT = 1;
static const int MORSE = 2;
static const int TIME = 3;
static const int GRAPH = 4;

static const int PAUSE = 5;
static const int RESUME = 6; 
static const int DOT = 7;
static const int DASH = 8;

void click_select();
void enter_standby();
void resume();

//typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
//void (*)(void *, struct Window *)

void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered -- do nothing
 }


/* Tries to resend the last request up to 10 times before printing that it could not connect. */
 void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
   
   if (try_count > 10) {
     try_count = 0;
     text_layer_set_text(temp_layer, "Could not connect to phone.");
     return;
   }
   
   try_count++;
   if (last_request == PAUSE) {
     enter_standby();
   } else if (last_request == RESUME) {
     resume();
   } else { // Get data for current mode
     click_select();
   }
 }

/* Handles all messages recieved from the server. */
 void in_received_handler(DictionaryIterator *received, void *context) {
   // incoming message received 
   // looks for key #0 in the incoming message
   int key = 0;
   Tuple *text_tuple = dict_find(received, key);
   if (text_tuple) {
     if (text_tuple->value) {
       // put it in this global variable
       strcpy(msg, text_tuple->value->cstring);
     }
     else strcpy(msg, "no value!");
     
     //replace '$' with '\n' in message
     unsigned int i;
     for (i = 0; i < strlen(msg); i++) {
       if (msg[i] == '$') msg[i] = '\n';
     }
     
     text_layer_set_text(temp_layer, msg);
   }
   else {
     text_layer_set_text(temp_layer, "no message!");
   }
 }

 void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
   text_layer_set_text(temp_layer, "Error in!");
 }

/* Sends a request to the server with the current mode. */
void click_select() {
   if (standby) return;
   DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
  
   int key;
   if (mode == MORSE) {
     key = DOT;
     last_request = key;
   } else {
     key = mode;
   }
  
   // send the message "hello?" to the phone, using key #0
   Tuplet value = TupletCString(key, "hello?");
   dict_write_tuplet(iter, &value);
   app_message_outbox_send();
}

/* This is called when the select button is clicked */
void select_click_handler(ClickRecognizerRef recognizer, void *context) {
   last_request = 0;
   click_select();
}

/* This called when select is long pressed. */
void select_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (mode == MORSE) { 
    last_request = DASH;
    int key = DASH;
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    Tuplet value = TupletCString(key, "hello?");
    dict_write_tuplet(iter, &value);
    app_message_outbox_send();
  } else {
    click_select();
  }
}

/* This is called when the down button is clicked */
void down_click_handler(ClickRecognizerRef recognizer, void *context) {
   if (standby) return;
   if (mode == TIME) {
     text_layer_set_text_alignment(temp_layer, GTextAlignmentLeft);
   } else {
     text_layer_set_text_alignment(temp_layer, GTextAlignmentCenter);
   }
   DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
   mode++;
   mode = mode % number_of_modes;
   last_request = 0;
   click_select();
}

/* Exits standby mode. */
void resume() {
   DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
   int key = RESUME;
   standby = 0;
   last_request = key;
   // send the message "hello?" to the phone, using key #0
   Tuplet value = TupletCString(key, "hello?");
   dict_write_tuplet(iter, &value);
   app_message_outbox_send();
}

/* Sends the standby signal to the server and puts the watch in standby mode. */
void enter_standby() {
   DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
   int key = PAUSE;
   standby = 1;
   last_request = key;
   Tuplet value = TupletCString(key, "hello?");
   dict_write_tuplet(iter, &value);
   app_message_outbox_send();
}

/* This is called when the up button is clicked */
void up_click_handler(ClickRecognizerRef recognizer, void *context) {
   if (standby) {
     resume();
   } else {
     enter_standby();
   }
}

/* this registers the appropriate function to the appropriate button */
void config_provider(void *context) {
   window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
   window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
   window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
   window_long_click_subscribe(BUTTON_ID_SELECT, 700, NULL, select_long_click_release_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  temp_layer = text_layer_create((GRect) { .origin = { 0, 40 }, .size = bounds.size });
  text_layer_set_text_alignment(temp_layer, GTextAlignmentCenter);
  text_layer_set_font(temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(temp_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(temp_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  
  window_set_click_config_provider(window, config_provider);
  
  // for registering AppMessage handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  click_select();
  app_event_loop();
  deinit();
}
