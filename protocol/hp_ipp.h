/******************************************************************************\

Copyright 2015 HP Development Company, L.P.

This program is free software; you can redistribute it and/or modify it under 
the terms of version 2 of the GNU General Public License as published by the 
Free Software Foundation.
This program is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with 
this program;
if not, write to:
Free Software Foundation, Inc.
51 Franklin Street, Fifth Floor
Boston, MA 02110-1301, USA.  

\******************************************************************************/

# ifndef _HP_IPP_H
# define _HP_IPP_H

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/ppd.h>


#define HP_DEV_URI_LEN 256
#define HP_DEV_NAME_LEN 128

#define MAX_ATTR_VALUES 8  
#define MAX_IPP_DATA_LENGTH 20000 

#define USB_BULK_TRANSFER_LENGTH 512

#define CRLF "\r\n"
#define CRLF_LENGTH 2
#define CRLFCRLF "\r\n\r\n"
#define CRLFCRLF_LENGTH 4

#define CHUNK_DELIMITER "0\r\n\r\n"
#define CHUNK_DELIMITER_LENGTH 5    

#define BASE_DECIMAL 10
#define BASE_HEX 16

#define TIMEOUT 3


#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
    #define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
    #define ippGetCount(attr)     attr->num_values
    #define ippGetGroupTag(attr)  attr->group_tag
    #define ippGetValueTag(attr)  attr->value_tag
    #define ippGetName(attr)      attr->name
    #define ippGetBoolean(attr, element) attr->values[element].boolean
    #define ippGetInteger(attr, element) attr->values[element].integer
    #define ippGetStatusCode(ipp) ipp->request.status.status_code
    #define ippGetString(attr, element, language) attr->values[element].string.text


    static ipp_attribute_t * ippFirstAttribute( ipp_t *ipp )
    {
        if (!ipp)
            return (NULL);
        return (ipp->current = ipp->attrs);
    }

    static ipp_attribute_t * ippNextAttribute( ipp_t *ipp )
    {
        if (!ipp || !ipp->current)
            return (NULL);
        return (ipp->current = ipp->current->next);
    }

    static int ippSetOperation( ipp_t *ipp, ipp_op_t op )
    {
        if (!ipp)
            return (0);
        ipp->request.op.operation_id = op;
        return (1);
    }

    static int ippSetRequestId( ipp_t *ipp, int request_id )
    {
        if (!ipp)
            return (0);
        ipp->request.any.request_id = request_id;
        return (1);
    }

    static int ippSetVersion( ipp_t *ipp, int major, int minor )
    {
        if (!ipp)
            return (0);
        ipp->request.any.version[0] = major;
        ipp->request.any.version[1] = minor;
        return (1);
    }

#endif


typedef struct _printer_t
{
    char device_uri [HP_DEV_URI_LEN] ;
    char name [HP_DEV_NAME_LEN] ;
    char printer_uri [HP_DEV_URI_LEN] ;
    char location [HP_DEV_NAME_LEN] ;
    char make_model [HP_DEV_NAME_LEN] ;
    char info [HP_DEV_NAME_LEN] ;
    int state;
    int accepting;
    struct _printer_t *next;
} printer_t;


typedef struct _attr_t
{
    char* name;
    void* values[MAX_ATTR_VALUES];
    int count;
    int type;
} attr_t;


typedef struct _raw_ipp
{
    int  data_length;
    char data[MAX_IPP_DATA_LENGTH];
} raw_ipp;

typedef enum _HPIPP_RESULT
{
    HPIPP_OK = 0,
    HPIPP_ERROR = 1,
    HPIPP_INVALID_LENGTH = 2,
} HPIPP_RESULT;


http_t * http = NULL;     /* HTTP object */
int auth_cancel_req = 0;  // 0--> authentication cancel is not requested, 
                          // 1 --> authentication cancelled


http_t*    acquireCupsInstance();
const char *getCupsErrorString(int status);
void freePrinterList(printer_t *list);
 
static ssize_t raw_ipp_response_read_callback(raw_ipp  *raw_buffer,ipp_uchar_t *buffer,size_t length);
static ssize_t raw_ipp_request_callback(volatile raw_ipp  *raw_buffer, ipp_uchar_t *buffer, size_t length);

void initializeIPPRequest(ipp_t *request);
int parsePrinterAttributes(ipp_t *response, printer_t * printer_list, int size);
ipp_t * createDeviceStatusRequest();
ipp_t * usbDoRequest(ipp_t *request, char* device_uri);
ipp_t * networkDoRequest(ipp_t *request, char* device_uri);
ipp_t * getDeviceStatusAttributes(char* device_uri, int *count);
int     getCupsPrinters(printer_t **printer_list);

HPIPP_RESULT parseResponseHeader(char* header, int *content_length, int *chunked, int* header_size);
HPIPP_RESULT prepend_http_header(raw_ipp *raw_request);
enum HPMUD_RESULT sendUSBRequest(char *buf, int size, raw_ipp *responseptr, char * device_uri);

# endif //_IPP_H
