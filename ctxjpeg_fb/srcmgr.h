/*****************************************************************************
 * srcmgr.h
 *
 * Include file for source manager
 *
 * $Id: //LinuxBasedReceivers/13.5/ClientEngine/base/pal/ctxgraph/srcmgr.h#1 $
 *
 * Copyright 2001-2011 Citrix Systems, Inc.  All Rights Reserved.
 *
 *****************************************************************************/

#ifndef SRCMGR_H
#define SRCMGR_H

#ifdef __cplusplus
extern "C"
{
#endif

void
srcmgr_jpeg_memory_src(
    j_decompress_ptr    cinfo,
    unsigned char      *pJpegDataBuffer,
    unsigned long       jpegDataBufferSize);

#ifdef __cplusplus
}
#endif

#endif /* SRCMGR_H */
