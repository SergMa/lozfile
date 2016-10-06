/*************************************************************/
/* COMPRESSION FUNCTIONS                                     */
/*                                                           */
/* (c) Mashkin S.V.                                          */
/*                                                           */
/*************************************************************/

#ifndef COMPRESS_RLE2_H
#define COMPRESS_RLE2_H

#include "types.h"

int rle_compress   ( uint8_t * data, int bytes, uint8_t * outdata, int outbytesmax );
int rle_decompress ( uint8_t * data, int bytes, uint8_t * outdata, int outbytesmax );

#endif //COMPRESS_RLE2_H
