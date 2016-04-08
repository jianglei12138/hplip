/*****************************************************************************\
pcardext - Python extension for HP photocard services
 
 (c) Copyright 2003-2015 HP Development Company, L.P.

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

Author: Don Welch

\*****************************************************************************/

#include <Python.h>
#include <structmember.h>
#include "../fat.h"


/* Ref: PEP 353 (Python 2.5) */
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

int verbose=0;

PyObject * readsectorFunc = NULL;
PyObject * writesectorFunc = NULL;

int ReadSector(int sector, int nsector, void *buf, int size)
{
    PyObject * result;
    char * result_str;
    
    if( readsectorFunc )
    {
        if( nsector <= 0 || (nsector*FAT_HARDSECT) > size || nsector > FAT_BLKSIZE )
            goto abort;
        
        result = PyObject_CallFunction( readsectorFunc, "ii", sector, nsector );
        
        if( result )
        {
            Py_ssize_t len = 0;
            PyString_AsStringAndSize( result, &result_str, &len );
            
            if( len < nsector*FAT_HARDSECT )
            {
                goto abort;
            }
            
            memcpy( buf, result_str, nsector*FAT_HARDSECT );
            return 0;
        }
    }
    
abort:    
    return 1;
}

int WriteSector(int sector, int nsector, void *buf, int size )
{
    PyObject * result;
    
    if( writesectorFunc )
    {
        result = PyObject_CallFunction( writesectorFunc, "iis#", sector, nsector, buf, size );
        
        return PyInt_AS_LONG( result );
    }

    return 1;
}


PyObject * pcardext_mount( PyObject * self, PyObject * args ) 
{
    if( !PyArg_ParseTuple( args, "OO", &readsectorFunc, &writesectorFunc ) )
    {
        return Py_BuildValue( "i", 1 );
    }
    
    if( !PyCallable_Check( readsectorFunc ) || !PyCallable_Check( writesectorFunc ) )
    {
        return Py_BuildValue( "i", 2 );
    }
        
    Py_INCREF( readsectorFunc );
    Py_INCREF( writesectorFunc );

    int i = FatInit();
    /*char buf[1024];
    sprintf( buf, "print 'FatInit()=%d\n'", i );
    PyRun_SimpleString( buf );*/

    return Py_BuildValue( "i", i ); // ==0 ->OK, !=0 --> NG
}


PyObject * pcardext_df( PyObject * self, PyObject * args ) 
{
    return Py_BuildValue( "i", FatFreeSpace() );
}


PyObject * pcardext_ls( PyObject * self, PyObject * args ) 
{
    PyObject * file_list;
    file_list = PyList_New((Py_ssize_t)0);
    FILE_ATTRIBUTES fa;

    FatDirBegin( &fa );
    
    do 
    {
        if( fa.Attr != 'x' )
            PyList_Append( file_list, Py_BuildValue( "(sci)",  fa.Name, fa.Attr, fa.Size ) );
        
    } while( FatDirNext( &fa ) );

    return file_list;
}


PyObject * pcardext_cp( PyObject * self, PyObject * args ) 
{
    char * name; 
    int fileno = 0;
    
    if( !PyArg_ParseTuple( args, "si", &name, &fileno ) )
    {
        return Py_BuildValue( "i", 0 );
    }
    
    return Py_BuildValue( "i", FatReadFile( name, fileno ) );
}

PyObject * pcardext_cd( PyObject * self, PyObject * args ) 
{
    char * dir;

    if( !PyArg_ParseTuple( args, "s", &dir ) )
    {
        return Py_BuildValue( "i", 0 );
    }
    
    FatSetCWD( dir );
    
    return Py_BuildValue( "i", 1 );
}

PyObject * pcardext_rm( PyObject * self, PyObject * args ) 
{
    char * name;
    if( !PyArg_ParseTuple( args, "s", &name ) )
    {
        return Py_BuildValue( "i", 0 );
    }

    return Py_BuildValue( "i", FatDeleteFile( name ) );
}

PyObject * pcardext_umount( PyObject * self, PyObject * args ) 
{
    return Py_BuildValue( "" );
}

PyObject * pcardext_info( PyObject * self, PyObject * args ) 
{
    PHOTO_CARD_ATTRIBUTES pa;
    FatDiskAttributes( &pa );
    
    return Py_BuildValue( "(siiiiissi)", pa.OEMID, pa.BytesPerSector, pa.SectorsPerCluster, pa.ReservedSectors,
                                        pa.RootEntries, pa.SectorsPerFat, pa.VolumeLabel, pa.SystemID,
                                        pa.WriteProtect );
}

PyObject * pcardext_read( PyObject * self, PyObject * args ) 
{
    char * name;
    int offset = 0;
    Py_ssize_t len = 0;
    void * buffer;
    
    if( !PyArg_ParseTuple( args, "sii", &name, &offset, &len ) )
    {
        return Py_BuildValue( "s", "" );
    }
    
    buffer = alloca( len );
    
    if( FatReadFileExt( name, offset, len, buffer ) == len )
    {
        return PyString_FromStringAndSize( (char *)buffer, len );
    }
    else
    {
        return Py_BuildValue( "s", "" );
    }
    
    
}


static PyMethodDef pcardext_methods[] = 
{
    { "mount",       (PyCFunction)pcardext_mount,  METH_VARARGS },
    { "ls",          (PyCFunction)pcardext_ls,     METH_VARARGS },
    { "cp",          (PyCFunction)pcardext_cp,     METH_VARARGS },
    { "cd",          (PyCFunction)pcardext_cd,     METH_VARARGS },
    { "rm",          (PyCFunction)pcardext_rm,     METH_VARARGS },
    { "umount",      (PyCFunction)pcardext_umount, METH_VARARGS },
    { "df",          (PyCFunction)pcardext_df,     METH_VARARGS },
    { "info",        (PyCFunction)pcardext_info,   METH_VARARGS },
    { "read",        (PyCFunction)pcardext_read,   METH_VARARGS },
    { NULL, NULL }
};  


static char pcardext_documentation[] = "Python extension for HP photocard services";

void initpcardext( void )
{
    PyObject * mod = Py_InitModule4( "pcardext", pcardext_methods, 
                                     pcardext_documentation, (PyObject*)NULL, 
                                     PYTHON_API_VERSION );
                     
    if (mod == NULL)
      return;
}


