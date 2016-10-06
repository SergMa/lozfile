/******************************************************************************/
/* lozfile.h                                                                  */
/* LOZ-FILE INPUT OUTPUT LIBRARY (IMPLEMENTATIONS)                            */
/*                                                                            */
/* Copyright (c) 2016 Sergey Mashkin                                          */
/* e-mail: mashkh@yandex.ru                                                   */
/******************************************************************************/

#include  "lozfile.h"
#include  "crc8.h"
#include  "fastlz.h"
#include  "compress_rle.h"
#include  "compress_rle2.h"
#include  "compress_lz.h"

#define MYLOGDEVICE 1 //MYLOGDEVICE_STDOUT
#include  "mylog.h"

#include  <stdlib.h>
#include  <errno.h>
#include  <string.h>
#include  <stdio.h>
#include  <sys/time.h>
#include  <time.h>
#include  <stdarg.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <unistd.h>
 

/******************************************************************************/
/* GLOBAL VARIABLES                                                           */
/******************************************************************************/

#define LOZ_FILEHEADER_SIZE      6
#define LOZ_SECTIONHEADER_SIZE   15

#define LOZ_CRC_SIZE             1

static uint8_t LOZ_FMT[] = { 'L','O','Z' };
#define LOZ_FMT_SIZE             sizeof(LOZ_FMT)

static uint8_t LOZ_BEGINMARKER[] = { 0xFA, 0xF5 };
#define LOZ_BEGINMARKER_SIZE     sizeof(LOZ_BEGINMARKER)

/******************************************************************************/
/* PRIVATE FUNCTIONS PROTOTYPES                                               */
/******************************************************************************/

int      file_exists                    ( const char * filepath );
char *   compression_to_str             ( uint8_t compression );
void     loz_section_copy               ( lozfile_section_t * dest, lozfile_section_t * src );
    
int      loz_compress_data              ( int compression, uint8_t * rawdata, int rawsize,
                                          uint8_t * compdata, int compsizemax, int * compsize );
    
int      loz_uncompress_data            ( int compression, uint8_t * compdata, int compsize,
                                          uint8_t * rawdata, int rawsizemax, int * rawsize );
    
long int loz_find_seq2                  ( lozfile_t * lozfile, uint8_t * seq2, long int startpos );
long int loz_find_seq2_reverse          ( lozfile_t * lozfile, uint8_t * seq2, long int startpos );
    
int      loz_read_fileheader            ( lozfile_t * lozfile );
int      loz_write_fileheader           ( lozfile_t * lozfile );
    
int      loz_read_section_header        ( lozfile_t * lozfile, lozfile_section_t * header, long int fpos );
int      loz_write_section_header       ( lozfile_t * lozfile, lozfile_section_t * header );
int      loz_write_section_header_crc   ( lozfile_t * lozfile, lozfile_section_t * header );
    
int      loz_write_compdata             ( lozfile_t * lozfile, long int fpos, uint8_t * compdata, int compsize );
int      loz_read_compdata              ( lozfile_t * lozfile, long int fpos, uint8_t * compdata, int compsize );
    
int      loz_section_first              ( lozfile_t * lozfile, lozfile_section_t * section );
int      loz_section_next               ( lozfile_t * lozfile, lozfile_section_t * curr, lozfile_section_t * next );
int      loz_section_last               ( lozfile_t * lozfile, lozfile_section_t * section );
int      loz_section_raw_fpos           ( lozfile_t * lozfile, lozfile_section_t * section, long int fpos );

/******************************************************************************/
/* PRIVATE FUNCTIONS                                                          */
/******************************************************************************/

//------------------------------------------------------------------------------
//Check if file exists
//returns:  0 = file does not exist
//          1 = file exists
//         -1 = error
int file_exists( const char * filepath )
{
        int         err;
        struct stat s;

        MYLOG_TRACE("@(filepath=%s)", filepath);
        
        if(filepath==NULL) {
                MYLOG_ERROR("invalid value: filepath=NULL");
                return -1;
        }
        err = stat( filepath, &s );
        if(err < 0) {
                if(errno == ENOENT) {
                        MYLOG_DEBUG("file \"%s\" does not exist", filepath);
                        return 0;
                }
                MYLOG_ERROR("Could not get file stat for file \"%s\"", filepath);
                return -1;
        }
        MYLOG_DEBUG("file \"%s\" exists", filepath);
        return 1;
}

//------------------------------------------------------------------------------
//Convert compression code to string
char * compression_to_str( uint8_t compression )
{
        static char str[32];
        switch(compression)
        {
        case LOZ_COMPRESSION_NONE:      return "none";
        case LOZ_COMPRESSION_RLE:       return "rle";
        case LOZ_COMPRESSION_RLE2:      return "rle2";
        case LOZ_COMPRESSION_LZ:        return "lz";
        case LOZ_COMPRESSION_FASTLZ1:   return "fastlz1";
        case LOZ_COMPRESSION_FASTLZ2:   return "fastlz2";
        default:
                snprintf(str,sizeof(str),"?(%d)",compression);
                return str;
        }
}

//------------------------------------------------------------------------------
//Copy section to another one
void loz_section_copy( lozfile_section_t * dest, lozfile_section_t * src )
{
        if(dest==NULL) {
                MYLOG_ERROR("Invalid argument: dest=NULL");
                return;
        }
        if(src==NULL) {
                MYLOG_ERROR("Invalid argument: src=NULL");
                return;
        }
        memcpy(dest,src,sizeof(lozfile_section_t));
        return;
}

//------------------------------------------------------------------------------
//Compress data with defined compression
//returns: LOZ_OK
//         LOZ_ERROR
int loz_compress_data ( int compression, uint8_t * rawdata, int rawsize,
                       uint8_t * compdata, int compsizemax, int * compsize )
{
        MYLOG_TRACE("@(compression=%s,rawdata=%p,rawsize=%d,compdata=%p,compsizemax=%d,compsize=%p)",
                    compression_to_str(compression), rawdata, rawsize, compdata, compsizemax, compsize);
        
        //Check input arguments
        if(rawdata==NULL) {
                MYLOG_ERROR("Invalid argument: rawdata=NULL");
                return LOZ_ERROR;
        }
        if(rawsize<=0) {
                MYLOG_ERROR("Invalid argument: rawsize=%d", rawsize);
                return LOZ_ERROR;
        }
        if(compdata==NULL) {
                MYLOG_ERROR("Invalid argument: compdata=NULL");
                return LOZ_ERROR;
        }
        if(compsizemax<=0) {
                MYLOG_ERROR("Invalid argument: compsizemax=%d", compsizemax);
                return LOZ_ERROR;
        }
        if(compsize==NULL) {
                MYLOG_ERROR("Invalid argument: compsize=NULL");
                return LOZ_ERROR;
        }
        if(compsizemax < 2 * rawsize) {
                MYLOG_ERROR("compdata buffer is too small: compsizemax=%d < rawsize=%d", compsizemax, rawsize);
                return LOZ_ERROR;
        }
        
        //compress rawdata[] into compdata[]
        switch(compression)
        {
        case LOZ_COMPRESSION_NONE:
                memcpy(compdata, rawdata, rawsize);
                *compsize = rawsize;
                return LOZ_OK;

        case LOZ_COMPRESSION_RLE:
                *compsize = RLE_Compress( rawdata, compdata, rawsize );
                return LOZ_OK;

        case LOZ_COMPRESSION_RLE2:
                *compsize = rle_compress( rawdata, rawsize, compdata, compsizemax );
                if(*compsize < 0) {
                        MYLOG_ERROR("rle_compress() failed");
                        return LOZ_ERROR;
                }
                return LOZ_OK;

        case LOZ_COMPRESSION_LZ:
                *compsize = LZ_Compress( rawdata, compdata, rawsize );
                return LOZ_OK;

        case LOZ_COMPRESSION_FASTLZ1:
                *compsize = fastlz_compress_level( 1, rawdata, rawsize, compdata );
                return LOZ_OK;

        case LOZ_COMPRESSION_FASTLZ2:
                *compsize = fastlz_compress_level( 2, rawdata, rawsize, compdata );
                return LOZ_OK;
        
        default:
                MYLOG_ERROR("Unsupported compression=%d", compression);
                return LOZ_ERROR;
        }
}

