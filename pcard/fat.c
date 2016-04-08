/*****************************************************************************\

  fat.c - FAT12/FAT16 file system

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

  Author: David Suffield

  Caveats:
  1. Global variables are used, therefore this library is not thread safe.
  2. Current implementation has not been tested on a big-edian system. 

\*****************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "fat.h"

/*
 * Private data structures.
 */

typedef struct
{
   int FatStartSector;
   int DataStartSector;
   int RootDirStartSector;
   int RootDirNumSectors;
   uint8_t *Fat;            /* cached FAT16 data */
   int FatSize;             /* in bytes */
   uint8_t *Fat12;         /* FAT12 backup */
   int Fat12Size;         /* in bytes */
   uint8_t *Fat16;         /* FAT16 backup */
   int WriteProtect;     /* 0=false, 1=true */
} DISK_ATTRIBUTES;

typedef struct
{
   char Name[16];
   int StartCluster;    /* equals zero if root */
   int StartSector;
   int CurrSector;
   int NumEntries;
} CURRENT_WORKING_DIRECTORY;

typedef struct
{
   char Name[16];
   char Attr;
   int StartCluster;
   int CurrCluster;
   int Size;
   int CurrSector;
   int CurrSectorNumInCluster;
   int CurrByte;
   int DirSectorNum;
   int DirEntryNum;           /* number in current directory */
} CURRENT_FILE_ATTRIBUTES;

typedef struct
{
   char JumpInstruction[3];      /* offset = 0 */
   char OEMID[8];                   /* 3 */
   uint16_t BytesPerSector;         /* 11 */
   uint8_t SectorsPerCluster;       /* 13 */
   uint16_t ReservedSectors;        /* 14 */
   uint8_t Fats;          /* number of copies of the fat, 16 */
   uint16_t RootEntries;           /* 17 */
   uint16_t SmallSectors;          /* 19 */
   uint8_t Media;                  /* 21 */
   uint16_t SectorsPerFat;         /* 22 */
   uint16_t SectorsPerTrack;       /* 24 */
   uint16_t Heads;                 /* 26 */
   uint32_t HiddenSectors;         /* 28 */
   uint32_t LargeSectors;  /* number of sector if SmallSectors == 0, 32 */
   uint8_t DriveNumber;     /* 36 */
   uint8_t CurrentHead;     /* 37 */
   uint8_t Signature;       /* 38 */
   uint32_t ID;             /* random serial number, 39 */
   uint8_t VolumeLabel[11]; /* 43 */
   uint8_t SystemID[8];     /* 54 */
   uint8_t LoadInstructions[512-64];
   uint16_t EndSignature; /*=AA55h*/
} __attribute__((packed)) FAT_BOOT_SECTOR;       /* 512 bytes total */

typedef struct
{
   char Name[8],Ext[3];    /* name and extension */
   uint8_t Attributes;     /* attribute bits */
   uint8_t Reserved[10];
   uint16_t Time; 
   uint16_t Date; 
   uint16_t StartCluster;       
   uint32_t Size;      /* size of the file in bytes */ 
} __attribute__((packed)) FAT_DIRECTORY;      /* 32 bytes total */

#define FAT_IS_DIR 0x10
#define FAT_END_OF_DIR 0x2
#define FAT_FILE_DELETED 0xe5
#define FAT_LONG_FILENAME 0x3

/* 
 * Private data objects. Note, static variables are initialized to zero.
 */
static FAT_BOOT_SECTOR bpb;   /* bios parameter block */
static DISK_ATTRIBUTES da;   
static CURRENT_WORKING_DIRECTORY cwd;
static CURRENT_FILE_ATTRIBUTES fa;

/* Convert 12-bit FAT to 16-bit FAT. */
int ConvertFat12to16(uint8_t *dest, uint8_t *src, int maxentry)
{
   uint8_t *p12 = src;
   uint16_t *p16 = (uint16_t *)dest;
   int i;

   for (i=0; i<maxentry; i++)
   {
      if (i&1)
      {
         *p16++ = lit2hs(*(uint16_t *)p12 >> 4);  /* odd fat entry */
         p12+=2;      
      }
      else
      {
         *p16++ = lit2hs(*(uint16_t *)p12 & 0xfff);  /* even fat entry */
         p12++;      
      }
   }
   return 0;
}

