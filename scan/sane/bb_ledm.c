/************************************************************************************\
  bb_ledm.c - HP SANE backend support for ledm based multi-function peripherals
  (c) 2010 Copyright HP Development Company, LP

  Primary Author: Naga Samrat Chowdary, Narla
  Contributing Authors: Yashwant Kumar Sahu, Sarbeswar Meher
\************************************************************************************/

# ifndef _GNU_SOURCE
# define _GNU_SOURCE
# endif

# include <stdarg.h>
# include <syslog.h>
# include <stdio.h>
# include <string.h>
# include <fcntl.h>
# include <math.h>
# include "sane.h"
# include "saneopts.h"
# include "hpmud.h"
# include "hpip.h"
# include "common.h"
# include "ledm.h"
# include "ledmi.h"
# include "http.h"
# include "xml.h"
# include <stdlib.h>

# include <stdint.h>

# define _STRINGIZE(x) #x
# define STRINGIZE(x) _STRINGIZE(x)

# define _BUG(args...) syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args)

//# define  BB_LEDM_DEBUG

# ifdef BB_LEDM_DEBUG
   # define _DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
# else
   # define _DBG(args...)
# endif

enum DOCUMENT_TYPE
{
  DT_AUTO = 1,
  DT_MAX,
};

enum SCANNER_STATE
{
  SS_IDLE = 1,
  SS_PROCESSING,
  SS_STOPPED,
};

enum SCANNER_STATE_REASON
{
  SSR_ATTENTION_REQUIRED = 1,
  SSR_CALIBRATING,
  SSR_COVER_OPEN,
  SSR_INPUT_TRAY_EMPTY,
  SSR_INTERNAL_STORAGE_FULL,
  SSR_LAMP_ERROR,
  SSR_LAMP_WARMING,
  SSR_MEDIA_JAM,
  SSR_BUSY,
  SSR_NONE,
};

struct media_size
{
  int width;         			       /* in 1/1000 of an inch */
  int height;        			       /* in 1/1000 of an inch */
};

struct device_settings
{
  enum COLOR_ENTRY color[CE_MAX];
  enum SCAN_FORMAT formats[SF_MAX];
  int jpeg_quality_factor_supported;           /* 0=false, 1=true */
  enum DOCUMENT_TYPE docs[DT_MAX];
  int document_size_auto_detect_supported;     /* 0=false, 1=true */
  int feeder_capacity;
};

struct device_platen
{
  int flatbed_supported;                       /* 0=false, 1=true */
  struct media_size minimum_size;
  struct media_size maximum_size;
  struct media_size optical_resolution;
  int platen_resolution_list[MAX_LIST_SIZE];
};

struct device_adf
{
  int supported;                               /* 0=false, 1=true */
  int duplex_supported;                        /* 0=false, 1=true */
  struct media_size minimum_size;
  struct media_size maximum_size;
  struct media_size optical_resolution;
  int adf_resolution_list[MAX_LIST_SIZE];
};

struct scanner_configuration
{
  struct device_settings settings;
  struct device_platen platen;
  struct device_adf adf;
};

struct scanner_status
{
  char *current_time;
  enum SCANNER_STATE state;
  enum SCANNER_STATE_REASON reason;
  int paper_in_adf;                            /* 0=false, 1=true */
  int scan_to_available;                       /* 0=false, 1=true */
};

struct wscn_scan_elements
{
  struct scanner_configuration config;
  struct scanner_status status;
  char model_number[32];
};

struct wscn_create_scan_job_response
{
  int jobid;
  int pixels_per_line;
  int lines;                                   /* number of lines */
  int bytes_per_line;                          /* zero if jpeg */
  enum SCAN_FORMAT format;
  int jpeg_quality_factor;
  int images_to_transfer;                      /* number of images to scan */
  enum INPUT_SOURCE source;
  enum DOCUMENT_TYPE doc;
  struct media_size input_size;
  int scan_region_xoffset;
  int scan_region_yoffset;
  int scan_region_width;
  int scan_region_height;
  enum COLOR_ENTRY color;
  struct media_size resolution;
};

struct bb_ledm_session
{
  struct wscn_create_scan_job_response job;    /* actual scan job attributes (valid after sane_start) */
  struct wscn_scan_elements elements;          /* scanner elements (valid after sane_open and sane_start) */
  HTTP_HANDLE http_handle;
};

/* Following elements must match their associated enum table. */
static const char *sf_element[SF_MAX] = { "", "raw", "jpeg" };  /* SCAN_FORMAT (compression) */
static const char *ce_element[CE_MAX] = { "", "K1", "Gray8", "Color8" };   /* COLOR_ENTRY */
static const char *is_element[IS_MAX] = { "", "Platen", "Adf", "ADFDuplex" };   /* INPUT_SOURCE */

