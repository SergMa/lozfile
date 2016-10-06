/*************************************************************/
/* COMPRESSION FUNCTIONS                                     */
/*                                                           */
/* (c) Mashkin S.V.                                          */
/*                                                           */
/*************************************************************/

#include "compress_rle2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>


//---------------------------------------------------------------------------
//returns: size of compressed data
//         -1 = error
int rle_compress( uint8_t * data, int bytes, uint8_t * outdata, int outbytesmax )
{
    int     i;
    uint8_t x;
    int8_t  *pcntr;
    uint8_t *outdatabeg = outdata;
    uint8_t *outdataend = outdata + outbytesmax;

    if(!data)
        return -1;
    if(!outdata)
        return -1;
    if(bytes<0)
        return -1;
    if(bytes==0)
        return 0;

    pcntr = (int8_t*)outdata;
    outdata++;  if(outdata >= outdataend)  return -1;
    x = *data;
    *pcntr   = 1;
    *outdata = x;

    for(i=1; i<bytes; i++) {

        data++;

        if(*data==x) {
            if(*pcntr<0) {
                (*pcntr)++;
                pcntr = (int8_t*)outdata;
                outdata++;  if(outdata >= outdataend) return -1;
                *pcntr = 2;
                *outdata = *data;
            }
            else {
                if(*pcntr==127) {
                    outdata++;  if(outdata >= outdataend) return -1;
                    pcntr = (int8_t*)outdata;
                    outdata++;  if(outdata >= outdataend) return -1;
                    *pcntr   = 1;
                    *outdata = *data;
                }
                else {
                    (*pcntr)++;
                }
            }
        }
        else {
            if(*pcntr==1) {
                outdata++;  if(outdata >= outdataend) return -1;
                *pcntr   =-2;
                *outdata = *data;
            }
            else if(*pcntr>1) {
                outdata++;  if(outdata >= outdataend) return -1;
                pcntr = (int8_t*)outdata;
                outdata++;  if(outdata >= outdataend) return -1;
                *pcntr   = 1;
                *outdata = *data;
            }
            else {
                if(*pcntr==-127) {
                    outdata++;  if(outdata >= outdataend) return -1;
                    pcntr = (int8_t*)outdata;
                    outdata++;  if(outdata >= outdataend) return -1;
                    *pcntr   = 1;
                    *outdata = *data;
                }
                else {
                    outdata++;  if(outdata >= outdataend) return -1;
                    *outdata = *data;
                    (*pcntr)--;
                }
            }
        }
        x = *data;
    }
    outdata++;                                 

    return (outdata - outdatabeg);
}

//---------------------------------------------------------------------------
//returns: size of decompressed data
//         -1 = error
int rle_decompress( uint8_t * data, int bytes, uint8_t * outdata, int outbytesmax )
{
    int      i;
    int      cntr;
    uint8_t *outdatabeg = outdata;
    uint8_t *outdataend = outdata + outbytesmax;

    uint8_t *dataend = data + bytes;

    if(!data)
        return -1;
    if(!outdata)
        return -1;
    if(bytes<0)
        return -1;
    if(bytes==0)
        return 0;

    //n = 0;
    while(1) {
        cntr = *((int8_t*)data++);

        if(cntr>0) {
            if(outdata+cntr > outdataend)
                return -1; //not enough space for outdata
            for(i=0; i<cntr; i++)
                *outdata++ = *data;
            data++;
        }
        else if(cntr<0) {
            cntr = -cntr;
            if(outdata+cntr > outdataend)
                return -1; //not enough space for outdata
            if(data+cntr > dataend)
                return -1; //out of data
            for(i=0; i<cntr; i++)
                *outdata++ = *data++;
        }
        else {
            return -1; //invalid counter value
        }
        if(data >= dataend)
            break;
    }

    return (outdata - outdatabeg);
}

