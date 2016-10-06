/******************************************************************************/
/* TEST PROGRAMM FOR LGZ-FILE LIBRARY                                         */
/* test.c                                                                     */
/* (c) Sergei Mashkin, 2015                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lgzfile.h"

#define MYLOGDEVICE 1 //MYLOGDEVICE_STDOUT
#include "mylog.h"

#define DATA_LEN   (4*65536)
#define BLOCKSIZE  (4096)

//#define COMPRESSION  LZ_COMPRESSION_NONE
//#define COMPRESSION  LZ_COMPRESSION_RLE
//#define COMPRESSION  LZ_COMPRESSION_LZ
//#define COMPRESSION  LZ_COMPRESSION_HUFFMAN
//#define COMPRESSION  LZ_COMPRESSION_FASTLZ1
#define COMPRESSION  LZ_COMPRESSION_FASTLZ2

//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
    int        i;
    int        err;
    uint8_t    data   [DATA_LEN];
    uint8_t    datalz [DATA_LEN];
    uint8_t    datatxt[DATA_LEN];
    lzfile_t * lzfile;
    FILE     * file;
    FILE     * file2;
    int        failed;

    MYLOG_INIT( 0
//        | MYLOG_ENABLED_ALL
        | MYLOG_ENABLED_ERROR
        | MYLOG_ENABLED_USER
    );

    printf("lzfile test start\n");

    //==========================================================================
    //Generate data[]
    /*
    for(i=0; i<DATA_LEN; i++) {
        data[i] = i % 20;
    }
    */

    //OR
    
    //Get data[] from defined file
    file = fopen("text.txt", "r");
    if(file==NULL) {
        MYLOG_ERROR("Could not open file in r mode");
        exit(EXIT_FAILURE);
    }
    err = fread( data, DATA_LEN, 1, file );
    if(err != 1) {
        MYLOG_ERROR("fread() failed: err=%d", err);
        exit(EXIT_FAILURE);
    }
    fclose(file);

#if 1
    //==========================================================================
    //Write half of data[] to .txt and .lz files (w+ mode)
    file = fopen("data.txt", "w+");
    if(file==NULL) {
        MYLOG_ERROR("Could not open file in w+ mode");
        exit(EXIT_FAILURE);
    }
    lzfile = lz_open( "data.lz", "w+", BLOCKSIZE, COMPRESSION );
    if(lzfile==NULL) {
        MYLOG_ERROR("Could not open lzfile in w+ mode");
        exit(EXIT_FAILURE);
    }

    err = fwrite( data, DATA_LEN/2, 1, file );
    if(err != 1) {
        MYLOG_ERROR("fwrite() failed");
        exit(EXIT_FAILURE);
    }
    err = lz_write( lzfile, (char*)data, DATA_LEN/2 );
    if(err != DATA_LEN/2) {
        MYLOG_ERROR("lz_write() failed");
        exit(EXIT_FAILURE);
    }

    lz_close(lzfile);
    fclose(file);
    
    //==========================================================================
    //Write another half of data[] to .txt and .lz files (r+ mode)
    file = fopen("data.txt", "r+");
    if(file==NULL) {
        MYLOG_ERROR("Could not open file in w+ mode");
        exit(EXIT_FAILURE);
    }
    lzfile = lz_open( "data.lz", "r+", BLOCKSIZE, COMPRESSION );
    if(lzfile==NULL) {
        MYLOG_ERROR("Could not open lzfile in w+ mode");
        exit(EXIT_FAILURE);
    }

    err = fseek( file, 0, SEEK_END );
    if(err < 0) {
        MYLOG_ERROR("fseek() failed");
        exit(EXIT_FAILURE);
    }
    err = fwrite( data+DATA_LEN/2, DATA_LEN/2, 1, file );
    if(err != 1) {
        MYLOG_ERROR("fwrite() failed");
        exit(EXIT_FAILURE);
    }
    
    err = lz_write( lzfile, (char*)data+DATA_LEN/2, DATA_LEN/2 );
    if(err != DATA_LEN/2) {
        MYLOG_ERROR("lz_write() failed");
        exit(EXIT_FAILURE);
    }

    lz_close(lzfile);
    fclose(file);

    /*
    for(i=0; i<DATA_LEN; i++) {
        err = lz_printf( lzfile, "%08X %6d this is test string %p: %d\n", i, i, data+i, data[i] );

        fprintf( file, "%08X %6d this is test %08X string %p: %d\n", i, i, 2*i, data+i, data[i] );
        
        if(err <= 0) {
            printf("Error: lz_write() failed\n");
            exit(EXIT_FAILURE);
        }
    }
    */
#endif
    //==========================================================================
    //Read from .txt to datatxt[], read from .lz to datalz[]
    file = fopen("data.txt", "r");
    if(file==NULL) {
        MYLOG_ERROR("Could not open file in r mode");
        exit(EXIT_FAILURE);
    }
    lzfile = lz_open( "data.lz", "r+", BLOCKSIZE, COMPRESSION );
    if(lzfile==NULL) {
        MYLOG_ERROR("Could not open lzfile in r mode");
        exit(EXIT_FAILURE);
    }
    file2 = fopen("data2.txt", "w");
    if(file2==NULL) {
        MYLOG_ERROR("Could not open file in w mode");
        exit(EXIT_FAILURE);
    }
    
    err = fread( datatxt, DATA_LEN, 1, file );
    if(err != 1) {
        MYLOG_ERROR("fread() failed: err=%d", err);
        exit(EXIT_FAILURE);
    }
    err = lz_read( lzfile, datalz, DATA_LEN );
    if(err != DATA_LEN) {
        MYLOG_ERROR("lz_read() failed");
        exit(EXIT_FAILURE);
    }
    err = fwrite( datalz, DATA_LEN, 1, file2 );
    if(err != 1) {
        MYLOG_ERROR("fwrite() failed: err=%d", err);
        exit(EXIT_FAILURE);
    }

    fclose(file2);
    lz_close(lzfile);
    fclose(file);

    //==========================================================================
    //Check data[]==datatxt[]==datalz[]
    printf("testing data:\n");
    printf("i     data[]   datatxt[]   datalz[]\n");
    failed = 0;
    for(i=0; i<DATA_LEN; i++) {
        if((data[i] != datatxt[i]) ||
           (data[i] != datalz[i] ))
        {
            failed++;
            printf("%6d   %02X     %02X      %02X", i,  0xFF&data[i], 0xFF&datatxt[i], 0xFF&datalz[i]);
            printf("  error");
            printf("\n");
        }
    }
    
    if(failed)
    {
        printf("test failed: %d errors\n", failed);
        exit(EXIT_FAILURE);
    }


    printf("lzfile test ok!\n");
    exit(EXIT_SUCCESS);
}
