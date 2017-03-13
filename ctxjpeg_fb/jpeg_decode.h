/***************************************************************************
 *
 *   jpeg_decode.h
 *
 *   Citrix Receiver interface to JPEG decoders.
 *
 *   $Id: //LinuxBasedReceivers/13.5/ClientEngine/unix/inc/jpeg_decode.h#1 $
 *
 *   Copyright 2011-2014 Citrix Systems, Inc.  All Rights Reserved.
 *
 ***************************************************************************/

#ifndef _JPEG_DECODE_H_
#define _JPEG_DECODE_H_

/* This header defines the interface between the Citrix Receiver and
 * JPEG decoding libraries, particularly hardware-accelerated decoders.
 * A library implementing this interface will be dynamically loaded by
 * Receiver and is expected to have a single exported symbol, identifying a
 * structure which describes its capabilities and entry points.
 *
 * There are three entry points: start_decode() to pass a decoding request,
 * finish_decode() to wait for a request to complete and complete_request()
 * to determine which asynchronous, possibly parallel, request has completed.
 * There is also an option to use a callback function instead of calling
 * complete_request().
 *
 * All requests must be handled independently, with any clean-up following
 * failure performed before a request is considered complete.
 */

/* This structure describes a request to decode a compressed image.
 * When decoding is completed asynchronously, a structure pointer
 * is used as a handle to retrieve the completion status.
 *
 * Members are marked with I, O, B, N to indicate the direction of data
 * transfer: Input (to library), Output, Both, Neither.
 * Note that the caller of start_decode() allocates the output buffer.
 *
 * There are four methods that may be used by the library to indicate that
 * an asynchronous decoding request is complete.
 *   Polling: the library sets the "status" member in the request structure
 *     and the caller polls its outstanding requests between other tasks.
 *     This must always be supported.
 *   Callback: libraries that create a thread for their own use can call
 *     a function provided with the request to indicate completion.
 *     Creation of such a thread just for completion reporting is discouraged.
 *   Signal: the library (or kernel drivers) may deliver a signal, to the
 *     original calling thread (pthread_signal()).
 *   I/O completion: If there is an open descriptor for an I/O device that
 *     provides data when a request is complete, the library may return
 *     the file descriptor.  The caller may use it only to determine
 *     when data is ready to read (select(), poll(), epoll()) and will
 *     not perform any other operations on the descriptor.
 *
 * The caller may set callback to NULL, completion_sig to -1,
 * and completion_fd to -1 to indicate that those completion notification
 * methods will not be used.
 *
 * When either signal or file descriptor is used for completion
 * notification, a call to complete_request() will be made when the
 * notification is processed, to give control to the library so that
 * it has the opportunity to update the status and indicate which
 * request has completed.  Otherwise complete_request() must not be called.
 * The finish_request() function must only be called when none of the
 * asynchronous notification methods are in use for the request.
 */

struct JPEG_request_v2 {
    void               *image;          /* I: pointer to input. */
    unsigned int        size;           /* I: size of image (bytes). */
    void               *buffer;         /* I: output location. */
    unsigned int        width;          /* B: original image width (pixels). */
    unsigned int        height;         /* B: original image height (pixels). */

    unsigned int        stride;         /* I: output row separation (bytes). */
    unsigned int        format;         /* I: XXXXYYYY
                                              XXXX.... input coded format
                                              ....YYYY output pixel format.
                                              See input_formats & output_formats
                                              in JPEG_decoder struct.
                                              input is VERSION_JPEGSDK_3 */
    void               *priv;        /* N: Decoder's own use. */
    volatile int        status;         /* O: status of request. */
                                        /* I: only if COMPLETION_CB set. */
    void                (*callback)(struct JPEG_request_v2 *);
    int                 completion_sig; /* I: only if COMPLETION_SIG is set.*/
    int                 completion_fd;  /* B: only if COMPLETION_FD set. */

    /** VERSION_JPEGSDK_2 **/

