/*****************************************************************************\

  hpmud.h - public definitions for multi-point transport driver

  (c) 2004-2015 Copyright HP Development Company, LP

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

  Author: Naga Samrat Chowdary Narla, Yashwant Sahu, Sarbeswar Meher
\*****************************************************************************/

#ifndef _HPMUD_H
#define _HPMUD_H

enum HPMUD_RESULT
{
   HPMUD_R_OK = 0,
   HPMUD_R_INVALID_DEVICE = 2,
   HPMUD_R_INVALID_DESCRIPTOR = 3,
   HPMUD_R_INVALID_URI = 4,
   HPMUD_R_INVALID_LENGTH = 8,
   HPMUD_R_IO_ERROR = 12,
   HPMUD_R_DEVICE_BUSY = 21,
   HPMUD_R_INVALID_SN = 28,
   HPMUD_R_INVALID_CHANNEL_ID = 30,
   HPMUD_R_INVALID_STATE = 31,
   HPMUD_R_INVALID_DEVICE_OPEN = 37,
   HPMUD_R_INVALID_DEVICE_NODE = 38,
   HPMUD_R_INVALID_IP = 45,
   HPMUD_R_INVALID_IP_PORT = 46,
   HPMUD_R_INVALID_TIMEOUT = 47,
   HPMUD_R_DATFILE_ERROR = 48,
   HPMUD_R_IO_TIMEOUT = 49,
   HPMUD_R_INVALID_MDNS = 50,
};

enum HPMUD_IO_MODE
{
   HPMUD_UNI_MODE=0, /* uni-di */
   HPMUD_RAW_MODE=1,   /* bi-di */
   HPMUD_DOT4_MODE=3,
   HPMUD_DOT4_PHOENIX_MODE=4,  /* (ie: clj2550, clj2840, lj3050, lj3055, clj4730mfp) */
   HPMUD_DOT4_BRIDGE_MODE=5,  /* (ie: clj2500) not USB compatable, use HPMUD_RAW_MODE, tested on F10 12/10/08 DES */
   HPMUD_MLC_GUSHER_MODE=6,   /* most new devices */
   HPMUD_MLC_MISER_MODE=7,  /* old stuff */
};

enum HPMUD_BUS_ID
{
   HPMUD_BUS_NA=0,
   HPMUD_BUS_USB=1,
   HPMUD_BUS_PARALLEL,
   HPMUD_BUS_ALL
};

enum HPMUD_DEVICE_TYPE
{
   HPMUD_AIO=0,
   HPMUD_PRINTER=1,
   HPMUD_SCANNER=2,
};

enum HPMUD_SCANTYPE
{
   HPMUD_SCANTYPE_NA = 0,
   HPMUD_SCANTYPE_SCL = 1,
   HPMUD_SCANTYPE_PML = 2,
   HPMUD_SCANTYPE_SOAP = 3,    /* Wookie (ie:ljcm1017) */
   HPMUD_SCANTYPE_MARVELL = 4,     /* (ie: ljm1005) */
   HPMUD_SCANTYPE_SOAPHT = 5,   /* HorseThief (ie: ljm1522) */
   HPMUD_SCANTYPE_SCL_DUPLEX = 6,
   HPMUD_SCANTYPE_LEDM = 7,
   HPMUD_SCANTYPE_MARVELL2 = 8,     /* (Tsunami lj 1212  and series) */
   HPMUD_SCANTYPE_ESCL=9,
};

enum HPMUD_SCANSRC
{
   HPMUD_SCANSRC_NA = 0,
   HPMUD_SCANSRC_FLATBED = 0x1,
   HPMUD_SCANSRC_ADF= 0x2,
   HPMUD_SCANSRC_CAMERA = 0x4,
};

enum HPMUD_STATUSTYPE
{
   HPMUD_STATUSTYPE_NA = 0,
   HPMUD_STATUSTYPE_VSTATUS = 1,  /* device-id vstatus */
   HPMUD_STATUSTYPE_SFIELD = 2,   /* device-id s-field */
   HPMUD_STATUSTYPE_PML = 3,      /* laserjet pml */
   HPMUD_STATUSTYPE_EWS = 6,      /* laserjet hp ews */
   HPMUD_STATUSTYPE_PJL = 8,      /* laserjet pjl */
   HPMUD_STATUSTYPE_PJLPML = 9,   /* laserjet pjl and pml */
};

enum HPMUD_SUPPORT_TYPE
{
   HPMUD_SUPPORT_TYPE_NONE = 0,   /* not supported */
   HPMUD_SUPPORT_TYPE_HPIJS = 1,  /* supported by hpijs only */
   HPMUD_SUPPORT_TYPE_HPLIP = 2,   /* supported by hpijs and "hp" backend */
};