# define POST_HEADER "POST /Scan/Jobs HTTP/1.1\r\nHost: localhost\r\nUser-Agent: \
hplip\r\nAccept: text/plain, */*\r\nAccept-Language: en-us,en\r\n\
Accept-Charset: ISO-8859-1,utf-8\r\nKeep-Alive: 1000\r\nProxy-Connection: keep-alive\r\n\
Content-Type: */*; charset=UTF-8\r\nX-Requested-With: XMLHttpRequest\r\n\
Content-Length: %d\r\nCookie: AccessCounter=new\r\n\
Pragma: no-cache\r\nCache-Control: no-cache\r\n\r\n" 

# define GET_SCANNER_ELEMENTS "GET /Scan/ScanCaps HTTP/1.1\r\n\
Host: localhost\r\nUser-Agent: hplip\r\n\
Accept: text/xml\r\n\
Accept-Language: en-us,en\r\n\
Accept-Charset:utf-8\r\n\
Keep-Alive: 20\r\nProxy-Connection: keep-alive\r\nCookie: AccessCounter=new\r\n0\r\n\r\n"

# define GET_SCANNER_STATUS "GET /Scan/Status HTTP/1.1\r\n\
Host: localhost\r\nUser-Agent: hplip\r\n\
Accept: text/xml\r\n\
Accept-Language: en-us,en\r\n\
Accept-Charset:utf-8\r\n\
Keep-Alive: 20\r\nProxy-Connection: keep-alive\r\nCookie: AccessCounter=new\r\n0\r\n\r\n"

# define CREATE_SCAN_JOB_REQUEST "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<ScanSettings xmlns=\"http://www.hp.com/schemas/imaging/con/cnx/scan/2008/08/19\">\
<XResolution>%d</XResolution>\
<YResolution>%d</YResolution>\
<XStart>%d</XStart>\
<Width>%d</Width>\
<YStart>%d</YStart>\
<Height>%d</Height>\
<Format>%s</Format>\
<CompressionQFactor>15</CompressionQFactor>\
<ColorSpace>%s</ColorSpace>\
<BitDepth>%d</BitDepth>\
<InputSource>%s</InputSource>\
<InputSourceType>%s</InputSourceType>%s\
<GrayRendering>NTSC</GrayRendering>\
<ToneMap>\
<Gamma>0</Gamma>\
<Brightness>%d</Brightness>\
<Contrast>%d</Contrast>\
<Highlite>0</Highlite>\
<Shadow>0</Shadow></ToneMap>\
<ContentType>Photo</ContentType></ScanSettings>" 

# define CANCEL_JOB_REQUEST "PUT %s HTTP/1.1\r\nHost: localhost\r\nUser-Agent: hplip\r\n\
Accept: text/plain\r\nAccept-Language: en-us,en\r\nAccept-Charset:utf-8\r\nKeep-Alive: 10\r\n\
Content-Type: text/xml\r\nProxy-Connection: Keep-alive\r\nX-Requested-With: XMLHttpRequest\r\nReferer: localhost\r\n\
Content-Length: %d\r\nCookie: AccessCounter=new\r\n\r\n"

#define CANCEL_JOB_DATA "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<j:Job xmlns:j=\"http://www.hp.com/schemas/imaging/con/ledm/jobs/2009/04/30\" \
xmlns:dd=\"http://www.hp.com/schemas/imaging/con/dictionaries/1.0/\" \
xmlns:fax=\"http://www.hp.com/schemas/imaging/con/fax/2008/06/13\" \
xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \
xsi:schemaLocation=\"http://www.hp.com/schemas/imaging/con/ledm/jobs/2009/04/30 ../schemas/Jobs.xsd\">\
<j:JobState>Canceled</j:JobState></j:Job>"

# define GET_SCAN_JOB_URL "GET %s HTTP/1.1\r\nHost: localhost\r\nUser-Agent: hplip\r\n\
Accept: text/plain\r\nAccept-Language: en-us,en\r\nAccept-Charset:utf-8\r\nX-Requested-With: XMLHttpRequest\r\n\
Keep-Alive: 300\r\nProxy-Connection: keep-alive\r\nCookie: AccessCounter=new\r\n0\r\n\r\n"

# define ZERO_FOOTER "\r\n0\r\n\r\n"

# define READY_TO_UPLOAD "<PageState>ReadyToUpload</PageState>"
# define CANCELED_BY_DEVICE "<PageState>CanceledByDevice</PageState>"
# define CANCELED_BY_CLIENT "<PageState>CanceledByClient</PageState>"
# define ADF_LOADED "<AdfState>Loaded</AdfState>"
# define ADF_EMPTY "<AdfState>Empty</AdfState>"
# define SCANNER_IDLE "<ScannerState>Idle</ScannerState>"
# define SCANNER_BUSY_WITH_SCAN_JOB "<ScannerState>BusyWithScanJob</ScannerState>"
# define JOBSTATE_PROCESSING "<j:JobState>Processing</j:JobState>"
# define JOBSTATE_CANCELED "<j:JobState>Canceled</j:JobState>"
# define JOBSTATE_COMPLETED "<j:JobState>Completed</j:JobState>"
# define PRESCANPAGE "<PreScanPage>"

