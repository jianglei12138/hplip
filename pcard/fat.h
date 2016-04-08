/*****************************************************************************\

  fat.h - FAT12/FAT16 file system
 
  (c) 2004 Copyright HP Development Company, LP

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USALP

\*****************************************************************************/
#ifndef _FAT_H
#define _FAT_H

#define FAT_HARDSECT 512  /* sector size in bytes */

/* Block size is set to the lowest common denominator. The ps325 reads 3 sectors max per command (LP?). While the ps130, ps 245,
 * and ?? can do 8 sector reads.
 */
#define FAT_BLKSIZE 3    /* block size in sectors */

typedef struct
{
   char Name[16];
   char Attr;
   int Size;
} FILE_ATTRIBUTES;

typedef struct
{
   char OEMID[8];                  
   int BytesPerSector;         
   int SectorsPerCluster;       
   int ReservedSectors;        
   int RootEntries;         
   int SectorsPerFat;         
   char VolumeLabel[11]; 
   char SystemID[8];     
   int WriteProtect;
} PHOTO_CARD_ATTRIBUTES;

/* APIs */
int FatInit(void);
int FatListDir(void);
int FatReadFile(char *name, int fd);
int FatReadFileExt(char *name, int offset, int len, void *outbuf);
int FatSetCWD(char *dir);
int FatDeleteFile(char *name); 
int FatFreeSpace(void);
int FatDirBegin(FILE_ATTRIBUTES * a);
int FatDirNext(FILE_ATTRIBUTES * a);
int FatDiskAttributes(PHOTO_CARD_ATTRIBUTES * pa);

/* System dependent external functions */
extern int ReadSector(int sector, int nsector, void *buf, int size);
extern int WriteSector(int sector, int nsector, void *buf, int size);

extern int verbose;

#if defined(WORDS_BIGENDIAN)
#define h2lits(A) ((((uint16_t)(A) & 0xff00) >> 8) | (((uint16_t)(A) & 0x00ff) << 8))    /* host to little-endian 16-bit value */
#define lit2hs h2lits                         /* little-endian to host 16-bit value */

#else
#define h2lits(A) (A)
#define lit2hs(A) (A)
#endif

#endif // _FAT_H
