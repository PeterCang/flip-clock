/**
 * Alynx Zhou <alynx.zhou@gmail.com> (https://alynx.one/)
 */
#define _CRT_SECURE_NO_WARNINGS
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "flipclock.h"
#include "clock.h"
#include "card.h"
#include "util/platform.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define FPS 60
#define MAX_PROGRESS 300
#define HALF_PROGRESS (MAX_PROGRESS / 2)
#define DOUBLE_TAP_INTERVAL_MS 300

#if defined(_WIN32)
static void _flipclock_get_program_dir_win32(char program_dir[])
{
	RETURN_IF_FAIL(program_dir != NULL);

	char *test_exe = os_get_executable_path_ptr(NULL);


	if (test_exe) {
		strcpy_s(program_dir, MAX_BUFFER_LENGTH - 1, test_exe);

		bfree(test_exe);
	}
}
#endif

struct flipclock *flipclock_create(int fixed_width, int fixed_height)
{
	struct flipclock *app = (struct flipclock *)malloc(sizeof(*app));
	if (app == NULL) {
		blog(LOG_ERROR, "[flipclock] Failed to create configuration!\n");
		return NULL;
	}

	app->source = NULL;
	app->update_mutex = NULL;
	app->clocks = NULL;
	app->type = TYPE_CLOCK;
	app->timer_value = 10;
	app->timer_start = 0;
	app->timer_last = 0;
	app->stopwatch_start = 0;
	app->stopwatch_last = 0;
	app->fixed_clock_width = fixed_width;
	app->fixed_clock_height = fixed_height;

	// Should create 1 clock in windowed mode.
	app->clocks_length = 1;
	app->last_touch_time = 0;
	app->last_touch_finger = 0;
	app->running = true;
	app->text_color.r = 0xd0;
	app->text_color.g = 0xd0;
	app->text_color.b = 0xd0;
	app->text_color.a = 0xff;
	app->box_color.r = 0x20;
	app->box_color.g = 0x20;
	app->box_color.b = 0x20;
	app->box_color.a = 0xff;
	app->background_color.r = 0x00;
	app->background_color.g = 0x00;
	app->background_color.b = 0x00;
	app->background_color.a = 0xff;
	app->ampm = false;
	app->show_hour = true;
	app->show_second = false;
	app->font_path[0] = '\0';
	app->conf_path[0] = '\0';
	app->text_scale = 1.0;
	app->card_scale = 1.0;
#if defined(_WIN32)
	app->program_dir[0] = '\0';
	_flipclock_get_program_dir_win32(app->program_dir);
#endif

#if defined(_WIN32)
	snprintf(app->font_path, MAX_BUFFER_LENGTH, "%s/flipclock.ttf",
		 app->program_dir);
#elif defined(__ANDROID__)
	// Directly under `app/src/main/assets` for Android APP.
	strncpy(app->font_path, "flipclock.ttf", MAX_BUFFER_LENGTH);
#elif defined(__linux__) && !defined(__ANDROID__)
	strncpy(app->font_path, PACKAGE_DATADIR "/fonts/flipclock.ttf",
		MAX_BUFFER_LENGTH);
#endif
	app->font_path[MAX_BUFFER_LENGTH - 1] = '\0';
	if (strlen(app->font_path) == MAX_BUFFER_LENGTH - 1)
		blog(LOG_WARNING, "[flipclock] `font_path` too long, may fail to load.\n");
	time_t raw_time = time(NULL);
	app->now = *localtime(&raw_time);
	return app;
}