static int parse_scan_elements(const char *payload, int size, struct wscn_scan_elements *elements)
{
  char tag[512];
  char value[128];
  int i;
  char *tail=(char *)payload;

  memset(elements, 0, sizeof(struct wscn_scan_elements));

  while (1)
  {
    get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);

    if (!tag[0])
      break;     /* done */

    if(strncmp(tag, "ColorEntries", 12) == 0)
    {
      int h=1;
      while(h)
      {
        get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
        if(strncmp(tag, "Platen", 6) ==0) break;
        if(strncmp(tag, "/ColorEntries", 13) ==0) break;
        if(strncmp(tag, "ColorType", 9)==0)
        {
          get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
          if (strcmp(value, ce_element[CE_K1]) == 0)
            elements->config.settings.color[CE_K1] = CE_K1;
          else if (strcmp(value, ce_element[CE_GRAY8]) == 0)
             elements->config.settings.color[CE_GRAY8] = CE_GRAY8;
          else if (strcmp(value, ce_element[CE_COLOR8]) == 0)
             elements->config.settings.color[CE_COLOR8] = CE_COLOR8;
//        else
//           _BUG("unknowned element=%s, sf_element[SF_JPEG]=%s, sf_element[SF_RAW]=%s\n", value, sf_element[SF_JPEG], sf_element[SF_RAW] );
          _DBG("FormatSupported:%s\n", value);
          get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
          if(strncmp(tag, "/ColorEntries", 13) == 0) h=0; 
         }
         if(strncmp(tag, "/ColorEntries", 13) == 0) h=0; 
       }   
    }         

    if(strncmp(tag, "Platen", 6) == 0)
    {
      elements->config.platen.flatbed_supported = 1;
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.minimum_size.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.minimum_size.height=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.maximum_size.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.maximum_size.height=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.optical_resolution.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.platen.optical_resolution.height=strtol(value, NULL, 10);        
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);

      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      i=1; 
      elements->config.platen.platen_resolution_list[0]=0;
      while(strcmp(tag, "/SupportedResolutions"))
      {
        get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
        if(!strcmp(tag, "Resolution"))
        {
          get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
          get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
          _DBG ("parse_scan_elements platen_resolution_list value=%s\n", value);
          if(strtol(value, NULL, 10) && elements->config.platen.platen_resolution_list[i-1] != strtol(value, NULL, 10))
          
            elements->config.platen.platen_resolution_list[i++]=strtol(value, NULL, 10);
        }
      }
      elements->config.platen.platen_resolution_list[0]=i-1;
    }

    if(strncmp(tag, "Adf", 3) == 0 && strlen(tag) == 3)
    {
      elements->config.adf.supported = 1;
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.minimum_size.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.minimum_size.height=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.maximum_size.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.maximum_size.height=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.optical_resolution.width=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      elements->config.adf.optical_resolution.height=strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);

      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      i=1; 
      elements->config.adf.adf_resolution_list[0]=0;
      while(strcmp(tag, "/SupportedResolutions"))
      {
        get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
        if(!strcmp(tag, "Resolution"))
        {
          get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
          get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
          _DBG ("parse_scan_elements adf_resolution_list value=%s", value);
          if(strtol(value, NULL, 10) && elements->config.adf.adf_resolution_list[i-1] != strtol(value, NULL, 10))
            elements->config.adf.adf_resolution_list[i++]=strtol(value, NULL, 10);
        }
      }
      elements->config.adf.adf_resolution_list[0]=i-1;
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);//FeederCapacity
      get_element(tail, size-(tail-payload), value, sizeof(value), &tail);
      _DBG ("parse_scan_elements FeederCapacity=%s", value);
      elements->config.settings.feeder_capacity = strtol(value, NULL, 10);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      get_tag(tail, size-(tail-payload), tag, sizeof(tag), &tail);
      if(!strcmp(tag, "AdfDuplexer"))
      {
         elements->config.adf.duplex_supported = 1;
         _DBG ("parse_scan_elements duplex_supported");
      }
    }
  }  /* end while (1) */
  return 0;
} /* parse_scan_elements */

static struct bb_ledm_session* create_session()
{
  struct bb_ledm_session* pbb;

  if ((pbb = malloc(sizeof(struct bb_ledm_session))) == NULL)
  {
    return NULL;
  }

  memset(pbb, 0, sizeof(struct bb_ledm_session));
  return pbb;
} /* create_session */

