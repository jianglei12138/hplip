/*****************************************************************************\

  hpmud.cpp - multi-point transport driver
 
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
  Contributor: Sarbeswar Meher
\*****************************************************************************/

#include "hpmud.h"
#include "hpmudi.h"

/* Client data. */
mud_session ms __attribute__ ((visibility ("hidden")));      /* mud session, one per client */
mud_session *msp __attribute__ ((visibility ("hidden"))) = &ms;

/*
 * sysdump() originally came from http://sws.dett.de/mini/hexdump-c , steffen@dett.de .  
 */
void __attribute__ ((visibility ("hidden"))) sysdump(const void *data, int size)
{
    /* Dump size bytes of *data. Output looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20 30 FF 00 00 00 00 39 00 unknown 0.....9.
     */

    unsigned char *p = (unsigned char *)data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};
    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4d", (int)((p-(unsigned char *)data) & 0xffff));
        }
            
        c = *p;
        if (isprint(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
            DBG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        DBG_SZ("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

/* Given the IEEE 1284 device id string, determine if this is a HP product. */
int __attribute__ ((visibility ("hidden"))) is_hp(const char *id)
{
   char *pMf;
   if (id == 0 || id[0] == 0)
        return 0;
    
   if ((pMf = strstr(id, "MFG:")) != NULL)
      pMf+=4;
   else if ((pMf = strstr(id, "MANUFACTURER:")) != NULL)
      pMf+=13;
   else
      return 0;

   if ((strncasecmp(pMf, "HEWLETT-PACKARD", 15) == 0) ||
      (strncasecmp(pMf, "APOLLO", 6) == 0) || (strncasecmp(pMf, "HP", 2) == 0))
   {
      return 1;  /* found HP product */
   }
   return 0;   
}

int __attribute__ ((visibility ("hidden"))) generalize_model(const char *sz, char *buf, int bufSize)
{
   const char *pMd=sz;
   int i, j, dd=0;

   if (sz == 0 || sz[0] == 0)
        return 0;

    
   for (i=0; pMd[i] == ' ' && i < bufSize; i++);  /* eat leading white space */

   for (j=0; (pMd[i] != 0) && (pMd[i] != ';') && (j < bufSize); i++)
   {
      if (pMd[i]==' ' || pMd[i]=='/')
      {
         /* Remove double spaces. */
         if (!dd)
         { 
            buf[j++] = '_';   /* convert space to "_" */
            dd=1;              
         }
      }
      else
      {
         buf[j++] = pMd[i];
         dd=0;       
      }
   }

   for (j--; buf[j] == '_' && j > 0; j--);  /* eat trailing white space */

   buf[++j] = 0;

   return j;   /* length does not include zero termination */
}

int __attribute__ ((visibility ("hidden"))) generalize_serial(const char *sz, char *buf, int bufSize)
{
   const char *pMd=sz;
   int i, j;
    
   if (sz == 0 || sz[0] == 0)
        return 0;
    
   for (i=0; pMd[i] == ' ' && i < bufSize; i++);  /* eat leading white space */

   for (j=0; (pMd[i] != 0) && (i < bufSize); i++)
   {
      buf[j++] = pMd[i];
   }

   for (i--; buf[i] == ' ' && i > 0; i--);  /* eat trailing white space */

   buf[++i] = 0;

   return i;   /* length does not include zero termination */
}

/* Parse serial number from uri string. */
int __attribute__ ((visibility ("hidden"))) get_uri_serial(const char *uri, char *buf, int bufSize)
{
   char *p;
   int i;

   if (uri == 0 || uri[0] == 0)
      return 0;
    
   buf[0] = 0;

   if ((p = strcasestr(uri, "serial=")) != NULL)
      p+=7;
   else
      return 0;

   for (i=0; (p[i] != 0) && (p[i] != '+') && (i < bufSize); i++)
      buf[i] = p[i];

   buf[i] = 0;

   return i;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) service_to_channel(mud_device *pd, const char *sn, HPMUD_CHANNEL *index)
{
   enum HPMUD_RESULT stat;

   *index=-1;
   
   /* Check for valid service requests. */
   if (strncasecmp(sn, "print", 5) == 0)
   {
      *index = HPMUD_PRINT_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-ews-ledm", 11) == 0)
   {
      *index = HPMUD_EWS_LEDM_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-ews", 6) == 0)
   {
      *index = HPMUD_EWS_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-soap-scan", 12) == 0)
   {
      *index = HPMUD_SOAPSCAN_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-soap-fax", 11) == 0)
   {
      *index = HPMUD_SOAPFAX_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-marvell-scan", 15) == 0)
   {
      *index = HPMUD_MARVELL_SCAN_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-marvell-fax", 14) == 0)
   {
      *index = HPMUD_MARVELL_FAX_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-ledm-scan", 12) == 0)
   {
      *index = HPMUD_LEDM_SCAN_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-marvell-ews", 11) == 0)
   {
       *index = HPMUD_MARVELL_EWS_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-ipp", 6) == 0)
   {
       if (strncasecmp(sn, "hp-ipp2", 7) == 0)
            *index = HPMUD_IPP_CHANNEL2;
       else
            *index = HPMUD_IPP_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-escl-scan", 12) == 0)
   {
      *index = HPMUD_ESCL_SCAN_CHANNEL;
   }
   /* All the following services require MLC/1284.4. */
   else if (pd->io_mode == HPMUD_RAW_MODE || pd->io_mode == HPMUD_UNI_MODE)
   {
      BUG("invalid channel_open state, current io_mode=raw/uni service=%s %s\n", sn, pd->uri);
      stat = HPMUD_R_INVALID_STATE;
      goto bugout;
   }
   else if (strncasecmp(sn, "hp-message", 10) == 0)
   {
      *index = HPMUD_PML_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-scan", 7) == 0)
   {
      *index = HPMUD_SCAN_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-fax-send", 11) == 0)
   {
      *index = HPMUD_FAX_SEND_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-card-access", 14) == 0)
   {
      *index = HPMUD_MEMORY_CARD_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-configuration-upload", 23) == 0)
   {
      *index = HPMUD_CONFIG_UPLOAD_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-configuration-download", 25) == 0)
   {
      *index = HPMUD_CONFIG_DOWNLOAD_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-devmgmt", 10) == 0)
   {
      *index = HPMUD_DEVMGMT_CHANNEL;
   }
   else if (strncasecmp(sn, "hp-wificonfig", 13) == 0)
   {
      *index = HPMUD_WIFI_CHANNEL;
   }
   else
   {
      BUG("invalid service=%s %s\n", sn, pd->uri);
      stat = HPMUD_R_INVALID_SN;
      goto bugout;
   }

   stat = HPMUD_R_OK;

bugout:
   return stat;
}


static int new_device(const char *uri, enum HPMUD_IO_MODE mode, int *result)
{
   int index=0;      /* device[0] is unused */
   int i=1;

   if (uri == 0 || uri[0] == 0)
      return 0;
   
   pthread_mutex_lock(&msp->mutex);
   
   if (msp->device[i].index)
   {
      BUG("invalid device_open state\n");        /* device is already open for this client, one device per session */
      *result = HPMUD_R_INVALID_STATE;
      goto bugout;
   }

   index = i;      /* currently only support one device per client or process */

   /* Based on uri, set local session attributes. */
   if (strcasestr(uri, ":/usb") != NULL)
   {
      msp->device[i].vf = musb_mud_device_vf;
   }
#ifdef HAVE_LIBNETSNMP
   else if (strcasestr(uri, ":/net") != NULL)
   {
      msp->device[i].vf = jd_mud_device_vf;
   }
#endif
#ifdef HAVE_PPORT
   else if (strcasestr(uri, ":/par") != NULL)
   {
      msp->device[i].vf = pp_mud_device_vf;
   }
#endif
   else
   {
      BUG("invalid uri %s\n", uri);
      *result = HPMUD_R_INVALID_URI;
      index = 0;
      goto bugout;
   }
   *result = HPMUD_R_OK;
   msp->device[i].io_mode = mode;
   msp->device[i].index = index;
   msp->device[i].channel_cnt = 0;
   msp->device[i].open_fd = -1;
   strcpy(msp->device[i].uri, uri);

bugout:
   pthread_mutex_unlock(&msp->mutex);

   return index;  /* return device index */
}

static int del_device(HPMUD_DEVICE index)
{
   pthread_mutex_lock(&msp->mutex);

   msp->device[index].index = 0;

   pthread_mutex_unlock(&msp->mutex);

   return 0;
}

/*  Make sure client closed down the device. */
int device_cleanup(mud_session *ps)
{
   int i, dd=1;

   if (!ps) return 0;

   if(!ps->device[dd].index)
      return 0;          /* nothing to do */

   BUG("device_cleanup: device uri=%s\n", ps->device[dd].uri);

   for (i=0; i<HPMUD_CHANNEL_MAX; i++)
   {
      if (ps->device[dd].channel[i].client_cnt)
      {
         BUG("device_cleanup: close channel %d...\n", i);
         hpmud_close_channel(dd, ps->device[dd].channel[i].index);
         BUG("device_cleanup: done closing channel %d\n", i);
      }
   }

   BUG("device_cleanup: close device dd=%d...\n", dd);
   hpmud_close_device(dd);
   BUG("device_cleanup: done closing device dd=%d\n", dd);

   return 0;
}

static void __attribute__ ((constructor)) mud_init(void)
{
   DBG("[%d] hpmud_init()\n", getpid());
}

static void __attribute__ ((destructor)) mud_exit(void)
{
   DBG("[%d] hpmud_exit()\n", getpid());
   device_cleanup(msp);
}

/*******************************************************************************************************************************
 * Helper functions.
 */

/* Parse the model from the IEEE 1284 device id string and generalize the model name */
int hpmud_get_model(const char *id, char *buf, int buf_size)
{
   char *pMd;

   if (id == 0 || id[0] == 0)
        return 0;
    
   buf[0] = 0;

   if ((pMd = strstr(id, "MDL:")) != NULL)
      pMd+=4;
   else if ((pMd = strstr(id, "MODEL:")) != NULL)
      pMd+=6;
   else
      return 0;

   return generalize_model(pMd, buf, buf_size);
}

/* Parse the model from the IEEE 1284 device id string. */
int hpmud_get_raw_model(char *id, char *raw, int rawSize)
{
   char *pMd;
   int i;

   if (id == 0 || id[0] == 0)
        return 0;
    
   raw[0] = 0;

   if ((pMd = strstr(id, "MDL:")) != NULL)
      pMd+=4;
   else if ((pMd = strstr(id, "MODEL:")) != NULL)
      pMd+=6;
   else
      return 0;

   for (i=0; (pMd[i] != ';') && (i < rawSize); i++)
      raw[i] = pMd[i];
   raw[i] = 0;

   return i;
}

/* Parse device model from uri string. */
int hpmud_get_uri_model(const char *uri, char *buf, int buf_size)
{
   char *p;
   int i;

   if (uri == 0 || uri[0] == 0)
     return 0;

   buf[0] = 0;

   if ((p = strstr(uri, "/")) == NULL)
      return 0;
   if ((p = strstr(p+1, "/")) == NULL)
      return 0;
   p++;

   for (i=0; (p[i] != '?') && (i < buf_size); i++)
      buf[i] = p[i];

   buf[i] = 0;

   return i;
}

/* Parse the data link from a uri string. */
int hpmud_get_uri_datalink(const char *uri, char *buf, int buf_size)
{
   char *p;
   int i;
   int zc=0;
#ifdef HAVE_LIBNETSNMP
   char ip[HPMUD_LINE_SIZE];
#endif

   if (uri == 0 || uri[0] == 0)
     return 0;

   buf[0] = 0;

   if ((p = strcasestr(uri, "device=")) != NULL)
      p+=7;
   else if ((p = strcasestr(uri, "ip=")) != NULL)
      p+=3;
   else if ((p = strcasestr(uri, "hostname=")) != NULL)
      p+=9;
   else if ((p = strcasestr(uri, "zc=")) != NULL)
   {
      p+=3;
      zc=1;
   }
   else
      return 0;

   if (zc)
   {
#ifdef HAVE_LIBNETSNMP
    if (mdns_lookup(p, ip) != MDNS_STATUS_OK)
        return 0;
    for (i=0; (ip[i] != 0) && (i < buf_size); i++)
        buf[i] = ip[i];
#else
      return 0;
#endif
   }
   else {
      for (i=0; (p[i] != 0) && (p[i] != '&') && (i < buf_size); i++)
         buf[i] = p[i];
   }

   buf[i] = 0;

   return i;
}

/***************************************************************************************************
 * Core functions.
 */

enum HPMUD_RESULT hpmud_open_device(const char *uri, enum HPMUD_IO_MODE iomode, HPMUD_DEVICE *dd)
{
   HPMUD_DEVICE index=0;
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_URI;
   int result;

   DBG("[%d,%d,%d,%d,%d,%d] hpmud_device_open() uri=%s iomode=%d\n", getpid(), getppid(), getuid(), geteuid(), getgid(), getegid(), uri, iomode);

   if ((index = new_device(uri, iomode, &result)) == 0)
   {   
      stat = result;
      goto bugout;
   }
   else
   {
      if ((stat = (msp->device[index].vf.open)(&msp->device[index])) != HPMUD_R_OK)
      {
         (msp->device[index].vf.close)(&msp->device[index]);  /* Open failed perform device cleanup. */
         del_device(index);
         goto bugout;
      }
   }

   *dd = index;
   stat = HPMUD_R_OK;

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_close_device(HPMUD_DEVICE dd)
{
   enum HPMUD_RESULT stat;

   DBG("[%d] hpmud_device_close() dd=%d\n", getpid(), dd);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd)
   {
      BUG("invalid device_close state\n");
      stat = HPMUD_R_INVALID_STATE;
   }
   else
   {
      stat = (msp->device[dd].vf.close)(&msp->device[dd]);
      del_device(dd);
   }
   return stat;
}

enum HPMUD_RESULT hpmud_get_device_id(HPMUD_DEVICE dd, char *buf, int size, int *bytes_read)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_get_device_id() dd=%d\n", getpid(), dd);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd)
   {
      BUG("invalid get_device_id state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.get_device_id)(&msp->device[dd], buf, size, bytes_read);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_get_device_status(HPMUD_DEVICE dd, unsigned int *status)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_get_device_status() dd=%d\n", getpid(), dd);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd)
   {
      BUG("invalid get_device_status state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.get_device_status)(&msp->device[dd], status);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_probe_devices(enum HPMUD_BUS_ID bus, char *buf, int buf_size, int *cnt, int *bytes_read)
{
   int len=0;

   DBG("[%d] hpmud_probe_devices() bus=%d\n", getpid(), bus);
   if (buf == NULL || buf_size <= 0)
        return HPMUD_R_INVALID_LENGTH;
    
   buf[0] = 0;
   *cnt = 0;

   if (bus == HPMUD_BUS_USB)
   {
      len = musb_probe_devices(buf, buf_size, cnt, HPMUD_AIO);
   }
#ifdef HAVE_PPORT
   else if (bus == HPMUD_BUS_PARALLEL)
   {
      len = pp_probe_devices(buf, buf_size, cnt);
   }
#endif
   else if (bus == HPMUD_BUS_ALL)
   {
      len = musb_probe_devices(buf, buf_size, cnt, HPMUD_AIO);
#ifdef HAVE_PPORT
      len += pp_probe_devices(buf+len, buf_size-len, cnt);
#endif
   }

   *bytes_read = len;

   return HPMUD_R_OK;
}

enum HPMUD_RESULT hpmud_probe_printers(enum HPMUD_BUS_ID bus, char *buf, int buf_size, int *cnt, int *bytes_read)
{
   int len=0;

   DBG("[%d] hpmud_probe_printers() bus=%d\n", getpid(), bus);

   if (buf == NULL || buf_size <= 0)
        return HPMUD_R_INVALID_LENGTH;
    
   buf[0] = 0;
   *cnt = 0;

   if (bus == HPMUD_BUS_ALL)
   {
      len = musb_probe_devices(buf, buf_size, cnt, HPMUD_PRINTER);
#ifdef HAVE_PPORT
      len += pp_probe_devices(buf+len, buf_size-len, cnt);
#endif
   }

   *bytes_read = len;

   return HPMUD_R_OK;
}

enum HPMUD_RESULT hpmud_open_channel(HPMUD_DEVICE dd, const char *channel_name, HPMUD_CHANNEL *cd)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_channel_open() dd=%d name=%s\n", getpid(), dd, channel_name);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd)
   {
      BUG("invalid channel_open state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.channel_open)(&msp->device[dd], channel_name, cd);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_close_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_channel_close() dd=%d cd=%d\n", getpid(), dd, cd);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd ||
        cd <=0 || cd > HPMUD_CHANNEL_MAX || msp->device[dd].channel[cd].client_cnt == 0)
   {
      BUG("invalid channel_close state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.channel_close)(&msp->device[dd], &msp->device[dd].channel[cd]);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_write_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd, const void *buf, int size, int sec_timeout, int *bytes_wrote)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_channel_write() dd=%d cd=%d buf=%p size=%d sectime=%d\n", getpid(), dd, cd, buf, size, sec_timeout);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd ||
        cd <=0 || cd > HPMUD_CHANNEL_MAX || msp->device[dd].channel[cd].client_cnt == 0)
   {
      BUG("invalid channel_write state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.channel_write)(&msp->device[dd], &msp->device[dd].channel[cd], buf, size, sec_timeout, bytes_wrote);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_read_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd, void *buf, int size, int sec_timeout, int *bytes_read)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;
   DBG("[%d] hpmud_channel_read() dd=%d cd=%d buf=%p size=%d sectime=%d\n", getpid(), dd, cd, buf, size, sec_timeout);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX || msp->device[dd].index != dd ||
        cd <=0 || cd > HPMUD_CHANNEL_MAX || msp->device[dd].channel[cd].client_cnt == 0)
   {
      BUG("invalid channel_read state\n");
      goto bugout;
   }

   stat = (msp->device[dd].vf.channel_read)(&msp->device[dd], &msp->device[dd].channel[cd], buf, size, sec_timeout, bytes_read);

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_get_dstat(HPMUD_DEVICE dd, struct hpmud_dstat *ds)
{
   enum HPMUD_RESULT stat = HPMUD_R_INVALID_STATE;

   DBG("[%d] hpmud_dstat() dd=%d ds=%p\n", getpid(), dd, ds);

   if (dd <= 0 || dd > HPMUD_DEVICE_MAX)
   {
      BUG("invalid dstat state\n");
      goto bugout;
   }

   strncpy(ds->uri, msp->device[dd].uri, sizeof(ds->uri));
   ds->io_mode = msp->device[dd].io_mode;
   ds->channel_cnt = msp->device[dd].channel_cnt;
   ds->mlc_up = msp->device[dd].mlc_up;

   stat = HPMUD_R_OK;

bugout:
   return stat;
}