/* Convert 16-bit FAT to 12-bit FAT. */
int ConvertFat16to12(uint8_t *dest, uint8_t *src, int maxentry)
{
   uint8_t *p12 = dest;
   uint16_t *p16 = (uint16_t *)src;
   int i;

   for (i=0; i<maxentry; i++, p16++)
   {
      if (i&1)
      {
         *p12 = (uint8_t)h2lits(*p16 >> 4);  /* odd fat entry */
         p12++;      
      }
      else
      {
         *(uint16_t *)p12 = h2lits(*p16 | (*(p16+1) << 12));  /* even fat entry */
         p12+=2;
      }
   }
   return 0;
}

int readsect(int sector, int nsector, void *buf, int size)
{
   int len=nsector, total=0;
   int i, n, stat=1;

   /* Read 1-blksize sectors. */
   for (i=0; i<nsector; i+=n, len-=n)
   {
      n = len > FAT_BLKSIZE ? FAT_BLKSIZE : len;
      if (ReadSector(sector+i, n, buf+total, size-total) != 0)
         goto bugout;
      total += n*FAT_HARDSECT;
   }

   stat = 0;

bugout:
  return stat;
};

int writesect(int sector, int nsector, void *buf, int size)
{
   int len=nsector, total=0;
   int i, n, stat=1;

   /* Write 1-blksize sectors. */
   for (i=0; i<nsector; i+=n, len-=n)
   {
      n = len > FAT_BLKSIZE ? FAT_BLKSIZE : len;
      if (WriteSector(sector+i, n, buf+total, size-total) != 0)
         goto bugout;
      total += n*FAT_HARDSECT;
   }

   stat = 0;

bugout:
  return stat;
};

int UpdateFat(void)
{
   int stat=1;
   uint8_t *p12=NULL;
   int i, total=0;

   if (strcmp((char *)bpb.SystemID, "FAT12") == 0)
   {
      if ((p12 = malloc(da.Fat12Size)) == NULL)
         goto bugout;
      ConvertFat16to12(p12, da.Fat, da.Fat12Size/1.5);
      for (i=0; i<bpb.SectorsPerFat; i++)
      {
         if (memcmp(p12+total, da.Fat12+total, FAT_HARDSECT) != 0)
            if (writesect(da.FatStartSector+i, 1, p12+total, FAT_HARDSECT) != 0)
               goto bugout;
         total += FAT_HARDSECT;
      }
   }
   else
   {  /* Assume FAT16 */
      for (i=0; i<bpb.SectorsPerFat; i++)
      {
         if (memcmp(da.Fat+total, da.Fat16+total, FAT_HARDSECT) != 0)
            if (writesect(da.FatStartSector+i, 1, da.Fat+total, FAT_HARDSECT) != 0)
               goto bugout;
         total += FAT_HARDSECT;
      }
   }

   stat = 0;

bugout:

   if (p12 != NULL)
      free(p12);

   return stat;
};

int RootSetCWD(void)
{
   /* Set CWD to the root directory. */
   cwd.Name[0] = '/'; cwd.Name[1] = 0;
   cwd.StartSector = da.RootDirStartSector;
   cwd.CurrSector = cwd.StartSector;
   cwd.NumEntries = bpb.RootEntries;
   cwd.StartCluster = 0;
    
    return 0;
};

/* Returns number of free clusters (ie: those with 0x0 as the value in FAT). */
int FindFreeClusters(void)
{
   int16_t *pfat = (int16_t *)da.Fat;
   int max_entry = da.FatSize/2;
   int i, freeclusters=0;

   for (i=0; i<max_entry; i++)
   {
      if (*pfat++ == 0x0)
         freeclusters++;
   }
   return freeclusters;
}

int ConvertClusterToSector(int cluster) 
{
   int sector;

   sector = cluster - 2;            /* skip first two FAT entries */
   sector *= bpb.SectorsPerCluster; /* find the sector number relative to data start */
   sector += da.DataStartSector;    /* find absolute sector number */

   return sector;
}