static int read_http_payload(struct ledm_session *ps, char *payload, int max_size, int sec_timeout, int *bytes_read)
{
  struct bb_ledm_session *pbb = ps->bb_session;
  int stat=1, total=0, len;
  int tmo=sec_timeout;
  enum HTTP_RESULT ret;
  int payload_length=-1;
  char *temp=NULL;

  *bytes_read = 0;

  if(http_read_header(pbb->http_handle, payload, max_size, tmo, &len) != HTTP_R_OK)
      goto bugout;

  _DBG("read_http_payload len=%d %s\n",len,payload);
  temp = strstr(payload, "HTTP/1.1 201 Created");
  if (temp)
  {
		*bytes_read = total = len;
		stat=0;
		return stat ;
  }
  
  temp=strstr(payload, "Content-Length:");
  if (temp)
  {
		temp=temp+16;
		temp=strtok(temp, "\r\n");
		payload_length=strtol(temp, NULL, 10);
		if (payload_length == 0)
	  	{
			*bytes_read = total = len;
			stat=0;
			return stat ;
		}
  }
  memset(payload, ' ', len);
  if(payload_length==-1)
  {
    int i=10;
    while(i)
    {
		len = 0;
		ret = http_read(pbb->http_handle, payload+total, max_size-total, tmo, &len);
		total+=len;
      	tmo=1;
    	i--;
		if (ret == HTTP_R_EOF)
        {
         _DBG("read_http_payload1 DONE......\n");
          break;    /* done */
        }

    	if (!(ret == HTTP_R_OK || ret == HTTP_R_EOF))
        {
        	_DBG("read_http_payload1 ERROR......\n");
        	goto bugout;
        }
    }//end while(i)
  }//end if(payload_length==-1)
  else
  {
	  len=payload_length;
	  while (total < payload_length) 
	  {
		  ret = http_read(pbb->http_handle, payload+total, max_size-total, tmo, &len);
		  total+=len;
		  tmo=1;
		  if (ret == HTTP_R_EOF)
		  {
		     _DBG("read_http_payload2 DONE......\n");
		     break;    /* done */
		  } 

		  if (!(ret == HTTP_R_OK || ret == HTTP_R_EOF))
		  {
        	  _DBG("read_http_payload2 ERROR......\n");
        	  goto bugout;
           }
        }//end while()
  }//end else

 *bytes_read = total;
  stat=0;

bugout:
   return stat;
} /* read_http_payload */

