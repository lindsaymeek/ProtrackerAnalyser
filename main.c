/*

Program:	Convert noisetracker module samples into power spectrum-time
Author:		Lindsay Meek
Version:	27/3/94

*/

#include "convert.h"
#include "pt.h"
#include "display.h"
#include "sample.h"
#include "parallel.h"

#include <stdio.h>
#include <stat.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <libraries/dos.h>
#include <graphics/gfxbase.h>
#include <exec/execbase.h>

extern struct ExecBase *SysBase;

void *AllocMem(),*OpenLibrary();
BPTR Open();
long Read();

bool module_free(void *module,uint32 size)
{
	FreeMem(module,size);

	return true;
}

bool module_load(text *file,void **module,uint32 *size)
{
	struct stat st;
	BPTR f;
	void *mod;
	
	if(0!=stat(file,&st))
		return false;

	if(NULL==(mod=AllocMem(st.st_size,MEMF_CHIP)))
		return false;

	*size=st.st_size;
	*module=mod;

	if(!(f=Open(file,MODE_OLDFILE)))
	{	
		module_free(mod,st.st_size);return false;
	}

	if(st.st_size != Read(f,(char *)mod,st.st_size))
	{
		Close(f);module_free(mod,st.st_size);return false;
	}

	Close(f);

	return true;
}

bool print_name(uint8 *addr)
{
	uint16 i;

	for(i=0;i<22;i++)
	{
		if(addr[i]!=0)
			printf("%c",addr[i]);
		else
			break;
	}

	printf("\n");

	return true;
}

/* return codes */
#define AN_COMPRESSED 1
#define AN_FORMAT     2
#define AN_MEMORY     3
#define AN_INTERNAL   4

/* convert samples in module to freq representations */
module_analyse(ProTrackerModule *mod,FreqSample *sampleb)
{
	int16 i,j,imin;
	uint32 ssize,smin;
	FreqSample *fs;
	ProTrackerSample *sample;
	int8 *samples;

	/* make sure the data is uncompressed ie. 'PACK' is not the song name */
	if(mod->song_Name[0]=='P' && mod->song_Name[1]=='A' &&
		mod->song_Name[2]=='C' && mod->song_Name[3]=='K')
		return AN_COMPRESSED;

	/* make sure its a 31 sample protracker module */
	/* Startrekker uses FLT8, ProTracker uses M.K. */
	if(mod->song_Format != 'M.K.' && mod->song_Format != 'FLT8')
		return AN_FORMAT;

	/* Find highest pattern # */
	j=0;
	for(i=0;i<mod->song_Length;i++)
	{
		if(mod->song_Patterns[i] > j)
			j = mod->song_Patterns[i];
	}

	printf("\nModule (P:%3d,L:%3d) : ",mod->song_Length,j); 
	print_name(&mod->song_Name[0]);

	printf("\n");

	if(false==convert_INIT())
		return AN_INTERNAL;

	/* initialise sample addresses,lengths */
	samples = (uint8 *)(&mod->song_Pattern[j+1]);

	for(i=0;i<31;i++)
	{
		fs = &sampleb[i];

		fs->amplitude	=	samples;
		fs->amp_len	=	mod->song_Sample[i].sample_Length *2;
		fs->n		=	i+1;
		samples		+=	fs->amp_len;
	}

	/* process the samples */
	do
	{
		/* select the samples of the lowest to highest size */
		imin=-1;

		for(i=0;i<31;i++)
		{
			fs=&sampleb[i];

			if((fs->power == NULL) && (fs->amp_len != 0))
			{
				if((imin < 0) || (fs->amp_len < smin))
				{
					 smin=fs->amp_len;
					 imin=i;
				} 
			}
		}

		if(imin >= 0)
		{

			sample=&mod->song_Sample[imin];
			fs    =&sampleb[imin];

			fs->volume    = sample->sample_Volume;

			if(sample->sample_Repeat_Length <= 1)
				fs->power_rep_enable = false;
			else
			{
				fs->power_rep_enable = true;
				fs->power_rep_len    = sample->sample_Repeat_Length *2;
				fs->power_rep_start  = sample->sample_Repeat * 2;
			}

			if(fs->amp_len!=0)
			 {
			  printf("Sample (N:%2d,L:%5ld): ",imin+1,fs->amp_len);

			  print_name(&sample->sample_Name[0]);

			  if(false==convert_DOMAIN(fs->amplitude,fs->amp_len,sample->sample_Volume,&fs->power,&fs->power_len))
			  {
				convert_CLOSE();

				return AN_MEMORY;
			  }
			}
		}

	} while(imin >= 0);

	convert_CLOSE();

	printf("\n");

	return 0;
}

