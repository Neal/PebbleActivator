#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "resource_ids.auto.h"
#include <stdint.h>
#include <string.h>
#include "../../common.h"

#define BITMAP_BUFFER_BYTES 1024

PBL_APP_INFO(MY_UUID, "Activator", "Ryan Petrich", 0x1, 0x0, RESOURCE_ID_IMAGE_ICON_TINY, APP_INFO_STANDARD_APP);

static struct WeatherData {
	Window window;
	TextLayer text_layer;
	BitmapLayer icon_layer;
	GBitmap icon_bitmap;
	uint8_t bitmap_data[BITMAP_BUFFER_BYTES];
	AppSync sync;
	uint8_t sync_buffer[32];
} s_data;

static void mkbitmap(GBitmap* bitmap, const uint8_t* data)
{
	bitmap->addr = (void*)data + 12;
	bitmap->row_size_bytes = ((uint16_t*)data)[0];
	bitmap->info_flags = ((uint16_t*)data)[1];
	bitmap->bounds.origin.x = 0;
	bitmap->bounds.origin.y = 0;
	bitmap->bounds.size.w = ((int16_t*)data)[4];
	bitmap->bounds.size.h = ((int16_t*)data)[5];
}

static void load_bitmap(uint32_t resource_id)
{
	const ResHandle h = resource_get_handle(resource_id);
	resource_load(h, s_data.bitmap_data, BITMAP_BUFFER_BYTES);
	mkbitmap(&s_data.icon_bitmap, s_data.bitmap_data);
}

static void send_cmd(uint8_t cmd, int integer)
{
	Tuplet value = TupletInteger(cmd, integer);
	
	DictionaryIterator *iter;
	app_message_out_get(&iter);
	
	if (iter == NULL)
		return;
	
	dict_write_tuplet(iter, &value);
	dict_write_end(iter);
	
	app_message_out_send();
	app_message_out_release();
}

void up_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
	(void)recognizer;
	(void)window;
	
	send_cmd(WATCH_KEY_PRESSED, WATCH_KEY_PRESSED_UP);
}

void select_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
	(void)recognizer;
	(void)window;
	
	send_cmd(WATCH_KEY_PRESSED, WATCH_KEY_PRESSED_SELECT);
}

void down_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
	(void)recognizer;
	(void)window;
	
	send_cmd(WATCH_KEY_PRESSED, WATCH_KEY_PRESSED_DOWN);
}

void click_config_provider(ClickConfig **config, Window *window)
{
	(void)window;
	
	config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
	config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
	
	config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
	config[BUTTON_ID_SELECT]->click.repeat_interval_ms = 100;
	
	config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
	config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

static char string_buffer[256];

static void in_recieved_handler(DictionaryIterator *received, void *context)
{
	Tuple *tuple = dict_read_first(received);
	do {
		switch (tuple->key) {
			case ACTIVATOR_REQUEST_VERSION:
				send_cmd(WATCH_RETURN_VERSION, WATCH_VERSION_CURRENT);
				break;
			case ACTIVATOR_SET_TEXT:
				strncpy(string_buffer, tuple->value->cstring, sizeof(string_buffer));
				text_layer_set_text(&s_data.text_layer, string_buffer);
				layer_mark_dirty(&s_data.text_layer.layer);
				break;
		}
	} while((tuple = dict_read_next(received)));
}

static void in_dropped_handler(void *context, AppMessageResult reason)
{
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context)
{
	text_layer_set_text(&s_data.text_layer, "Failed to send!");
	layer_mark_dirty(&s_data.text_layer.layer);
}

static AppMessageCallbacksNode app_callbacks = {
	.callbacks = {
		.in_received = in_recieved_handler,
		.in_dropped = in_dropped_handler,
		.out_failed = out_failed_handler,
	},
	.context = NULL
};

static void app_init(AppContextRef c)
{
	(void) c;

	resource_init_current_app(&ACTIVATOR_APP_RESOURCES);

	Window* window = &s_data.window;
	window_init(window, "Activator");
	window_set_background_color(window, GColorBlack);
	window_set_fullscreen(window, true);

	GRect icon_rect = (GRect) {(GPoint) {32, 20}, (GSize) { 80, 80 }};
	bitmap_layer_init(&s_data.icon_layer, icon_rect);
	load_bitmap(RESOURCE_ID_IMAGE_ICON);
	bitmap_layer_set_bitmap(&s_data.icon_layer, &s_data.icon_bitmap);
	layer_add_child(&window->layer, &s_data.icon_layer.layer);

	text_layer_init(&s_data.text_layer, GRect(0, 110, 144, 68));
	text_layer_set_text_color(&s_data.text_layer, GColorWhite);
	text_layer_set_background_color(&s_data.text_layer, GColorClear);
	text_layer_set_font(&s_data.text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(&s_data.text_layer, GTextAlignmentCenter);
	text_layer_set_text(&s_data.text_layer, "");
	layer_add_child(&window->layer, &s_data.text_layer.layer);

	window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
	window_stack_push(window, true);
	app_message_register_callbacks(&app_callbacks);
	send_cmd(WATCH_REQUEST_TEXT, 0);
}

static void app_deinit(AppContextRef c)
{
	app_message_deregister_callbacks(&app_callbacks);
	app_sync_deinit(&s_data.sync);
}

void pbl_main(void *params)
{
	PebbleAppHandlers handlers = {
		.init_handler = &app_init,
		.deinit_handler = &app_deinit,
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 64,
				.outbound = 16,
			},
  		}
	};
	app_event_loop(params, &handlers);
}
