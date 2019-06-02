
/*

Program:	Spectrogram display routines
Author:		Lindsay Meek
Version:	10/4/94

*/

#include "display.h"
#include "lang.h"
#include "parallel.h"

#include <exec/ports.h>
#include <exec/memory.h>

void display_PROCESS(void);
bool display_ABORT;

/* power spectrum (last values) */
typedef struct POWER {
bool  active,changed;
FreqSample *s;
uint8 *power;	/* offset to power data */
int32 length;	/* length of sample */
int32 frame;	/* frame # */
uint32 freq;	/* playback freq */
bool repeating;	/* repeating */
uint16 volume;	/* volume 0..64 */
} POWER;

POWER channel[4];

/* Display bars */
uint8 old[2][POWER_SIZE*2];
uint8 new[2][POWER_SIZE*2];

struct MsgPort *BackPort=NULL;		/* Startup protocol for process */
struct Message BackMsg;

struct Task *DisplayProc=NULL;		/* TCB for display process */
ULONG DisplayMask03=0;			/* Mask for channel 0,2 mods */
ULONG DisplayMask12=0;			/* Mask for channel 1,3 mods */
void *CreatePort(),*GetMsg();

bool   use_para;
uint32 *bars[256],*barmem;
uint32 *buf03,*buf12;
uint16 sqr_table[256];
bool   close_window;
extern uint16 VBlankFreq;

void *AllocMem();

/* stop the spectrogram process */
bool display_CLOSE()
{
	display_ABORT=true;

	if(DisplayProc!=NULL)
		Signal(DisplayProc,DisplayMask03);

	/* wait for the forked task to finish */
	fork_wait();

	if(barmem != NULL)	FreeMem(barmem,32*256);
	if(buf03 != NULL)	FreeMem(buf03,32*POWER_SIZE*2);
	if(buf12 != NULL)	FreeMem(buf12,32*POWER_SIZE*2);

	return true;
}

/* create the spectrogram process */
bool display_INIT(bool use_parallel)
{
	int16 i,j,k;
	POWER *ch;
	uint32 *bar,v;
	struct Message *msg;

	display_ABORT=false;

	buf03=NULL;buf12=NULL;barmem=NULL;
	close_window=false;

	/* initialise bar graphics */
	if(NULL==(barmem=AllocMem(32*256,MEMF_PUBLIC)))
		return false;
	if(NULL==(buf03=AllocMem(32*POWER_SIZE*2,MEMF_CHIP)))
		return false;
	if(NULL==(buf12=AllocMem(32*POWER_SIZE*2,MEMF_CHIP)))
		return false;

	bar=barmem;

	/* generate graphics */
	for(i=0;i<256;i++)
	{
		bars[i]=bar;

		for(j=0;j<256;j+=32)
		{
			v=0;
			for(k=0;k<32;k++)
			{
				if((j+k) < i)
					v |= (1L<<(31-k));
			}
			bars[i][j >> 5]=v;

		}

		bar += 8;
	}

	/* clear display regions */
	for(i=0;i<POWER_SIZE*2;i++)
	{
		for(j=0;j<8;j++)
		{
			buf03[j+(i<<3)]=0;
			buf12[j+(i<<3)]=0;
		}
	}

	/* generate sqr table */
	for(i=0;i<256;i++)
		sqr_table[i]=i*i;

	/* initialise channels */
	for(i=0;i<4;i++)
	{
		ch=&channel[i];

		ch->active=false;
		ch->changed=false;
	}

	for(i=0;i<2;i++)
	{
		for(j=0;j<POWER_SIZE*2;j++)
			old[i][j]=0;
	}

	/* Create the initialisation port */
	if(NULL==(BackPort=CreatePort(NULL,0)))
		return false;

	use_para = use_parallel;

	/* Fork the process */
	if(0==fork(display_PROCESS,-1))
		return false;

	/* Wait for a response and eat it */
	WaitPort(BackPort);
	msg=GetMsg(BackPort);
	DeletePort(BackPort);

	if(msg->mn_Node.ln_Pri == 0)
		return true;
	else
	{
		display_CLOSE();

		return false;
	}

}