/* Get next cluster from FAT. */
int GetNextCluster(int cluster) 
{
   uint16_t *pfat = (uint16_t *)da.Fat;
   return *(pfat+cluster);
}

/* Tries to load the directory entry specified by filenumber. */
int LoadFileInCWD(int filenumber) 
{
   uint8_t buf[FAT_HARDSECT];
   FAT_DIRECTORY *pde = (FAT_DIRECTORY *)buf;
   int i, j, fn, sector, cluster, cluster_cnt;
   int de_per_sector = FAT_HARDSECT/sizeof(FAT_DIRECTORY);
   uint8_t c;

   sector = filenumber / de_per_sector;        /* determine sector offset */
   fn = filenumber - (sector * de_per_sector);   /* determine file number in sector */

   if (cwd.StartCluster)
   {  /* CWD = subdirectory */

      /* Determine cluster */
      cluster = cwd.StartCluster;
      cluster_cnt = sector / bpb.SectorsPerCluster;
      for (i=0; i<cluster_cnt && cluster<0xfff7 && cluster; i++)
         cluster = GetNextCluster(cluster);
      if (cluster >= 0xfff7 || cluster == 0)
         return FAT_END_OF_DIR;
      cwd.CurrSector = ConvertClusterToSector(cluster);
      sector -= cluster_cnt * bpb.SectorsPerCluster;
   }
   else
   {  /* CWD = root */
      cwd.CurrSector = cwd.StartSector;

      /* Check for max entry. */
      if (filenumber > (da.RootDirNumSectors * de_per_sector))
         return FAT_END_OF_DIR;
   }

   cwd.CurrSector += sector;

   fa.DirEntryNum = fn;
   fa.DirSectorNum = cwd.CurrSector;

   /* Read the directory sector. */
   pde += fn;
   pde->Name[0] = 0;
   readsect(fa.DirSectorNum, 1, buf, sizeof(buf));

   c = pde->Name[0];
   if (c == 0x0)
      return FAT_END_OF_DIR;
   if (c == FAT_FILE_DELETED)
      return FAT_FILE_DELETED;

   /* Read file information from directory and convert to 8.3 format. */
   for (i=0; (i<sizeof(pde->Name)) && pde->Name[i] && (pde->Name[i] != ' '); i++)  /* copy charactors up to any space */
      fa.Name[i] = pde->Name[i];
   if (pde->Ext[0] && (pde->Ext[0] != ' '))
   {
      fa.Name[i++] = '.';
      for (j=0; (pde->Ext[j] != ' ') && (j<sizeof(pde->Ext)); j++, i++)  /* copy charactors up to space */
         fa.Name[i] = pde->Ext[j];
   }
   
   fa.Name[i] = 0;  /* zero terminate */

   fa.Attr =  pde->Attributes;

   if (pde->Attributes == 0xf) 
   { 
      return FAT_LONG_FILENAME;   /* ignor long filename (slot) directory entries */ 
   }

   fa.StartCluster = pde->StartCluster;
   fa.CurrCluster = fa.StartCluster;
   fa.Size = pde->Size;
   fa.CurrSectorNumInCluster = 0;

   return 0;
}

/* Look in CWD for file with given name. */
int LoadFileWithName(char *filename)
{
   int filenum;
   int ret, stat=1;

   filenum = 0;

   while (1)
   {
      ret = LoadFileInCWD(filenum);
      if (ret == FAT_END_OF_DIR) 
         break;

      if ((ret != FAT_FILE_DELETED) && (ret != FAT_LONG_FILENAME))
      {
         if (strcasecmp(fa.Name, filename) == 0)
         {
            stat = 0; /* found file */
            break;            
         }
      }
      filenum++;
   }
   return stat;
}

void PrintCurrFileInfo(void)
{
   fprintf(stdout, "%s   %d bytes (cluster %d, sector %d)", fa.Name, fa.Size,
                fa.StartCluster, ConvertClusterToSector(fa.StartCluster));
   if (fa.Attr & FAT_IS_DIR)
      fputs(" <DIR>\n", stdout);
   else
      fputc('\n', stdout);
}

