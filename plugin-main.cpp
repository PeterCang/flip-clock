#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <util/windows/win-version.h>
#include <util/windows/window-helpers.h>
#include <util/platform.h>
#include <obs-module.h>
#include <strsafe.h>
#include <strmif.h>
#include "flipclock.h"
#include "SDL_ttf.h"

static bool sdlSystemInited = false;
static struct obs_source_frame2 __frame;
// is stopwatch
static bool stopwatch_started = false;
// timer started
static bool timer_started = false;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-flipclock", "en-US")

#define TEXT_FLIPCLOCK obs_module_text("FlipClock")
#define TEXT_TYPE obs_module_text("FlipClock.type")
#define TEXT_TYPE_CLOCK obs_module_text("FlipClock.type.clock")
#define TEXT_TYPE_STOPWATCH obs_module_text("FlipClock.type.stopwatch")
#define TEXT_TYPE_TIMER obs_module_text("FlipClock.type.timer")
#define TEXT_12HOURS obs_module_text("FlipClock.12hours")
#define TEXT_SHOWSECONDS obs_module_text("FlipClock.showseconds")
#define TEXT_TIMER_VALUE obs_module_text("FlipClock.timervalue")
#define TEXT_TIMER_START obs_module_text("FlipClock.timer.start")
#define TEXT_TIMER_STOP obs_module_text("FlipClock.timer.stop")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Flip clock, Enhance your OBS streams with our versatile Clock Plugin! Display current time with customizable formats (AM/PM, seconds), countdowns, or a stopwatch – perfect for keeping your audience on time and engaged.";
}

static void update_settings_visibility(obs_properties_t *props,
				       struct flipclock *app)
{
	pthread_mutex_lock(&app->update_mutex);

	const enum flipclock_type type = app->type;

	obs_property_t *p = obs_properties_get(props, "ampm");
	obs_property_set_visible(p, type == TYPE_CLOCK);

	p = obs_properties_get(props, "show_second");
	obs_property_set_visible(p, type == TYPE_CLOCK);

	p = obs_properties_get(props, "timer");
	obs_property_set_visible(p, type == TYPE_TIMER);

	p = obs_properties_get(props, "timer_startstop");
	obs_property_set_visible(p, type == TYPE_TIMER);
	obs_property_set_description(p, timer_started
						? TEXT_TIMER_STOP
						: TEXT_TIMER_START);

	p = obs_properties_get(props, "stopwatch_startstop");
	obs_property_set_visible(p, type == TYPE_STOPWATCH);
	obs_property_set_description(p, stopwatch_started ? TEXT_TIMER_STOP
						      : TEXT_TIMER_START);

	pthread_mutex_unlock(&app->update_mutex);
}

static void update_settings(struct flipclock *app, obs_data_t *s)
{
	pthread_mutex_lock(&app->update_mutex);

	enum flipclock_type type =
		(enum flipclock_type)obs_data_get_int(s, "type");
	int timerValue = (int)obs_data_get_int(s, "timer");
	bool ampm = (int)obs_data_get_bool(s, "ampm");
	bool show_second = (int)obs_data_get_bool(s, "show_second");

	if (app->type == type && (timerValue*60) == app->timer_value && app->ampm == ampm && app->show_second == show_second ) {
		pthread_mutex_unlock(&app->update_mutex);

		return;
	}

	if (timerValue < 0)
		timerValue = 1;
	if (timerValue > 43200)
		timerValue = 43200;

	app->type = type;
	app->timer_value_org = app->timer_value = timerValue * 60;
	if (timer_started == false) {
		app->timer_start = app->timer_last = _time64(NULL);	
	}
	if (stopwatch_started == false) {
		app->stopwatch_last = app->stopwatch_start =	 _time64(NULL);
	}
	
	if (app->type == TYPE_CLOCK) {
		flipclock_set_ampm(app, ampm);
		flipclock_set_show_hour(app, true);
		flipclock_set_hour(app, true);
		flipclock_set_minute(app, true);
		flipclock_set_show_second(app, show_second);
		flipclock_set_second(app, true);
	} else if (app->type == TYPE_STOPWATCH) {
		flipclock_set_ampm(app, false);
		flipclock_set_show_hour(app, true);
		flipclock_set_hour(app, true);
		flipclock_set_minute(app, true);
		flipclock_set_show_second(app, true);
		flipclock_set_second(app, true);
	} else if (app->type == TYPE_TIMER) {
		flipclock_set_ampm(app, false);
		flipclock_set_minute(app, true);
		flipclock_set_show_second(app, true);
		flipclock_set_second(app, true);

		if (timerValue < 60) {
			flipclock_set_hour(app, true);
			flipclock_set_show_hour(app, false);
		} else {
			flipclock_set_show_hour(app, true);
			flipclock_set_hour(app, true);
		}
	}

	pthread_mutex_unlock(&app->update_mutex);
}

