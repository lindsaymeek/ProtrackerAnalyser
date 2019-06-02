
#ifndef _CONVERT_H
#define _CONVERT_H

#include "standard.h"

bool convert_INIT(void);
bool convert_CLOSE(void);
bool convert_DOMAIN(int8 *sample,uint32 length,uint16 volume,uint8 **power,uint32 *pow_len);
bool convert_FREE(uint8 *power,uint32 length);

/* Call this every blank to advance display index */
bool convert_TICK(void);

#endif
