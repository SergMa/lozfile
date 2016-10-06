/******************************************************************************/
/* loz.c                                                                      */
/* COMPRESS-DECOMPRESS UTILITE FOR LOZ-FILES                                  */
/*                                                                            */
/* Copyright (c) 2016 Sergey Mashkin                                          */
/******************************************************************************/

#include  <stdio.h>
#include  <sys/signal.h>
#include  <unistd.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdlib.h>
#include  <time.h>
#include  <sys/sysinfo.h>
#include  <sys/time.h>
#include  <sys/resource.h>

#include  "lozfile.h"

#define MYLOGDEVICE 1 //MYLOGDEVICE_STDOUT
#include  "mylog.h"

/******************************************************************************/
/* GLOBAL VARIABLES                                                           */
/******************************************************************************/

#define DEFAULT_SEGMENTSIZE     16384
#define DEFAULT_METHOD          "fastlz2"

#define ACTION_NULL             0
#define ACTION_HELP             1
#define ACTION_CREATE           2
#define ACTION_ADD              3
#define ACTION_EXTRACT          4
#define ACTION_ERROR            5

static int   action;
static char  filename1[256];
static char  filename2[256];
static char  method[16];
static int   segmentsize;

char * usagestr =
"\n"
"-----------------------------------------------------\n"
"NAME:\n"
"  loz\n"
"\n"
"DESCRIPTION:\n"
"  loz is compress-decompress program to work with\n"
"  files in LOZ-format.\n"
"\n"
"USAGE:\n"
"  loz -c <file> [<archive.loz>] [-m <method>] [-s <segmentsize>]\n"
"    Compress <file> with LOZ compressor. If name of output\n"
"    file not defined, original name of <file> will be used with\n"
"    .loz extension.\n"
"    -m <method> - set compression method if needed. Supported\n"
"    values are: none, rle, rle2, lz, fastlz1, fastlz2\n"
"    -s <segmentsize> - set segment size. Supported values\n"
"    are: 128...65536\n"
"\n"
"  loz -a <file> <archive.loz> [-s <segmentsize>]\n"
"    Compress <file> with LOZ compressor and add it to existing\n"
"    LOZ archive <archive.loz>. If LOZ archive does not exist,\n"
"    it will be created.\n"
"    -s <segmentsize> - set segment size. Supported values\n"
"    are: 128...65536\n"
"\n"
"  loz -x <archive.loz> [<file>]\n"
"    Decompress <archive.loz> with LOZ decompressor and write uncompressed\n"
"    data to <file>. If <file> exists it will be overwritten.\n"
"    If name of <file> is not defined and <archive.loz> filename has\n"
"    .loz extension, <archive.loz> filename without .loz will be\n"
"    used as name of output <file>.\n"
"\n"
"  loz -h\n"
"      Show help information (this page).\n"
"\n"
"    NOTE: full names of arguments may be used:\n"
"    --create      instead of -c\n"
"    --add         instead of -a\n"
"    --extract     instead of -x\n"
"    --method      instead of -m\n"
"    --segmentsize instead of -s\n"
"    --help        instead of -h\n"
"-----------------------------------------------------\n";

/*** FUNCTIONS *************************************/

//------------------------------------------------------------------------------
//Print USAGE message
void usage( void )
{
    printf("%s", usagestr);
}

//------------------------------------------------------------------------------
//Convert action code to string
char * action_to_str( int action )
{
    static char str[32];
    switch(action)
    {
    case ACTION_NULL:       return "null";
    case ACTION_HELP:       return "help";
    case ACTION_CREATE:     return "create";
    case ACTION_ADD:        return "add";
    case ACTION_EXTRACT:    return "extract";
    case ACTION_ERROR:      return "error";
    default:
        snprintf(str,sizeof(str),"?(%d)",action);
        return str;
    }
}

