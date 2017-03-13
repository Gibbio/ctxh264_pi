/*****************************************************************************
 *
 *   H264_decode.h
 *
 *   Citrix Receiver interface to H.264 decoders.
 *
 *   $Id: //icaclient/develop/main/UKH/NetClient/main/unix/inc/H264_decode.h#7 $
 *
 *   Copyright 2013-2017 Citrix Systems, Inc.  All Rights Reserved.
 *
 *****************************************************************************/

#ifndef _H264_DECODE_H_
#define _H264_DECODE_H_

/*  The purpose of this interface is to allow 3rd party H.264 decoders to be
 *  "plugged" into Citrix Receiver, taking advantage of hardware acceleration
 *  or other decoding optimisations. Receiver will receive H.264 encoded data
 *  on a per-frame basis and issue this to the decoder in parts. An ARGB frame
 *  buffer will be issued to the decoder immediately after the H.264 data. This
 *  will be used for drawing parts of the screen, such as text, in a "lossless"
 *  manner. The interface implementation is expected to merge this with the
 *  current H.264 frame (essentially, an overlay) such that no detail be lost
 *  from the ARGB frame buffer. Receiver will then instruct the interface to
 *  push the composed frame to screen, and the process repeats.
 *
 *  In essence, the sequence of API calls will usually be:
 *
 *  init()
 *  ...
 *  cxt = open_context()
 *  ...
 *
 *  repeat
 *
 *    start_frame( cxt )                        } - (+)see below
 *
 *    while ( more_h264_frame_data )            }
 *                                              }
 *      decode_frame( cxt, h264_frame_chunk )   } - (*)see below
 *                                              }
 *    end                                       }
 *
 *    compose_with_fb( cxt, ARGB_buffer ) / compose_with_rects( cxt, rects )
 *
 *    push_frame( cxt )
 *
 *  end
 *
 *  ...
 *  close_context( cxt )
 *
 *  The interface is free to decide how the ARGB merge should be done i.e.,
 *  whether to do this in hardware or software with the latter probably
 *  requiring a YUV->RGB H.264 frame buffer conversion at some point. This
 *  overlay should not be used for the construction of subsequent H.264 frames.
 *  Only the data that was previously H.264 decoded should be used for future
 *  H.264 frames.
 *
 *  Support for more than one H.264 decoding context may be required for cases
 *  where multiple monitors are in use.
 *
 *  (+)It is possible for Receiver to issue a start_frame() during the
 *  processing of an existing frame. Receiver will always set the encoded size
 *  to zero. This may happen as a result of an expose event. In this case,
 *  the plug-in should simply push the last composed frame.
 * 
 *  (*)Note that a frame may not actually contain any H.264 frame data, with
 *  only lossless areas being updated. If this is the case, the lossless
 *  composition step should be performed with the last composed frame.
 *
 *  SMALL FRAME SUPPORT
 *  -------------------
 *
 *  The server might decide that encoding an entire screen's worth of data for
 *  small changes is ineffficient, and therefore send the frame as a collection
 *  of small lossless images. These are known as "small frames" and can be
 *  optionally supported by the decoder. The decoder can indicate support by
 *  setting the H264_OPTION_SMALL_FRAME_SUPPORT flag.
 *
 *  Small frame objects (images or solid fill commands) will either be
 *  pre-composed onto the ARGB frame buffer, or issued via the
 *  compose_with_rects() call, depending on whether the option
 *  "H264_OPTION_PREFER_TEXT_RECTS" is set. If this option is set, the decoder
 *  must retain the small frame objects until the next H.264 frame, where
 *  all small frame objects can be purged.
 */


