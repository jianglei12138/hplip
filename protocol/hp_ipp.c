
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


#include <cups/cups.h>
#include <cups/language.h>
#include <cups/ppd.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "hp_ipp.h"
#include "hp_ipp_i.h"

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)


http_t* acquireCupsInstance()
{
    if ( http == NULL)
    {
        http = httpConnectEncrypt( cupsServer(), ippPort(), cupsEncryption() );
    }

    return http;
}


void _releaseCupsInstance()
{
    if (http)
    {
        httpClose(http);
    }

    http = NULL;
}


/*
 * 'validate_name()' - Make sure the printer name only contains valid chars.
 */
static int                                /* O - 0 if name is no good, 1 if name is good */
validate_name( const char *name )         /* I - Name to check */
{
    return 1; // TODO: Make it work with utf-8 encoding
}

const char *getCupsErrorString(int status)
{
     return ippErrorString(status);
}

void freePrinterList(printer_t *list)
{
     printer_t *temp;
     printer_t *elem = list;

     while (elem != NULL) {
         temp = elem;
         elem = elem->next;
         free(temp);
     }
}

int addCupsPrinter(char *name, char *device_uri, char *location, char *ppd_file, char *model, char *info)
{
     char printer_uri[ HTTP_MAX_URI ];
     cups_lang_t *language;                /* Default language */
     int ret_status = 0;
     ipp_status_t status = IPP_BAD_REQUEST;
     ipp_t * request = NULL,                /* IPP Request */
           *response = NULL;                /* IPP Response */

     if ( ( strlen( ppd_file ) > 0 && strlen( model ) > 0 ) ||
             ( strlen( ppd_file ) == 0 && strlen( model ) == 0) ) {
        // status_str = "Invalid arguments: specify only ppd_file or model, not both or neither";
         goto abort;
     }

     if ( !validate_name(name) ) {
         //status_str = "Invalid printer name";
         goto abort;
     }

     if ( info == NULL )
        strcpy( info, name );

     sprintf( printer_uri, "ipp://localhost/printers/%s", name );

     cupsSetUser ("root");
     /* Connect to the HTTP server */
     if (acquireCupsInstance() == NULL)
     {
        //status_str = "Unable to connect to CUPS server";
        goto abort;
     }

     /* Assemble the IPP request */
     request = ippNew();
     language = cupsLangDefault();

     ippSetOperation( request, CUPS_ADD_PRINTER );
     ippSetRequestId ( request, 1 );

     ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                  "attributes-charset", NULL, cupsLangEncoding( language ) );

     ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                  "attributes-natural-language", NULL, language->language );

     ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_URI,
                  "printer-uri", NULL, printer_uri );

     ippAddInteger( request, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                   "printer-state", IPP_PRINTER_IDLE );

     ippAddBoolean( request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1 );

     ippAddString( request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
                  device_uri );

     ippAddString( request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL,
                  info );

     ippAddString( request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL,
                  location );

     if ( strlen( model ) > 0 ) {
        ippAddString( request, IPP_TAG_PRINTER, IPP_TAG_NAME, "ppd-name", NULL, model );

        /* Send the request and get a response. */
        response = cupsDoRequest( http, request, "/admin/" );
     } else {
        /* Send the request and get a response. */
        response = cupsDoFileRequest( http, request, "/admin/", ppd_file );
     }

     if (response == NULL)
         status = cupsLastError();
     else
         status = ippGetStatusCode( response );

        // If user cancels the authentication pop-up, changing error code to IPP_NOT_AUTHENTICATED from IPP_FORBIDDEN
     if (status == IPP_FORBIDDEN && auth_cancel_req) {
        status = IPP_NOT_AUTHENTICATED;
        auth_cancel_req = 0;    // Reseting cancel request.
     }
     
     if ( status <= IPP_OK_CONFLICT ) {
        status =IPP_OK;
	ret_status = 0;
     } else {
	ret_status = 1;
     }

abort:
    if ( response != NULL )
        ippDelete( response );

    return ret_status;
}