/* Get the FAT boot sector and root directory. */
int FatInit(void)
{
   int bootsector_startsector, stat=1, fatsize;
   char dummy[FAT_HARDSECT];

   if (da.Fat != NULL)
      free(da.Fat);
   if (da.Fat12 != NULL)
      free(da.Fat12);
   da.Fat = NULL;
   da.Fat12 = NULL;

   /* Assume no MBR and boot sector starts at first sector. */
   bootsector_startsector = 0;

   /* Read boot sector. */
   /*fprintf( stdout, "start=%d", bootsector_startsector );*/
   if (readsect(bootsector_startsector, 1, &bpb, sizeof(bpb)) != 0)
      goto bugout;

   /* TODO: take care big-endian byte ordering in bpb. */

   if (bpb.BytesPerSector != FAT_HARDSECT)
      goto bugout;
   
   bpb.SystemID[5] = 0;

   if (verbose > 0)
   {
      fprintf(stderr, "bytes/sectors=%d\n", bpb.BytesPerSector);
      fprintf(stderr, "sectors/cluster=%d\n", bpb.SectorsPerCluster);
      fprintf(stderr, "reserved sectors=%d\n", bpb.ReservedSectors);
      fprintf(stderr, "sectors/FAT=%d\n", bpb.SectorsPerFat);
      fprintf(stderr, "root entries=%d\n", bpb.RootEntries);
      fprintf(stderr, "small sectors=%d\n", bpb.SmallSectors);
      fprintf(stderr, "large sectors=%d\n", bpb.LargeSectors);
      fprintf(stderr, "system id=%s\n", bpb.SystemID);   
   }

   /* Calculate where the fat and root directory are. */
   da.FatStartSector = bootsector_startsector + bpb.ReservedSectors;
   da.RootDirNumSectors = ((bpb.RootEntries * 32) + (bpb.BytesPerSector - 1)) / bpb.BytesPerSector;
   da.RootDirStartSector = da.FatStartSector + ((int16_t)bpb.Fats * (int16_t)bpb.SectorsPerFat);
   da.DataStartSector = da.RootDirStartSector + da.RootDirNumSectors;

   RootSetCWD();
   fatsize = bpb.SectorsPerFat * FAT_HARDSECT;
   
   if (strcmp((char *)bpb.SystemID, "FAT12") == 0)
   {
      da.Fat12Size = fatsize;
      if ((da.Fat12 = (uint8_t *)malloc(da.Fat12Size)) == NULL)
         goto bugout;
      if (readsect(da.FatStartSector, bpb.SectorsPerFat, da.Fat12, da.Fat12Size) != 0)
         goto bugout;
      da.FatSize = da.Fat12Size/1.5*2;
      if ((da.Fat = (uint8_t *)malloc(da.FatSize)) == NULL)
         goto bugout; 
      ConvertFat12to16(da.Fat, da.Fat12, da.Fat12Size/1.5);
   }
   else
   {
      da.FatSize = fatsize;
      if ((da.Fat16 = (uint8_t *)malloc(da.FatSize)) == NULL)
         goto bugout; 
      if (readsect(da.FatStartSector, bpb.SectorsPerFat, da.Fat16, da.FatSize) != 0)
         goto bugout;
      if ((da.Fat = (uint8_t *)malloc(da.FatSize)) == NULL)
         goto bugout; 
      memcpy(da.Fat, da.Fat16, da.FatSize);
   }

   if (verbose > 0)
   {
      fprintf(stderr, "FAT start sector=%d\n", da.FatStartSector);
      fprintf(stderr, "root start sector=%d\n", da.RootDirStartSector);
      fprintf(stderr, "root number of sectors=%d\n", da.RootDirNumSectors);
      fprintf(stderr, "data start sector=%d\n", da.DataStartSector);
   }

   /* Check for write protected disk. Try read/write to last sector in root directory. */
   da.WriteProtect = 1;
   if (readsect(da.RootDirStartSector+da.RootDirNumSectors-1, 1, dummy, sizeof(dummy)) == 0)
      if (writesect(da.RootDirStartSector+da.RootDirNumSectors-1, 1, dummy, sizeof(dummy)) == 0)
         da.WriteProtect = 0;

   stat = 0;

bugout:
   if (stat != 0)
   {
      if (da.Fat != NULL)
         free(da.Fat);
      if (da.Fat12 != NULL)
         free(da.Fat12);
      if (da.Fat16 != NULL)
         free(da.Fat16);
   }
   return stat;
}

