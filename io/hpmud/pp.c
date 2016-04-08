/*****************************************************************************\

  pp.c - parallel port support for multi-point transport driver 
 
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

\*****************************************************************************/

#ifdef HAVE_PPORT

#include "hpmud.h"
#include "hpmudi.h"

mud_device_vf __attribute__ ((visibility ("hidden"))) pp_mud_device_vf = 
{
   .read = pp_read,
   .write = pp_write,
   .open = pp_open,
   .close = pp_close,
   .get_device_id = pp_get_device_id,
   .get_device_status = pp_get_device_status,
   .channel_open = pp_channel_open,
   .channel_close = pp_channel_close,
   .channel_write = musb_channel_write,
   .channel_read = musb_channel_read
};

static mud_channel_vf pp_raw_channel_vf =
{
   .open = pp_raw_channel_open,
   .close = pp_raw_channel_close,
   .channel_write = musb_raw_channel_write,
   .channel_read = musb_raw_channel_read
};

static mud_channel_vf pp_mlc_channel_vf =
{
   .open = pp_mlc_channel_open,
   .close = pp_mlc_channel_close,
   .channel_write = musb_mlc_channel_write,
   .channel_read = musb_mlc_channel_read
};

static mud_channel_vf pp_dot4_channel_vf =
{
   .open = pp_dot4_channel_open,
   .close = pp_dot4_channel_close,
   .channel_write = musb_dot4_channel_write,
   .channel_read = musb_dot4_channel_read
};

static int frob_control(int fd, unsigned char mask, unsigned char val)
{
   struct ppdev_frob_struct frob;

   /* Convert ieee1284 control values to PC-style (invert Strobe, AutoFd and Select) . */
   frob.val = val ^ (mask & (PARPORT_CONTROL_STROBE | PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_SELECT));

   frob.mask = mask;
   return ioctl(fd, PPFCONTROL, &frob);
}

static unsigned char read_status(int fd)
{
   unsigned char status;
   if (ioctl(fd, PPRSTATUS, &status))
      BUG("read_status error: %m\n");
   
   /* Convert PC-style status values to ieee1284 (invert Busy). */
   return (status ^ PARPORT_STATUS_BUSY); 
} 

static int wait_status(int fd, unsigned char mask, unsigned char val, int usec)
{
   struct timeval tmo, now;
   struct timespec min;
   unsigned char status;
   int cnt=0;
   
   gettimeofday (&tmo, NULL);
   tmo.tv_usec += usec;
   tmo.tv_sec += tmo.tv_usec / 1000000;
   tmo.tv_usec %= 1000000;

   min.tv_sec = 0;
   min.tv_nsec = 5000000;  /* 5ms */

   while (1)
   {
      status = read_status(fd);
      if ((status & mask) == val)
      {
	//         bug("found status=%x mask=%x val=%x cnt=%d: %s %d\n", status, mask, val, cnt, __FILE__, __LINE__);
         return 0;
      } 
      cnt++;
      //      nanosleep(&min, NULL);
      gettimeofday(&now, NULL);
      if ((now.tv_sec > tmo.tv_sec) || (now.tv_sec == tmo.tv_sec && now.tv_usec > tmo.tv_usec))
      {
         DBG("wait_status timeout status=%x mask=%x val=%x us=%d\n", status, mask, val, usec);
         return -1;   /* timeout */
      }
   }
}

static int wait(int usec)
{
   struct timeval tmo, now;
   int cnt=0;
   
   gettimeofday (&tmo, NULL);
   tmo.tv_usec += usec;
   tmo.tv_sec += tmo.tv_usec / 1000000;
   tmo.tv_usec %= 1000000;

   while (1)
   {
      cnt++;
      gettimeofday(&now, NULL);
      if ((now.tv_sec > tmo.tv_sec) || (now.tv_sec == tmo.tv_sec && now.tv_usec > tmo.tv_usec))
      {
         return 0;   /* timeout */
      }
   }
}

static int ecp_is_fwd(int fd)
{
   unsigned char status;

   status = read_status(fd);
   if ((status & PARPORT_STATUS_PAPEROUT) == PARPORT_STATUS_PAPEROUT)
      return 1;
   return 0;
}

static int ecp_is_rev(int fd)
{
   unsigned char status;

   status = read_status(fd);
   if ((status & PARPORT_STATUS_PAPEROUT) == 0)
      return 1;
   return 0;
}

