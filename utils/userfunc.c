/******************************************************************************
 * Функции перевода дат, проверка контрольной суммы т.ж. здесь 
 * Все функции считают время от начала Эпохи (01-01-1970)
 * Все функции с маленькой буквы
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "userfunc.h"



/**
 * Convert numeric IP address into decimal dotted ASCII representation.
 * returns ptr to static buffer; not reentrant!
 *
 * @param addr ip address in network order to convert
 * @return pointer to a global static (!) buffer that holds the ASCII
 *         represenation of addr
 */
char *inet_ntoa(struct in_addr addr)
{
    static char str[16]; /* ="localhost"; */
#if 1
    u32 s_addr = addr.s_addr;
    char inv[3];
    char *rp;
    u8 *ap;
    u8 rem;
    u8 n;
    u8 i;

    rp = str;
    ap = (u8 *) & s_addr;
    for (n = 0; n < 4; n++) {
	i = 0;
	do {
	    rem = *ap % (u8) 10;
	    *ap /= (u8) 10;
	    inv[i++] = '0' + rem;
	} while (*ap);
	while (i--)
	    *rp++ = inv[i];
	*rp++ = '.';
	ap++;
    }
    *--rp = 0;
#endif
    return str;
}

/**
 * Вывод на экран, для отладки - если нет сбора данных
 */
int log_term_printf(const char *fmt, ...)
{
    char buf[256];
    int ret = 0, i = 0;
    va_list list;

    va_start(list, fmt);
    ret = vsnprintf(buf, sizeof(buf), fmt, list);
    va_end(list);

    if (ret < 0) {
	return ret;
    }

    while (buf[i] != '\0') {
	MAP_UARTCharPut(UARTA0_BASE, buf[i++]);
    }
    return ret;
}
