/*****************************************************************************\

  mlc.c - MLC support for multi-point tranport driver
 
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

#include "hpmud.h"
#include "hpmudi.h"

int __attribute__ ((visibility ("hidden"))) cut_buf(mud_channel *pc, char *buf, int size)
{
   int len;

   if (pc->rcnt > size)
   {
      /* Return part of rbuf. */
      len = size;
      memcpy(buf, &pc->rbuf[pc->rindex], len);
      pc->rindex += len;
      pc->rcnt -= len;
   }
   else
   {
      /* Return all of rbuf. */
      len = pc->rcnt;
      memcpy(buf, &pc->rbuf[pc->rindex], len);
      pc->rindex = pc->rcnt = 0;
   }

   return len;
} 

/* Write command reply back to peripheral. */
static int MlcForwardReply(mud_channel *pc, int fd, unsigned char *buf, int size)
{
   mud_device *pd = &msp->device[pc->dindex];
   int len=0;

   if ((len = (pd->vf.write)(fd, buf, size, HPMUD_EXCEPTION_TIMEOUT)) != size)
   {
      BUG("unable to MlcForwarReply: %m\n");
   }   
   return len;
}

/* Execute command from peripheral. */
static int MlcExecReverseCmd(mud_channel *pc, int fd, unsigned char *buf)
{
   mud_device *pd = &msp->device[pc->dindex];
   mud_channel *out_of_bound_channel;
   MLCCmd *pCmd;
   MLCReply *pReply;
   MLCCredit *pCredit;
   MLCCreditReply *pCreditReply;
   MLCCreditRequest *pCreditReq;
   MLCCreditRequestReply *pCreditReqReply;
   MLCError *pError;
   int len, size;
   static int cnt;

   pCmd = (MLCCmd *)buf;

   /* See if this packet is a command packet. */
   if (!(pCmd->h.hsid == 0 && pCmd->h.psid == 0))
   {
      if (pCmd->h.hsid == pCmd->h.psid)
      {
         /* Got a valid data packet handle it. This can happen when channel_read timeouts and p2hcredit=1. */
         out_of_bound_channel = &pd->channel[pCmd->h.hsid];

         if (out_of_bound_channel->ta.p2hcredit <= 0)
         {
            BUG("invalid data packet credit=%d\n", out_of_bound_channel->ta.p2hcredit);
            return 0;
         }

         size = ntohs(pCmd->h.length) - sizeof(MLCHeader);
         if (size > (HPMUD_BUFFER_SIZE - out_of_bound_channel->rcnt))
         {
            BUG("invalid data packet size=%d\n", size);
            return 0;
         }
         memcpy(&out_of_bound_channel->rbuf[out_of_bound_channel->rcnt], buf+sizeof(MLCHeader), size);
         out_of_bound_channel->rcnt += size;
         if (pCmd->h.credit)
            out_of_bound_channel->ta.h2pcredit += pCmd->h.credit;  /* note, piggy back credit is 1 byte wide */ 
         out_of_bound_channel->ta.p2hcredit--; /* one data packet was read, decrement credit count */
      }
      else
      {
         len = ntohs(pCmd->h.length);
         BUG("unsolicited data packet: hsid=%x, psid=%x, length=%d, credit=%d, status=%x\n", pCmd->h.hsid,
                                     pCmd->h.psid, len, pCmd->h.credit, pCmd->h.status);
         DBG_DUMP(buf, len);
      }
      return 0;  
   }

   /* Process any command. */
   switch (pCmd->cmd)
   {
      case MLC_CREDIT:
         pCredit = (MLCCredit *)buf;
         out_of_bound_channel = &pd->channel[pCredit->hsocket];
         out_of_bound_channel->ta.h2pcredit += ntohs(pCredit->credit);
         pCreditReply = (MLCCreditReply *)buf;
         pCreditReply->h.length = htons(sizeof(MLCCreditReply));
         pCreditReply->cmd |= 0x80;
         pCreditReply->result = 0;
         MlcForwardReply(pc, fd, (unsigned char *)pCreditReply, sizeof(MLCCreditReply)); 
         break;
      case MLC_CREDIT_REQUEST:
         pCreditReq = (MLCCreditRequest *)buf;
         if (cnt++ < 5)         
            BUG("unexpected MLCCreditRequest: cmd=%x, hid=%x, pid=%x, credit=%d\n", pCreditReq->cmd,
                                         pCreditReq->hsocket, pCreditReq->psocket, ntohs(pCreditReq->credit));
         pCreditReqReply = (MLCCreditRequestReply *)buf;
         pCreditReqReply->h.length = htons(sizeof(MLCCreditRequestReply));
         pCreditReqReply->cmd |= 0x80;
         pCreditReqReply->result = 0;
         pCreditReqReply->credit = 0;
         MlcForwardReply(pc, fd, (unsigned char *)pCreditReqReply, sizeof(MLCCreditRequestReply)); 
         break;
      case MLC_ERROR:
         pError = (MLCError *)buf;
         BUG("unexpected MLCError: cmd=%x, result=%x\n", pError->cmd, pError->result);
         return 1;
      default:
         pReply = (MLCReply *)buf;
         BUG("unexpected command: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
         pReply->h.length = htons(sizeof(MLCReply));
         pReply->cmd |= 0x80;
         pReply->result = 1;
         MlcForwardReply(pc, fd, (unsigned char *)pReply, sizeof(MLCReply)); 
         break;
   }
   return 0;
}

/* Get command from peripheral and processes the reverse command. */
int __attribute__ ((visibility ("hidden"))) MlcReverseCmd(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];   
   int stat=0, len, size;
   unsigned int pklen;
   unsigned char *pBuf;
   MLCReply *pPk;

   pPk = (MLCReply *)buf;

   pBuf = buf;

   /* Read packet header. */
   size = sizeof(MLCHeader);
   while (size > 0)
   {
      if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
      {
         BUG("unable to read MlcReverseCmd header: %m\n");
         stat = 1;
         goto bugout;
      }
      size-=len;
      pBuf+=len;
   }

   /* Determine packet size. */
   if ((pklen = ntohs(pPk->h.length)) > sizeof(buf))
   {
      BUG("invalid MlcReverseCmd packet size: size=%d\n", pklen);
      stat = 1;
      goto bugout;
   }

   /* Read packet data field. */
   size = pklen - sizeof(MLCHeader);
   while (size > 0)
   {
      if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
      {
         BUG("unable to read MlcReverseCmd data: %m\n");
         stat = 1;
         goto bugout;
      }
      size-=len;
      pBuf+=len;
   }

   stat = MlcExecReverseCmd(pc, fd, buf);

bugout:
   return stat;
}