static int ecp_rev_to_fwd(int fd)
{
   int dir=0;

   if (ecp_is_fwd(fd))
      return 0;

   /* Event 47: write NReverseRequest/nInit=1 */
   frob_control(fd, PARPORT_CONTROL_INIT, PARPORT_CONTROL_INIT);

   /* Event 48: wait PeriphClk/nAck=1, PeriphAck/Busy=0 */
   //   wait_status(fd, PARPORT_STATUS_PAPEROUT | PARPORT_STATUS_BUSY, PARPORT_STATUS_PAPEROUT, SIGNAL_TIMEOUT);

   /* Event 49: wait nAckReverse/PError=1 */
   wait_status(fd, PARPORT_STATUS_PAPEROUT, PARPORT_STATUS_PAPEROUT, PP_SIGNAL_TIMEOUT);

   ioctl(fd, PPDATADIR, &dir);

   return 0;
}

static int ecp_fwd_to_rev(int fd)
{
   int dir=1;

   if (ecp_is_rev(fd))
      return 0;

   /* Event 33: NPeriphRequest/nFault=0, PeriphAck/Busy=0 */
   wait_status(fd, PARPORT_STATUS_BUSY | PARPORT_STATUS_ERROR, 0, PP_DEVICE_TIMEOUT);

   /* Event 38: write HostAck/nAutoFd=0 */
   ioctl(fd, PPDATADIR, &dir);
   frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);
   wait(PP_SETUP_TIMEOUT);
   
   /* Event 39: write NReverseRequest/nInit=0 (start bus reversal) */
   frob_control(fd, PARPORT_CONTROL_INIT, 0);

   /* Event 40: wait nAckReverse/PError=0 */
   wait_status(fd, PARPORT_STATUS_PAPEROUT, 0, PP_SIGNAL_TIMEOUT);

   return 0;
}

static int ecp_write_addr(int fd, unsigned char data)
{
   int cnt=0, len=0;
   unsigned d=(data | 0x80); /* set channel address bit */

   ecp_rev_to_fwd(fd);

   /* Event 33: PeriphAck/Busy=0 */
   if (wait_status(fd, PARPORT_STATUS_BUSY, 0, PP_SIGNAL_TIMEOUT))
   {
      BUG("ecp_write_addr transfer stalled\n"); 
      goto bugout;
   }

   while (1)
   {   
      /* Event 34: write HostAck/nAutoFD=0 (channel command), data */
      frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);
      ioctl(fd, PPWDATA, &d);

      /* Event 35: write HostClk/NStrobe=0 */
      frob_control(fd, PARPORT_CONTROL_STROBE, 0);

      /* Event 36: wait PeriphAck/Busy=1 */
      if (wait_status(fd, PARPORT_STATUS_BUSY, PARPORT_STATUS_BUSY, PP_SIGNAL_TIMEOUT))
      {

         /* Event 72: write NReverseRequest/nInit=0 (Host Transfer Recovery) */
         frob_control(fd, PARPORT_CONTROL_INIT, 0);

         /* Event 73: wait nAckReverse/PError=0 */
         wait_status(fd, PARPORT_STATUS_PAPEROUT, 0, PP_SIGNAL_TIMEOUT);

         /* Event 74: write NReverseRequest/nInit=1 */
         frob_control(fd, PARPORT_CONTROL_INIT, PARPORT_CONTROL_INIT);

         /* Event 75: wait nAckReverse/PError=1 */
         wait_status(fd, PARPORT_STATUS_PAPEROUT, PARPORT_STATUS_PAPEROUT, PP_SIGNAL_TIMEOUT);

         cnt++;
         if (cnt > 4)
         {
            BUG("ecp_write_addr transfer stalled\n"); 
            goto bugout;
         }
         BUG("ecp_write_addr host transfer recovery cnt=%d\n", cnt); 
         continue;  /* retry */
      }
      break;  /* done */
   } /* while (1) */

   len = 1;
      
bugout:

   /* Event 37: write HostClk/NStrobe=1 */
   frob_control(fd, PARPORT_CONTROL_STROBE, PARPORT_CONTROL_STROBE);

   return len;
}

