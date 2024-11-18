#include "card.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "flipclock.h"
#include "clock.h"
#include "card.h"

static void _flipclock_clock_update_layout(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	const struct flipclock *app = clock->app;
	SDL_Rect hour_rect = {0, 0, 0, 0};
	SDL_Rect minute_rect = {0, 0, 0, 0};
	SDL_Rect second_rect = {0, 0, 0, 0};
	int cards_length = 3;
	if (app->show_second == false)
		cards_length -= 1;
	if (app->show_hour == false)
		cards_length -= 1;
	int spaces_length = cards_length + 1;
	// space/card = 1/8.
	/**
	 * In best condition, we have 1 + 8 + 1 + 8 + 1. However, the other
	 * length of window might be smaller, and the card is less than 8. We
	 * will enlarge the spaces of begining and end, so only care about
	 * spaces between cards when calculating position.
	 */
	if (clock->w >= clock->h) {
		int space_size = clock->w / (cards_length * 8 + spaces_length);
		int min_height = clock->h * 0.8;
		int min_width =
			clock->w * 8 / (cards_length * 8 + spaces_length);
		int card_size = min_height < min_width ? min_height : min_width;
		card_size *= app->card_scale;

		if (app->show_hour) {

			hour_rect.x = (clock->w - card_size * cards_length -
				       space_size * (spaces_length - 2)) /
				      2;
			hour_rect.y = (clock->h - card_size) / 2;
			hour_rect.w = card_size;
			hour_rect.h = card_size;
			flipclock_card_set_rect(clock->hour, hour_rect);
		}

		minute_rect.x = hour_rect.x + hour_rect.w + space_size;
		minute_rect.y = (clock->h - card_size) / 2;
		minute_rect.w = card_size;
		minute_rect.h = card_size;
		flipclock_card_set_rect(clock->minute, minute_rect);

		if (app->show_second) {
			second_rect.x = hour_rect.x + hour_rect.w + space_size +
					minute_rect.w + space_size;
			second_rect.y = (clock->h - card_size) / 2;
			second_rect.w = card_size;
			second_rect.h = card_size;
			flipclock_card_set_rect(clock->second, second_rect);
		}
	} else {
		int space_size = clock->h / (cards_length * 8 + spaces_length);
		int min_width = clock->w * 0.8;
		int min_height =
			clock->h * 8 / (cards_length * 8 + spaces_length);
		int card_size = min_height < min_width ? min_height : min_width;
		card_size *= app->card_scale;

		if (app->show_hour) {
			hour_rect.x = (clock->w - card_size) / 2;
			hour_rect.y = (clock->h - card_size * cards_length -
				       space_size * (spaces_length - 2)) /
				      2;
			hour_rect.w = card_size;
			hour_rect.h = card_size;
			flipclock_card_set_rect(clock->hour, hour_rect);
		}

		minute_rect.y = hour_rect.y + hour_rect.h + space_size;
		minute_rect.x = (clock->w - card_size) / 2;
		minute_rect.w = card_size;
		minute_rect.h = card_size;
		flipclock_card_set_rect(clock->minute, minute_rect);

		if (app->show_second) {
			second_rect.y = hour_rect.y + hour_rect.h + space_size +
					minute_rect.h + space_size;
			second_rect.x = (clock->w - card_size) / 2;
			second_rect.w = card_size;
			second_rect.h = card_size;
			flipclock_card_set_rect(clock->second, second_rect);
		}
	}
}

static void _flipclock_clock_create_cards(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	struct flipclock *app = clock->app;

	if ( app->show_hour )
		clock->hour = flipclock_card_create(app, clock->renderer);
	clock->minute = flipclock_card_create(app, clock->renderer);
	clock->second = NULL;
	if (app->show_second)
		clock->second = flipclock_card_create(app, clock->renderer);

	_flipclock_clock_update_layout(clock);
}

struct flipclock_clock *flipclock_clock_create(struct flipclock *app, int i)
{
	RETURN_VAL_IF_FAIL(app != NULL, NULL);

	/**
	 * We need `SDL_WINDOW_RESIZABLE` for auto-rotate
	 * while fullscreen on Android.
	 */
	unsigned int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
			     SDL_WINDOW_ALLOW_HIGHDPI;
	flags |= SDL_WINDOW_HIDDEN;

	struct flipclock_clock *clock = (struct flipclock_clock *)malloc(sizeof(*clock));
	if (clock == NULL) {
		blog(LOG_ERROR, "[flipclock] Failed to create clock!");
		return NULL;
	}
	clock->app = app;
	clock->window = NULL;
	clock->renderer = NULL;
// 	clock->renderTexture = NULL;
	clock->hour = NULL;
	clock->minute = NULL;
	clock->second = NULL;

