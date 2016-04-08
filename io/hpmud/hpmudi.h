/*****************************************************************************\

  hpmudi.h - internal definitions for multi-point transport driver

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

  Author: Naga Samrat Chowdary Narla,
\*****************************************************************************/

#ifndef _HPMUDI_H
#define _HPMUDI_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "hpmud.h"
#include "musb.h"
#include "mlc.h"
#include "dot4.h"
#include "pml.h"
#ifdef HAVE_LIBNETSNMP
#include "jd.h"
#include "mdns.h"
#endif
#ifdef HAVE_PPORT
#include "pp.h"
#endif

// DO NOT commit with HPMUD_DEBUG enabled :(
//#define HPMUD_DEBUG

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define BUG(args...) syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args)
//#define BUG(args...) fprintf(stderr, __FILE__ " " STRINGIZE(__LINE__) ": " args)

#ifdef HPMUD_DEBUG
   #define DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
// #define DBG(args...) fprintf(stderr, __FILE__ " " STRINGIZE(__LINE__) ": " args)
   #define DBG_DUMP(data, size) sysdump((data), (size))
   #define DBG_SZ(args...) syslog(LOG_INFO, args)
#else
   #define DBG(args...)
   #define DBG_DUMP(data, size)
   #define DBG_SZ(args...)
#endif

#define HEX2INT(x, i) if (x >= '0' && x <= '9')      i |= x - '0'; \
                       else if (x >= 'A' && x <= 'F') i |= 0xA + x - 'A'; \
                       else if (x >= 'a' && x <= 'f') i |= 0xA + x - 'a'

/* offset_of returns the number of bytes that the fieldname MEMBER is offset from the beginning of the structure TYPE */
#define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define HPMUD_EXCEPTION_TIMEOUT 45000000  /* microseconds */
#define HPMUD_EXCEPTION_SEC_TIMEOUT 45  /* seconds */
#define HPMUD_MDNS_TIMEOUT 10  /* seconds */

#define NFAULT_BIT  0x08
#define PERROR_BIT  0x20

enum HPMUD_CHANNEL_ID
{
   HPMUD_PML_CHANNEL = 1,
   HPMUD_PRINT_CHANNEL = 2,
   HPMUD_SCAN_CHANNEL = 4,
   HPMUD_FAX_SEND_CHANNEL = 7,
   HPMUD_CONFIG_UPLOAD_CHANNEL = 0xe,
   HPMUD_CONFIG_DOWNLOAD_CHANNEL = 0xf,
   HPMUD_MEMORY_CARD_CHANNEL = 0x11,
   HPMUD_EWS_CHANNEL = 0x12,          /* Embeded Web Server interface ff/1/1, any unused socket id */
   HPMUD_SOAPSCAN_CHANNEL = 0x13,          /* Soap Scan interface ff/2/1, any unused socket id */
   HPMUD_SOAPFAX_CHANNEL = 0x14,          /* Soap Fax interface ff/3/1, any unused socket id */
   HPMUD_MARVELL_SCAN_CHANNEL = 0x15,    /* Marvell scan interface ff/ff/ff, any unused socket id */
   HPMUD_MARVELL_FAX_CHANNEL = 0x16,    /* Marvell fax interface ff/ff/ff, any unused socket id */
   HPMUD_EWS_LEDM_CHANNEL = 0x17,     /* Embeded Web Server interface ff/4/1, any unused socket id */
   HPMUD_LEDM_SCAN_CHANNEL = 0x18,  /* LEDM scan interface ff/cc/0, any unused socket id */
   HPMUD_MARVELL_EWS_CHANNEL = 0x19, /*MARVELL EWS interface found in Cicad Series*/
   HPMUD_ESCL_SCAN_CHANNEL = 0x1a,  /* ESCL scan interface ff/cc/0, any unused socket id */
   HPMUD_WIFI_CHANNEL = 0x2b,      /* WIFI config */
   HPMUD_DEVMGMT_CHANNEL = 0x2c,      /* decimal 44 */
   HPMUD_IPP_CHANNEL = 0x2d,
   HPMUD_IPP_CHANNEL2 = 0x2e,
   HPMUD_MAX_CHANNEL_ID
};

#define HPMUD_DEVICE_MAX 2      /* zero is not used */
#define HPMUD_CHANNEL_MAX HPMUD_MAX_CHANNEL_ID

/* MLC/1284.4 attributes. Note for MLC, attributes must remain persistant while transport is up. */
typedef struct
{
   unsigned short h2pcredit;   /* host to peripheral credit (dot4: primary socket id credit for sending) */
   unsigned short p2hcredit;  /* peripheral to host credit (dot4: secondary socket id credit for sending) */
   unsigned short h2psize;  /* host to peripheral packet size in bytes (dot4: primary max packet size for sending) */
   unsigned short p2hsize;  /* peripheral to host packet size in bytes (dot4: secondary max packet size for sending) */
} transport_attributes;