static int _flipclock_parse_color(const char rgba[], SDL_Color *color)
{
	RETURN_VAL_IF_FAIL(rgba != NULL, -5);
	RETURN_VAL_IF_FAIL(color != NULL, -6);

	const int rgba_length = strlen(rgba);
	if (rgba_length == 0) {
		blog(LOG_ERROR, "[flipclock] Empty color string!\n");
		return -1;
	} else if (rgba[0] != '#') {
		blog(LOG_ERROR, "[flipclock] Color string must start with `#`!\n");
		return -2;
	} else if (rgba_length != 7 && rgba_length != 9) {
		blog(LOG_ERROR, "[flipclock] Color string must be in format `#rrggbb[aa]`!\n");
		return -3;
	} else {
		for (int i = 1; i < rgba_length; ++i) {
			// Cool, ctype.h always gives me surprise.
			if (!isxdigit(rgba[i])) {
				blog(LOG_ERROR , "[flipclock] Color string numbers should be hexcode!\n");
				return -4;
			}
		}
		/**
		 * Even if user input an invalid hexcode,
		 * we also let strtoll try to parse it.
		 * It's user's problem when displayed color
		 * is not what he/she wants.
		 */
		long long hex_number = strtoll(rgba + 1, NULL, 16);
		// Add 0xff as alpha if no alpha provided.
		if (rgba_length == 7)
			hex_number = (hex_number << 8) | 0xff;
		color->r = (hex_number >> 24) & 0xff;
		color->g = (hex_number >> 16) & 0xff;
		color->b = (hex_number >> 8) & 0xff;
		color->a = (hex_number >> 0) & 0xff;
		blog(LOG_ERROR,"[flipclock] Parsed color `rgba(%d, %d, %d, %d)`.\n", color->r,
			  color->g, color->b, color->a);
	}
	return 0;
}

static void _flipclock_create_clocks(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

	// I know what I am doing, silly tidy tools.
	// NOLINTNEXTLINE(bugprone-sizeof-expression)
	app->clocks = (struct flipclock_clock **)malloc(sizeof(*app->clocks) * app->clocks_length);

	if (app->clocks == NULL) {
		blog(LOG_ERROR, "[flipclock] Failed to create clocks!\n");
		return;
	}

	int i = 0;
	//for (int i = 0; i < app->clocks_length; ++i)
	app->clocks[i] = flipclock_clock_create(app, i);
}

#if defined(_WIN32)
static void _flipclock_create_clocks_win32(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

	_flipclock_create_clocks(app);
}
#endif

void flipclock_create_clocks(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

#if defined(_WIN32)
	_flipclock_create_clocks_win32(app);
#else
	// Android and Linux should share the same code here.
	_flipclock_create_clocks(app);
#endif
}

void flipclock_set_show_hour(struct flipclock *app, bool show_hour)
{
	RETURN_IF_FAIL(app != NULL);
	if( app->show_hour == show_hour ) return;

	app->show_hour = show_hour;
	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;
		flipclock_clock_set_show_hour(app->clocks[i], show_hour);
	}
}

void flipclock_set_show_second(struct flipclock *app, bool show_second)
{
	RETURN_IF_FAIL(app != NULL);

	if (app->show_second == show_second)
		return;

	app->show_second = show_second;
	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;
		flipclock_clock_set_show_second(app->clocks[i], show_second);
	}
}

/**
 * If you changed `ampm`, you must call `_flipclock_set_hour()` after it,
 * because hour number will change in differet types.
 */
void flipclock_set_ampm(struct flipclock *app, bool ampm)
{
	RETURN_IF_FAIL(app != NULL);

	if (app->ampm == ampm)
		return;

	app->ampm = ampm;
	if (app->ampm) {
		for (int i = 0; i < app->clocks_length; ++i) {
			if (app->clocks[i] == NULL)
				continue;
			char text[3];
			snprintf(text, sizeof(text), "%cM",
				 app->now.tm_hour / 12 ? 'P' : 'A');
			flipclock_clock_set_ampm(app->clocks[i], text);
		}
	} else {
		for (int i = 0; i < app->clocks_length; ++i) {
			if (app->clocks[i] == NULL)
				continue;
			flipclock_clock_set_ampm(app->clocks[i], NULL);
		}
	}
}

