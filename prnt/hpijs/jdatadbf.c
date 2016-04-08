#if defined (APDK_LJJETREADY) || defined (APDK_QUICKCONNECT) || defined (APDK_PSCRIPT)
/*
 * jdatadbf.c
 *
 * Copyright (C) 2001, Dorian Goldstein, Thomas G. Lane.
 *
 * This file contains compression data destination routines for the case of
 * This file is identicle with IJG's built in destination file manager in
 * every respect but 1... instead fo emmiting data to a file
 * it facilitates definition of a callback function.
 */

/* this is not a core library module, so it doesn't define JPEG_INTERNALS */
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

//Define JMETHOD macro here if not defined already in jmorecfg.h. JMETHOD macro has been removed from libjpeg-turbo 1.3.90.2
#ifndef JMETHOD
#define JMETHOD(type,methodname,arglist)  type (*methodname) ()
#endif

/* Expanded data destination object for stdio output */

typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */

  JOCTET * outbuff;             // target output buffer
  JOCTET * buffer;                // start of internal buffer
  UINT16   size_outbuff;        // current size of target output buffer
  JMETHOD (void, flush_output_buffer_callback, (JOCTET *outbuf, JOCTET* buffer, size_t size));
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE  4096    /* choose an efficiently fwrite'able size */


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  /* Allocate the output buffer --- it will be released when done with image */
  dest->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                  OUTPUT_BUF_SIZE * SIZEOF(JOCTET));

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

// DG Temp   if (dest->flush_output_buffer_callback == NULL) {
// DG Temp     if (dest->outbuff) {
// DG Temp       MEMCOPY(dest->outbuff + dest->size_outbuff, dest->buffer, OUTPUT_BUF_SIZE);
// DG Temp       dest->size_outbuff += OUTPUT_BUF_SIZE;
// DG Temp     }
// DG Temp   } else {
// DG Temp     (*dest->flush_output_buffer_callback)(dest->buffer, OUTPUT_BUF_SIZE);
// DG Temp   }

  (*dest->flush_output_buffer_callback)(dest->outbuff, dest->buffer, OUTPUT_BUF_SIZE);


  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

  return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

  /* Write any data remaining in the buffer */
  if (datacount > 0) {
// DG Temp     if (dest->flush_output_buffer_callback == NULL) {
// DG Temp       if (dest->outbuff) {
// DG Temp         MEMCOPY(dest->outbuff + dest->size_outbuff, dest->buffer, datacount);
// DG Temp         dest->size_outbuff += datacount;
// DG Temp       }
// DG Temp     } else {
// DG Temp       (*dest->flush_output_buffer_callback)(dest->buffer, datacount);
// DG Temp     }

    (*dest->flush_output_buffer_callback)(dest->outbuff, dest->buffer, datacount);
  }
}


/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

GLOBAL(void)
jpeg_buffer_dest (j_compress_ptr cinfo, JOCTET* outbuff, void* flush_output_buffer_callback)
{
  my_dest_ptr dest;

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if (cinfo->dest == NULL) {    /* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                  SIZEOF(my_destination_mgr));
  }

  dest = (my_dest_ptr) cinfo->dest;
  dest->pub.init_destination    = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination    = term_destination;

  dest->outbuff      = outbuff;
  dest->size_outbuff = 0;
  dest->flush_output_buffer_callback = flush_output_buffer_callback;
//(*dest->flush_output_buffer_callback)(-1 , -1);
}

GLOBAL(long)
jpeg_buffer_size_dest (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  return dest->size_outbuff;
}
#endif // APDK_LJJETREADY || APDK_QUICKCONNECT || APDK_PSCRIPT
