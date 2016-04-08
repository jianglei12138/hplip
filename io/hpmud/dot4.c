/*****************************************************************************\

  dot4.c - 1284.4 support multi-point tranport driver  
 
  (c) 2005-2007 Copyright HP Development Company, LP

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

/*
 * This 1284.4 implementation does not support "Multiple Outstanding Transactions" which is optional.
 */

/* Write command reply back to peripheral. */
static int Dot4ForwardReply(mud_channel *pc, int fd, unsigned char *buf, int size)
{
   mud_device *pd = &msp->device[pc->dindex];
   int len=0;

   if ((len = (pd->vf.write)(fd, buf, size, HPMUD_EXCEPTION_TIMEOUT)) != size)
   {
      BUG("unable to Dot4ForwarReply: %m\n");
   }   
   return len;
}

/* Execute command from peripheral. */
static int Dot4ExecReverseCmd(mud_channel *pc, int fd, unsigned char *buf)
{
   mud_device *pd = &msp->device[pc->dindex];
   mud_channel *out_of_bound_channel;
   DOT4Cmd *pCmd;
   DOT4Reply *pReply;
   DOT4Credit *pCredit;
   DOT4CreditReply *pCreditReply;
   DOT4CreditRequest *pCreditReq;
   DOT4CreditRequestReply *pCreditReqReply;
   DOT4Error *pError;
   int len, size;
   unsigned char socket;
   static int cnt;

   pCmd = (DOT4Cmd *)buf;

   /* See if this packet is a command packet. */
   if (!(pCmd->h.psid == 0 && pCmd->h.ssid == 0))
   {
      if (pCmd->h.psid == pCmd->h.ssid)
      {
         /* Got a valid data packet handle it. This can happen when channel_read timeouts and p2hcredit=1. */
         out_of_bound_channel = &pd->channel[pCmd->h.psid];

         if (out_of_bound_channel->ta.p2hcredit <= 0)
         {
            BUG("invalid data packet credit=%d\n", out_of_bound_channel->ta.p2hcredit);
            return 0;
         }

         size = ntohs(pCmd->h.length) - sizeof(DOT4Header);
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
         BUG("unsolicited data packet: psid=%x, ssid=%x, length=%d, credit=%d, status=%x\n", pCmd->h.psid,
                                     pCmd->h.ssid, len, pCmd->h.credit, pCmd->h.control);
         DBG_DUMP(buf, len);
      }
      return 0;  
   }

   /* Process any command. */
   switch (pCmd->cmd)
   {
      case DOT4_CREDIT:
         pCredit = (DOT4Credit *)buf;
         out_of_bound_channel = &pd->channel[pCredit->psocket];
         out_of_bound_channel->ta.h2pcredit += ntohs(pCredit->credit);
         pCreditReply = (DOT4CreditReply *)buf;
         pCreditReply->h.length = htons(sizeof(DOT4CreditReply));
         pCreditReply->h.credit = 1;       /* transaction credit for next command */
         pCreditReply->h.control = 0;
         pCreditReply->cmd |= 0x80;
         pCreditReply->result = 0;
         pCreditReply->psocket = out_of_bound_channel->sockid;
         pCreditReply->ssocket = out_of_bound_channel->sockid;
         Dot4ForwardReply(pc, fd, (unsigned char *)pCreditReply, sizeof(DOT4CreditReply)); 
         break;
      case DOT4_CREDIT_REQUEST:
         pCreditReq = (DOT4CreditRequest *)buf;
         if (cnt++ < 5)         
            BUG("unexpected DOT4CreditRequest: cmd=%x, hid=%x, pid=%x, maxcredit=%d\n", pCreditReq->cmd,
                                         pCreditReq->psocket, pCreditReq->ssocket, ntohs(pCreditReq->maxcredit));
         socket = pCreditReq->ssocket;
         pCreditReqReply = (DOT4CreditRequestReply *)buf;
         pCreditReqReply->h.length = htons(sizeof(DOT4CreditRequestReply));
         pCreditReqReply->h.credit = 1;       /* transaction credit for next command */
         pCreditReqReply->h.control = 0;
         pCreditReqReply->cmd |= 0x80;
         pCreditReqReply->result = 0;
         pCreditReqReply->psocket = socket;
         pCreditReqReply->ssocket = socket;
         pCreditReqReply->credit = 0;
         Dot4ForwardReply(pc, fd, (unsigned char *)pCreditReqReply, sizeof(DOT4CreditRequestReply)); 
         break;
      case DOT4_ERROR:
         pError = (DOT4Error *)buf;
         BUG("unexpected DOT4Error: cmd=%x, psocket=%d, ssocket=%d, error=%x\n", pError->cmd, pError->psocket, pError->ssocket, pError->error);
         return 1;
      default:
         pReply = (DOT4Reply *)buf;
         BUG("unexpected command: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
         pReply->h.length = htons(sizeof(DOT4Reply));
         pReply->h.credit = 1;       /* transaction credit for next command */
         pReply->h.control = 0;
         pReply->cmd |= 0x80;
         pReply->result = 1;
         Dot4ForwardReply(pc, fd, (unsigned char *)pReply, sizeof(DOT4Reply)); 
         break;
   }
   return 0;
}

/* Get command from peripheral and processes the reverse command. */
int __attribute__ ((visibility ("hidden"))) Dot4ReverseCmd(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];   
   int stat=0, len, size;
   unsigned int pklen;
   unsigned char *pBuf;
   DOT4Reply *pPk;

   pPk = (DOT4Reply *)buf;

   pBuf = buf;

   /* Read packet header. */
   size = sizeof(DOT4Header);
   while (size > 0)
   {
      if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
      {
         BUG("unable to read Dot4ReverseCmd header: %m\n");
         stat = 1;
         goto bugout;
      }
      size-=len;
      pBuf+=len;
   }

   /* Determine packet size. */
   if ((pklen = ntohs(pPk->h.length)) > sizeof(buf))
   {
      BUG("invalid Dot4ReverseCmd packet size: size=%d\n", pklen);
      stat = 1;
      goto bugout;
   }

   /* Read packet data field. */
   size = pklen - sizeof(DOT4Header);
   while (size > 0)
   {
      if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
      {
         BUG("unable to read Dot4ReverseCmd data: %m exp=%zd act=%zd\n", pklen-sizeof(DOT4Header), pklen-sizeof(DOT4Header)-size);
         stat = 1;
         goto bugout;
      }
      size-=len;
      pBuf+=len;
   }

   stat = Dot4ExecReverseCmd(pc, fd, buf);

bugout:
   return stat;
}