enum HPMUD_PLUGIN_TYPE
{
   HPMUD_PLUGIN_TYPE_NONE = 0,
   HPMUD_PLUGIN_TYPE_REQUIRED = 1,
   HPMUD_PLUGIN_TYPE_OPTIONAL = 2,
};



#define HPMUD_S_PRINT_CHANNEL "PRINT"
#define HPMUD_S_PML_CHANNEL "HP-MESSAGE"
#define HPMUD_S_SCAN_CHANNEL "HP-SCAN"
#define HPMUD_S_FAX_SEND_CHANNEL "HP-FAX-SEND"
#define HPMUD_S_CONFIG_UPLOAD_CHANNEL "HP-CONFIGURATION-UPLOAD"
#define HPMUD_S_CONFIG_DOWNLOAD_CHANNEL "HP-CONFIGURATION-DOWNLOAD"
#define HPMUD_S_MEMORY_CARD_CHANNEL "HP-CARD-ACCESS"
#define HPMUD_S_EWS_CHANNEL "HP-EWS"
#define HPMUD_S_EWS_LEDM_CHANNEL "HP-EWS-LEDM"
#define HPMUD_S_SOAP_SCAN "HP-SOAP-SCAN"
#define HPMUD_S_SOAP_FAX "HP-SOAP-FAX"
#define HPMUD_S_DEVMGMT_CHANNEL "HP-DEVMGMT"
#define HPMUD_S_MARVELL_SCAN_CHANNEL "HP-MARVELL-SCAN"
#define HPMUD_S_MARVELL_FAX_CHANNEL "HP-MARVELL-FAX"
#define HPMUD_S_LEDM_SCAN "HP-LEDM-SCAN"
#define HPMUD_S_WIFI_CHANNEL "HP-WIFICONFIG"
#define HPMUD_S_MARVELL_EWS_CHANNEL "HP-MARVELL-EWS"
#define HPMUD_S_IPP_CHANNEL "HP-IPP"
#define HPMUD_S_IPP_CHANNEL2 "HP-IPP2"
#define HPMUD_S_ESCL_SCAN "HP-ESCL-SCAN"

typedef int HPMUD_DEVICE;       /* usb, parallel or jetdirect */
#define HPMUD_DEVICE_MAX 2      /* zero is not used */

typedef int HPMUD_CHANNEL;
#define HPMUD_CHANNEL_MAX HPMUD_MAX_CHANNEL_ID

#define HPMUD_LINE_SIZE 256     /* Length of a line. */
#define HPMUD_BUFFER_SIZE 16384  /* General Read/Write buffer. */

struct hpmud_dstat
{
   char uri[HPMUD_LINE_SIZE];
   int client_cnt;                  /* number of clients that have this device opend */
   enum HPMUD_IO_MODE io_mode;
   int channel_cnt;                 /* number of open channels */
   int mlc_up;                      /* 0 = MLC/1284.4 transport up, 1 = MLD/1284.4 transport down */
};