int delCupsPrinter(char *pr_name)
{
    int ret_status;
    cups_lang_t *language;                /* Default language */
    char uri[ HTTP_MAX_URI ];
    const char *username = NULL;
    ipp_t * request = NULL,                /* IPP Request */
          *response = NULL;                /* IPP Response */

    if ( !validate_name(pr_name) ) {
        goto abort;
    }

    username = cupsUser();

    cupsSetUser ("root");
    /* Connect to the HTTP server */
    if (acquireCupsInstance() == NULL)
    {
        goto abort;
    }
    snprintf( uri, sizeof( uri ), "ipp://localhost/printers/%s", pr_name );

    /*
        * Build a CUPS_DELETE_PRINTER request, which requires the following
        * attributes:
        *
        *    attributes-charset
        *    attributes-natural-language
        *    printer-uri
       */
    request = ippNew();

    ippSetOperation( request, CUPS_DELETE_PRINTER );
    ippSetRequestId ( request, 1 );

    language = cupsLangDefault();

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                  "attributes-charset", NULL, cupsLangEncoding( language ) );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                  "attributes-natural-language", NULL, language->language );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_URI,
                  "printer-uri", NULL, uri );

    /*
     * Do the request and get back a response...
     */
    response = cupsDoRequest( http, request, "/admin/" );

    if (response == NULL)
        ret_status = cupsLastError();
    else
        ret_status = ippGetStatusCode( response );

    // If user cancels the authentication pop-up, changing error code to IPP_NOT_AUTHENTICATED from IPP_FORBIDDEN
    if (ret_status == IPP_FORBIDDEN && auth_cancel_req)
    {
        ret_status = IPP_NOT_AUTHENTICATED;
        auth_cancel_req = 0;    // Reseting cancel request.
    }

    if ( ret_status <= IPP_OK_CONFLICT )
        ret_status = IPP_OK;

abort:
    if (username)
        cupsSetUser(username);

    if ( response != NULL )
        ippDelete( response );

    return ret_status;
}


int setDefaultCupsPrinter(char *pr_name)
{
    int ret_status = 0;
    char uri[ HTTP_MAX_URI ];        /* URI for printer/class */
    ipp_t *request = NULL,           /* IPP Request */
          *response = NULL;          /* IPP Response */
    cups_lang_t *language;           /* Default language */
    const char *username = NULL;

    if ( !validate_name(pr_name) ) {
       goto abort;
    }

    username = cupsUser();

    cupsSetUser ("root");
    /* Connect to the HTTP server */
    if ( acquireCupsInstance () == NULL) {
        goto abort;
    }

    /*
      * Build a CUPS_SET_DEFAULT request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      */

    snprintf( uri, sizeof( uri ), "ipp://localhost/printers/%s", pr_name );

    request = ippNew();

    ippSetOperation( request, CUPS_SET_DEFAULT );
    ippSetRequestId ( request, 1 );

    language = cupsLangDefault();

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                  "attributes-charset", NULL, "utf-8" ); //cupsLangEncoding( language ) );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                  "attributes-natural-language",
                  //NULL, language != NULL ? language->language : "en");
                  NULL, language->language );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_URI,
                  "printer-uri", NULL, uri );

    /*
     * Do the request and get back a response...
     */

    response = cupsDoRequest( http, request, "/admin/" );
    if (response == NULL)
        ret_status = cupsLastError();
    else
        ret_status = ippGetStatusCode(response );

    // If user cancels the authentication pop-up, changing error code to IPP_NOT_AUTHENTICATED from IPP_FORBIDDEN
    if (ret_status == IPP_FORBIDDEN && auth_cancel_req)
    {
        ret_status = IPP_NOT_AUTHENTICATED;
        auth_cancel_req = 0;    // Reseting cancel request.
    }

    // ToDo: When you assign ret_status with IPP_OK, the caller will lose getting the actual status
    if ( ret_status <= IPP_OK_CONFLICT )
        ret_status = IPP_OK;

abort:
    if (username)
        cupsSetUser(username);

    if ( response != NULL )
        ippDelete( response );

    return ret_status;

}