static void log_settings(struct flipclock *app, obs_data_t *s)
{
	blog(LOG_INFO,
	     "[flipclock: '%s'] update settings:\n"
	     "\ttype: %d\n"
	     "\ttimer seconds: %d\n"
	     "\tampm: %d\n"
	     "\tshow_second: %d\n",
	     obs_source_get_name(app->source), app->type, app->timer_value,
	     app->ampm, app->show_second);
}


static bool time_value_changed(obs_properties_t *props, obs_property_t *p,
			  obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	struct flipclock *app = reinterpret_cast<struct flipclock *>(
		obs_properties_get_param(props));
	if (!app)
		return false;

	int timerValue = (int)obs_data_get_int(settings, "timer");
	if (timerValue*60 == (app->timer_value_org)) {
		return true;
	}

	timer_started = false;

	obs_property_t *pp = obs_properties_get(props, "timer_startstop");
	obs_property_set_description(pp, timer_started
						? TEXT_TIMER_STOP
						: TEXT_TIMER_START);

	return true;
}

static bool flipwatch_type_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);

	enum flipclock_type type =
		(enum flipclock_type)obs_data_get_int(settings, "type");

	obs_property_t *pp = obs_properties_get(props, "ampm");
	obs_property_set_visible(pp, type == TYPE_CLOCK);

	pp = obs_properties_get(props, "show_second");
	obs_property_set_visible(pp, type == TYPE_CLOCK);

	pp = obs_properties_get(props, "timer");
	obs_property_set_visible(pp, type == TYPE_TIMER);

	pp = obs_properties_get(props, "stopwatch_startstop");
	obs_property_set_visible(pp, type == TYPE_STOPWATCH);
	obs_property_set_description(pp, stopwatch_started ? TEXT_TIMER_STOP
						       : TEXT_TIMER_START);

	pp = obs_properties_get(props, "timer_startstop");
	obs_property_set_visible(pp, type == TYPE_TIMER);
	obs_property_set_description(pp, timer_started
						? TEXT_TIMER_STOP
						: TEXT_TIMER_START);

	return true;
}

static bool stopwatch_startstop_clicked(obs_properties_t *, obs_property_t *p,
				      void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	stopwatch_started = !stopwatch_started;

	obs_property_set_description(
		p, stopwatch_started ? TEXT_TIMER_STOP : TEXT_TIMER_START);

	app->stopwatch_last = app->stopwatch_start = _time64(NULL);

	if (stopwatch_started) {
		timer_started = false;
	}

	return true;
}

static bool timer_startstop_clicked(obs_properties_t *, obs_property_t *p, void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	timer_started = !timer_started;

	obs_property_set_description(p, timer_started 
						? TEXT_TIMER_STOP
						: TEXT_TIMER_START);

	app->timer_value = app->timer_value_org;
	app->timer_start = app->timer_last = _time64(NULL);

	if (timer_started) {
		stopwatch_started = false;
	}

	return true;
}

