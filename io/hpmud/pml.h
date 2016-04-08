/*****************************************************************************\

  pml.h - get/set pml api for hpmud
 
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
  (c) 2003-2004 Copyright HP Development Company, LP

\*****************************************************************************/

#ifndef _PML_H
#define _PML_H

/*
 * PML definitions
 */

enum PML_REQUESTS
{
   PML_GET_REQUEST = 0,
   PML_GET_NEXT_REQUEST = 0x1,
   PML_BLOCK_REQUEST = 0x3,
   PML_SET_REQUEST = 0x4,
   PML_ENABLE_TRAP_REQUEST = 0x5,
   PML_DISABLE_TRAP_REQUEST = 0x6,
   PML_TRAP_REQUEST = 0x7
};

enum PML_ERROR_VALUES
{
   PML_EV_OK = 0,
   PML_EV_OK_END_OF_SUPPORTED_OBJECTS = 0x1,
   PML_EV_OK_NEAREST_LEGAL_VALUE_SUBSTITUTED = 0x2,
   PML_EV_ERROR_UNKNOWN_REQUEST = 0x80,
   PML_EV_ERROR_BUFFER_OVERFLOW = 0x81,
   PML_EV_ERROR_COMMAND_EXECUTION_ERROR = 0x82,
   PML_EV_ERROR_UNKNOWN_OBJECT_IDENTIFIER = 0x83,
   PML_EV_ERROR_OBJECT_DOES_NOT_SUPPORT_REQUESTED_ACTION = 0x84,
   PML_EV_ERROR_INVALID_OR_UNSUPPORTED_VALUE = 0x85,
   PML_EV_ERROR_PAST_END_OF_SUPPORTED_OBJECTS = 0x86,
   PML_EV_ERROR_ACTION_CAN_NOT_BE_PERFORMED_NOW = 0x87
};

enum PML_DATA_TYPES
{
   PML_DT_OBJECT_IDENTIFIER = 0,
   PML_DT_ENUMERATION = 0x04,
   PML_DT_SIGNED_INTEGER = 0x08,
   PML_DT_REAL = 0x0C,
   PML_DT_STRING = 0x10,
   PML_DT_BINARY = 0x14,
   PML_DT_ERROR_CODE = 0x18,
   PML_DT_NULL_VALUE = 0x1C,
   PML_DT_COLLECTION = 0x20,
   PML_DT_UNKNOWN = 0xff
};

int __attribute__ ((visibility ("hidden"))) GetSnmp(const char *ip, int port, const char *szoid, void *buffer, unsigned int size, int *type, int *pml_result, int *result);

#endif // _PML_H