int controlCupsPrinter(char *pr_name, int op)
{
    ipp_t *request = NULL,                 /* IPP Request */
          *response = NULL;                /* IPP Response */
    char uri[ HTTP_MAX_URI ];        /* URI for printer/class */
    cups_lang_t *language;
    const char *username = NULL;
    int ret_status = 0;   //ToDo: Make use of IPP_BAD_REQUEST while changing to proper status codes

    if ( !validate_name(pr_name) ) {
        goto abort;
    }

    username = cupsUser();
    cupsSetUser ("root");
    /* Connect to the HTTP server */
    if (acquireCupsInstance () == NULL)
    {
        goto abort;
    }

    request = ippNew();

    ippSetOperation( request, op );
    ippSetRequestId ( request, 1 );

    language = cupsLangDefault();

    snprintf( uri, sizeof( uri ), "ipp://localhost/printers/%s", pr_name );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                  "attributes-charset", NULL, cupsLangEncoding( language ) );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                  "attributes-natural-language", NULL, language->language );

    ippAddString( request, IPP_TAG_OPERATION, IPP_TAG_URI,
                  "printer-uri", NULL, uri );


    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());

    if (op == IPP_PURGE_JOBS)
        ippAddBoolean(request, IPP_TAG_OPERATION, "purge-jobs", 1);

    response = cupsDoRequest(http, request, "/admin/");

    if (response == NULL)
        ret_status = cupsLastError();
    else
        ret_status = ippGetStatusCode( response );

    // If user cancels the authentication pop-up, changing error code to IPP_NOT_AUTHENTICATED from IPP_FORBIDDEN
    if (ret_status == IPP_FORBIDDEN && auth_cancel_req)
    {
        ret_status = IPP_NOT_AUTHENTICATED;
        auth_cancel_req = 0;    // Reseting cancel request.
    }

abort:
    if (username)
        cupsSetUser(username);

    if ( response != NULL )
        ippDelete( response );

    return ret_status;
}


/*
 * 'getCupsPrinters()' - Get installed cups printers.
 *
 * This function sends a CUPS_GET_PRINTERS request and return IPP response.
 * It also updates the number of cups printers found in  count variable.
 *
 */
ipp_t * __getCupsPrinters()
{
    ipp_t *request = NULL;  /* IPP request object */
    ipp_t *response = NULL; /* IPP response object */
    ipp_attribute_t *attr;     /* Current IPP attribute */


    static const char * attrs[] =         /* Requested attributes */
    {
        "printer-info",
        "printer-location",
        "printer-make-and-model",
        "printer-state",
        "printer-name",
        "device-uri",
        "printer-uri-supported",
        "printer-is-accepting-jobs",
    };

    /* Connect to the HTTP server */
    if (acquireCupsInstance() == NULL)
    {
        goto abort;
    }

    /* Assemble the IPP request */
    request = ippNewRequest(CUPS_GET_PRINTERS);

    if (request == NULL)
        goto abort;


    ippAddStrings( request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "requested-attributes", sizeof( attrs ) / sizeof( attrs[ 0 ] ),
                   NULL, attrs );

    /* Send the request and get a response. */
    response = cupsDoRequest( http, request, "/" );

abort:

    return response;
}


/*
 * 'parsePrinterAttributes()'
 *
 * This function parses the IPP response and updates the printer_list buffer.
 *
 */
