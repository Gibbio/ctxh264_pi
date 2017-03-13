/***************************************************************************
*
*   libjpeg.c
*
*   Wrap libjpeg.c to implement the client's own JPEG decoding interface.
*
*   Copyright 2001-2016 Citrix Systems, Inc.  All Rights Reserved.
*
*   $Id: //LinuxBasedReceivers/13.5/ClientEngine/base/pal/ctxgraph/unix/libjpeg.c#1 $
*
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <string.h>

#include "jpeg_decode.h"
#include "srcmgr.h"
#include "errmgr.h"

/* Forward declarations. */
static void start_decode(struct JPEG_request *);
static void finish_decode(struct JPEG_request *);
static struct JPEG_request *complete_request(void);
static void batch_decode(struct JPEG_request request[], int num_requests);

/* This structure tells the caller what this wrapper can do and provides
 * the entry points.
 */

struct JPEG_decoder JPEG_decoder = {
    TRADITIONAL_JPEG,           /* Input formats. */
    PIXEL_XRGB,                 /* Output pixel formats - just the one. */
    0,                          /* No parallel processing. */
    1,                          /* No internal concurrency. */
    0,                          /* No internal queueing. */
    4,                          /* Alignment preference. */
    start_decode,               /* Entry points. */
    finish_decode,
    complete_request,
    batch_decode
};

/* Should we use libjpeg-turbo? */
static boolean use_turbo = TRUE;

/* Batch decode debugging enabled? */
static boolean batchDebugging = FALSE;

/* Debug direct decode cropping enabled? */
static boolean debug_cropping = FALSE;


/*****************************************************************************
 * LibJpegCustomErrorExit
 *
 * Replacement function for the default libJPEG error function.  This function
 * does not call exit() like the standard libJPEG error manager.  Rather it
 * calls the longjmp function to return to the line after the caller of setjmp
 * This avoids returning the error code to the caller through the libJPEG
 * library, which makes no provision for this to happen
 *
 * Parameters
 *
 * cinfo: Pointer to JPEG decompression structure
 *
 * Returns
 *
 * Does not return, rather uses longjmp to restore stack to saved state
 *****************************************************************************/

static void
LibJpegCustomErrorExit(j_common_ptr cinfo)
{
    /*Get pointer to error handler*/

    PCTXS_JPEG_ERROR_MANAGER pErrorManager = (PCTXS_JPEG_ERROR_MANAGER) cinfo->err;

    /*Execute the longjmp to restore the stack state*/

    longjmp(pErrorManager->SetJumpStackState,-1);
}

/* Initialisation entry point.  Called by the OS on load. */

__attribute__((constructor)) static void init_decoder()
{
    struct jpeg_compress_struct cinfo;
    CTXS_JPEG_ERROR_MANAGER jerr;

    /* Try and determine whether libjpeg-turbo is available by 
     * attempting an encode using one of the extended color
     * spaces libjpeg-turbo supports, namely JCS_EXT_RGB (6). */

    /* Set up the error manager. */

    cinfo.err = jpeg_std_error( (struct jpeg_error_mgr *)&jerr );
    jerr.OriginalErrorManager.error_exit = LibJpegCustomErrorExit;
    if (setjmp(jerr.SetJumpStackState)) {
        /* "longjmp" here on error, see below. */
        use_turbo = FALSE;
        goto done;
    }		

    /* Initialize the compression struct. */

    jpeg_create_compress(&cinfo);

    cinfo.input_components = 3;

    /* Try an input color space of JCS_EXT_RGB (R0 G0 B0 R1 G1 B1 ...). */

    cinfo.in_color_space = 6 /* JCS_EXT_RGB */;

    /* The following call can succeed or throw an error:
       Success: execution continues, and use_turbo is set to "TRUE".
       Failure: LibJpegCustomErrorExit is invoked, and control is returned
       to the return point above. use_turbo is set to "FALSE",
       indicating that libjpeg was unable to honour the desired
       colour space, and we can assume libjpeg-turbo is not
       installed.
    */

    jpeg_set_defaults(&cinfo);

    /* libjpeg-turbo is installed. */

    use_turbo = TRUE;

done:
    /* Destroy the compression struct. */

    jpeg_destroy_compress( &cinfo );

    /* If the "CTXJPEG_FB_SW_BATCH_SIZE" env. variable is set, then tell
     * Receiver we are able to process batches of JPEGs. This is especially
     * useful for batch decode debugging and test. This parameter should be set
     * to an integer greater than one.
     */

    char *softwareBatchSize = getenv("CTXJPEG_FB_SW_BATCH_SIZE");
    if (softwareBatchSize) {        
        int temp = atoi(softwareBatchSize);
        if (temp > 1) {
            JPEG_decoder.concurrency = temp;
            JPEG_decoder.completion_handling |= BATCH_DECODING;
            printf("ctxjpeg_fb: using batch size of %d.\n", JPEG_decoder.concurrency);
            batchDebugging = TRUE;
        }
    }

    /* Debug cropping enabled? */

    debug_cropping = (NULL != getenv("CTXJPEG_FB_SW_DEBUG_CROPPING"));
    if (debug_cropping) {
        JPEG_decoder.output_formats |= CROP_OUTPUT_X_OFFSET | CROP_OUTPUT_Y_OFFSET;
        printf("Enabling debug cropping support.\n");
    }

}