/*  V2 "Converged Mode" support
 *
 *  The server can now isolate and instantiate a H.264 context for arbitrary
 *  sized video regions on screen. The rest of the screen will typically be
 *  composed of images and solid fills. To support this mode of operation,
 *  the interface should supply all of the V2 functions as described below 
 *  and indicate a H264_decoder structure version of "2.0".
 *
 *  The main difference between V1 and V2 is the addition of "canvasses". A 
 *  canvas should represent the contents of a monitor or a window, and it is 
 *  possible that several canvasses will be created. The decoder is able to 
 *  indicate the maximum number of canvasses supported by setting the 
 *  "max_canvasses" field. All other H.264 decoder fields remain unchanged
 *  and should be filled.
 *
 *  Receiver will use the supplied V2 canvas functions to update and draw into
 *  each canvas, and the decoder is expected to take responsibility for
 *  ensuring the canvas is shown on screen as and when instructed by Receiver.
 * 
 *  All drawing operations (image, solid fills, area copies) to canvasses will
 *  be relative to the canvas origin (top-left (0,0)) and never exceed the
 *  canvas bounds.
 *
 *  H.264 contexts will be created with the new "create_h264_context" function
 *  and be managed using the V1 functions. At the end of a frame, a number of 
 *  push_canvas() calls will be made by Receiver (depending on the number of 
 *  monitors in the session) upon which the H.264 decoded output must be ready
 *  for display, along with any other image operations that comprise the
 *  frame.
 *
 *  To aid understanding, the sequence of calls will typically look as follows,
 *  assuming a 2 monitor 1280x1024 layout and NO H.264 activity.
 *
 *
 *  SESSION START
 *
 *  init()
 *  ...
 *
 *  CANV_context sc1 = create_canvas(1280, 1024, 0, 0);     // Left monitor
 *  CANV_context sc2 = create_canvas(1280, 1024, 1280, 0);  // Right monitor
 *
 *  ...
 *
 *  repeat
 *
 *    sequence of copy_image(), fill_rect(), etc ... for canvas 1
 *
 *    sequence of copy_image(), fill_rect(), etc ... for canvas 2
 *
 *    push_canvas(sc1)   // push only called if there were updates for canvas 1
 *
 *    push_canvas(sc2)   // push only called if there were updates for canvas 2
 *
 *  end
 *
 *  If the server wishes to use H.264 for a video region, the sequence of calls
 *  will look like the above except that a call to create a H.264 context will
 *  be made when the video region is initialized. H.264 contexts may be fairly
 *  short lived, ending when the user closes or moves the affected region.
 *  H.264 contexts cannot be moved; the server will close and open a new
 *  context if a video region has moved. Any H.264 context data for a canvas
 *  will always preceed other image operations. Commands for different
 *  canvasses will not be interleaved.
 *
 *  // Inside repeat loop, video region is instantiated
 *  // Server instructs Receiver to create a H.264 context at (200, 200) on
 *  // canvas 1 of width 400 and height 200 pixels.
 * 
 *      hc1 = create_h264_context(sc1, {200,200,600,400})
 *  
 *      // Receiver supplies H.264 encoded data
 *      start_frame(hc1)
 *
 *      sequence of decode_frame(hc1), ...
 *
 *      // Receiver performs other image operations
 *
 *      sequence of copy_image(), fill_rect(), etc ... for canvas 1
 *
 *      // Frame complete, push canvas to screen
 *
 *      push_canvas(sc1) // Pushes all decoded output/draws to screen
 *
 *  // repeat...
 *
 *  close_context(hc1) // Done with H.264
 *
 *  // sc1 may still be used for non-H.264 draws until session end
 */


/*  Interface versioning.
 *
 *  Specifying a version of "1.0" indicates support for H.264 + lossless text
 *  feature found in XA/XD server versions 7.0 and later, and also 3D Pro found
 *  in server versions 4.0 and later.
 *
 *  Specifying a version of "2.0" indicates support for the modes as documented
 *  for "1.0" above as well as support for canvasses. It is recommended that
 *  this level of support is implemented to give the best possible experience.
 *
 *  2.1: Add show, hide cursor commands.
 *
 *       Added option H264_OPTION_FORCE_FULL_RENEG which allows the plug-in
 *       to request that a full Thinwire renegotiation happens on a server
 *       initiated mode change. This flag does not normally need to be set.
 */

#define VERSION_MAJOR  2
#define VERSION_MINOR  1

/*  Pixel formats for the (lossless) frame buffer or lossless objects that are
 *  passed to the decoder for composition. The plug-in can express a preference
 *  as to which format Receiver should supply when image operations need
 *  to be performed.
 */

typedef enum _LLPixelFormat
{
    PIXEL_FORMAT_ARGB = 0x00,
    PIXEL_FORMAT_BGRA = 0x01
} LLPixelFormat;

/* gcc allows bool as a keyword in C. */
#ifndef __cplusplus

#ifndef bool
#define	bool	unsigned char
#endif /* bool */

#endif /* __cplusplus */