int __parsePrinterAttributes(ipp_t *response, printer_t **printer_list)
{
    ipp_attribute_t *attr;     
    ipp_tag_t grp_tag;
    ipp_tag_t val_tag;
    char *attr_name;

    printer_t *t_printer = NULL;
    printer_t *t_printer_list = NULL;

    for ( attr = ippFirstAttribute(response); attr != NULL; attr = ippNextAttribute(response) )
    {
        if ( ippGetGroupTag(attr) != IPP_TAG_PRINTER )
            continue;

	t_printer = (printer_t *) calloc(1, sizeof(printer_t));
        if (t_printer == NULL) {
            BUG("Memory allocation for printer struct failed!\n");
            goto abort;
        }

	if (t_printer_list == NULL) {
	    t_printer_list = t_printer;
            *printer_list = t_printer_list;
        } else {
	    t_printer_list->next = t_printer;
	    t_printer_list = t_printer;
	}

        while ( attr != NULL && ippGetGroupTag(attr) == IPP_TAG_PRINTER ) {
             attr_name = ippGetName(attr);
             val_tag  = ippGetValueTag(attr); 

             if ( strcmp(attr_name, "printer-name") == 0 &&
                                        val_tag == IPP_TAG_NAME ) {
                  strcpy(t_printer->name, ippGetString(attr, 0, NULL) );
             }
             else if ( strcmp(attr_name, "device-uri") == 0 &&
                                         val_tag == IPP_TAG_URI ) {
                  strcpy(t_printer->device_uri, ippGetString(attr, 0, NULL) );
             }
             else if ( strcmp(attr_name, "printer-uri-supported") == 0 &&
                                                 val_tag == IPP_TAG_URI ) {
                  strcpy(t_printer->printer_uri, ippGetString(attr, 0, NULL) );
             }
             else if ( strcmp(attr_name, "printer-info") == 0 &&
                                        val_tag == IPP_TAG_TEXT ) {
                  strcpy(t_printer->info, ippGetString(attr, 0, NULL) );
             }
             else if ( strcmp(attr_name, "printer-location") == 0 &&
                                           val_tag == IPP_TAG_TEXT ) {
                  strcpy(t_printer->location, ippGetString(attr, 0, NULL) );
             }
             else if ( strcmp(attr_name, "printer-make-and-model") == 0 &&
                                                  val_tag == IPP_TAG_TEXT ) {
                  strcpy(t_printer->make_model, ippGetString(attr, 0, NULL) );
             } 
             else if ( strcmp(attr_name, "printer-state") == 0 &&
                                             val_tag == IPP_TAG_ENUM ) {
                   t_printer->state = ( ipp_state_t ) ippGetInteger(attr, 0);
             }
             else if (!strcmp(attr_name, "printer-is-accepting-jobs") &&
                                             val_tag == IPP_TAG_BOOLEAN) {
                   t_printer->accepting = ippGetBoolean(attr, 0);
             }

             attr = ippNextAttribute(response);
        }
    }

abort:
    return 0;
}


int getCupsPrinters(printer_t **printer_list)
{
     ipp_t *response = NULL;
     int ret_status = 0;

     response = __getCupsPrinters();

     if (response) {
         ret_status = __parsePrinterAttributes(response, printer_list);
         ippDelete(response);
     }

     return ret_status;
}


/*
 * 'initializeIPPRequest()' - Initialize request with those attributes which 
 * are common for all requests .
 */
void initializeIPPRequest(ipp_t *request)
{

    if (request)
    {
        //Set common request fields
        ippSetVersion ( request, 2, 0 );
        ippSetRequestId ( request, 100 );
    }
}


/*
 * 'createDeviceStatusRequest()' - Create IPP request and update the same with values needed for getting device status attributes.
 */
ipp_t * createDeviceStatusRequest()
{

    ipp_t *request = NULL;                /* IPP request object */
    static const char * attrs[] =         /* Requested attributes */
    {
        "marker-names",
        "marker-types",
        "marker-levels",
        "marker-low-levels",
        "printer-state",
        "printer-state-reasons",
    };

    /* Assemble the IPP request */
    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
    initializeIPPRequest(request);
    if (request)
    {
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "");
        ippAddStrings( request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", sizeof( attrs ) / sizeof( attrs[ 0 ] ),  NULL, attrs );
    }
    
    return request;
}


/*
 * 'ExtractIPPData()' - Extract IPP Data from IPP raw response. It will have HTTP header and may have chunk data. 
 */
HPIPP_RESULT ExtractIPPData(char* data, int *length)
{
    char * ptr = data;
    int chunked = 0;
    int content_length = 0;
    HPIPP_RESULT status = HPIPP_OK;
    
    status = parseResponseHeader(data, &content_length, &chunked, NULL);
    if(HPIPP_OK != status )
        return status;

    DBG("Transfer-Encoding (chunked = %d), DataLength(with http header) = [%d]\n", chunked, *length);
    
    //Remove http header.
    ptr = strstr(data, CRLFCRLF);
    if(ptr)
    {
        ptr += CRLFCRLF_LENGTH;
        *length = *length - (ptr - data);
        memmove(data, ptr, *length);        
    }

    if (chunked)
    {       
       status = removeChunkInfo(data, length);
    }

    return status; 
}