static void start_decode(struct JPEG_request *request)
{
    struct jpeg_decompress_struct cinfo;        /* libjpeg decoding context */
    CTXS_JPEG_ERROR_MANAGER       jerr;         /* Custom error manager */
    int                           jmpRet;       /* Return value of setjmp() */
    unsigned char                *pBmpRow;
    JSAMPARRAY					  scanlineBuffer[2];
    int                           decode_height;
    int                           stride;
    unsigned                      char *old_buf = 0, *waste = 0;
    boolean                       cropped = FALSE;
    
    if (!request || !request->v2.image || !request->v2.buffer) {
        if (request)
            request->v2.status = JPEG_BAD_PARAM;
        return;
    }
    
    /* Create the error handler. */
    
    cinfo.err = jpeg_std_error((struct jpeg_error_mgr *)&jerr);

    /* Overwrite the default error handler exit function. */

    jerr.OriginalErrorManager.error_exit = LibJpegCustomErrorExit;
    
    /*Save stack state*/

    jmpRet = setjmp(jerr.SetJumpStackState);

    if (jmpRet != 0)
    {
        /* There was an error in the jpeg library,
         * all error processing code occurs here.
         */
#ifdef DEBUG
        char buf[JMSG_LENGTH_MAX];

        cinfo.err->format_message((j_common_ptr)&cinfo, buf);
        fprintf(stderr, "Error occured in libJPEG: %s\n", buf);
#endif /* DEBUG */

        /* Clean up.  Don't delete the decompress structure in the
         * exception handler since we can get an infinite loop.
         */

        request->v2.status = JPEG_INTERNAL;
        return;
    }

    /* Create decompression manager. */

    jpeg_create_decompress(&cinfo);

    /* Associate the JPEG data with the decompression manager
     * and the callback function.
     */
    
    srcmgr_jpeg_memory_src(&cinfo, request->v2.image, request->v2.size);

    /* Read the file header, setting the default compression parameters. */

    /**************************************
     ****** FIXME - map error codes. ******
     **************************************
     */

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        /*There was an error reading the file header*/

        fprintf(stderr, "jpeg_read_header() failed\n");
        jpeg_destroy_decompress(&cinfo);
        request->v2.status =  JPEG_BAD_DATA;
        return;
    }

    if (use_turbo) {
	    /* We've established that libjpeg-turbo is available. Specify
	       an output colour space of BGRX (4-bytes), avoiding the need
	       for a 24-to-32 bit colour space conversion. */
	    cinfo.out_color_space = 9; /* JCS_EXT_BGRX */
    }

    if (!jpeg_start_decompress(&cinfo)) {
        jpeg_destroy_decompress(&cinfo);
        request->v2.status = JPEG_BAD_DATA;
        return;
    }

    /* Check if we need to crop. If so, decode to a temporary buffer
     * then do a "fake" direct decode to the original buffer for
     * x-direction cropping.
     */

    if (   debug_cropping
        && (request->v2.crop_x > 0 || request->v2.crop_y > 0 || request->v2.crop_width != cinfo.image_width)
       ) {
        waste = (unsigned char *)
                    malloc(cinfo.image_width * cinfo.image_height * 4);
        old_buf = request->v2.buffer;

        /* Decode to our waste buffer. */

        request->v2.buffer = waste;

        /* Decode all the image. */

        decode_height = cinfo.image_height;
        stride = cinfo.image_width * 4;
        cropped = TRUE;
    } else {
        decode_height = request->v2.crop_height;
        stride = request->v2.stride;
    }

    if (use_turbo) {
        pBmpRow = request->v2.buffer;  /* Point at first scanline in output. */
        scanlineBuffer[0] = (JSAMPARRAY)pBmpRow;

        /* Decode all lines. */

        while (cinfo.output_scanline < decode_height) {
            jpeg_read_scanlines(&cinfo, (JSAMPARRAY)scanlineBuffer, 1);
            pBmpRow += stride;
            scanlineBuffer[0] = (JSAMPARRAY)pBmpRow;
        }
    } else {
        /* Bail out on unhandled image formats. */

        if ((JCS_RGB       != cinfo.out_color_space) &&
            (JCS_GRAYSCALE != cinfo.out_color_space)) {
            fprintf(stderr, "libjpeg: unsupported image format %d\n",
                    cinfo.out_color_space);

            /* Clean up and return error code. */
	        
            jpeg_destroy_decompress(&cinfo);        
            request->v2.status = JPEG_BAD_PARAM;
            return;
        }

        /*
         * Allocate the scanline buffer: this buffer receives a
         * single scanline from the JPEG.  Note that an allocation failure will
         * call the errorexit handler which will longjmp us back to
         * the error handler at the start of the function
         */

        scanlineBuffer[0] = (cinfo.mem->alloc_sarray)(
                                (j_common_ptr)&cinfo,
                                JPOOL_IMAGE,
                                cinfo.image_width * cinfo.output_components,
                                1);

        /*
         * Now we process each scanline in the image.
         * As we read each scanline from the JPEG library,
         * we add it to the bitmap data buffer
         */
	     
        pBmpRow = request->v2.buffer;  /* Point at first scanline in output. */

        /* Decode the requested number of lines. */

        while (cinfo.output_scanline < decode_height) {
            JDIMENSION       column;            /* Current column in image. */
            unsigned int    *pBmpCol;           /* Pointer to column in BMP. */
            unsigned char   *pSource;           /* Pointer to source data. */
	        
            pBmpCol = (unsigned int *)pBmpRow;  /* Next output location. */

            /* Read a single scanline from the JPEG image. */

            jpeg_read_scanlines(&cinfo, scanlineBuffer[0], 1);
            pSource = scanlineBuffer[0][0];
	        
            /* Now add this scanline to the bitmap. */

            if (JCS_RGB == cinfo.out_color_space) {
                for (column = cinfo.image_width; column > 0; column--) {
                    *pBmpCol = (((*pSource << 8) + pSource[1]) << 8) +
                                   pSource[2];
                    ++pBmpCol;
                    pSource += 3;
                }
            } else {
                unsigned char b;

                for (column = cinfo.image_width; column > 0; column--) {
                    b = *pSource++;
                    *pBmpCol++ = (((b << 8) + b) << 8) + b;
                }
            }
            pBmpRow += stride;
        }
    }

    /* As per libjpeg documentation, jpeg_finish_decompress should not be
       called if we've finished with the object early. This can occur when
       only a partial number of lines is required.
       Instead, jpeg_abort_compress() should be called.
     */

    if (cinfo.output_height < cinfo.image_height) {
        /* Decoded part of the image, call abort. */

        jpeg_abort_decompress(&cinfo);
    } else {
        jpeg_finish_decompress(&cinfo);
    }
    jpeg_destroy_decompress(&cinfo);

    if (cropped) {
        int            y;
        unsigned char *ptr = old_buf;

        /* Now we've got to copy to form the cropped JPEG. */

        for (y = 0; y < request->v2.crop_height; y++) {
            unsigned char *img_ptr;
            /* Find the correct point in the source image. */

            img_ptr = request->v2.buffer +
                        ((y + request->v2.crop_y) * cinfo.image_width * 4) +
                        (request->v2.crop_x * 4);

            /* Copy the full or partial scanline. */

            memcpy(ptr, img_ptr, request->v2.crop_width * 4);

            /* Advance ptr. */
            ptr += request->v2.stride;
        }
        /* Free waste buffer. */

        free(request->v2.buffer);

        /* Ensure we return the correct buffer! */

        request->v2.buffer = old_buf;
    }
    request->v2.status = JPEG_SUCCESS;
}

/* Dummy functions for handling non-existent non-blocking requests. */

static void finish_decode(struct JPEG_request *request)
{
}

static struct JPEG_request *complete_request(void)
{
    return NULL;
}

/* Synchronous batch decoding implementation. This function simply processes
 * the batch of JPEGs serially, and is only useful for debugging purposes. 
 */

static void batch_decode(struct JPEG_request request[], int num_requests)
{
    int i;

    if (batchDebugging) {
        /* Report the number of JPEGs in this batch. */

        printf("ctxjpeg_fb::batch_decode(%d)\n", num_requests); 
    }
    
    for (i = 0; i < num_requests; i++) {
        start_decode(&request[i]);
    }
}
