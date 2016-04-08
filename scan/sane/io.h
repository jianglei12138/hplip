/************************************************************************************\

  io.h - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2006 Copyright HP Development Company, LP

  Permission is hereby granted, free of charge, to any person obtaining a copy 
  of this software and associated documentation files (the "Software"), to deal 
  in the Software without restriction, including without limitation the rights 
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
  of the Software, and to permit persons to whom the Software is furnished to do 
  so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Contributing Author: Don Welch, David Suffield, Naga Samrat Chowdary Narla,
						Sarbeswar Meher
\************************************************************************************/

#if !defined(_IO_H)
#define _IO_H

#include "sane.h"
#include "hpmud.h"

int __attribute__ ((visibility ("hidden"))) InitDbus(void);
int __attribute__ ((visibility ("hidden"))) SendScanEvent(char * device_uri, int event);
int __attribute__ ((visibility ("hidden"))) ReadChannelEx(int deviceid, int channelid, unsigned char * buffer, int length, int timeout);

#define EVENT_START_SCAN_JOB 2000
#define EVENT_END_SCAN_JOB 2001
#define EVENT_SCANNER_FAIL 2002
#define EVENT_PLUGIN_FAIL 2003
#define EVENT_SCAN_ADF_LOADED 2004
#define EVENT_SCAN_TO_DESTINATION_NOTSET = 2005
#define EVENT_SCAN_WAITING_FOR_PC = 2006
#define EVENT_SCAN_ADF_JAM 2007
#define EVENT_SCAN_ADF_DOOR_OPEN 2008
#define EVENT_SCAN_CANCEL 2009
#define EVENT_SIZE_WARNING 2010
#define EVENT_SCAN_ADF_NO_DOCS 2011
#define EVENT_SCAN_ADF_MISPICK 2012
#define EVENT_SCAN_BUSY 2013
#define EVENT_ERROR_NO_PROBED_DEVICES_FOUND  5018

#define DBUS_INTERFACE "com.hplip.StatusService"
#define DBUS_PATH "/"

#endif


