/*****************************************************************************\

  musb.h - USB support for multi-point transport driver
 
  (c) 2010-2014 Copyright HP Development Company, LP

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

  Author: Naga Samrat Chowdary Narla, Sarbeswar Meher
\*****************************************************************************/

#ifndef _MUSB_H
#define _MUSB_H

#ifdef HAVE_LIBUSB01
#include <usb.h>
#else
#include <libusb.h>
#endif

#include "hpmud.h"
#include "hpmudi.h"

#define LIBUSB_TIMEOUT 30000              /* milliseconds */
#define LIBUSB_CONTROL_REQ_TIMEOUT 5000

enum FD_ID
{
   FD_NA=0,
   FD_7_1_2,         /* bi-di interface */
   FD_7_1_3,         /* 1284.4 interface */
   FD_7_1_4,         /* IPP interface */
   FD_ff_1_1,        /* HP EWS interface */
   FD_ff_2_1,        /* HP Soap Scan interface */
   FD_ff_3_1,        /* HP Soap Fax interface */
   FD_ff_ff_ff,        /* HP dot4 interface */
   FD_ff_d4_0,        /* HP dot4 interface */
   FD_ff_4_1,        /* orblite scan / rest scan interface */
   FD_ff_1_0,        /* Marvell fax support*/
   FD_ff_cc_0,
   FD_ff_2_10,
   FD_ff_9_1,
   MAX_FD
};

enum BRIGE_REG_ID
{
   ECRR=2,
   CCTR=3,
   ATAA=8
};

/* USB file descriptor, one for each USB protocol. */
typedef struct
{
#ifdef HAVE_LIBUSB01
   usb_dev_handle *hd;
#else
   libusb_device_handle *hd;
#endif
   
   enum FD_ID fd;
   int config;
   int interface;
   int alt_setting;

   /* Write thread definitions. */
   int write_active;             /* 0=no, 1=yes */
   const void *write_buf;
   int write_size;
   int write_return;             /* return value, normally number bytes written */
   pthread_t tid;
   pthread_mutex_t mutex;
   pthread_cond_t write_done_cond;

   unsigned char ubuf[HPMUD_BUFFER_SIZE];           /* usb read packet buffer */     
   int uindex;
   int ucnt;             
} file_descriptor;

struct _mud_device;
struct _mud_channel;

extern struct _mud_device_vf __attribute__ ((visibility ("hidden"))) musb_mud_device_vf;

int __attribute__ ((visibility ("hidden"))) musb_write(int fd, const void *buf, int size, int usec_timout);    
int __attribute__ ((visibility ("hidden"))) musb_read(int fd, void *buf, int size, int usec_timout); 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_open(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_close(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_get_device_id(struct _mud_device *pd, char *buf, int size, int *len);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_get_device_status(struct _mud_device *pd, unsigned int *status);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_open(struct _mud_device *pd, const char *sn, HPMUD_CHANNEL *cd);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_close(struct _mud_device *pd, struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_write(struct _mud_device *pd, struct _mud_channel *pc, const void *buf, int length, int timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_read(struct _mud_device *pd, struct _mud_channel *pc, void *buf, int length, int timeout, int *bytes_read);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_write(struct _mud_channel *pc, const void *buf, int length, int timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_read(struct _mud_channel *pc, void *buf, int length, int timeout, int *bytes_wrote);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_comp_channel_open(struct _mud_channel *pc);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_write(struct _mud_channel *pc, const void *buf, int length, int timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_read(struct _mud_channel *pc, void *buf, int length, int timeout, int *bytes_wrote);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_write(struct _mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_read(struct _mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read);

int __attribute__ ((visibility ("hidden"))) musb_probe_devices(char *lst, int lst_size, int *cnt, enum HPMUD_DEVICE_TYPE devtype);
int __attribute__ ((visibility ("hidden"))) power_up(struct _mud_device *pd, int fd);

#endif // _MUSB_H