int FatFreeSpace(void)
{
   return(FindFreeClusters() * bpb.SectorsPerCluster * FAT_HARDSECT);
}

int FatDiskAttributes( PHOTO_CARD_ATTRIBUTES * pa )
{
    strncpy( pa->OEMID, bpb.OEMID, 8 );
    pa->BytesPerSector = bpb.BytesPerSector;
    pa->SectorsPerCluster = bpb.SectorsPerCluster;
    pa->ReservedSectors = bpb.ReservedSectors;
    pa->SectorsPerFat = bpb.SectorsPerFat;
    pa->RootEntries = bpb.RootEntries;
    strncpy( pa->SystemID, (char *)bpb.SystemID, 8 );
    strncpy( pa->VolumeLabel, (char *)bpb.VolumeLabel, 11 );
    pa->WriteProtect = da.WriteProtect;
    
    return 0;
}

/*  Prints out all entries in the current directory to stdout. */
int FatListDir(void) 
{
   int ret, filenum;
   int freespace;

   if (verbose > 0)
   {
      freespace = FatFreeSpace();
      fprintf(stdout, "Free Space=%d bytes\n", freespace);
   }

   filenum = 0;
   while (1) 
   {
      ret = LoadFileInCWD(filenum);
      if (ret == FAT_END_OF_DIR) 
      {
         fputs("<EOD>\n", stdout);
         break;
      }
      if ((ret != FAT_FILE_DELETED) && (ret != FAT_LONG_FILENAME))
         PrintCurrFileInfo();
      filenum++;
   }
   return 0;
}

/*   Directory List "Iterator": Begin,  Next... */
static int fatdir_filenum = 0;

int FatDirBegin(FILE_ATTRIBUTES *a)
{
    fatdir_filenum = 0;
    return FatDirNext( a );
}

int FatDirNext(FILE_ATTRIBUTES *a)
{
    int ret;
    ret = LoadFileInCWD( fatdir_filenum );

    if( ret == FAT_END_OF_DIR )
        return 0;
    
    if ((ret != FAT_FILE_DELETED) && (ret != FAT_LONG_FILENAME))
    {
        strcpy( a->Name, fa.Name );
        
        if( fa.Attr == FAT_IS_DIR )
            a->Attr = 'd';
        else
            a->Attr = ' ';
        
        a->Size = fa.Size;
    }
    else
    {
        strcpy( a->Name, "" );
        a->Attr = 'x';
        a->Size = 0;    
    }
    
    fatdir_filenum++;
    return 1;
}

/* Dump FAT file to the output file (fd). */
int FatReadFile(char *name, int fd)
{
   uint8_t *buf=NULL;
   int cluster, sector, old;
   int block = bpb.SectorsPerCluster * FAT_HARDSECT;
   int i, n, total = 0;

   if (LoadFileWithName(name) != 0)
      goto bugout;   /* file not found */

   cluster = fa.StartCluster;
   sector = ConvertClusterToSector(cluster);

   if ((buf = malloc(block)) == NULL)
   {
       goto bugout;
   }

   for (i=0; i<fa.Size; i+=n)
   {
      if (readsect(sector, bpb.SectorsPerCluster, buf, block) != 0)   /* read full cluster */
      {
         total = -1;
         goto bugout;
      }

      n = fa.Size-i > block ? block : fa.Size-i;
      write(fd, buf, n);
      
      total += n;
      old = cluster;
      cluster = GetNextCluster(cluster);
      if (cluster >= 0xfff7 || cluster == 0)
         break;

      sector = ConvertClusterToSector(cluster);
   }

bugout:
   if (buf != NULL)
      free(buf);

   return total;
}

