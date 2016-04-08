#include "utils.h"
#include "string.h"
#include <dlfcn.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

extern int errno;

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
   while ((buf[i] != '=') && (i < buf_len) && (j < UTILS_LINE_SIZE))
      key[j++] = buf[i++];
   for (j--; key[j] == ' ' && j > 0; j--);  /* eat white space before = */
   key[++j] = 0;

   if (buf[i] == '=')
      for (i++; buf[i] == ' ' && i < buf_len; i++);  /* eat white space after = */

   j = 0;
   while ((buf[i] != '\n') && (i < buf_len) && (j < UTILS_LINE_SIZE))
      value[j++] = buf[i++];
   for (j--; value[j] == ' ' && j > 0; j--);  /* eat white space before \n */
   value[++j] = 0;

   if (buf[i] == '\n')
     i++;   /* bump past '\n' */

   if (tail != NULL)
      *tail = buf + i;  /* tail points to next line */

   return i;
}


/* Get value for specified section and key from hplip.conf. */
enum UTILS_CONF_RESULT get_conf(const char *section, const char *key, char *value, int value_size)
{
   return get_key_value(CONFDIR "/hplip.conf", section, key, value, value_size);
}

/* Get value for specified section and key from specified file. */
enum UTILS_CONF_RESULT get_key_value(const char *file, const char *section, const char *key, char *value, int value_size)
{
   char new_key[UTILS_LINE_SIZE];
   char new_value[UTILS_LINE_SIZE];
   char rcbuf[255];
   char new_section[32];
   char *tail;
   FILE *inFile;
   enum UTILS_CONF_RESULT stat = UTILS_CONF_DATFILE_ERROR;
   int i,j;

   if((inFile = fopen(file, "r")) == NULL) 
   {
      BUG("unable to open %s: %m\n", file);
      goto bugout;
   } 

   new_section[0] = 0;

   /* Read the config file */
   while ((fgets(rcbuf, sizeof(rcbuf), inFile) != NULL))
   {
      if (rcbuf[0] == '[')
      {
         i = j = 0;
         while ((rcbuf[i] != ']') && (j < (sizeof(new_section)-2)))
            new_section[j++] = rcbuf[i++];
         new_section[j++] = rcbuf[i++];   /* ']' */
         new_section[j] = 0;        /* zero terminate */
         continue;
      }

      GetPair(rcbuf, strlen(rcbuf), new_key, new_value, &tail);

      if ((strcasecmp(new_section, section) == 0) && (strcasecmp(new_key, key) == 0))
      {
         strncpy(value, new_value, value_size);
         stat = UTILS_CONF_OK;
         break;  /* done */
      }
   }

   if (stat != UTILS_CONF_OK)
      BUG("unable to find %s %s in %s\n", section, key, file);
        
bugout:        
   if (inFile != NULL)
      fclose(inFile);
         
   return stat;
}


enum UTILS_PLUGIN_STATUS validate_plugin_version()
{
    char hplip_version[128];
    char plugin_version[128];

    if (get_conf("[hplip]", "version", hplip_version, sizeof(hplip_version)) != UTILS_CONF_OK)
      return UTILS_PLUGIN_STATUS_NOT_INSTALLED;

    if (get_key_value(HPLIP_PLUGIN_STATE,"[plugin]" , "version", plugin_version, sizeof(plugin_version)) != UTILS_CONF_OK )
    {
        BUG("validate_plugin_version() Failed to get Plugin version from [%s]\n", "/var/lib/hp/hplip.state");
        return UTILS_PLUGIN_STATUS_NOT_INSTALLED;
    }


    if (strcmp(hplip_version, plugin_version) == 0)
    {
        return UTILS_PLUGIN_STATUS_OK;
    }
    else
    {
        BUG("validate_plugin_version() Plugin version[%s] mismatch with HPLIP version[%s]\n",plugin_version, hplip_version);
        return UTILS_PLUGIN_STATUS_MISMATCH;
    }
    return UTILS_PLUGIN_STATUS_NOT_INSTALLED;
}


