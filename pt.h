
/*

Program:	Protracker module format as a C structure
Author:		Lindsay Meek
Version:	27/3/94

*/

#ifndef _PT_H
#define _PT_H

#include "standard.h"

typedef struct ProTrackerSample {
text 	sample_Name[22];
uint16	sample_Length;		/* In words */
uint8	sample_FineTune;	/* Lower Four Bits */
uint8	sample_Volume;		/* 0-64 */
uint16	sample_Repeat;		/* Repeat offset (words) */
uint16  sample_Repeat_Length;	/* In Words */
} ProTrackerSample;

typedef struct ProTrackerNote {
uint8	note_SampleMSB_PeriodMSB;
uint8	note_PeriodLSB;
uint8	note_SampleLSB_EffectMSB;
uint8	note_EffectLSB;
} ProTrackerNote;

typedef struct ProTrackerChannels {
ProTrackerNote	Chan[4];
} ProTrackerChannels;

typedef struct ProTrackerPattern {
ProTrackerChannels	Sequence[64];
} ProTrackerPattern;

typedef struct ProTrackerModule {
text 			song_Name[20];
ProTrackerSample	song_Sample[31];
uint8			song_Length;		/* Patterns */
uint8			song_Reserved;		/* 127 */
uint8			song_Patterns[128];	/* Pattern sequence */
uint32			song_Format;		/* M.K. for 31 samples */
ProTrackerPattern	song_Pattern[1];	/* First Pattern */
} ProTrackerModule;

#endif