/*
 * 'parseResponseHeader()' - Parse HTTP Header and update content_length and chunk flag. 
 *
 */
HPIPP_RESULT parseResponseHeader(char *header, int *content_length, int *chunked, int *header_size)
{
    HPIPP_RESULT hr = HPIPP_OK;
    char *ptr = NULL;
    
    if(!header || !content_length || !chunked)
        return HPIPP_ERROR;

    if(header_size) 
    {
        ptr = strstr(header, CRLFCRLF);
        *header_size = (ptr)? (ptr - header + CRLFCRLF_LENGTH): 0;            
    }

    if (strcasestr(header, "Transfer-Encoding:") && strcasestr(header, "chunked"))
    {
        *chunked = 1;
        *content_length = 0;
    }
    else if (ptr = strstr(header,"Content-Length:"))
    {
        *content_length = strtol(ptr + strlen("Content-Length:"), NULL, BASE_DECIMAL);
        *chunked = 0;
    }
    else
    {
        DBG("parseResponseHeader: Could not find Transfer-Encoding: chunked or Content-Length: \n");
        hr = HPIPP_ERROR;   
    } 

    DBG("chunked = [%d] , Content_length = [%d]\n", *chunked, *content_length);
    return  hr;  
}


/*
 * 'getDeviceStatusAttributes()' - Get Device Status Attributes.
 *
 * This function sends a IPP_GET_PRINTER_ATTRIBUTES request  and request for 
 * marker-names, marker-types, marker-levels, marker-low-levels, printer-state, printer-state-reasons.
 * In addition to this it also updates the attribute count in the response.
 */
ipp_t * getDeviceStatusAttributes(char* device_uri, int *count)
{   
    ipp_t *request = NULL;  /* IPP request object */
    ipp_t *response = NULL; /* IPP response object */
    ipp_attribute_t *attr;     /* Current IPP attribute */
    int max_count = 0;    

    //Create Device Status Request
    request = createDeviceStatusRequest();
    if (request == NULL)
        goto abort;

    //Send request to the server based on connection 
    if(strcasestr(device_uri, ":/usb") != NULL )
    {
        response = usbDoRequest(request, device_uri);
    }
    else if (strcasestr(device_uri, ":/net") != NULL )
    {        
        response = networkDoRequest(request, device_uri);
    }
    else
    {
        BUG("Invalid device URI (%s)\n", device_uri);
        goto abort;
    }
        
    //Find out the attribute count.
    if (response)
        for ( attr = ippFirstAttribute( response ); attr != NULL; attr = ippNextAttribute( response ) )
            max_count++;    

abort:
    *count = max_count;
    return response;
}

/*************************** UTILS ***************************/

/*
 * 'usbDoRequest()' - Send ipp request to a usb device and read back the response.
 *
 * This function sends ipp request to a device correponding to caller provided device_uri and reads back the response.
 * This function then parses and stores the raw response into ipp_t (response) and sends back the same. 
 */
ipp_t * usbDoRequest(ipp_t *request, char* device_uri)
{
    ipp_t *response = NULL; /* IPP response object */
    ipp_state_t state;
    raw_ipp raw_request;
    raw_ipp raw_response;

    memset(&raw_request, 0, sizeof(raw_request));
    memset(&raw_response, 0, sizeof(raw_response));

    //Convert ipp_t request structure into raw ipp format 
    state = ippWriteIO(&raw_request, (ipp_iocb_t)raw_ipp_request_callback, 1, NULL, request); 
    if(IPP_ERROR == state)
    {
        BUG("ippWriteIO Failed...\n");
        goto abort;
    }   

    //Prepend http header to the raw request buffer 
    if (prepend_http_header(&raw_request) != HPIPP_OK)
        goto abort;

    //Send request to the device and get back the response  
    if (HPMUD_R_OK != sendUSBRequest(raw_request.data, raw_request.data_length, &raw_response, device_uri))
    {
        goto abort;
    }        
     
    //Convert Raw data into ipp_t response structure format
    response = ippNew();    
    state = ippReadIO(&raw_response, (ipp_iocb_t)raw_ipp_response_read_callback, 1, NULL, response);
    if(response && (IPP_ERROR == state))
    {
        BUG("ippWriteIO Failed...\n");
        ippDelete( response );
        response = NULL;
    }   

abort:
    return response;
}


