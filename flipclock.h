/**
 * Author: Alynx Zhou <alynx.zhou@gmail.com> (https://alynx.one/)
 */
#ifndef __FLIPCLOCK_H__
#define __FLIPCLOCK_H__

#include <stdbool.h>
#include <time.h>

#include "SDL.h"
#include "obs.h"
#include "pthread.h"
#include "clock.h"

#if defined(_WIN32)
#	include <windows.h>
#endif

/**
 * Similiar with GLib. Those macros are only used for debug, which means a
 * failure should be a programmer error so the code should be checked.
 */
#define RETURN_IF_FAIL(EXPR)                                              \
	do {                                                              \
		if (!(EXPR)) {                                            \
			blog(LOG_ERROR, "[flipclock] %s: `%s` failed!\n", __func__, #EXPR); \
			return;                                           \
		}                                                         \
	} while (0)

#define RETURN_VAL_IF_FAIL(EXPR, VAL)                                     \
	do {                                                              \
		if (!(EXPR)) {                                            \
			blog(LOG_ERROR,"[flipclock] %s: `%s` failed!\n", __func__, #EXPR); \
			return (VAL);                                     \
		}                                                         \
	} while (0)

#define PROGRAM_TITLE "FlipClock"
#define MAX_BUFFER_LENGTH 2048

enum flipclock_type {
	TYPE_CLOCK,
	TYPE_STOPWATCH,
	TYPE_TIMER,
};

struct flipclock {
	obs_source_t *source;
	pthread_mutex_t update_mutex;

	// clock type, default: TYPE_CLOCK
	flipclock_type type;
	// timer value minutes
	int timer_value;
	// initial value of timer_value
	int timer_value_org;
	__time64_t timer_start; 
	__time64_t timer_last;
	__time64_t stopwatch_start;
	__time64_t stopwatch_last;
	// fixed clock width/ height
	int fixed_clock_width;
	int fixed_clock_height;
	// Structures not shared by clocks.
	struct flipclock_clock **clocks;
	// Number of clocks.
	int clocks_length;
	// Structures shared by clocks.
	struct tm now;
	SDL_Color box_color;
	SDL_Color text_color;
	SDL_Color background_color;
	char font_path[MAX_BUFFER_LENGTH];
	char conf_path[MAX_BUFFER_LENGTH];
	double text_scale;
	double card_scale;
#if defined(_WIN32)
	HWND preview_window;
	char program_dir[MAX_BUFFER_LENGTH];
#endif
	bool ampm;
	bool show_hour;
	bool show_second;
	long long last_touch_time;
	SDL_FingerID last_touch_finger;
	bool running;
};

struct flipclock *flipclock_create(int fixed_width, int fixed_height);
void flipclock_create_clocks(struct flipclock *app);
void flipclock_refresh(struct flipclock *app, int clock_index);
void flipclock_create_textures(struct flipclock *app, int clock_index);
void flipclock_destroy_textures(struct flipclock *app, int clock_index);
void flipclock_open_fonts(struct flipclock *app, int clock_index);
void flipclock_close_fonts(struct flipclock *app, int clock_index);
void flipclock_destroy_clocks(struct flipclock *app);
void flipclock_destroy(struct flipclock *app);
void flipclock_set_ampm(struct flipclock *app, bool ampm);
void flipclock_set_hour(struct flipclock *app, bool flip);
void flipclock_set_show_hour(struct flipclock *app, bool show_hour);
void flipclock_set_show_second(struct flipclock *app, bool show_second);
void flipclock_set_second(struct flipclock *app, bool flip);
void flipclock_set_minute(struct flipclock *app, bool flip);
void flipclock_animate(struct flipclock *app);

#endif
