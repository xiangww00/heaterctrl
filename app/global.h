
/*************************************************************************
 *
 *  Copyright (C) 2018  NextAI Technologies, Inc.  All rights reserved. **
 *
 *************************************************************************/

/* $Id$ */

#ifndef _GOBAL_H_
#define _GOBAL_H_

#include "common.h"


GBLDEF(volatile int64_t SysTicks, 0);          //1KHz
GBLDEF(volatile uint32_t P1Hz, 0);          

GBLDEF(volatile int8_t gEventUpdate, 0);

GBLDEF(uint32_t MainVersion , 0);

extern const char build_date[];
extern const char build_time[];

#endif
