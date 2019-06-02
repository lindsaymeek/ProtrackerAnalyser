
#ifndef _SIM_H
#define _SIM_H

#include "standard.h"
#include "parallel.h"

bool sim_INIT();
bool sim_CLOSE();
bool sim_ON(uint16 bit);
bool sim_OFF(uint16 bit);

#endif
