/*************************************************************************
|  COPYRIGHT (c) 2000 BY ABATRON AG
|*************************************************************************
|
|  PROJECT NAME: BDI Configuration Utility
|  FILENAME    : bdicnf.c
|
|  COMPILER    : GCC
|
|  TARGET OS   : LINUX / UNIX
|  TARGET HW   : PC
|
|*************************************************************************
|
|  DESCRIPTION :
|  This module builds the configuration stored into the BDI flash memory.
|
|*************************************************************************/

/*************************************************************************
|  INCLUDES
|*************************************************************************/

#if defined(WIN32)
#include <windows.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "bdierror.h"
#include "bdidll.h"
#include "bdicnf.h"

/*************************************************************************
|  DEFINES
|*************************************************************************/

#define MAX_LINE_LEN    256
#define BDI_FW_COUNT    40


/* ASCII codes */
#define HT              9
#define CR              13
#define LF              10


/*************************************************************************
|  MACROS
|*************************************************************************/

/*************************************************************************
|  TYPEDEFS
|*************************************************************************/

/*************************************************************************
|  TABLES
|*************************************************************************/

/*************************************************************************
|  LOCALS
|*************************************************************************/

/****************************************************************************
 ****************************************************************************

    BDI_Append___ :

    Host independent helper function to append a numeric value to a buffer.
    The bytes will be stored in the link format order (Motorola byte order).

     INPUT  : value     value
              buffer    pointer to buffer
     OUTPUT : RETURN    pointer to next byte after the stored value

 ****************************************************************************/

static BYTE* BDI_AppendByte(BYTE  value, BYTE* buffer)
{
  *buffer++ = value;
  return buffer;
} /* BDI_AppendByte */

static BYTE* BDI_AppendWord(WORD value, BYTE* buffer)
{
  *buffer++ = (BYTE)(value>>8);
  *buffer++ = (BYTE)value;
  return buffer;
} /* BDI_AppendWord */

static BYTE* BDI_AppendLong(DWORD value, BYTE* buffer)
{
  *buffer++ = (BYTE)(value>>24);
  *buffer++ = (BYTE)(value>>16);
  *buffer++ = (BYTE)(value>>8);
  *buffer++ = (BYTE)value;
  return buffer;
} /* BDI_AppendLong */


/****************************************************************************
 ****************************************************************************

    BDI_Extract___ :

    Host independent helper function to extract a numeric value from a buffer.
    The value must be stored in link format (Motorola byte order).

     INPUT  : buffer    pointer to buffer
     OUTPUT : value     pointer to variable where to store the value
              RETURN    pointer to next byte after the extracted value
     INOUT  :

 ****************************************************************************/

static BYTE* BDI_ExtractCHAR (char* value, BYTE* buffer)
{
  *value = (char)*buffer++;
  return buffer;
} /* BDI_ExtractCHAR */

static BYTE* BDI_ExtractByte(BYTE* value, BYTE* buffer)
{
  *value = *buffer++;
  return buffer;
} /* BDI_ExtractByte */

static BYTE* BDI_ExtractWord(WORD* value, BYTE* buffer)
{
  WORD x;
  x = (WORD)*buffer++;
  x = (x<<8) + (WORD)*buffer++;
  *value = x;
  return buffer;
} /* BDI_ExtractWord */

static BYTE* BDI_ExtractLong(DWORD* value, BYTE* buffer)
{
  DWORD x;
  x = (DWORD)*buffer++;
  x = (x<<8) + (DWORD)*buffer++;
  x = (x<<8) + (DWORD)*buffer++;
  x = (x<<8) + (DWORD)*buffer++;
  *value = x;
  return buffer;
} /* BDI_ExtractLong */

static char* BDI_ExtractString (WORD size, char* string, char* getPtr)
{
  /* skip spaces */
  while (*getPtr == ' ') getPtr++;

  /* copy string */
  if (*getPtr == '"') {
    getPtr++;
    while ((*getPtr != '"') && (*getPtr != 0)) {
      if (size > 1) {
        *string++ = *getPtr;
        size--;
      } /* if */
      getPtr++;
    } /* while */
    if (*getPtr != 0) getPtr++;
  } /* if */
  else {
    while ((*getPtr != ' ') && (*getPtr != ';') && (*getPtr != 0)) {
      if (size > 1) {
        *string++ = *getPtr;
        size--;
      } /* if */
      getPtr++;
    } /* while */
  } /* else */

  *string = 0;
  return getPtr;
} /* BDI_ExtractString */

