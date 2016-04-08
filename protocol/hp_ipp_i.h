/******************************************************************************\

Copyright 2015 HP Development Company, L.P.

This program is free software; you can redistribute it and/or modify it under
the terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program;
if not, write to:
Free Software Foundation, Inc.
51 Franklin Street, Fifth Floor
Boston, MA 02110-1301, USA.

\******************************************************************************/

# ifndef _HP_IPP_I_H
# define _HP_IPP_I_H

#include "hpmud.h"
#include "utils.h"

HPIPP_RESULT removeChunkInfo(char* data, int *length);
HPIPP_RESULT prepend_http_header(raw_ipp *raw_request);
static ssize_t                                                  /* O - Number of bytes written */
raw_ipp_request_callback(volatile raw_ipp  *raw_buffer,         /* O - RAW IPP buffer*/
                                       ipp_uchar_t *buffer,     /* I - Data to write */
                                       size_t      length);      /* I - Number of bytes to write */
static ssize_t                                              /* O - Number of bytes read */
raw_ipp_response_read_callback(raw_ipp      *src,           /* I - Source raw ipp data buffer */
                               ipp_uchar_t  *buffer,        /* O - Read buffer */
                               size_t       length);         /* O - Number of bytes to read */
enum HPMUD_RESULT writeChannel(char *buf, int size, HPMUD_DEVICE hd, HPMUD_CHANNEL cd);
enum HPMUD_RESULT readChannel(raw_ipp *responseptr, HPMUD_DEVICE hd, HPMUD_CHANNEL cd);
enum HPMUD_RESULT sendUSBRequest(char *buf, int size, raw_ipp *responseptr, char * device_uri);
ipp_t * networkDoRequest(ipp_t *request, char* device_uri);
ipp_t * usbDoRequest(ipp_t *request, char* device_uri);


#endif

