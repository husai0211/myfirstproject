#ifndef __BDICNF_H__
#define __BDICNF_H__
/*************************************************************************
|  COPYRIGHT (c) 2000 BY ABATRON AG
|*************************************************************************
|
|  PROJECT NAME: BDI Configuration Utility
|  FILENAME    : bdicnf.h
|
|  COMPILER    : GCC
|
|  TARGET OS   : LINUX
|  TARGET HW   : PC
|
|  PROGRAMMER  : Abatron / RD
|  CREATION    : 19.12.00
|
|*************************************************************************
|
|  DESCRIPTION :
|  Helper functions to build the BDI configuration structure
|
|
|*************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************
|  DEFINES
|*************************************************************************/

#define BDI_MAX_CONFIG_SIZE     0x10000
#define BDI_MAX_REGDEF_SIZE     0x10000

/* BDI type */
#define BDI_TYPE_HS             0
#define BDI_TYPE_20             1      /* BDI2000       */
#define BDI_TYPE_21             2      /* BDI2000 Rev.C */
#define BDI_TYPE_10             3      /* BDI1000       */
#define BDI_TYPE_30             4      /* BDI3000       */
#define BDI_TYPE_LAST           4

/*************************************************************************
|  TYPEDEFS
|*************************************************************************/

/*************************************************************************
|  FUNCTIONS
|*************************************************************************/

int CNF_BuildRomConfig(const char* szFileName, BYTE* data);
int CNF_BuildRomRegdef(const char* szFileName, BYTE* config, BYTE* regdef);

#ifdef __cplusplus
}
#endif

#endif