static int ecp_write_data(int fd, unsigned char data)
{
   int cnt=0, len=0;

   //   ecp_rev_to_fwd(fd);

   /* Event 33: check PeriphAck/Busy=0 */
   if (wait_status(fd, PARPORT_STATUS_BUSY, 0, PP_SIGNAL_TIMEOUT))
   {
      BUG("ecp_write_data transfer stalled\n"); 
      goto bugout;
   }

   while (1)
   {   
      /* Event 34: write HostAck/nAutoFD=1 (channel data), data */
      frob_control(fd, PARPORT_CONTROL_AUTOFD, PARPORT_CONTROL_AUTOFD);
      ioctl(fd, PPWDATA, &data);

      /* Event 35: write HostClk/NStrobe=0 */
      frob_control(fd, PARPORT_CONTROL_STROBE, 0);

      /* Event 36: wait PeriphAck/Busy=1 */
      if (wait_status(fd, PARPORT_STATUS_BUSY, PARPORT_STATUS_BUSY, PP_SIGNAL_TIMEOUT))
      {

         /* Event 72: write NReverseRequest/nInit=0 (Host Transfer Recovery) */
         frob_control(fd, PARPORT_CONTROL_INIT, 0);

         /* Event 73: wait nAckReverse/PError=0 */
         wait_status(fd, PARPORT_STATUS_PAPEROUT, 0, PP_SIGNAL_TIMEOUT);

         /* Event 74: write NReverseRequest/nInit=1 */
         frob_control(fd, PARPORT_CONTROL_INIT, PARPORT_CONTROL_INIT);

         /* Event 75: wait nAckReverse/PError=1 */
         wait_status(fd, PARPORT_STATUS_PAPEROUT, PARPORT_STATUS_PAPEROUT, PP_SIGNAL_TIMEOUT);

         cnt++;
         if (cnt > 4)
         {
            BUG("ecp_write_data transfer stalled\n"); 
            goto bugout;
         }
         BUG("ecp_write_data host transfer recovery cnt=%d\n", cnt); 
         continue;  /* retry */
      }
      break;  /* done */
   } /* while (1) */

   len = 1;
      
bugout:

   /* Event 37: write HostClk/NStrobe=1 */
   frob_control(fd, PARPORT_CONTROL_STROBE, PARPORT_CONTROL_STROBE);

   return len;
}

static int ecp_read_data(int fd, unsigned char *data)
{
   int len=0;

   //   ecp_fwd_to_rev(fd);

   /* Event 43: wait PeriphClk/NAck=0 */
   if (wait_status(fd, PARPORT_STATUS_ACK, 0, PP_SIGNAL_TIMEOUT))
   {
      len = -1;
      goto bugout;
   }
   ioctl(fd, PPRDATA, data);

   /* Event 44: write HostAck/nAutoFd=1 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, PARPORT_CONTROL_AUTOFD);

   /* Event 45: wait PeriphClk/NAck=1 */
   wait_status(fd, PARPORT_STATUS_ACK, PARPORT_STATUS_ACK, PP_SIGNAL_TIMEOUT);

   /* Event 46: write HostAck/nAutoFd=0 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);

   len = 1;
      
bugout:

   return len;
}

static int ecp_read(int fd, void *buffer, int size, int usec)
{
   int i=0;
   unsigned char *p = (unsigned char *)buffer;
   
   ecp_fwd_to_rev(fd);

   while (i < size)
   {
      if (ecp_read_data(fd, p+i) != 1)
      {
         usec-=PP_SIGNAL_TIMEOUT;
         if (usec > 0)
            continue;

//         return -1;
         return -ETIMEDOUT;   /* timeout */
      }
      i++;
   }
   return i;
}

static int ecp_write(int fd, const void *buffer, int size)
{
   int i;
   unsigned char *p = (unsigned char *)buffer;
   static int timeout=0;

   if (timeout)
   {
      timeout=0;
      return -1;        /* report timeout */
   }
   
   ecp_rev_to_fwd(fd);

   for (i=0; i < size; i++)
   {
      if (ecp_write_data(fd, p[i]) != 1)
      {
         if (i)
            timeout=1;  /* save timeout, report bytes written */
         else
            i=-1;       /* report timeout */
         break;
      }
   }
   return i;
}

