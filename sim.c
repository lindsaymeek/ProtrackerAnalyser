
/*

Program:	Parallel port simulator
Author:		Lindsay Meek
Version:	14/4/94

*/

#include "sim.h"

#include <exec/memory.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/view.h>

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

void *OpenWindow(),CloseWindowSafely(),*AllocMem();

#ifdef use_sim

struct Window *w=NULL;
uint8 *wdata=NULL;
uint8 wmask=0;

#define MY_WIDTH	(8*8+16)
#define MY_HEIGHT	(8+16)

bool sim_CLOSE(void)
{
	if(w!=NULL)
	{
		CloseWindowSafely(w);
		w=NULL;
	}

	if(wdata!=NULL)
	{
		FreeMem(wdata,sizeof(uint8)*8*8);
		wdata=NULL;
	}

	return true;

}

struct NewWindow window;

bool sim_INIT(void)
{

   	window.Flags= ACTIVATE | GIMMEZEROZERO |
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
   	window.TopEdge =0;
   	window.Title="PB0-7";

   	if(NULL==(w=OpenWindow(&window)))
	{ sim_CLOSE();	return false;	}

	SetDrMd(w->RPort,JAM2);
	SetAPen(w->RPort,1);
	SetBPen(w->RPort,0);

	if(NULL==(wdata=AllocMem(sizeof(uint8)*8*8,MEMF_CHIP)))
	{ sim_CLOSE(); return false;	}

	return true;
}

bool sim_REFRESH(void)
{
	int16 i,j;
	uint8 *b;

	/* generate the buffer */
	b=wdata;
	for(j=0;j<8;j++)
	{
		for(i=0;i<8;i++)
		{
			if(wmask & (1<<i))
				*b++=0xff;
			else
				*b++=0;
		}
	}

	BltTemplate(wdata,0,8,w->RPort,1,1,8*8,8);

	return true;
}

bool sim_ON(uint16 bit)
{
	uint8 newmask,pad;

	newmask = wmask | (1<<(bit & 7));

	if(wmask != newmask)
	{
		wmask = newmask;

		return(sim_REFRESH());
	}

	return true;
}

bool sim_OFF(uint16 bit)
{
	uint8 newmask,pad;

	newmask = wmask & (255-(1<<(bit & 7)));

	if(wmask != newmask)
	{
		wmask = newmask;

		return(sim_REFRESH());
	}

	return true;
}

#endif