//------------------------------------------------------------------------------
//Check if method is valid
int method_from_str( char * method )
{
    if(0==strcmp(method,"none")) {
        return LOZ_COMPRESSION_NONE;
    }
    else if(0==strcmp(method,"rle")) {
        return LOZ_COMPRESSION_RLE;
    }
    else if(0==strcmp(method,"rle2")) {
        return LOZ_COMPRESSION_RLE2;
    }
    else if(0==strcmp(method,"lz")) {
        return LOZ_COMPRESSION_LZ;
    }
    else if(0==strcmp(method,"fastlz1")) {
        return LOZ_COMPRESSION_FASTLZ1;
    }
    else if(0==strcmp(method,"fastlz2")) {
        return LOZ_COMPRESSION_FASTLZ2;
    }
    else {
        printf("method=%s is unsupported\n", method);
        return -1;
    }
}

//------------------------------------------------------------------------------
//Check if segmentsize is valid
int segmentsize_valid( int segmentsize )
{
    if( (segmentsize >= LOZ_BLOCKSIZE_MIN) ||
        (segmentsize <= LOZ_BLOCKSIZE_MAX)   )
    {
        return 1;
    }
    else {
        printf("segmentsize=%d is unsupported\n", segmentsize);
        return 0;
    }
}

//------------------------------------------------------------------------------
//Parse arguments of command line: get action and parameters
void parse_arguments( int argc, char *argv[] )
{
    int pos;

    action       = ACTION_NULL;
    filename1[0] = '\0';
    filename2[0] = '\0';
    method[0]    = '\0';
    segmentsize  = -1;
    
    //get action-code and parameters from command line arguments
    pos = 0;
    while( ++pos<argc )
    {
            MYLOG_DEBUG("pos=%d", pos);
       
            if( (0==strcasecmp(argv[pos],"--help")) ||
                (0==strcasecmp(argv[pos],"-h")) )
            {
                    if(argc>2)
                        goto exit_fail; //too many arguments
                    action = ACTION_HELP;
                    break;
            }
            
            if( (0==strcasecmp(argv[pos],"--create")) ||
                (0==strcasecmp(argv[pos],"-c")) )
            {
                    if(action != ACTION_NULL)
                            goto exit_fail; //many actions in single command
                    action = ACTION_CREATE;
                    
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename1, sizeof(filename1), "%s", argv[pos] );
        
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename2, sizeof(filename2), "%s", argv[pos] );

                    continue;
            }
            
            if( (0==strcasecmp(argv[pos],"--add")) ||
                (0==strcasecmp(argv[pos],"-a")) )
            {
                    if(action != ACTION_NULL)
                            goto exit_fail; //many actions in single command
                    action = ACTION_ADD;
                    
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename1, sizeof(filename1), "%s", argv[pos] );
        
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename2, sizeof(filename2), "%s", argv[pos] );
        
                    continue;
            }
            
            if( (0==strcasecmp(argv[pos],"--extract")) ||
                (0==strcasecmp(argv[pos],"-x")) )
            {
                    if(action != ACTION_NULL)
                            goto exit_fail; //many actions in single command
                    action = ACTION_EXTRACT;
                    
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename1, sizeof(filename1), "%s", argv[pos] );
        
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( filename2, sizeof(filename2), "%s", argv[pos] );
        
                    if(filename1[0]=='\0')
                            goto exit_fail; //'filename1' does not exist
                    if(filename2[0]=='\0') {
                            if( strstr(filename1,".lzf") == filename1 + strlen(filename1) - 4 )
                                    snprintf(filename2, strlen(filename1) - 3, filename1);
                            else
                                    goto exit_fail; //could not generate 'filename2'
                    }
                    continue;
            }
            
            if( (0==strcasecmp(argv[pos],"--method")) ||
                (0==strcasecmp(argv[pos],"-m")) )
            {
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            snprintf( method, sizeof(method), "%s", argv[pos] );
    
                    if(method[0]=='\0')
                            goto exit_fail; //'method' does not exist after --method
                    continue;
            }
            
            if( (0==strcasecmp(argv[pos],"--segmentsize")) ||
                (0==strcasecmp(argv[pos],"-s")) )
            {
                    pos++;
                    if( (pos<argc) && (argv[pos][0]!='-') )
                            segmentsize = atoi(argv[pos]);
                            
                    if(segmentsize==-1)
                            goto exit_fail; //'segmentsize' does not exist after --segmentsize
                    continue;
            }

            goto exit_fail; //unknown action, invalid arguments
    }

    //post process (set defaults, check validity)
    switch(action)
    {
    case ACTION_CREATE:
            if(filename1[0]=='\0')
                    goto exit_fail;
            if(filename2[0]=='\0')
                    snprintf( filename2, sizeof(filename2), "%s.lzf", filename1 );
            if(method[0]=='\0')
                    snprintf(method,sizeof(method),DEFAULT_METHOD);
            if(segmentsize==-1)
                    segmentsize = DEFAULT_SEGMENTSIZE;
            if(method_from_str(method)<0)
                    goto exit_fail;
            if(!segmentsize_valid(segmentsize))
                    goto exit_fail;
            break;
    
    case ACTION_ADD:
            if(filename1[0]=='\0')
                    goto exit_fail;
            if(filename2[0]=='\0')
                    goto exit_fail;
            if(method[0]!='\0')
                    goto exit_fail;
            if(segmentsize==-1)
                    segmentsize = DEFAULT_SEGMENTSIZE;
            if(!segmentsize_valid(segmentsize))
                    goto exit_fail;
            break;

    case ACTION_EXTRACT:
            if(filename1[0]=='\0')
                    goto exit_fail;
            if(filename2[0]=='\0') {
                    if( strstr(filename1,".lzf") == filename1 + strlen(filename1) - 4 )
                            snprintf(filename2, strlen(filename1) - 3, filename1);
                    else
                            goto exit_fail; //could not generate 'filename2'
            }
            if(method[0]!='\0')
                    goto exit_fail;
            if(segmentsize!=-1)
                    goto exit_fail;
            break;

    case ACTION_HELP:
            if(filename1[0]!='\0')
                    goto exit_fail;
            if(filename2[0]!='\0')
                    goto exit_fail;
            if(method[0]!='\0')
                    goto exit_fail;
            if(segmentsize!=-1)
                    goto exit_fail;
            break;
    }
    
    MYLOG_DEBUG( "action          =%s", action_to_str(action) );
    MYLOG_DEBUG( "filename1       =%s", filename1             );
    MYLOG_DEBUG( "filename2       =%s", filename2             );
    MYLOG_DEBUG( "method          =%s", method                );
    MYLOG_DEBUG( "segmentsize     =%d", segmentsize           );
    return;
    
