/*****************************************************************************\

  model.c - model parser for hplip devices 
 
  (c) 2006-2007 Copyright HP Development Company, LP

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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "list.h"
#include "hpmud.h"
#include "hpmudi.h"

#define SECTION_SIZE 4096 /* Rough estimate of key/value section size in bytes. */

typedef struct
{
  char *name;
  char *incFile; 
  int valueSize;  /* size of list in bytes */
  char *value;    /* list of key/value pairs */
  struct list_head list;
} LabelRecord;

static LabelRecord head;   /* list of labels from include files */
static char homedir[255] = "";

static int GetPair(char *buf, int buf_len, char *key, char *value, char **tail)
{
   int i=0, j;

   key[0] = 0;
   value[0] = 0;

   if (buf[i] == '#')
   {
      for (; buf[i] != '\n' && i < buf_len; i++);  /* eat comment line */
      if (buf[i] == '\n')
         i++;   /* bump past '\n' */
   }

   j = 0;
   while ((buf[i] != '=') && (i < buf_len) && (j < HPMUD_LINE_SIZE))
      key[j++] = buf[i++];
   for (j--; key[j] == ' ' && j > 0; j--);  /* eat white space before = */
   key[++j] = 0;

   if (buf[i] == '=')
      for (i++; buf[i] == ' ' && i < buf_len; i++);  /* eat white space after = */

   j = 0;
   while ((buf[i] != '\n') && (i < buf_len) && (j < HPMUD_LINE_SIZE))
      value[j++] = buf[i++];
   for (j--; value[j] == ' ' && j > 0; j--);  /* eat white space before \n */
   value[++j] = 0;

   if (buf[i] == '\n')
     i++;   /* bump past '\n' */

   if (tail != NULL)
      *tail = buf + i;  /* tail points to next line */

   return i;
}

static int ReadConfig()
{
   char key[HPMUD_LINE_SIZE];
   char value[HPMUD_LINE_SIZE];
   char rcbuf[255];
   char section[32];
   char *tail;
   FILE *inFile = NULL;
   int stat=1;

   homedir[0] = 0;
        
   if((inFile = fopen(CONFDIR "/hplip.conf", "r")) == NULL) 
   {
      BUG("unable to open %s: %m\n", CONFDIR "/hplip.conf");
      goto bugout;
   } 

   section[0] = 0;

   /* Read the config file */
   while ((fgets(rcbuf, sizeof(rcbuf), inFile) != NULL))
   {
      if (rcbuf[0] == '[')
      {
         strncpy(section, rcbuf, sizeof(section)); /* found new section */
         continue;
      }

      GetPair(rcbuf, strlen(rcbuf), key, value, &tail);

      if ((strncasecmp(section, "[dirs]", 6) == 0) && (strcasecmp(key, "home") == 0))
      {
         strncpy(homedir, value, sizeof(homedir));
         break;  /* done */
      }
   }
        
   stat = 0;

bugout:        
   if (inFile != NULL)
      fclose(inFile);
         
   return stat;
}

/* Find last occurance of y in x. */
static char *strrstr(const char *x, const char *y) 
{
   char *prev=NULL, *next;

   if (*y == '\0')
      return strchr(x, '\0');

   while ((next = strstr(x, y)) != NULL)
   {
      prev = next;
      x = next + 1;
   }
   return prev;
}

static int CopyLabel(char *label, char *buf, int bufSize)
{
   struct list_head *p;
   LabelRecord *pl;
   int i=0, found=0;

   /* Look for label. */
   list_for_each(p, &head.list)
   {
      pl = list_entry(p, LabelRecord, list);
      if (strcasecmp(pl->name, label) == 0)
      {
         found = 1;    /* found label */
         break;
      }
   }

   if (!found)
   {
      BUG("error undefined label %s\n", label);
      goto bugout;
   }

   if (pl->valueSize > bufSize)
   {
      BUG("error label %s size=%d buf=%d\n", label, pl->valueSize, bufSize);
      goto bugout;
   }

   memcpy(buf, pl->value, pl->valueSize);
   i=pl->valueSize;

bugout:
   return i;
}

