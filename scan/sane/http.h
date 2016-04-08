/************************************************************************************\

  http.h - HTTP/1.1 feeder and consumer

  (c) 2008 Copyright HP Development Company, LP

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

  Primary Author: Naga Samrat Chowdary, Narla
  Contributing Author: Sarbeswar Meher
\************************************************************************************/

#ifndef _HTTP_H
#define _HTTP_H

enum HTTP_RESULT
{
   HTTP_R_OK = 0,
   HTTP_R_IO_ERROR,
   HTTP_R_EOF,
   HTTP_R_IO_TIMEOUT,
   HTTP_R_MALLOC_ERROR,
   HTTP_R_INVALID_BUF_SIZE,
};

# define ZERO_FOOTER "\r\n0\r\n\r\n"

typedef void * HTTP_HANDLE;

enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_open(HPMUD_DEVICE dd, const char *hpmud_channel, HTTP_HANDLE *handle);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_close(HTTP_HANDLE handle);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_read_header(HTTP_HANDLE handle, void *data, int max_size, int sec_timout, int *bytes_read);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_read_payload(HTTP_HANDLE handle, void *data, int max_size, int sec_timout, int *bytes_read);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_read(HTTP_HANDLE handle, void *data, int max_size, int sec_timout, int *bytes_read);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_read_size(HTTP_HANDLE handle, void *data, int max_size, int sec_timout, int *bytes_read);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_write(HTTP_HANDLE handle, void *data, int data_size, int sec_timout);
enum HTTP_RESULT __attribute__ ((visibility ("hidden"))) http_read2(HTTP_HANDLE handle, void *data, int max_size, int tmo, int *bytes_read);
void __attribute__ ((visibility ("hidden"))) http_unchunk_data(char *buffer);
int __attribute__ ((visibility ("hidden"))) clear_stream(HTTP_HANDLE handle, void *data, int max_size, int *bytes_read);
#endif  // _HTTP_H