static int nibble_read_data(int fd, unsigned char *data)
{
   int len=0;
   unsigned char nibble;   

   /* Event 7: write HostBusy/nAutoFd=0 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);

   /* Event 8: peripheral sets low-order nibble. */

   /* Event 9: wait PtrClk/NAck=0 */
   if (wait_status(fd, PARPORT_STATUS_ACK, 0, PP_SIGNAL_TIMEOUT))
   {
      len = -1;
      goto bugout;
   }
   nibble = read_status(fd) >> 3;
   nibble = ((nibble & 0x10) >> 1) | (nibble & 0x7);
   *data = nibble;

   /* Event 10: write HostBusy/nAutoFd=1 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, PARPORT_CONTROL_AUTOFD);

   /* Event 11: wait PtrClk/NAck=1 */
   wait_status(fd, PARPORT_STATUS_ACK, PARPORT_STATUS_ACK, PP_SIGNAL_TIMEOUT);

   /* Event 7: write HostBusy/nAutoFd=0 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);

   /* Event 8: peripheral sets high-order nibble. */

   /* Event 9: wait PtrClk/NAck=0 */
   if (wait_status(fd, PARPORT_STATUS_ACK, 0, PP_SIGNAL_TIMEOUT))
   {
      len = -1;
      goto bugout;
   }
   nibble = read_status(fd) >> 3;
   nibble = ((nibble & 0x10) >> 1) | (nibble & 0x7);
   *data |= (nibble<<4);

   /* Event 10: write HostBusy/nAutoFd=1 */
   frob_control(fd, PARPORT_CONTROL_AUTOFD, PARPORT_CONTROL_AUTOFD);

   /* Event 11: wait PtrClk/NAck=1 */
   wait_status(fd, PARPORT_STATUS_ACK, PARPORT_STATUS_ACK, PP_SIGNAL_TIMEOUT);

   len = 1;
      
bugout:

   return len;
}

static int nibble_read(int fd, int flag, void *buffer, int size, int usec)
{
   int i=0;
   unsigned char *p = (unsigned char *)buffer;
   int m = IEEE1284_MODE_NIBBLE | flag;
   int mc = IEEE1284_MODE_COMPAT;
   unsigned char status;

   ioctl (fd, PPNEGOT, &mc);
   if (ioctl (fd, PPNEGOT, &m))
   {
      DBG("nibble_read negotiation failed: %m\n");
      return -1;
   }

   while (i < size)
   {
      if (nibble_read_data(fd, p+i) != 1)
      {
         usec-=PP_SIGNAL_TIMEOUT;
         if (usec > 0)
            continue;

//         return -1;
         return -ETIMEDOUT;   /* timeout */
      }

      i++;

      /* More data? */
      status = read_status(fd);
      if (status & PARPORT_STATUS_ERROR)
      {
         /* Event 7: write HostBusy/nAutoFd=0, idle phase */
         frob_control(fd, PARPORT_CONTROL_AUTOFD, 0);
         
         break;  /* done */
      }
   }

   return i;
}

static int compat_write_data(int fd, unsigned char data)
{
   int len=0;

   /* wait Busy=0 */
   if (wait_status(fd, PARPORT_STATUS_BUSY, 0, PP_DEVICE_TIMEOUT))
   {
      BUG("compat_write_data transfer stalled\n"); 
      goto bugout;
   }

   ioctl(fd, PPWDATA, &data);
   wait(PP_SETUP_TIMEOUT);

   /* write NStrobe=0 */
   frob_control(fd, PARPORT_CONTROL_STROBE, 0);

   /* wait Busy=1 */
   if (wait_status(fd, PARPORT_STATUS_BUSY, PARPORT_STATUS_BUSY, PP_SIGNAL_TIMEOUT))
   {
      BUG("compat_write_data transfer stalled\n"); 
      goto bugout;
   }

   /* write nStrobe=1 */
   frob_control(fd, PARPORT_CONTROL_STROBE, PARPORT_CONTROL_STROBE);

   len = 1;
      
bugout:
   return len;
}

static int compat_write(int fd, const void *buffer, int size)
{
   int i=0;
   unsigned char *p = (unsigned char *)buffer;
   int m = IEEE1284_MODE_COMPAT;
   static int timeout=0;

   if (timeout)
   {
      timeout=0;
      return -1;        /* report timeout */
   }

   if (ioctl(fd, PPNEGOT, &m))
   {
      BUG("compat_write failed: %m\n");
      goto bugout;
   }

   for (i=0; i < size; i++)
   {
      if (compat_write_data(fd, p[i]) != 1)
      {
         if (i)
            timeout=1;  /* save timeout, report bytes written */
         else
            i=-1;       /* report timeout */
         break;
      }
   }

bugout:
   return i;
}

static int claim_pp(int fd)
{
   int stat=1;

   /* Claim parallel port (can block forever). */
   if (ioctl(fd, PPCLAIM))
   {
      BUG("failed claim_pp fd=%d: %m\n", fd);
      goto bugout;
   }

   DBG("claimed pp fd=%d\n", fd);

   stat=0;

bugout:
   return stat;   
}