/* check to see if the window close has been received */
bool display_CHECK()
{
	return close_window;
}

/* advance spectrogram indexes (if active) */
/* must be called every 25 ms */
bool display_TICK()
{
	int16 i;
	uint32 offset;
	bool  redraw03,redraw12;
	POWER *ch;

	redraw03=false;
	redraw12=false;

	for(i=0;i<4;i++)
	{
		ch=&channel[i];

		if(ch->active==true)
		{
			ch->frame++;
			
			if(ch->repeating==false)
			{
				/* compute offset into sample buffer */
				offset = (ch->freq * ch->frame)/VBlankFreq;

				/* convert to power buffer offset */
				offset = (offset >> M) * POWER_SIZE;

				/* exceed length limit ? */
				if(offset >= ch->length)
				{
					/* is repeat used ? */
					if(ch->s->power_rep_enable == false)
					{
						par_SAM_STOP(ch->s->n);

						ch->active=false;
					}
					else
						ch->repeating=true;
				}
			}

			if(ch->repeating==true)
			{
				/* compute offset from repeat point */
				offset = ((ch->freq * ch->frame)/VBlankFreq)-ch->length;

				/* round to repeat length */
				offset = offset % ch->s->power_rep_len;

				/* compute offset in sample buffer */
				offset = offset + ch->s->power_rep_start;
				
				/* convert this to an offset within the power buffer */
				offset = (offset >> M)*POWER_SIZE;
			}

			if(ch->active==true)
				ch->power = &ch->s->power[offset];

			ch->changed=true;

			if(i==0 || i==3)
				redraw03=true;
			else
				redraw12=true;
		}
	}

	if(DisplayProc!=NULL)
	{
	 if(redraw03==true)
		Signal(DisplayProc,DisplayMask03);

 	 if(redraw12==true)
		Signal(DisplayProc,DisplayMask12);
	}

	return true;
}

/* trigger a new sample */
void display_TRIGGER(int16 chan,FreqSample *s,uint16 period)
{
	POWER *ch=&channel[chan];

	ch->changed     = true;

	if(s->power_len != 0)
	{	
		if(ch->active == true)
			par_SAM_STOP(ch->s->n);

		ch->active      = true;

		ch->s   	= s;
		ch->length      = s->power_len;
		ch->power	= s->power;
		ch->frame	= 0;
		ch->freq	= 3579546 / period;
		ch->repeating	= false;
		ch->volume	= s->volume;

		par_SAM_START(s->n);
	}
	else
		ch->active	= false;

	if(DisplayProc!=NULL)
	{
	if(chan==0 || chan==3)
		Signal(DisplayProc,DisplayMask03);
	else
		Signal(DisplayProc,DisplayMask12);
	}
}

/* modulate the period of a channel */
void display_PERIOD(int16 chan,uint16 period)
{
	POWER *ch=&channel[chan];

	if(ch->active==true)
	{
		ch->changed     = true;
		ch->freq	= 3579546 / period;
	}
}

/* modulate the volume of a channel */
void display_VOLUME(int16 chan,uint16 vol)
{
	POWER *ch=&channel[chan];

	if(ch->active==true)
	{
		ch->changed     = true;
		ch->volume	= vol;
	}
}

/* Display process (uses workbench windows) */

#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/view.h>

#define INTUITION_REV 0L
#define INT_NAME "intuition.library"

void *OpenLibrary(),*OpenWindow();

struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

void StripIntuiMessages(struct MsgPort *mp,struct Window *wind)
{
   struct IntuiMessage *msg,*succ;

   msg=(void *)mp->mp_MsgList.lh_Head;

   while(succ = (void *)msg->ExecMessage.mn_Node.ln_Succ)
   {
      if(msg->IDCMPWindow == wind)
      {
         Remove(msg);
         ReplyMsg(msg);
      }
   msg = succ;
   }
}