void flipclock_set_hour(struct flipclock *app, bool flip)
{
	RETURN_IF_FAIL(app != NULL);

	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;

		char text[3] = {0};

		if (app->type == TYPE_CLOCK) {
			strftime(text, sizeof(text), app->ampm ? "%I" : "%H",
				 &app->now);
			// Trim zero when using 12-hour clock.
			if (app->ampm && text[0] == '0') {
				text[0] = text[1];
				text[1] = text[2];
			}
		} else if (app->type == TYPE_STOPWATCH) {
			__time64_t now = _time64(NULL);
			__time64_t ts = now - app->stopwatch_start;
			int h = ts / (60 * 60);
			snprintf(text, 3, "%02d", h);
		} else if (app->type == TYPE_TIMER) {
			int ts = app->timer_value;

			if (ts <= 0)
				ts = 0;

			int h = ts / (60 * 60);

			snprintf(text, 3, "%02d", h);
		}

		flipclock_clock_set_hour(app->clocks[i], text, flip);
	}
}

void flipclock_set_minute(struct flipclock *app, bool flip)
{
	RETURN_IF_FAIL(app != NULL);

	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;

		char text[3] = {0};
		if (app->type == TYPE_CLOCK) {
			strftime(text, sizeof(text), "%M", &app->now);
		} else if (app->type == TYPE_STOPWATCH) {
			__time64_t now = _time64(NULL);
			__time64_t ts = now - app->stopwatch_start;
			int h = ts / (60 * 60);
			ts -= h * (60 * 60);
			if (ts < 0)
				ts = 0;
			int m = ts / (60);

			snprintf(text, 3, "%02d", m);
		} else if (app->type == TYPE_TIMER) {
			int ts = app->timer_value;

			if (ts <= 0)
				ts = 0;

			int h = ts / (60 * 60);
			ts -= h * (60 * 60);
			if (ts < 0)
				ts = 0;
			int m = ts / (60);

			snprintf(text, 3, "%02d", m);
		}

		flipclock_clock_set_minute(app->clocks[i], text, flip);
	}
}

void flipclock_set_second(struct flipclock *app, bool flip)
{
	RETURN_IF_FAIL(app != NULL);

	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;
		char text[3] = {0};
		if (app->type == TYPE_CLOCK) {
			strftime(text, sizeof(text), "%S", &app->now);
		} else if (app->type == TYPE_STOPWATCH) {
			__time64_t now = _time64(NULL);
			__time64_t ts = now - app->stopwatch_start;
			int h = ts / (60 * 60);
			ts -= h * (60 * 60);
			if (ts < 0)
				ts = 0;
			int m = ts / (60);
			ts -= m * (60);
			if (ts < 0)
				ts = 0;
			int s = ts;

			snprintf(text, 3, "%02d", s);
		} else if (app->type == TYPE_TIMER) {
			int ts = app->timer_value;

			if (ts <= 0)
				ts = 0;

			int h = ts / (60 * 60);
			ts -= h * (60 * 60);
			if (ts < 0)
				ts = 0;
			int m = ts / (60);
			ts -= m * (60);
			if (ts < 0)
				ts = 0;
			int s = ts;

			snprintf(text, 3, "%02d", s);
		}
		flipclock_clock_set_second(app->clocks[i], text, flip);
	}
}

void flipclock_animate(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

	// Pause when minimized.
	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;
		if (!app->clocks[i]->waiting)
			flipclock_clock_animate(app->clocks[i]);
	}
}

void flipclock_destroy_clocks(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

	for (int i = 0; i < app->clocks_length; ++i) {
		if (app->clocks[i] == NULL)
			continue;
		flipclock_clock_destroy(app->clocks[i]);
	}
	free(app->clocks);
}

void flipclock_destroy(struct flipclock *app)
{
	RETURN_IF_FAIL(app != NULL);

	free(app);
}
