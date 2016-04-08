/*****************************************************************************\

  pp.h - parallel port support for multi-point transport driver
 
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

#ifndef _PP_H
#define _PP_H

#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include "hpmud.h"
#include "hpmudi.h"

/*
 * PC-style parallel port bit definitions.
 *
 * Status
 *  bit
 *   7 - Busy * 
 *   6 - NAck
 *   5 - PError (PARPORT_STATUS_PAPEROUT)
 *   4 - Select  
 *
 *   3 - NFault (PARPORT_STATUS_ERROR)
 *   2 -
 *   1 -
 *   0 -
 *
 * Control
 *  bit
 *   7 - 
 *   6 - 
 *   5 - 
 *   4 -   
 *
 *   3 - Select *
 *   2 - Init
 *   1 - AutoFD *
 *   0 - Strobe *
 * 
 *              * inverted
 *
 * Notes:
 *   For ECP mode use low-level parport ioctl instead of high-level parport read/writes because its more reliable. High-level support
 *   for Compatible and Nibble modes are probably ok, but for consistency low-level parport ioctl is used.
 *
 */

#define PP_DEVICE_TIMEOUT 30000000   /* device timeout (us) */
//#define PP_SIGNAL_TIMEOUT 1000000   /* signal timeout (us), too long for 1ms timeout, DES 8/18/08  */
//#define PP_SIGNAL_TIMEOUT 1000   /* signal timeout (us), too short for DJ540, DES 8/18/08 */  
#define PP_SIGNAL_TIMEOUT 100000   /* signal timeout (us), DES 8/18/08 */
#define PP_SETUP_TIMEOUT 10   /* setup timeout (us) */

struct _mud_device;
struct _mud_channel;

extern struct _mud_device_vf __attribute__ ((visibility ("hidden"))) pp_mud_device_vf;

int __attribute__ ((visibility ("hidden"))) pp_write(int fd, const void *buf, int size, int usec);
int __attribute__ ((visibility ("hidden"))) pp_read(int fd, void *buf, int size, int usec);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_open(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_close(struct _mud_device *pd);                 
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_get_device_id(struct _mud_device *pd, char *buf, int size, int *len);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_get_device_status(struct _mud_device *pd, unsigned int *status);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_channel_open(struct _mud_device *pd, const char *sn, HPMUD_CHANNEL *cd);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_channel_close(struct _mud_device *pd, struct _mud_channel *pc);

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_raw_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_raw_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_mlc_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_mlc_channel_close(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_dot4_channel_open(struct _mud_channel *pc);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_dot4_channel_close(struct _mud_channel *pc);

int __attribute__ ((visibility ("hidden"))) pp_probe_devices(char *lst, int lst_size, int *cnt);

#endif // _PP_H

