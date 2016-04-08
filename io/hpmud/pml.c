/*****************************************************************************\

  pml.c - get/set pml api for hpmud

  The get/set pml commands are a high level interface to hpmud. This hpmud system
  interface sits on top of the hpmud core interface. The system interface does 
  not use the hpmud memory map file system.
 
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include "hpmud.h"
#include "hpmudi.h"

#ifdef HAVE_LIBNETSNMP
#ifdef HAVE_UCDSNMP
#include <ucd-snmp/ucd-snmp-config.h>
#include <ucd-snmp/ucd-snmp-includes.h>
#else
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#endif
#endif

static int PmlOidToHex(const char *szoid, unsigned char *oid, int oidSize)
{
   char *tail;
   int i=0, val;

   if (szoid[0] == 0)
      goto bugout;

   val = strtol(szoid, &tail, 10);

   while (i < oidSize)
   {
      if (val > 128)
      {
         BUG("invalid oid size: oid=%s\n", szoid);
         goto bugout;
      }
      oid[i++] = (unsigned char)val;

      if (*tail == 0)
         break;         /* done */

      val = strtol(tail+1, &tail, 10);
   }

bugout:
   return i;
}

/* Convert ascii snmp oid to pml hex oid. */
static int SnmpToPml(const char *snmp_oid, unsigned char *oid, int oidSize)
{
   static const char hp_pml_mib_prefix[] = "1.3.6.1.4.1.11.2.3.9.4.2";
   static const char standard_printer_mib_prefix[] = "1.3.6.1.2.1.43";
   static const char host_resource_mib_prefix[] = "1.3.6.1.2.1.25";
   int len=0;

   if (strncmp(snmp_oid, hp_pml_mib_prefix, sizeof(hp_pml_mib_prefix)-1) == 0)
   {
      /* Strip out snmp prefix and convert to hex. */
      len = 0;
      len += PmlOidToHex(&snmp_oid[sizeof(hp_pml_mib_prefix)], &oid[0], oidSize);
      len--; /* remove trailing zero in pml mib */
   }
   else if   (strncmp(snmp_oid, standard_printer_mib_prefix, sizeof(standard_printer_mib_prefix)-1) == 0)
   {
      /* Replace snmp prefix with 2 and convert to hex. */
      len = 1;
      oid[0] = 0x2;
      len += PmlOidToHex(&snmp_oid[sizeof(standard_printer_mib_prefix)], &oid[1], oidSize);  
   }
   else if   (strncmp(snmp_oid, host_resource_mib_prefix, sizeof(host_resource_mib_prefix)-1) == 0)
   {
      /* Replace snmp prefix with 3 and convert to hex. */
      len = 1;
      oid[0] = 0x3;
      len += PmlOidToHex(&snmp_oid[sizeof(host_resource_mib_prefix)], &oid[1], oidSize);
   }
   else
      BUG("SnmpToPml failed snmp oid=%s\n", snmp_oid);

   return len;
}

#ifdef HAVE_LIBNETSNMP

static int SnmpErrorToPml(int snmp_error)
{
   int err;

   switch (snmp_error)
   {
      case SNMP_ERR_NOERROR:
         err = PML_EV_OK;
         break;
      case SNMP_ERR_TOOBIG:
         err = PML_EV_ERROR_BUFFER_OVERFLOW;
         break;
      case SNMP_ERR_NOSUCHNAME:
         err = PML_EV_ERROR_UNKNOWN_OBJECT_IDENTIFIER;
         break;
      case SNMP_ERR_BADVALUE:
         err = PML_EV_ERROR_INVALID_OR_UNSUPPORTED_VALUE;
         break;
      case SNMP_ERR_READONLY:
         err = PML_EV_ERROR_OBJECT_DOES_NOT_SUPPORT_REQUESTED_ACTION;
         break;
      case SNMP_ERR_GENERR:
      default:
         err = PML_EV_ERROR_UNKNOWN_REQUEST;
         break;
   }

   return err;
}

