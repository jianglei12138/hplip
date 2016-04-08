/*****************************************************************************\

  jd.c - JetDirect support for multi-point transport driver 
 
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
  Client/Server generic message format (see messaging-protocol.doc):

  Author: Naga Samrat Chowdary Narla, Sarbeswar Meher
\*****************************************************************************/
#ifdef HAVE_LIBNETSNMP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <signal.h>
#include "hpmud.h"
#include "hpmudi.h"


mud_device_vf __attribute__ ((visibility ("hidden"))) jd_mud_device_vf = 
{
   .open = jd_open,
   .close = jd_close,
   .get_device_id = jd_get_device_id,
   .get_device_status = jd_get_device_status,
   .channel_open = jd_channel_open,
   .channel_close = jd_channel_close,
   .channel_write = jd_channel_write,
   .channel_read = jd_channel_read
};

static mud_channel_vf jd_channel_vf =
{
   .open = jd_s_channel_open, 
   .close = jd_s_channel_close,
   .channel_write = jd_s_channel_write,
   .channel_read = jd_s_channel_read
};

static const int PrintPort[] = { 0, 9100, 9100, 9101, 9102 };
static const int ScanPort0[] = { 0, 9290, 9290, 9291, 9292 };
static const int GenericPort[] = { 0, 9220, 9220, 9221, 9222 };
static const int ScanPort1[] = { 0, 8290, 8290, 0, 0 };        /* hack for CLJ28xx */
static const int GenericPort1[] = { 0, 8292, 8292, 0, 0 };     /* hack for CLJ28xx (fax) */

const char __attribute__ ((visibility ("hidden"))) *kStatusOID = "1.3.6.1.4.1.11.2.3.9.1.1.7.0";            /* device id snmp oid */

static int ReadReply(mud_channel *pc)
{
   char buf[HPMUD_LINE_SIZE];
   int len=0, num=0;
   char *tail;
   enum HPMUD_RESULT stat;

   stat = jd_s_channel_read(pc, buf, sizeof(buf), 2, &len);
   buf[len] = 0;

   if (stat == HPMUD_R_OK)
      num = strtol((char *)buf, &tail, 10);

   return num;
}

static int device_id(const char *iporhostname, int port, char *buffer, int size)
{
   int len=0, maxSize, result, dt, status;

   maxSize = (size > 1024) ? 1024 : size;   /* RH8 has a size limit for device id */

   if ((len = GetSnmp(iporhostname, port, (char *)kStatusOID, (unsigned char *)buffer, maxSize, &dt, &status, &result)) == 0)
   {       
        //Try one more time with previous default community name string "public.1" which was used for old HP printers.  
       if ((len = GetSnmp(iporhostname, PORT_PUBLIC_1, (char *)kStatusOID, (unsigned char *)buffer, maxSize, &dt, &status, &result)) == 0)
       {
            BUG("unable to read device-id\n");
       }
   }

   return len; /* length does not include zero termination */
}

