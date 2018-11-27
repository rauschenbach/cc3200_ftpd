#ifndef _USERFUNC_H
#define _USERFUNC_H


#include <stdio.h>
#include "globdefs.h"
#include "main.h"

#define uDelay(x)	MAP_UtilsDelay(20 * x)
#define mDelay(x)	MAP_UtilsDelay(20000 * x)
#define PRINTF	    	log_term_printf



char *inet_ntoa(struct in_addr addr);
int  log_term_printf(const char *, ...);



#endif			/* userfunc.h */