    /* Crop the output if supported by the decoder. Used when direct decode
     * is enabled and only part of the image is wanted. Receiver will set the
     * fields below if the decoder advertises CROP_OUTPUT_* as part of
     * it's output formats (see below).
     *
     * If neither flag is specified AND direct decode is enabled, Receiver
     * will only issue bitmaps for decoding that have a destination height
     * up to and including the bitmap's original height, and decoding starts
     * from the first scanline i.e., crop_y == 0.
     */
    int                 crop_x;       /* Used when CROP_OUTPUT_X_OFFSET is set. */
    int                 crop_y;       /* Used when CROP_OUTPUT_Y_OFFSET is set. */  
    int                 crop_width;   /* Used when CROP_OUTPUT_X_OFFSET is set. */
    int                 crop_height;  /* Used when CROP_OUTPUT_Y_OFFSET is set. */
};

struct JPEG_request {
    struct JPEG_request_v2 v2;

    /** VERSION_JPEGSDK_3 **/

    int                 output_size;    /* I: Size of output buffer in bytes
                                              (see buffer) */
};

/* Request status values. */

#define JPEG_SUCCESS    0               /* Request completed. */
#define JPEG_BUSY       1               /* Request in progress. */
#define JPEG_BAD_FIT    2               /* Actual size differs from request. */
#define JPEG_SHORT      3               /* Incomplete image. */
#define JPEG_BAD_DATA   4               /* Invalid input. */
#define JPEG_BAD_PARAM  5               /* Invalid argument. */
#define JPEG_INTERNAL   6               /* Internal decoder problem. */

#define JPEG_LAST_RV    6

/* Decoders are loaded as a shared library containing an exported symbol,
 * JPEG_decoder, that refers to a statically-allocated instance
 * of this structure.
 */

struct JPEG_decoder {
    /* Capabilities - indicating what this decoder can do. */

    /* What input image formats the decoder can decode.
     * All decoders must support TRADITIONAL_JPEG.
     * Any decoder that supports other formats MUST set
     * DESIRE_INPUT_TYPE in addition to the supported
     * input formats.
     * If DESIRE_INPUT_TYPE is set, the format member of the
     * JPEG_request structure will have the appropriate
     * INPUT_* value set in it for each request made.
     */
      
    unsigned int input_formats;

#define TRADITIONAL_JPEG        1       /* Compatible with libjpeg.so.62 */
#define ARITHMETIC_ENCODING     2       /* JPEGs with arithmetic encoding. */
#define JPEG_XR                 4       /* ISO/IEC 29199-2 (2009) or HD_PHOTO */
#define DESIRE_INPUT_TYPE       0x8000  /* This decoder desires to know the
                                         * type of the input image.
                                         * VERSION_JPEGSDK_3 */
    /* Used in JPEG_request format field if DESIRE_INPUT_TYPE is set.
     * VERSION_JPEGSDK_3 */
#define INPUT_TRADITIONAL_JPEG          (TRADITIONAL_JPEG    <<16)
#define INPUT_ARITHMETIC_ENCODING       (ARITHMETIC_ENCODING <<16)
#define INPUT_JPEG_XR                   (JPEG_XR             <<16)

    /* The supported image output formats.
     * If the decoder can support cropping the output image
     */

    unsigned int output_formats;

#define PIXEL_XRGB              1       /* 32-bit, red (0xff0000),
                                         * green (0xff00), blue (0xff).
                                         */
#define PIXEL_XBGR              2       /* 32-bit, red (0xff), etc. */

#define CROP_OUTPUT_X_OFFSET    0x1000  /* This decoder supports cropped image
                                         * output with a non-zero x offset.
                                         * VERSION_JPEGSDK_2 */
#define CROP_OUTPUT_Y_OFFSET    0x2000  /* This decoder supports cropped image
                                         * output with a non-zero y offset.
                                         * VERSION_JPEGSDK_2 */

   /* Does the decode function block, or does it return while decoding
    * continues concurrently.  If so, how is completion reported?
    * The supported methods are: the caller polls each request struct and
    * those whose status is not JPEG_BUSY have completed;
    * a callback is made by another thread;
    * a signal is directed to the calling thread;
    * or an open file descriptor becomes ready for reading (but the caller
    * will not read it directly).
    *
    * Implementations should not do anything unusual
    * (such as creating a thread) to support asynchronous processing.
    * It is perfectly acceptable to support only synchronous completion.
    */

