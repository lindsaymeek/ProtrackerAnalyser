
/*

Program:	Parallel port handling code
Author:		Lindsay Meek
Version:	20/4/94

*/

#include "parallel.h"
#include "display.h"

#include <exec/tasks.h>
#include <exec/interrupts.h>
#include <hardware/cia.h>
#include <stdio.h>
#include <stdlib.h>
#include <libraries/dos.h>
#include <resources/cia.h>

bool modulation,irq;
uint16	Frame_Constant;

extern uint16 VBlankFreq;
extern void par_INTERRUPT();

#ifdef use_sim
#include "sim.h"
#endif

/* CIA resource calls */
struct MiscResource;
long AbleICR(struct MiscResource *resource, long mask);
struct Interrupt *AddICRVector(struct MiscResource *resource, long iCRBit, struct Interrupt *interrupt);
void RemICRVector(struct MiscResource *resource, long iCRBit, struct Interrupt *interrupt);
long SetICR(struct MiscResource *resource, long mask);
struct MiscResource *OpenResource(char *);

struct MiscResource *ciabase=NULL;

struct Interrupt parIRQ;

bool parIRQ_INIT()
{
	if(NULL==(ciabase=OpenResource(CIABNAME)))
	{ 
		printf("Couldn't aquire %s\n",CIABNAME); 

		return false; 
	}

	parIRQ.is_Node.ln_Type 	= NT_INTERRUPT;
	parIRQ.is_Node.ln_Pri  	= 127;
	parIRQ.is_Node.ln_Name 	= "Pulse Modulator";
	parIRQ.is_Data 		= NULL;
	parIRQ.is_Code 		= par_INTERRUPT;

	par_DISABLE();

	AddICRVector(ciabase,CIAICRB_TA,&parIRQ);

	irq=true;

	SetICR(ciabase,CIAICRF_TA | CIAICRF_SETCLR);	/* Enable IRQ */

	return true;
}

bool parIRQ_CLOSE()
{
	par_DISABLE();

	SetICR(ciabase,CIAICRF_TA);			/* Disable interrupt */

	RemICRVector(ciabase,CIAICRB_TA,&parIRQ);	/* Remove IRQ server */

	return true;
}

bool par_INIT(void)
{
	irq=false;

	par_INITh();

	if(modulation==true)
	{
		if(false==parIRQ_INIT())
		{
			return false;
		}
	}

#ifdef use_sim
	return(sim_INIT());
#endif

#ifndef use_sim
	return true;
#endif
}

bool par_CLOSE(void)
{
	par_CLOSEh();

#ifdef use_sim
	sim_CLOSE();
#endif
	
	if(irq==true)
	{
		irq=false;

		parIRQ_CLOSE();
	}

	return true;
}

#asm

	include	"hardware/cia.i"

_ciaa	=	$bfe001		;i defined it here so i could use the small model!
_ciab	=	$bfd000		;i defined it here so i could use the small model!

	dseg
	global	_ddrb,1
	global	_portb,1
	cseg

	
;
;void par_INITh(void)
;
	public	_par_INITh

_par_INITh:

	move.b	_ciaa+ciaddrb,d0
	move.b	d0,_ddrb
	move.b	_ciaa+ciaprb,d0
	move.b	d0,_portb

	clr.b	_ciaa+ciaprb
	move.b	#255,_ciaa+ciaddrb
	rts

;
;void par_CLOSEh(void)
;
	public	_par_CLOSEh

_par_CLOSEh:

	move.b	_portb,_ciaa+ciaprb
	move.b	_ddrb,_ciaa+ciaddrb
	rts

;
;void par_ONh(uint16 bit)
;
	
	public	_par_ONh

_par_ONh:

	move.l	4(sp),d0
	bset	d0,_ciaa+ciaprb
	rts

;
;void par_OFFh(uint16 bit)
;

	public	_par_OFFh

_par_OFFh:

	move.l	4(sp),d0
	bclr	d0,_ciaa+ciaprb
	rts

;
;void par_DISABLE(void) (disable timerA)
;
	public	_par_DISABLE

_par_DISABLE:

	clr.b	_ciab+ciacra
	rts

;
;void par_ENABLE(uint16 timer) (enable timerA)
;
	public	_par_ENABLE

_par_ENABLE:

	clr.b	_ciab+ciacra
	move.l	4(sp),d0
	move.b	d0,_ciab+ciatalo
	lsr.w	#8,d0
	move.b	d0,_ciab+ciatahi
	move.b	#16+1+8,_ciab+ciacra
	rts

;
;void par_INTERRUPT() (handle timerA underflow)
;
	public	_par_INTERRUPT

	XREF	_pulse_index
	XREF	_pulse_list
	XREF	_pulse_size
	XREF	_geta4

_par_INTERRUPT:

	movem.l	d2/d1/a0/a1/a4,-(sp)
	
	jsr	_geta4

	tst.w	_pulse_size		;exit if called
	beq.s	parie

	clr.b	_ciab+ciacra		;stop timer