/* A structure to describe some image data, be it a frame buffer, individual
 * lossless text rectangles, small frame images or solid fills.
 *
 * In the latter 3 cases, Receiver will choose 1 of 4 possible operations:
 *
 *  IMAGE_OP_DRAW_LOSSLESS: draw supplied lossless text at specified
 *  coordinates, using specified width and height. A pointer to the image data
 *  will be given via the "bits" parameter. The destination coordinates, width
 *  and height can be used to index the text rectangle, for example, in a
 *  database-type system.
 *
 *  IMAGE_OP_DELETE_LOSSLESS: delete lossless text at specified coordinates,
 *  using specified width and height. Note that "delete" does not necessarily
 *  require data to be fully removed, for example, this operation may be
 *  implemented as an "alpha-zerorer" i.e., set all alpha values in the
 *  destination rectangle to zero. Alternatively, the destination coordinates,
 *  width and height can be used to locate the text rectangle, for example,
 *  in a database-type system.
 *
 *  IMAGE_OP_SMALL_FRAME_BITMAP: draw supplied image at specified coordinates,
 *  using specified with and height. A pointer to the image data will be given
 *  via the "bits" parameter. The destination coordinates, width and height can
 *  be used to index the text rectangle, for example, in a database-type system.
 *  This operation is almost identical to IMAGE_OP_DRAW_LOSSLESS, the only
 *  difference being that it will never be "undone" by a corresponding delete.
 *
 *  IMAGE_OP_SMALL_FRAME_SOLID_FILL: draw a solid rectangle of colour specified
 *  by "col", using the specified destination coordinates, width and height.
 */

typedef enum _ImageOp
{
    IMAGE_OP_DRAW_LOSSLESS              = 0,
    IMAGE_OP_DELETE_LOSSLESS            = 1,
    IMAGE_OP_SMALL_FRAME_BITMAP         = 2,
    IMAGE_OP_SMALL_FRAME_SOLID_FILL     = 3
} ImageOp;

struct image_buf
{
    unsigned int cb_size;       /* Size of this structure. */

    void *mem;                  /* Pointer to allocated memory. */

    void *bits;                 /* Pointer to start of image bits. */

    unsigned char pixel_format; /* Pixel format (see above). */

    unsigned char lossless_op;  /* Lossless operation (see above). */

    int stride;                 /* Stride (in bytes). Not necessarily width *
                                 * sizeof(pixel) if a specified alignment is
                                 * required.
                                 */

    unsigned int width;         /* Image width (pixels). */

    unsigned int height;        /* Image height (pixels). */

    int dst_x, dst_y;           /* The x and y coordinates of the upper-left
                                 * corner of the destination rectangle if
                                 * this image is being transferred onto a
                                 * canvas.
                                 */

    int src_x, src_y;           /* The x and y coordinates of the upper-left
                                 * corner of the source rectangle if this
                                 * operation is a copy from source.
                                 */

    unsigned int col;           /* RGB colour for solid fills
                                 * (IMAGE_OP_SMALL_FRAME_SOLID_FILL)
                                 */
};

/*  A structure to provide information about windows to the interface. This
 *  is required for seamless and full-screen/windowed sessions and enables the
 *  decoder interface to update those windows with the correct areas of the
 *  composed H.264 frame.
 */

typedef enum _WindowInfoFlags
{
    /* The window ID being passed in is a seamless window. */
    WINDOW_INFO_FLAG_SEAMLESS   = 0x00000001,
    /* Repaint the entire window, not just the dirty region. */
    WINDOW_INFO_FLAG_REPAINT    = 0x00000002
} WindowInfoFlags;

struct window_info
{
    unsigned int cb_size;   /* Size of this structure. */

    unsigned int id;        /* Window manager specific ID (or handle). */

    SIGNED_RECT rect;       /* Window rect. This is used to determine where to
                             * draw the image from the composed H.264 frame
                             * as the source rectangle may differ from the
                             * window's position and size.
                             */

    int         target_x;   /* Target x-offset within context. */

    int         target_y;   /* Target y-offset within context. */

    unsigned int flags;     /* See above. */
};


typedef unsigned int H264_context;
#define H264_INVALID_CONTEXT            0

typedef unsigned int CANV_context;
#define CANV_INVALID_CONTEXT            0

