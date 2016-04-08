/*****************************************************************************\
  io_defs.h : I/O definitions

  Copyright (c) 1996 - 2015, HP Co.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#ifndef APDK_IO_DEFS_H
#define APDK_IO_DEFS_H

#define NFAULT_BIT  0x08
#define SELECT_BIT  0x10
#define PERROR_BIT  0x20
#define NACK_BIT    0x40
#define BUSY_BIT    0x80

#define STATUS_MASK (NFAULT_BIT | PERROR_BIT)
#define BUSY_MASK   (BUSY_BIT | NACK_BIT | NFAULT_BIT)

#define DEVICE_IS_BUSY(reg) (reg == BUSY_MASK)
#define DEVICE_IS_OOP(reg)  ((reg & STATUS_MASK) == OOP)
#define DEVICE_JAMMED_OR_TRAPPED(reg) ( ((reg & STATUS_MASK) == JAMMED) || ((reg & STATUS_MASK) == ERROR_TRAP) )
#define DEVICE_PAPER_JAMMED(reg)  ((reg & STATUS_MASK) == JAMMED)
#define DEVICE_IO_TRAP(reg)       ((reg & STATUS_MASK) == ERROR_TRAP)

#define OOP             (NFAULT_BIT | PERROR_BIT)
#define JAMMED          (PERROR_BIT)
#define ERROR_TRAP      (0)
#define OFFLINE         (NFAULT_BIT)

#define MAX_BUSY_TIME    30000     // in msec, ie 30 sec
#define C32_STATUS_WAIT  2000      // in msec, ie 2 sec

#define MAX_SLOW_POLL_TIMES     3
#define MIN_XFER_FOR_SLOW_POLL  2

#endif //APDK_IO_DEFS_H