pari:	move.w	_pulse_index,d0
	move.w	d0,d1
	asl.w	#2,d1
	lea	_pulse_list,a0
	move.l	0(a0,d1.w),a1		;fetch pulse

	move.b	(a1),d2
	bclr	d2,_ciaa+ciaprb		;turn off parallel port bit
	
	addq.w	#1,d0
	cmp.w	_pulse_size,d0		;exit if no more pulses
	bge.s	parie

	move.w	d0,_pulse_index		;update index
	move.l	4(a0,d1.w),a0		;next pulse

	move.w	2(a0),d0		;fetch timer
	sub.w	2(a1),d0		;subtract last timer
	beq.s	pari			;<=0 .. process new value
	
	move.b	d0,_ciab+ciatalo	;program timer
	lsr.w	#8,d0
	move.b	d0,_ciab+ciatahi
	move.b	#1+16+8,_ciab+ciacra	;start timer

parie:	movem.l	(sp)+,d2/d1/a0/a1/a4

	moveq	#0,d0
	rts

#endasm

void par_ON(uint16 bit)
{
	par_ONh(bit);

#ifdef use_sim
	sim_ON(bit);
#endif
}

void par_OFF(uint16 bit)
{
	par_OFFh(bit);

#ifdef use_sim
	sim_OFF(bit);
#endif
}

/* parallel port trigger structure */
/* do not change as interrupt depends on bit,timer fields */
typedef struct PAR_TRIG {
uint8	bit;		/* bit of parallel port to trigger */
uint8	value;		/* test value */

uint16  timer;		/* timer value for pulse modulation */

uint8	first,last;	/* first, last bar */

uint8	combine;	/* 0=maximum 1=average (freq spectrum integration) */
uint8	test;		/* 0=bge 1=blt (test) 2=pulse */

uint16  count;		/* size of circular buffer */
uint16	index;		/* index into circular buffer for storage */
uint16  accum;		/* accumulator */
uint8   *buf;		/* circular buffer */

} PAR_TRIG;

typedef struct SAM_TRIG {
uint16	n,bit;
uint16  nest,pad;
} SAM_TRIG;

uint16 		trig_max=0;
PAR_TRIG 	par_trigs[8];
uint16 		pulse_size=0,pulse_index;
PAR_TRIG 	*pulse_list[8];
uint16		sample_max=0;
SAM_TRIG	sam_trigs[31];

/* Add a check to the triggering list */
bool par_ADD(uint16 freq1,uint16 freq2,uint16 combine,uint16 test,uint16 value,uint16 bit,uint16 frames)
{
	uint16 n1,n2,i;
	PAR_TRIG *pt;

	if(trig_max >= 8)
		return false;

	if(test != 3)
	{
	/* screen out silly buggers */
	if(freq1 > freq2) return false;
	if(freq2 > 20000) return false;
	if(bit < 0 || bit > 7) return false;

	if((test!=2) && (value < 0 || value > 255)) return false;
	}

	for(i=0;i<trig_max;i++)
	{
		if(par_trigs[i].bit == bit) return false;
	}

	pt=&par_trigs[trig_max]; trig_max++;

	if(test != 3)
	{
	/* work out which bars */
	n1=(DISPLAY_BARS*freq1)/DISPLAY_FREQ;
	n2=(DISPLAY_BARS*freq2)/DISPLAY_FREQ;
	
	pt->first = (uint8)n1;
	pt->last  = (uint8)n2;

	pt->combine = combine;
	pt->index   = 0;
	pt->count   = frames;
	pt->accum   = 0;

	if(NULL==(pt->buf=malloc(sizeof(uint8)*frames)))
		return false;	

	for(i=0;i<frames;i++)
		pt->buf[i]=0;
	}
	else
		pt->buf = NULL;

	pt->test    = test;
	pt->bit     = (uint8)bit;
	pt->value   = (uint8)(value & 0xff);

	/* setup modulation table stuff */
	if(test==2)
	{	
		pulse_list[pulse_size++] = pt;

		if(test==2)
			pt->timer = Frame_Constant;

		modulation=true;
	}

	return true;
}

/* Reset modulation table (call every raster (20ms)) */
bool par_TRIGGER(void)
{
	uint16 i,j;
	bool swap;
	PAR_TRIG *pt1,*pt2;

	if(modulation==true)
	{
		/* disable timer interrupt */
		par_DISABLE();

		/* turn on all the lights at the start of frame */
		for(i=0;i<pulse_size;i++)
			par_ONh(pulse_list[i]->bit);

		/* bubble sort the pulse modulations from lowest to highest timer */
		swap=true;
		while(swap==true)
		{
			swap=false;

			pt1=pulse_list[0];

			for(i=1;i<pulse_size;i++)
			{
				pt2=pulse_list[i];
			
				if(pt2->timer < pt1->timer)
				{
					swap=true;
	
					pulse_list[i-1]=pt2;
					pulse_list[i]=pt1;
				}
				else
					pt1=pt2;
			}
		}

		/* program the first interrupt */
		pulse_index=0;

		par_ENABLE(pulse_list[0]->timer);

	}

	return true;
}