static BYTE* BDI_ExtractLine(WORD count, char* line, BYTE* buffer)
{
  BYTE  rxChar;
  char* putPtr;

  /* skip leading control chars and spaces */
  rxChar = *buffer;
  while (rxChar <= ' ') {
    buffer++;
    rxChar = *buffer;
  } /* while */

  /* read next line */
  putPtr = line;
  while ((rxChar <= 0x7f) && (rxChar != CR) && (rxChar != LF) && (count > 0)) {
    if (rxChar == HT) rxChar = ' ';
    *putPtr++ = (char)rxChar;
    buffer++;
    rxChar = *buffer;
    count--;
  } /* while */

  *putPtr = 0;  /* terminating zero */
  return buffer;
} /* BDI_ExtractLine */


/****************************************************************************
 ****************************************************************************
    Build the configuration data from the configuration file

    INPUT:  szFileName  the configuration file name
    OUTPUT: data        the data to program into the BDI flash
            return      number of bytes to program

 ****************************************************************************/

/* configuration parts */
#define CNF_PART_NONE           0
#define CNF_PART_INIT           1
#define CNF_PART_TARGET         2
#define CNF_PART_HOST           3
#define CNF_PART_FLASH          4
#define CNF_PART_REGS           5

static void BuildFileName(char* s, const char* p, const char* n)
{
  char* ptr;

  (void)strcpy(s, p);
  ptr = s + strlen(s);
  while (ptr > s) {
    if ((*ptr == '/') || (*ptr == '\\')) {
      ptr++;
      break;
    } /* if */
    ptr--;
  } /* while */
  *ptr = 0;
  (void)strcat(s, n);
} /* BuildFileName */


int CNF_BuildRomConfig(const char* szFileName, BYTE* data)
{
  int     count;
  FILE*   configFile;

  /* open configuration file */
  configFile = fopen(szFileName, "rb");
  if (configFile == NULL)  return BDI_ERR_FILE_ACCESS;
  count = (int)fread(data, 1, BDI_MAX_CONFIG_SIZE, configFile);
  fclose(configFile);
  return count;
} /* BDI_BuildRomConfig */


int CNF_BuildRomRegdef(const char* szFileName, BYTE* config, BYTE* regdef)
{
  size_t  count;
  BYTE*   pConfig;
  BYTE*   pRegdef;
  char*   pLine;
  char    line[256];
  char    string[256];
  char    regdefName[256];
  int     configPart;
  int     core;
  FILE*   regdefFile;

  pConfig = config;
  pRegdef = regdef;
  configPart = CNF_PART_NONE;
  do {
    pConfig = BDI_ExtractLine(sizeof line, line, pConfig);
    if (line[0] != 0) {
      pLine = line;
      pLine = BDI_ExtractString(sizeof string, string, pLine);

      /* process part line */
      if (string[0] == '[') {
        if (strcmp(string, "[REGS]") == 0) {
          configPart = CNF_PART_REGS;
        } /* else if */
        else {
          configPart = CNF_PART_NONE;
        } /* else */
      } /* if */

      /* process config line */
      else if ((string[0] != 0) && (configPart == CNF_PART_REGS)) {
        core = 0;
        pLine = line;
        if (*pLine == '#') {
          pLine++;
          pLine = BDI_ExtractString(sizeof string, string, pLine);
          core = atoi(string);
        } /* if */
        pLine = BDI_ExtractString(sizeof string, string, pLine);
        if (strcmp(string, "FILE") == 0) {
          pLine = BDI_ExtractString(sizeof string, string, pLine);
          if (string[0] == '$') {
            BuildFileName(regdefName, szFileName, string + 1);
          } /* if */
          else {
            (void)strcpy(regdefName, string);
          } /* else */
          regdefFile = fopen(regdefName,"rb");
          if (regdefFile == NULL) return BDI_ERR_FILE_ACCESS;
          *pRegdef++ = 0x80 | (core & 0x3f);
          count = fread(pRegdef, 1, BDI_MAX_REGDEF_SIZE - (pRegdef - regdef), regdefFile);
          fclose(regdefFile);
          pRegdef += count;
        } /* else if */
      } /* else if */

    } /* if */
  } while (*pConfig != 0xff);

  return (int)(pRegdef - regdef);
} /* BDI_BuildRomRegdef */

#if 0
/****************************************************************************
 ****************************************************************************

   main (for test only)

 ****************************************************************************/

int main(int argc, char *argv[ ])
{
  CNF_InitEntryT  initList[1024];
  BYTE            modeList[1024];
  int             count;
  int             i;

  count = CNF_GetInitList("eval7t.cfg", 2, 10, initList);
  printf("Init Count = %i\n", count);
  for (i = 0; i < count; i++) {
    printf("%2i 0x%08lx 0x%08lx\n", initList[i].cmd, initList[i].addr, initList[i].value);
  }

  count = CNF_GetModeList("eval7t.cfg", 2, 10, modeList);
  printf("Mode Count = %i\n", count);
  for (i = 0; i < count; i+=8) {
    printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
           modeList[i+0], modeList[i+1], modeList[i+2], modeList[i+3],
           modeList[i+4], modeList[i+5], modeList[i+6], modeList[i+7]);
  }

} /* main */
#endif
