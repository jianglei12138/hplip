/************************************************************************************\

  io.c - HP SANE backend for multi-function peripherals (libsane-hpaio)

  (c) 2001-2008 Copyright HP Development Company, LP

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

  Contributing Authors: Don Welch, David Suffield 

\************************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include "hpmud.h"
#include "common.h"
#include "pml.h"
#include "io.h"
#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif
#define DEBUG_DECLARE_ONLY
#include "sanei_debug.h"

#ifdef HAVE_DBUS
DBusError dbus_err;
DBusConnection * dbus_conn;

int __attribute__ ((visibility ("hidden"))) InitDbus(void)
{
   dbus_error_init(&dbus_err);
   dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_err);
    
   if (dbus_error_is_set(&dbus_err))
   { 
      BUG("dBus Connection Error (%s)!\n", dbus_err.message); 
      dbus_error_free(&dbus_err); 
   }

   if (NULL == dbus_conn) 
   { 
      return 0; 
   }

   return 1;
}

int __attribute__ ((visibility ("hidden"))) SendScanEvent(char *device_uri, int event)
{
    DBusMessage * msg = dbus_message_new_signal(DBUS_PATH, DBUS_INTERFACE, "Event");
    char * printer = "";
    char * title = "";
    int jobid = 0; 
    char * username = "";

    uid_t uid = getuid();
    struct passwd *p = getpwuid (uid);
    username = p->pw_name;

    if (NULL == username)
        username = "";

    if (NULL == msg)
    {
        BUG("dbus message is NULL!\n");
        return 0;
    }

    dbus_message_append_args(msg, 
        DBUS_TYPE_STRING, &device_uri,
        DBUS_TYPE_STRING, &printer,
        DBUS_TYPE_UINT32, &event, 
        DBUS_TYPE_STRING, &username, 
        DBUS_TYPE_UINT32, &jobid,
        DBUS_TYPE_STRING, &title, 
        DBUS_TYPE_INVALID);

    if (!dbus_connection_send(dbus_conn, msg, NULL))
    {
        BUG("dbus message send failed!\n");
        return 0;
    }

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(msg);

    return 1;
}
#else
int __attribute__ ((visibility ("hidden"))) InitDbus(void)
{
   return 1;
}
int __attribute__ ((visibility ("hidden"))) SendScanEvent(char *device_uri, int event)
{
    return 1;
}
#endif  /* HAVE_DBUS */
 
/* Read full requested data length in BUFFER_SIZE chunks. Return number of bytes read. */
int __attribute__ ((visibility ("hidden"))) ReadChannelEx(int deviceid, int channelid, unsigned char * buffer, int length, int timeout)
{
   int n, len, size, total=0;
   enum HPMUD_RESULT stat;

   size = length;

   while(size > 0)
   {
      len = size > HPMUD_BUFFER_SIZE ? HPMUD_BUFFER_SIZE : size;
        
      stat = hpmud_read_channel(deviceid, channelid, buffer+total, len, timeout, &n);
      if (n <= 0)
      {
         break;    /* error or timeout */
      }
      size-=n;
      total+=n;
   }
        
   return total;
}