void CloseWindowSafely(struct Window *wind)
{
   Forbid();
   StripIntuiMessages(wind->UserPort,wind);
   wind->UserPort=NULL;
   ModifyIDCMP(wind,0L);
   Permit();
   CloseWindow(wind);
}

#define MY_WIDTH  (256+16)
#define MY_HEIGHT (96+16)

void SetAPen(struct RastPort *r,ULONG index);
void Move(struct RastPort *r,ULONG xo,ULONG yo);
void Draw(struct RastPort *r,ULONG xo,ULONG yo);

/* redraw the power spectrum (template version) */
void display_UPDATE(struct RastPort *r,uint8 *old,uint8 *new,uint32 *buf)
{
	uint8 o,n;
	int16 i,maxi;
	uint32 *bar,*mem;

	mem=buf;

	maxi=-1;

	for(i=0;i<POWER_SIZE*2;i++)
	{
		n=*new++;
		o=old[i];

		if(o != n)
		{
			bar=bars[n];

			*mem++=*bar++;
			*mem++=*bar++;
			*mem++=*bar++;
			*mem++=*bar++;

			*mem++=*bar++;
			*mem++=*bar++;
			*mem++=*bar++;
			*mem++=*bar++;

			old[i]=n;

			if(i > maxi) maxi=i;
		}
		else
			mem += 8;
	}


	if(maxi != -1)
		BltTemplate(buf,0,32,r,1,1,256,maxi+1);
}

/* process a channel and combine it into the main display buffer */
/* volume is also incorporated here */
bool display_PROC(POWER *ch,uint16 k)
{
	uint8 *buf,*pow;
	uint16 j,x,vol,n;
	uint32 l,m;
	bool changed=false;

	vol=ch->volume;

	if(ch->changed==true)
	{
		ch->changed=false;

		changed=true;

		if(ch->active==true)
		{
			buf = new[k];

			l=0;
			m=2*ch->freq;
			n=0;
			pow=ch->power;

			Forbid();

			for(j=0;j<POWER_SIZE;j++)
			{
				x=sqr_table[*pow++];

				/* clip to maximum */
				if(x>127) x=127;

				/* increment bar counter */
				while (l >= DISPLAY_FREQ)
				{
					l -= DISPLAY_FREQ;
					n ++;
				}

				if(n < POWER_SIZE*2)	
				{
					/* update appropriate bar */
					buf[n] += ((x * vol) >> 6);
				}

				l+=m;
			}

			Permit();
		}
	}

	return changed;
}

long AllocSignal();
void *FindTask();
ULONG Wait();