static int ResolveAttributes(FILE *fp, char *attr, int attrSize)
{
   char label[128];
   int i=0, j, ch;

   /* Process each key/value line. */
   ch = fgetc(fp);
   while (ch != EOF)
   {
      if (ch == '[')
      {
         ungetc(ch, fp);     /* found new section, done with current section */
         break;         
      }

      if (ch == '#' || ch == ' ')
      {
         while ((ch = fgetc(fp)) != '\n' && ch != EOF);  /* skip line */
      }
      else if (ch == '\n')
      {
         /* skip blank line */
      }
      else if (ch == '%')
      {
         j=0;
         while ((ch = fgetc(fp)) != '\n' && ch != EOF)  /* get label */
         {
            if (j < sizeof(label)-1)
               label[j++] = ch;
         }
         label[j-1] = 0;
         i += CopyLabel(label, attr+i, attrSize-i);
      }
      else
      {
         if (i < attrSize-1)
            attr[i++] = ch;
         while ((ch = fgetc(fp)) != '\n' && ch != EOF)  /* get key/value line */
         {
            if (i < attrSize-1)
               attr[i++] = ch;
         }
         if (i < attrSize-1)
            attr[i++] = '\n';
      }

      if (ch == '\n')
         ch = fgetc(fp);   /* bump to next line */
      continue;
   }

   attr[i] = 0;   /* terminate string */

   return i;
}
static int RegisterLabel(FILE *fp, char *incFile, char *label)
{
   struct list_head *p;
   LabelRecord *pl;
   char buf[SECTION_SIZE];
   int i=0, stat=1, ch;

   /* Look for duplicate label. */
   list_for_each(p, &head.list)
   {
      pl = list_entry(p, LabelRecord, list);
      if (strcasecmp(pl->name, label) == 0)
      {
         BUG("error duplicate label %s\n", label);
         goto bugout;
      }
   }

   if ((pl = (LabelRecord *)malloc(sizeof(LabelRecord))) == NULL)
   {
      BUG("unable to creat label record: %m\n");
      goto bugout;
   }

   pl->incFile = strdup(incFile);
   pl->name = strdup(label);

   /* Process each key/value line. */
   ch = fgetc(fp);
   while (ch != EOF)
   {
      if (ch == '[')
      {
         ungetc(ch, fp);     /* found new section, done with label */
         break;         
      }

      if (ch == '#' || ch == ' ')
      {
         while ((ch = fgetc(fp)) != '\n' && ch != EOF);  /* skip line */
      }
      else if (ch == '\n')
      {
         /* skip blank line */
      }
      else
      {
         if (i < SECTION_SIZE-1)
            buf[i++] = ch;
         while ((ch = fgetc(fp)) != '\n' && ch != EOF)  /* get key/value line */
         {
            if (i < SECTION_SIZE-1)
               buf[i++] = ch;
         }
         if (i < SECTION_SIZE-1)
            buf[i++] = '\n';
      }

      if (ch == '\n')
         ch = fgetc(fp);   /* bump to next line */
      continue;
   }

   buf[i] = 0;   /* terminate string */

   pl->value = strdup(buf);
   pl->valueSize = i;  /* size does not include zero termination */

   list_add(&(pl->list), &(head.list));
   stat = 0;

bugout:

   return stat;
}

static int UnRegisterLabel(LabelRecord *pl)
{
   if (pl->incFile)
      free(pl->incFile);
   if (pl->name)
      free(pl->name);
   if (pl->value)
      free(pl->value);
   list_del(&(pl->list));
   free(pl);
   return 0;
}

static int DelList()
{
   struct list_head *p, *n;
   LabelRecord *pl;
 
   /* Remove each label. */
   list_for_each_safe(p, n, &head.list)
   {
      pl = list_entry(p, LabelRecord, list);
      UnRegisterLabel(pl);
   }
   return 0;
}

/* Parse *.inc file. */
static int ParseInc(char *incFile)
{
   FILE *fp=NULL;
   struct list_head *p;
   LabelRecord *pl;
   char rcbuf[255];
   char section[128];
   int stat=1, n;

   /* Look for duplicate include file. */
   list_for_each(p, &head.list)
   {
      pl = list_entry(p, LabelRecord, list);
      if (strcmp(pl->incFile, incFile) == 0)
      {
         BUG("error duplicate include file %s\n", incFile);
         goto bugout;
      }
   }

   if ((fp = fopen(incFile, "r")) == NULL)
   {
      BUG("open %s failed: %m\n", incFile);
      goto bugout;
   }

   section[0] = 0;

   /* Read the *.inc file, check each line for new label. */
   while ((fgets(rcbuf, sizeof(rcbuf), fp) != NULL))
   {
      if (rcbuf[0] == '[')
      {
         strncpy(section, rcbuf+1, sizeof(section)); /* found new section */
         n = strlen(section);
         section[n-2]=0; /* remove ']' and CR */
         RegisterLabel(fp, incFile, section);
      }
   }

   stat = 0;

bugout:
   if (fp)
      fclose(fp);
   return stat;
}

/* Parse *.dat file. */
static int ParseFile(char *datFile, char *model, char *attr, int attrSize, int *bytes_read)
{
   FILE *fp;
   char rcbuf[255];
   char section[128];
   char file[128];
   int found=0, n;

   if ((fp = fopen(datFile, "r")) == NULL)
      goto bugout;

   section[0] = 0;

   /* Read the *.dat file, check each line for model match. */
   while ((fgets(rcbuf, sizeof(rcbuf), fp) != NULL))
   {
      if (rcbuf[0] == '[')
      {
         strncpy(section, rcbuf+1, sizeof(section)); /* found new section */
         n = strlen(section);
         section[n-2]=0; /* remove ']' and CR */
         if (strcasecmp(model, section) == 0)
         {
            /* Found model match. */
            *bytes_read = ResolveAttributes(fp, attr, attrSize); 
            found = 1; 
            break;
         }
      }
      else if (strncmp(rcbuf, "%include", 8) == 0)
      {
         strncpy(file, datFile, sizeof(file));        /* get dirname from *.dat file */
         n = strrstr(file, "/") - file + 1;
         strncpy(file+n, rcbuf+9, sizeof(file)-n);      /* concatenate include filename to dirname */
         n = strlen(file);
         file[n-1]=0;        /* remove CR */
         ParseInc(file);
      }
   }

bugout:
   if (fp)
      fclose(fp);

   return found;
}