FreqSample samples[31];
void *mt_data=NULL;
struct GfxBase *GfxBase=NULL;

/* Trigger structure located in pro-tracker play routine */
typedef struct TRIGGER {
int16	number;		/* 1..31 */
uint16  period;		/* 120..65535 */
uint32	channel;	/* Chip address $dff0a0,b0,c0,d0  */
uint16  volume;		/* 0..64 */
} TRIGGER;

extern TRIGGER Trigger;

/* glue from pro-tracker play routine. this triggers a sample */
void module_trigger(void)
{
	FreqSample *fs=&samples[Trigger.number-1];

	/* call the display trigger routine */
	if(fs->power != NULL)
	{
 	 display_TRIGGER(	((Trigger.channel & 0xff)-0xa0)>>4,
				fs,
				Trigger.period);
	}
}

/* glue from pro-tracker play routine. this modulates the period of a sample */
/* uses trigger structure as above */
void module_period(void)
{
	/* call the display modulate routine */

	display_PERIOD(		((Trigger.channel & 0xff)-0xa0)>>4,
				Trigger.period);
}

/* glue from pro-tracker play routine. this modulates the volume of a sample */
/* uses trigger structure as above */
void module_volume(void)
{
	/* call the display modulate routine */

	display_VOLUME(		((Trigger.channel & 0xff)-0xa0)>>4,
				Trigger.volume);
}
	
/* DMA WAIT value used by play routine (CPU dependent) */
UWORD DMAWAIT=300;

/* System Vertical Blanking Frequency */
uint16 VBlankFreq;

real_main(int argc,char **argv)
{
	ProTrackerModule *mod;
	uint32 size;
	bool use_parallel;
	int16 i,oldpri,rc;
	char file[64];

	VBlankFreq = SysBase->VBlankFrequency;

	if(false==req_INIT())
	{	printf("Can't open reqtools.library\n"); return 1; }

	if(NULL==(GfxBase=OpenLibrary("graphics.library",0)))
	{	req_CLOSE(); printf("Can't open graphics.library\n"); return 2; }

	use_parallel=false;

	/* scan the DMAWAIT value if included */
	if(argc > 1)
	{	
		DMAWAIT=atoi(argv[1]);

		if(argc > 2)
		{
			use_parallel=true;

			if(false==par_PARSE(argv[2]))
			{
				req_CLOSE(); 
				CloseLibrary(GfxBase);

				printf("Can't parse definition file %s\n",argv[2]);
				return 3;
			}
		}
	}

	while(true==req_FILE(file))
	{
		if(true==module_load(file,&mod,&size))
		{

			/* reset the sample buffers */
			for(i=0;i<31;i++) samples[i].power=NULL;

			/* convert the samples to frequency representations */
			rc=module_analyse(mod,samples);

			if(rc==0 || rc==AN_MEMORY)
			{

				if(rc==AN_MEMORY)
					printf("Warning: Not all samples were processed due to low memory\n");

				if(true==display_INIT(use_parallel))
				{
					/* boot up the priority to stop interference from the system (critical) */

					oldpri=changepri(21);

					mt_data=mod;
					mt_init();

					/* go until we are aborted */
					while(display_CHECK()==false)
		  			{
						/* wait for vbi */
						WaitTOF();

						/* call pulse width modulation code */
						if(use_parallel==true)
							par_TRIGGER();

						/* play music */
						mt_music();

						/* call spectrogram display tick routine */
						display_TICK();
		  			}

					mt_end();

					changepri(oldpri);

				 display_CLOSE();

				}
				else
					printf("Error: Can't create display\n");
			}
			else
			{
				printf("Error: ");

				switch(rc) {
					default:
					case AN_INTERNAL:	printf("Out of memory\n"); break;
					case AN_COMPRESSED:	printf("This module is compressed\n"); break;
					case AN_FORMAT:		printf("Unrecognised module format\n"); break;
				}
		
				printf("\n");
			}

		/* shut down */
		for(i=0;i<31;i++)
		{
			if(samples[i].power!=NULL)
				convert_FREE(samples[i].power,samples[i].power_len);
		}

		module_free(mod,size);
	 	}
	}

	CloseLibrary(GfxBase);
	req_CLOSE();

	printf("Protracker Stereo Spectrum Analyser (V1.1a)\n");
	printf("Written by Lindsay Meek. (meek@fizzy.csu.murdoch.edu.au)\n");

	return 0;
}
