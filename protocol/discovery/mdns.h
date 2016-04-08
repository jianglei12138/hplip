/*****************************************************************************\

  mdns.h - mDNS related calls 
 
  (c) 2015 Copyright HP Development Company, LP

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

  Author: Sanjay Kumar
\*****************************************************************************/

#ifndef _DISCOVERY_MDNS_H
#define _DISCOVERY_MDNS_H

//MDNS Packet fields
#define QTYPE_A     1
#define QTYPE_TXT  16
#define QTYPE_SRV  33
#define QTYPE_AAAA 28
#define QTYPE_PTR  12
#define QCLASS_IN   1
#define MDNS_HEADER_SIZE  12

//Error Codes
#define MDNS_STATUS_OK 0
#define MDNS_STATUS_ERROR 1
#define MDNS_STATUS_TIMEOUT 2

#define MAX_IP_ADDR_LEN 16
#define MAX_URI_LEN 256
#define MAX_MDL_NAME_LEN 256
#define MAX_NAME_LENGTH 256
#define MAX_MDNS_RESPONSE_LEN 2048
#define MODE_READ_ALL 0
#define MODE_READ_SINGLE 1

/*Relevant MDNS Resource Record(RR) fields */
typedef struct _DNS_RECORD
{
    char ip[MAX_IP_ADDR_LEN];
    char mdl[MAX_MDL_NAME_LEN];
    char name[MAX_MDL_NAME_LEN];
    struct _DNS_RECORD *next;
}DNS_RECORD;

typedef struct _DNS_PKT_HEADER
{
    unsigned short  id;
    unsigned short  flags;
    unsigned short  questions;
    unsigned short  answers;
    unsigned short  authorities;
    unsigned short  additionals;
}DNS_PKT_HEADER;


//#define MDNS_DEBUG

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define BUG(args...) syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args)
#ifdef MDNS_DEBUG
   #define DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)
#else
   #define DBG(args...)
#endif

/*Function Prototypes*/
int   mdns_probe_nw_scanners(char* buf, int buf_size, int *count);
int   mdns_lookup(char* hostname, unsigned char* ip);


/*Helper Function Prototypes*/
static int   mdns_convert_name_to_dns(const char *name, int name_size, char *dns_name);
static int   mdns_read_single_response(int udp_socket, char *recvbuffer, int recvbufsize);
static int   mdns_open_socket(int *psocket);
static int   mdns_send_query(int udp_socket, char *fqdn, int query_type);
static int   mdns_readName(unsigned char* start, unsigned char *p, char *buf);
static int   mdns_update_uris(DNS_RECORD *rr, char* uris_buf, int buf_size, int *count);
static void  mdns_create_query_packet(char* fqdn, int query_type, char* dnsquery, int *length);
static void  mdns_read_header(char *Response, DNS_PKT_HEADER *h);
static void  mdns_parse_respponse(unsigned char *Response, DNS_RECORD *rr);
static void  mdns_rr_cleanup(DNS_RECORD *rr);
static DNS_RECORD *mdns_read_responses(int udp_socket, int mode);
static unsigned char* mdns_readMDL(unsigned char *p, unsigned char *normalized_mdl, int len);
#endif // _DISCOVERY_MDNS_H