static void *fc_create(obs_data_t *settings, obs_source_t *source)
{
	int default_width = 1920;
	int default_height = 1080;
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		default_width = ovi.base_width;
		default_height = default_width / 1.777777777777778;
	}

	struct flipclock *app = flipclock_create(default_width, default_height);
	app->source = source;

	pthread_mutex_init(&app->update_mutex, NULL);

	flipclock_create_clocks(app);

	const enum video_range_type range = VIDEO_RANGE_DEFAULT;
	enum video_colorspace cs = VIDEO_CS_DEFAULT;
	enum video_trc trc = VIDEO_TRC_DEFAULT;
	switch (cs) {
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_601:
	case VIDEO_CS_709:
	case VIDEO_CS_SRGB:
		trc = VIDEO_TRC_SRGB;
		break;
	case VIDEO_CS_2100_PQ:
		trc = VIDEO_TRC_PQ;
		break;
	case VIDEO_CS_2100_HLG:
		trc = VIDEO_TRC_HLG;
	}

	__frame.range = range;
	__frame.trc = trc;

	bool success = video_format_get_parameters_for_format(
		cs, range, VIDEO_FORMAT_RGBA,
		__frame.color_matrix, __frame.color_range_min,
		__frame.color_range_max);

	flipclock_set_ampm(app, false);

	flipclock_set_hour(app, true);

	flipclock_set_show_second(app, true);
	flipclock_set_minute(app, true);
	flipclock_set_second(app, true);

	update_settings(app, settings);
	log_settings(app, settings);

	return app;
}

static const char *fc_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return TEXT_FLIPCLOCK;
}

static void fc_destroy(void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	pthread_mutex_destroy(&app->update_mutex);
	app->update_mutex = NULL;

	flipclock_destroy(app);
}

static void fc_update(void *data, obs_data_t *settings)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	update_settings(app, settings);

	log_settings(app, settings);
}

static void fc_tick(void *data, float seconds)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	if (!obs_source_showing(app->source))
		return;

	if (app->type == TYPE_CLOCK) {
		struct tm past = app->now;
		time_t raw_time = time(NULL);
		app->now = *localtime(&raw_time);
		if (app->now.tm_hour != past.tm_hour) {
			flipclock_set_ampm(app, app->ampm);
			flipclock_set_hour(app, true);
		}

		if (app->now.tm_min != past.tm_min)
			flipclock_set_minute(app, true);
		if (app->show_second && app->now.tm_sec != past.tm_sec)
			flipclock_set_second(app, true);
	} else if (app->type == TYPE_STOPWATCH && stopwatch_started) {
		__time64_t now = _time64(NULL);
		if (now - app->stopwatch_last > 0) {
			__time64_t ts1 =
				app->stopwatch_last - app->stopwatch_start;
			__time64_t ts2 =
				now - app->stopwatch_start;

			int h1 = ts1 / (60 * 60);
			int h2 = ts2 / (60 * 60);
			ts1 -= h1 * (60 * 60);
			if (ts1 < 0)
				ts1 = 0;
			int m1 = ts1 / (60);
			ts2 -= h2 * (60 * 60);
			if (ts2 < 0)
				ts2 = 0;
			int m2 = ts2 / (60);
			ts1 -= m1 * (60);
			if (ts1 < 0)
				ts1 = 0;
			int s1 = ts1;
			ts2 -= m2 * (60);
			if (ts2 < 0)
				ts2 = 0;
			int s2 = ts2;
			app->stopwatch_last = now;

			if ( h1 != h2 )
				flipclock_set_hour(app, true);
			if (m1 != m2)
				flipclock_set_minute(app, true);
			if ( s1 != s2 )
				flipclock_set_second(app, true);
		}
	} else if (app->type == TYPE_TIMER && timer_started) {
		__time64_t now = _time64(NULL);
		if (now - app->timer_last > 0) {
			int ts1 = app->timer_value;
			int ts2 = app->timer_value-(now - app->timer_last);

			if (ts1 <= 0) ts1 = 0;
			if (ts2 <= 0) ts2 = 0;
			int h1 = ts1 / (60 * 60);
			int h2 = ts2 / (60 * 60);
			ts1 -= h1 * (60 * 60);
			if (ts1 < 0) ts1 = 0;
			int m1 = ts1 / (60);
			ts2 -= h2 * (60 * 60);
			if (ts2 < 0)
				ts2 = 0;
			int m2 = ts2 / (60);
			ts1 -= m1 * (60);
			if (ts1 < 0)
				ts1 = 0;
			int s1 = ts1;
			ts2 -= m2 * (60);
			if (ts2 < 0)
				ts2 = 0;
			int s2 = ts2;

			app->timer_value -= (now - app->timer_last);
			app->timer_last = now;
			
			if (app->timer_value >= 0) {
				if (h1 != h2 ) flipclock_set_hour(app, true);
				if( m1 != m2 ) flipclock_set_minute(app, true);
				if ( s1 != s2 ) flipclock_set_second(app, true);
			}
		}
	}

	flipclock_animate(app);

	int _w = app->clocks[0]->w;
	int _h = app->clocks[0]->h;
	
	 SDL_Surface *SaveSurface = SDL_CreateRGBSurfaceWithFormat(
		0, _w, _h, 32, SDL_PIXELFORMAT_RGBA32);

	SDL_RenderReadPixels(app->clocks[0]->renderer, NULL,
			      SaveSurface->format->format, SaveSurface->pixels,
			      SaveSurface->pitch);


	__frame.timestamp = 0;
	__frame.width = _w;
	__frame.height = _h;
	__frame.format = VIDEO_FORMAT_RGBA;
	__frame.flip = false;
	__frame.flags = OBS_SOURCE_FRAME_LINEAR_ALPHA;

	__frame.data[0] = (uint8_t *)SaveSurface->pixels;
	//__frame.data[1] = __frame.data[0] + (_w* _h);
	__frame.linesize[0] = _w*4;
	//__frame.linesize[1] = _h;

	obs_source_output_video2(app->source, &__frame);

	SDL_FreeSurface(SaveSurface);
}

