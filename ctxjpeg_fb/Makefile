###############################################################################
#
#	Name:		Makefile
#
#	$Id: //LinuxBasedReceivers/13.5/ClientEngine/unix/CitrixPluginSDK/jpeg_sample/Makefile#1 $
#
#	Purpose:	Build a JPEG decoder plugin that wraps libjpeg.so.
#			This is mostly independent of the rest of the build
#			system, as it is meant to simulate an SDK user.
#
#	Copyright 1998-2014 Citrix Systems, Inc.  All rights reserved.
#			
###############################################################################

CFLAGS=-fpic -O3 -Wall -I. -fno-stack-protector

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $+

ctxjpeg_fb.so: libjpeg.o srcmgr.o
	$(LD) -o $@ -shared -E $+ -ljpeg

clean:
	rm -f *.o ctxjpeg_fb.so

clobber: clean
	rm -f *~