static int SetSnmp(const char *ip, int port, const char *szoid, int type, void *buffer, unsigned int size, int *pml_result, int *result)
{
   struct snmp_session session, *ss=NULL;
   struct snmp_pdu *pdu=NULL;
   struct snmp_pdu *response=NULL;
   oid anOID[MAX_OID_LEN];
   size_t anOID_len = MAX_OID_LEN;
   unsigned int i, len=0;
   uint32_t val;

   *result = HPMUD_R_IO_ERROR;
   *pml_result = PML_EV_ERROR_UNKNOWN_REQUEST;

   init_snmp("snmpapp");

   snmp_sess_init(&session );                   /* set up defaults */
   session.peername = (char *)ip;
   session.version = SNMP_VERSION_1;
   session.community = (unsigned char *)SnmpPort[port];
   session.community_len = strlen((const char *)session.community);
   ss = snmp_open(&session);                     /* establish the session */
   if (ss == NULL)
      goto bugout;

   pdu = snmp_pdu_create(SNMP_MSG_SET);
   read_objid(szoid, anOID, &anOID_len);

   switch (type)
   {
      case PML_DT_ENUMERATION:
      case PML_DT_SIGNED_INTEGER:
         /* Convert PML big-endian to SNMP little-endian byte stream. */
         for(i=0, val=0; i<size && i<sizeof(val); i++)    
            val = ((val << 8) | ((unsigned char *)buffer)[i]);
         snmp_pdu_add_variable(pdu, anOID, anOID_len, ASN_INTEGER, (unsigned char *)&val, sizeof(val));
         break;
      case PML_DT_REAL:
      case PML_DT_STRING:
      case PML_DT_BINARY:
      case PML_DT_NULL_VALUE:
      case PML_DT_COLLECTION:
      default:
         snmp_pdu_add_variable(pdu, anOID, anOID_len, ASN_OCTET_STR, buffer, size);
         break;
   }

  
   /* Send the request and get response. */
   if (snmp_synch_response(ss, pdu, &response) != STAT_SUCCESS)
      goto bugout;

   if (response->errstat == SNMP_ERR_NOERROR) 
   {
      len = size;
   }

   *pml_result = SnmpErrorToPml(response->errstat);
   *result = HPMUD_R_OK;

bugout:
   if (response != NULL)
      snmp_free_pdu(response);
   if (ss != NULL)
      snmp_close(ss);
   return len;
}

int __attribute__ ((visibility ("hidden"))) GetSnmp(const char *ip, int port, const char *szoid, void *buffer, unsigned int size, int *type, int *pml_result, int *result)
{
   struct snmp_session session, *ss=NULL;
   struct snmp_pdu *pdu=NULL;
   struct snmp_pdu *response=NULL;
   unsigned int i, len=0;
   oid anOID[MAX_OID_LEN];
   size_t anOID_len = MAX_OID_LEN;
   struct variable_list *vars;
   uint32_t val;
   unsigned char tmp[sizeof(uint32_t)];

   *result = HPMUD_R_IO_ERROR;
   *type = PML_DT_NULL_VALUE;
   *pml_result = PML_EV_ERROR_UNKNOWN_REQUEST;

   init_snmp("snmpapp");

   snmp_sess_init(&session );                   /* set up defaults */
   session.peername = (char *)ip;
   session.version = SNMP_VERSION_1;
   session.community = (unsigned char *)SnmpPort[port];
   session.community_len = strlen((const char *)session.community);
   session.retries = 1;
   session.timeout = 1000000;         /* 1 second */
   ss = snmp_open(&session);                     /* establish the session */
   if (ss == NULL)
      goto bugout;

   pdu = snmp_pdu_create(SNMP_MSG_GET);
   read_objid(szoid, anOID, &anOID_len);
   snmp_add_null_var(pdu, anOID, anOID_len);
  
   /* Send the request and get response. */
   if (snmp_synch_response(ss, pdu, &response) != STAT_SUCCESS)
      goto bugout;

   if (response->errstat == SNMP_ERR_NOERROR) 
   {
      vars = response->variables;
      switch (vars->type)
      {
         case ASN_INTEGER:
            *type = PML_DT_SIGNED_INTEGER;

            /* Convert SNMP little-endian to PML big-endian byte stream. */
            len = (sizeof(uint32_t) < size) ? sizeof(uint32_t) : size;
            val = *vars->val.integer;
            for(i=len; i>0; i--)
            {
               tmp[i-1] = val & 0xff;
               val >>= 8;
            }

            /* Remove any in-significant bytes. */
            for (; tmp[i]==0 && i<len; i++)
               ;
            len -= i;

            memcpy(buffer, tmp+i, len);
            break;
         case ASN_NULL:
            *type = PML_DT_NULL_VALUE;
            break;
         case ASN_OCTET_STR:
            *type = PML_DT_STRING;
            len = (vars->val_len < size) ? vars->val_len : size;
            memcpy(buffer, vars->val.string, len);
            break;
         default:
            BUG("unable to GetSnmp: data type=%d\n", vars->type);
            goto bugout;
            break;
      }
   }

   *pml_result = SnmpErrorToPml(response->errstat);
   *result = HPMUD_R_OK;

bugout:
   if (response != NULL)
      snmp_free_pdu(response);
   if (ss != NULL)
      snmp_close(ss);
   return len;
}