static void fc_defaults(obs_data_t *defaults)
{
	obs_data_set_default_int(defaults, "type", TYPE_CLOCK);
	obs_data_set_default_bool(defaults, "ampm", false);
	obs_data_set_default_bool(defaults, "show_second", true);
	obs_data_set_default_int(defaults, "timer", 10);
}

static uint32_t fc_width(void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	return app->clocks[0]->w;
}

static uint32_t fc_height(void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	return app->clocks[0]->h;
}

static obs_properties_t *fc_properties(void *data)
{
	struct flipclock *app = reinterpret_cast<struct flipclock *>(data);

	obs_properties_t *ppts = obs_properties_create();
	obs_properties_set_param(ppts, app, NULL);

	obs_property_t *p;
	p = obs_properties_add_list(ppts, "type", TEXT_TYPE,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_TYPE_CLOCK, TYPE_CLOCK);
	obs_property_list_add_int(p, TEXT_TYPE_STOPWATCH, TYPE_STOPWATCH);
	obs_property_list_add_int(p, TEXT_TYPE_TIMER, TYPE_TIMER);
	obs_property_set_modified_callback(p, flipwatch_type_changed);

	obs_properties_add_bool(ppts, "ampm", TEXT_12HOURS);

	obs_properties_add_bool(ppts, "show_second", TEXT_SHOWSECONDS);

	p = obs_properties_add_int(ppts, "timer", TEXT_TIMER_VALUE, 1, 43200, 1);
	obs_property_set_modified_callback(p, time_value_changed);

	obs_properties_add_button(ppts, "timer_startstop", TEXT_TIMER_START,
				  timer_startstop_clicked);

	obs_properties_add_button(ppts, "stopwatch_startstop", TEXT_TIMER_START,
				  stopwatch_startstop_clicked);

	return ppts;
}

bool obs_module_load(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		blog(LOG_ERROR,"[flipclock] SDL init failed: %s\n", SDL_GetError());
		return false;
	}
	if (TTF_Init() < 0) {
		blog(LOG_ERROR, "[flipclock] SDL ttf init failed: %s\n", TTF_GetError());
		return false;
	}

	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

	sdlSystemInited = true;

	blog(LOG_INFO, "[flipclock] subsystem inited\n");

	struct obs_source_info flipclock_info = {};

	flipclock_info.id = "win_flipclock";
	flipclock_info.type = OBS_SOURCE_TYPE_INPUT;
	flipclock_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	flipclock_info.get_name = fc_getname;
	flipclock_info.create = fc_create;
	flipclock_info.destroy = fc_destroy;
	flipclock_info.update = fc_update;
	flipclock_info.video_tick = fc_tick;
	flipclock_info.get_width = fc_width,
	flipclock_info.get_height = fc_height,
	flipclock_info.get_defaults = fc_defaults;
	flipclock_info.get_properties = fc_properties,
	flipclock_info.icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE;

	obs_register_source(&flipclock_info);

	return true;
}

void obs_module_unload(void)
{
	if (sdlSystemInited) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_Quit();
		sdlSystemInited = false;
	}
	blog(LOG_INFO, "[flipclock] subsystem uninited\n");
}
