

#include <string.h>
#include "MIMXRT1176_cm7.h"

int _write(int iFileHandle, const char *pcBuffer, int iLength);

int ITM_Init(void)
{
	/* nothing to do, pinmux and clock config is done already */
    return 0;		//error free
}

void ITM_PrintChar(unsigned char chr)
{
	ITM_SendChar((uint32_t)chr);
}

void ITM_PrintString(const char *s)
{
	_write(1, s, strlen(s));
}