//------------------------------------------------------------------------------
//Uncompress data with defined compression
//returns: LOZ_OK
//         LOZ_ERROR
int loz_uncompress_data ( int compression, uint8_t * compdata, int compsize,
                         uint8_t * rawdata, int rawsizemax, int * rawsize )
{
        MYLOG_TRACE("@(compression=%s,compdata=%p,compsize=%d,rawdata=%p,rawsizemax=%d,rawsize=%p)",
                    compression_to_str(compression), compdata, compsize, rawdata, rawsizemax, rawsize);
        
        //Check input arguments
        if(compdata==NULL) {
                MYLOG_ERROR("Invalid argument: compdata=NULL");
                return LOZ_ERROR;
        }
        if(compsize<=0) {
                MYLOG_ERROR("Invalid argument: compsize=%d", compsize);
                return LOZ_ERROR;
        }
        if(rawdata==NULL) {
                MYLOG_ERROR("Invalid argument: rawdata=NULL");
                return LOZ_ERROR;
        }
        if(rawsizemax<=0) {
                MYLOG_ERROR("Invalid argument: rawsizemax=%d", rawsizemax);
                return LOZ_ERROR;
        }
        if(rawsize==NULL) {
                MYLOG_ERROR("Invalid argument: rawsize=NULL");
                return LOZ_ERROR;
        }
        
        //uncompress compdata[] into rawdata[]
        switch(compression)
        {
        case LOZ_COMPRESSION_NONE:
                memcpy(rawdata, compdata, compsize);
                *rawsize = compsize;
                return LOZ_OK;
                
        case LOZ_COMPRESSION_RLE:
                *rawsize = RLE_Uncompress( compdata, rawdata, compsize, rawsizemax );
                return LOZ_OK;

        case LOZ_COMPRESSION_RLE2:
                *rawsize = rle_decompress ( compdata, compsize, rawdata, rawsizemax );
                if(*rawsize < 0) {
                        MYLOG_ERROR("rle_decompress() failed");
                        return LOZ_ERROR;
                }
                return LOZ_OK;
                
        case LOZ_COMPRESSION_LZ:     
                *rawsize = LZ_Uncompress( compdata, rawdata, compsize, rawsizemax );
                return LOZ_OK;
                
        case LOZ_COMPRESSION_FASTLZ1:
        case LOZ_COMPRESSION_FASTLZ2:
                *rawsize = fastlz_decompress( compdata, compsize, rawdata, rawsizemax );
                return LOZ_OK;
        
        default:
                MYLOG_ERROR("Unsupported compression=%d", compression);
                return LOZ_ERROR;
        }
}

//------------------------------------------------------------------------------
//Find sequence of two bytes in file from startpos
//returns:  fpos     = in-file position of seq2 (if found)
//          LOZ_ERROR = error
//          LOZ_EOF   = seq2 not found, End Of File achieved
long int loz_find_seq2 ( lozfile_t * lozfile, uint8_t * seq2, long int startpos )
{
        int       err;
        long int  fpos;
        int       readed;
        uint8_t   buf[LOZ_BEGINMARKER_SIZE];

        MYLOG_TRACE("@(lozfile=%p,seq2=%p,startpos=%ld)", lozfile, seq2, startpos);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("Invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(seq2==NULL) {
                MYLOG_ERROR("Invalid argument seq2=NULL");
                return LOZ_ERROR;
        }
        if(startpos < 0) {
                MYLOG_ERROR("startpos=%ld", startpos);
                return LOZ_ERROR;
        }

        //Set startpos
        fpos = startpos;
        err = fseek( lozfile->fd, fpos, SEEK_SET );
        if(err) {
                MYLOG_ERROR("fseek(%ld) failed: err=%d: %s", startpos, errno, strerror(errno) );
                return LOZ_ERROR;
        }
        
        readed = 0;
        while(1) {
                //shift buffer
                buf[0] = buf[1];
                
                //read byte
                err = fread( &buf[1], sizeof(buf[1]), 1, lozfile->fd );
                if(err != 1) {
                        if( feof(lozfile->fd) ) {
                                MYLOG_DEBUG("EOF of lozfile achieved");
                                return LOZ_EOF;
                        }
                        else if( ferror(lozfile->fd) ) {
                                MYLOG_ERROR("Could not read from file: err: %d: %s", errno, strerror(errno));
                                return LOZ_ERROR;
                        }
                        else {
                                MYLOG_ERROR("Could not read %d bytes from file", sizeof(buf[1]));
                                return LOZ_ERROR;
                        }
                }
                readed++;
                
                //compare bytes stream with seq2
                if( (readed >= LOZ_BEGINMARKER_SIZE) &&
                    (buf[0]==seq2[0]) &&
                    (buf[1]==seq2[1]) )
                {
                        fpos = startpos + readed - LOZ_BEGINMARKER_SIZE;
                        MYLOG_DEBUG("seq2 has been found at fpos=%ld", fpos);
                        return fpos;
                }
        }
}

//------------------------------------------------------------------------------
//Find sequence of two bytes in file from startpos (reverse direction)
//returns:  fpos     = in-file position of seq2 (if found)
//          LOZ_ERROR = error
//          LOZ_EOF   = seq2 not found / End Of File achieved
long int loz_find_seq2_reverse ( lozfile_t * lozfile, uint8_t * seq2, long int startpos )
{
        int      err;
        long int fpos;
        uint8_t  buf[LOZ_BEGINMARKER_SIZE] = {0,0};
        char     b;
        int      n;
        
        MYLOG_TRACE("@(lozfile=%p,seq2[]=%02X %02X,startpos=%ld)",
                    lozfile, seq2[0]&0xFF, seq2[1]&0xFF, startpos);
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(seq2==NULL) {
                MYLOG_ERROR("invalid argument seq2=NULL");
                return LOZ_ERROR;
        }
        if(startpos < 0) {
                MYLOG_ERROR("startpos=%ld", startpos);
                return LOZ_ERROR;
        }

        //Scan file
        n = 0;
        for(fpos=startpos; fpos>=0; fpos--)
        {
                //read byte
                err = fseek( lozfile->fd, fpos, SEEK_SET );
                if(err) {
                        MYLOG_ERROR("fseek(%ld) failed: err=%d: %s", fpos, errno, strerror(errno) );
                        return LOZ_ERROR;
                }
                err = fread( &b, sizeof(b), 1, lozfile->fd );
                if(err != 1) {
                        if( feof(lozfile->fd) ) {
                                MYLOG_DEBUG("EOF of lozfile achieved");
                                return LOZ_EOF;
                        }
                        else if( ferror(lozfile->fd) ) {
                                MYLOG_ERROR("fread() failed: err: %d: %s", errno, strerror(errno));
                                return LOZ_ERROR;
                        }
                        else {
                                MYLOG_ERROR("could not read byte from file");
                                return LOZ_ERROR;
                        }
                }
                n++;
                //put byte into shift-buffer
                buf[1] = buf[0];
                buf[0] = b;
                //compare buffer with seq2
                if( (buf[0]==seq2[0]) &&
                    (buf[1]==seq2[1]) &&
                    (n >= LOZ_BEGINMARKER_SIZE) )
                {
                        MYLOG_DEBUG("seq2 has been found at fpos=%ld",fpos);
                        return fpos;
                }
        }

        MYLOG_ERROR("seq2 has not been found in file");
        return LOZ_EOF; 
}

