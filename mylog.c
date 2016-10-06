/*************************************************************/
/* LOG-OUT UTILITES                                          */
/*                                                           */
/* (c) Mashkin S.V.                                          */
/*                                                           */
/*************************************************************/

#include "mylog.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if MYLOGDEVICE == MYLOGDEVICE_STDOUT

int _logs_enabled = 0x0000;

//------------------------------------------------------------
//Print out log message into log device (for MYLOG_USER)
//returns:  ---
void  mylog_out( const char * logtype, int line, const char * func, const char * format, ... )
{
    va_list   arg;
    char    * funcstr;
    int       funclen;

    funclen = strlen(func);
    if(funclen<=24)
            funcstr = (char*)func;
    else    funcstr = (char*)func + (funclen-24);


    printf( "%s:%24s:%6d: ", logtype, funcstr, line );
    va_start( arg, format );
    vprintf( format, arg );
    va_end( arg );
    printf("\n");
    fflush(stdout);
    return;
}

#endif /* #if MYLOGDEVICE == MYLOGDEVICE_STDOUT */