/* If H264_OPTION_PREFER_TEXT_RECTS is specified by the interface, Receiver
 * will issue a list of lossless text rectangle data instead of an ARGB
 * frame buffer, for composition with the H.264 frame. Also, if small frame
 * support is enabled, and H264_OPTION_PREFER_TEXT_RECTS is specified, 
 * Receiver will issue the individual small frame images, similar to how
 * text rectangles are issued. Otherwise, small frame images will be
 * composed onto the same ARGB frame buffer as the lossless text rectangles.
 */

typedef enum _H264Option
{
	H264_OPTION_LOSSLESS			= 0x00000001,
	H264_OPTION_WINDOW_SUPPORT		= 0x00000002,
	H264_OPTION_PREFER_TEXT_RECTS	= 0x00000004,
    H264_OPTION_SMALL_FRAME_SUPPORT = 0x00000008,
    H264_OPTION_FORCE_FULL_RENEG    = 0x00000010
} H264Option;

typedef enum _ChromaFormat
{
	H264_CHROMA_FORMAT_400	= 0x00000001, /* Monochrome. */
	H264_CHROMA_FORMAT_420	= 0x00000002,
	H264_CHROMA_FORMAT_422  = 0x00000004,
	H264_CHROMA_FORMAT_444  = 0x00000008
} ChromaFormat;

/*  Decoders are loaded as a shared library containing an exported symbol,
 *  H264_decoder, that refers to a statically-allocated instance
 *  of this structure.
 */

struct H264_decoder
{
    /*  Must be set to VERSION_MAJOR (see above). */
    unsigned int ver_major;

    /*  Must be set to VERSION_MINOR (see above). */
    unsigned int ver_minor;

	/*  The maximum number of contexts the decoder supports. Specify "0"
     *  to indicate no limit.
     */
	int max_contexts;

    /*  The maximum width and height (in pixels) supported by a decoding
     *  context.
     */
	unsigned int width, height;

    /*  The maximum frame rate supported by a decoding context. */
	int max_fps;

    /*  Decoder specific options (see above). Used during negotiation
     *  of capabilities with the server.
     */
	unsigned int options;

    /*  Supported chroma formats (see above). Again, might be used during
     *  capability negotiation with the server.
     */
    unsigned int chroma_formats;

    /*  Preferred alpha value for lossless objects. Usually set to 0 or 255.
        For example, for a pixel format of ARGB, and a preferred alpha value
        of 255 (0xFF), the value is set per pixel as follows: FF RR GG BB.
        And for a pixel format of BGRA: BB GG RR FF.
    */
    unsigned char pref_lossless_alpha_val;

    /*  Preferred pixel format for lossless objects (not guaranteed). Should
     *  be set to a PixelFormat value (enum above).
     */
    unsigned char pref_lossless_pixel_fmt;

    /*  Function: init()
     *
     *  Perform any decoder specific initialisation. Called immediately after
     *  the deocding interface is loaded. Note that it is permitted for init()
     *  to switch implementations of the below functions at runtime if desired.
     *  For example, decode_frame might be implemented as:
     *
     *  bool decode_frame(...)
     *  {
     *      return (*pfn_decode_frame)(...);
     *  }
     *
     *  where "pfn_decode_frame()" is a pointer to a function which might vary
     *  based on some checks performed when init() is called.
     *
     *  Returns:
     *       true:              success, decoder will be used for H.264.
     *
     *       false:             failure, decoder cannot be used.
     */

    bool (*init)();


    /*  Function: open_context()
     *
     *  Create a new H.264 decoding context. The context returned should be
     *  used in subsequent calls. The "options" parameter is used to request
     *  particular features of the decoder that might be required throughout
     *  the lifetime of this context.
     *
     *  Input:
     *       width, height:     requested width and height (in pixels) of the
     *                          new context.
     *
     *       codec_data:        pointer to some H.264 specific codec data.
     *
     *       len:               length (in bytes) of codec data.
     *
     *       options:           flags indicating support for decoder features
     *                          (e.g., windowed drawing - see above).
     *
     *  Returns:
     *       > 0 (success):     a valid H264_context.
     *
     *       H264_INVALID_CONTEXT (error).
     */

    H264_context (*open_context)(int width, int height, void *codec_data,
                                 int len, unsigned int options);