/* Create channel object given the requested socket id and service name. */
static int new_channel(mud_device *pd, int index, const char *sn)
{
   int stat=1;

   /* Check for existing name service already open. */
   if (pd->channel[index].client_cnt)
   {
#if 0
      if (index == HPMUD_EWS_CHANNEL)
      {
         pd->channel[index].client_cnt++;  /* allow multiple clients for separate USB interfaces only */
         stat = 0;
         DBG("reused %s channel=%d clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].client_cnt, pd->channel_cnt);
      }
      else
#endif
         BUG("%s channel=%d is busy, used by [%d], clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].pid, pd->channel[index].client_cnt, pd->channel_cnt);
      goto bugout; 
   }

   pd->channel[index].vf = jd_channel_vf;
   pd->channel[index].index = index;
   pd->channel[index].client_cnt = 1;
   pd->channel[index].sockid = index;
   pd->channel[index].pid = getpid();
   pd->channel[index].dindex = pd->index;
   pd->channel[index].fd = 0;
   pd->channel[index].socket = -1;
   strcpy(pd->channel[index].sn, sn);
   pd->channel_cnt++;

   stat = 0;
   DBG("new %s channel=%d clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].client_cnt, pd->channel_cnt);

bugout:
   return stat;
}

/* Remove channel object given the channel decriptor. */
static int del_channel(mud_device *pd, mud_channel *pc)
{
   pc->client_cnt--;

   if (pc->client_cnt <= 0)
   {
      pd->channel_cnt--;
   }
   DBG("removed %s channel=%d clientCnt=%d channelCnt=%d\n", pc->sn, pc->index, pc->client_cnt, pd->channel_cnt);
   return 0;
}

/*********************************************************************************************************************************
 * JetDirect mud_device functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_open(mud_device *pd)
{
   char uri_model[128];
   char model[128];
   char *p, *tail;
   int len=0;
   int printqueue = 0;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   pthread_mutex_lock(&pd->mutex);

   //Check whether the URI is generated dynamically by using bonjour discovery. If yes, then
   //there is no need of validating the MDL string against Device ID MDL string. If URI belongs to
   //an existing print queue then we need to validate the MDL, because the same IP address might get
   //assigned to some other device after adding a print queue.
   printqueue = (strstr(pd->uri, "queue=false") )? 0:1;

   //Scanimage discards "&queue=false" from the device URI and hence above check will not work.
   //Since Scanjets do not support SNMP hence skip device_id call by setting printqueue to 0
   if(strstr(pd->uri, "scanjet"))
      printqueue = 0;

   if (pd->id[0] == 0)
   {
      /* First client. */
      hpmud_get_uri_datalink(pd->uri, pd->ip, sizeof(pd->ip));

      if ((p = strcasestr(pd->uri, "port=")) != NULL)
         pd->port = strtol(p+5, &tail, 10);
      else
         pd->port = PORT_PUBLIC;

      if (pd->port > PORT_PUBLIC_3)
      {
         stat = HPMUD_R_INVALID_IP_PORT;
         BUG("invalid ip port=%d\n", pd->port);
         goto blackout;
      }

      if(printqueue)
      {
          len = device_id(pd->ip, pd->port, pd->id, sizeof(pd->id));  /* get new copy and cache it  */
          if (len == 0)
          {
             stat = HPMUD_R_IO_ERROR;
             goto blackout;
          }
      }
   }

   if(printqueue)
   {
       /* Make sure uri model matches device id model. */
       hpmud_get_uri_model(pd->uri, uri_model, sizeof(uri_model));
       hpmud_get_model(pd->id, model, sizeof(model));
       if (strcasecmp(uri_model, model) != 0)
       {
          stat = HPMUD_R_INVALID_URI;  /* different device plugged in */
          BUG("invalid uri model %s != %s\n", uri_model, model);
          goto blackout;
       }
   }

   stat = HPMUD_R_OK;

blackout:
   pthread_mutex_unlock(&pd->mutex);

   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_close(mud_device *pd)
{
   enum HPMUD_RESULT stat = HPMUD_R_OK;

   pthread_mutex_lock(&pd->mutex);
   pd->id[0] = 0;
   pthread_mutex_unlock(&pd->mutex);

   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_get_device_id(mud_device *pd, char *buf, int size, int *len)
{
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;
   
   *len=0;

   pthread_mutex_lock(&pd->mutex);

   *len = device_id(pd->ip, pd->port, pd->id, sizeof(pd->id));  /* get new copy and cache it  */ 

   if (*len)
   {
      memcpy(buf, pd->id, *len > size ? size : *len); 
      stat = HPMUD_R_OK;
   }

   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_get_device_status(mud_device *pd, unsigned int *status)
{
   *status = NFAULT_BIT;    /* there is no 8-bit status, so fake it */
   return HPMUD_R_OK;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_write(mud_device *pd, mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{   
   enum HPMUD_RESULT stat;

   pthread_mutex_lock(&pd->mutex);
   stat  = (pc->vf.channel_write)(pc, buf, length, sec_timeout, bytes_wrote);
   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_read(mud_device *pd, mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{   
   enum HPMUD_RESULT stat;

   if (pd->io_mode == HPMUD_UNI_MODE)
   {
      stat = HPMUD_R_INVALID_STATE;
      BUG("invalid channel_read io_mode=%d\n", pd->io_mode);
   }
 
   pthread_mutex_lock(&pd->mutex);
   stat  = (pc->vf.channel_read)(pc, buf, length, sec_timeout, bytes_read);
   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_open(mud_device *pd, const char *sn, HPMUD_CHANNEL *cd)
{
   int index;
   enum HPMUD_RESULT stat;

   /* Check for valid service requests. */
   if ((stat = service_to_channel(pd, sn, &index)) != HPMUD_R_OK)
      goto bugout;

   pthread_mutex_lock(&pd->mutex);

   if (new_channel(pd, index, sn))
   {
      stat = HPMUD_R_DEVICE_BUSY;
   }
   else
   {
      if ((stat = (pd->channel[index].vf.open)(&pd->channel[index])) != HPMUD_R_OK)  /* call transport specific open */
         del_channel(pd, &pd->channel[index]);   /* open failed, cleanup */
      else
         *cd = index;
   }

   pthread_mutex_unlock(&pd->mutex);

bugout:
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_channel_close(mud_device *pd, mud_channel *pc)
{
   enum HPMUD_RESULT stat = HPMUD_R_OK;

   pthread_mutex_lock(&pd->mutex);
   stat = (pc->vf.close)(pc);      /* call trasport specific close */
   del_channel(pd, pc);
   pthread_mutex_unlock(&pd->mutex);

   return stat;
}

/*******************************************************************************************************************************
 * JetDirect channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_open(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   struct sockaddr_in pin,tmp_pin;  
   struct hostent *he;
   char buf[HPMUD_LINE_SIZE];
   int r, len, port;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   bzero(&tmp_pin, sizeof(tmp_pin)); 
   bzero(&pin, sizeof(pin));  
   pin.sin_family = AF_INET;  

   if(inet_pton(AF_INET, pd->ip, &(tmp_pin.sin_addr))) //Returns 0 when IP is invalid.
        pin.sin_addr.s_addr = inet_addr(pd->ip);  
   else
   {
        if((he=gethostbyname(pd->ip)) == NULL)
        {
            BUG("gethostbyname() returned NULL\n");
            goto bugout;  
        }

        pin.sin_addr = *((struct in_addr *)he->h_addr);
   }

   switch (pc->index)
   {
      case HPMUD_PRINT_CHANNEL:
         port = PrintPort[pd->port];
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open print port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to print port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         break;
      case HPMUD_SCAN_CHANNEL:
         if (pd->io_mode == HPMUD_DOT4_PHOENIX_MODE)
            port = ScanPort1[pd->port];
         else
            port = ScanPort0[pd->port];
         pin.sin_port = htons(port);

         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open scan port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to scan err=%d port %d: %m %s\n", errno, port, pd->uri);  
            goto bugout;  
         }
         if (pd->io_mode != HPMUD_DOT4_PHOENIX_MODE)
         {
            r = ReadReply(pc);
            if (r != 0)
            {
               BUG("invalid scan response %d port %d %s\n", r, port, pd->uri);  
               goto bugout;  
            } 
         }
         break;
      case HPMUD_MEMORY_CARD_CHANNEL:
      case HPMUD_FAX_SEND_CHANNEL:
      case HPMUD_CONFIG_UPLOAD_CHANNEL:
      case HPMUD_CONFIG_DOWNLOAD_CHANNEL:
         if (pd->io_mode == HPMUD_DOT4_PHOENIX_MODE)
            port = GenericPort1[pd->port];
         else
            port = GenericPort[pd->port];
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         } 

         if (pd->io_mode != HPMUD_DOT4_PHOENIX_MODE)
         {
            r = ReadReply(pc);
            if (r != 220)
            {  
               BUG("invalid response %d port %d %s\n", r, port, pd->uri);  
               goto bugout;  
            } 
            len = sprintf(buf, "open %d\n", pc->index);
            send(pc->socket, buf, len, 0);
            r = ReadReply(pc);
            if (r != 200)
            {  
               BUG("invalid response %d port %d %s\n", r, port, pd->uri);  
               goto bugout;  
            } 
            len = sprintf(buf, "data\n");
            send(pc->socket, "data\n", len, 0);
            r = ReadReply(pc);
            if (r != 200)
            {  
               BUG("invalid response %d port %d %s\n", r, port, pd->uri);  
               goto bugout;  
            }
         }

         break;
      case HPMUD_EWS_CHANNEL:
         port = 80;
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open ews port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to ews port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }
         break; 
      case HPMUD_SOAPSCAN_CHANNEL:
         port = 8289;
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open soap-scan port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to soap-scan port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }
         break; 
      case HPMUD_SOAPFAX_CHANNEL:
         port = 8295;
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open soap-fax port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to soap-fax port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }
         break; 
      case HPMUD_MARVELL_SCAN_CHANNEL:
         port = 8290;  /* same as ScanPort1[1] */
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open marvell-scan port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to marvell-scan port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }
         break;
      case HPMUD_LEDM_SCAN_CHANNEL:
      case HPMUD_EWS_LEDM_CHANNEL:
      case HPMUD_ESCL_SCAN_CHANNEL:
         port = 8080;  
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
         {
            BUG("unable to open ledm-scan port %d: %m %s\n", port, pd->uri);
            goto bugout;
         }
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1)
         {
            BUG("unable to connect to ledm-scan port %d: %m %s\n", port, pd->uri);
            goto bugout;
         }
         break;            
      case HPMUD_IPP_CHANNEL:
         port = 631;
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
         {
            BUG("unable to open ipp port %d: %m %s\n", port, pd->uri);
            goto bugout;
         }
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1)
         {
            BUG("unable to connect to ipp port %d: %m %s\n", port, pd->uri);
            goto bugout;
         }
         break;
      case HPMUD_MARVELL_FAX_CHANNEL:
         port = 8285;  
         pin.sin_port = htons(port);
         if ((pc->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
         {  
            BUG("unable to open marvell-fax port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }  
         if (connect(pc->socket, (struct sockaddr *)&pin, sizeof(pin)) == -1) 
         {  
            BUG("unable to connect to marvell-fax port %d: %m %s\n", port, pd->uri);  
            goto bugout;  
         }
         break; 
      case HPMUD_PML_CHANNEL:
         /* Do nothing here, use GetPml/SetPml instead of ReadData/WriteData. */
         break;
      default:
         BUG("unsupported service %d %s\n", pc->index, pd->uri);
         stat = HPMUD_R_INVALID_SN;
         goto bugout;
         break;
   }  

   stat = HPMUD_R_OK;

bugout:
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_close(mud_channel *pc)
{
   if (pc->socket >= 0)
   {
      close(pc->socket);

      /* Delay for back-to-back scanning using scanimage. Otherwise next channel_open(HPMUD_SCAN_CHANNEL) can fail. */
      usleep(100000);
   }

   pc->socket = -1;  

   return HPMUD_R_OK;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_write(mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{
   mud_device *pd = &msp->device[pc->dindex];
   int len, size, total=0;
   struct timeval tmo;
   fd_set master;
   fd_set writefd;
   int maxfd, ret;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   *bytes_wrote=0;
   size = length;

   if (pc->socket<0)
   {
      stat = HPMUD_R_INVALID_STATE;
      BUG("invalid data link socket=%d %s\n", pc->socket, pd->uri);
      goto bugout;
   }

   FD_ZERO(&master);
   FD_SET(pc->socket, &master);
   maxfd = pc->socket;
   size = length;

   while (size > 0)
   {
      tmo.tv_sec = HPMUD_EXCEPTION_SEC_TIMEOUT;  /* note linux select will modify tmo */
      tmo.tv_usec = 0;
      writefd = master;
      if ((ret = select(maxfd+1, NULL, &writefd, NULL, &tmo)) == 0)
      {
         stat = HPMUD_R_IO_TIMEOUT;
         BUG("timeout write_channel %s\n", pd->uri);
         goto bugout;   /* timeout */
      }
      len = send(pc->socket, buf+total, size, 0);
      if (len < 0)
      {
         BUG("unable to write_channel: %m %s\n", pd->uri);
         goto bugout;
      }
      size-=len;
      total+=len;
      *bytes_wrote+=len;
   }
   
   DBG("write socket=%d len=%d size=%d\n", pc->socket, len, length);
   DBG_DUMP(buf, len < 32 ? len : 32);

   stat = HPMUD_R_OK;

bugout:
   return stat;
}

/*
 * Channel_read() tries to read "length" bytes from the peripheral. The returned read count may be zero
 * (timeout, no data available), less than "length" or equal "length".
 */
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) jd_s_channel_read(mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{
   mud_device *pd = &msp->device[pc->dindex];
   int len=0;
   struct timeval tmo;
   fd_set master;
   fd_set readfd;
   int maxfd, ret;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   *bytes_read = 0;

   if (pc->socket<0)
   {
      stat = HPMUD_R_INVALID_STATE;
      BUG("invalid data link socket=%d %s\n", pc->socket, pd->uri);
      goto bugout;
   }

   FD_ZERO(&master);
   FD_SET(pc->socket, &master);
   maxfd = pc->socket;
   tmo.tv_sec = sec_timeout;
   tmo.tv_usec = 0;

   readfd = master;
   ret = select(maxfd+1, &readfd, NULL, NULL, &tmo);
   if (ret < 0)
   {
      BUG("unable to read_channel: %m %s\n", pd->uri);
      goto bugout;
   }
   if (ret == 0)
   {
      stat = HPMUD_R_IO_TIMEOUT;
//      if (sec_timeout >= HPMUD_EXCEPTION_SEC_TIMEOUT)
         BUG("timeout read_channel sec=%d %s\n", sec_timeout, pd->uri);
      goto bugout;
   }
   else
   {
      if ((len = recv(pc->socket, buf, length, 0)) < 0)
      {
         BUG("unable to read_channel: %m %s\n", pd->uri);
         goto bugout;
      }
   }

   DBG("read socket=%d len=%d size=%d\n", pc->socket, len, length);
   DBG_DUMP(buf, len < 32 ? len : 32);

   *bytes_read = len;
   stat = HPMUD_R_OK;

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_make_net_uri(const char *ip, int port, char *uri, int uri_size, int *bytes_read)
{
   char id[1024];
   char model[128];
   enum HPMUD_RESULT stat;

   DBG("[%d] hpmud_make_net_uri() ip=%s port=%d\n", getpid(), ip, port);

   *bytes_read=0;

   uri[0]=0;

   if (ip == 0 || ip[0]==0)
   {
      BUG("invalid ip %s\n", ip);
      stat = HPMUD_R_INVALID_IP;
      goto bugout;
   }

   if (device_id(ip, port, id, sizeof(id)) > 0 && is_hp(id))
   {
      hpmud_get_model(id, model, sizeof(model));
      if (port == PORT_PUBLIC)
         *bytes_read = snprintf(uri, uri_size, "hp:/net/%s?ip=%s", model, ip); 
      else
         *bytes_read = snprintf(uri, uri_size, "hp:/net/%s?ip=%s&port=%d", model, ip, port); 
   }
   else
   {
      BUG("invalid ip %s\n", ip);
      stat = HPMUD_R_INVALID_IP;
      goto bugout;
   }

   stat = HPMUD_R_OK;

bugout:
   return stat;
}

enum HPMUD_RESULT hpmud_make_mdns_uri(const char *host, int port, char *uri, int uri_size, int *bytes_read)
{
   char id[1024];
   char model[128];
   char ip[HPMUD_LINE_SIZE];              /* internet address */
   enum HPMUD_RESULT stat;

   DBG("[%d] hpmud_make_mdns_uri() host=%s port=%d\n", getpid(), host, port);

   *bytes_read=0;

   uri[0]=0;

   if (host == 0 || host[0]==0)
   {
      BUG("invalid host %s\n", host);
      stat = HPMUD_R_INVALID_MDNS;
      goto bugout;
   }

   if (mdns_lookup(host, ip) != MDNS_STATUS_OK)
   {
      BUG("invalid host %s, check firewall UDP/5353 or try using IP\n", host);
      stat = HPMUD_R_INVALID_MDNS;
      goto bugout;
   }

   if (device_id(ip, port, id, sizeof(id)) > 0 && is_hp(id))
   {
      hpmud_get_model(id, model, sizeof(model));
      if (port == 1)
         *bytes_read = snprintf(uri, uri_size, "hp:/net/%s?zc=%s", model, host);
      else
         *bytes_read = snprintf(uri, uri_size, "hp:/net/%s?zc=%s&port=%d", model, host, port);
   }
   else
   {
      BUG("invalid host %s, or try using IP\n", host);
      stat = HPMUD_R_INVALID_MDNS;
      goto bugout;
   }

   stat = HPMUD_R_OK;

bugout:
   return stat;
}

#endif  /* HAVE_LIBNETSNMP */





