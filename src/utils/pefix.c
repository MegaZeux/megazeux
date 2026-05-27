/**
 * This tool modifies standard Microsoft PE32 and PE32+ (Portable Executable)
 * files on the i386 and AMD64 platforms, such that two identical programs
 * compiled with identical compilers produce an identical binary. Generally,
 * PE binaries are not identical, because TimeDateStamp (which changes
 * continuously) is used in several mandatory executable sections.
 *
 * Apart from the main PE32 header, it is also necessary to modify the
 * optional .edata, .idata and .rsrc sections, since these sections' headers
 * (and in the case of .rsrc, contents) also contain TimeDateStamps.
 *
 * This program additionally works around a binutils bug which generates
 * random garbage between module names in the idata section. Normally,
 * this wouldn't affect program execution, but it does make the binaries
 * non-identical. Replacing this garbage with zeroes solves the problem.
 *
 * Finally, we take the opportunity to tweak DllCharacteristics so that
 * ASLR is enabled on newer Windows operating systems.
 *
 * Copyright (C) 2009-2010 Alistair John Strachan <alistair@devzero.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define SECTION_NAME_LEN 8

typedef enum
{
  SUCCESS,
  OPEN_ERROR,
  READ_ERROR,
  WRITE_ERROR,
  INVALID_MAGIC,
} error_t;

static const char *err_str(error_t err)
{
  switch(err)
  {
    case SUCCESS:
      return "Success";
    case OPEN_ERROR:
      return "Failed to open file";
    case READ_ERROR:
      return "Failure while reading from file";
    case WRITE_ERROR:
      return "Failure while writing to file";
    case INVALID_MAGIC:
      return "File magic was unrecognized or invalid";
    default:
      return "Unknown error";
  }
}

static long ftell_and_rewind(FILE *f)
{
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  return size;
}

static error_t pe_csum(FILE *f, uint32_t *csum)
{
  uint32_t sum = 0;
  long count, len;
  uint16_t w;

  len = ftell_and_rewind(f);

  for(count = len; count > 1; count -= 2)
  {
    if(fread(&w, sizeof(uint16_t), 1, f) != 1)
      return READ_ERROR;
    sum += w;
  }

  if(count == 1)
  {
    w = 0;
    if(fread(&w, sizeof(uint8_t), 1, f) != 1)
      return READ_ERROR;
    sum += w;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);

  *csum = (uint32_t)((uint16_t)sum + len);
  return SUCCESS;
}

static error_t process_rsrc(FILE *f, uint32_t rsrc_offset)
{
  uint16_t num_name, num_id, i;
  error_t err = SUCCESS;
  uint32_t ul, *dirs;

  // Skip Characteristics
  if(fseek(f, 4, SEEK_CUR))
    return READ_ERROR;

  // Re-write TimeDateStamp as zero
  ul = 0;
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    return WRITE_ERROR;

  // Skip MajorVersion, MinorVersion
  if(fseek(f, 2 + 2, SEEK_CUR))
    return READ_ERROR;

  // Read NumberOfNamedEntries
  if(fread(&num_name, sizeof(uint16_t), 1, f) != 1)
    return READ_ERROR;

  // Read NumberOfIdEntries
  if(fread(&num_id, sizeof(uint16_t), 1, f) != 1)
    return READ_ERROR;

  dirs = malloc(sizeof(uint32_t) * (num_name + num_id));

  for(i = 0; i < num_name + num_id; i++)
  {
    // Skip Name (never used)
    if(fseek(f, 4, SEEK_CUR))
    {
      err = READ_ERROR;
      goto err_free_dirs;
    }

    // Read OffsetToData
    if(fread(&dirs[i], sizeof(uint32_t), 1, f) != 1)
    {
      err = READ_ERROR;
      goto err_free_dirs;
    }
  }

  for(i = 0; i < num_name + num_id; i++)
  {
    // Data is interesting because if the high bit is set on this,
    // we need to read a subdirectory (recursively).

    if(!(dirs[i] & 0x80000000))
      continue;
    dirs[i] &= ~0x80000000;

    // Skip to the specified .rsrc subdirectory
    if(fseek(f, rsrc_offset + dirs[i], SEEK_SET))
    {
      err = READ_ERROR;
      goto err_free_dirs;
    }

    // Process the .rsrc subdirectory
    err = process_rsrc(f, rsrc_offset);
    if(err != SUCCESS)
      goto err_free_dirs;
  }

err_free_dirs:
  free(dirs);
  return err;
}

static error_t walk_rvas(FILE *f, uint16_t cpu_type,
 uint32_t virtaddr, uint32_t offset)
{
  uint32_t rva32, pad, phys, last_phys = 0;
  unsigned int i;
  uint64_t rva;
  long pos;

  while(1)
  {
    // RVAs are real virtual addresses, not just relative ones,
    // so reading them is machine specific
    if(cpu_type == 0x014c)
    {
      if(fread(&rva32, sizeof(uint32_t), 1, f) != 1)
        return READ_ERROR;
      rva = rva32;
    }
    else if(cpu_type == 0x8664)
    {
      if(fread(&rva, sizeof(uint64_t), 1, f) != 1)
        return READ_ERROR;
    }

    // Zero RVA means "stop reading"
    if(!rva)
      break;

    // Convert to file offset (physaddr)
    phys = rva - virtaddr + offset;

    // We need to have processed at least one phys
    // before pad can be computed.

    if(last_phys)
    {
      // Store current offset
      pos = ftell(f);

      // Seek to import name
      if(fseek(f, last_phys, SEEK_SET))
        return READ_ERROR;

      // Compute string length for this RVA function
      for(i = 0; ; i++)
      {
        int b = fgetc(f);
        if(b < 0)
          return READ_ERROR;

        if(i >= 2 && !b)
          break;
      }

      // Win32 I/O needs this between interleaved read/write
      if(fseek(f, ftell(f), SEEK_SET))
        return READ_ERROR;

      // Compute and rewrite pad as zero
      pad = phys - last_phys - (i + 1);
      for(i = 0; i < pad; i++)
        if(fputc(0, f) < 0)
          return WRITE_ERROR;

      // Seek back to where we were
      if(fseek(f, pos, SEEK_SET))
        return READ_ERROR;
    }

    last_phys = phys;
  }

  return SUCCESS;
}

static error_t modify_pe(FILE *f)
{
  uint32_t rsrc_offset = 0, edata_offset = 0, idata_offset = 0;
  uint16_t cpu_type, num_sections, dll_characteristics;
  char section_name[SECTION_NAME_LEN + 1];
  uint32_t ul, idata_virtaddr = 0;
  error_t err = SUCCESS;
  long csum_offset;
  int skip = 0;

  // Check 'MZ' magic
  if(fgetc(f) != 'M')
    return INVALID_MAGIC;
  if(fgetc(f) != 'Z')
    return INVALID_MAGIC;

  // Next 58 bytes of DOS header aren't interesting
  if(fseek(f, 58, SEEK_CUR))
    return READ_ERROR;

  // Read so called 'lfanew' field; offset to PE header
  if(fread(&ul, sizeof(uint32_t), 1, f) != 1)
    return READ_ERROR;

  // Skip to lfanew offset (move us to PE header)
  if(fseek(f, ul, SEEK_SET))
    return READ_ERROR;

  // Check 'PE\x00\x00' magic
  if(fgetc(f) != 'P')
    return INVALID_MAGIC;
  if(fgetc(f) != 'E')
    return INVALID_MAGIC;
  if(fgetc(f) != 0)
    return INVALID_MAGIC;
  if(fgetc(f) != 0)
    return INVALID_MAGIC;

  // Read in Machine (we support limited CPU types)
  if(fread(&cpu_type, sizeof(uint16_t), 1, f) != 1)
    return READ_ERROR;

  // Can only support i386 and AMD64 binaries
  if(cpu_type != 0x014c && cpu_type != 0x8664)
    return INVALID_MAGIC;

  // Read NumberOfSections (used below)
  if(fread(&num_sections, sizeof(uint16_t), 1, f) != 1)
    return READ_ERROR;

  // Win32 I/O needs this between interleaved read/write
  if(fseek(f, ftell(f), SEEK_SET))
    return READ_ERROR;

  // The checksum must be rewritten as a constant value. However,
  // the logical choice of zero does not work correctly, as this
  // exposes a Windows bug which leaves the EXE open after it has
  // loaded, breaking the updater. Use the 2038 timestamp instead.
  ul = 0x7fffffff;
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    return WRITE_ERROR;

  // Skip PointerToSymbolTable, NumberOfSymbols, SizeOfOptionalHeader
  // Characteristics
  if(fseek(f, 4 + 4 + 2 + 2, SEEK_CUR))
    return READ_ERROR;

  // Skip Magic, MajorLinkerVersion, MinorLinkerVersion, SizeOfCode,
  // SizeOfInitializedData, SizeOfUninitializedData, AddressOfEntryPoint,
  // BaseOfCode, BaseOfData [i386], ImageBase*, SectionAlignment,
  // FileAlignment, MajorOperatingSystemVersion, MinorOperatingSystemVersion,
  // MajorImageVersion, MinorImageVersion, MajorSubsystemVersion,
  // MinorSubsystemVersion, Reserved1, SizeOfImage, SizeOfHeaders
  if(cpu_type == 0x014c)
    skip = 2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4 +
           4 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4;
  else if(cpu_type == 0x8664)
    skip = 2 + 1 + 1 + 4 + 4 + 4 + 4 + 4 +
           8 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4;
  if(fseek(f, skip, SEEK_CUR))
    return READ_ERROR;

  // Squash obsolete Checksum with zero
  ul = 0;
  csum_offset = ftell(f);
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    return WRITE_ERROR;

  // Skip Subsystem
  if(fseek(f, 2, SEEK_CUR))
    return READ_ERROR;

  // Read DllCharacteristics
  if(fread(&dll_characteristics, sizeof(uint16_t), 1, f) != 1)
    return READ_ERROR;

  // Switch on DYNAMIC_BASE and NX_COMPAT
  dll_characteristics |= (0x40 | 0x100);

  // Roll back and re-write DllCharacteristics
  if(fseek(f, -2, SEEK_CUR))
    return READ_ERROR;
  if(fwrite(&dll_characteristics, sizeof(uint16_t), 1, f) != 1)
    return WRITE_ERROR;

  // Skip SizeOfStackReserve*, SizeOfStackCommit*, SizeOfHeapReserve*,
  // SizeOfHeapCommit*, LoaderFlags, NumberOfRvaAndSizes,
  // DataDirectory (16*(4+4))
  if(cpu_type == 0x014c)
    skip = 4 + 4 + 4 + 4 + 4 + 4 + 16 * (4 + 4);
  else if(cpu_type == 0x8664)
    skip = 8 + 8 + 8 + 8 + 4 + 4 + 16 * (4 + 4);
  if(fseek(f, skip, SEEK_CUR))
    return READ_ERROR;

  // PE Section Header processing (find .rsrc if present)
  section_name[SECTION_NAME_LEN] = 0;
  while(num_sections-- > 0)
  {
    uint32_t *offset = NULL;

    // Read the Name
    if(fread(&section_name, SECTION_NAME_LEN, 1, f) != 1)
      return READ_ERROR;

    // Skip Misc
    if(fseek(f, 4, SEEK_CUR))
      return READ_ERROR;

    // Read VirtualAddress (only needed for .idata)
    if(fread(&ul, sizeof(uint32_t), 1, f) != 1)
      return READ_ERROR;

    // Skip SizeOfRawData
    if(fseek(f, 4, SEEK_CUR))
      return READ_ERROR;

    // This isn't .rsrc so we skip PointerToRawData
    if(strcmp(section_name, ".rsrc") == 0)
      offset = &rsrc_offset;
    else if(strcmp(section_name, ".edata") == 0)
      offset = &edata_offset;
    else if(strcmp(section_name, ".idata") == 0)
    {
      offset = &idata_offset;
      idata_virtaddr = ul;
    }

    if(offset)
    {
      // Read the .rsrc PointerToRawData for this section
      if(fread(offset, sizeof(uint32_t), 1, f) != 1)
        return READ_ERROR;
    }
    else
    {
      // Skip the PointerToRawData for this section
      if(fseek(f, 4, SEEK_CUR))
        return READ_ERROR;
    }

    // Skip PointerToRelocations, PointerToLinenumbers,
    // NumberOfRelocations, NumberOfLinenumbers, Characteristics
    if(fseek(f, 4 + 4 + 2 + 2 + 4, SEEK_CUR))
      return READ_ERROR;
  }

  // PE .rsrc section handling
  if(rsrc_offset != 0)
  {
    // Reposition file pointer at (.rsrc)
    if(fseek(f, rsrc_offset, SEEK_SET))
      return READ_ERROR;

    err = process_rsrc(f, rsrc_offset);
    if(err != SUCCESS)
      return err;
  }

  // PE .edata section handling
  if(edata_offset != 0)
  {
    // Reposition file pointer at (.edata+4), Skip Characteristics
    if(fseek(f, edata_offset + 4, SEEK_SET))
      return READ_ERROR;

    // Re-write TimeDateStamp as zero (new Checksum computed later)
    ul = 0;
    if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
      return WRITE_ERROR;
  }

  if(idata_offset != 0)
  {
    // Reposition file pointer at (.idata)
    if(fseek(f, idata_offset, SEEK_SET))
      return READ_ERROR;

    while(1)
    {
      uint32_t fnlist;
      long offset;

      // dwRVAFunctionNameList
      if(fread(&fnlist, sizeof(uint32_t), 1, f) != 1)
        return READ_ERROR;

      // Skip dwUseless1, dwUseless2, dwRVAModuleName, dwRVAFunctionAddressList
      if(fseek(f, 4 + 4 + 4 + 4, SEEK_CUR))
        return READ_ERROR;

      // Zero dwRVAFunctionNameList means there are no more modules
      if(!fnlist)
        break;

      fnlist = fnlist - idata_virtaddr + idata_offset;

      // Store current file offset
      offset = ftell(f);

      // Seek to fnlist RVAs
      if(fseek(f, fnlist, SEEK_SET))
        return READ_ERROR;

      // Walk fnlist RVAs
      err = walk_rvas(f, cpu_type, idata_virtaddr, idata_offset);
      if(err != SUCCESS)
        return err;

      // Move back to original position
      if(fseek(f, offset, SEEK_SET))
        return READ_ERROR;
    }
  }

  // Compute and write back valid checksum
  err = pe_csum(f, &ul);
  if(err != SUCCESS)
    return err;
  if(fseek(f, csum_offset, SEEK_SET))
    return READ_ERROR;
  if(fwrite(&ul, sizeof(uint32_t), 1, f) != 1)
    return WRITE_ERROR;

  return SUCCESS;
}

int main(int argc, char *argv[])
{
  error_t err = SUCCESS;
  FILE *f;

  if(argc != 2)
  {
    fprintf(stderr, "usage: %s [exe]\n", argv[0]);
    return 0;
  }

  f = fopen(argv[1], "r+b");
  if(f)
  {
    err = modify_pe(f);
    fclose(f);
  }
  else
    err = OPEN_ERROR;

  if(err != SUCCESS)
    fprintf(stderr, "Failed: %s\n", err_str(err));

  return err;
}
