
#ifndef _SAMPLE_H
#define _SAMPLE_H

/* sample definition */

#include "standard.h"

typedef struct FreqSample {
	int8  *amplitude;
	uint32 amp_len;
	uint8  *power;
	uint32 power_len;
	bool   power_rep_enable;
	uint32 power_rep_start;
	uint32 power_rep_len;
	uint16 volume;
	uint16 n;
} FreqSample;

#endif
