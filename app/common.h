
/*************************************************************************
 *
 *  Copyright (C) 2018  NextAI Technologies, Inc.  All rights reserved. **
 *
 *************************************************************************/

/* $Id$ */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include "stm32f0xx.h"

#ifdef MAIN
#define GBLDEF(name,value)        name = value
#else
#define GBLDEF(name,value)        extern name
#endif

#ifdef DEBUG 
#define EPRINTF(a) printf a
#define DPRINTF(a) printf a
#define FPRINTF(a) printf a

#define assert(ex)  do {                                               \
                          if (!(ex)) {                                 \
	                      (void)printf("Assertion failed: file \"%s\", \
    	                  line %d\n", __FILE__, __LINE__);             \
                          }                                            \
                        } while (0)

#else
#define FPRINTF(a) printf a
#define EPRINTF(a)
#define DPRINTF(a)

#define assert(ex)
#endif

//#define UNUSED(x) ((void)(x))
#define MAX3(a,b,c) (a>b?(a>c?a:c):(b>c?b:c))

#define DisableMIRQ __asm volatile ( " cpsid i " ::: "memory")
#define EnableMIRQ  __asm volatile ( " cpsie i " ::: "memory")

#endif