	clock->waiting = false;
	clock->i = i;
	SDL_Rect display_bounds;
	SDL_GetDisplayBounds(i, &display_bounds);
	// Give each window a unique title.
	char window_title[MAX_BUFFER_LENGTH];
	snprintf(window_title, MAX_BUFFER_LENGTH, PROGRAM_TITLE " %d", i);
	clock->window = SDL_CreateWindow(
		window_title, display_bounds.x + (display_bounds.w - app->fixed_clock_width) / 2,
		display_bounds.y + (display_bounds.h - app->fixed_clock_height) / 2,
		app->fixed_clock_width, app->fixed_clock_height, flags);
	if (clock->window == NULL) {
		flipclock_clock_destroy(clock);
		clock = NULL;

		blog(LOG_ERROR, "[flipclock] failed to create sdl window: %s\n", SDL_GetError());

		return NULL;
	}
	if (app->fixed_clock_width > 0 && app->fixed_clock_height > 0 ) {
		clock->w = app->fixed_clock_width;
		clock->h = app->fixed_clock_height;
	} else {
		// Get actual window size after create it.
		SDL_GetWindowSize(clock->window, &clock->w, &clock->h);
	}
	clock->renderer = SDL_CreateRenderer(
		clock->window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE |
			SDL_RENDERER_PRESENTVSYNC);

	if (clock->renderer == NULL) {
		flipclock_clock_destroy(clock);
		clock = NULL;

		blog(LOG_ERROR, "[flipclock] failed to create renderer: %s\n", SDL_GetError());

		return NULL;
	}
	SDL_SetRenderDrawBlendMode(clock->renderer, SDL_BLENDMODE_BLEND);

// 	clock->renderTexture =
// 		SDL_CreateTexture(clock->renderer, SDL_PIXELFORMAT_NV12,
// 				  SDL_TEXTUREACCESS_TARGET, clock->w, clock->h);
// 	if (clock->renderTexture == NULL) {
// 		flipclock_clock_destroy(clock);
// 		clock = NULL;
// 
// 		blog(LOG_ERROR, "[flipclock] failed to renderer texture: %s\n", SDL_GetError());
// 
// 		return NULL;
// 	}
// 
// 	SDL_SetRenderTarget(clock->renderer, clock->renderTexture);
// 
	_flipclock_clock_create_cards(clock);

	return clock;
}
void flipclock_clock_set_show_hour(struct flipclock_clock *clock,
				     bool show_hour)
{
	RETURN_IF_FAIL(clock != NULL);

	if (show_hour) {
		if (clock->hour == NULL)
			clock->hour = flipclock_card_create(clock->app,
							      clock->renderer);
	} else {
		if (clock->hour != NULL) {
			flipclock_card_destory(clock->hour);
			clock->hour = NULL;
		}
	}

	// Toggling hour always changes size.
	_flipclock_clock_update_layout(clock);
}

void flipclock_clock_set_show_second(struct flipclock_clock *clock,
				     bool show_second)
{
	RETURN_IF_FAIL(clock != NULL);

	if (show_second) {
		if (clock->second == NULL)
			clock->second = flipclock_card_create(clock->app,
							      clock->renderer);
	} else {
		if (clock->second != NULL) {
			flipclock_card_destory(clock->second);
			clock->second = NULL;
		}
	}

	// Toggling seconds always changes size.
	_flipclock_clock_update_layout(clock);
}

void flipclock_clock_set_hour(struct flipclock_clock *clock, const char hour[],
			      bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	if (!clock->app->show_hour)
		return;

	flipclock_card_set_text(clock->hour, hour);
	if (flip)
		flipclock_card_flip(clock->hour);
}

void flipclock_clock_set_minute(struct flipclock_clock *clock,
				const char minute[], bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_set_text(clock->minute, minute);
	if (flip)
		flipclock_card_flip(clock->minute);
}

void flipclock_clock_set_second(struct flipclock_clock *clock,
				const char second[], bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	if (!clock->app->show_second)
		return;

	flipclock_card_set_text(clock->second, second);
	if (flip)
		flipclock_card_flip(clock->second);
}

void flipclock_clock_set_ampm(struct flipclock_clock *clock, const char ampm[])
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_set_sub_text(clock->hour, ampm);
	// Set ampm should never flip a card.
}

void flipclock_clock_animate(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	const struct flipclock *app = clock->app;
	SDL_SetRenderDrawColor(clock->renderer, app->background_color.r,
			       app->background_color.g, app->background_color.b,
			       app->background_color.a);
	SDL_RenderClear(clock->renderer);

	if ( app->show_hour)
		flipclock_card_animate(clock->hour);

	flipclock_card_animate(clock->minute);
	if (app->show_second)
		flipclock_card_animate(clock->second);

	//SDL_RenderPresent(clock->renderer);
}

void flipclock_clock_destroy(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_destory(clock->hour);
	clock->hour = NULL;
	flipclock_card_destory(clock->minute);
	if (clock->second != NULL)
		flipclock_card_destory(clock->second);
	SDL_DestroyRenderer(clock->renderer);
	SDL_DestroyWindow(clock->window);
	free(clock);
}