    unsigned int completion_handling;

#define BACKGROUND_DECODING     1       /* Caller continues during decoding. */
#define COMPLETION_CALLBACK     2       /* Callback in library's own thread. */
#define COMPLETION_SIGNAL       4       /* Signal sent on completion. */
#define COMPLETION_FD           8       /* Completion via select()/poll(). */
#define BATCH_DECODING          0x10    /* Does this decoder support batches
                                           of JPEGs? If so, multiple request
                                           structures are issued
                                           (see batch_decode() below).
                                           VERSION_JPEGSDK_2 */
#define INDIRECT_ONLY           0x20    /* Does this decoder require all JPEGs
                                           to be decoded to an intermediate
                                           buffer prior to being pushed to the
                                           LVB?  Otherwise direct decoding to
                                           the LVB is permitted and may occur
                                           based on the .INI parameters.
                                           VERSION_JPEGSDK_3 */

    /* Can the library decode more than one JPEG concurrently? */

    unsigned int concurrency;           /* Must be >= 1. */

    /* Can the library accept queued requests? */

    unsigned int queue_limit;           /* May be zero. */

    /* If the decoder is more efficient when the output buffer has specific
     * alignment in memory, the preferred alignment is set here.
     * A buffer address is preferred when there is no remainder on division
     * by the alignment value.
     * The library must return the data as requested in all cases, but the
     * caller may use this value to optimise buffer allocation.
     */

    unsigned int preferred_alignment;

    /* Entry points: request a decoding.
     * If the request is not complete when the function returns
     * (asynchronous operation) the "status" member must be set to JPEG_BUSY.
     * If JPEG_BAD_FIT is returned, request->width and request->height
     * should contain the correct values on return.
     * 
     * If implemented asynchronously, complete_request() will be called to
     * determine which requests have completed upon receipt of a signal or
     * other I/O readiness event (see complete_request() below).
     */

    void (*start_decode)(struct JPEG_request *request);

    /* Request the result of a previous request, blocking if necessary.
     * It is not needed if start_decode() is synchronous, but may be called,
     * so the function should simply return immediately if the request
     * is already marked as complete (status not JPEG_BUSY).
     * If JPEG_BAD_FIT is returned, request->width and request->height
     * should contain the correct values on return.
     * Not to be called when any form of asynchronous completion notification
     * was requested.
     */

    void (*finish_decode)(struct JPEG_request *request);

    /* If either of the signal or I/O readiness completion reporting
     * methods are in use, this function must be called to determine
     * which request has completed, or NULL if no request has completed
     * since the last call.
     */

    struct JPEG_request *(*complete_request)(void);

    /** VERSION_JPEGSDK_2 **/

    /* Synchronous batch decode: decode an array of JPEGs. If BATCH_DECODING
     * is specified as a completion_handling flag, this function must be
     * implemented. The number of items in the array will never exceed 
     * "JPEG_decoder.concurrency".
     */

    void (*batch_decode)(struct JPEG_request request[], int num_requests);
};

/* Decoders supporting version 3 of this SDK and later must export a symbol,
 * JPEG_decoder_version, that refers to a statically-allocated instance
 * of this structure.
 */

struct JPEG_decoder_version {
    /* The version number details which level or version of the header, SDK
     * and structures.
     * Receivers supporting version 3 of this SDK and later will read the
     * version_number field.
     * Receivers supporting version 2 and 1 of this SDK will not read the
     * version_number field.
     * NOTE:  A Receiver supporting version 1 of this SDK will not function
     *        correctly with a library supporting both version 2 of the SDK
     *        and the CROP_OUTPUT_{X,Y}_OFFSET flags.
     */
    unsigned int version_number;        /* Version of the header & structures */

#define VERSION_JPEGSDK_1 1     /* Base version of the SDK. */
#define VERSION_JPEGSDK_2 2     /* Adds support for cropping,
                                   batch & direct decoding */
#define VERSION_JPEGSDK_3 3     /* Adds support for specifying the input
                                   JPEG type, output buffer size, ability
                                   to disable direct decoding */

    /* A callback function to tell the library which version of this SDK
     * the Receiver will be using.
     * Receivers supporting version 3 of this SDK and later will call this
     * callback function with the appropriate VERSION_JPEGSDK_* value.
     * Receivers supporting version 2 and 1 of this SDK will not call this
     * callback.
     * This callback is called prior to any JPEG decoding occurring.
     * This callback is used for one-time initialization.
     */

    void (*supported_version)(unsigned int version_number);
};

#endif /* _JPEG_DECODE_H_ */