/*
 * 'networkDoRequest()' - Send ipp request to a network device and read back the response.
 *
 * This function makes use of cups API (cupsDoRequest) to send ipp request and get back the ipp response.
 */
ipp_t * networkDoRequest(ipp_t *request, char* device_uri)
{
    ipp_t *response = NULL;             /* IPP response object */
    char ip[HPMUD_LINE_SIZE] = {0};
    http_t * http = NULL;     /* HTTP object */

    //Get IP address
    hpmud_get_uri_datalink(device_uri, ip, HPMUD_LINE_SIZE);

    //Create http connection
    http = httpConnectEncrypt( ip, ippPort(), cupsEncryption() );
    if (http == NULL)
        goto abort;

    //Send the request and get a response. 
    if ( ( response = cupsDoRequest( http, request, "/ipp/printer" ) ) == NULL )
    {        
        goto abort;
    }

abort:

    return response;
}


/*
 * 'sendUSBRequest()' - Send request and read response. 
 *
 */
enum HPMUD_RESULT sendUSBRequest(char *buf, int size, raw_ipp *responseptr, char * device_uri)
{
    HPMUD_DEVICE hd = 0;
    HPMUD_CHANNEL cd;
    enum HPMUD_RESULT stat = HPMUD_R_OK;
    int device_already_open = 0; 

    DBG("sendUSBRequest: buf = %p, size = %d, responseptr = %p, device_uri = %s\n", buf, size, responseptr, device_uri);

    /* Open hp device. */
    if ((stat = hpmud_open_device(device_uri, HPMUD_RAW_MODE, &hd)) != HPMUD_R_OK)
    {        
        if(stat == HPMUD_R_INVALID_STATE)
        {
            //hpmud reports HPMUD_R_INVALID_STATE error when we try to open the device again which was already opened by tools like hp-levels 
            /// or hp-toolbox. We can use this already opened device but we need to make sure we do not close the device after use.
            hd = 1;
            device_already_open = 1;
        }
        else
        {
            BUG("Device open failed with status code = %d\n", stat);
            goto abort;
        }
    }

    /* Open ipp channel. */
    if  ((stat = hpmud_open_channel(hd, HPMUD_S_IPP_CHANNEL, &cd)) != HPMUD_R_OK)
    {
        if ((stat = hpmud_open_channel(hd, HPMUD_S_IPP_CHANNEL2, &cd)) != HPMUD_R_OK)
        {
            BUG("Channel open failed with status code = %d\n", stat);
            goto abort;
        }
    }

    //Write request on the channel 
    if  ((stat = writeChannel(buf, size, hd, cd)) != HPMUD_R_OK)
    {
        BUG("Channel write failed with status code = %d\n", stat);
        goto abort;
    }

    //Read the response from the channel
    if  ((stat = readChannel(responseptr, hd, cd)) != HPMUD_R_OK)
    {
        BUG("Channel read failed with status code = %d\n", stat);
        //goto abort;
    }

    
    //Remove header and chunking information from the raw IPP response  
    ExtractIPPData(responseptr->data, &responseptr->data_length);
    stat = HPMUD_R_OK;

abort:

   if (cd > 0)
      hpmud_close_channel(hd, cd);
   if (hd > 0 && !device_already_open)
      hpmud_close_device(hd);   

    return stat;
}


/*
 * 'readChannel()' - Read response from the channel. 
 *
 */
enum HPMUD_RESULT readChannel(raw_ipp *responseptr, HPMUD_DEVICE hd, HPMUD_CHANNEL cd)
{
    int bytes_read = 0;
    int bytes_remaining = 0;
    int content_length = 0;
    int chunked = 0;
    int header_size = 0;
    enum HPMUD_RESULT stat = HPMUD_R_OK;
    char* data = responseptr->data;
    int* size = &responseptr->data_length;      
 
    if(!responseptr)
    {
        DBG("NULL  responseptr passed.\n");
        return HPMUD_R_INVALID_LENGTH;
    }

    memset(responseptr, 0, MAX_IPP_DATA_LENGTH);  