exit_fail:
    printf("Invalid arguments\n");
    action = ACTION_ERROR;
    return;
}

/*** MAIN FUNCTION *********************************/

//---------------------------------------------------
int main(int argc, char *argv[])
{
    int        err;
    FILE     * file = NULL;
    lozfile_t * lozfile = NULL;
    uint8_t  * buff = NULL;

    MYLOG_INIT( 0
      //| MYLOG_ENABLED_ALL
      | MYLOG_ENABLED_ERROR
      | MYLOG_ENABLED_USER
    );

    parse_arguments( argc, argv );

    //======== actions==========================================================
    switch(action)
    {
    case ACTION_ERROR:
        {
            printf("error: invalid arguments!\n"
                   "Use lzf --help to show usage page.\n");
            exit(EXIT_FAILURE);
        }

    case ACTION_HELP:
        {
            usage();
            exit(EXIT_SUCCESS);
        }

    case ACTION_CREATE:
        {
            printf("create LOZ archive\n");

            buff = malloc(segmentsize);
            if(buff==NULL) {
                printf("Error: could not allocate memory for buffer.\n");
                goto exit_fail;
            }
            file = fopen( filename1, "r" );
            if(file==NULL) {
                printf("Error: could not open file \"%s\".\n", filename1);
                goto exit_fail;
            }
            lozfile = loz_open( filename2, "w+", segmentsize, method_from_str(method) );
            if(lozfile==NULL) {
                printf("Error: could not create LOZ-archive \"%s\".\n", filename2);
                goto exit_fail;
            }
            while(1) {
                err = fread(buff,sizeof(uint8_t),1,file);
                if(err != 1) {
                    if(feof(file)) {
                        break;
                    }
                    else {
                        printf("Error: could not read from file \"%s\".\n", filename1);
                        goto exit_fail;
                    }
                }
                err = loz_write(lozfile,(char*)buff,1);
                if(err != 1) {
                    printf("Error: could not write to LOZ-archive \"%s\".\n", filename2);
                    goto exit_fail;
                }
            }
            fclose(file);
            loz_close(lozfile);
            free(buff);
            printf("ok.\n");
            exit(EXIT_SUCCESS);
        }

    case ACTION_ADD:
        {
            printf("add new data to LOZ archive\n");

            buff = malloc(1024);
            if(buff==NULL) {
                printf("Error: could not allocate memory for buffer.\n");
                goto exit_fail;
            }
            file = fopen( filename1, "r" );
            if(file==NULL) {
                printf("Error: could not open file \"%s\".\n", filename1);
                goto exit_fail;
            }
            lozfile = loz_open( filename2, "r+", 1024, LOZ_COMPRESSION_NONE );
            if(lozfile==NULL) {
                printf("Error: could not create LOZ-archive \"%s\".\n", filename2);
                goto exit_fail;
            }
            while(1) {
                err = fread(buff,sizeof(uint8_t),1,file);
                if(err != 1) {
                    if(feof(file)) {
                        break;
                    }
                    else {
                        printf("Error: could not read from file \"%s\".\n", filename1);
                        goto exit_fail;
                    }
                }
                err = loz_write(lozfile,(char*)buff,1);
                if(err != 1) {
                    printf("Error: could not write to LOZ-archive \"%s\".\n", filename2);
                    goto exit_fail;
                }
            }
            fclose(file);
            loz_close(lozfile);
            free(buff);
            printf("ok.\n");
            exit(EXIT_SUCCESS);
        }

    case ACTION_EXTRACT:
        {
            printf("extract LOZ archive\n");

            buff = malloc(65535);
            if(buff==NULL) {
                printf("Error: could not allocate memory for buffer.\n");
                goto exit_fail;
            }
            lozfile = loz_open( filename1, "r", 65535, LOZ_COMPRESSION_LZ );
            if(lozfile==NULL) {
                printf("Error: could not open LOZ-archive \"%s\".\n", filename1);
                goto exit_fail;
            }
            file = fopen( filename2, "w" );
            if(file==NULL) {
                printf("Error: could not open file \"%s\".\n", filename2);
                goto exit_fail;
            }
            while(1) {
                err = loz_read(lozfile,(char*)buff,1);
                if(err == 0) {
                    break;
                }
                else if(err != 1) {
                    printf("Error: could not read from LOZ-archive \"%s\".\n", filename1);
                    goto exit_fail;
                }
                err = fwrite(buff,sizeof(uint8_t),1,file);
                if(err != 1) {
                    if(feof(file)) {
                        break;
                    }
                    else {
                        printf("Error: could not write to file \"%s\".\n", filename2);
                        goto exit_fail;
                    }
                }
            }
            fclose(file);
            loz_close(lozfile);
            free(buff);
            printf("ok.\n");
            exit(EXIT_SUCCESS);
        }
    
    default:
        {
            printf("error: unexpected action=%d\n", action);
            exit(EXIT_FAILURE);
        }
    }

exit_fail:
        if(file)
            fclose(file);
        if(lozfile)
            loz_close(lozfile);
        if(buff)
            free(buff);
        exit(EXIT_FAILURE);
}