static int release_pp(int fd)
{
   int stat=1, m=IEEE1284_MODE_COMPAT;

   /* Restore compat_mode (default), otherwise close(fd) may block restoring compat_mode. */
   if (ioctl(fd, PPNEGOT, &m))
   {
      BUG("failed release_pp fd=%d: %m\n", fd);
      goto bugout;
   }

   ioctl(fd, PPRELEASE);

   DBG("released pp fd=%d\n", fd);

   stat=0;

bugout:
   return 0;
}

static int device_id(int fd, char *buffer, int size)
{
   int len=0, maxSize;

   maxSize = (size > 1024) ? 1024 : size;   /* RH8 has a size limit for device id */

   len = nibble_read(fd, IEEE1284_DEVICEID, buffer, maxSize, 0);
   if (len < 0)
   {
      BUG("unable to read device-id ret=%d\n", len);
      len = 0;
      goto bugout;
   }
   if (len > (size-1))
      len = size-1;   /* leave byte for zero termination */
   if (len > 2)
      len -= 2;
   memcpy(buffer, buffer+2, len);    /* remove length */
   buffer[len]=0;

   DBG("read actual device_id successfully fd=%d len=%d\n", fd, len);

bugout:
   return len; /* length does not include zero termination */
}

static int device_status(int fd, unsigned int *status)
{
   int m, stat=1;
   unsigned char byte = NFAULT_BIT;        /* set default */

   m = IEEE1284_MODE_COMPAT;
   if (ioctl (fd, PPNEGOT, &m))
   {
      BUG("unable to read device_status: %m\n");
      stat = HPMUD_R_IO_ERROR;
      goto bugout;
   }
   byte = read_status(fd);

   *status = (unsigned int)byte;
   stat = 0;
   DBG("read actual device_status successfully fd=%d\n", fd);

bugout:
   return stat; 
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

   if (pd->io_mode == HPMUD_RAW_MODE || pd->io_mode == HPMUD_UNI_MODE)
      pd->channel[index].vf = pp_raw_channel_vf;
   else if (pd->io_mode == HPMUD_MLC_GUSHER_MODE || pd->io_mode == HPMUD_MLC_MISER_MODE)
      pd->channel[index].vf = pp_mlc_channel_vf;
   else
      pd->channel[index].vf = pp_dot4_channel_vf;

   pd->channel[index].index = index;
   pd->channel[index].client_cnt = 1;
   pd->channel[index].sockid = index;   /* static socket id is valid for MLC but not 1284.4 */
   pd->channel[index].pid = getpid();
   pd->channel[index].dindex = pd->index;
   pd->channel[index].fd = -1;
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
 * Parallel port mud_device functions.
 */

int __attribute__ ((visibility ("hidden"))) pp_write(int fd, const void *buf, int size, int usec)
{
   int len=0, m;

   ioctl(fd, PPGETMODE, &m);

   if (m & (IEEE1284_MODE_ECPSWE | IEEE1284_MODE_ECP))
   {
      len = ecp_write(fd, buf, size);
   }
   else
   {  
      len = compat_write(fd, buf, size);
   }

   DBG("write fd=%d len=%d size=%d\n", fd, len, size);
   DBG_DUMP(buf, len < 32 ? len : 32);

   return len;
}

int __attribute__ ((visibility ("hidden"))) pp_read(int fd, void *buf, int size, int usec)
{
   int len=0, m;
//   int sec = usec/1000000;

   ioctl(fd, PPGETMODE, &m);

   if (m & (IEEE1284_MODE_ECPSWE | IEEE1284_MODE_ECP))
   {  
      len = ecp_read(fd, buf, size, usec);
   }
   else
   {
      len = nibble_read(fd, 0, buf, size, usec);
   }

   DBG("read fd=%d len=%d size=%d usec=%d\n", fd, len, size, usec);
   DBG_DUMP(buf, len < 32 ? len : 32);

   return len;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_open(mud_device *pd)
{
   char dev[255], uriModel[128], model[128];
   int len, m, fd;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   pthread_mutex_lock(&pd->mutex);

   hpmud_get_uri_model(pd->uri, uriModel, sizeof(uriModel));

   if (pd->id[0] == 0)
   {
      /* First client, open actual kernal device, use blocking i/o. */
      hpmud_get_uri_datalink(pd->uri, dev, sizeof(dev));
      if ((fd = open(dev, O_RDWR | O_NOCTTY)) < 0)            
      {
         BUG("unable to open %s: %m\n", pd->uri);
         goto bugout;
      }

      /* Open can succeed with no connected device, see if this is a valid device. */
      if (ioctl(fd, PPGETMODES, &m))
      {
         BUG("unable to open %s: %m\n", pd->uri);
         goto bugout;
      }

      /* Claim parallel port (can block forever). */
      if (claim_pp(fd))
         goto bugout;

      len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy and cache it  */ 

      if (len > 0 && is_hp(pd->id))
         power_up(pd, fd);

      release_pp(fd);

      if (len == 0)
         goto bugout;

      pd->open_fd = fd;
   }

   /* Make sure uri model matches device id model. */
   hpmud_get_model(pd->id, model, sizeof(model));
   if (strcmp(uriModel, model) != 0)
   {
      stat = HPMUD_R_INVALID_DEVICE_NODE;  /* probably a laserjet, or different device plugged in */  
      BUG("invalid model %s != %s\n", uriModel, model);
      goto bugout;
   }

   stat = HPMUD_R_OK;

bugout:
   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_close(mud_device *pd)
{
   enum HPMUD_RESULT stat = HPMUD_R_OK;

   pthread_mutex_lock(&pd->mutex);

   if (pd->open_fd >=0)
      close(pd->open_fd);

   pd->open_fd = -1;
   pd->id[0] = 0;

   pthread_mutex_unlock(&pd->mutex);

   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_get_device_id(mud_device *pd, char *buf, int size, int *len)
{
   int m, fd = pd->open_fd;
   enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;
   
   *len=0;

   pthread_mutex_lock(&pd->mutex);

   if (fd < 0)
   {
      stat = HPMUD_R_INVALID_STATE;
      BUG("invalid get_device_id state\n");
      goto bugout;
   }

   if (pd->io_mode == HPMUD_UNI_MODE)
   {
      *len = strlen(pd->id);  /* use cached copy */
      DBG("using cached device_id io_mode=%d\n", pd->io_mode);
   }
   else
   {
      ioctl(fd, PPGETMODE, &m);
      if (m & (IEEE1284_MODE_ECPSWE | IEEE1284_MODE_ECP))
      {
         *len = strlen(pd->id);  /* channel is busy, return cached copy. */
         DBG("using cached device_id m=%x\n", m);
      }
      else
      {
         if (pd->channel_cnt == 0)
         {
            /* Device not in use. Claim it, but release for other processes. */
            if (claim_pp(fd))
               goto bugout;
            *len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy */
            release_pp(fd);
         }
         else
         {
            /* Device already claimed by open_channel. */
            *len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy */
         }
      }
   }

   if (*len)
   {
      memcpy(buf, pd->id, *len > size ? size : *len); 
      stat = HPMUD_R_OK;
   }

bugout:
   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_get_device_status(mud_device *pd, unsigned int *status)
{
   int fd=pd->open_fd;
   enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;
   int m, r=0;

   pthread_mutex_lock(&pd->mutex);

   if (fd < 0)
   {
      stat = HPMUD_R_INVALID_STATE;
      BUG("invalid get_device_id state\n");
      goto bugout;
   }

   if (pd->io_mode == HPMUD_UNI_MODE)
   {
      *status = NFAULT_BIT;   /* fake status */
      DBG("using cached device_status io_mode=%d\n", pd->io_mode);
   }
   else
   {
      ioctl(fd, PPGETMODE, &m);
      if (m & (IEEE1284_MODE_ECPSWE | IEEE1284_MODE_ECP))
      {
         *status = NFAULT_BIT;        /* channel is busy, fake 8-bit status */
         DBG("using cached device_status m=%x\n", m);
      }
      else
      {
         if (pd->channel_cnt == 0)
         {
            /* Device not in use. Claim it, but release for other processes. */
            if (claim_pp(fd))
               goto bugout;
            r = device_status(fd, status);
            release_pp(fd);
         }
         else
         {
            /* Device already claimed by open_channel. */
            r = device_status(fd, status);
         }
      }
   }

   if (r != 0)
      goto bugout;
    
   stat = HPMUD_R_OK;

bugout:
   pthread_mutex_unlock(&pd->mutex);
   return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_channel_open(mud_device *pd, const char *sn, HPMUD_CHANNEL *cd)
{
   int index;
   enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;

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

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_channel_close(mud_device *pd, mud_channel *pc)
{
   enum HPMUD_RESULT stat = HPMUD_R_OK;

   pthread_mutex_lock(&pd->mutex);
   stat = (pc->vf.close)(pc);      /* call trasport specific close */
   del_channel(pd, pc);
   pthread_mutex_unlock(&pd->mutex);

   return stat;
}

/*******************************************************************************************************************************
 * Parallel port raw_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_raw_channel_open(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   if (claim_pp(pd->open_fd))
      return HPMUD_R_IO_ERROR;
   pc->fd = pd->open_fd;
   return HPMUD_R_OK;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_raw_channel_close(mud_channel *pc)
{
   if (pc->fd >= 0)
      release_pp(pc->fd);
   pc->fd = -1;
   return HPMUD_R_OK;
}

/*******************************************************************************************************************************
 * Parallel port mlc_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_mlc_channel_open(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;
   int i, m;

   /* Initialize MLC transport if this is the first MLC channel. */
   if (pd->channel_cnt==1)
   {
      if (claim_pp(pd->open_fd))
         goto bugout;

      /* Negotiate ECP mode. */
      m = IEEE1284_MODE_ECPSWE;
      if (ioctl(pd->open_fd, PPNEGOT, &m)) 
      {
         BUG("unable to negotiate %s ECP mode: %m\n", pd->uri);
         goto bugout;
      }

      /* Enable MLC mode with ECP channel-77. */
      ecp_write_addr(pd->open_fd, 78);
      ecp_write(pd->open_fd, "\0", 1);
      ecp_write_addr(pd->open_fd, 77);

      /* MLC initialize */
      if (MlcInit(pc, pd->open_fd) != 0)
         goto bugout;

      /* Reset transport attributes for all channels. */
      for (i=0; i<HPMUD_CHANNEL_MAX; i++)
         memset(&pd->channel[i].ta, 0 , sizeof(transport_attributes));

      pd->mlc_fd = pd->open_fd;
      pd->mlc_up=1;

   } /* if (pDev->ChannelCnt==1) */
 
   if (MlcConfigSocket(pc, pd->mlc_fd))
      goto bugout;

   if (MlcOpenChannel(pc, pd->mlc_fd))
      goto bugout;

   pc->rcnt = pc->rindex = 0;

   stat = HPMUD_R_OK;

bugout:
   return stat;  
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_mlc_channel_close(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   enum HPMUD_RESULT stat = HPMUD_R_OK;
   int m;

   if (pd->mlc_up)
   {
      if (MlcCloseChannel(pc, pd->mlc_fd))
         stat = HPMUD_R_IO_ERROR;
   }

   /* Remove MLC transport if this is the last MLC channel. */
   if (pd->channel_cnt==1)
   {
      if (pd->mlc_up)
      {
         if (MlcExit(pc, pd->mlc_fd))
            stat = HPMUD_R_IO_ERROR;
      }
      pd->mlc_up=0;

      ecp_write_addr(pd->mlc_fd, 78);     /* disable MLC mode with ECP channel-78 */
      ecp_write(pd->mlc_fd, "\0", 1);

      m = IEEE1284_MODE_NIBBLE;
      ioctl(pd->mlc_fd, PPNEGOT, &m);
      release_pp(pd->mlc_fd);

      /* Delay for batch scanning. */
      sleep(1);
   }

   return stat;
}

/*******************************************************************************************************************************
 * Parallel port dot4_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_dot4_channel_open(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;
   int i, m;

   /* Initialize MLC transport if this is the first MLC channel. */
   if (pd->channel_cnt==1)
   {
      if (claim_pp(pd->open_fd))
         goto bugout;

      /* Negotiate ECP mode. */
      m = IEEE1284_MODE_ECPSWE;
      if (ioctl(pd->open_fd, PPNEGOT, &m)) 
      {
         BUG("unable to negotiate %s ECP mode: %m\n", pd->uri);
         goto bugout;
      }

      /* Enable MLC mode with ECP channel-77. */
      ecp_write_addr(pd->open_fd, 78);
      ecp_write(pd->open_fd, "\0", 1);
      ecp_write_addr(pd->open_fd, 77);

      /* DOT4 initialize */
      if (Dot4Init(pc, pd->open_fd) != 0)
         goto bugout;

      /* Reset transport attributes for all channels. */
      for (i=0; i<HPMUD_CHANNEL_MAX; i++)
         memset(&pd->channel[i].ta, 0 , sizeof(transport_attributes));

      pd->mlc_fd = pd->open_fd;
      pd->mlc_up=1;

   } /* if (pDev->ChannelCnt==1) */

   if (Dot4GetSocket(pc, pd->mlc_fd))
      goto bugout;

   if (Dot4OpenChannel(pc, pd->mlc_fd))
      goto bugout;

   pc->rcnt = pc->rindex = 0;

   stat = HPMUD_R_OK;

bugout:
   return stat;  
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) pp_dot4_channel_close(mud_channel *pc)
{
   mud_device *pd = &msp->device[pc->dindex];
   enum HPMUD_RESULT stat = HPMUD_R_OK;
   int m;

   if (pd->mlc_up)
   {
      if (Dot4CloseChannel(pc, pd->mlc_fd))
         stat = HPMUD_R_IO_ERROR;
   }

   /* Remove MLC transport if this is the last MLC channel. */
   if (pd->channel_cnt==1)
   {
      if (pd->mlc_up)
      {
         if (Dot4Exit(pc, pd->mlc_fd))
            stat = HPMUD_R_IO_ERROR;
      }
      pd->mlc_up=0;

      ecp_write_addr(pd->mlc_fd, 78);     /* disable MLC mode with ECP channel-78 */
      ecp_write(pd->mlc_fd, "\0", 1);

      m = IEEE1284_MODE_NIBBLE;
      ioctl(pd->mlc_fd, PPNEGOT, &m);
      release_pp(pd->mlc_fd);

      /* Delay for batch scanning. */
      sleep(1);
   }

   return stat;
}

/*******************************************************************************************************************************
 * Parallel port probe devices, walk the parallel port bus(s) looking for HP products.
 */

int __attribute__ ((visibility ("hidden"))) pp_probe_devices(char *lst, int lst_size, int *cnt)
{
   struct hpmud_model_attributes ma;
   char dev[HPMUD_LINE_SIZE];
   char rmodel[128];
   char model[128];
   char id[1024];
   int i, size=0, fd, m;

   for (i=0; i < 4; i++)
   {
      sprintf(dev, "/dev/parport%d", i);

      if ((fd = open(dev, O_RDONLY | O_NOCTTY)) < 0)            
         continue;

      /* Silently check the port for valid device (no syslog errors). */
      if (ioctl(fd, PPGETMODES, &m) == 0)
      {
         if (claim_pp(fd) == 0)
         {
            if (device_id(fd, id, sizeof(id)) > 0 && is_hp(id))
            {
               hpmud_get_model(id, model, sizeof(model));
               hpmud_get_raw_model(id, rmodel, sizeof(rmodel));
               snprintf(dev, sizeof(dev), "hp:/par/%s?device=/dev/parport%d", model, i);

               /* See if device is supported by hplip. */
               hpmud_query_model(dev, &ma); 
               if (ma.support != HPMUD_SUPPORT_TYPE_HPLIP)
               {
                  release_pp(fd);
                  close(fd);
                  BUG("ignoring %s support=%d\n", dev, ma.support);
                  continue;           /* ignor, not supported */
               }

               if (strncasecmp(rmodel, "hp ", 3) == 0)
                  size += sprintf(lst+size,"direct %s \"HP %s\" \"HP %s LPT parport%d HPLIP\" \"%s\"\n", dev, &rmodel[3], &rmodel[3], i, id);
               else
                  size += sprintf(lst+size,"direct %s \"HP %s\" \"HP %s LPT parport%d HPLIP\" \"%s\"\n", dev, rmodel, rmodel, i, id);
               *cnt+=1;
            }
            release_pp(fd);
         }
         else
         {
            BUG("unable to probe %s: %m\n", dev);  /* device is busy */
         }
      }
      close(fd);
   }
   return size;
}

enum HPMUD_RESULT hpmud_make_par_uri(const char *dnode, char *uri, int uri_size, int *bytes_read)
{
   char model[128];
   char id[1024];
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;
   int fd=-1, m;

   DBG("[%d] hpmud_make_par_uri() dnode=%s\n", getpid(), dnode);

   *bytes_read=0;

   uri[0]=0;

   if ((fd = open(dnode, O_RDONLY | O_NOCTTY)) < 0) 
   {
      BUG("unable to open %s: %m\n", dnode);           
      goto bugout;
   }

   if (ioctl(fd, PPGETMODES, &m))
   {
      BUG("unable to make uri %s: %m\n", dnode);
      goto bugout;
   }

   if (claim_pp(fd))
   {
      BUG("unable to make uri %s: %m\n", dnode);  /* device is busy */
      goto bugout;
   }

   if (device_id(fd, id, sizeof(id)) > 0 && is_hp(id))
   {
      hpmud_get_model(id, model, sizeof(model));
      *bytes_read = snprintf(uri, uri_size, "hp:/par/%s?device=%s", model, dnode);
   }
   release_pp(fd);

   stat = HPMUD_R_OK;

bugout:
   if (fd >= 0)
      close(fd);
   return stat;
}

#endif /* HAVE_PPORT */
