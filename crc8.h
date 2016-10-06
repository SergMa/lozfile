/******************************************************************************/
/* crc8.h                                                                     */
/* CRC8 UTILITES (DEFINITIONS)                                                */
/*                                                                            */
/* Autor(s): ?                                                                */
/******************************************************************************/

#ifndef CRC8_H
#define CRC8_H

#define CRC8_INIT  0xFF

/******************************************************************************/
/* FUNCTIONS                                                                  */
/******************************************************************************/

unsigned char crc8       ( unsigned char data, unsigned char crc );
unsigned char crc8_array ( unsigned char * data, int bytes, unsigned char crc );

#endif //CRC8_H