    /*  Function: start_frame()
     *
     *  Initializes the decoding interface to begin decoding a new frame. The
     *  total encoded data size of the frame must be supplied to ensure that
     *  an entire H.264 encoded frame can be assembled from it's constituent
     *  parts. This is especially useful if the decoder does not support
     *  partial decoding.
     *
     *  The function may also be called with a number of "dirty" rectangles.
     *  These rectangles define which areas of the composed frame have actually
     *  changed, so that the implementation may take advantage of updating only
     *  those areas on screen, avoiding the need for a full screen update on
     *  each frame. If no rectangles are specified, then the implementation
     *  must assume that the whole context (monitor) has changed.
     *
     *  An encoded size of 0 means that the frame is a just a text frame or
     *  small frame, or Receiver wishes to redraw the last frame, the latter
     *  of which can happen on window expose events.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     *
     *       encoded_size:      encoded data size of the whole H.264 frame.
     *
     *       dirty_rects:       array of dirty rectangles (see above).
     *
     *       num_rects:         number of dirty rectangles, or 0.
     *
     *  Returns:
     *       true:              success, ready to receive frame data.
     *
     *       false:             error, unable to start decoding frame.
     */

    bool (*start_frame)(H264_context cxt, unsigned int encoded_size,
                        SIGNED_RECT dirty_rects[], unsigned int num_rects);


    /*  Function: decode_frame()
     *
     *  Accepts partial H.264 encoded data and decodes an entire frame once
     *  all the data is available. This is indicated to the decoder when
     *  the boolean "last" is set to "true". The input "H264_data" is
     *  NOT guaranteed to be valid once this function returns.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     *
     *       H264_data:         partial/entire encoded H.264 frame data.
     *
     *       len:               length (in bytes) of H.264 frame data.
     *
     *       last:              boolean to indicate whether this frame data is
     *                          the last in a frame.
     *
     *  Returns:
     *       true:              H.264 data successfully consumed (last == false)
     *                          frame successfully decoded (last == true).
     *
     *       false:             invalid partial H.264 frame data (last == false)
     *                          unable to decode frame (last == true).
     */

    bool (*decode_frame)(H264_context cxt, void *H264_data, int len, bool last);


    /*  Function: compose_with_fb()
     *
     *  Instructs the decoding interface to compose a frame buffer with the
     *  current H.264 frame. The frame buffer is described by the various fields
     *  in a "image_buf" structure (see above).
     *
     *  A number of "interesting" rectangles may be passed to the function as
     *  a hint to the affected areas.
     *
     *  Any pixels in the supplied frame buffer that do not need to be
     *  displayed should be marked transparent, i.e., have an alpha value of
     *  0.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     *
     *       fb:                frame buffer to compose with H.264 frame.
     *
     *       interesting_rects: an array of interesting rectangles indicating
     *                          affected areas.
     *
     *       num_rects:         number of rectangles, or 0 if the entire frame
     *                          buffer should be inspected.
     *
     *  Returns:
     *       true:              success.
     *
     *       false:             error.
     */

    bool (*compose_with_fb)(H264_context cxt, struct image_buf *fb,
                            SIGNED_RECT interesting_rects[], unsigned int num_rects);


    /*  Function: compose_with_rects()
     *
     *  Instructs the decoding interface to compose a list of text rectangles
     *  or small frame images with the current H.264 frame. Each object, including 
     *  it's destination location, is described by an "image_buf" structure.
     *
     *  This function may be called multiple times if Receiver is unable to
     *  issue all the rectangles in a single call. The terminating call will be
     *  indicated by setting the "last" parameter to TRUE.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     *
     *       text_rects:        list of objects to compose.
     *
     *       num_rects:         number of rectangles, or 0 for none.
     *
     *       last:              boolean to indicate whether this is the last
     *                          batch of rectangles to compose for the frame.
     *
     *  Returns:
     *       true:              success.
     *
     *       false:             error.
     */

    bool (*compose_with_rects)(H264_context cxt, struct image_buf objects[],
                               unsigned int num_objects, bool last);