#else

int __attribute__ ((visibility ("hidden"))) SetSnmp(const char *ip, int port, const char *szoid, int type, void *buffer, unsigned int size, int *pml_result, int *result)
{
   BUG("no JetDirect support enabled\n");
   return 0;
}

int __attribute__ ((visibility ("hidden"))) GetSnmp(const char *ip, int port, const char *szoid, void *buffer, unsigned int size, int *type, int *pml_result, int *result)
{
   BUG("no JetDirect support enabled\n");
   return 0;
}

#endif /* HAVE_LIBSNMP */

/* Set a PML object in the hp device. */
enum HPMUD_RESULT hpmud_set_pml(HPMUD_DEVICE device, HPMUD_CHANNEL channel, const char *snmp_oid, int type, void *data, int data_size, int *pml_result)
{
   unsigned char message[HPMUD_BUFFER_SIZE];
   unsigned char oid[HPMUD_LINE_SIZE];
   char ip[HPMUD_LINE_SIZE], *psz, *tail;
   unsigned char *p=message;
   int len, dLen, result, reply, status, port;
   struct hpmud_dstat ds;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   DBG("[%d] hpmud_set_pml() dd=%d cd=%d oid=%s type=%d data=%p size=%d\n", getpid(), device, channel, snmp_oid, type, data, data_size);

   if ((result = hpmud_get_dstat(device, &ds)) != HPMUD_R_OK)
   {
      stat = result;
      goto bugout;
   }

   if (strcasestr(ds.uri, "net/") != NULL)
   {
      /* Process pml via snmp. */

      hpmud_get_uri_datalink(ds.uri, ip, sizeof(ip));

      if ((psz = strstr(ds.uri, "port=")) != NULL)
         port = strtol(psz+5, &tail, 10);
      else
         port = PORT_PUBLIC;

      SetSnmp(ip, port, snmp_oid, type, data, data_size, &status, &result);
      if (result != HPMUD_R_OK)
      {
         BUG("SetPml failed ret=%d\n", result);
         stat = result;
         goto bugout;       
      }
   }       
   else
   {
      /* Process pml via local transport. */

      /* Convert snmp ascii oid to pml hex oid. */
      dLen = SnmpToPml(snmp_oid, oid, sizeof(oid));
   
      *p++ = PML_SET_REQUEST;
      *p++ = PML_DT_OBJECT_IDENTIFIER;
      *p++ = dLen;                          /* assume oid length is < 10 bits */
      memcpy(p, oid, dLen);
      p+=dLen;
      *p = type;
      *p |= data_size >> 8;                   /* assume data length is 10 bits */
      *(p+1) = data_size & 0xff;    
      p += 2; 
      memcpy(p, data, data_size);

      result = hpmud_write_channel(device, channel, message, dLen+data_size+3+2, HPMUD_EXCEPTION_SEC_TIMEOUT, &len);  
      if (result != HPMUD_R_OK)
      {
         BUG("SetPml channel_write failed ret=%d\n", result);
         stat = result;
         goto bugout;       
      }    

      result = hpmud_read_channel(device, channel, message, sizeof(message), HPMUD_EXCEPTION_SEC_TIMEOUT, &len);
      if (result != HPMUD_R_OK || len == 0)
      {
         BUG("SetPml channel_read failed ret=%d len=%d\n", result, len);
         goto bugout;       
      }    

      p = message;
      reply = *p++;       /* read command reply */
      status = *p++;      /* read execution outcome */

      if (reply != (PML_SET_REQUEST | 0x80) && status & 0x80)
      {
         BUG("SetPml failed reply=%x outcome=%x\n", reply, status);
         DBG_DUMP(p, len-2);
         goto bugout;       
      }   
   }
   
   *pml_result = status;
   stat = HPMUD_R_OK;

   DBG("set_pml result pmlresult=%x\n", status);

bugout:
   return stat;
}

