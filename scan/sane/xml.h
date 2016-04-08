/************************************************************************************\

  xml.h - HP SANE backend support for xml parsing

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

  Author: David Suffield

\************************************************************************************/
#ifndef _MXML_H
#define _MXML_H

int __attribute__ ((visibility ("hidden"))) get_array_size(const char *tag);
/* Get xml element from the buffer. The returned element is zero terminated. */
int __attribute__ ((visibility ("hidden"))) get_element(const char *buf, int buf_size, char *element, int element_size, char **tail);
/* Get next xml tag from the buffer. The returned xml tag is zero terminated. */
int __attribute__ ((visibility ("hidden"))) get_tag(const char *buf, int buf_size, char *tag, int tag_size, char **tail);

#endif  // _MXML_H
