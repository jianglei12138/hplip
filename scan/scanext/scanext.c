/*******************************************************************
scanext - Python extension class for SANE

Portions (c) Copyright 2015 HP Development Company, L.P.

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

Based on:
"_sane.c", part of the Python Imaging Library (PIL)
http://www.pythonware.com/products/pil/

Modified to work without PIL by Don Welch

(C) Copyright 2003 A.M. Kuchling.  All Rights Reserved
(C) Copyright 2004 A.M. Kuchling, Ralph Heinkel  All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of A.M. Kuchling and
Ralph Heinkel not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

A.M. KUCHLING, R.H. HEINKEL DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

*******************************************************************/


/* _ScanDevice objects */

#include "Python.h"
#include "sane.h"
#include <sys/time.h>

#if PY_MAJOR_VERSION >= 3
    #define  PyInt_AsLong  PyLong_AsLong
    #define  PyInt_FromLong PyLong_FromLong
    #define  PyInt_Check PyLong_Check
    #define  PyUNICODE_FromUNICODE PyUnicode_FromString
    #define  PyUNICODE_CHECK PyUnicode_Check
    #define FORMAT_STRING "(iy#)"

    #if (PY_VERSION_HEX < 0x03030000)
        #define PyUNICODE_AsBYTES(x) PyBytes_AsString(PyUnicode_AsUTF8String(x))
    #else
        #define PyUNICODE_AsBYTES PyUnicode_AsUTF8
    #endif 
    
    #define MOD_ERROR_VAL NULL
    #define MOD_SUCCESS_VAL(val) val
    #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
    #define MOD_DEF(ob, name, doc, methods) \
          static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);	\

#else
    #define  PyUNICODE_FromUNICODE PyString_FromString
    #define  PyUNICODE_CHECK PyString_Check
    #define  PyUNICODE_AsBYTES PyString_AsString
    #define FORMAT_STRING "(is#)"

    #define MOD_ERROR_VAL
    #define MOD_SUCCESS_VAL(val)
    #define MOD_INIT(name) void init##name(void)
    #define MOD_DEF(ob, name, doc, methods) \
          ob = Py_InitModule3(name, methods, doc);	\

#endif

static char scanext_documentation[] = "Python extension for HP scan sane driver";
static PyObject *ErrorObject;

typedef struct
{
    PyObject_HEAD SANE_Handle h;
} _ScanDevice;

#ifdef WITH_THREAD
PyThreadState *_save;
#endif

/* Raise a SANE exception using a SANE_Status code */
PyObject *raiseSaneError (SANE_Status st)
{
    const char *string;

    if (st == SANE_STATUS_GOOD)
    {
        Py_INCREF (Py_None);
        return (Py_None);
    }

    string = sane_strstatus (st);
    //PyErr_SetString (ErrorObject, string);
    PyErr_SetObject(ErrorObject,  PyInt_FromLong(st));
    return NULL;
}

/* Raise an exception using a character string */
PyObject * raiseError(const char * str)
{
    PyErr_SetString (ErrorObject, str);
    return NULL;
}

/* Raise an exception using a character string */
PyObject * raiseDeviceClosedError(void)
{
    return raiseError ("_ScanDevice object is closed");
}

static PyObject *getErrorMessage(PyObject * self, PyObject * args)
{
    int st;

    if (!PyArg_ParseTuple (args, "i", &st))
        raiseError("Invalid arguments.");

    return Py_BuildValue("s", sane_strstatus (st));
}

static PyTypeObject ScanDevice_type;

#define SaneDevObject_Check(v)  ((v)->ob_type == &ScanDevice_type)

static _ScanDevice *newScanDeviceObject (void)
{
    _ScanDevice *self;

    self = PyObject_NEW (_ScanDevice, &ScanDevice_type);

    if (self == NULL)
        return NULL;

    self->h = NULL;
    return self;
}


/* _ScanDevice methods */

static void deAlloc (_ScanDevice * self)
{
    if (self->h)
        sane_close (self->h);

    self->h = NULL;
    PyObject_DEL (self);
}