/* Get a PML object from the hp device. */
enum HPMUD_RESULT hpmud_get_pml(HPMUD_DEVICE device, HPMUD_CHANNEL channel, const char *snmp_oid, void *buf, int buf_size, int *bytes_read, int *type, int *pml_result)
{
   unsigned char message[HPMUD_BUFFER_SIZE];
   unsigned char oid[HPMUD_LINE_SIZE];
   char ip[HPMUD_LINE_SIZE], *psz, *tail;
   unsigned char *p=message;
   int len, dLen, result, reply, status, dt, port;
   struct hpmud_dstat ds;
   enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

   DBG("[%d] hpmud_get_pml() dd=%d cd=%d oid=%s data=%p size=%d\n", getpid(), device, channel, snmp_oid, buf, buf_size);

   if ((result = hpmud_get_dstat(device, &ds)) != HPMUD_R_OK)
   {
      stat = result;
      goto bugout;
   }

   if (strcasestr(ds.uri, "net/") != NULL)
   {
      /* Process pml via snmp. */

      hpmud_get_uri_datalink(ds.uri, ip, sizeof(ip));

      if ((psz = strstr(ds.uri, "port=")) != NULL)
         port = strtol(psz+5, &tail, 10);
      else
         port = PORT_PUBLIC;

      dLen = GetSnmp(ip, port, snmp_oid, message, sizeof(message), &dt, &status, &result);
      if (result != HPMUD_R_OK)
      {
        //Try one more time with previous default community name string ("public.1" which was used for old HP printers)  
        dLen = GetSnmp(ip, PORT_PUBLIC_1, snmp_oid, message, sizeof(message), &dt, &status, &result);
        if (result != HPMUD_R_OK)
        {
            BUG("GetPml failed ret=%d\n", result);
            stat = result;
            goto bugout;
        }
      }
      p = message;    
   }       
   else
   {
      /* Process pml via local transport. */

      /* Convert snmp ascii oid to pml hex oid. */
      dLen = SnmpToPml(snmp_oid, oid, sizeof(oid));
   
      *p++ = PML_GET_REQUEST;
      *p++ = PML_DT_OBJECT_IDENTIFIER;
      *p++ = dLen;                          /* assume oid length is < 10 bits */
 
      memcpy(p, oid, dLen);
      result = hpmud_write_channel(device, channel, message, dLen+3, HPMUD_EXCEPTION_SEC_TIMEOUT, &len);
      if (result != HPMUD_R_OK)
      {
         BUG("GetPml channel_write failed ret=%d\n", result);
         stat = result;
         goto bugout;       
      }    

      result = hpmud_read_channel(device, channel, message, sizeof(message), HPMUD_EXCEPTION_SEC_TIMEOUT, &len);
      if (result != HPMUD_R_OK || len == 0)
      {
         BUG("GetPml channel_read failed ret=%d len=%d\n", result, len);
         goto bugout;       
      }    

      p = message;
      reply = *p++;       /* read command reply */
      status = *p++;      /* read execution outcome */

      if (reply != (PML_GET_REQUEST | 0x80) && status & 0x80)
      {
         BUG("GetPml failed reply=%x outcome=%x\n", reply, status);
         DBG_DUMP(p, len-2);
         goto bugout;       
      }   

      dt = *p++;       /* read data type */

      if (dt == PML_DT_ERROR_CODE)
      {
         /* Ok, but invalid data type requested, get new data type. */
         p += 2;       /* eat length and err code */
         dt = *p++;  /* read data type */
      } 

      if (dt != PML_DT_OBJECT_IDENTIFIER)
      {
         BUG("GetPml failed data type=%x\n", dt);
         goto bugout;       
      }   

      dLen = *p++;     /* read oid length */
      p += dLen;       /* eat oid */

      dt = *p;    /* read data type. */
      dLen = ((*p & 0x3) << 8 | *(p+1));         /* read 10 bit len from 2 byte field */
      p += 2;                               /* eat type and length */
   }

   if (dLen > buf_size)
        dLen = buf_size;

   memcpy(buf, p, dLen);
   *bytes_read = dLen; 
   *type = dt;
   *pml_result = status;
   stat = HPMUD_R_OK;

   DBG("get_pml result len=%d datatype=%x pmlresult=%x\n", dLen, dt, status);
   DBG_DUMP(buf, dLen);

bugout:
   return stat;
}