/* Check for triggering conditions on parallel port */
bool par_CHECK(uint8 *left,uint8 *right)
{
	uint16 i,j,val,n;
	PAR_TRIG *pt;

	for(i=0;i<trig_max;i++)
	{
		pt=&par_trigs[i];

		if(pt->test != 3)
		{
 		 if(pt->combine)
		 {
			val=0;n=0;

			/* average value in interval */
			for(j=pt->first;j<=pt->last;j++)
			{
				val += (left[j]+right[j]); n++;
			}

			if(n > 1)
				val /= n;
		}
		else
		{
			val=0;

			/* maximum value in interval */
			for(j=pt->first;j<=pt->last;j++)
			{
				if(left[j]  > val) val=left[j];
				if(right[j] > val) val=right[j];
			}
		}

		/* compute accumulated average using temporal filtering */
		pt->accum = pt->accum - pt->buf[pt->index] + val;

		/* update buffer */
		pt->buf[pt->index++] = val;
		if(pt->index >= pt->count)
			pt->index=0;

		/* compute average value over time */
		val = (uint8)(pt->accum/pt->count);

		/* perform test and set parallel port light */	
		switch(pt->test) {
			case 1:
				if(val < pt->value)
					par_ON(pt->bit);
				else
					par_OFF(pt->bit);
				break;
			case 0:
				if(val >= pt->value)
					par_ON(pt->bit);
				else
					par_OFF(pt->bit);
				break;
			case 2:
				/* setup timer constant for pulse modulation */
				pt->timer = (Frame_Constant * (val+1)) >> 8;
				break;
		 }
		}
	}

	return true;
}

/* sample triggering functions */
bool par_SAM_START(uint16 n)
{
	uint16 i;
	SAM_TRIG *st;

	for(i=0;i<sample_max;i++)
	{
		st=&sam_trigs[i];

		if(st->n == n)
		{
			if(st->nest++ == 0)
				par_ONh(st->bit);

			return true;
		}
	}
	return false;
}

bool par_SAM_STOP(uint16 n)
{
	uint16 i;
	SAM_TRIG *st;

	for(i=0;i<sample_max;i++)
	{
		st=&sam_trigs[i];

		if(st->n == n)
		{
			if(st->nest > 0)
			{
				if(--st->nest == 0)
					par_OFFh(st->bit);
			}
			return true;
		}
	}
	return false;
}

char str[128];

/* parse a definition file */
bool par_PARSE(text *file)
{
	FILE *f=fopen(file,"r");
	int bit,f1,f2,combine,val,test,frames,i,num;
	bool ok;

	if(f==NULL) return false;

	switch(VBlankFreq) {
		case 50:
			Frame_Constant = 709379 / 50;
			break;
		case 60:
			Frame_Constant = 715909 / 60;
			break;
		default:
			Frame_Constant = 709379 / VBlankFreq;
			break;
	}

	modulation=false;

	while(!feof(f))
	{
		if(0==fscanf(f,"%d",&bit)) 	/* port bit */
			break;

		if(feof(f)) break;

		if(0==fscanf(f,"%s",str))	/* mode */
			break;

		if(0==stricmp(str,"freq"))
		{
		if(0==fscanf(f,"%d",&f1))	/* lower freq */
			break;
		if(0==fscanf(f,"%d",&f2))	/* upper freq */
			break;
		if(0==fscanf(f,"%s",str))	/* max/ave over range */
			break;

		if(0==stricmp(str,"max"))
			combine=0;
		else if(0==stricmp(str,"ave"))
			combine=1;
		else
			break;

		if(0==fscanf(f,"%d",&frames))	/* frame integration */
			break;

		if(0==fscanf(f,"%s",str))	/* test */
			break;

		if(0==stricmp(str,"<"))
			test=1;
		else if(0==stricmp(str,">=") || 0==stricmp(str,">"))
			test=0;
		else if(0==stricmp(str,"pulse"))
			test=2;
		else
			break;
		
		if(test != 2)
		{
 			if(0==fscanf(f,"%d",&val))	/* value for comparisons */
				break;
		}

		if(false==par_ADD(f1,f2,combine,test,val,bit,frames))
		 {
			fclose(f);
			return false;
		 }
		}
		else if(0==stricmp(str,"sample"))
		{
			if(0==fscanf(f,"%d",&num))
				break;

			if(num < 1 || num > 31)
				break;

			if(sample_max >= 31) break;

			sam_trigs[sample_max].n 	= num;
			sam_trigs[sample_max].nest 	= 0;
			sam_trigs[sample_max++].bit 	= bit;
		}
		else
			break;
	}

	if(feof(f))
		ok=true;
	else
		ok=false;

	fclose(f);

	return ok;
}