/* Parse and convert all known key value pairs in buffer. Do sanity check on values. */
static int parse_key_value_pair(char *buf, int len, struct hpmud_model_attributes *ma)
{
   char key[HPMUD_LINE_SIZE];
   char value[HPMUD_LINE_SIZE];
   char *tail, *tail2;
   int i=0, ret=HPMUD_R_OK;

   ma->prt_mode = HPMUD_RAW_MODE;
   ma->mfp_mode = HPMUD_DOT4_MODE;
   ma->scantype = 0;
   ma->statustype = HPMUD_STATUSTYPE_SFIELD;
   ma->support = HPMUD_SUPPORT_TYPE_NONE;

   if (buf == NULL)
      return HPMUD_R_OK;    /* initialize ma */

   tail = buf;

   while (i < len)
   {
      i += GetPair(tail, len-i, key, value, &tail);

      if (strcasecmp(key, "io-mode") == 0)
      {
         ma->prt_mode = strtol(value, &tail2, 10);      /* uni | raw | mlc */
      }
      else if (strcasecmp(key, "io-mfp-mode") == 0)
      {
         ma->mfp_mode = strtol(value, &tail2, 10);      /* mfc | dot4 */
      }
      else if(strcasecmp(key, "scan-type") == 0)
      {
         ma->scantype = strtol(value, &tail2, 10); /*SCL, PML, SOAP, MARVELL, LEDM*/
      }
      else if(strcasecmp(key, "scan-src") == 0)
      {
         ma->scansrc = strtol(value, &tail2, 10); /*Flatbed, ADF, Camera or combination of these*/
      }
      else if(strcasecmp(key, "status-type") == 0)
      {
         ma->statustype = strtol(value, &tail2, 10);
      }
      else if(strcasecmp(key, "support-type") == 0)
      {
         ma->support = strtol(value, &tail2, 10);
      }
      else if(strcasecmp(key, "plugin") == 0)
      {
         ma->plugin = strtol(value, &tail2, 10);
      }
      else
      {
         /* Unknown keys are ignored (R_AOK). */
      }
   }  // end while (i < len)

   return ret;
}

/* Request device model attributes for URI. Return all attributes. */
enum HPMUD_RESULT hpmud_get_model_attributes(char *uri, char *attr, int attrSize, int *bytes_read)
{
   char sz[256];
   char model[256];
   int found;
   enum HPMUD_RESULT stat = HPMUD_R_DATFILE_ERROR;

   memset(attr, 0, attrSize);

   INIT_LIST_HEAD(&head.list);

   if (homedir[0] == 0)    
      ReadConfig();

   hpmud_get_uri_model(uri, model, sizeof(model));

   /* Search /data/models.dat file for specified model. */
   snprintf(sz, sizeof(sz), "%s/data/models/models.dat", homedir);
   found = ParseFile(sz, model, attr, attrSize, bytes_read);   /* save any labels in *.inc files */

   if (!found)
   {
      BUG("no %s attributes found in %s\n", model, sz);  

      DelList();   /* Unregister all labels. */

      /* Search /data/models/unreleased/unreleased.dat file for specified model. */
      snprintf(sz, sizeof(sz), "%s/data/models/unreleased/unreleased.dat", homedir);
      found = ParseFile(sz, model, attr, attrSize, bytes_read);   /* save any *.inc files */
   }

   if (!found)
   {  
      BUG("no %s attributes found in %s\n", model, sz);  
      goto bugout;
   }  

   stat = HPMUD_R_OK;

bugout:   
   DelList();  /* Unregister all labels. */
   return stat;
}

/* Request device model attributes for URI. Return filled in hpmud_model_attributes structure. */
enum HPMUD_RESULT hpmud_query_model(char *uri, struct hpmud_model_attributes *ma)
{
   char buf[SECTION_SIZE];
   int len;
   enum HPMUD_RESULT stat = HPMUD_R_DATFILE_ERROR;

   parse_key_value_pair(NULL, 0, ma);  /* set ma defaults */

   if (hpmud_get_model_attributes(uri, buf, sizeof(buf), &len) != 0)
      goto bugout;  /* model not found, return ma defaults */

   parse_key_value_pair(buf, len, ma);

   stat=HPMUD_R_OK;

bugout:

   return stat;
}