typedef struct _mud_channel_vf
{
   enum HPMUD_RESULT (*open)(struct _mud_channel *pc);                                        /* transport specific open */
   enum HPMUD_RESULT (*close)(struct _mud_channel *pc);                                       /* transport specific close */
   enum HPMUD_RESULT (*channel_write)(struct _mud_channel *pc, const void *buf, int size, int timeout, int *bytes_wrote);  /* tranport specific write */
   enum HPMUD_RESULT (*channel_read)(struct _mud_channel *pc, void *buf, int size, int timeout, int *bytes_read);   /* transport specific read */
} mud_channel_vf;

typedef struct _mud_device_vf
{
   int (*write)(int fd, const void *buf, int size, int usec_timeout);                     /* low level device write */
   int (*read)(int fd, void *buf, int size, int usec_timout);           /* low level device read */
   enum HPMUD_RESULT (*open)(struct _mud_device *pd);                                        /* device specific open */
   enum HPMUD_RESULT (*close)(struct _mud_device *pd);                                       /* device specific close */
   enum HPMUD_RESULT (*get_device_id)(struct _mud_device *pd, char *id, int size, int *bytes_read);                      /* IEEE 1284 device id string */
   enum HPMUD_RESULT (*get_device_status)(struct _mud_device *pd, unsigned int *status);                     /* device 8-bit status */
   enum HPMUD_RESULT (*channel_open)(struct _mud_device *pd, const char *channel_name, HPMUD_CHANNEL *cd);                        /* channel specific open */
   enum HPMUD_RESULT (*channel_close)(struct _mud_device *pd, struct _mud_channel *pc);                                     /* channel specific close */
   enum HPMUD_RESULT (*channel_write)(struct _mud_device *pd, struct _mud_channel *pc, const void *buf, int size, int sec_timeout, int *bytes_wrote);
   enum HPMUD_RESULT (*channel_read)(struct _mud_device *pd, struct _mud_channel *pc, void *buf, int size, int sec_timeout, int *bytes_read);
} mud_device_vf;

typedef struct _mud_channel
{
   char sn[HPMUD_LINE_SIZE];         /* service name */
   unsigned char sockid;       /* socket id */
   int client_cnt;             /* number of clients using this channel */
   int index;                  /* channel[index] of this object */
   int fd;                     /* file descriptor for this channel */
   pid_t pid;                  /* process owner */
   int dindex;                 /* device[dindex] parent device */

   /* MLC/1284.4 specific variables. */
   transport_attributes ta;
   unsigned char rbuf[HPMUD_BUFFER_SIZE];  /* read packet buffer */
   int rindex;
   int rcnt;

   /* JetDirect specific data. */
   int socket;

   mud_channel_vf vf;
} mud_channel;

typedef struct _mud_device
{
   char uri[HPMUD_LINE_SIZE];
   char id[1024];                    /* device id */
   int index;                        /* device[index] of this object */
   enum HPMUD_IO_MODE io_mode;
   mud_channel channel[HPMUD_CHANNEL_MAX];
   int channel_cnt;                  /* number of open channels */
   int open_fd;                      /* file descriptor used by device_open */

   /* MLC/1284.4 specific variables. */
   int mlc_up;                       /* 0=transport down, 1=transport up */
   int mlc_fd;                       /* file descriptor used by 1284.4/MLC transport */

   /* JetDirect specific data. */
   char ip[HPMUD_LINE_SIZE];              /* internet address */
   int port;

   mud_device_vf vf;                 /* virtual function table */
   pthread_mutex_t mutex;
} mud_device;

typedef struct
{
   mud_device device[HPMUD_DEVICE_MAX];
   pthread_mutex_t mutex;
} mud_session;

extern mud_session *msp __attribute__ ((visibility ("hidden")));

void __attribute__ ((visibility ("hidden"))) sysdump(const void *data, int size);
int __attribute__ ((visibility ("hidden"))) mm_device_lock(int fd, HPMUD_DEVICE index);
int __attribute__ ((visibility ("hidden"))) mm_device_unlock(int fd, HPMUD_DEVICE index);
int __attribute__ ((visibility ("hidden"))) mm_device_trylock(int fd, HPMUD_DEVICE index);
int __attribute__ ((visibility ("hidden"))) is_hp(const char *id);
int  __attribute__ ((visibility ("hidden"))) generalize_model(const char *sz, char *buf, int bufSize);
int  __attribute__ ((visibility ("hidden"))) generalize_serial(const char *sz, char *buf, int bufSize);
int __attribute__ ((visibility ("hidden"))) get_uri_model(const char *uri, char *buf, int bufSize);
int __attribute__ ((visibility ("hidden"))) get_uri_serial(const char *uri, char *buf, int bufSize);
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) service_to_channel(mud_device *pd, const char *sn, HPMUD_CHANNEL *index);
extern int (*getSIData)(char **pData , int *pDataLen, char **pModeSwitch, int *pModeSwitchLen);
extern void (*freeSIData)(char *pData, char *pModeSwitch);

static const char *SnmpPort[] = { "","public","public.1","public.2","public.3"};

#define PORT_PUBLIC  1
#define PORT_PUBLIC_1  2
#define PORT_PUBLIC_2  3
#define PORT_PUBLIC_3  4

#endif // _HPMUDI_H