/*
 * Get command reply from peripheral. Waits for reply then returns. Processes any reverse commands
 * while waiting for a reply.
 */
static int MlcReverseReply(mud_channel *pc, int fd, unsigned char *buf, int bufsize)
{
   mud_device *pd = &msp->device[pc->dindex];
   int stat=0, len, size, pklen;
   unsigned char *pBuf;
   MLCReply *pPk;

   pPk = (MLCReply *)buf;
   
   while (1)
   {
      pBuf = buf;

      /* Read packet header. */
      size = sizeof(MLCHeader);
      while (size > 0)
      {
         if ((len = (pd->vf.read)(fd, pBuf, size, 4000000)) < 0)   /* wait 4 seconds, same as dot4 */
         {
            BUG("unable to read MlcReverseReply header: %m bytesRead=%zd\n", sizeof(MLCHeader)-size);
            stat = 2;  /* short timeout */
            goto bugout;
         }
         size-=len;
         pBuf+=len;
      }

      /* Determine packet size. */
      pklen = ntohs(pPk->h.length);
      if (pklen < 0 || pklen > bufsize)
      {
         BUG("invalid MlcReverseReply packet size: size=%d, buf=%d\n", pklen, bufsize);
         stat = 1;
         goto bugout;
      }

      if (pklen == 0)
      {
         /* Got invalid MLC header from peripheral, try this "off-by-one" firmware hack (ie: OJ600). */
         BUG("trying MlcReverseReply firmware hack\n");
         memcpy(buf, &buf[1], sizeof(MLCHeader)-1);
         pklen = ntohs(pPk->h.length);
         if (pklen <= 0 || pklen > bufsize)
         {
            BUG("invalid MlcReverseReply packet size: size=%d, buf=%d\n", pklen, bufsize);
            stat = 1;
            goto bugout;
         }
         if ((len = (pd->vf.read)(fd, --pBuf, 1, 1000000)) < 0)   /* wait 1 second */
         {
            BUG("unable to read MlcReverseReply header: %m\n");
            stat = 1;
            goto bugout;
         }
         pBuf++;
         DBG_DUMP(buf, sizeof(MLCHeader));
      }

      /* Read packet data field. */
      size = pklen - sizeof(MLCHeader);
      while (size > 0)
      {
         if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
         {
            BUG("unable to read MlcReverseReply data: %m exp=%zd act=%zd\n", pklen-sizeof(MLCHeader), pklen-sizeof(MLCHeader)-size);
            stat = 1;
            goto bugout;
         }
         size-=len;
         pBuf+=len;
      }

      /* Check for reply. */
      if (pPk->cmd & 0x80)
         break;

      stat = MlcExecReverseCmd(pc, fd, buf);

      if (stat != 0)
         break;

   } /* while (1) */

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcInit(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n, cnt;
   MLCInit *pCmd;
   MLCInitReply *pReply;

   memset(buf, 0, sizeof(MLCInit));
   pCmd = (MLCInit *)buf;
   n = sizeof(MLCInit);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_INIT;
   pCmd->rev = 3;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MLCInit: %m\n");
      stat = 1;
      goto bugout;
   }

   cnt=0;
   while(1)
   {
      stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
      pReply = (MLCInitReply *)buf;

      if ((stat != 0) || (pReply->cmd != (0x80 | MLC_INIT)) || (pReply->result != 0))
      {
         if (errno == EIO && cnt<1)
         {
            /* hack for usblp.c 2.6.5 */
            BUG("invalid MLCInitReply retrying...\n");
            sleep(1);   
            cnt++;
            continue;
         }
         if (stat == 2 && cnt<1)
         {
            /* hack for Tahoe */
            BUG("invalid MLCInitReply retrying command...\n");
            memset(buf, 0, sizeof(MLCInit));
            n = sizeof(MLCInit);
            pCmd->h.length = htons(n);
            pCmd->cmd = MLC_INIT;
            pCmd->rev = 3;
            (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT);
            cnt++;
            continue;
         }
         BUG("invalid MLCInitReply: cmd=%x, result=%x\n, revision=%x\n", pReply->cmd, pReply->result, pReply->rev);
         stat = 1;
         goto bugout;
      }
      break;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcExit(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCExit *pCmd;
   MLCExitReply *pReply;

   memset(buf, 0, sizeof(MLCExit));
   pCmd = (MLCExit *)buf;
   n = sizeof(MLCExit);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_EXIT;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MLCExit: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCExitReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_EXIT)) || (pReply->result != 0))
   {
      BUG("invalid MLCExitReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcConfigSocket(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCConfigSocket *pCmd;
   MLCConfigSocketReply *pReply;

   if (pc->ta.h2psize > 0)
     return stat;   /* already got host/peripheral packet sizes */

   memset(buf, 0, sizeof(MLCConfigSocket));
   pCmd = (MLCConfigSocket *)buf;
   n = sizeof(MLCConfigSocket);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_CONFIG_SOCKET;
   pCmd->socket = pc->sockid;
   pCmd->h2psize = htons(HPMUD_BUFFER_SIZE);
   pCmd->p2hsize = htons(HPMUD_BUFFER_SIZE);
   pCmd->status = 0;   /* status level?? */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MLCConfigSocket: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCConfigSocketReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_CONFIG_SOCKET)) || (pReply->result != 0))
   {
      BUG("invalid MLCConfigSocketReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.h2psize = ntohs(pReply->h2psize);
   pc->ta.p2hsize = ntohs(pReply->p2hsize);

bugout:
   return stat;
}

/* Write data to peripheral. */
int __attribute__ ((visibility ("hidden"))) MlcForwardData(mud_channel *pc, int fd, const void *buf, int size, int usec_timeout)
{
   mud_device *pd = &msp->device[pc->dindex];
   int stat=0, len, n;
   MLCHeader h;

   memset(&h, 0, sizeof(h));
   n = sizeof(MLCHeader) + size;
   h.length = htons(n);
   h.hsid = pc->sockid;
   h.psid = pc->sockid;
      
   if ((len = (pd->vf.write)(fd, &h, sizeof(MLCHeader),  usec_timeout)) != sizeof(MLCHeader))
   {
      BUG("unable to write MlcForwardData header: %m\n");
      stat = 1;
      goto bugout;
   }

   if ((len = (pd->vf.write)(fd, buf, size, usec_timeout)) != size)
   {
      BUG("unable to write MlcForwardData: %m\n");
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

/* Read data from peripheral. */
int __attribute__ ((visibility ("hidden"))) MlcReverseData(mud_channel *pc, int fd, void *buf, int length, int usec_timeout)
{
   mud_device *pd = &msp->device[pc->dindex];
   mud_channel *out_of_bound_channel;
   int len, size, total;
   MLCHeader *pPk;

   pPk = (MLCHeader *)buf;

   while (1)
   {
      total = 0;

      /* Read packet header. */
      size = sizeof(MLCHeader);
      while (size > 0)
      {
         /* Use requested client timeout until we start reading. */
         if (total == 0)
            len = (pd->vf.read)(fd, buf+total, size, usec_timeout);
         else
            len = (pd->vf.read)(fd, buf+total, size, HPMUD_EXCEPTION_TIMEOUT);

         if (len < 0)
         {
            /* Got a timeout, if exception timeout or timeout occured after read started thats an error. */
            if (usec_timeout >= HPMUD_EXCEPTION_TIMEOUT || total > 0)
               BUG("unable to read MlcReverseData header: %m %s\n", pd->uri);
            goto bugout;
         }
         size-=len;
         total+=len;
      }

      /* Determine data size. */
      size = ntohs(pPk->length) - sizeof(MLCHeader);

      if (size > length)
      {
         BUG("invalid MlcReverseData size: size=%d, buf=%d\n", size, length);
         goto bugout;
      } 

      /* Make sure data packet is for this channel. */
      if (pPk->hsid != pc->sockid && pPk->psid != pc->sockid)
      {
         if (pPk->hsid == 0 && pPk->psid == 0)
         {
            /* Ok, got a command channel packet instead of a data packet, handle it... */
            while (size > 0)
            {
               if ((len = (pd->vf.read)(fd, buf+total, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
               {
                  BUG("unable to read MlcReverseData command: %m\n");
                  goto bugout;
               }
               size-=len;
               total=len;
            }
            MlcExecReverseCmd(pc, fd, buf);
            continue;   /* try again for data packet */
         }
         else if (pPk->hsid == pPk->psid)
         {
            /* Got a valid data packet for another channel handle it. This can happen when ReadData timeouts and p2hcredit=1. */
            out_of_bound_channel = &pd->channel[pPk->hsid];
            unsigned char *pBuf;

            if (out_of_bound_channel->ta.p2hcredit <= 0)
            {
               BUG("invalid data packet credit=%d\n", out_of_bound_channel->ta.p2hcredit);
               goto bugout;
            }

            if (size > (HPMUD_BUFFER_SIZE - out_of_bound_channel->rcnt))
            {
               BUG("invalid data packet size=%d\n", size);
               goto bugout;
            }
            
            total = 0;
            pBuf = &out_of_bound_channel->rbuf[out_of_bound_channel->rcnt];
            while (size > 0)
            {
               if ((len = (pd->vf.read)(fd, pBuf+total, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
               {
                  BUG("unable to read MlcReverseData: %m\n");
                  goto bugout;
               }
               size-=len;
               total+=len;
            }

            out_of_bound_channel->rcnt += total;
            if (pPk->credit)
               out_of_bound_channel->ta.h2pcredit += pPk->credit;  /* note, piggy back credit is 1 byte wide */ 
            out_of_bound_channel->ta.p2hcredit--; /* one data packet was read, decrement credit count */
            continue;   /* try again for data packet */
         }
         else
         {
            MLCCmd *pCmd = (MLCCmd *)buf;
            BUG("invalid MlcReverseData state: exp hsid=%x, act hsid=%x, psid=%x, length=%d, credit=%d, status=%x, cmd=%x\n", pc->sockid, 
                                                pPk->hsid, pPk->psid, ntohs(pPk->length), pPk->credit, pPk->status, pCmd->cmd);
            goto bugout;
         }
      }

      if (pPk->credit)
      {
         pc->ta.h2pcredit += pPk->credit;  /* note, piggy back credit is 1 byte wide */ 
      }

      total = 0;  /* eat packet header */
   
      /* Read packet data field with exception_timeout. */
      while (size > 0)
      {
         if ((len = (pd->vf.read)(fd, buf+total, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
         {
            BUG("unable to read MlcReverseData: %m\n");
            goto bugout;
         }
         size-=len;
         total+=len;
      }
      break; /* done reading data packet */
   }  /* while (1) */

bugout:
   return total;
}

int __attribute__ ((visibility ("hidden"))) MlcOpenChannel(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCOpenChannel *pCmd;
   MLCOpenChannelReply *pReply;

   memset(buf, 0, sizeof(MLCOpenChannel));
   pCmd = (MLCOpenChannel *)buf;
   n = sizeof(MLCOpenChannel);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_OPEN_CHANNEL;
   pCmd->hsocket = pc->sockid;   /* assume static socket ids */    
   pCmd->psocket = pc->sockid;
   pCmd->credit = htons(0);             /* credit sender will accept from receiver (set by MlcDevice::ReadData) */
   //   SetH2PCredit(0);                    /* initialize sender to receiver credit */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MlcOpenChannel: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCOpenChannelReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_OPEN_CHANNEL)) || (pReply->result != 0))
   {
      BUG("invalid MlcOpenChannelReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.h2pcredit = ntohs(pReply->credit);

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcCloseChannel(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCCloseChannel *pCmd;
   MLCCloseChannelReply *pReply;

   memset(buf, 0, sizeof(MLCCloseChannel));
   pCmd = (MLCCloseChannel *)buf;
   n = sizeof(MLCCloseChannel);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_CLOSE_CHANNEL;
   pCmd->hsocket = pc->sockid;   /* assume static socket ids */    
   pCmd->psocket = pc->sockid;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MlcCloseChannel: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCCloseChannelReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_CLOSE_CHANNEL)) || (pReply->result != 0))
   {
      BUG("invalid MlcCloseChannelReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcCredit(mud_channel *pc, int fd, unsigned short credit)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCCredit *pCmd;
   MLCCreditReply *pReply;

   memset(buf, 0, sizeof(MLCCredit));
   pCmd = (MLCCredit *)buf;
   n = sizeof(MLCCredit);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_CREDIT;
   pCmd->hsocket = pc->sockid;   /* assume static socket ids */    
   pCmd->psocket = pc->sockid;
   pCmd->credit = htons(credit);                /* set peripheral to host credit */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MlcCredit: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCCreditReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_CREDIT)) || (pReply->result != 0))
   {
      BUG("invalid MlcCreditReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.p2hcredit += credit;

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) MlcCreditRequest(mud_channel *pc, int fd, unsigned short credit)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   MLCCreditRequest *pCmd;
   MLCCreditRequestReply *pReply;

   memset(buf, 0, sizeof(MLCCreditRequest));
   pCmd = (MLCCreditRequest *)buf;
   n = sizeof(MLCCreditRequest);
   pCmd->h.length = htons(n);
   pCmd->cmd = MLC_CREDIT_REQUEST;
   pCmd->hsocket = pc->sockid;   /* assume static socket ids */    
   pCmd->psocket = pc->sockid;
   pCmd->credit = htons(credit);                /* request host to peripheral credit */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write MlcCreditRequest: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = MlcReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (MLCCreditRequestReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | MLC_CREDIT_REQUEST)) || (pReply->result != 0))
   {
      BUG("invalid MlcCreditRequestReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.h2pcredit += ntohs(pReply->credit);

bugout:
   return stat;
}



