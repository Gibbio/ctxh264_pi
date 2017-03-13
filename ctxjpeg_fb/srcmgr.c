/******************************************************************************
 * srcmgr.c
 *
 * Source manager implementation file.  The source manager implemented here has
 * a memory buffer as the JPEG data source.  The memory buffer contains the
 * entire JPEG file.
 *
 * Copyright 2001-2011 Citrix Systems, Inc.  All Rights Reserved.
 *
 * $Id: //LinuxBasedReceivers/13.5/ClientEngine/base/pal/ctxgraph/srcmgr.c#1 $
 *
 ****************************************************************************/
#ifdef NCS
#include "citrix.h"
#endif

#ifdef ENABLE_DYNAMIC_LIBJPEG
#include "ga_dljpg.h"
#else
#include <stdio.h>
#include <sys/types.h>
#include <jpeglib.h>
#endif /* ENABLE_DYNAMIC_LIBJPEG */

#define END_OF_JPEG_STREAM_BYTE_0   0xFF
#define END_OF_JPEG_STREAM_BYTE_1   0xD9

static void init_source(j_decompress_ptr cinfo);
static boolean fill_input_buffer(j_decompress_ptr cinfo);
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void term_source(j_decompress_ptr cinfo);
/*****************************************************************************
 * srcmgr_jpeg_memory_src
 *
 * Associates callback functions with decompress source structure.  The source
 * struct requires a number of functions for certain tasks.  In the default
 * libJPEG implementation, these are stdio related functions (ie source is file,
 * dest is file.  This implementation uses buffers as the source of the JPEG
 * data and for the output bitmap data
 *
 * Parameters
 *
 * cinfo: Pointer to decompression source manager
 *
 * pJpegDataBuffer: Pointer to buffer containing JPEG data
 *
 * jpegDataBufferSize: Size of the buffer pointed at by pJpegDataBuffer, (bytes)
 *
 * Returns
 *
 * This function has no return value
 *****************************************************************************/
void
srcmgr_jpeg_memory_src(
    j_decompress_ptr    cinfo,
    unsigned char      *pJpegDataBuffer,
    unsigned long       jpegDataBufferSize)
{
    struct jpeg_source_mgr     *pSourceManager;  /*Pointer to source manager*/


    /* Allocate a new jpeg_source_mgr structure. */


    cinfo->src = (*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
                                            JPOOL_PERMANENT,
                                            sizeof(struct jpeg_source_mgr));

    pSourceManager = cinfo->src;

    /* Fill in callback functions for source manager. */

    pSourceManager->init_source       = init_source;
    pSourceManager->fill_input_buffer = fill_input_buffer;
    pSourceManager->skip_input_data   = skip_input_data;
    pSourceManager->resync_to_restart = jpeg_resync_to_restart;
    pSourceManager->term_source       = term_source;

    /*
     * Set up the work buffer pointer and next input byte value.
     * Set these to the passed in buffer pointer and buffer size
     */

    pSourceManager->bytes_in_buffer = jpegDataBufferSize;
    pSourceManager->next_input_byte = pJpegDataBuffer;
}

/******************************************************************************
 * init_source
 *
 * Initialize source.  This is called by jpeg_read_header() before any data is
 * actually read.  Unlike init_destination(), it may leave bytes_in_buffer set
 * to 0 (in which case a fill_input_buffer() call will occur immediately).
 *
 * Parameters
 *
 * cInfo: Pointer to decompress structure
 *
 * Returns
 *
 * This function has no return value
 *****************************************************************************/

static void
init_source(j_decompress_ptr cinfo)
{
    /*Does nothing*/
}

/*****************************************************************************
 * fill_input_buffer
 *
 * This is called whenever bytes_in_buffer has reached zero and more data is
 * required.  This function sets the buffer pointer to an end of stream marker
 *
 * Parameters
 *
 *
 * cInfo: Pointer to decompress structure
 *
 * Returns
 *
 * TRUE: Always
 *****************************************************************************/

static boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
    static unsigned char        endMarker[2] = {END_OF_JPEG_STREAM_BYTE_0,
                                                END_OF_JPEG_STREAM_BYTE_1};
    struct jpeg_source_mgr     *pSourceManager;

    /* Just indicate EOS. */

    pSourceManager = cinfo->src;
    pSourceManager->next_input_byte = endMarker;
    pSourceManager->bytes_in_buffer = sizeof endMarker;
    return TRUE;
}

/*****************************************************************************
 * skip_input_data
 *
 * Skip num_bytes worth of data.  The buffer pointer and count should be
 * advanced over num_bytes input bytes, refilling the buffer as needed.  This
 * is used to skip over a potentially large amount of uninteresting data (such
 * as an APPn marker).  In some applications it may be possible to optimize away
 * the reading of the skipped data, but it's not clear that being smart is worth
 * much trouble; large skips are uncommon.  bytes_in_buffer may be zero on
 * return. A zero or negative skip count should be treated as a no-op.
 *
 * Parameters
 *
 * cInfo: Decompress struct pointer
 *
 * num_bytes: Number of bytes to skip
 *
 * Returns
 *
 * This function has no return value
 *****************************************************************************/

static void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{    
    struct jpeg_source_mgr     *pSourceManager;

    pSourceManager = cinfo->src;

    /*Check for nops (0byte or negative reads)*/

    if (num_bytes <= 0)
        return;         /* Treat as nop. */

    /* Determine whether it is possible to satisfy this request
     * based on the data that is currently in the buffer
     */

    if (num_bytes <= (long)pSourceManager->bytes_in_buffer) {
        /* Increment the pointer. */

        pSourceManager->next_input_byte += num_bytes;
        pSourceManager->bytes_in_buffer -= num_bytes;
    } else {
        pSourceManager->bytes_in_buffer = 0;
    }
}


/******************************************************************************
 * term_source
 *
 * Terminate source --- called by jpeg_finish_decompress() after all data has
 * been read.  Often a no-op.
 *
 * Parameters
 *
 * cInfo: Decompress struct pointer
 *
 * Returns
 *
 * This function has no return value
 *****************************************************************************/

static void
term_source(j_decompress_ptr cinfo)
{
}
