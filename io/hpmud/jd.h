/*****************************************************************************\

  jd.h - JetDirect support for multi-point transport driver
 
  (c) 2004-2007 Copyright HP Development Company, LP

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

\*****************************************************************************/

#ifndef _JD_H
#define _JD_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "hpmud.h"
#include "hpmudi.h"

struct _mud_device;
struct _mud_channel;

extern const char __attribute__ ((visibility ("hidden"))) *kStatusOID;            /* device id snmp oid */

extern struct _mud_device_vf __attribute__ ((visibility ("hidden"))) jd_mud_device_vf;

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_open(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_close(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_get_device_id(struct _mud_device *pd, char *buf, int size, int *len);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_get_device_status(struct _mud_device *pd, unsigned int *status);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_open(struct _mud_device *pd, const char *sn, HPMUD_CHANNEL *cd);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_close(struct _mud_device *pd, struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_write(struct _mud_device *pd, struct _mud_channel *pc, const void *buf, int length, int timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_read(struct _mud_device *pd, struct _mud_channel *pc, void *buf, int length, int timeout, int *bytes_read);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_write(struct _mud_channel *pc, const void *buf, int length, int timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_read(struct _mud_channel *pc, void *buf, int length, int timeout, int *bytes_wrote);

#endif // _JD_H