/*
 * Get command reply from peripheral. Waits for reply then returns. Processes any reverse commands
 * while waiting for a reply.
 */
static int Dot4ReverseReply(mud_channel *pc, int fd, unsigned char *buf, int bufsize)
{
   mud_device *pd = &msp->device[pc->dindex];
   int stat=0, len, size, pklen;
   unsigned char *pBuf;
   DOT4Reply *pPk;

   pPk = (DOT4Reply *)buf;
   
   while (1)
   {
      pBuf = buf;

      /* Read packet header. */
      size = sizeof(DOT4Header);
      while (size > 0)
      {
         if ((len = (pd->vf.read)(fd, pBuf, size, 4000000)) < 0)   /* wait 4 seconds, 2 fails on PS2575 1200dpi uncompressed scanning  */
         {
            BUG("unable to read Dot4ReverseReply header: %m bytesRead=%zd\n", sizeof(DOT4Header)-size);
            stat = 2;  /* short timeout */
            goto bugout;
         }
         size-=len;
         pBuf+=len;
      }

      /* Determine packet size. */
      pklen = ntohs(pPk->h.length);
      if (pklen <= 0 || pklen > bufsize)
      {
         BUG("invalid Dot4ReverseReply packet size: size=%d, buf=%d\n", pklen, bufsize);
         stat = 1;
         goto bugout;
      }

      /* Read packet data field. */
      size = pklen - sizeof(DOT4Header);
      while (size > 0)
      {
         if ((len = (pd->vf.read)(fd, pBuf, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
         {
            BUG("unable to read Dot4ReverseReply data: %m exp=%zd act=%zd\n", pklen-sizeof(DOT4Header), pklen-sizeof(DOT4Header)-size);
            stat = 1;
            goto bugout;
         }
         size-=len;
         pBuf+=len;
      }

      /* Check for reply. */
      if (pPk->cmd & 0x80)
         break;

      stat = Dot4ExecReverseCmd(pc, fd, buf);

      if (stat != 0)
         break;

   } /* while (1) */

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4Init(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n, cnt;
   DOT4Init *pCmd;
   DOT4InitReply *pReply;

   memset(buf, 0, sizeof(DOT4Init));
   pCmd = (DOT4Init *)buf;
   n = sizeof(DOT4Init);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;       /* transaction credit for reply */
   pCmd->cmd = DOT4_INIT;
   pCmd->rev = 0x20;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write DOT4Init: %m\n");
      stat = 1;
      goto bugout;
   }

   cnt=0;
   while(1)
   {
      stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
      pReply = (DOT4InitReply *)buf;

      if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_INIT)) || (pReply->result != 0))
      {
         if (errno == EIO && cnt<1)
         {
            /* hack for usblp.c 2.6.5 */
            BUG("invalid DOT4InitReply retrying...\n");
            sleep(1);   
            cnt++;
            continue;
         }
         if (stat == 2 && cnt<1)
         {
            /* hack for Fullhouse, Swami and Northstar */
            BUG("invalid DOT4InitReply retrying command...\n");
            memset(buf, 0, sizeof(DOT4Init));
            n = sizeof(DOT4Init);
            pCmd->h.length = htons(n);
            pCmd->h.credit = 1;       /* transaction credit for reply */
            pCmd->cmd = DOT4_INIT;
            pCmd->rev = 0x20;
            (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT);
            cnt++;
            continue;
         }
         BUG("invalid DOT4InitReply: cmd=%x, result=%x\n, revision=%x\n", pReply->cmd, pReply->result, pReply->rev);
         stat = 1;
         goto bugout;
      }
      break;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4Exit(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4Exit *pCmd;
   DOT4ExitReply *pReply;

   memset(buf, 0, sizeof(DOT4Exit));
   pCmd = (DOT4Exit *)buf;
   n = sizeof(DOT4Exit);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_EXIT;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write DOT4Exit: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4ExitReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_EXIT)) || (pReply->result != 0))
   {
      BUG("invalid DOT4ExitReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4GetSocket(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4GetSocket *pCmd;
   DOT4GetSocketReply *pReply;

   memset(buf, 0, sizeof(DOT4GetSocket));
   pCmd = (DOT4GetSocket *)buf;
   n = sizeof(DOT4GetSocket);
   len = strlen(pc->sn);
   memcpy(buf+sizeof(DOT4GetSocket), pc->sn, len);
   n += len;
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_GET_SOCKET;
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write DOT4GetSocket: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4GetSocketReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_GET_SOCKET)) || (pReply->result != 0))
   {
      BUG("invalid DOT4GetSocketReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->sockid = pReply->socket;

   if (pc->sockid != pc->index)
      BUG("invalid sockid match sockid=%d index=%d\n", pc->sockid, pc->index);

bugout:
   return stat;
}

/* Write data to peripheral. */
int __attribute__ ((visibility ("hidden"))) Dot4ForwardData(mud_channel *pc, int fd, const void *buf, int size, int usec_timeout)
{
   mud_device *pd = &msp->device[pc->dindex];
   int stat=0, len, n;
   DOT4Header h;

   memset(&h, 0, sizeof(h));
   n = sizeof(DOT4Header) + size;
   h.length = htons(n);
   h.psid = pc->sockid;
   h.ssid = pc->sockid;
      
   if ((len = (pd->vf.write)(fd, &h, sizeof(DOT4Header), usec_timeout)) != sizeof(DOT4Header))
   {
      BUG("unable to write Dot4ForwardData header: %m\n");
      stat = 1;
      goto bugout;
   }

   if ((len = (pd->vf.write)(fd, buf, size, usec_timeout)) != size)
   {
      BUG("unable to write Dot4ForwardData: %m\n");
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

/* Read data from peripheral. */
int __attribute__ ((visibility ("hidden"))) Dot4ReverseData(mud_channel *pc, int fd, void *buf, int length, int usec_timeout)
{
   mud_device *pd = &msp->device[pc->dindex];
   mud_channel *out_of_bound_channel;
   int len, size, total;
   DOT4Header *pPk;

   pPk = (DOT4Header *)buf;

   while (1)
   {
      total = 0;

      /* Read packet header. */
      size = sizeof(DOT4Header);
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
               BUG("unable to read Dot4ReverseData header: %m %s\n", pd->uri);
            goto bugout;
         }
         size-=len;
         total+=len;
      }

      /* Determine data size. */
      size = ntohs(pPk->length) - sizeof(DOT4Header);

      if (size > length)
      {
         BUG("invalid Dot4ReverseData size: size=%d, buf=%d\n", size, length);
         goto bugout;
      } 

      /* Make sure data packet is for this channel. */
      if (pPk->psid != pc->sockid && pPk->ssid != pc->sockid)
      {
         if (pPk->psid == 0 && pPk->ssid == 0)
         {
            /* Ok, got a command channel packet instead of a data packet, handle it... */
            while (size > 0)
            {
               if ((len = (pd->vf.read)(fd, buf+total, size, HPMUD_EXCEPTION_TIMEOUT)) < 0)
               {
                  BUG("unable to read Dot4ReverseData command: %m\n");
                  goto bugout;
               }
               size-=len;
               total=len;
            }
            Dot4ExecReverseCmd(pc, fd, buf);
            continue;   /* try again for data packet */
         }
         else if (pPk->psid == pPk->ssid)
         {
            /* Got a valid data packet for another channel handle it. This can happen when ReadData timeouts and p2hcredit=1. */
            out_of_bound_channel = &pd->channel[pPk->psid];
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
            DOT4Cmd *pCmd = (DOT4Cmd *)buf;
            BUG("invalid Dot4ReverseData state: unexpected packet psid=%x, ssid=%x, cmd=%x\n", pPk->psid, pPk->ssid, pCmd->cmd);
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
            BUG("unable to read Dot4ReverseData: %m\n");
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

int __attribute__ ((visibility ("hidden"))) Dot4OpenChannel(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4OpenChannel *pCmd;
   DOT4OpenChannelReply *pReply;

   memset(buf, 0, sizeof(DOT4OpenChannel));
   pCmd = (DOT4OpenChannel *)buf;
   n = sizeof(DOT4OpenChannel);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_OPEN_CHANNEL;
   pCmd->psocket = pc->sockid;
   pCmd->ssocket = pc->sockid;
   pCmd->maxp2s = htons(HPMUD_BUFFER_SIZE);  /* max primary to secondary packet size in bytes */
   pCmd->maxs2p = htons(HPMUD_BUFFER_SIZE);  /* max secondary to primary packet size in bytes */
   pCmd->maxcredit = htons(0xffff);          /* "unlimited credit" mode, give primary (sender) as much credit as possible */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write Dot4OpenChannel: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4OpenChannelReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_OPEN_CHANNEL)) || (pReply->result != 0))
   {
      BUG("invalid Dot4OpenChannelReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.h2psize = ntohs(pReply->maxp2s);
   pc->ta.p2hsize = ntohs(pReply->maxs2p);
   pc->ta.h2pcredit = ntohs(pReply->credit);

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4CloseChannel(mud_channel *pc, int fd)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4CloseChannel *pCmd;
   DOT4CloseChannelReply *pReply;

   memset(buf, 0, sizeof(DOT4CloseChannel));
   pCmd = (DOT4CloseChannel *)buf;
   n = sizeof(DOT4CloseChannel);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_CLOSE_CHANNEL;
   pCmd->psocket = pc->sockid;
   pCmd->ssocket = pc->sockid;

   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write Dot4CloseChannel: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4CloseChannelReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_CLOSE_CHANNEL)) || (pReply->result != 0))
   {
      BUG("invalid Dot4CloseChannelReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4Credit(mud_channel *pc, int fd, unsigned short credit)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4Credit *pCmd;
   DOT4CreditReply *pReply;

   memset(buf, 0, sizeof(DOT4Credit));
   pCmd = (DOT4Credit *)buf;
   n = sizeof(DOT4Credit);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_CREDIT;
   pCmd->psocket = pc->sockid;
   pCmd->ssocket = pc->sockid;
   pCmd->credit = htons(credit);                /* set peripheral to host credit */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write Dot4Credit: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4CreditReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_CREDIT)) || (pReply->result != 0))
   {
      BUG("invalid Dot4CreditReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.p2hcredit += credit;

bugout:
   return stat;
}

int __attribute__ ((visibility ("hidden"))) Dot4CreditRequest(mud_channel *pc, int fd, unsigned short credit)
{
   mud_device *pd = &msp->device[pc->dindex];
   unsigned char buf[HPMUD_BUFFER_SIZE];
   int stat=0, len, n;
   DOT4CreditRequest *pCmd;
   DOT4CreditRequestReply *pReply;

   memset(buf, 0, sizeof(DOT4CreditRequest));
   pCmd = (DOT4CreditRequest *)buf;
   n = sizeof(DOT4CreditRequest);
   pCmd->h.length = htons(n);
   pCmd->h.credit = 1;
   pCmd->cmd = DOT4_CREDIT_REQUEST;
   pCmd->psocket = pc->sockid;
   pCmd->ssocket = pc->sockid;
   //   pCmd->maxcredit = htons(credit);                /* request host to peripheral credit */
   pCmd->maxcredit = htons(0xffff);                /* request host to peripheral credit */
   
   if ((len = (pd->vf.write)(fd, pCmd, n, HPMUD_EXCEPTION_TIMEOUT)) != n)
   {
      BUG("unable to write Dot4CreditRequest: %m\n");
      stat = 1;
      goto bugout;
   }

   stat = Dot4ReverseReply(pc, fd, buf, sizeof(buf));
   pReply = (DOT4CreditRequestReply *)buf;

   if ((stat != 0) || (pReply->cmd != (0x80 | DOT4_CREDIT_REQUEST)) || (pReply->result != 0))
   {
      BUG("invalid Dot4CreditRequestReply: cmd=%x, result=%x\n", pReply->cmd, pReply->result);
      stat = 1;
      goto bugout;
   }

   pc->ta.h2pcredit += ntohs(pReply->credit);

bugout:
   return stat;
}

