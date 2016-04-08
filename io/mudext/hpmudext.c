/*****************************************************************************\
 hpmudext - Python extension for HP multi-point transport driver (HPMUD)

 (c) Copyright 2015 HP Development Company, L.P.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

Requires:
Python 2.2+

Authors: Don Welch, David Suffield, Naga Samrat Chowdary Narla

\*****************************************************************************/

#include <Python.h>
#include <stdarg.h>
#include "hpmud.h"
#include "hpmudi.h"

/* Ref: PEP 353 (Python 2.5) */
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)




/*
HPMUDEXT API:

result_code, dd = open_device(uri, io_mode)

result_code = close_device(dd)

result_code, data = get_device_id(dd)

result_code, data = probe_devices(bus)

result_code, cd = open_channel(dd, channel_name)

result_code = close_channel(dd, cd)

result_code, bytes_written = write_channel(dd, cd, data, [timeout])

result_code, data = read_channel(dd, cd, bytes_to_read, [timeout])

result_code, pml_result_code = set_pml(dd, cd, oid, type, data)

result_code, data, pml_result_code = get_pml(dd, cd, oid, type)

result_code, uri = make_usb_uri(busnum, devnum)

result_code, uri = make_net_uri(ip, port)

result_code, uri = make_zc_uri(ip, port)

result_code, ip_address = get_zc_ip_address(hostname)

result_code, uri = make_par_uri(devnode)

*/


 static PyObject *open_device(PyObject *self, PyObject *args)
{
    char * uri;
    enum HPMUD_IO_MODE io_mode;
    HPMUD_DEVICE dd;
    enum HPMUD_RESULT result = HPMUD_R_OK;

    if (!PyArg_ParseTuple(args, "si", &uri, &io_mode))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_open_device(uri, io_mode, &dd);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(ii)", result, dd);
}

static PyObject *close_device(PyObject *self, PyObject *args)
{
    HPMUD_DEVICE dd;
    enum HPMUD_RESULT result = HPMUD_R_OK;

    if (!PyArg_ParseTuple(args, "i", &dd))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_close_device(dd);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", result);
}

