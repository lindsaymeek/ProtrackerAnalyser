
#ifndef _PARALLEL_H
#define _PARALLEL_H

#include "standard.h"

bool par_INIT();
bool par_CLOSE();
void par_ON(uint16 bit);	/* Set a bit on the parallel port */
void par_OFF(uint16 bit);	/* Clear a bit on the parallel port */

bool par_CHECK(uint8 *left,uint8 *right);	/* check trigger code for a given spectrum */
bool par_PARSE(text *file);	/* parse a definition file */
bool par_TRIGGER();		/* trigger called every vblank */

bool par_SAM_START(uint16 n);
bool par_SAM_STOP(uint16 n);

#define use_sim

#endif
