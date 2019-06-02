
#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "sample.h"
#include "fft.h"

/* Spectrum display generation routines */

bool display_INIT(bool use_parallel);
bool display_TICK();

/* Trigger sample and change its period code */
void display_TRIGGER(int16 channel,FreqSample *sample,uint16 period);
void display_PERIOD(int16 channel,uint16 period);
void display_VOLUME(int16 channel,uint16 volume);

bool display_CLOSE();
bool display_CHECK();	/* to see if the user has closed */

#define DISPLAY_FREQ 40000
#define DISPLAY_BARS (POWER_SIZE*2)

#endif
