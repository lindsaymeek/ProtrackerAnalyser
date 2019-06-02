
/*

Program:	Generate Power-Spectrum/Time Data for a Sample
Author:		Lindsay Meek
Version:	27/3/94

*/

#include "fft.h"
#include "standard.h"
#include "convert.h"

#include <stdlib.h>

//#define debug

/* conversion data */
int16 x[N+1],qf[POWER_SIZE];

/* square root table */
uint8 *sqrtt=NULL;

/* 	
	generates an output array giving the power spectrum values for
	every n samples of a digitised signal. these are stored in
	256x8bit unsigned integer format.
*/

bool convert_INIT(void)
{
	uint16 i,j,k;

	/* generate sqrt table */

	if(NULL==(sqrtt=malloc(65536*sizeof(uint8))))
		return false;	

	j=0;
	for(i=1;i<256;i++)
	{
		k=i*i;

		while(j!=k)
			sqrtt[j++]=i-1;
	}

	return true;
}

bool convert_CLOSE(void)
{
	if(sqrtt!=NULL) free(sqrtt);

	return true;
}

/* convert a sample to the freq domain using a real(powerspectrum) fft */
bool convert_DOMAIN(int8 *samples,uint32 length,uint16 volume,
		    uint8 **power,uint32 *power_len)
{
	uint16 i,j,n;
	uint8 *pt;
	uint32 sam_win,pow_win,size;

	/* compute number of conversions in sample */
	n=length / N;

	/* compute size of power table */
	size=n*POWER_SIZE;

	/* allocate output power table */
	if(NULL==(pt=malloc(sizeof(uint8)*size))) return false;

	*power 		= pt;
	*power_len 	= size;

	/* produce the power windows */
	sam_win = 0;
	pow_win = 0;
	for(i=0;i<n;i++)
	{

#ifdef debug
		printf("sample offset %ld power offset %ld\n",sam_win,pow_win);
#endif

		for(j=1;j<(N-1);j++)
		{
			x[j]=samples[sam_win+j] << 8;
		}

		x[0]=0;x[N-1]=0;

		/* compute real-fft power spectrum */			
		fft();

		for(j=0;j<POWER_SIZE;j++)
		{
			pt[pow_win+j]=sqrtt[qf[j]];
		}

		sam_win += N;
		pow_win += POWER_SIZE;
	}

#ifdef debug
	printf("done\n");
#endif

	return true;
}

bool convert_FREE(uint8 *power,uint32 length)
{
	free(power);
	return true;
}
