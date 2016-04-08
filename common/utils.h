#ifndef _COMMON_UTILS_H
#define _COMMON_UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
//#include "hpmud.h"

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define BUG(args...) syslog(LOG_ERR, __FILE__ " " STRINGIZE(__LINE__) ": " args)
#define DBG(args...) syslog(LOG_INFO, __FILE__ " " STRINGIZE(__LINE__) ": " args)

#define UTILS_LINE_SIZE 256     /* Length of a line. */
#define UTILS_BUFFER_SIZE 16384  /* General Read/Write buffer. */

#define MAX_FILE_PATH_LEN   512

#define PRNT_PLUGIN_LJ "lj.so"
#define PRNT_PLUGIN_HBPL1 "hbpl1.so"
#define SCAN_PLUGIN_MARVELL "bb_marvell.so"
#define SCAN_PLUGIN_SOAP "bb_soap.so"
#define SCAN_PLUGIN_SOAPHT "bb_soapht.so"
#define SCAN_PLUGIN_ESCL "bb_escl.so"

#define HPLIP_PLUGIN_STATE  "/var/lib/hp/hplip.state"
#define CUPS_TMP_DIR   getenv("TMPDIR") ? : getenv("HOME") ?:"/tmp"

enum UTILS_CONF_RESULT
{
   UTILS_CONF_OK = 0,
   UTILS_CONF_FILE_NOT_FOUND,       // =1,
   UTILS_CONF_SECTION_NOT_FOUND,    // =2,
   UTILS_CONF_KEY_NOT_FOUND,        // =3,
   UTILS_CONF_DATFILE_ERROR,        // = 4,
};


enum UTILS_PLUGIN_STATUS
{
   UTILS_PLUGIN_STATUS_OK = 0,
   UTILS_PLUGIN_STATUS_MISMATCH,        // = 1,
   UTILS_PLUGIN_STATUS_NOT_INSTALLED,   // = 2,
};

enum UTILS_PLUGIN_LIBRARY_TYPE
{
   UTILS_PRINT_PLUGIN_LIBRARY = 0,          // = 0,
   UTILS_SCAN_PLUGIN_LIBRARY,           // =1,
   UTILS_FAX_PLUGIN_LIBRARY,            // =2,
//   UTILS_GENERAL_PLUGIN_LIBRARY         //=4,     // Future use.. 
};


#ifdef __cplusplus
extern "C" {
#endif
   
    /*
     * get_conf - get key value from hplip.conf
     *
     * This function is a stateless utils helper function.
     *
     * inputs:
     *  section - zero terminated string (ie: "[dirs]")
     *  key - zero terminated string (ie: "home")
     *  value_size - size of value buffer in bytes
     *
     * outputs:
     *  return value - see enum definition
     */
    enum UTILS_CONF_RESULT get_conf(const char *section, const char *key, char *value, int value_size);

    /*
     * get_key_value - get key value from specified file
     *
     * This function is a stateless utils helper function.
     *
     * inputs:
     *  file - zero terminated file path
     *  section - zero terminated string (ie: "[dirs]")
     *  key - zero terminated string (ie: "home")
     *  value_size - size of value buffer in bytes
     *
     * outputs:
     *  return value - see enum definition
     */       

    enum UTILS_CONF_RESULT get_key_value(const char *file, const char *section, const char *key, char *value, int value_size);

    /*
     * validate_plugin_version - validates the plugin version
     *
     * inputs: void
     *
     * outputs:
     *  return value - see enum definition
     */       

    enum UTILS_PLUGIN_STATUS validate_plugin_version();

    
    /*
     * load_library - Loads the Library by validating the library.
     *
     * inputs: 
     *         const char *szLibName  - Plugin or Library Name (full path, if required)
     * outputs:
     *        return void* - returns dlopen handler. Returns NULL, in case of error.
     */       
     
    void *load_library (const char *szLibName);

     /*
     * load_plugin_library - Loads the Plugin Library depending the module type.
     *
     * inputs: enum UTILS_PLUGIN_LIBRARY_TYPE  -see enum definition
     *         const char *szPluginName  - Plugin or Library Name (e.g. "lj.so")
     * outputs:
     *        return void* - returns dlopen handler. Returns NULL, in case of error.
     */       
     
    void *load_plugin_library (enum UTILS_PLUGIN_LIBRARY_TYPE eLibraryType, const char *szPluginName);

         /*
     * get_library_symbol - Loads the symbol from the library.
     *
     * inputs: void *pLibHandler  -dlopen Handler
     *         const char *szSymbol  - Symbol to search in library
     * outputs:
     *        return void* - returns dlsym handler. Returns NULL, in case of error.
     */       

    void *get_library_symbol(void *pLibHandler, const char *szSymbol);
         /*
     * unload_library - Unloads the Library.
     *
     * inputs: void *pLibHandler  -dlopen Handler
     *
     * outputs:
     *        return void.
     */       

    void unload_library(void *pLibHandler);

    /*
    *  Function : createTempFile()
    * Argements: 
    *    1. char* szFileName --> (in and out) --> FIle name must contains "XXXXXX" at end. e.g. "/tmp/file_XXXXXX"
    *    2. FILE** pFilePtr  --> (out)        --> returns the File pionter
    * Return value:
    *   Returns file descreptor
    */

    int createTempFile(char* szFileName, FILE** pFilePtr);
    int getHPLogLevel();

#ifdef __cplusplus
}
#endif




#endif //_COMMON_UTILS_H