    /*  Function: push_frame()
     *
     *  Instructs the interface to push the composed H.264 frame to the
     *  supplied window(s), as defined by a "window_info" structure. This
     *  function may block or return immediately, depending on the value of
     *  the "wait" parameter.
     *
     *  In the case of a full screen or windowed session, a single window
     *  should be supplied so that the composed frame can be drawn into it.
     *
     *  In the case of a seamless sessions, an array of windows should be
     *  provided so that each window can be updated with the corresponding
     *  area in the composed frame.
     *
     *  If no window is supplied, the decoding interface is free to choose
     *  how the composed frame should be displayed on screen, if at all.
     *
     *  If "wait" is "true", this function must not return until the frame
     *  has been displayed. If "wait" is "false", the supplied (but optional)
     *  boolean will be set to "true" once the frame is pushed to screen.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     *
     *       windows:           an array of window_info structures (see above).
     *
     *       num_windows:       number of window_info structures, or 0.
     *
     *       wait:              boolean to indicate whether the function should
     *                          return immediately or wait until the frame has
     *                          been displayed.
     *
     *       pushed:            a pointer to a boolean which will be set to
     *                          "true" once frame has been pushed to screen.
     *                          This parameter may be NULL.
     *
     *  Returns:
     *       true:              success, frame displayed.
     *
     *       false:             error, couldn't display frame.
     */

    bool (*push_frame)(H264_context cxt, struct window_info windows[],
                       unsigned int num_windows, bool wait, bool *pushed);


    /*  Function: close_context()
     *
     *  Tears down a previously opened H.264 decoding context. Any resources
     *  associated with the context should be released.
     *
     *  Input:
     *       cxt:               handle to a valid H.264 context.
     */

    void (*close_context)(H264_context cxt);


    /*  Function: end()
     *
     *  Perform any decoder specific de-initialisation. Called after all
     *  contexts are closed and client wishes to stop decoding any further,
     *  data from interface perhaps due to user wishing to terminate the
     *  program.
     */

    void (*end)();

    /************************ V2 CONVERGED MODE SUPPORT **********************/

    /*  The maximum number of canvasses the decoder supports. Specify "0"
     *  to indicate no limit.
     */
    int max_canvasses;
    
  
    /*  Function: create_canvas()
     *
     *  Create a new canvas for screen updates. Typically, there will be one
     *  canvas create request per monitor. The decoder plug-in should allocate
     *  any underlying memory structures (e.g., a frame buffer) in accordance
     *  with the width, height and pixel format specified.
     *
     *  Also supplied is the position of this canvas relative to the canvas
     *  that has a top-left offset of {0, 0}.
     *
     *  It is not necessary for the decoder to initialize any H.264 contexts at
     *  this point.
     *
     *  The returned context will be supplied for all future canvas operations.
     *
     *  Input:
     *       width, height:     requested width and height (in pixels) of the
     *                          new context.
     *
     *       pix_format:        bit depth.
     *
     *       x_off, y_off:      offset of top-left of canvas from display
     *                          origin.
     *
     *  Returns:
     *       > 0 (success):     a valid CANV_context.
     *
     *       CANV_INVALID_CONTEXT (error).
     */

    CANV_context (*create_canvas)(int width, int height, 
                                  LLPixelFormat pix_format, 
                                  int x_off, int y_off);


    /*  Function: create_h264_context()
     *
     *  Create a new H.264 decoding contex on the supplied canvas. The H.264
     *  context dimensions and position are defined by the input rectangle
     *  "dest", and will be relative to the canvas origin (top-left, (0,0)).
     *
     *  The V1 interface functions "start_frame", "decode_frame" and
     *  "close_context" will be used to manage the H.264 context.
     *
     *  All H.264 decoded output must be ready for display when push_canvas()
     *  is called.
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     *
     *       dest:              destination rectangle of H.264 context from
     *                          which width, height and offset can be derived.
     *
     *       fmt:               chroma format for H.264 context.
     *
     *  Returns:
     *       > 0 (success):     a valid H264_context.
     *
     *       H264_INVALID_CONTEXT (error).
     */

    H264_context (*create_h264_context)(CANV_context cxt, SIGNED_RECT dest, 
                                        ChromaFormat fmt);
    

    /*  Function: get_pointer_for_image()  [OPTIONAL]
     *
     *  Returns a pointer to a buffer that the Receiver engine will fill with
     *  image data. The intention is to minimize copying of image data by
     *  allowing the plug-in to specify the buffer. The returned value points
     *  to the location that will be used for the top-left hand pixel.
     *
     *  The buffer will guaranteed to be filled by the next draw operation.
     *
     *  If this function is supplied (i.e., non-NULL), Receiver may attempt to
     *  use this to optimize image drawing, in preference to copy_image()
     *  below.
     *
     *  In addition to the returned pointer, the function should also return a
     *  stride to ensure the image is decoded correctly (in bytes).
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     *
     *       dest:              destination rectangle for the image.
     *
     *       stride:            pointer to an integer to receive canvas stride
     *                          (in bytes).
     *
     *  Returns:
     *       non-NULL:          valid pointer (along with stride).
     *
     *       NULL (error).
     */

