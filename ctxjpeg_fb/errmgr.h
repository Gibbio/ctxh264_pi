/***************************************************************************
 *
 * errmgr.h
 *
 * Header file for JPEGLIB alternate error handler.  The standard JPEGLIB error
 * handler performs an exit() call when a fatal error occurs, which is clearly
 * unacceptable.  This implementation of the JPEGLIB error handler function
 * uses a setjmp / longjmp combination to return to the caller outside LIBJPEG
 * without unwinding the stack back through libJPEG.  C++ implementation can do
 * the same using SEH.
 *
 * $Id: //LinuxBasedReceivers/13.5/ClientEngine/base/pal/ctxgraph/errmgr.h#1 $
 *
 * Copyright 2001-2004 Citrix Systems, Inc.  All Rights Reserved.
 *
 *****************************************************************************/

#ifndef ERRMGR_H
#define ERRMGR_H

/*****************************************************************************
 * CTXS_JPEG_ERROR_MANAGER
 *
 * Structure which overrides normal libJPEG error manager
 *
 * Members
 *
 * OriginalErrorManager: Original libJPEG error manager
 *
 * SetJumpStackState: Stack state for longjmp command.  Before the libJPEG
 * functions are called, the caller should call setjmp to save the stack state
 * into this error manager, if setjmp returns 0, start to execute the libJPEG
 * command, otherwise, assume that the libJPEG command failed.
 *****************************************************************************/
  

typedef struct tagCTXS_JPEG_ERROR_MANAGER
{
    struct jpeg_error_mgr   OriginalErrorManager;
    jmp_buf                 SetJumpStackState;
} CTXS_JPEG_ERROR_MANAGER, *PCTXS_JPEG_ERROR_MANAGER;

void
OverwriteDefaultErrorHandlers(struct jpeg_error_mgr * jerr);

#endif /* ERRMGR_H */
