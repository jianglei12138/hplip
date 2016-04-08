/************************************************************************************\

  xml.c - HP SANE backend support for xml parsing

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>

int __attribute__ ((visibility ("hidden"))) get_array_size(const char *tag)
{
   char *p, *tail;

   if ((p = strstr(tag, "arraySize=\"")))
      return strtol(p+11, &tail, 10);
   else
      return 0;
}

/* Get xml element from the buffer. The returned element is zero terminated. */
int __attribute__ ((visibility ("hidden"))) get_element(const char *buf, int buf_size, char *element, int element_size, char **tail)
{
   int i, j;

   element[0]=0;

   for (i=0, j=0; buf[i] != '<' && j < (element_size-1) && i < buf_size; i++)
      element[j++] = buf[i];

   element[j]=0;   /* zero terminate */

   if (tail != NULL)
      *tail = (char *)buf + i;   /* tail points to next tag */

   return j;  /* length does not include zero termination */
}

/* Get next xml tag from the buffer. The returned xml tag is zero terminated. */
int __attribute__ ((visibility ("hidden"))) get_tag(const char *buf, int buf_size, char *tag, int tag_size, char **tail)
{
   int i=0, j=0, dd=0, lf=0;

   tag[0]=0;

   while (1)
   {
      for (; buf[i] != '<' && i < buf_size; i++);  /* eat up space before '<' */

      if (buf[i] != '<')
         break;

      if (i < (buf_size-4) && (strncmp(&buf[i], "<!--", 4) == 0))
      {
         for (; buf[i] != '>' && i < buf_size; i++);  /* eat comment line */
         i++;
         continue;
      }
   
      i++; /* eat '<' */

      for (j=0; buf[i] != '>' && j < (tag_size-1) && i < buf_size; i++)
      {
         if (buf[i] == '\r')
         {
            tag[j++] = '\n';   /* convert CR to LF */
            lf=1;            /* set remove back-to-back LF flag */
         }
         else if (buf[i] == '\n')
         {
            if (!lf)
               tag[j++] = buf[i]; 
         }
         else if (buf[i] == ' ')
         {
            if (!dd)
            {
               tag[j++] = buf[i];
               dd=1;             /* set remove back-to-back space flag */
            }
         }
         else
         {
            tag[j++] = buf[i];
            dd=0;
            lf=0;
         }
      }
      break;
   }
 
   if (i < buf_size)
      i++;     /* eat '>' */

   tag[j]=0;  /* zero terminate */

   if (tail != NULL)
      *tail = (char *)buf + i;   /* tail points to next tag */

   return j;   /* length does no include zero termination */
}