    void *(*get_pointer_for_image)(CANV_context cxt, SIGNED_RECT dest, 
                                    int *stride);


    /*  Function: copy_image()
     *
     *  Called when the decoder is requested to copy the supplied image
     *  "source" to the supplied destination rectangle "dest". 
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     *
     *       dest:              destination rectangle for the image.
     *
     *       source:            a structure describing the input image.
     *
     *  Returns:
     *       true:              copy success.
     *
     *       false:             error.
     */

    bool (*copy_image)(CANV_context cxt, SIGNED_RECT dest, 
                        struct image_buf *source);
    

    /*  Function: copy_rect()
     *
     *  Copy the canvas source rectangle to the destination rectangle. The
     *  rectangles may overlap, in which the case the copy should be regarded
     *  as if an intermediate buffer was being used.
     *
     *  The source and destination rectangles will never overlap a H.264
     *  context.
     *
     *  Input:
     *       dest_cxt:          handle to a valid canvas context.
     *
     *       dest:              destination rectangle.
     *
     *       src_cxt:           handle to a valid canvas context.
     *
     *       source:            source rectangle.
     *
     *  Returns:
     *       true:              copy success.
     *
     *       false:             error.
     */

    bool (*copy_rect)(CANV_context dest_cxt, SIGNED_RECT dest, 
                      CANV_context src_cxt, SIGNED_RECT source);


    /*  Function: fill_rect()
     *
     *  Fill the canvas destination rectangle with solid colour.
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     *
     *       dest:              destination rectangle.
     *
     *       rgb:               24-bit RGB colour (e.g., 0xRRGGBB).
     *
     *  Returns:
     *       true:              fill success.
     *
     *       false:             error.
     */

    bool (*fill_rect)(CANV_context cxt, SIGNED_RECT rect, unsigned int rgb);


    /*  Function: push_canvas()
     *
     *  Instructs the decoding interface to push the canvas to the
     *  supplied window(s), as defined by a "window_info" structure.
     *
     *  In the case of a full screen or windowed session, a single window
     *  should be supplied so that the composed frame can be drawn into it.
     *
     *  In the case of a seamless sessions, an array of windows should be
     *  provided so that each window can be updated with the corresponding
     *  area in the composed frame.
     *
     *  If no window is supplied, the decoding interface is free to choose
     *  how the composed frame should be displayed on screen, if at all.
     *
     *  The plug-in may choose to wait until the frame has been pushed
     *  to screen, or return immediately. In the latter case, care must be
     *  taken to ensure that the context is in a writeable state as Receiver
     *  will not wait before proceeding with any further draw operations.
     *
     *  Any H.264 contexts that intersect this canvas should also be in
     *  a "pushable" state.
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     *
     *       windows:           an array of window_info structures (see above).
     *
     *       num_windows:       number of window_info structures, or 0.
     *
     *  Returns:
     *       true:              success, frame pushed to screen.
     *
     *       false:             error, couldn't push frame.
     */

    bool (*push_canvas)(CANV_context cxt, struct window_info windows[],
                         unsigned int num_windows);


    /*  Function: destroy_canvas()
     *
     *  Destroys a previously created canvas. Any resources associated with the
     *  canvas should be freed.
     *
     *  Input:
     *       cxt:               handle to a valid canvas context.
     */

    void (*destroy_canvas)(CANV_context cxt);

    /* 2.1 support. */
    
    /*  Function: show_cursor()
     *
     *  Instructs the plug-in to show the mouse cursor.
     *  
     *  This is an optional function and does not need to be implemented if the
     *  plug-in does not draw it's own cursor.
     *
     */

    void (*show_cursor)();

    /*  Function: hide_cursor()
     *
     *  Instructs the plug-in to hide the mouse cursor.
     *
     *  This is an optional function and does not need to be implemented if the
     *  plug-in does not draw it's own cursor.
     *
     */

    void (*hide_cursor)();

};

#endif /* _H264_DECODE_H_ */