static int get_scanner_elements(struct ledm_session *ps, struct wscn_scan_elements *elements)
{
  struct bb_ledm_session *pbb = ps->bb_session;
  int bytes_read = 0;
  int stat=1, tmo=10;
  char buf[8192];

  if (http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
  {
    _BUG("unable to open http connection %s\n", ps->uri);
    goto bugout;
  }

  /* Write the xml payload. */
  if (http_write(pbb->http_handle, GET_SCANNER_ELEMENTS, sizeof(GET_SCANNER_ELEMENTS)-1, tmo) != HTTP_R_OK)
  {
    _BUG("unable to get_scanner_elements %s\n", ps->uri);
    goto bugout;
  }

  /* Read http response. */
  if (read_http_payload(ps, buf, sizeof(buf), tmo, &bytes_read))
    goto bugout;

  _DBG("get_scanner_elements bytes_read=%d len=%d buf=%s\n", bytes_read, strlen(buf), buf);

   http_unchunk_data(buf);
   bytes_read=strlen(buf);
  
  _DBG("get_scanner_elements buf=%s\n", buf);
  parse_scan_elements(buf, bytes_read, elements);
  stat=0;

bugout:
  if (pbb->http_handle)
  {
    http_close(pbb->http_handle);
    pbb->http_handle = 0;
  }
  return stat;
} /* get_scanner_elements */

static int cancel_job(struct ledm_session *ps)
{
  struct bb_ledm_session *pbb = ps->bb_session;
  int len, stat=1, tmo=5/*EXCEPTION_TIMEOUT*/;
  char buf[2048];
  int bytes_read;

  _DBG("cancel_job user_cancel=%d job_id=%d url=%s \n", ps->user_cancel, ps->job_id, ps->url);
  if (ps->job_id == 0 || ps->user_cancel == 0)
  {
       ps->job_id = 0;
       ps->page_id = 0;
       return 0 ;
  }
  
  if (http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
  {
    _BUG("unable to open http connection %s\n", ps->uri);
    goto bugout;
  }

  len = snprintf(buf, sizeof(buf), CANCEL_JOB_REQUEST, ps->url, strlen(CANCEL_JOB_DATA));
  if (http_write(pbb->http_handle, buf, len, 1) != HTTP_R_OK)
  {
    _BUG("unable to cancel_job %s\n", ps->url);
  }
 
  len = snprintf(buf, sizeof(buf), CANCEL_JOB_DATA);
  if (http_write(pbb->http_handle, buf, len, 1) != HTTP_R_OK)
  {
    _BUG("unable to cancel_job %s\n", ps->url);
  }

  if (read_http_payload(ps, buf, sizeof(buf), tmo, &bytes_read))
    goto bugout;

  stat=0;

bugout:
  if (pbb->http_handle)
  {
    http_close(pbb->http_handle);
    pbb->http_handle = 0;
  }
  return stat;   
}; /* cancel_job */

/* --------------------------- LEDM API Calls -----------------------------*/

int bb_open(struct ledm_session *ps)
{
  struct bb_ledm_session *pbb;
  struct device_settings *ds;
  int stat=1, i, j;

  _DBG("bb_open()\n");

  if ((ps->bb_session = create_session()) == NULL)
    goto bugout;

  pbb = ps->bb_session;

  /* Get scanner elements from device. */
  if (get_scanner_elements(ps, &pbb->elements))
  {
    goto bugout;
  }

  /* Determine supported Scan Modes. */
  ds = &pbb->elements.config.settings;
  for(i=0, j=0; i<CE_MAX; i++)
  {
    if (ds->color[i] == CE_K1)
    {
      ps->scanModeList[j] = SANE_VALUE_SCAN_MODE_LINEART;
      ps->scanModeMap[j++] = CE_K1;
    }
    if (ds->color[i] == CE_GRAY8)
    {
       ps->scanModeList[j] = SANE_VALUE_SCAN_MODE_GRAY;
       ps->scanModeMap[j++] = CE_GRAY8;
    }
    if (ds->color[i] == CE_COLOR8)
    {
      ps->scanModeList[j] = SANE_VALUE_SCAN_MODE_COLOR;
      ps->scanModeMap[j++] = CE_COLOR8;
    }
  }
   
  /* Determine scan input sources. */
  i=0;
  if (pbb->elements.config.platen.flatbed_supported)
  {
    ps->inputSourceList[i] = STR_ADF_MODE_FLATBED;
    ps->inputSourceMap[i++] = IS_PLATEN;
  }
  if (pbb->elements.config.adf.supported)
  {
    ps->inputSourceList[i] = STR_ADF_MODE_ADF;
    ps->inputSourceMap[i++] = IS_ADF;
  }
  if (pbb->elements.config.adf.duplex_supported)
  {
    ps->inputSourceList[i] = STR_TITLE_DUPLEX;
    ps->inputSourceMap[i++] = IS_ADF_DUPLEX;
  }

  /* Determine if jpeg quality factor is supported. */
  if (pbb->elements.config.settings.jpeg_quality_factor_supported)
    ps->option[LEDM_OPTION_JPEG_QUALITY].cap &= ~SANE_CAP_INACTIVE;
  else
    ps->option[LEDM_OPTION_JPEG_QUALITY].cap |= SANE_CAP_INACTIVE;


  /* Set flatbed x,y extents. */
  ps->platen_min_width = SANE_FIX(pbb->elements.config.platen.minimum_size.width/1000.0*MM_PER_INCH);
  ps->platen_min_height = SANE_FIX(pbb->elements.config.platen.minimum_size.height/1000.0*MM_PER_INCH);
  ps->platen_tlxRange.max = SANE_FIX(pbb->elements.config.platen.maximum_size.width/11.811023);
  ps->platen_brxRange.max = ps->platen_tlxRange.max;
  ps->platen_tlyRange.max = SANE_FIX(pbb->elements.config.platen.maximum_size.height/11.811023);
  ps->platen_bryRange.max = ps->platen_tlyRange.max;

  /* Set adf/duplex x,y extents. */
  ps->adf_min_width = SANE_FIX(pbb->elements.config.adf.minimum_size.width/1000.0*MM_PER_INCH);
  ps->adf_min_height = SANE_FIX(pbb->elements.config.adf.minimum_size.height/1000.0*MM_PER_INCH);
  ps->adf_tlxRange.max = SANE_FIX(pbb->elements.config.adf.maximum_size.width/11.811023);
  ps->adf_brxRange.max = ps->adf_tlxRange.max;
  ps->adf_tlyRange.max = SANE_FIX(pbb->elements.config.adf.maximum_size.height/11.811023);
  ps->adf_bryRange.max = ps->adf_tlyRange.max;

  if (pbb->elements.config.platen.flatbed_supported)
  {
      i = pbb->elements.config.platen.platen_resolution_list[0] + 1;
      while(i--)
      {
          _DBG("bb_open platen_resolution_list = %d\n",  pbb->elements.config.platen.platen_resolution_list[i]);
          ps->platen_resolutionList[i] = pbb->elements.config.platen.platen_resolution_list[i];
          ps->resolutionList[i] = pbb->elements.config.platen.platen_resolution_list[i];
      }
  }
  if (pbb->elements.config.adf.supported)
  {
     i = pbb->elements.config.adf.adf_resolution_list[0] + 1;
     while(i--)
     {
         _DBG("bb_open adf_resolution_list = %d\n", pbb->elements.config.adf.adf_resolution_list[i]);
         ps->adf_resolutionList[i] = pbb->elements.config.adf.adf_resolution_list[i]; 
         ps->resolutionList[i] = pbb->elements.config.adf.adf_resolution_list[i];
     }
  }
  stat = 0;

bugout:
  return stat;
} /* bb_open */

int bb_close(struct ledm_session *ps)
{
  if (ps->bb_session)
  {
      free(ps->bb_session);
      ps->bb_session = NULL;
  }
  return 0;
} 

/* Set scan parameters. If scan has started, use actual known parameters otherwise estimate. */
int bb_get_parameters(struct ledm_session *ps, SANE_Parameters *pp, int option)
{
  struct bb_ledm_session *pbb = ps->bb_session;
  pp->last_frame = SANE_TRUE;
  int factor;

  _DBG("bb_get_parameters(option=%d)\n", option);

  switch(ps->currentScanMode)
  {
    case CE_K1:
      pp->format = SANE_FRAME_GRAY;     /* lineart (GRAY8 converted to MONO by IP) */
      pp->depth = 1;
      factor = 1;
      break;
    case CE_GRAY8:
      pp->format = SANE_FRAME_GRAY;     /* grayscale */
      pp->depth = 8;
      factor = 1;
      break;
    case CE_COLOR8:
    default:
      pp->format = SANE_FRAME_RGB;      /* color */
      pp->depth = 8;
      factor = 3;
      break;
  }

  switch (option)
  {
    case SPO_STARTED:  /* called by xsane */
      if (ps->currentCompression == SF_RAW && ps->currentScanMode != CE_GRAY8)
      {
         /* Set scan parameters based on scan job response values */
        //pp->lines = pbb->job.lines;
        pp->lines = (int)(SANE_UNFIX(ps->effectiveBry - ps->effectiveTly)/MM_PER_INCH*ps->currentResolution);
        pp->pixels_per_line = pbb->job.pixels_per_line;
        pp->bytes_per_line = pbb->job.bytes_per_line;
      }
      else  /* Must be SF_JFIF or ScanMode==CE_BLACK_AND_WHITE1. */
      {
        /* Set scan parameters based on IP. Note for Linart, use IP for hpraw and jpeg. */
        //pp->lines = ps->image_traits.lNumRows;
        pp->lines = (int)(SANE_UNFIX(ps->effectiveBry - ps->effectiveTly)/MM_PER_INCH*ps->currentResolution);
        pp->pixels_per_line = ps->image_traits.iPixelsPerRow;
        pp->bytes_per_line = BYTES_PER_LINE(pp->pixels_per_line, pp->depth * factor);
      }
      break;
    case SPO_STARTED_JR: /* called by sane_start */
      /* Set scan parameters based on scan job response values */
      pp->lines = pbb->job.lines;
      pp->pixels_per_line = pbb->job.pixels_per_line;
      pp->bytes_per_line = pbb->job.bytes_per_line;
      break;
    case SPO_BEST_GUESS:  /* called by xsane & sane_start */
      /* Set scan parameters based on best guess. */
      pp->lines = (int)round(SANE_UNFIX(ps->effectiveBry - ps->effectiveTly)/MM_PER_INCH*ps->currentResolution);
      pp->pixels_per_line = (int)round(SANE_UNFIX(ps->effectiveBrx -ps->effectiveTlx)/MM_PER_INCH*ps->currentResolution);
      pp->bytes_per_line = BYTES_PER_LINE(pp->pixels_per_line, pp->depth * factor);
      break;
    default:
      break;
  }
return 0;
}

/***
* Function: bb_is_paper_in_adf()
* Arguments: 
*    1) struct ledm_session *ps (IN)
*
* Return Value: (type: Int)
*    0  = no paper in adf,
*    1  = paper in adf,
*    -1 = error
*/

int bb_is_paper_in_adf(struct ledm_session *ps) /* 0 = no paper in adf, 1 = paper in adf, -1 = error */
{
  char buf[1024];
  int bytes_read;
  struct bb_ledm_session *pbb = ps->bb_session;

  if(http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
  {
        _BUG("unable to open channel HPMUD_S_LEDM_SCAN \n");
        return -1;
  }
  if (http_write(pbb->http_handle, GET_SCANNER_STATUS, sizeof(GET_SCANNER_STATUS)-1, 10) != HTTP_R_OK)
  {
    _BUG("unable to get scanner status \n");
  }
  read_http_payload(ps, buf, sizeof(buf), EXCEPTION_TIMEOUT, &bytes_read);

  http_close(pbb->http_handle);   /* error, close http connection */
  pbb->http_handle = 0;
  _DBG("bb_is_paper_in_adf .job_id=%d page_id=%d buf=%s \n", ps->job_id, ps->page_id, buf );
  if(strstr(buf, ADF_LOADED)) return 1;
  if(strstr(buf, ADF_EMPTY))
  {
     if (strstr(buf, SCANNER_BUSY_WITH_SCAN_JOB)) return 1;
     if (ps->currentInputSource ==IS_ADF_DUPLEX && ps->page_id % 2 == 1)
        return 1;
     else  
        return 0;
  }
  else return -1;
}


SANE_Status bb_start_scan(struct ledm_session *ps)
{
  char buf[4096] = {0};
  char buf1[1024]={0};
  int len, bytes_read, paper_status;
  int i, timeout = 10 ;
  char szPage_ID[5] = {0};
  char szJob_ID[5] = {0};
  SANE_Status stat = SANE_STATUS_IO_ERROR;
  struct bb_ledm_session *pbb = ps->bb_session;
  
  ps->user_cancel = 0;
  _DBG("bb_start_scan() entering...job_id=%d\n", ps->job_id);
  if (ps->job_id == 0)
  {
    if(http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
    {
        _BUG("unable to open channel HPMUD_S_LEDM_SCAN \n");
        goto bugout;
    }

    if (http_write(pbb->http_handle, GET_SCANNER_STATUS, sizeof(GET_SCANNER_STATUS)-1, timeout) != HTTP_R_OK)
    {
       _BUG("unable to GET_SCANNER_STATUS \n");
       goto bugout;
    }
 
    read_http_payload(ps, buf, sizeof(buf), timeout, &bytes_read);
     
    if(!strstr(buf, SCANNER_IDLE)) 
    {
        stat = SANE_STATUS_DEVICE_BUSY;
        goto bugout;
    }

    http_close(pbb->http_handle);
  	pbb->http_handle = 0;

    if(http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
    {
        _BUG("unable to open channel HPMUD_S_LEDM_SCAN \n");
        goto bugout;
    }

    len = snprintf(buf, sizeof(buf), CREATE_SCAN_JOB_REQUEST,
        ps->currentResolution,//<XResolution>
        ps->currentResolution,//<YResolution>
        (int) (ps->currentTlx / 5548.7133),//<XStart>
        (int) ((ps->currentBrx / 5548.7133) - (ps->currentTlx / 5548.7133)),//<Width>
        (int) (ps->currentTly / 5548.7133),//<YStart>
        (int) ((ps->currentBry / 5548.7133) - (ps->currentTly / 5548.7133)),//<Height>
        "Jpeg",//<Format>
        (! strcmp(ce_element[ps->currentScanMode], "Color8")) ? "Color" : (! strcmp(ce_element[ps->currentScanMode], "Gray8")) ? "Gray" : "Gray",//<ColorSpace>
        ((! strcmp(ce_element[ps->currentScanMode], "Color8")) || (! strcmp(ce_element[ps->currentScanMode], "Gray8"))) ? 8: 8,//<BitDepth>
        ps->currentInputSource == IS_PLATEN ? is_element[1] : is_element[2],//<InputSource>
        ps->currentInputSource == IS_PLATEN ? is_element[1] : is_element[2],//<InputSourceType>
        ps->currentInputSource != IS_ADF_DUPLEX ? "" : "<AdfOptions><AdfOption>Duplex</AdfOption></AdfOptions>",
        (int)ps->currentBrightness,//<Brightness>
        (int)ps->currentContrast);//<Contrast>

    len = len + strlen(ZERO_FOOTER);

    len = snprintf(buf1, sizeof(buf1), POST_HEADER, len);
    if (http_write(pbb->http_handle, buf1, strlen(buf1), timeout) != HTTP_R_OK)
    {
        //goto bugout;
    }
    
    if (http_write(pbb->http_handle, buf, strlen(buf), 1) != HTTP_R_OK)
    {
        //goto bugout;
    }

    /* Write zero footer. */
    if (http_write(pbb->http_handle, ZERO_FOOTER, sizeof(ZERO_FOOTER)-1, 1) != HTTP_R_OK)
    {
        //goto bugout;
    }
    memset(buf, 0, sizeof(buf));
    /* Read response. */
    if (read_http_payload(ps, buf, sizeof(buf), timeout, &bytes_read))
       goto bugout;

    http_close(pbb->http_handle);
    pbb->http_handle = 0;

    char joblist[64];
    char* jl=strstr(buf, "Location:");
    if (!jl) goto bugout;
    jl=jl+10;
	
    int i=0;
    while(*jl != '\r')
    { 
      joblist[i]=*jl; 
      jl=jl+1; i++;
    } 
    joblist[i]='\0';

    strcpy(ps->url, joblist);
    char *c=ps->url;
    c=strstr(c, "JobList"); 
    if (c)
    {
      c=c+8;
      int job_id=strtol(c, NULL, 10);
      itoa(job_id, szJob_ID,10);
      itoa(1, szPage_ID,10);
      ps->page_id = 1;
      ps->job_id = job_id;
    }
  }
  else
  {
    if (ps->currentInputSource == IS_PLATEN)
    {
       stat = SANE_STATUS_INVAL;
       goto bugout;
    }

    ps->page_id++;
    itoa(ps->job_id,szJob_ID,10);
    itoa(ps->page_id, szPage_ID,10);
  }
  _DBG("bb_start_scan() url=%s page_id=%d\n", ps->url, ps->page_id);
  
  memset(buf, 0, sizeof(buf)-1);

  if(http_open(ps->dd, HPMUD_S_LEDM_SCAN, &pbb->http_handle) != HTTP_R_OK)
  {
      _BUG("unable to open channel HPMUD_S_LEDM_SCAN \n");
      goto bugout;
  }
  while(strstr(buf, READY_TO_UPLOAD) == NULL)
  {
     _DBG("bb_start_scan() ENTERING....buf=%s\n", buf);
     len = snprintf(buf, sizeof(buf), GET_SCAN_JOB_URL, ps->url);

     if (http_write(pbb->http_handle, buf, strlen(buf), 1) != HTTP_R_OK)
     {
        //goto bugout;
        break ;
     }
     if (read_http_payload (ps, buf, sizeof(buf), 5, &len) != HTTP_R_OK)
     {
        //goto bugout
        _DBG("bb_start_scan() read_http_payload FAILED len=%d buf=%s\n", len, buf);
        break;
     }
      //For a new scan, buf must contain <PreScanPage>. 
     if (NULL == strstr(buf,PRESCANPAGE)) 
     {         //i.e Paper is not present in Scanner
         stat = SANE_STATUS_NO_DOCS;
         goto bugout;
     }
     if (strstr(buf,JOBSTATE_CANCELED) || strstr(buf, CANCELED_BY_DEVICE) || strstr(buf, CANCELED_BY_CLIENT))
     {
        _DBG("bb_start_scan() SCAN CANCELLED\n");
        stat = SANE_STATUS_GOOD;
        ps->user_cancel = 1;
        goto bugout;
     }
     if (strstr(buf, JOBSTATE_COMPLETED))
     {
        stat = SANE_STATUS_GOOD;
        goto bugout;
     }
     usleep(500000);//0.5 sec delay
  }//end while()

  char *c = strstr(buf, "<BinaryURL>");
  _DBG("bb_start_scan() BinaryURL=%s \n", c);
  
  if (!c) goto bugout;
  c +=11;
  char BinaryURL[30];
  i = 0;
  while(*c != '<')
  {
     BinaryURL[i++] = *c ;
     c++;
  }
  BinaryURL[i] = '\0';
  //_DBG("bb_start_scan() BinaryURL=%s\n", BinaryURL);
  len = snprintf(buf, sizeof(buf), GET_SCAN_JOB_URL, BinaryURL);
 
  if (http_write(pbb->http_handle, buf, strlen(buf), timeout) != HTTP_R_OK)
  {
 	//goto bugout;
  }
 
  if (http_read_header(pbb->http_handle, buf, sizeof(buf), timeout, &len) != HTTP_R_OK)
  {
	 //goto bugout;
  }

  if(strstr(buf, "HTTP/1.1 400 Bad Request")) http_read_header(pbb->http_handle, buf, sizeof(buf), timeout, &len);
  
  stat = SANE_STATUS_GOOD;
bugout:
  if (stat && pbb->http_handle)
  {
    http_close(pbb->http_handle);   /* error, close http connection */
    pbb->http_handle = 0;
  }
  return stat;
} /* bb_start_scan */

int get_size(struct ledm_session* ps)
{
  struct bb_ledm_session *pbb = ps->bb_session;
  char buffer[7];
  int i=0, tmo=50, len;

  if(ps->currentResolution >= 1200) tmo *= 5;
  
  while(1)
  {
    if(http_read_size(pbb->http_handle, buffer+i, 1, tmo, &len) == 2) return 0;
    if( i && *(buffer+i) == '\n' && *(buffer+i-1) == '\r') break;
    i++;
  }
  *(buffer+i+1)='\0';
  return strtol(buffer, NULL, 16);
}

int bb_get_image_data(struct ledm_session* ps, int maxLength) 
{
  struct bb_ledm_session *pbb = ps->bb_session;
  int size=0, stat=1;
  char buf_size[2];
  int len=0, tmo=50;
  _DBG("bb_get_image_data http_handle=%p cnt=%d pbb=%p\n", pbb->http_handle, ps->cnt, pbb);
  if(ps->currentResolution >= 1200) tmo *= 5;
 
  if (ps->cnt == 0)
  {
    size = get_size(ps);
    if(size == 0) 
    { 
      http_read_size(pbb->http_handle, buf_size, 2, tmo, &len);
      http_read_size(pbb->http_handle, buf_size, -1, tmo, &len);
      return 0; 
    }
    http_read_size(pbb->http_handle, ps->buf, size, tmo, &len);
    ps->cnt += len;
    http_read_size(pbb->http_handle, buf_size, 2, tmo, &len);
  }

  return stat=0;
}

int bb_end_page(struct ledm_session *ps, int io_error)
{
   struct bb_ledm_session *pbb = ps->bb_session;

  _DBG("bb_end_page(error=%d)\n", io_error);

   if (pbb->http_handle)
   {
      http_close(pbb->http_handle);
      pbb->http_handle = 0;
   }
   return 0;
}

int bb_end_scan(struct ledm_session* ps, int io_error)
{
  struct bb_ledm_session *pbb = ps->bb_session;

  _DBG("bb_end_scan(error=%d)\n", io_error);

  if (pbb->http_handle)
  {
    http_close(pbb->http_handle);
    pbb->http_handle = 0;
  }
  cancel_job(ps);
  memset(ps->url, 0, sizeof(ps->url));
  ps->job_id = 0;
  ps->page_id = 0;
  return 0;
}