    //Read http header and figure out Transfer-Encoding of the response
    stat = hpmud_read_channel(hd, cd, data, USB_BULK_TRANSFER_LENGTH, TIMEOUT, &bytes_read);
    if(HPMUD_R_OK != stat &&  HPMUD_R_IO_TIMEOUT != stat)
    {
        return stat;
    }

    DBG("Header bytes read from the channel = %d, status = [%d] \n", bytes_read, stat);
    *size +=  bytes_read;

    if (HPIPP_OK != parseResponseHeader(data, &content_length, &chunked, &header_size))
        return HPMUD_R_IO_ERROR;

    bytes_remaining = content_length - (bytes_read - header_size); //Update bytes remaining 

    //Read remaining response
    while(bytes_read)
    {
        if (*size + USB_BULK_TRANSFER_LENGTH > MAX_IPP_DATA_LENGTH) //Check for the overflow condition
        {
            stat = HPMUD_R_INVALID_LENGTH;
            break;            
        }
        
        stat = hpmud_read_channel(hd, cd, &data[*size], USB_BULK_TRANSFER_LENGTH, TIMEOUT, &bytes_read);

        DBG("Bytes read from the channel = %d , status = [%d], bytes_remaining = [%d]\n", bytes_read, stat, bytes_remaining);
        if(HPMUD_R_OK != stat &&  HPMUD_R_IO_TIMEOUT != stat)
            break;
 
        *size +=  bytes_read;
        if(chunked)
        {        
            if(memcmp(&data[*size - CHUNK_DELIMITER_LENGTH], CHUNK_DELIMITER, CHUNK_DELIMITER_LENGTH) == 0)
            {
                DBG("Chunk end recieved....\n");
                break;        
            }
        }
        else
        {
            bytes_remaining -= bytes_read;
            if(bytes_remaining == 0)
            {
                DBG("Complete unchunked data recieved....\n");
                break;   
            }
        }
    }

    DBG("Total bytes read from the channel = %d\n", *size);
    return stat;
}


/*
 * 'writeChannel()' - Write request on the channel. 
 */
enum HPMUD_RESULT writeChannel(char *buf, int size, HPMUD_DEVICE hd, HPMUD_CHANNEL cd)
{
    int timeout = 1;
    int total = 0;
    int len = 0;
    int transfer_size = 0;
    enum HPMUD_RESULT stat = HPMUD_R_OK;

    while(size > 0)
    {
        transfer_size = (size > USB_BULK_TRANSFER_LENGTH)? USB_BULK_TRANSFER_LENGTH : size;
        stat = hpmud_write_channel(hd, cd, buf+total, transfer_size, timeout, &len);
        DBG("Bytes written on the channel = %d Size = %d, transfer_size = %d\n\n\n", len, size, transfer_size);
        total = len;
        size -=  len;
    }

    DBG("Total bytes written on the channel = %d\n", total);

    return stat;
}


/*
 * 'raw_ipp_response_read_callback()' - Read IPP data from a raw IPP response buffer.
 *
 * This callback function is called from cups API ippReadIO. ippReadIO function reads data from file, 
 * buffer or socket and then parses and converts raw ipp data into ipp_t format.
 */
static ssize_t				                    /* O - Number of bytes read */
raw_ipp_response_read_callback(raw_ipp      *src,           /* I - Source raw ipp data buffer */
                               ipp_uchar_t  *buffer,	    /* O - Read buffer */
                               size_t       length)         /* O - Number of bytes to read */
{

    //Check if remaining data has length bytes left. If not then return only remaining bytes.
    if (src->data_length < length )
    {
        DBG("Requested Length(%d) is more than source buffer length(%d).\n", length, src->data_length);
        length = src->data_length;
    }

    //Copy data to the requested buffer
    memcpy(buffer, src->data, length);

    //Remove copied data from the src, so that next time same data is not copied.
    memmove(src->data, &src->data[length], src->data_length - length);

    //Update src with new length.
    src->data_length = src->data_length - length;

    return (length);

}

/*
 * 'raw_ipp_request_callback()' - Write RAW IPP data to a buffer.
 *
 * This callback function (called from cupsSendRequest) recieves the converted data from
 * ipp_t request structure  to * a binary format in iterations. This function keeps on 
 * updating raw_buffer which is passed by the caller. Once all iterations are over, 
 * raw_buffer will contain complete IPP request data in a binary form which can later 
 * be sent to the IPP server.
 */
