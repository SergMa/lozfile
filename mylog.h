/******************************************************************************/
/* mylog.h                                                                    */
/* LOG-OUT UTILITES                                                           */
/*                                                                            */
/* Copyright (c) 2016 Sergey Mashkin                                          */
/******************************************************************************/

#ifndef MYLOG_H
#define MYLOG_H

#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>

extern int _logs_enabled;

//---- FUNCTIONS ---------------------------------------------

void  mylog_out ( const char * logtype, int line, const char * func, const char * format, ... );

#define MYLOGDEVICE_NOLOGS     0
#define MYLOGDEVICE_STDOUT     1

#ifndef MYLOGDEVICE
#define MYLOGDEVICE MYLOGDEVICE_STDOUT
#endif

#define MYLOG_ENABLED_NO        (0)
#define MYLOG_ENABLED_USER      (1<<0)
#define MYLOG_ENABLED_ERROR     (1<<1)
#define MYLOG_ENABLED_WARNING   (1<<2)
#define MYLOG_ENABLED_EVENT     (1<<3)
#define MYLOG_ENABLED_MESSAGE   (1<<4)
#define MYLOG_ENABLED_STATE     (1<<5)
#define MYLOG_ENABLED_DEBUG     (1<<6)
#define MYLOG_ENABLED_TRACE     (1<<7)
#define MYLOG_ENABLED_GOD       (1<<8)
#define MYLOG_ENABLED_REALTIME  (1<<9)
#define MYLOG_ENABLED_ALL       (0xFFFF)

    //////////////////////////////////////////////////////////////
#if MYLOGDEVICE == MYLOGDEVICE_NOLOGS
    //////////////////////////////////////////////////////////////

    #define MYLOG_INIT(levels)
    
    #define MYLOG_SCREEN(mess...) \
        do{                       \
            printf(mess);         \
            printf("\n");         \
        }while(0)

    #define MYLOG_USER(mess...)
    #define MYLOG_ERROR(mess...)
    #define MYLOG_WARNING(mess...)
    #define MYLOG_EVENT(mess...)
    #define MYLOG_MESSAGE(mess...)
    #define MYLOG_STATE(mess...)
    #define MYLOG_DEBUG(mess...)
    #define MYLOG_TRACE(mess...)
    #define MYLOG_GOD(mess...)
    #define MYLOG_REALTIME(mess...)

    //////////////////////////////////////////////////////////////
#elif MYLOGDEVICE == MYLOGDEVICE_STDOUT
    //////////////////////////////////////////////////////////////

    #define MYLOG_INIT(levels)                                    \
        do{                                                       \
            _logs_enabled = levels;                               \
        }while(0)

    #define MYLOG_SCREEN(mess...)                                 \
        do{                                                       \
            printf(mess);                                         \
            printf("\n");                                         \
        }while(0)

    #define MYLOG_USER(mess...)                                   \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_USER) {              \
                mylog_out("USE",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_ERROR(mess...)                                  \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_ERROR) {             \
                mylog_out("ERR",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_WARNING(mess...)                                \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_WARNING) {           \
                mylog_out("WAR",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_EVENT(mess...)                                  \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_EVENT) {             \
                mylog_out("EVE",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_MESSAGE(mess...)                                \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_MESSAGE) {           \
                mylog_out("MES",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_STATE(mess...)                                  \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_STATE) {             \
                mylog_out("STA",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_DEBUG(mess...)                                  \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_DEBUG) {             \
                mylog_out("DEB",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_TRACE(mess...)                                  \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_TRACE) {             \
                mylog_out("TRA",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_GOD(mess...)                                    \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_GOD) {               \
                mylog_out("GOD",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    #define MYLOG_REALTIME(mess...)                               \
        do{                                                       \
            if(_logs_enabled & MYLOG_ENABLED_REALTIME) {          \
                mylog_out("REA",__LINE__,__FUNCTION__,mess);      \
            }                                                     \
        }while(0)

    //////////////////////////////////////////////////////////////
#endif // MYLOGDEVICE
    //////////////////////////////////////////////////////////////

#endif //MYLOG_H
