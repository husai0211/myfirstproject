#ifndef __BDIDLL_H__
#define __BDIDLL_H__
/*************************************************************************
|  COPYRIGHT (c) 2000 BY ABATRON AG
|*************************************************************************
|
|  PROJECT NAME: BDI Communication Driver
|  FILENAME    : bdilnk.h
|
|  COMPILER    : GCC
|
|  TARGET OS   : LINUX
|  TARGET HW   : PC
|
|  PROGRAMMER  : Abatron / RD
|  CREATION    : 27.03.00
|
|*************************************************************************
|
|  DESCRIPTION :
|  Data link functions for communication with BDI
|
|
|*************************************************************************
|
|
|  UPDATES     :
|
|  DATE      NAME  CHANGES
|  -----------------------------------------------------------
|  Latest update
|  ...
|  13.13.97  aba   Bla bla ...
|  ...
|  First update
|
|*************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************
|  DEFINES
|*************************************************************************/


#define INADDR_NONE             0xffffffff

/*************************************************************************
|  TYPEDEFS
|*************************************************************************/

#define FALSE           0
#define TRUE            1

typedef unsigned char   CHAR;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
typedef int             BOOL;


/*************************************************************************
|  FUNCTIONS
|*************************************************************************/

DWORD BDI_IPAddrIntel(const char* ipAddress);
DWORD BDI_IPAddrMotorola(const char* ipAddress);

void BDI_DoDelay(DWORD delay);
int  BDI_Open(const char* port, DWORD baudrate);
void BDI_Close(void);

int  BDI_Transaction(      int       commandLength,
                     const void     *commandData,
                           int       answerSize,
                           void     *answerData,
                           DWORD     commandTime);


#ifdef __cplusplus
}
#endif

#endif