//------------------------------------------------------------------------------
//Read file header and check its validity
//returns: LOZ_OK          = ok
//         LOZ_ERROR       = error
//         LOZ_EOF         = End Of File achieved
//         LOZ_UNSUPPORTED = invalid/unsupported file format
//         LOZ_BAD_CRC     = bad file-header-CRC value
int loz_read_fileheader( lozfile_t * lozfile )
{
        int     err;
        uint8_t buf[LOZ_FILEHEADER_SIZE];
        uint8_t crc_cc;

        MYLOG_TRACE("@(lozfile=%p)", lozfile);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }

        //set fpos to the begining of file
        err = fseek( lozfile->fd, 0L, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not read file-header: fseek(0L) failed");
                return LOZ_ERROR;
        }

        //read file-header
        err = fread( &buf, sizeof(buf), 1, lozfile->fd );
        if(err != 1) {
                if( feof(lozfile->fd) ) {
                        MYLOG_DEBUG("EOF of lozfile achieved");
                        return LOZ_EOF;
                }
                else if( ferror(lozfile->fd) ) {
                        MYLOG_ERROR("fread() failed: err: %d: %s", errno, strerror(errno));
                        return LOZ_ERROR;
                }
                else {
                        MYLOG_ERROR("could not read %d bytes from file", 11);
                        return LOZ_ERROR;
                }
        }
        
        if( buf[0] != LOZ_FMT[0] ||
            buf[1] != LOZ_FMT[1] ||
            buf[2] != LOZ_FMT[2] )
        {
                MYLOG_ERROR("file format is not LZF");
                return LOZ_ERROR;
        }
        lozfile->version        = buf[3];
        lozfile->compression    = buf[4];
        lozfile->fileheader_crc = buf[5];
        
        if( lozfile->version != LOZ_VERSION_0 ) {
                MYLOG_ERROR("LZF version (%d) is not supported", lozfile->version );
                return LOZ_UNSUPPORTED;
        }
        
        crc_cc = crc8_array( buf + LOZ_FMT_SIZE,
                             LOZ_FILEHEADER_SIZE - LOZ_FMT_SIZE - LOZ_CRC_SIZE,
                             CRC8_INIT );
        if(crc_cc==0x00)
                crc_cc = 0x01; //CRC could not be 0x00, replace this with 0x01
        
        if(crc_cc != lozfile->fileheader_crc) {
                MYLOG_ERROR("Invlaid LZF-file header CRC: %02X (calculated), %02X (readed)",
                            crc_cc&0xFF, lozfile->fileheader_crc&0xFF);
                return LOZ_BAD_CRC;
        }
        
        MYLOG_DEBUG("LZF fileheader is valid: version=%d,compression=%s",
                    lozfile->version, compression_to_str(lozfile->compression) );
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Write file header to opened file
//returns: LOZ_OK    = ok
//         LOZ_ERROR = error
int loz_write_fileheader( lozfile_t * lozfile )
{
        int  err;
        uint8_t buf[LOZ_FILEHEADER_SIZE];

        MYLOG_TRACE("@(lozfile=%p)", lozfile);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }

        buf[0] = LOZ_FMT[0];
        buf[1] = LOZ_FMT[1];
        buf[2] = LOZ_FMT[2];
        buf[3] = lozfile->version;
        buf[4] = lozfile->compression;
        
        lozfile->fileheader_crc = crc8_array( buf + LOZ_FMT_SIZE,
                                             LOZ_FILEHEADER_SIZE - LOZ_FMT_SIZE - LOZ_CRC_SIZE,
                                             CRC8_INIT );
        if(lozfile->fileheader_crc==0x00)
                lozfile->fileheader_crc = 0x01; //CRC could not be 0x00, replace this with 0x01
        
        buf[5] = lozfile->fileheader_crc;

        //set fpos to the begining of file
        err = fseek( lozfile->fd, 0L, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not write file-header: fseek(0L) failed");
                return LOZ_ERROR;
        }

        //write file-header
        err = fwrite( &buf, sizeof(buf), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write file-header");
                return LOZ_ERROR;
        }

        MYLOG_DEBUG("LZF fileheader has been written: version=%d, compression=%s",
                    lozfile->version, compression_to_str(lozfile->compression));
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Read Section-header from defined fpos
//returns: LOZ_OK      = ok, section-header is valid
//         LOZ_ERROR   = error
//         LOZ_EOF     = End Of File achieved
//         LOZ_BAD_CRC = section is corrupted (or has invalid format)
int loz_read_section_header( lozfile_t * lozfile, lozfile_section_t * header, long int fpos )
{
        int      err;
        uint8_t  buf[LOZ_SECTIONHEADER_SIZE]; //size of section-header
        uint8_t  crc;
        
        MYLOG_TRACE("@(lozfile=%p,header=%p,fpos=%ld)", lozfile, header, fpos);
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(header==NULL) {
                MYLOG_ERROR("invalid argument header=NULL");
                return LOZ_ERROR;
        }
        if(fpos < 0) {
                MYLOG_ERROR("invalid argument fpos=%ld", fpos);
                return LOZ_ERROR;
        }

        //set fpos to the defined value
        err = fseek( lozfile->fd, fpos, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not write file-header: fseek(%ld) failed",fpos);
                return LOZ_ERROR;
        }
        
        //Read data from file to buf
        err = fread( buf, LOZ_SECTIONHEADER_SIZE, 1, lozfile->fd );
        if(err != 1) {
                if( feof(lozfile->fd) ) {
                        MYLOG_DEBUG("EOF of lozfile achieved");
                        return LOZ_EOF;
                }
                else if( ferror(lozfile->fd) ) {
                        MYLOG_ERROR("fread() failed: err: %d: %s", errno, strerror(errno));
                        return LOZ_ERROR;
                }
                else {
                        MYLOG_ERROR("could not read %d bytes from file", 11);
                        return LOZ_ERROR;
                }
        }
        
        //Fill section header structure
        header->header_is_valid = 0;

        header->fpos = fpos;
        
        header->beginmarker[0] = buf[0];
        header->beginmarker[1] = buf[1];
        
        header->rawpos   = ((uint32_t)buf[ 2]<< 0) +
                           ((uint32_t)buf[ 3]<< 8) +
                           ((uint32_t)buf[ 4]<<16) +
                           ((uint32_t)buf[ 5]<<24) ;
        header->rawsize  = ((uint32_t)buf[ 6]<< 0) +
                           ((uint32_t)buf[ 7]<< 8) +
                           ((uint32_t)buf[ 8]<<16) +
                           ((uint32_t)buf[ 9]<<24) ;
        header->compsize = ((uint32_t)buf[10]<< 0) +
                           ((uint32_t)buf[11]<< 8) +
                           ((uint32_t)buf[12]<<16) +
                           ((uint32_t)buf[13]<<24) ;
        header->crc = buf[14];

        //Calculate rawpos_end
        header->rawpos_end = header->rawpos + header->rawsize - 1;

        //Check header validity
        if( (header->beginmarker[0] != LOZ_BEGINMARKER[0]) ||
            (header->beginmarker[1] != LOZ_BEGINMARKER[1])  )
        {
                MYLOG_ERROR("invalid section begin marker: %02X,%02X", header->beginmarker[0], header->beginmarker[1]);
                return LOZ_ERROR;
        }

        //Calculate CRC
        crc = crc8_array( buf + LOZ_BEGINMARKER_SIZE,
                          LOZ_SECTIONHEADER_SIZE - LOZ_BEGINMARKER_SIZE - LOZ_CRC_SIZE,
                          CRC8_INIT );
        if(crc==0x00)
                crc = 0x01; //CRC could not be 0x00, replace this with 0x01
                
        if(crc != header->crc) {
                MYLOG_ERROR("invalid section crc: %02X(readed), %02X(calculated)", header->crc, crc);
                return LOZ_BAD_CRC;
        }
        
        header->header_is_valid = 1;
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Write Section-header from header->fpos
//returns: LOZ_OK      = ok, section-header is valid
//         LOZ_ERROR   = error
int loz_write_section_header( lozfile_t * lozfile, lozfile_section_t * header )
{
        int     err;
        uint8_t crc;

        MYLOG_TRACE("@(lozfile=%p,header=%p)", lozfile, header);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(header==NULL) {
                MYLOG_ERROR("invalid argument header=NULL");
                return LOZ_ERROR;
        }
        if(header->fpos < LOZ_FILEHEADER_SIZE) {
                MYLOG_ERROR("invalid argument header->fpos=%ld", header->fpos);
                return LOZ_ERROR;
        }

        //set fpos to the defined value
        err = fseek( lozfile->fd, header->fpos, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not write section-header: fseek(%ld) failed",header->fpos);
                return LOZ_ERROR;
        }

        //Write section header into file
        //beginmarker
        err = fwrite( header->beginmarker, sizeof(header->beginmarker), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section beginmarker: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        //rawpos
        err = fwrite( &header->rawpos, sizeof(header->rawpos), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section rawpos: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        //rawsize
        err = fwrite( &header->rawsize, sizeof(header->rawsize), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section rawsize: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        //compsize
        err = fwrite( &header->compsize, sizeof(header->compsize), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section compsize: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        //crc = 0  - mark section as invalid - use loz_write_section_header_crc() to write actual CRC
        crc = 0;
        err = fwrite( &crc, sizeof(crc), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section crc=0: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }

        //calculate true crc
        header->crc = crc8_array( (uint8_t*)&header->rawpos,   sizeof(header->rawpos),   CRC8_INIT );
        header->crc = crc8_array( (uint8_t*)&header->rawsize,  sizeof(header->rawsize),  header->crc );
        header->crc = crc8_array( (uint8_t*)&header->compsize, sizeof(header->compsize), header->crc );
        if(header->crc==0x00)
                header->crc = 0x01;
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Write Section-header CRC of header
//returns: LOZ_OK    = ok, section-header is valid
//         LOZ_ERROR = error
int loz_write_section_header_crc( lozfile_t * lozfile, lozfile_section_t * header )
{
        int     err;

        MYLOG_TRACE("@(lozfile=%p,header=%p)", lozfile, header);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(header==NULL) {
                MYLOG_ERROR("invalid argument header=NULL");
                return LOZ_ERROR;
        }

        //Set fpos to the begining of section-header.crc
        err = fseek( lozfile->fd, header->fpos + LOZ_SECTIONHEADER_SIZE - LOZ_CRC_SIZE, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not set startpos: fseek(%ld) failed: err=%d: %s", header->fpos, errno, strerror(errno) );
                return LOZ_ERROR;
        }

        //Write section header crc into file (actual value)
        //crc
        err = fwrite( &header->crc, sizeof(header->crc), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write section crc: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }

        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Write Section-data from defined fpos
//inputs:  lozfile
//         fpos
//         compdata = pointer to compressed data
//         compsize = size of compressed data
//returns: LOZ_OK    = ok, section-header is valid
//         LOZ_ERROR = error
int loz_write_compdata( lozfile_t * lozfile, long int fpos, uint8_t * compdata, int compsize )
{
        int      err;
        uint8_t  crc;

        MYLOG_TRACE("@(lozfile=%p,fpos=%ld,compdata=%p,compsize=%d)",
                    lozfile, fpos, compdata, compsize );

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(fpos < LOZ_FILEHEADER_SIZE + LOZ_SECTIONHEADER_SIZE) {
                MYLOG_ERROR("invalid argument fpos=%ld < %d", fpos, LOZ_FILEHEADER_SIZE + LOZ_SECTIONHEADER_SIZE);
                return LOZ_ERROR;
        }
        if(compdata==NULL) {
                MYLOG_ERROR("invalid argument compdata=NULL");
                return LOZ_ERROR;
        }
        if(compsize <= 0) {
                MYLOG_ERROR("invalid argument compsize=%d", compsize);
                return LOZ_ERROR;
        }

        //Set fpos to the begining of section-data
        err = fseek( lozfile->fd, fpos, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not set startpos: fseek(%ld) failed: err=%d: %s", fpos, errno, strerror(errno) );
                return LOZ_ERROR;
        }

        //Write compressed data to file
        err = fwrite( compdata, compsize, 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write %d bytes of compressed data: err=%d: %s", compsize, errno, strerror(errno) );
                return LOZ_ERROR;
        }

        //Calculate CRC for compressed data
        crc = crc8_array( compdata, compsize, CRC8_INIT );
        if(crc==0x00)
                crc = 0x01;
        
        //Write compressed data CRC
        err = fwrite( &crc, sizeof(crc), 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not write compressed data crc: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Read Section-data (compressed data) from current fpos
//inputs:  lozfile   = pointer to structure of opened LZ-file
//         fpos     = start in-file position of data to be readed from file
//         compdata = pointer to compressed data buffer
//         compsize = size of data to be readed
//returns: LOZ_OK      = ok, section-header is valid
//         LOZ_ERROR   = error
//         LOZ_EOF     = End Of File achieved
//         LOZ_BAD_CRC = data is corrupted
int loz_read_compdata( lozfile_t * lozfile, long int fpos, uint8_t * compdata, int compsize )
{
        int      err;
        uint8_t  crc_rd;
        uint8_t  crc_cc;

        MYLOG_TRACE("@(lozfile=%p,fpos=%ld,compdata=%p,compsize=%d)", lozfile, fpos, compdata, compsize );

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(fpos < LOZ_FILEHEADER_SIZE) {
                MYLOG_ERROR("invalid argument fpos=%ld (intersection with fileheader)", fpos);
                return LOZ_ERROR;
        }
        if(compdata==NULL) {
                MYLOG_ERROR("invalid argument compdata=NULL");
                return LOZ_ERROR;
        }
        if(compsize <= 0) {
                MYLOG_ERROR("invalid argument compsize=%d", compsize);
                return LOZ_ERROR;
        }

        //Set fpos to the begining of section-data
        err = fseek( lozfile->fd, fpos, SEEK_SET );
        if(err) {
                MYLOG_ERROR("could not set startpos: fseek(%ld) failed: err=%d: %s", fpos, errno, strerror(errno) );
                return LOZ_ERROR;
        }

        //Read compressed data from file
        err = fread( compdata, compsize, 1, lozfile->fd );
        if(err != 1) {
                MYLOG_ERROR("could not read %d bytes of compressed data: err=%d: %s", compsize, errno, strerror(errno) );
                if( feof(lozfile->fd) ) {
                        MYLOG_DEBUG("EOF of lozfile achieved");
                        return LOZ_EOF;
                }
                else if( ferror(lozfile->fd) ) {
                        MYLOG_ERROR("fread() failed: err: %d: %s", errno, strerror(errno));
                        return LOZ_ERROR;
                }
                else {
                        MYLOG_ERROR("could not read %d bytes from file", compsize);
                        return LOZ_ERROR;
                }
        }
        
        //Read compressed data CRC from file
        err = fread( &crc_rd, sizeof(crc_rd), 1, lozfile->fd );
        if(err != 1) {
                if( feof(lozfile->fd) ) {
                        MYLOG_DEBUG("EOF of lozfile achieved");
                        return LOZ_EOF;
                }
                else if( ferror(lozfile->fd) ) {
                        MYLOG_ERROR("fread() failed: err: %d: %s", errno, strerror(errno));
                        return LOZ_ERROR;
                }
                else {
                        MYLOG_ERROR("could not read %d bytes from file", sizeof(crc_rd));
                        return LOZ_ERROR;
                }
        }
        //Check CRC
        if(crc_rd==0x00) {
                MYLOG_ERROR("compressed data status is invalid (crc=0x00)");
                return LOZ_BAD_CRC;
        }

        //Calculate CRC for compressed data
        crc_cc = crc8_array( compdata, compsize, CRC8_INIT );
        if(crc_cc==0x00)
                crc_cc = 0x01;

        if(crc_cc != crc_rd) {
                MYLOG_ERROR("section data is corrupted");
                return LOZ_BAD_CRC;
        }
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Read header of the 1st section of file
//returns: LOZ_OK      = ok
//         LOZ_ERROR   = error
//         LOZ_EOF     = End Of File achieved
//         LOZ_BAD_CRC = Corrupted section header (bad CRC)
int loz_section_first ( lozfile_t * lozfile, lozfile_section_t * header )
{
        int err;
        
        MYLOG_TRACE("@(lozfile=%p,header=%p)", lozfile, header );
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(header==NULL) {
                MYLOG_ERROR("invalid argument header=NULL");
                return LOZ_ERROR;
        }
        
        //Get fpos of the 1st section, move rdpos to the begining of compressed data of section
        err = loz_read_section_header( lozfile, header, LOZ_FILEHEADER_SIZE ); //skip file-header
        if(err != LOZ_OK) {
                MYLOG_ERROR("could not get the valid 1st section: loz_read_section_header() failed with error or bad-crc");
                return err;
        }
        return LOZ_OK;
}

//------------------------------------------------------------------------------
//Read header of next section
//inputs:  lozfile = pointer to opened lz-file
//         curr   = pointer to current section-header structure (given)
//         next   = pointer to current section-header structure (will be filled)
//returns: LOZ_OK    = ok
//         LOZ_ERROR = error
//         LOZ_EOF   = End Of File achieved / no more section found
int loz_section_next ( lozfile_t * lozfile, lozfile_section_t * curr, lozfile_section_t * next )
{
        int      err;
        long int fpos;
        
        MYLOG_TRACE("@(lozfile=%p,curr=%p,next=%p)", lozfile, curr, next );
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(curr==NULL) {
                MYLOG_ERROR("invalid argument curr=NULL");
                return LOZ_ERROR;
        }
        if(next==NULL) {
                MYLOG_ERROR("invalid argument next=NULL");
                return LOZ_ERROR;
        }
        
        if(curr->header_is_valid) {
                //curr section header is valid - use its info and
                //get fpos of next section by curr->fpos and curr->compsize
                fpos = curr->fpos + LOZ_SECTIONHEADER_SIZE + curr->compsize + LOZ_CRC_SIZE;
                
                err = loz_read_section_header( lozfile, next, fpos );
                if(err == LOZ_OK) {
                        MYLOG_DEBUG("next section (valid) has been found at fpos=%ld",next->fpos);
                        return LOZ_OK;
                }
                return err;
        }
        else {
                //try to find next section by section-begin-marker
                fpos = curr->fpos;
                while(1) {
                        //search for section-begin-marker bytes
                        fpos = loz_find_seq2( lozfile, LOZ_BEGINMARKER, fpos+1 ); //skip begin-marker of curr section
                        if(fpos < 0) {
                                MYLOG_ERROR("could not find next section by begin-marker: loz_find_seq2() failed");
                                return fpos;
                        }
                        //try to read section header
                        err = loz_read_section_header( lozfile, next, fpos );
                        if(err == LOZ_OK) {
                                MYLOG_DEBUG("next section has been found at fpos=%ld",next->fpos);
                                return LOZ_OK; //next section has been found
                        }
                        else if(err == LOZ_BAD_CRC) {
                                //this is not section begin or corrupted section: skip begin-marker of curr section
                                continue;
                        }
                        else {
                                MYLOG_ERROR("could not read section: loz_read_section_header() failed");
                                return err;
                        }
                }
        }
}

//------------------------------------------------------------------------------
//Read header of previous valid section in file
//inputs:  lozfile = pointer to opened lz-file
//         curr = pointer to current section-header structure
//         prev = pointer to previous section-header structure
//returns: LOZ_OK    = ok
//         LOZ_ERROR = error
//         LOZ_EOF   = End Of File achieved / no more section found
int loz_section_prev ( lozfile_t * lozfile, lozfile_section_t * curr, lozfile_section_t * prev )
{
        int      err;
        long int fpos;
        
        MYLOG_TRACE("@(lozfile=%p,curr=%p,prev=%p)",lozfile,curr,prev);
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(curr==NULL) {
                MYLOG_ERROR("invalid argument curr=NULL");
                return LOZ_ERROR;
        }
        if(prev==NULL) {
                MYLOG_ERROR("invalid argument prev=NULL");
                return LOZ_ERROR;
        }
        
        //Set fpos to curr.fpos
        fpos = curr->fpos;
        fpos--;
        
        while(1) {
                //search potential section-begin-marker
                fpos = loz_find_seq2_reverse( lozfile, LOZ_BEGINMARKER, fpos );
                if(fpos < 0) {
                        MYLOG_ERROR("could not find section-begin marker back from current section");
                        return fpos;
                }

                //try to read section
                err = loz_read_section_header( lozfile, prev, fpos );
                if(err == LOZ_OK) {
                        MYLOG_DEBUG("previous section has been found at fpos=%ld",prev->fpos);
                        return LOZ_OK;
                }

                //search again
                fpos --;
        }
}

//------------------------------------------------------------------------------
//Read header of last valid section in file
//inputs:  lozfile = pointer to opened lz-file
//         header = pointer to current section-header structure (given)
//returns: LOZ_OK    = ok
//         LOZ_ERROR = error
//         LOZ_EOF   = End Of File achieved / no more section found
int loz_section_last ( lozfile_t * lozfile, lozfile_section_t * header )
{
        int      err;
        long int fpos;
        
        MYLOG_TRACE("@(lozfile=%p,header=%p)",lozfile,header);
        
        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(header==NULL) {
                MYLOG_ERROR("invalid argument header=NULL");
                return LOZ_ERROR;
        }
        
        //Get last section at fpos = filesize
        err = fseek( lozfile->fd, 0L, SEEK_END );
        if(err) {
                MYLOG_ERROR("could not set fpos to the end of file: fseek() failed: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        fpos = ftell( lozfile->fd );
        if(fpos == -1) {
                MYLOG_ERROR("could not get fpos at the end of file: ftell() failed: err=%d: %s", errno, strerror(errno) );
                return LOZ_ERROR;
        }
        fpos--;
        
        while(1) {
                //search potential section-begin-marker
                fpos = loz_find_seq2_reverse( lozfile, LOZ_BEGINMARKER, fpos );
                if(fpos < 0) {
                        MYLOG_ERROR("could not find section-begin marker back from end of file");
                        return fpos;
                }

                //try to read section
                err = loz_read_section_header( lozfile, header, fpos );
                if(err == LOZ_OK) {
                        MYLOG_DEBUG("last section has been found at fpos=%ld",header->fpos);
                        return LOZ_OK;
                }

                //search again
                fpos --;
        }
}

//------------------------------------------------------------------------------
//Write available data from lozfile->wrbuff[] to file
//inputs:   lozfile = pointer to opened lozfile
//returns:  written = number of bytes successfully written to file
//          LOZ_ERROR = error
int loz_flush_wrbuff_to_file( lozfile_t * lozfile )
{
        int              compsize;
        int              err;
        lozfile_section_t section;

        MYLOG_TRACE("@(lozfile=%p)", lozfile);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(lozfile->wrbuff==NULL) {
                MYLOG_ERROR("invalid argument wrbuff=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->lzbuff==NULL) {
                MYLOG_ERROR("invalid argument lzbuff=NULL");
                return LOZ_ERROR;
        }

        if(lozfile->wrbuff_pos == 0) {
                MYLOG_DEBUG("There is no data in lozfile->wrbuff[] - nothing written to file");
                return 0;
        }
        
        //compress wrbuff[] into lzbuff[]
        err = loz_compress_data ( lozfile->compression,
                                 lozfile->wrbuff,
                                 lozfile->wrbuff_pos,
                                 lozfile->lzbuff,
                                 lozfile->lzbuffsize,
                                 &compsize );
        if(err != LOZ_OK) {
                MYLOG_ERROR("loz_compress_data() failed with error=%d", err);
                return LOZ_ERROR;
        }
        
        section.beginmarker[0] = LOZ_BEGINMARKER[0];
        section.beginmarker[1] = LOZ_BEGINMARKER[1];
        section.fpos           = lozfile->wr_fpos;
        section.rawpos         = lozfile->wr_rawpos - lozfile->wrbuff_pos;
        section.rawpos_end     = lozfile->wr_rawpos;
        section.rawsize        = lozfile->wrbuff_pos;
        section.compsize       = compsize;

        err = loz_write_section_header( lozfile, &section );
        if(err) {
                MYLOG_ERROR("loz_write_section_header() failed");
                return LOZ_ERROR;
        }

        err = loz_write_compdata( lozfile,
                                 section.fpos + LOZ_SECTIONHEADER_SIZE,
                                 lozfile->lzbuff,
                                 compsize );
        if(err) {
                MYLOG_ERROR("loz_write_compdata() failed");
                return LOZ_ERROR;
        }

        err = loz_write_section_header_crc( lozfile, &section );
        if(err) {
                MYLOG_ERROR("loz_write_section_header_crc() failed");
                return LOZ_ERROR;
        }
        
        lozfile->wr_fpos += LOZ_SECTIONHEADER_SIZE + compsize + LOZ_CRC_SIZE;

        lozfile->wrbuff_pos = 0;
    
        return section.rawsize;
}

/******************************************************************************/
/* FUNCTIONS                                                                  */
/******************************************************************************/

//------------------------------------------------------------------------------
//Open lozfile. If file does not exist, it will be created.
//
//inputs:   filename    = name of logfile
//          rwmode      = read/write mode:
//                        "r"-read only
//                        "r+"-read/write-update (create/update file, if file does not exist it will be created)
//                        "w+"-read/write-clear (clear existing file, if file does not exist it will be created)
//          buffsize    = size of read-write buffers (LOZ_BLOCKSIZE_MIN .. LOZ_BLOCKSIZE_MAX), -1=use default value
//          compression = type of compression for new data to be written
//
//returns:  lozfile   = pointer to opened lz-file
//          NULL     = error
lozfile_t * loz_open ( const char * filename, char * rwmode, int buffsize, int compression )
{
        lozfile_t         * lozfile = NULL;
        int                err;
        int                exists;
        lozfile_section_t   section;

        MYLOG_TRACE("@(filename=%s,rwmode=%s,buffsize=%d,compression=%s)",
                    filename, rwmode, buffsize, compression_to_str(compression) );

        //Check input arguments
        if(filename==NULL) {
                MYLOG_ERROR("invalid argument: filename=NULL");
                return NULL;
        }
        if(filename[0]=='\0') {
                MYLOG_ERROR("invalid argument: filename=\"\"");
                return NULL;
        }
        if(rwmode==NULL) {
                MYLOG_ERROR("invalid argument: rwmode=NULL");
                return NULL;
        }
        if( (buffsize < LOZ_BLOCKSIZE_MIN) ||
            (buffsize > LOZ_BLOCKSIZE_MAX)  ) {
                MYLOG_ERROR("invalid argument: buffsize=%d, must be %d..%d", buffsize, LOZ_BLOCKSIZE_MIN, LOZ_BLOCKSIZE_MAX );
                return NULL;
        }

        switch(compression)
        {
        case LOZ_COMPRESSION_NONE:   
        case LOZ_COMPRESSION_RLE:    
        case LOZ_COMPRESSION_RLE2:    
        case LOZ_COMPRESSION_LZ:     
        case LOZ_COMPRESSION_FASTLZ1:
        case LOZ_COMPRESSION_FASTLZ2:
                break;
        default:
                MYLOG_ERROR("unsupported compression=%d",compression);
                return NULL;
        }

        //allocate memory
        lozfile = malloc(sizeof(lozfile_t));
        if(lozfile==NULL)
                return NULL;

        lozfile->version        = LOZ_VERSION_0;
        lozfile->compression    = compression;
        lozfile->filesize       = 0L;
        lozfile->rwmode         = LOZ_READWRITE;
        lozfile->fd             = NULL;
        lozfile->fid            = -1;
        lozfile->rd_fpos        = 0;
        lozfile->wr_fpos        = 0;
        lozfile->rd_rawpos      = 0;
        lozfile->wr_rawpos      = 0;
                           
        lozfile->buffsize       = 0;
        lozfile->lzbuffsize     = 0;
        lozfile->strbuffsize    = 0;
        lozfile->wrbuff         = NULL;
        lozfile->rdbuff         = NULL;
        lozfile->lzbuff         = NULL;
        lozfile->strbuff        = NULL;
        lozfile->wrbuff_pos     = 0;
        lozfile->rdbuff_pos     = 0;
        lozfile->rdbuff_n       = 0;

        //check if file already exists
        exists = file_exists(filename);
        if(exists < 0) {
                MYLOG_ERROR("file_exists() failed");
                goto exit_fail;
        }

        //open file in defined rwmode
        if(0==strcmp(rwmode,"r")) {
                lozfile->rwmode = LOZ_READONLY;
                if(!exists) {
                        MYLOG_ERROR("file \"%s\" does not exist", filename);
                        return NULL;
                }
                lozfile->fd = fopen( filename, "rb" ); //read only
        }
        else if(0==strcmp(rwmode,"r+")) {
                lozfile->rwmode = LOZ_READWRITE;
                lozfile->fd = fopen( filename, "r+b" ); //read/write/update
        }
        else if(0==strcmp(rwmode,"w+")) {
                lozfile->rwmode = LOZ_READWRITE_CLEAR;
                lozfile->fd = fopen( filename, "w+b" ); //read/write/clear
        }
        else {
                MYLOG_ERROR("unsupported rwmode=\"%s\"", rwmode);
                goto exit_fail;
        }
        
        if(lozfile->fd==NULL)
                goto exit_fail;

        lozfile->fid = fileno( lozfile->fd );
        if(lozfile->fid == -1)
                goto exit_fail;

        //allocate read/write buffers
        lozfile->buffsize = buffsize;
        lozfile->lzbuffsize = 2 * buffsize;
        lozfile->strbuffsize = LOZ_STRLEN_MAX;

        lozfile->rdbuff = malloc( lozfile->buffsize );
        if(lozfile->rdbuff==NULL)
                goto exit_fail;
        
        lozfile->wrbuff = malloc( lozfile->buffsize );
        if(lozfile->wrbuff==NULL)
                goto exit_fail;

        lozfile->lzbuff = malloc( lozfile->lzbuffsize );
        if(lozfile->lzbuff==NULL)
                goto exit_fail;

        lozfile->strbuff = malloc( lozfile->strbuffsize );
        if(lozfile->strbuff==NULL)
                goto exit_fail;

        //Check/create opened file
        
        //read & check LZ-file header
        if( lozfile->rwmode != LOZ_READWRITE_CLEAR )
        {
                err = loz_read_fileheader(lozfile);
                switch(err)
                {
                case LOZ_ERROR:
                        MYLOG_ERROR("Could not read file-header: loz_read_fileheader() failed with LOZ_ERROR");
                        goto exit_fail;
                
                case LOZ_UNSUPPORTED:
                        MYLOG_ERROR("Unsupported LZF version: loz_read_fileheader() failed with LOZ_UNSUPPORTED");
                        goto exit_fail;
                
                case LOZ_EOF:
                        MYLOG_ERROR("LZF file \"%s\" is empty", filename);
                        goto exit_fail;
                
                case LOZ_OK:
                        break;
                
                default:
                        MYLOG_ERROR("loz_read_fileheader() return unexpected error: %d", err);
                        goto exit_fail;
                }
        }
        
        switch(lozfile->rwmode)
        {
        case LOZ_READONLY:
                //move rd_fpos into the begining of data (to the 1st file-section)
                lozfile->rd_fpos   = LOZ_FILEHEADER_SIZE;
                lozfile->wr_fpos   = 0L; //will not be used in read-only mode
                
                lozfile->rd_rawpos = 0L;
                lozfile->wr_rawpos = 0L; //will not be used in read-only mode
                break;
        
        case LOZ_READWRITE:
                //move rdpos into the begining of data (to the 1st file-section)
                lozfile->rd_fpos   = LOZ_FILEHEADER_SIZE;
                lozfile->rd_rawpos = 0L;
                
                //move wrpos after last valid file-section
                err = loz_section_last( lozfile, &section );
                switch(err)
                {
                case LOZ_ERROR:
                        MYLOG_ERROR("loz_section_last() failed with LOZ_ERROR");
                        goto exit_fail;
                
                case LOZ_EOF:
                        MYLOG_DEBUG("lozfile is empty: loz_section_last() failed with LOZ_EOF");
                        lozfile->wr_fpos   = LOZ_FILEHEADER_SIZE;
                        lozfile->wr_rawpos = 0L;
                        break;
                
                case LOZ_BAD_CRC:
                        MYLOG_DEBUG("lozfile is corrupted: loz_section_last() failed with LOZ_BAD_CRC");
                        goto exit_fail;
                
                case LOZ_OK:
                        lozfile->wr_fpos   = section.fpos + LOZ_SECTIONHEADER_SIZE + section.compsize + LOZ_CRC_SIZE;
                        lozfile->wr_rawpos = section.rawpos + section.rawsize;
                        break;
                
                default:
                        MYLOG_ERROR("loz_section_last() failed with unexpected error=%d", err);
                        goto exit_fail;
                }
                break;
        
        case LOZ_READWRITE_CLEAR:
                //write LZ-file header into the file
                err = loz_write_fileheader(lozfile);
                if(err) {
                        MYLOG_ERROR("could not write LZ-file header");
                        goto exit_fail;
                }
                //move rd_fpos,wr_fpos into the begining of data (to the 1st file-section position)
                lozfile->rd_fpos = LOZ_FILEHEADER_SIZE; //skip file-header
                lozfile->wr_fpos = LOZ_FILEHEADER_SIZE; //skip file-header
                lozfile->rd_rawpos = 0L;
                lozfile->wr_rawpos = 0L;
                break;
        
        default:
                MYLOG_ERROR("unexpected value of rwmode: %d", lozfile->rwmode);
                goto exit_fail;
        }

        return lozfile;

exit_fail:
        if(lozfile) {
                loz_close( lozfile );
                free(lozfile);
        }
        return NULL;
}

//------------------------------------------------------------------------------
//Stream write data to lozfile
//returns: written  = number of bytes written to file (in normal case written==size)
//         LOZ_ERROR = error
int loz_write( lozfile_t * lozfile, char * data, int size )
{
        int       i;
        uint8_t * p;
        int       err;

        MYLOG_TRACE("@(lozfile=%p,data=%p,size=%d)", lozfile, data, size);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return LOZ_ERROR;
        }
        if(data==NULL) {
                MYLOG_ERROR("invalid argument data=NULL");
                return LOZ_ERROR;
        }
        if(size <= 0) {
                MYLOG_ERROR("invalid argument size=%d", size);
                return LOZ_ERROR;
        }
        
        p = (uint8_t*)data;
        for(i=0; i<size; i++)
        {
                //add data to wrbuff[]
                lozfile->wrbuff[ lozfile->wrbuff_pos++ ] = *p++;
                lozfile->wr_rawpos++;
                
                //is full block formed?
                if(lozfile->wrbuff_pos >= lozfile->buffsize)
                {
                        //compress wrbuff[] into lzbuff[], write to file
                        err = loz_flush_wrbuff_to_file( lozfile );
                        if(err != lozfile->buffsize) {
                                MYLOG_ERROR("loz_flush_wrbuff_to_file() returns error=%d", err);
                                return err;
                        }
                }
        }
    
        return size;
}

//------------------------------------------------------------------------------
//Read data from lz-file
//inputs:   lozfile = pointer to lz-file
//returns:  readed = number of valid bytes successfully readed from file (size - corrupted bytes)
//          LOZ_ERROR
//          LOZ_UNSUPPORTED
int loz_read( lozfile_t * lozfile, void * ptr, int size )
{
        int                err;
        int                i;
        uint8_t          * p;
        lozfile_section_t   section;
        lozfile_section_t   next;
        int                decompsize;
        int                readed;

        MYLOG_TRACE("@(lozfile=%p,ptr=%p,size=%d)", lozfile, ptr, size);

        //check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument: lozfile=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("invalid argument: lozfile->fd=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->rdbuff==NULL) {
                MYLOG_ERROR("invalid argument: lozfile->rdbuff=NULL");
                return LOZ_ERROR;
        }
        if(lozfile->buffsize==0) {
                MYLOG_ERROR("invalid argument: lozfile->buffsize=0");
                return LOZ_ERROR;
        }
        if(ptr==NULL) {
                MYLOG_ERROR("invalid argument: ptr=NULL");
                return LOZ_ERROR;
        }
        if(size<=0) {
                MYLOG_ERROR("invalid argument: size=%d", size);
                return LOZ_ERROR;
        }

        readed = 0;
        p = ptr;
        for(i=0; i<size; i++)
        {
                //has full block been readed?
                if( (lozfile->rdbuff_n == 0) ||
                    (lozfile->rdbuff_pos >= lozfile->buffsize) )
                {
                        MYLOG_DEBUG("lozfile->rd_rawpos=%ld", lozfile->rd_rawpos);

                        lozfile->rdbuff_pos = 0;

                        //read section header from file
                        err = loz_read_section_header( lozfile, &section, lozfile->rd_fpos );
                        if(err==LOZ_ERROR) {
                                MYLOG_ERROR("loz_read_section_header() failed");
                                return LOZ_ERROR;
                        }
                        else if(err==LOZ_EOF) {
                                MYLOG_ERROR("loz_read_section_header() return End Of File");
                                return readed;
                        }
                        else if(err==LOZ_BAD_CRC) {
                                MYLOG_DEBUG("loz_read_section_header() return Bad CRC. Try to repair it!");
                                //go-go-go
                        }
                        
                        MYLOG_DEBUG("section.fpos           =%ld", section.fpos);
                        MYLOG_DEBUG("section.header_is_valid=%d",  section.header_is_valid);
                        MYLOG_DEBUG("section.rawpos         =%d",  section.rawpos);
                        MYLOG_DEBUG("section.rawpos_end     =%d",  section.rawpos_end);
                        MYLOG_DEBUG("section.rawsize        =%d",  section.rawsize);
                        MYLOG_DEBUG("section.compsize       =%d",  section.compsize);
                        MYLOG_DEBUG("lozfile.compression     =%s",  compression_to_str(lozfile->compression) );
                        
                        if(section.header_is_valid)
                        {
                                //read compressed data to lzbuff[]
                                err = loz_read_compdata( lozfile,
                                                        section.fpos + LOZ_SECTIONHEADER_SIZE,
                                                        lozfile->lzbuff,
                                                        section.compsize );
                                if(err==LOZ_ERROR) {
                                        MYLOG_ERROR("loz_read_compdata() failed with LOZ_ERROR");
                                        return LOZ_ERROR;
                                }
                                else if(err==LOZ_EOF) {
                                        MYLOG_WARNING("loz_read_compdata() failed with LOZ_EOF");
                                        return readed;
                                }
                                else if(err==LOZ_BAD_CRC) {
                                        MYLOG_WARNING("loz_read_compdata() failed with LOZ_BAD_CRC");
                                        //fill rdbuff[] with LOZ_FILLER
                                        memset(lozfile->rdbuff, LOZ_FILLER, section.rawsize);
                                }
                                else if(err==LOZ_OK) {
                                        //uncompress data from lzbuff[] to rdbuff[]
                                        err = loz_uncompress_data ( lozfile->compression,
                                                                   lozfile->lzbuff,
                                                                   section.compsize,
                                                                   lozfile->rdbuff,
                                                                   lozfile->buffsize,
                                                                   &decompsize );
                                        if(err != LOZ_OK) {
                                                MYLOG_ERROR("Could not uncompress section-data: loz_uncompress_data() failed with error=%d", err);
                                                return LOZ_ERROR;
                                        }
                                        
                                }
                                else {
                                        MYLOG_ERROR("Unexpected error of loz_read_compdata(): %d", err);
                                        return LOZ_ERROR;
                                }
                                
                                lozfile->rd_fpos = section.fpos + LOZ_SECTIONHEADER_SIZE + section.compsize + LOZ_CRC_SIZE;
                        }
                        else {
                                //1.try to search for the next section,
                                //2.calculate rawsize of current (invalid) section,
                                //3.fill rdbuff[] with LOZ_FILLER byte
                                
                                err = loz_section_next ( lozfile, &section, &next );
                                if(err==LOZ_ERROR) {
                                        MYLOG_ERROR("Current section is invalid, could not get next section");
                                        return readed;
                                }
                                else if(err==LOZ_EOF) {
                                        MYLOG_ERROR("Current section is invalid and last: could not get next section");
                                        return readed;
                                }
                                else if(err==LOZ_BAD_CRC) {
                                        MYLOG_ERROR("Current section is invalid, next section is invalid");
                                        return readed;
                                }
                                
                                MYLOG_DEBUG("try to repair section.rawsize: next.rawpos=%ld, lozfile->rd_rawpos=%ld",
                                            next.rawpos, lozfile->rd_rawpos);
                                
                                section.rawsize = next.rawpos - lozfile->rd_rawpos;
                                if(section.rawsize > lozfile->buffsize) {
                                        MYLOG_ERROR("Too big value of repaired section.rawsize=%d > lozfile->buffsize=%d",
                                                    section.rawsize, lozfile->buffsize);
                                        return readed;
                                }
                                
                                section.compsize = next.fpos - section.fpos - LOZ_SECTIONHEADER_SIZE - LOZ_CRC_SIZE;
                                
                                //read compressed data to lzbuff[]
                                err = loz_read_compdata( lozfile,
                                                        section.fpos + LOZ_SECTIONHEADER_SIZE,
                                                        lozfile->lzbuff,
                                                        section.compsize );
                                if(err==LOZ_ERROR) {
                                        MYLOG_ERROR("loz_read_compdata() failed with LOZ_ERROR");
                                        return LOZ_ERROR;
                                }
                                else if(err==LOZ_EOF) {
                                        MYLOG_WARNING("loz_read_compdata() failed with LOZ_EOF");
                                        return readed;
                                }
                                else if(err==LOZ_BAD_CRC) {
                                        MYLOG_WARNING("loz_read_compdata() failed with LOZ_BAD_CRC");
                                        //fill rdbuff[] with LOZ_FILLER
                                        memset(lozfile->rdbuff, LOZ_FILLER, section.rawsize);
                                }
                                else if(err==LOZ_OK) {
                                        //uncompress data from lzbuff[] to rdbuff[]
                                        err = loz_uncompress_data ( lozfile->compression,
                                                                   lozfile->lzbuff,
                                                                   section.compsize,
                                                                   lozfile->rdbuff,
                                                                   lozfile->buffsize,
                                                                   &decompsize );
                                        if(err != LOZ_OK) {
                                                MYLOG_ERROR("Could not uncompress section-data: loz_uncompress_data() failed with error=%d", err);
                                                return LOZ_ERROR;
                                        }
                                        
                                }
                                else {
                                        MYLOG_ERROR("Unexpected error of loz_read_compdata(): %d", err);
                                        return LOZ_ERROR;
                                }
                                
                                lozfile->rd_fpos = section.fpos + LOZ_SECTIONHEADER_SIZE + section.compsize + LOZ_CRC_SIZE;
                                
                                //fill rdbuff[] with LOZ_FILLER
                                //memset(lozfile->rdbuff, LOZ_FILLER, section.rawsize);
                        }
                        
                        if(decompsize > lozfile->buffsize) {
                                MYLOG_ERROR("decompsize=%d is too big (lozfile->buffsize=%d)", decompsize, lozfile->buffsize);
                                return LOZ_ERROR;
                        }
                        
                        lozfile->rdbuff_n += decompsize;
                }
                
                //output data from rdbuff[]
                *p++ = lozfile->rdbuff[ lozfile->rdbuff_pos++ ];
                
                lozfile->rd_rawpos ++;
                lozfile->rdbuff_n --;
                
                readed ++;
        }
        return size;
}

//------------------------------------------------------------------------------
//printf to the end of lz-file
//inputs:   lozfile = pointer to lz-file
//          format, ...  - printf string
//returns:  0 = ok
//         -1 = error
int loz_printf( lozfile_t * lozfile, const char * format, ... )
{
        int         n;
        int         err;
        va_list     arg;

        MYLOG_TRACE("@(lozfile=%p,format=%s,...)", lozfile, format);

        //Check input arguments
        if(lozfile==NULL) {
                MYLOG_ERROR("invalid argument lozfile=NULL");
                return -1;
        }
        if(lozfile->fd==NULL) {
                MYLOG_ERROR("lozfile is not opened yet");
                return -1;
        }
        if(format==NULL) {
                MYLOG_ERROR("invalid argument format=NULL");
                return -1;
        }

        //printf
        va_start(arg,format);
        n = vsnprintf( (char*)lozfile->strbuff, lozfile->buffsize, format, arg );
        va_end(arg);

        if(n<0)
                return -1;

        //write data to file
        err = loz_write( lozfile, (char*)lozfile->strbuff, n );
        if(err != n) {
                MYLOG_ERROR("could not write %d bytes: loz_write() failed", n);
                return -1;
        }
        
        //flush
        //loz_flush( lozfile );

        return n;
}

//------------------------------------------------------------------------------
//vprintf to the end of lz-file
//inputs:   lozfile = pointer to lz-file
//          format, ...  - printf string
//returns:  0 = ok
//         -1 = error
int loz_vprintf( lozfile_t * lozfile, const char * format, va_list arg )
{
        int      n;
        int      err;

        MYLOG_TRACE("@(lozfile=%p,format=%s,...)", lozfile, format);

        //check input arguments
        if(lozfile==NULL)
                return -1;
        if(lozfile->fd==NULL)
                return -1;
        if(lozfile->wrbuff==NULL)
                return -1;
        if(lozfile->buffsize==0)
                return -1;
        if(format==NULL)
                return -1;

        //vprintf
        n = vsnprintf( (char*)lozfile->strbuff, lozfile->buffsize, format, arg );
        if(n<0)
                return -1;

        //write data to file
        err = loz_write( lozfile, (char*)lozfile->strbuff, n );
        if(err != 1)
                return -1;

        //flush
        //loz_flush( lozfile );

        return n;
}

//------------------------------------------------------------------------------
//Close lz-file.
//inputs:   lozfile = pointer to lz-file
void loz_close( lozfile_t * lozfile )
{
        MYLOG_TRACE("@(lozfile=%p)", lozfile);
        
        if(lozfile) {
                if(lozfile->fd) {
                        //fflush data
                        loz_flush(lozfile);
                        //close file
                        fclose(lozfile->fd);
                }
                if(lozfile->rdbuff)
                        free(lozfile->rdbuff);
                if(lozfile->wrbuff)
                        free(lozfile->wrbuff);
                if(lozfile->strbuff)
                        free(lozfile->strbuff);
                if(lozfile->lzbuff)
                        free(lozfile->lzbuff);
                free(lozfile);
        }
        return;
}

//------------------------------------------------------------------------------
//Flush lz-file.
//inputs:   lozfile = pointer to lz-file
void loz_flush( lozfile_t * lozfile )
{
        int err;
        
        MYLOG_TRACE("@(lozfile=%p)", lozfile);

        //check input arguments
        if(lozfile==NULL)
                return;
        if(lozfile->fd==NULL)
                return;
        if(lozfile->wrbuff==NULL)
                return;
        if(lozfile->lzbuff==NULL)
                return;

        //flush available data from wrbuff[]
        err = loz_flush_wrbuff_to_file( lozfile );
        if(err < LOZ_OK) {
                MYLOG_ERROR("loz_flush_wrbuff_to_file() failed with error=%d", err);
        }

        fflush( lozfile->fd );
        return;
}

//------------------------------------------------------------------------------
//Get current valid size of LOZ-file.
//inputs:   lozfile = pointer to loz-file
void loz_filesize( lozfile_t * lozfile )
{
        int err;
        
        MYLOG_TRACE("@(lozfile=%p)", lozfile);

        //check input arguments
        if(lozfile==NULL)
                return;
        if(lozfile->fd==NULL)
                return;
        
        return lozfile->filesize;
}