/* Dump FAT file, given the "offset" in bytes and the "len" in bytes, to the output buffer. */
int FatReadFileExt(char *name, int offset, int len, void *outbuf)
{
   uint8_t *buf=NULL;
   int cluster, sector, old;
   int block = bpb.SectorsPerCluster * FAT_HARDSECT;  /* cluster size in bytes */
   int i, n, total = 0, btotal = 0;
   int bn, boff, blen; 
   int b1 = offset / block;   /* first cluster to read */ 
   int b2 = (offset+len) / block;   /* last cluster to read */

   if (LoadFileWithName(name) != 0)
      goto bugout;   /* file not found */

   cluster = fa.StartCluster;
   sector = ConvertClusterToSector(cluster);

   if ((buf = malloc(block)) == NULL)
   {
       goto bugout;
   }

   for (i=0, bn=0; i<fa.Size; i+=n, bn++)
   {
      /* Determine size in bytes to write for this cluster */
      n = fa.Size-i > block ? block : fa.Size-i;  

      /* Read/write data if it falls within "offset" and "len". */ 
      if (bn >= b1)
      {
         if (readsect(sector, bpb.SectorsPerCluster, buf, block) != 0)   /* read full cluster */
         {
            total = -1;
            goto bugout;
         }

         if (bn == b1)
            boff = offset-total;    /* cluster overlaps "offset" */
         else
            boff = 0;         /* cluster is past "offset" */

         if (bn <= b2)
         {
            if (bn == b2)
               blen = (offset+len)-total-boff;    /* cluster overlaps "len" */
            else
               blen = n-boff;         /* cluster is within "len" */

            memcpy(outbuf+btotal, buf+boff,  blen);
            btotal += blen;
         }       
         else
            break; /* done writing data */  
      }

      total += n;
      old = cluster;
      cluster = GetNextCluster(cluster);
      if (cluster >= 0xfff7 || cluster == 0)
         break;

      sector = ConvertClusterToSector(cluster);
   }

bugout:
   if (buf != NULL)
      free(buf);

   return btotal;
}

/* Make dir current working directory. */
int FatSetCWD(char *dir) 
{
   int ret;

   if (dir[0] == '.')
      return 0;

   if (dir[0] == '/')
   {
      RootSetCWD();
      return 0;
   }

   if (strcmp(cwd.Name, dir) == 0)
      return 0;
 
   ret = LoadFileWithName(dir);
   if (ret != 0)
      return ret;

   if (!(fa.Attr & FAT_IS_DIR))
      return 1;

   strncpy(cwd.Name, fa.Name, sizeof(cwd.Name));
   cwd.StartSector = ConvertClusterToSector(fa.StartCluster);
   cwd.CurrSector = cwd.StartSector;
   cwd.StartCluster = fa.StartCluster;
   return 0;
}

int FatDeleteFile(char *name) 
{
   uint8_t buf[FAT_HARDSECT];
   uint16_t *pfat = (uint16_t *)da.Fat;
   int cluster, next_cluster, filenum, stat=1;

   if (LoadFileWithName(name) != 0)
      goto bugout;

   cluster = fa.StartCluster;

   /* Free all fat cluster entries for specified file. */
   while ((cluster <= 0xFFF8) && (cluster != 0x0000)) 
   {
      next_cluster = *(pfat+cluster);   /* get next cluster */
      *(pfat+cluster) = 0;         /* free current cluster */
      cluster = next_cluster;
   }

   /* Remove directory entry for specified file. */
   readsect(fa.DirSectorNum, 1, buf, sizeof(buf));
   filenum = fa.DirEntryNum & 0x000F;
   filenum <<= 4;
   buf[filenum * 2] = FAT_FILE_DELETED;
   if (writesect(fa.DirSectorNum, 1, buf, sizeof(buf)) != 0)
      goto bugout;

   /* Write updated fat to disk. */
   if (UpdateFat() != 0)
      goto bugout;

   stat = 0;

bugout:
   return stat;
}




