#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include "config.h"

#ifdef HAVE_SDL_H
# include <SDL.h>
#elif defined(HAVE_SDL_SDL_H)
# include <SDL/SDL.h>
#endif

int screenshot(SDL_Surface *);

#endif