struct hpmud_model_attributes
{
   enum HPMUD_IO_MODE prt_mode;        /* print only (io_mode) */
   enum HPMUD_IO_MODE mfp_mode;        /* pml | scan | fax (io_mode) */
   enum HPMUD_SCANTYPE scantype;       /* scan protocol i.e. SCL, PML, SOAP, MARVELL, LEDM */
   enum HPMUD_STATUSTYPE statustype;
   enum HPMUD_SUPPORT_TYPE support;
   enum HPMUD_PLUGIN_TYPE plugin;
   enum HPMUD_SUPPORT_TYPE reserved[5];
   enum HPMUD_SCANSRC scansrc; /*Flatbed, ADF, Camera or combination of these*/
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * hpmud_device_open - open specified device, call normally does not block
 *
 * inputs:
 *  uri - specifies device to open
 *  io_mode - see enum definition
 *
 * outputs:
 *  dd - device descriptor
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_open_device(const char *uri, enum HPMUD_IO_MODE io_mode, HPMUD_DEVICE *dd);

/*
 * hpmud_device_close - close specified device, call does not block
 *
 * inputs:
 *  dd - device descriptor
 *
 * outputs:
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_close_device(HPMUD_DEVICE dd);

/*
 * hpmud_get_device_id - read IEEE 1284 device ID string, call normally does not block
 *
 * If the device is busy, a cached copy may be returned.
 *
 * inputs:
 *  dd - device descriptor
 *  buf_size - maximum size of buf
 *
 * outputs:
 *  buf - zero terminated device ID string
 *  bytes_read - size of device ID string, does not include zero termination
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_get_device_id(HPMUD_DEVICE dd, char *buf, int buf_size, int *bytes_read);

/*
 * hpmud_get_device_status - read 8-bit device status, call normally does not block
 *
 * inputs:
 *  dd - device descriptor
 *
 * outputs:
 *  status - 3-bit status, supported by inkjets only
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_get_device_status(HPMUD_DEVICE dd, unsigned int *status);

/*
 * hpmud_probe_devices - probe local buses for HP supported devices, call normally does not block
 *
 * inputs:
 *  bus - see enum definiton
 *  buf_size - size of read buffer
 *
 * outputs:
 *  buf - zero terminated CUPS backend formatted data
 *  cnt - number of HP devices found
 *  bytes_read - number of bytes actually read
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_probe_devices(enum HPMUD_BUS_ID bus, char *buf, int buf_size, int *cnt, int *bytes_read);

/*
 * hpmud_probe_printers - probe local buses for HP supported printers, call normally does not block
 *
 * inputs:
 *  bus - see enum definiton
 *  buf_size - size of read buffer
 *
 * outputs:
 *  buf - zero terminated CUPS backend formatted data
 *  cnt - number of HP devices found
 *  bytes_read - number of bytes actually read
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_probe_printers(enum HPMUD_BUS_ID bus, char *buf, int buf_size, int *cnt, int *bytes_read);

/*
 * hpmud_channel_open - open specified channel, call will block
 *
 * Only EWS channel can be opened by more than one process.
 *
 * inputs:
 *  dd - device descriptor
 *  channel_name - requested service name
 *
 * outputs:
 *  cd - channel descriptor
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_open_channel(HPMUD_DEVICE dd, const char *channel_name, HPMUD_CHANNEL *cd);

/*
 * hpmud_channel_close - close specified channel, call will block
 *
 * inputs:
 *  dd - device descriptor
 *  cd - channel descriptor
 *
 * outputs:
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_close_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd);

/*
 * hpmud_channel_write - write data to specified channel, call will block
 *
 * May return with partial bytes written (ie: bytes_wrote < size) with or with-out a timeout.
 *
 * inputs:
 *  dd - device descriptor
 *  cd - channel descriptor
 *  buf - data to write
 *  size - number of bytes to write
 *  timeout - in seconds
 *
 * outputs:
 *  bytes_wrote - number of bytes actually wrote
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_write_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd, const void *buf, int size, int timeout, int *bytes_written);

/*
 * hpmud_channel_read - read data from specified channel, call will block
 *
 * May return with partial bytes read (ie: bytes_read < size) or zero if timeout occured.
 *
 * inputs:
 *  dd - device descriptor
 *  cd - channel descriptor
 *  size - number of bytes to read
 *  timeout - in seconds
 *
 * outputs:
 *  buf - read data buffer
 *  bytes_read - number of bytes actually read
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_read_channel(HPMUD_DEVICE dd, HPMUD_CHANNEL cd, void *buf, int size, int timeout, int *bytes_read);

/*
 * hpmud_dstat - get device information
 *
 * inputs:
 *  dd - device descriptor
 *
 * outputs:
 *  ds - see dstat definition
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_get_dstat(HPMUD_DEVICE dd, struct hpmud_dstat *ds);

/*
 * hpmud_set_pml - set pml object
 *
 * Set_pml is a high level interface to hpmud. This command calls the hpmud core interface.
 * This command can be used with local or jetdirect connections. Jetdirect connection will
 * use snmp.
 *
 * inputs:
 *  dd - device descriptor
 *  cc - channel descriptor
 *  snmp_oid - snmp encoded pml oid
 *  type - oid data type
 *  data - data payload
 *  data_size - number of bytes to write
 *
 * outputs:
 *  pml_result
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_set_pml(HPMUD_DEVICE device, HPMUD_CHANNEL channel, const char *snmp_oid, int type, void *data, int data_size, int *pml_result);

/*
 * hpmud_get_pml - get pml object
 *
 * Get_pml is a high level interface to hpmud. This command calls the hpmud core interface.
 * This command can be used with local or jetdirect connections. Jetdirect connection will
 * use snmp.
 *
 * inputs:
 *  dd - device descriptor
 *  cc - channel descriptor
 *  snmp_oid - snmp encoded pml oid
 *  data_size - data buffer size in bytes
 *
 * outputs:
 *  data - data payload
 *  type - pml data type
 *  pml_result
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_get_pml(HPMUD_DEVICE device, HPMUD_CHANNEL channel, const char *snmp_oid, void *buf, int buf_size, int *bytes_read, int *type, int *pml_result);

/*
 * hpmud_get_model - parse device model from the IEEE 1284 device id string.
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  id - IEEE 1284 device id string
 *  buf_size - size of buf in bytes
 *
 * outputs:
 *  buf - device model string (generalized)
 *  return value - length of string in bytes, does not include zero termination
 */
int hpmud_get_model(const char *id, char *buf, int buf_size);

/*
 * hpmud_get_raw_model - parse device model from the IEEE 1284 device id string.
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  id - IEEE 1284 device id string
 *  buf_size - size of buf in bytes
 *
 * outputs:
 *  buf - device model string (raw)
 *  return value - length of string in bytes, does not include zero termination
 */
int hpmud_get_raw_model(char *id, char *raw, int rawSize);

/*
 * hpmud_get_uri_model - parse device model from uri
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  uri
 *  buf_size - size of buf in bytes
 *
 * outputs:
 *  buf - device model string
 *  return value - length of string in bytes, does not include zero termination
 */
int hpmud_get_uri_model(const char *uri, char *buf, int buf_size);

/*
 * hpmud_get_uri_datalink - parse the data link from uri
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  uri
 *  buf_size - size of buf in bytes
 *
 * outputs:
 *  buf - device model string
 *  return value - length of string in bytes, does not include zero termination
 */
int hpmud_get_uri_datalink(const char *uri, char *buf, int buf_size);

/*
 * hpmud_get_model_attributes - get all model attributes for specified device
 *
 * Reads device model attributes from models.dat file. This function is a
 * stateless hpmud helper function.
 *
 * inputs:
 *  uri - specifies device
 *  buf_size - size of buf in bytes
 *
 * outputs:
 *  buf - buffer for all model attributes, key/value pair, one per line
 *  bytes_read - number of bytes actually read
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_get_model_attributes(char *uri, char *attr, int attrSize, int *bytes_read);

/*
 * hpmud_model_query - get model attributes structure for specified device
 *
 * Reads device model attributes from models.dat file. This function is a
 * stateless hpmud helper function.
 *
 * inputs:
 *  uri - specifies device
 *
 * outputs:
 *  ma - see structure definition
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_query_model(char *uri, struct hpmud_model_attributes *ma);

/*
 * hpmud_make_usb_uri - make a usb uri from bus:dev pair
 *
 * This function is a stateless hpmud helper function. The lsusb command can be used
 * determine the bus:dev pair.
 *
 * inputs:
 *  busnum - specifies usbfs bus number
 *  devnum - specifies usbfs device number
 *  uri_size - size of uri buffer in bytes
 *
 * outputs:
 *  uri - zero terminated string
 *  bytes_read - size of uri
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_make_usb_uri(const char *busnum, const char *devnum, char *uri, int uri_size, int *bytes_read);

/*
 * hpmud_make_usb_serial_uri - make a usb uri from product serial number
 *
 * This function is a stateless hpmud helper function. The lsusb command can be used
 * determine the product serial number.
 *
 * inputs:
 *  sn - specifies product serial number
 *  uri_size - size of uri buffer in bytes
 *
 * outputs:
 *  uri - zero terminated string
 *  bytes_read - size of uri
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_make_usb_serial_uri(const char *sn, char *uri, int uri_size, int *bytes_read);

/*
 * hpmud_make_net_uri - make a net uri from IP
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  ip - internet address
 *  port - 1-4
 *  uri_size - size of uri buffer in bytes
 *
 * outputs:
 *  uri - zero terminated string
 *  bytes_read - size of uri
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_make_net_uri(const char *ip, int port, char *uri, int uri_size, int *bytes_read);

/*
 * hpmud_make_par_uri - make a par uri from parallel port
 *
 * This function is a stateless hpmud helper function.
 *
 * inputs:
 *  dnode - device node
 *  uri_size - size of uri buffer in bytes
 *
 * outputs:
 *  uri - zero terminated string
 *  bytes_read - size of uri
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_make_par_uri(const char *dnode, char *uri, int uri_size, int *bytes_read);


/*
 * hpmud_make_mdns_uri - make a network uri from host name
 *
 * This function is a stateless hpmud helper function. Requires UDP port 5353 to be open.
 *
 * inputs:
 *  host - zero terminated string (ie: "npi7c8a3e")
 *  uri_size - size of uri buffer in bytes
 *
 * outputs:
 *  uri - zero terminated string
 *  bytes_read - size of uri
 *  return value - see enum definition
 */
enum HPMUD_RESULT hpmud_make_mdns_uri(const char *host, int port, char *uri, int uri_size, int *bytes_read);

#ifdef __cplusplus
}
#endif

#endif // _HPMUD_H

/*********************** For Python 2.X and 3.X support ***************************/


#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);
  #define FORMAT_STRING "(iy#ii)"
  #define FORMAT_STRING1 "(iy#)"
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT(name) void init##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          ob = Py_InitModule3(name, methods, doc);
  #define FORMAT_STRING "(is#ii)"
  #define FORMAT_STRING1 "(is#)"
#endif
