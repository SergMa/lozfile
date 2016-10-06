/******************************************************************************/
/* lozfile.h                                                                  */
/* LOZ-FILE INPUT OUTPUT LIBRARY (DEFINITIONS)                                */
/*                                                                            */
/* Copyright (c) 2016 Sergey Mashkin                                          */
/* e-mail: mashkh@yandex.ru                                                   */
/*                                                                            */
/* Includes:                                                                  */
/*                                                                            */
/* FastLZ - lightning-fast lossless compression library                       */
/* Author: Ariya Hidayat                                                      */
/* Official website: http://www.fastlz.org                                    */
/*                                                                            */
/* Compression library (LZ, RLE, Huffman, Shannon-Fano algorithms)            */
/* Author: Marcus Geelnard                                                    */
/* Official website: marcus.geelnard at home.se                               */
/*                                                                            */
/* RLE2 - another implementation of RLE compression algorithm                 */
/* Author: Sergei Mashkin                                                     */
/* e-mail: mashkh@yandex.ru                                                   */
/*                                                                            */
/******************************************************************************/

#ifndef LOZFILE_H
#define LOZFILE_H

#include "types.h"
#include <stdio.h>
#include <stdarg.h>

/******************************************************************************/
/* DESCRIPTION                                                                */
/******************************************************************************/

/* LOZ-file has format:
 * -File-header:--------------------
 * [ 0]   - FMT byte 'L'
 * [ 1]   - FMT byte 'O'
 * [ 2]   - FMT byte 'Z'
 * [ 3]   - 0x00 - version of LOZ-file format
 * [ 4]   - COMPRESSION, byte[1] - type of compression (0=NONE,1=RLE,2=LZO,...)
 * [ 5]   - File-header.CRC/VALID ([3..8]: 0=invalid, 1..255=CRC (0x00 result of crc8() is replaced by 0x01 value)
 * -Data-section:-------------------
 * [ 0]   - Section-begin-marker, byte[0] (0xFA)
 * [ 1]   - Section-begin-marker, byte[1] (0xF5)
 * [ 2]   - RAWPOS, byte[0] - unsigned int (start position of data in uncompressed raw file)
 * [ 3]   - RAWPOS, byte[1]
 * [ 4]   - RAWPOS, byte[2]
 * [ 5]   - RAWPOS, byte[3]
 * [ 6]   - RAWSIZE, byte[0] - unsigned int (uncompressed section data size)
 * [ 7]   - RAWSIZE, byte[1]
 * [ 8]   - RAWSIZE, byte[2]
 * [ 9]   - RAWSIZE, byte[3]
 * [10]   - COMPSIZE, byte[0] - unsigned int (compressed section data size)
 * [11]   - COMPSIZE, byte[1]
 * [12]   - COMPSIZE, byte[2]
 * [13]   - COMPSIZE, byte[3]
 * [14]   - Section-Header.CRC/VALID ([2..9]: 0=invalid, 1..255=CRC (0x00 result of crc8() is replaced by 0x01 value)
 * [..]   - Compressed-Data, byte[0]
 * [..]   - Compressed-Data, byte[1]
 * [..]   - Compressed-Data, byte[2]
 * [..]   - Compressed-Data, byte[3]
 * [XX]   - Compressed-Data.CRC/VALID: 0=invalid, 1..255=CRC (0x00 result of crc8() is replaced by 0x01 value)
 * -Data-section:-------------------
 * [  ]
 * [  ]
 *
 */

/******************************************************************************/
/* DEFINITIONS                                                                */
/******************************************************************************/

//compression formats
#define  LOZ_COMPRESSION_NONE       0
#define  LOZ_COMPRESSION_RLE        1
#define  LOZ_COMPRESSION_RLE2       2
#define  LOZ_COMPRESSION_LZ         3
#define  LOZ_COMPRESSION_FASTLZ1    4
#define  LOZ_COMPRESSION_FASTLZ2    5

#define  LOZ_COMPRESSION_MIN        LOZ_COMPRESSION_NONE
#define  LOZ_COMPRESSION_MAX        LOZ_COMPRESSION_FASTLZ2

#define  LOZ_VERSION_0              0x00

#define  LOZ_BLOCKSIZE_MIN          32
#define  LOZ_BLOCKSIZE_MAX          65535
#define  LOZ_STRLEN_MAX             16384

#define  LOZ_READONLY               0    // "r"-read only
#define  LOZ_READWRITE              1    // "r+"-read/write-update (create/update file, if file does not exist it will be created)
#define  LOZ_READWRITE_CLEAR        2    // "w+"-read/write-clear (clear existing file, if file does not exist it will be created)

#define  LOZ_OK                     ( 0)
#define  LOZ_ERROR                  (-1)
#define  LOZ_EOF                    (-2)
#define  LOZ_BAD_CRC                (-3)
#define  LOZ_UNSUPPORTED            (-4)
    
#define LOZ_FILLER                  '?'


//LOZ-file structure
typedef struct lozfile_t lozfile_t;
struct lozfile_t
{
        FILE     * fd;          // pointer to opened file
        int        version;
        int        rwmode;
        uint8_t    compression; //currently used compression format for new data to be written into file

        int        fid;         //id of opened file
        long int   filesize;
        uint8_t    fileheader_crc;

        long int   rd_fpos;     //current read  position in file (compressed data)
        long int   wr_fpos;     //current write position in file (compressed data)

        long int   rd_rawpos;   //current read  position in file (uncompressed data)
        long int   wr_rawpos;   //current write position in file (uncompressed data)
        
        int        buffsize;    //size of wrbuff, rdbuff
        int        lzbuffsize;  //size of lzbuff
        int        strbuffsize; //size of wrbuff, rdbuff

        uint8_t  * wrbuff;      //write buffer for uncompressed (raw) data 
        uint8_t  * rdbuff;      //read  buffer for uncompressed (raw) data
        uint8_t  * lzbuff;      //read/write buffer for compressed data
        uint8_t  * strbuff;     //repair buffer for string functions

        int        rdbuff_n;    //available bytes in rdbuff
        int        rdbuff_pos;  //current position in read buffer
        int        wrbuff_pos;  //current position in write buffer
        
        int        error;       //last error
};

typedef struct lozfile_section_t lozfile_section_t;
struct lozfile_section_t
{
        long int   fpos; //begining of section in file
        char       header_is_valid;
        
        uint8_t    beginmarker[2];
        uint32_t   rawpos;
        uint32_t   rawpos_end;
        uint32_t   rawsize;
        uint32_t   compsize;
        uint8_t    crc;
};
    



/******************************************************************************/
/* FUNCTION DEFINITIONS                                                       */
/******************************************************************************/
lozfile_t * loz_open        ( const char * filename, char * rwmode, int buffsize, int compression );
int         loz_write       ( lozfile_t * lozfile, char * data, int size );
int         loz_read        ( lozfile_t * lozfile, void * ptr, int size );
int         loz_printf      ( lozfile_t * lozfile, const char * format, ... );
int         loz_vprintf     ( lozfile_t * lozfile, const char * format, va_list arg );
void        loz_close       ( lozfile_t * lozfile );
void        loz_flush       ( lozfile_t * lozfile );
long int    loz_filesize    ( lozfile_t * lozfile );
/*              
void        loz_fseek       ( lozfile_t * lozfile, long int fpos );
long int    loz_ftell       ( lozfile_t * lozfile );
*/  

#endif /* LOZFILE_H */
