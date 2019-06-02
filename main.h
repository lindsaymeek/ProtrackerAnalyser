
#ifndef _SAMPLE_H
#define _SAMPLE_H

/* sample definition */

#include "standard.h"

typedef struct FreqSample {
	uint16 usage;
	uint32 period;
	real   true_period;
	uint16 window;
	uint8  *power;
	uint16 power_len;
} FreqSample;

#endif
