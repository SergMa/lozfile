/******************************************************************************/
/* crc8.c                                                                     */
/* CRC8 UTILITES (IMPLEMENTATIONS)                                            */
/*                                                                            */
/* Autor(s): ?                                                                */
/******************************************************************************/

#include "crc8.h"

#define GP  0x107   /* x^8 + x^2 + x + 1 */
#define DI  0x07

static unsigned char crc8_table[256];     /* 8-bit table */
static char          made_table = 0;

/******************************************************************************/
/* PRIVATE FUNCTIONS                                                          */
/******************************************************************************/

//------------------------------------------------------------------------------
//Initialize table for fast calculation of CRC8
static void init_crc8()
{
        int i,j;
        unsigned char crc;

        for (i=0; i<256; i++) {
                crc = i;
                for (j=0; j<8; j++)
                        crc = (crc << 1) ^ ((crc & 0x80) ? DI : 0);
                crc8_table[i] = crc & 0xFF;
        }
        made_table=1;

        return;
}

/******************************************************************************/
/* FUNCTIONS                                                                  */
/******************************************************************************/

//------------------------------------------------------------------------------
//Calculate CRC8 for single byte
//Inputs:  data = byte
//         crc  = previous crc8 value (if not defined, use CRC8_INIT)
//Returns: crc8 = calculated CRC8
unsigned char crc8( unsigned char data, unsigned char crc )
{
        if (!made_table)
                init_crc8();

        return crc8_table[ (crc^data) & 0xFF ];
}

//------------------------------------------------------------------------------
//Calculate CRC8 for array of bytes
//Inputs:  data  = array of bytes
//         bytes = number of bytes in data[] array
//         crc   = previous crc8 value (if not defined, use CRC8_INIT)
//Returns: crc8  = calculated CRC8
unsigned char crc8_array( unsigned char * data, int bytes, unsigned char crc )
{
        if (!made_table)
                init_crc8();

	/* loop over the buffer data */
	while (bytes-- > 0)
		crc = crc8_table[ (crc^*data++) & 0xFF ];
	return crc;
}