static PyObject *get_device_id(PyObject *self, PyObject *args)
{
    HPMUD_DEVICE dd;
    enum HPMUD_RESULT result = HPMUD_R_OK;
    char buf[HPMUD_BUFFER_SIZE];
    int bytes_read = 0;

    if (!PyArg_ParseTuple(args, "i", &dd))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_get_device_id(dd, buf, HPMUD_BUFFER_SIZE, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(is#)", result, buf, bytes_read);

}

static PyObject *probe_devices(PyObject *self, PyObject *args)
{
    enum HPMUD_BUS_ID bus;
    enum HPMUD_RESULT result = HPMUD_R_OK;
    int cnt = 0;
    int bytes_read = 0;
    char buf[HPMUD_BUFFER_SIZE * 4];

    if (!PyArg_ParseTuple(args, "i", &bus))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_probe_devices(bus, buf, HPMUD_BUFFER_SIZE * 4, &cnt, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(is#)", result, buf, bytes_read);
}

static PyObject *open_channel(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd = -1;
    char * channel_name;
    HPMUD_CHANNEL cd = -1;

    if (!PyArg_ParseTuple(args, "is", &dd, &channel_name))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_open_channel(dd, channel_name, &cd);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(ii)", result, cd);

}

static PyObject *close_channel(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd;
    HPMUD_CHANNEL cd;

    if (!PyArg_ParseTuple(args, "ii", &dd, &cd))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_close_channel(dd, cd);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("i", result);
}

static PyObject *write_channel(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd;
    HPMUD_CHANNEL cd;
    int timeout = 30;
    char * buf;
    int buf_size = 0;
    int bytes_written = 0;

    if (!PyArg_ParseTuple(args, "iis#|i", &dd, &cd, &buf, &buf_size, &timeout))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_write_channel(dd, cd, buf, buf_size,  timeout, &bytes_written);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(ii)", result, bytes_written);
}

static PyObject *read_channel(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd;
    HPMUD_CHANNEL cd;
    int timeout = 30;
    char buf[HPMUD_BUFFER_SIZE];
    int bytes_read = 0;
    int bytes_to_read;

    if (!PyArg_ParseTuple(args, "iii|i", &dd, &cd, &bytes_to_read, &timeout))
        return NULL;

    if (bytes_to_read > HPMUD_BUFFER_SIZE)
        return Py_BuildValue("(is#)", HPMUD_R_INVALID_LENGTH, "", 0);

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_read_channel(dd, cd, (void *)buf, bytes_to_read,  timeout, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue(FORMAT_STRING1, result, buf, bytes_read);
}

static PyObject *set_pml(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd;
    HPMUD_CHANNEL cd;
    char * oid;
    int type;
    char * data;
    int data_size;
    int pml_result;

    if (!PyArg_ParseTuple(args, "iisis#", &dd, &cd, &oid, &type, &data, &data_size))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_set_pml(dd, cd, oid, type, (void *)data, data_size, &pml_result);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(ii)", result, pml_result);
}

static PyObject *get_pml(PyObject *self, PyObject *args)
{
    enum HPMUD_RESULT result = HPMUD_R_OK;
    HPMUD_DEVICE dd;
    HPMUD_CHANNEL cd;
    char * oid;
    int type;
    char buf[HPMUD_BUFFER_SIZE * 4];
    int pml_result;
    int bytes_read = 0;

    if (!PyArg_ParseTuple(args, "iisi", &dd, &cd, &oid, &type))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_get_pml(dd, cd, oid, (void *)buf, HPMUD_BUFFER_SIZE * 4, &bytes_read, &type, &pml_result);
    Py_END_ALLOW_THREADS
    return Py_BuildValue(FORMAT_STRING, result, buf, bytes_read, type, pml_result);
}

static PyObject *make_usb_uri(PyObject *self, PyObject *args)
{
    char * busnum;
    char * devnum;
    char uri[HPMUD_BUFFER_SIZE];
    enum HPMUD_RESULT result = HPMUD_R_OK;
    int bytes_read = 0;

    if (!PyArg_ParseTuple(args, "ss", &busnum, &devnum))
            return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_make_usb_uri(busnum, devnum, uri, HPMUD_BUFFER_SIZE, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue(FORMAT_STRING1, result, uri, bytes_read);
}

#ifdef HAVE_LIBNETSNMP
static PyObject *make_net_uri(PyObject *self, PyObject *args)
{
    char * ip;
    int port;
    char uri[HPMUD_BUFFER_SIZE];
    enum HPMUD_RESULT result = HPMUD_R_OK;
    int bytes_read = 0;

    if (!PyArg_ParseTuple(args, "si", &ip, &port))
            return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_make_net_uri(ip, port, uri, HPMUD_BUFFER_SIZE, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue(FORMAT_STRING1, result, uri, bytes_read);
}
#else
static PyObject *make_net_uri(PyObject *self, PyObject *args)
{
    return Py_BuildValue(FORMAT_STRING1, HPMUD_R_INVALID_URI, "", 0);
}
#endif /* HAVE_LIBSNMP */

#ifdef HAVE_LIBNETSNMP
static PyObject *make_zc_uri(PyObject *self, PyObject *args)
{
    char *hn;
    int port;
    char uri[HPMUD_BUFFER_SIZE];
    int bytes_read = 0;
    enum HPMUD_RESULT result = HPMUD_R_OK;

    if (!PyArg_ParseTuple(args, "si", &hn, &port))
            return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_make_mdns_uri(hn, port, uri, HPMUD_BUFFER_SIZE, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue(FORMAT_STRING1, result, uri, bytes_read);
}
#else
static PyObject *make_zc_uri(PyObject *self, PyObject *args)
{
    return Py_BuildValue(FORMAT_STRING1, HPMUD_R_INVALID_URI, "", 0);
}
#endif /* HAVE_LIBSNMP */

#ifdef HAVE_LIBNETSNMP
static PyObject *get_zc_ip_address(PyObject *self, PyObject *args)
{
    char *hn;
    char ip[HPMUD_BUFFER_SIZE] = {0,};
    enum HPMUD_RESULT result = HPMUD_R_OK;

    if (!PyArg_ParseTuple(args, "s", &hn))
            return NULL;

    Py_BEGIN_ALLOW_THREADS

    if(mdns_lookup(hn, ip) != MDNS_STATUS_OK)
        result =  HPMUD_R_INVALID_MDNS;

    Py_END_ALLOW_THREADS

    return Py_BuildValue("(is)", result, ip);
}
#else
static PyObject *get_zc_ip_address(PyObject *self, PyObject *args)
{
    return Py_BuildValue("(is)", HPMUD_R_INVALID_URI, "");
}
#endif /* HAVE_LIBSNMP */

#ifdef HAVE_PPORT
static PyObject *make_par_uri(PyObject *self, PyObject *args)
{
    char * devnode;
    char uri[HPMUD_BUFFER_SIZE];
    enum HPMUD_RESULT result = HPMUD_R_OK;
    int bytes_read = 0;

    if (!PyArg_ParseTuple(args, "s", &devnode))
            return NULL;

    Py_BEGIN_ALLOW_THREADS
    result = hpmud_make_par_uri(devnode, uri, HPMUD_BUFFER_SIZE, &bytes_read);
    Py_END_ALLOW_THREADS

    return Py_BuildValue(FORMAT_STRING1, result, uri, bytes_read);
}
#else
static PyObject *make_par_uri(PyObject *self, PyObject *args)
{
    return Py_BuildValue(FORMAT_STRING1, HPMUD_R_INVALID_URI, "", 0);
}
#endif /* HAVE_PPORT */

// Unwrapped MUD APIs:
// int hpmud_get_model(const char *id, char *buf, int buf_size);
// int hpmud_get_uri_model(const char *uri, char *buf, int buf_size);
// int hpmud_get_uri_datalink(const char *uri, char *buf, int buf_size);
// enum HPMUD_RESULT hpmud_get_model_attributes(char *uri, char *attr, int attrSize);
// enum HPMUD_RESULT hpmud_model_query(char *uri, struct hpmud_model_attributes *ma);
// enum HPMUD_RESULT hpmud_get_device_status(HPMUD_DEVICE dd, unsigned int *status);
// enum HPMUD_RESULT hpmud_get_dstat(HPMUD_DEVICE dd, struct hpmud_dstat *ds);


static PyMethodDef mudext_functions[] =
{
    {"open_device",       (PyCFunction)open_device,  METH_VARARGS },
    {"close_device",       (PyCFunction)close_device,  METH_VARARGS },
    {"get_device_id",       (PyCFunction)get_device_id,  METH_VARARGS },
    {"probe_devices",     (PyCFunction)probe_devices,  METH_VARARGS },
    {"open_channel",     (PyCFunction)open_channel,  METH_VARARGS },
    {"write_channel",     (PyCFunction)write_channel,  METH_VARARGS },
    {"read_channel",     (PyCFunction)read_channel,  METH_VARARGS },
    {"close_channel",     (PyCFunction)close_channel,  METH_VARARGS },
    {"set_pml",               (PyCFunction)set_pml,  METH_VARARGS },
    {"get_pml",              (PyCFunction)get_pml,  METH_VARARGS },
    {"make_usb_uri",        (PyCFunction)make_usb_uri,  METH_VARARGS },
    {"make_net_uri",        (PyCFunction)make_net_uri,  METH_VARARGS },
    {"make_zc_uri",         (PyCFunction)make_zc_uri,  METH_VARARGS },
    {"get_zc_ip_address",   (PyCFunction)get_zc_ip_address, METH_VARARGS },
    {"make_par_uri",        (PyCFunction)make_par_uri,  METH_VARARGS },
    { NULL, NULL }
};


static char mudext_documentation[] = "Python extension for HP multi-point transport driver";

static void insint(PyObject *d, char *name, int value)
{
    PyObject *v = PyLong_FromLong((long) value);

    if (!v || PyDict_SetItemString(d, name, v))
        Py_FatalError("Initialization failed.");

    Py_DECREF(v);
}

static void insstr(PyObject *d, char *name, char *value)
{
    PyObject *v = PyUnicode_FromString(value);

    if (!v || PyDict_SetItemString(d, name, v))
        Py_FatalError("Initialization failed.");

    Py_DECREF(v);
}

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "hpmudext",     /* m_name */
        mudext_documentation,  /* m_doc */
        -1,                  /* m_size */
        mudext_functions,    /* m_methods */
        NULL,                /* m_reload */
        NULL,                /* m_traverse */
        NULL,                /* m_clear */
        NULL,                /* m_free */
    };
#endif

/*void inithpmudext(void)*/
/*PyObject* PyInit_hpmudext(void)
{
  /*PyObject *mod = Py_InitModule3("hpmudext", mudext_functions, mudext_documentation);*/
  /*  PyObject* mod = PyModule_Create(&moduledef);

    if (mod == NULL)
      return NULL ;

    PyObject* d = PyModule_GetDict(mod);

    // enum HPMUD_RESULT
    insint(d, "HPMUD_R_OK", HPMUD_R_OK);
    insint(d, "HPMUD_R_INVALID_DEVICE", HPMUD_R_INVALID_DEVICE);
    insint(d, "HPMUD_R_INVALID_DESCRIPTOR", HPMUD_R_INVALID_DESCRIPTOR);
    insint(d, "HPMUD_R_INVALID_URI", HPMUD_R_INVALID_URI);
    insint(d, "HPMUD_R_INVALID_LENGTH", HPMUD_R_INVALID_LENGTH);
    insint(d, "HPMUD_R_IO_ERROR", HPMUD_R_IO_ERROR);
    insint(d, "HPMUD_R_DEVICE_BUSY", HPMUD_R_DEVICE_BUSY);
    insint(d, "HPMUD_R_INVALID_SN", HPMUD_R_INVALID_SN);
    insint(d, "HPMUD_R_INVALID_CHANNEL_ID", HPMUD_R_INVALID_CHANNEL_ID);
    insint(d, "HPMUD_R_INVALID_STATE", HPMUD_R_INVALID_STATE);
    insint(d, "HPMUD_R_INVALID_DEVICE_OPEN", HPMUD_R_INVALID_DEVICE_OPEN);
    insint(d, "HPMUD_R_INVALID_DEVICE_NODE", HPMUD_R_INVALID_DEVICE_NODE);
    insint(d, "HPMUD_R_INVALID_IP", HPMUD_R_INVALID_IP);
    insint(d, "HPMUD_R_INVALID_IP_PORT", HPMUD_R_INVALID_IP_PORT);
    insint(d, "HPMUD_R_INVALID_TIMEOUT", HPMUD_R_INVALID_TIMEOUT);
    insint(d, "HPMUD_R_DATFILE_ERROR", HPMUD_R_DATFILE_ERROR);
    insint(d, "HPMUD_R_IO_TIMEOUT", HPMUD_R_IO_TIMEOUT);

    // enum HPMUD_IO_MODE
    insint(d, "HPMUD_UNI_MODE", HPMUD_UNI_MODE);
    insint(d, "HPMUD_RAW_MODE",HPMUD_RAW_MODE);
    insint(d, "HPMUD_DOT4_MODE", HPMUD_DOT4_MODE);
    insint(d, "HPMUD_DOT4_PHOENIX_MODE", HPMUD_DOT4_PHOENIX_MODE);
    insint(d, "HPMUD_DOT4_BRIDGE_MODE", HPMUD_DOT4_BRIDGE_MODE);
    insint(d, "HPMUD_MLC_GUSHER_MODE", HPMUD_MLC_GUSHER_MODE);
    insint(d, "HPMUD_MLC_MISER_MODE", HPMUD_MLC_MISER_MODE);

    // enum HPMUD_BUS_ID
    insint(d, "HPMUD_BUS_NA", HPMUD_BUS_NA);
    insint(d, "HPMUD_BUS_USB", HPMUD_BUS_USB);
    insint(d, "HPMUD_BUS_PARALLEL", HPMUD_BUS_PARALLEL);
    insint(d, "HPMUD_BUS_ALL", HPMUD_BUS_ALL);

    // Channel names
    insstr(d, "HPMUD_S_PRINT_CHANNEL", HPMUD_S_PRINT_CHANNEL);
    insstr(d, "HPMUD_S_PML_CHANNEL", HPMUD_S_PML_CHANNEL);
    insstr(d, "HPMUD_S_SCAN_CHANNEL", HPMUD_S_SCAN_CHANNEL);
    insstr(d, "HPMUD_S_FAX_SEND_CHANNEL", HPMUD_S_FAX_SEND_CHANNEL);
    insstr(d, "HPMUD_S_CONFIG_UPLOAD_CHANNEL", HPMUD_S_CONFIG_UPLOAD_CHANNEL);
    insstr(d, "HPMUD_S_CONFIG_DOWNLOAD_CHANNEL", HPMUD_S_CONFIG_DOWNLOAD_CHANNEL);
    insstr(d, "HPMUD_S_MEMORY_CARD_CHANNEL", HPMUD_S_MEMORY_CARD_CHANNEL);
    insstr(d, "HPMUD_S_EWS_CHANNEL", HPMUD_S_EWS_CHANNEL);
    insstr(d, "HPMUD_S_EWS_LEDM_CHANNEL", HPMUD_S_EWS_LEDM_CHANNEL);
    insstr(d, "HPMUD_S_SOAP_SCAN", HPMUD_S_SOAP_SCAN);
    insstr(d, "HPMUD_S_SOAP_FAX", HPMUD_S_SOAP_FAX);
    insstr(d, "HPMUD_S_DEVMGMT_CHANNEL", HPMUD_S_DEVMGMT_CHANNEL);
    insstr(d, "HPMUD_S_WIFI_CHANNEL", HPMUD_S_WIFI_CHANNEL);
    insstr(d, "HPMUD_S_MARVELL_FAX_CHANNEL", HPMUD_S_MARVELL_FAX_CHANNEL);
    insstr(d, "HPMUD_S_LEDM_SCAN", HPMUD_S_LEDM_SCAN);
    insstr(d, "HPMUD_S_MARVELL_EWS_CHANNEL", HPMUD_S_MARVELL_EWS_CHANNEL);

    // Max buffer size
    insint(d, "HPMUD_BUFFER_SIZE", HPMUD_BUFFER_SIZE);

    return mod;

}


  */

MOD_INIT(hpmudext)  {
    PyObject* mod ;
    MOD_DEF(mod, "hpmudext", mudext_documentation, mudext_functions);
  if (mod == NULL)
    return MOD_ERROR_VAL;

PyObject* d = PyModule_GetDict(mod);

    // enum HPMUD_RESULT
    insint(d, "HPMUD_R_OK", HPMUD_R_OK);
    insint(d, "HPMUD_R_INVALID_DEVICE", HPMUD_R_INVALID_DEVICE);
    insint(d, "HPMUD_R_INVALID_DESCRIPTOR", HPMUD_R_INVALID_DESCRIPTOR);
    insint(d, "HPMUD_R_INVALID_URI", HPMUD_R_INVALID_URI);
    insint(d, "HPMUD_R_INVALID_LENGTH", HPMUD_R_INVALID_LENGTH);
    insint(d, "HPMUD_R_IO_ERROR", HPMUD_R_IO_ERROR);
    insint(d, "HPMUD_R_DEVICE_BUSY", HPMUD_R_DEVICE_BUSY);
    insint(d, "HPMUD_R_INVALID_SN", HPMUD_R_INVALID_SN);
    insint(d, "HPMUD_R_INVALID_CHANNEL_ID", HPMUD_R_INVALID_CHANNEL_ID);
    insint(d, "HPMUD_R_INVALID_STATE", HPMUD_R_INVALID_STATE);
    insint(d, "HPMUD_R_INVALID_DEVICE_OPEN", HPMUD_R_INVALID_DEVICE_OPEN);
    insint(d, "HPMUD_R_INVALID_DEVICE_NODE", HPMUD_R_INVALID_DEVICE_NODE);
    insint(d, "HPMUD_R_INVALID_IP", HPMUD_R_INVALID_IP);
    insint(d, "HPMUD_R_INVALID_IP_PORT", HPMUD_R_INVALID_IP_PORT);
    insint(d, "HPMUD_R_INVALID_TIMEOUT", HPMUD_R_INVALID_TIMEOUT);
    insint(d, "HPMUD_R_DATFILE_ERROR", HPMUD_R_DATFILE_ERROR);
    insint(d, "HPMUD_R_IO_TIMEOUT", HPMUD_R_IO_TIMEOUT);

    // enum HPMUD_IO_MODE
    insint(d, "HPMUD_UNI_MODE", HPMUD_UNI_MODE);
    insint(d, "HPMUD_RAW_MODE",HPMUD_RAW_MODE);
    insint(d, "HPMUD_DOT4_MODE", HPMUD_DOT4_MODE);
    insint(d, "HPMUD_DOT4_PHOENIX_MODE", HPMUD_DOT4_PHOENIX_MODE);
    insint(d, "HPMUD_DOT4_BRIDGE_MODE", HPMUD_DOT4_BRIDGE_MODE);
    insint(d, "HPMUD_MLC_GUSHER_MODE", HPMUD_MLC_GUSHER_MODE);
    insint(d, "HPMUD_MLC_MISER_MODE", HPMUD_MLC_MISER_MODE);

    // enum HPMUD_BUS_ID
    insint(d, "HPMUD_BUS_NA", HPMUD_BUS_NA);
     insint(d, "HPMUD_BUS_USB", HPMUD_BUS_USB);
    insint(d, "HPMUD_BUS_PARALLEL", HPMUD_BUS_PARALLEL);
    insint(d, "HPMUD_BUS_ALL", HPMUD_BUS_ALL);

    // Channel names
    insstr(d, "HPMUD_S_PRINT_CHANNEL", HPMUD_S_PRINT_CHANNEL);
    insstr(d, "HPMUD_S_PML_CHANNEL", HPMUD_S_PML_CHANNEL);
    insstr(d, "HPMUD_S_SCAN_CHANNEL", HPMUD_S_SCAN_CHANNEL);
    insstr(d, "HPMUD_S_FAX_SEND_CHANNEL", HPMUD_S_FAX_SEND_CHANNEL);
    insstr(d, "HPMUD_S_CONFIG_UPLOAD_CHANNEL", HPMUD_S_CONFIG_UPLOAD_CHANNEL);
    insstr(d, "HPMUD_S_CONFIG_DOWNLOAD_CHANNEL", HPMUD_S_CONFIG_DOWNLOAD_CHANNEL);
    insstr(d, "HPMUD_S_MEMORY_CARD_CHANNEL", HPMUD_S_MEMORY_CARD_CHANNEL);
    insstr(d, "HPMUD_S_EWS_CHANNEL", HPMUD_S_EWS_CHANNEL);
    insstr(d, "HPMUD_S_EWS_LEDM_CHANNEL", HPMUD_S_EWS_LEDM_CHANNEL);
    insstr(d, "HPMUD_S_SOAP_SCAN", HPMUD_S_SOAP_SCAN);
    insstr(d, "HPMUD_S_SOAP_FAX", HPMUD_S_SOAP_FAX);
    insstr(d, "HPMUD_S_DEVMGMT_CHANNEL", HPMUD_S_DEVMGMT_CHANNEL);
    insstr(d, "HPMUD_S_WIFI_CHANNEL", HPMUD_S_WIFI_CHANNEL);
    insstr(d, "HPMUD_S_MARVELL_FAX_CHANNEL", HPMUD_S_MARVELL_FAX_CHANNEL);
    insstr(d, "HPMUD_S_LEDM_SCAN", HPMUD_S_LEDM_SCAN);
    insstr(d, "HPMUD_S_MARVELL_EWS_CHANNEL", HPMUD_S_MARVELL_EWS_CHANNEL);

    // Max buffer size
    insint(d, "HPMUD_BUFFER_SIZE", HPMUD_BUFFER_SIZE);


  return MOD_SUCCESS_VAL(mod);

}