static ssize_t				                        /* O - Number of bytes written */
raw_ipp_request_callback(volatile raw_ipp  *raw_buffer,		/* O - RAW IPP buffer*/
                                       ipp_uchar_t *buffer,	/* I - Data to write */
                                       size_t      length)	/* I - Number of bytes to write */
{

    if (raw_buffer->data_length + length < MAX_IPP_DATA_LENGTH)
    {
        memcpy((char *)&raw_buffer->data[raw_buffer->data_length], buffer, length);
        raw_buffer->data_length = raw_buffer->data_length + length;
    }
    else
    {   
        BUG("Insufficient raw_ipp->data size. %d bytes needed.\n", raw_buffer->data_length + length);
    }

    return (length);
}


/*
 * 'prepend_http_header()' - Adds http header in the beginning of the ipp raw data and shifts 
 * raw data accordingly.
 */
HPIPP_RESULT prepend_http_header(raw_ipp *raw_request)
{

    char *http_header_tamplate = "POST /ipp/printer HTTP/1.1\r\nContent-Length: %d\r\nContent-Type: application/ipp\r\nHOST: Localhost\r\n\r\n";
    char http_header[1024] = {0};
    int http_header_size = 0;

    //Create transport header for the request        
    http_header_size = sprintf(http_header, http_header_tamplate, raw_request->data_length);

    //Shift raw_request->data by http_header_size bytes and copy http_header in the beginning
    if (raw_request->data_length + http_header_size >= MAX_IPP_DATA_LENGTH)
        return HPIPP_INVALID_LENGTH;
  
    memmove(raw_request->data + http_header_size, raw_request->data , raw_request->data_length);
    memcpy(raw_request->data,  http_header, http_header_size);
    raw_request->data_length = raw_request->data_length + http_header_size;  
    
    return HPIPP_OK;

}

/*
 * 'removeChunkInfo()' - Remove Chunk length information from the response. 
 */
HPIPP_RESULT removeChunkInfo(char* data, int *length)
{
    char* chunksize_start = data;
    char* chunksize_end = NULL;
    int chunklen = 0;
    int remaining_bytes = *length;
    HPIPP_RESULT hr = HPIPP_OK;
    int newlength = *length;

    do
    {
        chunksize_end = strstr(chunksize_start, CRLF);
        if(chunksize_end == NULL)
        {
            BUG("removeChunkInfo failed.\n");
            return HPIPP_ERROR; 
        }  

        chunksize_end += CRLF_LENGTH;
        chunklen = strtol(chunksize_start, NULL, BASE_HEX);             //Convert ChunkLength from ASCII hex format to an integer.
        remaining_bytes -= (chunksize_end -chunksize_start);            //Update remmaining length

        DBG("chunklen = [%d], remaining_bytes= [%d], newlength = [%d]\n", chunklen, remaining_bytes, newlength);      
        if(chunklen > remaining_bytes)
        {
            BUG("RemoveChunkInfo failed.\n");
            return HPIPP_ERROR; 
        }

        memmove(chunksize_start, chunksize_end, remaining_bytes);       //Delete chunklengthinfo from buffer by moving remaining_bytes data
        newlength -= (chunksize_end -chunksize_start);                  //Subtract size of chunklengthinfo from ipp data length
        chunksize_start += chunklen;                                    //Move to next chunk
        remaining_bytes -= chunklen;                                    //Update remmaining length

        //Check if chunk data ends with CRLF. If so then remove that also.
        if(memcmp(chunksize_start, CRLF, CRLF_LENGTH) == 0)
        {
            remaining_bytes -= CRLF_LENGTH;                                               
            newlength -= CRLF_LENGTH;                                                
            memmove(chunksize_start, chunksize_start + CRLF_LENGTH, remaining_bytes);  
        }

    } while(chunklen);
                          
    //Set remaining_bytes buffer to 0 and update length of the buffer to newlength before returning.    
    memset(data+newlength, 0, MAX_IPP_DATA_LENGTH - newlength); 
    *length = newlength;
 
   return hr;
}


