/*****************************************************************************\

  dot4.h - 1284.4 support for multi-point transport driver 
 
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

#ifndef _DOT4_H
#define _DOT4_H
#include "mlc.h"

enum DOT4_COMMAND
{
  DOT4_INIT = MLC_INIT,
  DOT4_OPEN_CHANNEL = MLC_OPEN_CHANNEL,
  DOT4_CLOSE_CHANNEL = MLC_CLOSE_CHANNEL,
  DOT4_CREDIT = MLC_CREDIT,
  DOT4_CREDIT_REQUEST = MLC_CREDIT_REQUEST,
  DOT4_GET_SOCKET = 0x9,
  DOT4_GET_SERVICE = 0xa,
  DOT4_EXIT = MLC_EXIT,
  DOT4_ERROR = MLC_ERROR
};

/*
 * Note, following structures must be packed. The "pragma pack" statement is not recognized by all gcc compilers (ie: ARM based),
 * so we use __attribute__((packed)) instead.
 */

typedef struct
{
   unsigned char psid;   /* primary socket id (ie: host) */
   unsigned char ssid;   /* secondary socket id (ie: peripheral) */
   unsigned short length;   /* packet length (includes header) */ 
   unsigned char credit;   /* data packet credit, reserved if command */
   unsigned char control;  /* bit field: 0=normal */
} __attribute__((packed)) DOT4Header;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char rev;
} __attribute__((packed)) DOT4Init;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
   unsigned char rev;
} __attribute__((packed)) DOT4InitReply;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
} __attribute__((packed)) DOT4Exit;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
} __attribute__((packed)) DOT4ExitReply;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char psocket;      /* primary socket id */
   unsigned char ssocket;      /* secondary socket id */
   unsigned short maxp2s;      /* max primary to secondary packet size in bytes */
   unsigned short maxs2p;      /* max secondary to primary packet size in bytes */
   unsigned short maxcredit;   /* max outstanding credit */
} __attribute__((packed)) DOT4OpenChannel;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
   unsigned char psocket;
   unsigned char ssocket;
   unsigned short maxp2s;      /* max primary to secondary packet size in bytes */
   unsigned short maxs2p;      /* max secondary to primary packet size in bytes */
   unsigned short maxcredit;   /* max outstanding credit */
   unsigned short credit;
} __attribute__((packed)) DOT4OpenChannelReply;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char psocket;      /* primary socket id */
   unsigned char ssocket;      /* secondary socket id */
} __attribute__((packed)) DOT4CloseChannel;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
   unsigned char psocket;      /* primary socket id */
   unsigned char ssocket;      /* secondary socket id */
} __attribute__((packed)) DOT4CloseChannelReply;

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
   unsigned char socket;
} __attribute__((packed)) DOT4GetSocketReply; 

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char psocket;
   unsigned char ssocket;
   unsigned short credit;    /* credit for sender */ 
} __attribute__((packed)) DOT4Credit; 

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char psocket;
   unsigned char ssocket;
   unsigned short maxcredit;   /* maximum outstanding credit */
} __attribute__((packed)) DOT4CreditRequest; 

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char result;
   unsigned char psocket;
   unsigned char ssocket;
   unsigned short credit;   /* credit for sender */
} __attribute__((packed)) DOT4CreditRequestReply; 

typedef struct
{
   DOT4Header h;
   unsigned char cmd;
   unsigned char psocket;     /* primary socket id which contains the error */
   unsigned char ssocket;     /* secondary socket id which contains the error */
   unsigned char error;
} __attribute__((packed)) DOT4Error; 

typedef DOT4ExitReply DOT4Reply;
typedef DOT4Exit DOT4Cmd;
typedef DOT4CloseChannelReply DOT4CreditReply;
typedef DOT4Exit DOT4GetSocket;

int __attribute__ ((visibility ("hidden"))) Dot4ReverseCmd(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4Init(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4Exit(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4GetSocket(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4ForwardData(struct _mud_channel *pc, int fd, const void *buf, int size, int usec_timeout);
int __attribute__ ((visibility ("hidden"))) Dot4ReverseData(struct _mud_channel *pc, int fd, void *buf, int length, int usec_timeout);
int __attribute__ ((visibility ("hidden"))) Dot4OpenChannel(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4CloseChannel(struct _mud_channel *pc, int fd);
int __attribute__ ((visibility ("hidden"))) Dot4Credit(struct _mud_channel *pc, int fd, unsigned short credit);
int __attribute__ ((visibility ("hidden"))) Dot4CreditRequest(struct _mud_channel *pc, int fd, unsigned short credit);

#endif // _DOT4_H