void *load_plugin_library (enum UTILS_PLUGIN_LIBRARY_TYPE eLibraryType, const char *szPluginName)
{
    void *pHandler = NULL;
    char szHome[256];
    char szLibraryFile[256];

    if (szPluginName == NULL || szPluginName[0] == '\0')
    {
        BUG("Invalid Library name\n");
        return pHandler;
    }
    
    if (get_conf("[dirs]", "home", szHome, sizeof(szHome)) != UTILS_CONF_OK)
    {
        BUG("Failed to find the home directory from hplip.conf file\n");
        return pHandler;
    }
    
    if (validate_plugin_version() != UTILS_PLUGIN_STATUS_OK )
    {
        BUG("Plugin version is not matching \n");
        return pHandler;
    }
    
    if (eLibraryType == UTILS_PRINT_PLUGIN_LIBRARY)
        snprintf(szLibraryFile, sizeof(szLibraryFile), "%s/prnt/plugins/%s", szHome, szPluginName);
    else if (eLibraryType == UTILS_SCAN_PLUGIN_LIBRARY)
        snprintf(szLibraryFile, sizeof(szLibraryFile), "%s/scan/plugins/%s", szHome, szPluginName);
    else if (eLibraryType == UTILS_FAX_PLUGIN_LIBRARY)
        snprintf(szLibraryFile, sizeof(szLibraryFile), "%s/fax/plugins/%s", szHome, szPluginName);
    else
    {
        BUG("Invalid Library Type =%d \n",eLibraryType);
        return pHandler;
    }

    return load_library (szLibraryFile);
    
}

void *load_library (const char *szLibraryFile)
{
    void *pHandler = NULL;

    if (szLibraryFile == NULL || szLibraryFile[0] == '\0')
    {
        BUG("Invalid Library name\n");
        return pHandler;
    }
    
    if ((pHandler = dlopen(szLibraryFile, RTLD_NOW|RTLD_GLOBAL)) == NULL)
        BUG("unable to load library %s: %s\n", szLibraryFile, dlerror());

    return pHandler;
}

void *get_library_symbol(void *pLibHandler, const char *szSymbol)
{
    void *pSymHandler = NULL;
    if (pLibHandler == NULL)
    {
        BUG("Invalid Library hanlder\n");
        return NULL;
    }

    if (szSymbol == NULL || szSymbol[0] == '\0')
    {
        BUG("Invalid Library symbol\n");
        return NULL;
    }

    pSymHandler = dlsym(pLibHandler, szSymbol);
    if (pSymHandler == NULL)
        BUG("Can't find %s symbol in Library:%s\n",szSymbol,dlerror());

    return pSymHandler;
}

void unload_library(void *pLibHandler)
{
    if (pLibHandler)
        dlclose(pLibHandler);
    else
        BUG("Invalid Library hanlder pLibHandler = NULL.\n");
}

int createTempFile(char* szFileName, FILE** pFilePtr)
{
    int iFD = -1;

    if (szFileName == NULL || szFileName[0] == '\0' || pFilePtr == NULL)
    {
        BUG("Invalid Filename/ pointer\n");
        return 0;
    }

    if (strstr(szFileName,"XXXXXX") == NULL)
        strcat(szFileName,"_XXXXXX");

    iFD = mkstemp(szFileName);
    if(-1 == iFD)
    {
        BUG("Failed to create the temp file Name[%s] errno[%d : %s]\n",szFileName,errno,strerror(errno));
        return 0;
    }
    else
    {
        *pFilePtr = fdopen(iFD,"w+");
    }

    return iFD;
}

int getHPLogLevel()
{
    FILE    *fp;
    char    str[258];
    char    *p;
    int iLogLevel = 0;

    fp = fopen ("/etc/cups/cupsd.conf", "r");
    if (fp == NULL)
        return 0;
    while (!feof (fp))
    {
        if (!fgets (str, 256, fp))
        {
            break;
        }
        if ((p = strstr (str, "hpLogLevel")))
        {
            p += strlen ("hpLogLevel") + 1;
            iLogLevel = atoi (p);
            break;
        }
    }
    fclose (fp);
    return iLogLevel;
}