static PyObject *closeScan (_ScanDevice * self, PyObject * args)
{
    if (!PyArg_ParseTuple (args, ""))
        return NULL;

    if (self->h)
        sane_close (self->h);

    self->h = NULL;
    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *getParameters (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;
    SANE_Parameters p;
    char *format_name = "unknown";

    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    Py_BEGIN_ALLOW_THREADS
    st = sane_get_parameters (self->h, &p);
    Py_END_ALLOW_THREADS

    if (st != SANE_STATUS_GOOD)
        return raiseSaneError (st);

    switch (p.format)
    {
        case (SANE_FRAME_GRAY):
            format_name = "gray";
            break;
        case (SANE_FRAME_RGB):
            format_name = "color";
            break;
        case (SANE_FRAME_RED):
            format_name = "red";
            break;
        case (SANE_FRAME_GREEN):
            format_name = "green";
            break;
        case (SANE_FRAME_BLUE):
            format_name = "blue";
            break;
    }

    return Py_BuildValue ("isiiiii", p.format, format_name,
                          p.last_frame, p.pixels_per_line,
                          p.lines, p.depth, p.bytes_per_line);
}

static PyObject *startScan (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;

    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    /* sane_start can take several seconds, if the user initiates
       a new scan, while the scan head of a flatbed scanner moves
       back to the start position after finishing a previous scan.
       Hence it is worth to allow threads here.
     */
    Py_BEGIN_ALLOW_THREADS
    st = sane_start (self->h);
    Py_END_ALLOW_THREADS

    if (st != SANE_STATUS_GOOD &&
        st != SANE_STATUS_EOF &&
        st != SANE_STATUS_NO_DOCS)
          return raiseSaneError(st);

    return Py_BuildValue("i", st);
}

static PyObject *cancelScan (_ScanDevice * self, PyObject * args)
{
    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    sane_cancel (self->h);
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *getOptions (_ScanDevice * self, PyObject * args)
{
    const SANE_Option_Descriptor *d;
    PyObject *list, *value;
    int i = 1;

    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    if (!(list = PyList_New (0)))
        raiseError("Unable to allocate list.");

    do
    {
        d = sane_get_option_descriptor (self->h, i);
        if (d != NULL)
        {
            PyObject *constraint = NULL;
            int j;

            switch (d->constraint_type)
            {
            case (SANE_CONSTRAINT_NONE):
                Py_INCREF (Py_None);
                constraint = Py_None;
                break;
            case (SANE_CONSTRAINT_RANGE):
                if (d->type == SANE_TYPE_INT)
                    constraint =
                        Py_BuildValue ("iii", d->constraint.range->min,
                                       d->constraint.range->max,
                                       d->constraint.range->quant);
                else
                    constraint = Py_BuildValue ("ddd",
                                                SANE_UNFIX (d->
                                                            constraint.
                                                            range->min),
                                                SANE_UNFIX (d->
                                                            constraint.
                                                            range->max),
                                                SANE_UNFIX (d->
                                                            constraint.
                                                            range->quant));
                break;
            case (SANE_CONSTRAINT_WORD_LIST):
                constraint = PyList_New (d->constraint.word_list[0]);

                if (d->type == SANE_TYPE_INT)
                    for (j = 1; j <= d->constraint.word_list[0]; j++)
                        PyList_SetItem (constraint, j - 1,
                                        PyInt_FromLong (d->constraint.
                                                        word_list[j]));
                else
                    for (j = 1; j <= d->constraint.word_list[0]; j++)
                        PyList_SetItem (constraint, j - 1,
                                        PyFloat_FromDouble (SANE_UNFIX
                                                            (d->
                                                             constraint.
                                                             word_list[j])));
                break;
            case (SANE_CONSTRAINT_STRING_LIST):
                constraint = PyList_New (0);

                for (j = 0; d->constraint.string_list[j] != NULL; j++)
                    PyList_Append (constraint,
                                   PyUNICODE_FromUNICODE (d->constraint.
                                                        string_list[j]));
                break;
            }
            value = Py_BuildValue ("isssiiiiO", i, d->name, d->title, d->desc,
                                   d->type, d->unit, d->size, d->cap, constraint);

            PyList_Append (list, value);
        }
        i++;
    }

    while (d != NULL);
    return list;
}

static PyObject *getOption (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;
    const SANE_Option_Descriptor *d;
    PyObject *value = NULL;
    int n;
    void *v;

    if (!PyArg_ParseTuple (args, "i", &n))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    d = sane_get_option_descriptor (self->h, n);
    v = malloc (d->size + 1);
    st = sane_control_option (self->h, n, SANE_ACTION_GET_VALUE, v, NULL);

    if (st != SANE_STATUS_GOOD)
    {
        free (v);
        return raiseSaneError(st);
    }

    switch (d->type)
    {
        case (SANE_TYPE_BOOL):
        case (SANE_TYPE_INT):
            value = Py_BuildValue ("i", *((SANE_Int *) v));
            break;

        case (SANE_TYPE_FIXED):
            value = Py_BuildValue ("d", SANE_UNFIX ((*((SANE_Fixed *) v))));
            break;

        case (SANE_TYPE_STRING):
            value = Py_BuildValue ("s", v);
            break;

        case (SANE_TYPE_BUTTON):
        case (SANE_TYPE_GROUP):
            value = Py_BuildValue ("O", Py_None);
            break;
    }

    free (v);
    return value;
}

static PyObject *setOption (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;
    const SANE_Option_Descriptor *d;
    SANE_Int i;
    PyObject *value;
    int n;

    if (!PyArg_ParseTuple (args, "iO", &n, &value))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    d = sane_get_option_descriptor (self->h, n);
    switch (d->type)
    {
        case (SANE_TYPE_BOOL):
            if (!PyInt_Check (value))
                return raiseError("SANE_Bool requires an integer.");

            SANE_Bool b = PyInt_AsLong(value);

            if (b != SANE_FALSE && b > SANE_TRUE)
                b = SANE_TRUE;

            st = sane_control_option (self->h, n, SANE_ACTION_SET_VALUE, (void *)&b, &i);
            break;

        case (SANE_TYPE_INT):
            if (!PyInt_Check (value))
                return raiseError("SANE_Int requires an integer.");

            SANE_Int j = PyInt_AsLong (value);
            st = sane_control_option (self->h, n, SANE_ACTION_SET_VALUE, (void *)&j, &i);
            break;

        case (SANE_TYPE_FIXED):
            if (!PyFloat_Check (value))
                return raiseError("SANE_Fixed requires an float.");

            SANE_Fixed f = SANE_FIX (PyFloat_AsDouble (value));
            st = sane_control_option (self->h, n, SANE_ACTION_SET_VALUE, (void *)&f, &i);
            break;

        case (SANE_TYPE_STRING):
            if (!PyUNICODE_CHECK (value))
                return raiseError("SANE_String requires a a string.");

            SANE_String s = malloc (d->size + 1);
            strncpy (s, PyUNICODE_AsBYTES (value), d->size - 1);
            ((SANE_String) s)[d->size - 1] = 0;
            st = sane_control_option (self->h, n, SANE_ACTION_SET_VALUE, (void *)s, &i);
            free(s);
            break;

        case (SANE_TYPE_BUTTON):
        case (SANE_TYPE_GROUP):
            break;
    }

    if (st != SANE_STATUS_GOOD)
        return raiseSaneError(st);

    return Py_BuildValue ("i", i);
}

static PyObject *setAutoOption (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;
    const SANE_Option_Descriptor *d;
    SANE_Int i;
    int n;

    if (!PyArg_ParseTuple (args, "i", &n))
        raiseError("Invalid arguments.");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    d = sane_get_option_descriptor (self->h, n);
    st = sane_control_option (self->h, n, SANE_ACTION_SET_AUTO, NULL, &i);

    if (st != SANE_STATUS_GOOD)
        return raiseSaneError (st);

    return Py_BuildValue ("i", i);
}

#define MAX_READSIZE 65536

static PyObject *readScan (_ScanDevice * self, PyObject * args)
{
    SANE_Status st;
    SANE_Int len;
    SANE_Byte buffer[MAX_READSIZE];
    int bytes_to_read;

    if (!PyArg_ParseTuple (args, "i", &bytes_to_read))
        raiseError("Invalid arguments.");

    if (bytes_to_read > MAX_READSIZE)
        return raiseError("bytes_to_read > MAX_READSIZE");

    if (self->h == NULL)
        return raiseDeviceClosedError();

    //Py_BEGIN_ALLOW_THREADS
    Py_UNBLOCK_THREADS
    st = sane_read (self->h, buffer, bytes_to_read, &len);
    //Py_END_ALLOW_THREADS
    Py_BLOCK_THREADS

    if (st != SANE_STATUS_GOOD &&
        st != SANE_STATUS_EOF &&
        st != SANE_STATUS_NO_DOCS)
    {
        sane_cancel(self->h);
        //Py_BLOCK_THREADS
        return raiseSaneError(st);
    }

    return Py_BuildValue (FORMAT_STRING, st, buffer, len);
}


static PyMethodDef ScanDevice_methods[] = {
    {"getParameters", (PyCFunction) getParameters, METH_VARARGS},

    {"getOptions", (PyCFunction) getOptions, METH_VARARGS},
    {"getOption", (PyCFunction) getOption, METH_VARARGS},
    {"setOption", (PyCFunction) setOption, METH_VARARGS},
    {"setAutoOption", (PyCFunction) setAutoOption, METH_VARARGS},

    {"startScan", (PyCFunction) startScan, METH_VARARGS},
    {"cancelScan", (PyCFunction) cancelScan, METH_VARARGS},
    {"readScan", (PyCFunction) readScan, METH_VARARGS},
    {"closeScan", (PyCFunction) closeScan, METH_VARARGS},
    {NULL, NULL}
};


static PyTypeObject ScanDevice_type =
{
    PyVarObject_HEAD_INIT( &PyType_Type, 0 ) /* ob_size */
    "_ScanDevice",                   /* tp_name */
    sizeof(_ScanDevice ),              /* tp_basicsize */
    0,                                     /* tp_itemsize */
    ( destructor ) deAlloc,           /* tp_dealloc */
    0,                                     /* tp_print */
    0,                      /* tp_getattr */
    0,                                     /* tp_setattr */
    0,                                     /* tp_compare */
    0,                                     /* tp_repr */
    0,                                     /* tp_as_number */
    0,                                     /* tp_as_sequence */
    0,                                     /* tp_as_mapping */
    0,                                     /* tp_hash */
    0,                                     /* tp_call */
    0,                                     /* tp_str */
    PyObject_GenericGetAttr,               /* tp_getattro */
    PyObject_GenericSetAttr,               /* tp_setattro */
    0,                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* tp_flags */
    "Scan Device object",                 /* tp_doc */
    0,                                     /* tp_traverse */
    0,                                     /* tp_clear */
    0,                                     /* tp_richcompare */
    0,                                     /* tp_weaklistoffset */
    0,                                     /* tp_iter */
    0,                                     /* tp_iternext */
    ScanDevice_methods,         /*job_methods, */           /* tp_methods */
    0,                       /* tp_members */
    0,                                     /* tp_getset */
    0,                                     /* tp_base */
    0,                                     /* tp_dict */
    0,                                     /* tp_descr_get */
    0,                                     /* tp_descr_set */
    0,                                     /* tp_dictoffset */
    0,                                     /* tp_init */
    0,                                     /* tp_alloc */
    0,                                     /* tp_new */
};


/* --------------------------------------------------------------------- */

static void auth_callback (SANE_String_Const resource,
                       SANE_Char * username, SANE_Char * password)
{
    printf("auth_callback\n");
}

static PyObject *init (PyObject * self, PyObject * args)
{
    SANE_Status st;
    SANE_Int version;

    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments.");

    /* XXX Authorization is not yet supported */
    st = sane_init (&version, auth_callback);

    if (st != SANE_STATUS_GOOD)
        return raiseSaneError (st);

    return Py_BuildValue ("iiii", version, SANE_VERSION_MAJOR (version),
                          SANE_VERSION_MINOR (version),
                          SANE_VERSION_BUILD (version));

}



static PyObject *deInit (PyObject * self, PyObject * args)
{
    if (!PyArg_ParseTuple (args, ""))
        raiseError("Invalid arguments");

    sane_exit ();
    Py_INCREF (Py_None);
    return Py_None;
}

static PyObject *getDevices (PyObject * self, PyObject * args)
{
    const SANE_Device **device_list;
    SANE_Status st;
    PyObject *list;
    int local_only=SANE_FALSE, i;

    if (!PyArg_ParseTuple (args, "|i", &local_only))
        raiseError("Invalid arguments");

    st = sane_get_devices (&device_list, local_only);

    if (st != SANE_STATUS_GOOD)
        return raiseSaneError (st);

    if (!(list = PyList_New (0)))
        return raiseError("Unable to allocate device list.");

    for (i=0; device_list[i]; i++)
    {
        PyList_Append (list, Py_BuildValue ("ssss", device_list[i]->name, device_list[i]->vendor,
                                            device_list[i]->model, device_list[i]->type));
    }

    return list;
}

/* Function returning new _ScanDevice object */

static PyObject *openDevice (PyObject * self, PyObject * args)
{
    _ScanDevice *rv;
    SANE_Status st;
    char *name;

    if (!PyArg_ParseTuple (args, "s", &name))
        raiseError("Invalid arguments");

    rv = newScanDeviceObject ();

    if (rv == NULL)
        return raiseError("Unable to create _ScanDevice object.");

    st = sane_open (name, &(rv->h));

    if (st != SANE_STATUS_GOOD)
    {
        Py_DECREF (rv);
        return raiseSaneError (st);
    }
    return (PyObject *) rv;
}

static PyObject *isOptionActive (PyObject * self, PyObject * args)
{
    SANE_Int cap;
    long lg;

    if (!PyArg_ParseTuple (args, "l", &lg))
        raiseError("Invalid arguments");

    cap = lg;
    return PyInt_FromLong (SANE_OPTION_IS_ACTIVE (cap));
}

static PyObject *isOptionSettable (PyObject * self, PyObject * args)
{
    SANE_Int cap;
    long lg;

    if (!PyArg_ParseTuple (args, "l", &lg))
        raiseError("Invalid arguments");

    cap = lg;
    return PyInt_FromLong (SANE_OPTION_IS_SETTABLE (cap));
}


/* List of functions defined in the module */

static PyMethodDef ScanExt_methods[] = {
    {"init", init, METH_VARARGS},
    {"deInit", deInit, METH_VARARGS},
    {"getDevices", getDevices, METH_VARARGS},
    {"openDevice", openDevice, METH_VARARGS},
    {"isOptionActive", isOptionActive, METH_VARARGS},
    {"isOptionSettable", isOptionSettable, METH_VARARGS},
    {"getErrorMessage", getErrorMessage, METH_VARARGS},
    {NULL, NULL}                /* sentinel */
};

static void insint (PyObject * d, char *name, int value)
{
    PyObject *v = PyInt_FromLong ((long) value);

    if (!v || PyDict_SetItemString (d, name, v))
        Py_FatalError ("can't initialize sane module");

    Py_DECREF (v);
}

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "scanext",     /* m_name */
        scanext_documentation,  /* m_doc */
        -1,                  /* m_size */
        ScanExt_methods,    /* m_methods */
        NULL,                /* m_reload */
        NULL,                /* m_traverse */
        NULL,                /* m_clear */
        NULL,                /* m_free */
    };

#endif


MOD_INIT(scanext)  {
    PyObject* mod ;
    MOD_DEF(mod, "scanext", scanext_documentation, ScanExt_methods);
    if (mod == NULL)
    	return MOD_ERROR_VAL;

    /* Add some symbolic constants to the module */
    PyObject* d = PyModule_GetDict(mod);
    ErrorObject = PyErr_NewException("scanext.error", NULL, NULL); 
    if (ErrorObject == NULL) 
    {	
         Py_DECREF(mod);	
         return MOD_ERROR_VAL;
    }
    PyDict_SetItemString (d, "error", ErrorObject);


    insint (d, "INFO_INEXACT", SANE_INFO_INEXACT);
    insint (d, "INFO_RELOAD_OPTIONS", SANE_INFO_RELOAD_OPTIONS);
    insint (d, "RELOAD_PARAMS", SANE_INFO_RELOAD_PARAMS);

    insint (d, "FRAME_GRAY", SANE_FRAME_GRAY);
    insint (d, "FRAME_RGB", SANE_FRAME_RGB);
    insint (d, "FRAME_RED", SANE_FRAME_RED);
    insint (d, "FRAME_GREEN", SANE_FRAME_GREEN);
    insint (d, "FRAME_BLUE", SANE_FRAME_BLUE);

    insint (d, "CONSTRAINT_NONE", SANE_CONSTRAINT_NONE);
    insint (d, "CONSTRAINT_RANGE", SANE_CONSTRAINT_RANGE);
    insint (d, "CONSTRAINT_WORD_LIST", SANE_CONSTRAINT_WORD_LIST);
    insint (d, "CONSTRAINT_STRING_LIST", SANE_CONSTRAINT_STRING_LIST);

    insint (d, "TYPE_BOOL", SANE_TYPE_BOOL);
    insint (d, "TYPE_INT", SANE_TYPE_INT);
    insint (d, "TYPE_FIXED", SANE_TYPE_FIXED);
    insint (d, "TYPE_STRING", SANE_TYPE_STRING);
    insint (d, "TYPE_BUTTON", SANE_TYPE_BUTTON);
    insint (d, "TYPE_GROUP", SANE_TYPE_GROUP);

    insint (d, "UNIT_NONE", SANE_UNIT_NONE);
    insint (d, "UNIT_PIXEL", SANE_UNIT_PIXEL);
    insint (d, "UNIT_BIT", SANE_UNIT_BIT);
    insint (d, "UNIT_MM", SANE_UNIT_MM);
    insint (d, "UNIT_DPI", SANE_UNIT_DPI);
    insint (d, "UNIT_PERCENT", SANE_UNIT_PERCENT);
    insint (d, "UNIT_MICROSECOND", SANE_UNIT_MICROSECOND);

    insint (d, "CAP_SOFT_SELECT", SANE_CAP_SOFT_SELECT);
    insint (d, "CAP_HARD_SELECT", SANE_CAP_HARD_SELECT);
    insint (d, "CAP_SOFT_DETECT", SANE_CAP_SOFT_DETECT);
    insint (d, "CAP_EMULATED", SANE_CAP_EMULATED);
    insint (d, "CAP_AUTOMATIC", SANE_CAP_AUTOMATIC);
    insint (d, "CAP_INACTIVE", SANE_CAP_INACTIVE);
    insint (d, "CAP_ADVANCED", SANE_CAP_ADVANCED);

    /* handy for checking array lengths: */
    insint (d, "SANE_WORD_SIZE", sizeof (SANE_Word));

    /* possible return values of set_option() */
    insint (d, "INFO_INEXACT", SANE_INFO_INEXACT);
    insint (d, "INFO_RELOAD_OPTIONS", SANE_INFO_RELOAD_OPTIONS);
    insint (d, "INFO_RELOAD_PARAMS", SANE_INFO_RELOAD_PARAMS);

    // SANE status codes
    insint (d, "SANE_STATUS_GOOD",  SANE_STATUS_GOOD); //Operation completed succesfully.
    insint (d, "SANE_STATUS_UNSUPPORTED", SANE_STATUS_UNSUPPORTED); // Operation is not supported.
    insint (d, "SANE_STATUS_CANCELLED", SANE_STATUS_CANCELLED); //Operation was cancelled.
    insint (d, "SANE_STATUS_DEVICE_BUSY", SANE_STATUS_DEVICE_BUSY); // Device is busy---retry later.
    insint (d, "SANE_STATUS_INVAL",  SANE_STATUS_INVAL); // Data or argument is invalid.
    insint (d, "SANE_STATUS_EOF", SANE_STATUS_EOF);  // No more data available (end-of-file).
    insint (d, "SANE_STATUS_JAMMED", SANE_STATUS_JAMMED); // Document feeder jammed.
    insint (d, "SANE_STATUS_NO_DOCS", SANE_STATUS_NO_DOCS); // Document feeder out of documents.
    insint (d, "SANE_STATUS_COVER_OPEN", SANE_STATUS_COVER_OPEN); // Scanner cover is open.
    insint (d, "SANE_STATUS_IO_ERROR", SANE_STATUS_IO_ERROR); // Error during device I/O.
    insint (d, "SANE_STATUS_NO_MEM", SANE_STATUS_NO_MEM); // Out of memory.
    insint (d, "SANE_STATUS_ACCESS_DENIED", SANE_STATUS_ACCESS_DENIED);  // Access to resource has been denied.

    // Maximum buffer size for read()
    insint(d, "MAX_READSIZE", MAX_READSIZE);

    /* Check for errors */
    if (PyErr_Occurred ())
        Py_FatalError ("can't initialize module scanext");

  return MOD_SUCCESS_VAL(mod);
}