/* This is the display process which generates the spectrograms */
void display_PROCESS(void)
{
	int16 i;
	bool  redraw0,redraw1,signalled,parenabled;
	long sig03,sig12;
	ULONG retmask,IDCMPMask;
   	struct NewWindow window;
	struct Window *w[2];
	struct RastPort *r[2];
	struct MsgPort *IDCMP;
	struct Message *msg;

	for(i=0;i<2;i++)	w[i]=NULL;

	IDCMP=NULL;
	signalled=false;
	IntuitionBase=NULL;
	parenabled=false;

	sig03=-1;sig12=-1;

	if(-1==(sig03=AllocSignal(-1)))
		goto abort;
	if(-1==(sig12=AllocSignal(-1)))
		goto abort;

	DisplayProc=FindTask(NULL);
	DisplayMask03=1L<<sig03;
	DisplayMask12=1L<<sig12;

	if(NULL==(IDCMP=CreatePort("SpectroCloseGadget",0)))
		goto abort;

	if(NULL==(IntuitionBase=(struct IntuitionBase *)OpenLibrary(INT_NAME,INTUITION_REV)))
		goto abort;

	if(use_para == true)
	{
		if(false==par_INIT())
			goto abort;

		parenabled=true;
	}

   	window.Flags= ACTIVATE | GIMMEZEROZERO | WINDOWCLOSE |
        		WINDOWDRAG | WINDOWDEPTH |
            		NOCAREREFRESH ;

   	window.IDCMPFlags = NULL;  /* For the moment */

   	window.Type 		= WBENCHSCREEN;
   	window.FirstGadget 	= NULL;
   	window.CheckMark 	= NULL;
   	window.Screen 		= NULL;
   	window.BitMap 		= NULL;
   	window.Width		= MY_WIDTH;
   	window.Height		= MY_HEIGHT;
   	window.DetailPen	= 0;
   	window.BlockPen		= 1;

   	window.LeftEdge=0;
   	window.TopEdge =GfxBase->NormalDisplayRows-MY_HEIGHT;
   	window.Title="LEFT";

   	if(NULL==(w[0]=(struct Window *)OpenWindow(&window)))
		goto abort;
	
	window.Title="RIGHT";
   	window.LeftEdge=GfxBase->NormalDisplayColumns-MY_WIDTH;

   	if(NULL==(w[1]=(struct Window *)OpenWindow(&window)))
		goto abort;

	w[0]->UserPort = IDCMP;
	w[1]->UserPort = IDCMP;
	ModifyIDCMP(w[0],CLOSEWINDOW);
	ModifyIDCMP(w[1],CLOSEWINDOW);

	IDCMPMask = 1L<<IDCMP->mp_SigBit;

	for(i=0;i<2;i++)
	{
		r[i]=w[i]->RPort;

		SetDrMd(r[i],JAM2);	/* Completely redraw */
		SetAPen(r[i],1);
		SetBPen(r[i],0);
	}

	/* Tell the parent we're alive */	
	BackMsg.mn_Node.ln_Type=NT_MESSAGE;
	BackMsg.mn_Node.ln_Pri=0;
	PutMsg(BackPort,&BackMsg);
	signalled=true;

	while(display_ABORT==false)
	{
		/* sync with the parent */
		retmask=Wait(DisplayMask03 | DisplayMask12 | IDCMPMask);

		/* check for close gadget */
		if(NULL != (msg=GetMsg(IDCMP)))
		{
			ReplyMsg(msg);
			close_window = true;
		}

		redraw0=false;redraw1=false;

		if(retmask & DisplayMask03)
		{
			memset(new[0],0,POWER_SIZE*2);

			if(true==display_PROC(&channel[0],0))
				redraw0=true;
			if(true==display_PROC(&channel[3],0))
				redraw0=true;

		}

		if(retmask & DisplayMask12)
		{
			memset(new[1],0,POWER_SIZE*2);

			if(true==display_PROC(&channel[1],1))
				redraw1=true;
			if(true==display_PROC(&channel[2],1))
				redraw1=true;
			
		}

		if(redraw0==true)
			display_UPDATE(r[0],old[0],new[0],buf03);

		if(redraw1==true)
			display_UPDATE(r[1],old[1],new[1],buf12);

		if(parenabled == true)
		{
			/* check for triggering frequencies */
 			if(redraw0==true || redraw1==true)
			{
				par_CHECK(new[0],new[1]);
			}
		}
	}

abort:

	Forbid();
	
	if(sig03 != -1)	FreeSignal(sig03);
	if(sig12 != -1)	FreeSignal(sig12);
	
	DisplayProc = NULL;

	Permit();

	for(i=0;i<2;i++)
	{
		if(w[i]!=NULL) 	CloseWindowSafely(w[i]);
	}

	if(IDCMP != NULL)	DeletePort(IDCMP);

	if(parenabled == true)
		par_CLOSE();

	if(IntuitionBase!=NULL) CloseLibrary(IntuitionBase);

	/* signal an error if we failed */
	if(signalled==false)
	{
	 BackMsg.mn_Node.ln_Type=NT_MESSAGE;
	 BackMsg.mn_Node.ln_Pri=1;
	 PutMsg(BackPort,&BackMsg);
	}
}
