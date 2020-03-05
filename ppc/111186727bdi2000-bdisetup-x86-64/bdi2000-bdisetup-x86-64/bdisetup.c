/*************************************************************************
|  COPYRIGHT (c) 2000 BY ABATRON AG
|*************************************************************************
|
|  PROJECT NAME: BDI Setup Utility
|  FILENAME    : bdisetup.c
|
|  COMPILER    : GCC
|
|  TARGET OS   : LINUX / UNIX
|  TARGET HW   : PC
|
|  PROGRAMMER  : Abatron / RD
|  CREATION    : 27.03.00
|
|*************************************************************************
|
|  DESCRIPTION :
|
|  BDI setup utility for bdiGDB, bdiADA, bdiWind and bdiAccess:
|  ============================================================
|
|  This command line utility can be used to update the firmware/logic
|  of the BDI. It also transfers the network configuration.
|  There are 4 tasks the utility can process:
|  - Reading the loaded firmware / logic version
|  - Erasing the firmware and logic
|  - Updating the firmware and logic
|  - Storing the network configuration parameters into the BDI flash
|  Every of this task is started by executing the setup utility with
|  different parameters. The first parameter always selects the task
|  to execute:
|
|  bdisetup { -v | -e | -u | -c } [additional parameters]
|
|       -v      Read version
|       -e      Erase firmware and logic
|       -u      Update firmware and/or logic
|       -c      Store network configuration
|
|  There are two common additional parameters which define the serial port
|  and the serial baudrate:
|
|       -pP     Port to use, replace P with the port to use e.g. /dev/ttyS0
|       -bB     Baudrate to use, replace B with 9, 19, 38, 57 or 115
|
|  Additional parameters for update (-u):
|
|       -aA     Application type, replace A with STD, GDB, ADA, TOR or ACC
|       -tT     Target type, replace T with CPU32,PPC400,PPC600,PPC700,MPC800,
|                 ARM,TRICORE,MCF,HC12,MCORE,MIPS,MIPS64,XSCALE
|       -dD     Replace D with the directory with the firmware/logic files
|
|  Additional parameters for network configuration (-c):
|
|       -iI     Replace I with the BDI IP address (format: xxx.xxx.xxx.xxx)
|       -hH     Replace H with the host IP address
|       -mM     Replace M with the subnet mask.
|               A subnet mask of 255.255.255.255 disables the gateway function
|       -gG     Replace G with the default gateway IP address
|       -fF     Replace F with the path and name of the configuration file
|
|  All parameters have default values. See function main(). You may adjust
|  this default values for your convenience.
|
|  Examples
|  ========
|
|  bdisetup -v -p/dev/ttyS0 -b57        Read current loaded versions
|
|  bdisetup -u -aGDB -tMPC800           Update BDI with newest firmware/logic
|                                       for MPC8xx targets and GNU debugger.
|
|  bdisetup -c \                        Set network configuration.
|  -i151.120.25.101 \
|  -h151.120.25.115 \
|  -fE:\cygnus\root\usr\demo\mpc8260\ppc750.cnf
|
|
|  Build the setup utility:
|  =======================
|
|  To build the setup utility use GCC as follows:
|
|  gcc bdisetup.c bdidll.c -o bdisetup
|
|*************************************************************************/

/*************************************************************************
|  INCLUDES
|*************************************************************************/


#if defined(WIN32)
#include <windows.h>
#include <io.h>
#else /* defined(WIN32) */
#include <sys/param.h>
#include <unistd.h>
#include <dirent.h>
#endif /* defined(WIN32) */
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "bdierror.h"
#include "bdicmd.h"
#include "bdidll.h"
#include "bdicnf.h"

/*************************************************************************
|  DEFINES
|*************************************************************************/

#if defined(WIN32)
#define MAXPATHLEN  _MAX_PATH
#endif

#define BDI_DEFAULT_EXEC_TIME    500 /* default BDI command execution time */
#define BDI_MAX_LOGIC_VERSION    999 /* the maximal logic version */
#define BDI_MAX_FW_VERSION       255 /* the maximal firmware version */

#define ISP20_NBR_OF_ROWS        134
#define ISP20_ROW_BITS           240
#define ISP20_UES_BITS           120
#define ISP20_UES_CHARS          15

#define ISP10_NBR_OF_ROWS        118
#define ISP10_ROW_BITS           160
#define ISP10_UES_BITS           80
#define ISP10_UES_CHARS          10

#define ISPHS_NBR_OF_ROWS        102
#define ISPHS_ROW_BITS           80
#define ISPHS_UES_BITS           40

/* Firmware update mode */
#define BDI_UPDATE_AUTO         0   /* update firmware/logic only if needed */
#define BDI_UPDATE_FIRMWARE     1   /* update firmware in any case */
#define BDI_UPDATE_LOGIC        2   /* update logic in any case */
#define BDI_UPDATE_ALL          3   /* update firmware and logic in any case */


/*************************************************************************
|  MACROS
|*************************************************************************/

/*************************************************************************
|  TYPEDEFS
|*************************************************************************/

typedef struct {
  WORD    firmwareType;
  WORD    logicType;
  char*   firmwareName;
  char*   logicName;
} BDI_SetupInfoT;

typedef struct {
  WORD    bdi;
  WORD    loader;
  WORD    firmware;
  WORD    logic;
  char    sn[8+1];
} BDI_VersionT;


/*************************************************************************
|  LOCALS
|*************************************************************************/

static const BDI_SetupInfoT BHS_SetupInfo[] =
{
/* 00 */ { 0x0000,  0000, "BDIHSFW",  "C32JEDHS" },
/* 01 */ { 0x1000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 02 */ { 0x2000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 03 */ { 0x3000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 04 */ { 0x4000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 05 */ { 0x5000,  1000, "BDIPPCFW", "PPCJEDHS" },
/* 06 */ { 0x6000,  5000, "BDIMCFFW", "MCFJEDHS" },
/* 07 */ { 0x7000,  3000, "BDIC12FW", "C12JEDHS" },
/* 08 */ { 0x8000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 09 */ { 0x9000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 10 */ { 0xA000,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 11 */ { 0xB000,  0000, "NOT_SUPP", "NOT_SUPP" },

/* 12 */ { 0xCC00,  0000, "NOT_SUPP", "NOT_SUPP" },
/* 13 */ { 0xCD00,  1000, "NOT_SUPP", "NOT_SUPP" },
/* 14 */ { 0xCE00,  8000, "NOT_SUPP", "NOT_SUPP" },
/* 15 */ { 0xCF00,  4000, "NOT_SUPP", "NOT_SUPP" },
/* 16 */ { 0xD000,  4000, "NOT_SUPP", "NOT_SUPP" },
/* 17 */ { 0xD100,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 18 */ { 0xD200,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 19 */ { 0xD300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 20 */ { 0xD400,  9000, "NOT_SUPP", "NOT_SUPP" },
/* 21 */ { 0xD500,  9000, "NOT_SUPP", "NOT_SUPP" },
/* 22 */ { 0xD600,  9000, "NOT_SUPP", "NOT_SUPP" },
/* 23 */ { 0xD700,  6000, "NOT_SUPP", "NOT_SUPP" },
/* 24 */ { 0xD800,  6000, "NOT_SUPP", "NOT_SUPP" },
/* 25 */ { 0xD900,  5000, "NOT_SUPP", "NOT_SUPP" },
/* 26 */ { 0xDA00,  5000, "NOT_SUPP", "NOT_SUPP" },
/* 27 */ { 0xDB00,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 28 */ { 0xDC00,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 29 */ { 0xDD00,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 30 */ { 0xDE00,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 31 */ { 0xDF00,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 32 */ { 0xE000,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 33 */ { 0xE100,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 34 */ { 0xE200,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 35 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 36 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 37 */ { 0xE200,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 38 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 39 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 40 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 41 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 42 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 43 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 44 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 45 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 46 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 47 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 48 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 49 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 50 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
};

static const BDI_SetupInfoT B20_SetupInfo[] =
{
/* 00 */ { 0x0000,  0000, "B20C32FW", "C32JED20" },
/* 01 */ { 0x1000,  0000, "B20C32AA", "C32JED20" },
/* 02 */ { 0x2000,  0000, "B20C32WR", "C32JED20" },
/* 03 */ { 0x3000,  1000, "B20PPCAA", "PPCJED20" },
/* 04 */ { 0x4000, 21000, "B20PPCWR", "PPCJWR20" },
/* 05 */ { 0x5000,  1000, "B20PPCFW", "PPCJED20" },
/* 06 */ { 0x6000,  5000, "B20MCFFW", "MCFJED20" },
/* 07 */ { 0x7000,  3000, "B20C12FW", "C12JED20" },
/* 08 */ { 0x8000,  4000, "B20MMCFW", "MMCJED20" },
/* 09 */ { 0x9000,  8000, "B20ARMWR", "ARMJED20" },
/* 10 */ { 0xA000,  8000, "B20ARMFW", "ARMJED20" },
/* 11 */ { 0xB000,  6000, "B20TRIFW", "TRIJED20" },

/* 12 */ { 0xCC00,  0000, "B20C32GD", "C32JED20" },
/* 13 */ { 0xCD00,  1000, "B20PPCGD", "PPCJED20" },
/* 14 */ { 0xCE00,  8000, "B20ARMGD", "ARMJED20" },
/* 15 */ { 0xCF00,  4000, "B20MMCGD", "MMCJED20" },
/* 16 */ { 0xD000,  4000, "B20MMCWR", "MMCJED20" },
/* 17 */ { 0xD100,  7000, "B20COPFW", "COPJED20" },
/* 18 */ { 0xD200,  7000, "B20COPWR", "COPJED20" },
/* 19 */ { 0xD300,  7000, "B20COPGD", "COPJED20" },
/* 20 */ { 0xD400,  9000, "B20PP4FW", "PP4JED20" },
/* 21 */ { 0xD500,  9000, "B20PP4WR", "PP4JED20" },
/* 22 */ { 0xD600,  9000, "B20PP4GD", "PP4JED20" },
/* 23 */ { 0xD700,  7000, "B20QP4GD", "COPJED20" },
/* 24 */ { 0xD800,  6000, "B20TRIGD", "TRIJED20" },
/* 25 */ { 0xD900,  7000, "B20QP4FW", "COPJED20" },
/* 26 */ { 0xDA00,  5000, "B20MCFGD", "MCFJED20" },
/* 27 */ { 0xDB00,  7000, "B20PWSFW", "COPJED20" },
/* 28 */ { 0xDC00,  7000, "B20PWSWR", "COPJED20" },
/* 29 */ { 0xDD00,  7000, "B20PWSGD", "COPJED20" },
/* 30 */ { 0xDE00, 10000, "B20R4KFW", "R4KJED20" },
/* 31 */ { 0xDF00, 10000, "B20XLSGD", "XLSJED20" },
/* 32 */ { 0xE000, 10000, "B20R4KGD", "R4KJED20" },
/* 33 */ { 0xE100, 11000, "B20XSCFW", "XSCJED20" },
/* 34 */ { 0xE200,  8000, "B20AV8FW", "ARMJED20" },
/* 35 */ { 0xE300, 11000, "B20XSCGD", "XSCJED20" },
/* 36 */ { 0xE400, 10000, "B20R5KGD", "R5KJED20" },
/* 37 */ { 0xE500,  7000, "B20PQ3FW", "COPJED20" },
/* 38 */ { 0xE600,  8000, "B20AV8GD", "ARMJED20" },
/* 39 */ { 0xE700,  7000, "B20PQ3GD", "COPJED20" },
/* 40 */ { 0xE800,  8000, "B20A11FW", "ARMJED20" },
/* 41 */ { 0xE900,  8000, "B20A11GD", "ARMJED20" },
/* 42 */ { 0xEA00, 10000, "B20R5KFW", "R5KJED20" },
/* 43 */ { 0xEB00, 12000, "B20P55FW", "P55JED20" },
/* 44 */ { 0xEC00, 12000, "B20P55GD", "P55JED20" },
/* 45 */ { 0xED00, 13000, "B20PA6FW", "PA6JED20" },
/* 46 */ { 0xEE00, 13000, "B20PA6GD", "PA6JED20" },
/* 47 */ { 0xEF00, 14000, "B20SWDFW", "SWDJED20" },
/* 48 */ { 0xF000, 14000, "B20SWDGD", "SWDJED20" },
/* 49 */ { 0xF100, 14000, "B20SV8FW", "SWDJED20" },
/* 50 */ { 0xF200, 14000, "B20SV8GD", "SWDJED20" },
};

static const BDI_SetupInfoT B21_SetupInfo[] =
{
/* 00 */ { 0x0000,  0000, "B20C32FW", "C32JED21" },
/* 01 */ { 0x1000,  0000, "B20C32AA", "C32JED21" },
/* 02 */ { 0x2000,  0000, "B20C32WR", "C32JED21" },
/* 03 */ { 0x3000,  1000, "B20PPCAA", "PPCJED21" },
/* 04 */ { 0x4000, 21000, "B20PPCWR", "PPCJWR21" },
/* 05 */ { 0x5000,  1000, "B20PPCFW", "PPCJED21" },
/* 06 */ { 0x6000,  5000, "B20MCFFW", "MCFJED21" },
/* 07 */ { 0x7000,  3000, "B20C12FW", "C12JED21" },
/* 08 */ { 0x8000,  4000, "B20MMCFW", "MMCJED21" },
/* 09 */ { 0x9000,  8000, "B20ARMWR", "ARMJED21" },
/* 10 */ { 0xA000,  8000, "B20ARMFW", "ARMJED21" },
/* 11 */ { 0xB000,  6000, "B20TRIFW", "TRIJED21" },

/* 12 */ { 0xCC00,  0000, "B20C32GD", "C32JED21" },
/* 13 */ { 0xCD00,  1000, "B20PPCGD", "PPCJED21" },
/* 14 */ { 0xCE00,  8000, "B20ARMGD", "ARMJED21" },
/* 15 */ { 0xCF00,  4000, "B20MMCGD", "MMCJED21" },
/* 16 */ { 0xD000,  4000, "B20MMCWR", "MMCJED21" },
/* 17 */ { 0xD100,  7000, "B20COPFW", "COPJED21" },
/* 18 */ { 0xD200,  7000, "B20COPWR", "COPJED21" },
/* 19 */ { 0xD300,  7000, "B20COPGD", "COPJED21" },
/* 20 */ { 0xD400,  9000, "B20PP4FW", "PP4JED21" },
/* 21 */ { 0xD500,  9000, "B20PP4WR", "PP4JED21" },
/* 22 */ { 0xD600,  9000, "B20PP4GD", "PP4JED21" },
/* 23 */ { 0xD700,  7000, "B20QP4GD", "COPJED21" },
/* 24 */ { 0xD800,  6000, "B20TRIGD", "TRIJED21" },
/* 25 */ { 0xD900,  7000, "B20QP4FW", "COPJED21" },
/* 26 */ { 0xDA00,  5000, "B20MCFGD", "MCFJED21" },
/* 27 */ { 0xDB00,  7000, "B20PWSFW", "COPJED21" },
/* 28 */ { 0xDC00,  7000, "B20PWSWR", "COPJED21" },
/* 29 */ { 0xDD00,  7000, "B20PWSGD", "COPJED21" },
/* 30 */ { 0xDE00, 10000, "B20R4KFW", "R4KJED21" },
/* 31 */ { 0xDF00, 10000, "B20XLSGD", "XLSJED21" },
/* 32 */ { 0xE000, 10000, "B20R4KGD", "R4KJED21" },
/* 33 */ { 0xE100, 11000, "B20XSCFW", "XSCJED21" },
/* 34 */ { 0xE200,  8000, "B20AV8FW", "ARMJED21" },
/* 35 */ { 0xE300, 11000, "B20XSCGD", "XSCJED21" },
/* 36 */ { 0xE400, 10000, "B20R5KGD", "R5KJED21" },
/* 37 */ { 0xE500,  7000, "B20PQ3FW", "COPJED21" },
/* 38 */ { 0xE600,  8000, "B20AV8GD", "ARMJED21" },
/* 39 */ { 0xE700,  7000, "B20PQ3GD", "COPJED21" },
/* 40 */ { 0xE800,  8000, "B20A11FW", "ARMJED21" },
/* 41 */ { 0xE900,  8000, "B20A11GD", "ARMJED21" },
/* 42 */ { 0xEA00, 10000, "B20R5KFW", "R5KJED21" },
/* 43 */ { 0xEB00, 12000, "B20P55FW", "P55JED21" },
/* 44 */ { 0xEC00, 12000, "B20P55GD", "P55JED21" },
/* 45 */ { 0xED00, 13000, "B20PA6FW", "PA6JED21" },
/* 46 */ { 0xEE00, 13000, "B20PA6GD", "PA6JED21" },
/* 47 */ { 0xEF00, 14000, "B20SWDFW", "SWDJED21" },
/* 48 */ { 0xF000, 14000, "B20SWDGD", "SWDJED21" },
/* 49 */ { 0xF100, 14000, "B20SV8FW", "SWDJED21" },
/* 50 */ { 0xF200, 14000, "B20SV8GD", "SWDJED21" },
};

static const BDI_SetupInfoT B10_SetupInfo[] =
{
/* 00 */ { 0x0000,  0000, "B10C32FW", "C32JED10" },
/* 01 */ { 0x1000,  0000, "B10C32AA", "C32JED10" },
/* 02 */ { 0x2000,  0000, "B10C32WR", "C32JED10" },
/* 03 */ { 0x3000,  1000, "B10PPCAA", "PPCJED10" },
/* 04 */ { 0x4000, 21000, "B10PPCWR", "PPCJWR10" },
/* 05 */ { 0x5000,  1000, "B10PPCFW", "PPCJED10" },
/* 06 */ { 0x6000,  5000, "B10MCFFW", "MCFJED10" },
/* 07 */ { 0x7000,  3000, "B10C12FW", "C12JED10" },
/* 08 */ { 0x8000,  4000, "B10MMCFW", "MMCJED10" },
/* 09 */ { 0x9000,  8000, "B10ARMWR", "ARMJED10" },
/* 10 */ { 0xA000,  8000, "B10ARMFW", "ARMJED10" },
/* 11 */ { 0xB000,  6000, "B10TRIFW", "TRIJED10" },

/* 12 */ { 0xCC00,  0000, "B10C32GD", "C32JED10" },
/* 13 */ { 0xCD00,  1000, "B10PPCGD", "PPCJED10" },
/* 14 */ { 0xCE00,  8000, "B10ARMGD", "ARMJED10" },
/* 15 */ { 0xCF00,  4000, "B10MMCGD", "MMCJED10" },
/* 16 */ { 0xD000,  4000, "B10MMCWR", "MMCJED10" },
/* 17 */ { 0xD100,  7000, "B10COPFW", "COPJED10" },
/* 18 */ { 0xD200,  7000, "B10COPWR", "COPJED10" },
/* 19 */ { 0xD300,  7000, "B10COPGD", "COPJED10" },
/* 20 */ { 0xD400,  9000, "B10PP4FW", "PP4JED10" },
/* 21 */ { 0xD500,  9000, "B10PP4WR", "PP4JED10" },
/* 22 */ { 0xD600,  9000, "B10PP4GD", "PP4JED10" },
/* 23 */ { 0xD700,  6000, "NOT_SUPP", "NOT_SUPP" },
/* 24 */ { 0xD800,  6000, "B10TRIGD", "TRIJED10" },
/* 25 */ { 0xD900,  5000, "NOT_SUPP", "NOT_SUPP" },
/* 26 */ { 0xDA00,  5000, "B10MCFGD", "MCFJED10" },
/* 27 */ { 0xDB00,  7000, "B10PWSFW", "COPJED10" },
/* 28 */ { 0xDC00,  7000, "B10PWSWR", "COPJED10" },
/* 29 */ { 0xDD00,  7000, "B10PWSGD", "COPJED10" },
/* 30 */ { 0xDE00, 10000, "B10R4KFW", "R4KJED10" },
/* 31 */ { 0xDF00, 10000, "B10XLSGD", "XLSJED10" },
/* 32 */ { 0xE000, 10000, "B10R4KGD", "R4KJED10" },
/* 33 */ { 0xE100, 11000, "B10XSCFW", "XSCJED10" },
/* 34 */ { 0xE200, 11000, "NOT_SUPP", "NOT_SUPP" },
/* 35 */ { 0xE300, 11000, "B10XSCGD", "XSCJED10" },
/* 36 */ { 0xE400, 10000, "B10R5KGD", "R5KJED10" },
/* 37 */ { 0xE500,  7000, "B10PQ3FW", "COPJED10" },
/* 38 */ { 0xE600,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 39 */ { 0xE700,  7000, "B10PQ3GD", "COPJED10" },
/* 40 */ { 0xE800,  8000, "B10A11FW", "ARMJED10" },
/* 41 */ { 0xE900,  8000, "B10A11GD", "ARMJED10" },
/* 42 */ { 0xEA00, 10000, "B10R5KFW", "R5KJED10" },
/* 43 */ { 0xEB00, 12000, "B10P55FW", "P55JED10" },
/* 44 */ { 0xEC00, 12000, "B10P55GD", "P55JED10" },
/* 45 */ { 0xED00, 13000, "B10PA6FW", "PA6JED10" },
/* 46 */ { 0xEE00, 13000, "B10PA6GD", "PA6JED10" },
/* 47 */ { 0xEF00, 13000, "B10SWDFW", "SWDJED10" },
/* 48 */ { 0xF000, 13000, "B10SWDGD", "SWDJED10" },
/* 49 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
/* 50 */ { 0xE300,  7000, "NOT_SUPP", "NOT_SUPP" },
};

static const BDI_SetupInfoT B30_SetupInfo[] =
{
/* 00 */ { ( 0 << 8), 0, "B30C32FW", "" },
/* 01 */ { ( 1 << 8), 0, "B30C32AA", "" },
/* 02 */ { ( 2 << 8), 0, "B30C32WR", "" },
/* 03 */ { ( 3 << 8), 0, "B30PPCAA", "" },
/* 04 */ { ( 4 << 8), 0, "B30PPCWR", "" },
/* 05 */ { ( 5 << 8), 0, "B30PPCFW", "" },
/* 06 */ { ( 6 << 8), 0, "B30MCFFW", "" },
/* 07 */ { ( 7 << 8), 0, "B30C12FW", "" },
/* 08 */ { ( 8 << 8), 0, "B30MMCFW", "" },
/* 09 */ { ( 9 << 8), 0, "B30ARMWR", "" },
/* 10 */ { (10 << 8), 0, "B30ARMFW", "" },
/* 11 */ { (11 << 8), 0, "B30TRIFW", "" },
/* 12 */ { (12 << 8), 0, "B30C32GD", "" },
/* 13 */ { (13 << 8), 0, "B30PPCGD", "" },
/* 14 */ { (14 << 8), 0, "B30ARMGD", "" },
/* 15 */ { (15 << 8), 0, "B30MMCGD", "" },
/* 16 */ { (16 << 8), 0, "B30MMCWR", "" },
/* 17 */ { (17 << 8), 0, "B30COPFW", "" },
/* 18 */ { (18 << 8), 0, "B30COPWR", "" },
/* 19 */ { (19 << 8), 0, "B30COPGD", "" },
/* 20 */ { (20 << 8), 0, "B30PP4FW", "" },
/* 21 */ { (21 << 8), 0, "B30PP4WR", "" },
/* 22 */ { (22 << 8), 0, "B30PP4GD", "" },
/* 23 */ { (23 << 8), 0, "B30QP4GD", "" },
/* 24 */ { (24 << 8), 0, "B30TRIGD", "" },
/* 25 */ { (25 << 8), 0, "B30QP4FW", "" },
/* 26 */ { (26 << 8), 0, "B30MCFGD", "" },
/* 27 */ { (27 << 8), 0, "B30PWSFW", "" },
/* 28 */ { (28 << 8), 0, "B30PWSWR", "" },
/* 29 */ { (29 << 8), 0, "B30PWSGD", "" },
/* 30 */ { (30 << 8), 0, "B30R4KFW", "" },
/* 31 */ { (31 << 8), 0, "B30XLSGD", "" },
/* 32 */ { (32 << 8), 0, "B30R4KGD", "" },
/* 33 */ { (33 << 8), 0, "B30XSCFW", "" },
/* 34 */ { (34 << 8), 0, "B30AV8FW", "" },
/* 35 */ { (35 << 8), 0, "B30XSCGD", "" },
/* 36 */ { (36 << 8), 0, "B30R5KGD", "" },
/* 37 */ { (37 << 8), 0, "B30PQ3FW", "" },
/* 38 */ { (38 << 8), 0, "B30AV8GD", "" },
/* 39 */ { (39 << 8), 0, "B30PQ3GD", "" },
/* 40 */ { (40 << 8), 0, "B30A11FW", "" },
/* 41 */ { (41 << 8), 0, "B30A11GD", "" },
/* 42 */ { (42 << 8), 0, "B30R5KFW", "" },
/* 43 */ { (43 << 8), 0, "B30P55FW", "" },
/* 44 */ { (44 << 8), 0, "B30P55GD", "" },
/* 45 */ { (45 << 8), 0, "B30PA6FW", "" },
/* 46 */ { (46 << 8), 0, "B30PA6GD", "" },
/* 47 */ { (47 << 8), 0, "B30SWDFW", "" },
/* 48 */ { (48 << 8), 0, "B30SWDGD", "" },
/* 49 */ { (49 << 8), 0, "B30SV8FW", "" },
/* 50 */ { (50 << 8), 0, "B30SV8GD", "" },
};

static BYTE     cmdBuffer[BDI_MAX_FRAME_SIZE];
static BYTE     ansBuffer[BDI_MAX_FRAME_SIZE];

static char aszFuseMap[ISP20_NBR_OF_ROWS][ISP20_ROW_BITS + 1];


/****************************************************************************
 ****************************************************************************
                General Helper functions
 ****************************************************************************
 ****************************************************************************/

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

 ****************************************************************************/

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


/****************************************************************************
 ****************************************************************************

   Converts a version number into a string
        0 = <unknown>
      101 = 1.01

     INPUT:  version         version a number
     OUTPUT: return          version as string

 ****************************************************************************/

static void BDI_Version2String(WORD version, char* szVersion)
{

  if ((version == 0) || (version > BDI_MAX_FW_VERSION)) {
    strcpy(szVersion, "unknown");
  } /* if */
  else {
    *szVersion++ = (char)('0'+ version / 100);
    version %= 100;
    *szVersion++ = '.';
    *szVersion++ = (char)('0'+ version / 10);
    version %= 10;
    *szVersion++ = (char)('0'+ version);
    *szVersion++ = 0;
  } /* else */
} /* BDI_Version2String */


/****************************************************************************
 ****************************************************************************

    BDI_Extension2Version :

    Converts a version extension into a number

     INPUT:  version         version as extension (e.g. .102)
     OUTPUT: return          version as number

 ****************************************************************************/

static WORD BDI_Extension2Version(char* szExt)
{
  WORD  version;
  int   i;

  if (strlen(szExt) != 4) return 0;
  version = 0;
  for (i=1; i<4; i++) {
    if ((szExt[i] < '0') || (szExt[i] > '9')) return 0;
    version *= 10;
    version += (WORD)(szExt[i] - '0');
  } /* for */
  return version;
} /* BDI_Extension2Version */


/****************************************************************************
 ****************************************************************************

    BDI_GetNewest :

    Searches for the newest file.
    The extension is the version (e.g. *.120 = V1.20 )

     INPUT:  szPath     the path to the firmware files
             szName     the name of the file (e.g. b20copgd)
     OUTPUT: szNewName  the full name incl. path of the found file
             RETURN     the newest version or 0 if not found

 ****************************************************************************/

#if defined(WIN32)

static WORD BDI_GetNewest(const char* szPath, const char* szName, char* szNewName)
{
  char                  szFullName[_MAX_PATH];
  char                  szDrive[_MAX_DRIVE];
  char                  szDir[_MAX_DIR];
  char                  szDummy[_MAX_FNAME];
  char                  szExt[_MAX_EXT];
  struct _finddata_t    fileInfo;
  intptr_t              hFile;
  WORD                  version;
  WORD                  nextVersion;

  /* build full file name */
  strcpy(szFullName, szPath);
  strcat(szFullName, "/dummy.xxx");
  _splitpath_s(szFullName,
               szDrive, sizeof szDrive,
               szDir,   sizeof szDir,
               szDummy, sizeof szDummy,
               szExt,   sizeof szExt);
  _makepath_s(szFullName, sizeof szFullName, szDrive, szDir, szName, "???");

  /* find first file */
  version = 0;
  hFile   = _findfirst(szFullName, &fileInfo );
  if (hFile == -1L) return 0;
  do {
    _splitpath_s(fileInfo.name,
                 szDrive, sizeof szDrive,
                 szDir,   sizeof szDir,
                 szDummy, sizeof szDummy,
                 szExt,   sizeof szExt);
    nextVersion = BDI_Extension2Version(szExt);
    if (nextVersion > version) {
      version = nextVersion;
      strcpy(szFullName, szPath);
      strcat(szFullName, "/");
      strcat(szFullName, fileInfo.name);
    } /* if */
  } while (_findnext(hFile, &fileInfo ) == 0);
  _findclose(hFile);
  strcpy(szNewName, szFullName);
  printf("Full file name: %s\n", szNewName);
  return version;
} /* BDI_GetNewest */

#else /* defined(WIN32) */

static WORD BDI_GetNewest(const char* szPath, const char* szName, char* szNewName)
{
  WORD           version;
  WORD           tmp;
  DIR           *dp;
  struct dirent *d;
  size_t         nameLen;

  /* open the firmware directory */
  dp = opendir(szPath);
  if (dp == NULL) return 0;

  /* search for newest */
  version = 0;
  nameLen = strlen(szName);
  while ((d = readdir(dp))) {
    if (    (strncasecmp(d->d_name, szName, nameLen) == 0)
         && (strlen(d->d_name) == (nameLen + 4))
        ) {
      tmp = BDI_Extension2Version(&d->d_name[nameLen]);
      if (tmp > version) {
        version = tmp;
        strcpy(szNewName, szPath);
        strcat(szNewName, "/");
        strcat(szNewName, d->d_name);
      } /* if */
    } /* if */
  } /* while */

  closedir(dp);
  return version;
} /* BDI_GetNewest */

#endif /* defined(WIN32) */

/****************************************************************************
 ****************************************************************************
                Firmware programming functions
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
 ****************************************************************************

    DecodeSRecord:

    Decode an Data S-Record (S1,S2,S3).

    INPUT  : sRecord      the S-Record to decode
    OUTPUT : addrPtr      the address for the decoded data
             dataPtr      the data part of the record (binary)
             RETURN       number of databytes, 0 if no data record, -1 if error

 ****************************************************************************/

static BYTE Hex2Bin(char highDigit, char lowDigit)
{
  static BYTE HexTable[] = {0,1,2,3,4,5,6,7,8,9,
                            0,0,0,0,0,0,0,         /* :;<=>?@ */
                            10,11,12,13,14,15};
  return  (BYTE)(   (HexTable[highDigit - '0']<<4)
                  + (HexTable[lowDigit  - '0']   ) );
} /* Hex2Bin */


static int DecodeSRecord(char* sRecord, DWORD* addrPtr, BYTE* dataPtr)
{
  int       count;
  int       recLen;
  int       addrLen;
  int       i;
  char      recType;
  BYTE      checksum;
  BYTE      nextValue;
  DWORD     address;

  if (*sRecord++ != 'S') return -1;
  recType   = *sRecord++;
  nextValue = Hex2Bin(*sRecord, *(sRecord+1));
  sRecord  += 2;
  checksum  = nextValue;
  recLen    = nextValue;

  /* extract address */
  if      (recType == '1') addrLen = 2;
  else if (recType == '2') addrLen = 3;
  else if (recType == '3') addrLen = 4;
  else                     return 0;
  address = 0;
  for (i=0; i<addrLen; i++) {
    nextValue = Hex2Bin(*sRecord, *(sRecord+1));
    sRecord  += 2;
    checksum  = (BYTE)(checksum + nextValue);
    address   = (address << 8) + nextValue;
  } /* for */
  *addrPtr = address;

  /* get data */
  count = recLen - addrLen - 1;
  for (i=0; i<count; i++) {
    nextValue = Hex2Bin(*sRecord, *(sRecord+1));
    sRecord  += 2;
    checksum  = (BYTE)(checksum + nextValue);
    *dataPtr++ = nextValue;
  } /* for */

  /* check sum */
  nextValue = Hex2Bin(*sRecord, *(sRecord+1));
  checksum = (BYTE)(checksum + nextValue);
  if (checksum != 0xFF) return -1;
  else                  return count;
} /* DecodeSRecord */


/****************************************************************************
 ****************************************************************************

 Read a block from the BDI memory (via loader command)

  INPUT:  addr            address of the memory block
          count           number of bytes to read (up to 1024)
  OUTPUT: block           the read data block
          return          error code

 ****************************************************************************/

static int BDI_ReadMemory(DWORD addr, WORD count, BYTE *block)
{
  BYTE *cmdPtr;
  BYTE *ansPtr;
  int   rxCount;
  BYTE  answer;
  int   i;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_READ_MEMORY, cmdBuffer);
  cmdPtr = BDI_AppendLong(addr,  cmdPtr);
  cmdPtr = BDI_AppendWord(count, cmdPtr);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 1000);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if ((rxCount != (int)(count+7)) || (answer != BDI_LDR_READ_MEMORY))
    return BDI_ERR_INVALID_RESPONSE;

  /* copy response data */
  ansPtr += 6; /* skip address and count */
  for (i=0; i<count; i++) *block++ = *ansPtr++;

  return BDI_OKAY;
} /* BDI_ReadMemory */


/****************************************************************************
 ****************************************************************************

 Erase a flash sector in BDI-HS memory (via loader command)

  INPUT:  addr            an address within the sector
  OUTPUT: return          error code

 ****************************************************************************/

static int BDI_EraseSector(DWORD addr)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  int       rxCount;
  BYTE      answer;
  BYTE      error;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ERASE_FLASH, cmdBuffer);
  cmdPtr = BDI_AppendLong(addr,  cmdPtr);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 10000);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if ((rxCount != 2) || (answer != BDI_LDR_ERASE_FLASH))
    return BDI_ERR_INVALID_RESPONSE;

  ansPtr = BDI_ExtractByte(&error,    ansPtr);
  if (error != 0) return BDI_ERR_FLASH_ERASE;

  return BDI_OKAY;
} /* BDI_EraseSector */


/****************************************************************************
 ****************************************************************************

 Program a block to BDI flash memory (via loader command)

  INPUT:  addr            address of the memory block
          count           number of bytes to program (up to 1024)
          block           the data to write
  OUTPUT: errorAddr       address of the failing byte
          return          error code

 ****************************************************************************/

static int BHS_ProgramFlash(DWORD  addr,
                            WORD   count,
                            BYTE  *block,
                            DWORD *errorAddr)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  int       rxCount;
  BYTE      answer;
  BYTE      error;
  WORD      i;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_PROGRAM_FLASH, cmdBuffer);
  cmdPtr = BDI_AppendLong(addr,  cmdPtr);
  cmdPtr = BDI_AppendWord((WORD)(count/2), cmdPtr);   /* number of words in BDI-HS */
  for (i=0; i<count; i++) *cmdPtr++ = *block++;

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 1000);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if ((rxCount != 6) || (answer != BDI_LDR_PROGRAM_FLASH))
    return BDI_ERR_INVALID_RESPONSE;

  ansPtr = BDI_ExtractByte(&error,    ansPtr);
  ansPtr = BDI_ExtractLong(errorAddr, ansPtr);
  if (error != 0) return BDI_ERR_FLASH_PROGRAM;

  return BDI_OKAY;
} /* BHS_ProgramFlash */

static int B20_ProgramFlash(DWORD  addr,
                            WORD   count,
                            BYTE  *block,
                            DWORD *errorAddr)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  int       rxCount;
  BYTE      answer;
  BYTE      error;
  WORD      i;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_PROGRAM_FLASH, cmdBuffer);
  cmdPtr = BDI_AppendLong(addr,  cmdPtr);
  cmdPtr = BDI_AppendWord(count, cmdPtr);
  for (i=0; i<count; i++) *cmdPtr++ = *block++;

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 1000);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if ((rxCount != 6) || (answer != BDI_LDR_PROGRAM_FLASH))
    return BDI_ERR_INVALID_RESPONSE;

  ansPtr = BDI_ExtractByte(&error,    ansPtr);
  ansPtr = BDI_ExtractLong(errorAddr, ansPtr);
  if (error != 0) return BDI_ERR_FLASH_PROGRAM;

  return BDI_OKAY;
} /* B20_ProgramFlash */

static int B10_ProgramFlash(DWORD  addr,
                            WORD   count,
                            BYTE  *block,
                            DWORD *errorAddr)
{
  return B20_ProgramFlash(addr, count, block, errorAddr);
} /* B10_ProgramFlash */

static int B30_ProgramFlash(DWORD  addr,
                            WORD   count,
                            BYTE  *block,
                            DWORD *errorAddr)
{
  return B20_ProgramFlash(addr, count, block, errorAddr);
} /* B30_ProgramFlash */


/****************************************************************************
 ****************************************************************************
 Update firmware
 The Loader must be activ and waiting for a command

  INPUT:  fileName    the S-Record file name
  OUTPUT: return      error code

 ****************************************************************************/

#define BHS_CONFIG_ADDR            0x084000L

static int BHS_UpdateFirmware(const char* fileName)
{
  int           result = BDI_OKAY;
  FILE*         srecFile;
  char          szLine[256];
  int           dataCount;
  DWORD         dataAddress;
  BYTE          dataValues[256];
  DWORD         errorAddr;

  /* Open the firmware file (read only) */
  srecFile = fopen(fileName,"r");
  if (srecFile == NULL) return BDI_ERR_FIRMWARE_FILE;

  /* erase flash */
  if (result == BDI_OKAY) result = BDI_EraseSector(BHS_CONFIG_ADDR);
  if (result == BDI_OKAY) result = BDI_EraseSector(0x0A0000);
  if (result == BDI_OKAY) result = BDI_EraseSector(0x0C0000);
  if (result == BDI_OKAY) result = BDI_EraseSector(0x0E0000);
  if (result != BDI_OKAY) {
    fclose(srecFile);
    return result;
  } /* if */

  /* / program firmware */
  while (result == BDI_OKAY) {
    if (fgets(szLine, sizeof szLine, srecFile) != NULL) {
      dataCount = DecodeSRecord(szLine, &dataAddress, dataValues);
      if (dataCount > 0) {
        result = BHS_ProgramFlash(dataAddress, (WORD)dataCount, dataValues, &errorAddr);
      } /* if */
      else if (dataCount < 0) {
        result = BDI_ERR_FIRMWARE_FILE;
      } /* else */
    } /* if */
    else break;
  } /* while */

  /* program firmware trigger */
  if (result == BDI_OKAY) {
    dataValues[0] = 0xAA;
    dataValues[1] = 0x55;
    dataValues[2] = 0x55;
    dataValues[3] = 0xAA;
    result = BHS_ProgramFlash(0x0A0000, 4, dataValues, &errorAddr);
  } /* if */

  fclose(srecFile);
  return result;
} /* BHS_UpdateFirmware */


#define B20_FIRMWARE_ADDR         0x01040000L /* base address of firmware */

static int B20_UpdateFirmware(const char* fileName)
{
  int           result = BDI_OKAY;
  FILE*         srecFile;
  char          szLine[256];
  int           dataCount;
  DWORD         dataAddress;
  BYTE          dataValues[256];
  DWORD         errorAddr;

  int           sendCount;
  DWORD         nextAddr;
  DWORD         baseAddr;
  BYTE          sendData[BDI_MAX_BLOCK_SIZE];

  /* Open the firmware file (read only) */
  srecFile = fopen(fileName, "r");
  if (srecFile == NULL) return BDI_ERR_FIRMWARE_FILE;

  /* erase flash */
  nextAddr = 0;
  baseAddr = 0;
  printf("Erasing firmware flash ....\n");
  if (result == BDI_OKAY) result = BDI_EraseSector(B20_FIRMWARE_ADDR);
  if (result == BDI_OKAY) result = BDI_EraseSector(B20_FIRMWARE_ADDR + 0x40000);
  if (result == BDI_OKAY) result = BDI_EraseSector(B20_FIRMWARE_ADDR + 0x80000);
  if (result != BDI_OKAY) {
    fclose(srecFile);
    printf("Erasing firmware flash failed\n");
    return result;
  } /* if */
  printf("Erasing firmware flash passed\n");

  /* program firmware */
  printf("Programming firmware flash ....\n");
  sendCount = 0;
  while (result == BDI_OKAY) {
    if (fgets(szLine, sizeof szLine, srecFile) != NULL) {
      dataCount = DecodeSRecord(szLine, &dataAddress, dataValues);
      if (dataCount > 0) {
        if (sendCount == 0) {
          nextAddr = dataAddress;
          baseAddr = dataAddress;
        } /* if */
        else if ((nextAddr != dataAddress) || ((sendCount + dataCount) > BDI_MAX_BLOCK_SIZE)) {
          /* make multiple of 4 */
          while ((sendCount & 3) != 0) {
            sendData[sendCount] = 0xFF;
            sendCount++;
          } /* while */
          result = B20_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
          nextAddr  = dataAddress;
          baseAddr  = dataAddress;
          sendCount = 0;
          putchar('.');
          fflush(stdout);
        } /* else if */
        /* copy new data to send buffer */
        (void)memcpy(&sendData[sendCount], dataValues, dataCount);
        sendCount += dataCount;
        nextAddr  += dataCount;
      } /* if */
      else if (dataCount < 0) {
        result = BDI_ERR_FIRMWARE_FILE;
      } /* else if */
    } /* if */

    /* send last block */
    else {
      if (sendCount > 0) {
        /* make multiple of 4 */
        while ((sendCount & 3) != 0) {
          sendData[sendCount] = 0xFF;
          sendCount++;
        } /* while */
        result = B20_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
      } /* if */
      break;
    } /* else */
  } /* while */

  /* program firmware trigger */
  if (result == BDI_OKAY) {
    dataValues[0] = 0xAA;
    dataValues[1] = 0x55;
    dataValues[2] = 0x55;
    dataValues[3] = 0xAA;
    result = B20_ProgramFlash(B20_FIRMWARE_ADDR, 4, dataValues, &errorAddr);
  } /* if */

  if (result == BDI_OKAY) {
    printf("\nProgramming firmware flash passed\n");
  } /* if */
  else {
    printf("\nProgramming firmware flash failed\n");
  } /* else */

  fclose(srecFile);
  return result;
} /* B20_UpdateFirmware */


#define B10_FIRMWARE_ADDR         0x0A0000L /* base address of firmware */
#define B10_CONFIG_ADDR           0x086000L /* base address of BDM configuration */

static int B10_UpdateFirmware(const char* fileName)
{
  int           result = BDI_OKAY;
  FILE*         srecFile;
  char          szLine[256];
  int           dataCount;
  DWORD         dataAddress;
  BYTE          dataValues[256];
  DWORD         errorAddr;

  int           sendCount;
  DWORD         nextAddr;
  DWORD         baseAddr;
  BYTE          sendData[BDI_MAX_BLOCK_SIZE];

  /* Open the firmware file (read only) */
  srecFile = fopen(fileName,"r");
  if (srecFile == NULL) return BDI_ERR_FIRMWARE_FILE;

  /* erase flash */
  nextAddr = 0;
  baseAddr = 0;
  printf("Erasing firmware flash ....\n");
  if (result == BDI_OKAY) result = BDI_EraseSector(B10_CONFIG_ADDR);
  if (result == BDI_OKAY) result = BDI_EraseSector(B10_FIRMWARE_ADDR);
  if (result == BDI_OKAY) result = BDI_EraseSector(B10_FIRMWARE_ADDR + 0x20000);
  if (result == BDI_OKAY) result = BDI_EraseSector(B10_FIRMWARE_ADDR + 0x40000);
  if (result != BDI_OKAY) {
    fclose(srecFile);
    printf("Erasing firmware flash failed\n");
    return result;
  } /* if */
  printf("Erasing firmware flash passed\n");

  printf("Programming firmware flash ....\n");
  sendCount = 0;
  while (result == BDI_OKAY) {
    if (fgets(szLine, sizeof szLine, srecFile) != NULL) {
      dataCount = DecodeSRecord(szLine, &dataAddress, dataValues);
      if (dataCount > 0) {
        if (sendCount == 0) {
          nextAddr = dataAddress;
          baseAddr = dataAddress;
        } /* if */
        else if ((nextAddr != dataAddress) || ((sendCount + dataCount) > BDI_MAX_BLOCK_SIZE)) {
          /* make multiple of 4 */
          while ((sendCount & 3) != 0) {
            sendData[sendCount] = 0xFF;
            sendCount++;
          } /* while */
          result = B10_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
          nextAddr  = dataAddress;
          baseAddr  = dataAddress;
          sendCount = 0;
          putchar('.');
          fflush(stdout);
        } /* else if */
        /* copy new data to send buffer */
        (void)memcpy(&sendData[sendCount], dataValues, dataCount);
        sendCount += dataCount;
        nextAddr  += dataCount;
      } /* if */
      else if (dataCount < 0) {
        result = BDI_ERR_FIRMWARE_FILE;
      } /* else if */
    } /* if */

    /* send last block */
    else {
      if (sendCount > 0) {
        /* make multiple of 4 */
        while ((sendCount & 3) != 0) {
          sendData[sendCount] = 0xFF;
          sendCount++;
        } /* while */
        result = B10_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
      } /* if */
      break;
    } /* else */
  } /* while */

  /* program firmware trigger */
  if (result == BDI_OKAY) {
    dataValues[0] = 0xAA;
    dataValues[1] = 0x55;
    dataValues[2] = 0x55;
    dataValues[3] = 0xAA;
    result = B10_ProgramFlash(B10_FIRMWARE_ADDR, 4, dataValues, &errorAddr);
  } /* if */

  if (result == BDI_OKAY) {
    printf("\nProgramming firmware flash passed\n");
  } /* if */
  else {
    printf("\nProgramming firmware flash failed\n");
  } /* else */

  fclose(srecFile);
  return result;
} /* B10_UpdateFirmware */


#define B30_FIRMWARE_ADDR         0x00100000L /* base address of firmware */

static int B30_UpdateFirmware(const char* fileName)
{
  int           result;
  int	        sector;
  DWORD	        address;
  FILE*         srecFile;
  char          szLine[256];
  int           dataCount;
  DWORD         dataAddress;
  BYTE          dataValues[256];
  DWORD         errorAddr;
  DWORD         copySrc;
  DWORD         copyDest;
  DWORD         copyCount;
  DWORD         copyType;

  int           sendCount;
  DWORD         nextAddr;
  DWORD         baseAddr;
  BYTE          sendData[BDI_MAX_BLOCK_SIZE];

  /* Open the firmware file (read only) */
  result = BDI_OKAY;
  srecFile = fopen(fileName, "r");
  if (srecFile == NULL) return BDI_ERR_FIRMWARE_FILE;

  /* erase flash */
  nextAddr = 0;
  baseAddr = 0;
  printf("Erasing firmware flash ....\n");
  address = B30_FIRMWARE_ADDR;
  for (sector = 0; sector < 16; sector++) {
    if (result == BDI_OKAY) result = BDI_EraseSector(address);
    address += 0x10000;
  } /* for */
  if (result != BDI_OKAY) {
    fclose(srecFile);
    printf("Erasing firmware flash failed\n");
    return result;
  } /* if */
  printf("Erasing firmware flash passed\n");

  /* program firmware */
  printf("Programming firmware flash ....\n");
  sendCount = 0;
  while (result == BDI_OKAY) {
    if (fgets(szLine, sizeof szLine, srecFile) != NULL) {
      dataCount = DecodeSRecord(szLine, &dataAddress, dataValues);
      if (dataCount > 0) {
        if (sendCount == 0) {
          nextAddr = dataAddress;
          baseAddr = dataAddress;
        } /* if */
        else if ((nextAddr != dataAddress) || ((sendCount + dataCount) > BDI_MAX_BLOCK_SIZE)) {
          /* make multiple of 4 */
          while ((sendCount & 3) != 0) {
            sendData[sendCount] = 0xFF;
            sendCount++;
          } /* while */
          result = B30_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
          nextAddr  = dataAddress;
          baseAddr  = dataAddress;
          sendCount = 0;
          putchar('.');
          fflush(stdout);
        } /* else if */
        /* copy new data to send buffer */
        (void)memcpy(&sendData[sendCount], dataValues, dataCount);
        sendCount += dataCount;
        nextAddr  += dataCount;
      } /* if */
      else if (dataCount < 0) {
        result = BDI_ERR_FIRMWARE_FILE;
      } /* else if */
    } /* if */

    /* send last block */
    else {
      if (sendCount > 0) {
        /* make multiple of 4 */
        while ((sendCount & 3) != 0) {
          sendData[sendCount] = 0xFF;
          sendCount++;
        } /* while */
        result = B30_ProgramFlash(baseAddr, (WORD)sendCount, sendData, &errorAddr);
      } /* if */
      break;
    } /* else */
  } /* while */

  /* check if plausible firmware header */
  if (result == BDI_OKAY) {
    (void)BDI_ReadMemory(B30_FIRMWARE_ADDR, 8 * 4, dataValues);
    (void)BDI_ExtractLong(&copySrc,   (dataValues +  4));
    (void)BDI_ExtractLong(&copyDest,  (dataValues +  8));
    (void)BDI_ExtractLong(&copyCount, (dataValues + 12));
    (void)BDI_ExtractLong(&copyType,  (dataValues + 24));
    copyCount *= 4;
    if (    (copySrc  < 0x00100000) || ((copySrc  + copyCount) > 0x00400000)
         || (copyDest < 0x40000000) || ((copyDest + copyCount) > 0x41000000)
         || ((copyType & 0xffff) != 1)
       ) {
      printf("\nInvalid Firmware File!\n");
      fclose(srecFile);
      return BDI_ERR_FIRMWARE_FILE;
    } /* if */
  } /* if */

  /* program firmware trigger */
  if (result == BDI_OKAY) {
    dataValues[0] = 0xAA;
    dataValues[1] = 0x55;
    dataValues[2] = 0x55;
    dataValues[3] = 0xAA;
    result = B30_ProgramFlash(B30_FIRMWARE_ADDR, 4, dataValues, &errorAddr);
  } /* if */

  if (result == BDI_OKAY) {
    printf("\nProgramming firmware flash passed\n");
  } /* if */
  else {
    printf("\nProgramming firmware flash failed\n");
  } /* else */

  fclose(srecFile);
  return result;
} /* B30_UpdateFirmware */


/****************************************************************************
 ****************************************************************************
                Logic programming functions
 ****************************************************************************
 ****************************************************************************/

#define ISP_2096_ID     0x13
#define ISP_2032_ID     0x15
#define ISP_2064_ID     0x12

/****************************************************************************
 ****************************************************************************

 ispLSI: Enable / Disable ISP Mode

  INPUT:  -
  OUTPUT: -
          return          error code

 ****************************************************************************/

static int ISP_Enable()
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_ENABLE, cmdBuffer);
  cmdPtr = BDI_AppendByte(1, cmdPtr);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 100);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* ISP_Enable */

static int ISP_Disable()
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_ENABLE, cmdBuffer);
  cmdPtr = BDI_AppendByte(0, cmdPtr);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 100);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* ISP_Disable */


/****************************************************************************
 ****************************************************************************

 ispLSI: Get Device ID

  INPUT:  -
  OUTPUT: deviceId        the ID of the ispLSI Device
          return          error code

 ****************************************************************************/

static int  ISP_GetDeviceId(BYTE *deviceId)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_READ_ID, cmdBuffer);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 100);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(deviceId, ansBuffer+1);

  return BDI_OKAY;
} /* ISP_GetDeviceId */


/****************************************************************************
 ****************************************************************************

 ispLSI: Read cell array line

  INPUT:  nLine           the number of the line to program
  OUTPUT: szProgData      the data of the line (ASCII) loaded at program level
          szErasedData    the data of the line (ASCII) loaded at erased level
          return          error code

 ****************************************************************************/

static int ISP_ReadArrayLine(int      nLine,
                             char*    szProgData,
                             char*    szErasedData)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  BYTE      answer;
  int       rxCount;
  int       lineLength;
  int       i;
  char*     putPtr;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_READ_LINE, cmdBuffer);
  cmdPtr = BDI_AppendByte((BYTE)nLine, cmdPtr);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 100);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if (answer != BDI_LDR_ISP_READ_LINE) return BDI_ERR_INVALID_RESPONSE;
  lineLength = (rxCount - 1) / 2;

  /* copy prog data */
  putPtr = szProgData;
  for (i = 0; i < lineLength; i++) *putPtr++ = (char)(*ansPtr++);
  *putPtr = 0;

  /* copy erased data */
  putPtr = szErasedData;
  for (i = 0; i < lineLength; i++) *putPtr++ = (char)(*ansPtr++);
  *putPtr = 0;

  return BDI_OKAY;
} /* ISP_ReadArrayLine */


/****************************************************************************
 ****************************************************************************

 ispLSI: Program cell array line

  INPUT:  nLine           the number of the line to read
          szLineData      the data of the line (ASCII)
  OUTPUT: -
          return          error code

 ****************************************************************************/

static int ISP_ProgramArrayLine(int nLine, const char* szLineData)
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_PROGRAM_LINE, cmdBuffer);
  cmdPtr = BDI_AppendByte((BYTE)nLine, cmdPtr);
  while (*szLineData != 0) {
    cmdPtr = BDI_AppendByte((BYTE)(*szLineData++), cmdPtr);
  } /* while */

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 300);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* ISP_ProgramArrayLine */


/****************************************************************************
 ****************************************************************************

 ispLSI: Read UES

  INPUT:  -
  OUTPUT: szUES           User Electronic Signature
          return          error code

 ****************************************************************************/

static int ISP_ReadUES(char* szUES)
{
  BYTE     *cmdPtr;
  BYTE     *ansPtr;
  BYTE      answer;
  int       rxCount;
  int       uesLength;
  int       i;
  char*     putPtr;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_READ_UES, cmdBuffer);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 100);
  if (rxCount < 0) return rxCount;

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if (answer != BDI_LDR_ISP_READ_UES) return BDI_ERR_INVALID_RESPONSE;
  uesLength = rxCount - 1;

  /* copy UES */
  putPtr = szUES;
  for (i = 0; i < uesLength; i++) *putPtr++ = (char)(*ansPtr++);
  *putPtr = 0;

  return BDI_OKAY;
} /* ISP_ReadUES */


/****************************************************************************
 ****************************************************************************

 ispLSI: Program UES

  INPUT:  csUES           User Electronic Signature
  OUTPUT: -
          return          error code

 ****************************************************************************/

static int ISP_ProgramUES(const char* szUES)
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_PROGRAM_UES, cmdBuffer);
  while (*szUES != 0) {
    cmdPtr = BDI_AppendByte((BYTE)(*szUES++), cmdPtr);
  } /* while */

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 300);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* ISP_ProgramUES */


/****************************************************************************
 ****************************************************************************

 ispLSI: Erase the chip

  INPUT:  -
  OUTPUT: -
          return          error code

 ****************************************************************************/

static int ISP_Erase(void)
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_ISP_ERASE, cmdBuffer);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 600);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* ISP_Erase */


/****************************************************************************
 ****************************************************************************

 Conversion Ascii/Hex to UES

 ****************************************************************************/

/* 10 hex digits to 40 bit UES */
static void ISPHS_Hex2UES(const char* szHex, char* szUES)
{
  int   i;
  char  c;

  *szUES = 0;
  for (i = 0; i < 10; i++) {
    c = toupper(szHex[i]);
    switch (c) {
      case '0': strcat(szUES, "0000"); break;
      case '1': strcat(szUES, "0001"); break;
      case '2': strcat(szUES, "0010"); break;
      case '3': strcat(szUES, "0011"); break;
      case '4': strcat(szUES, "0100"); break;
      case '5': strcat(szUES, "0101"); break;
      case '6': strcat(szUES, "0110"); break;
      case '7': strcat(szUES, "0111"); break;
      case '8': strcat(szUES, "1000"); break;
      case '9': strcat(szUES, "1001"); break;
      case 'A': strcat(szUES, "1010"); break;
      case 'B': strcat(szUES, "1011"); break;
      case 'C': strcat(szUES, "1100"); break;
      case 'D': strcat(szUES, "1101"); break;
      case 'E': strcat(szUES, "1110"); break;
      case 'F': strcat(szUES, "1111"); break;
      default : strcat(szUES, "0000");
    } /* switch */
  } /* for */
} /* ISPHS_Hex2UES */


/* Ascii string  to 120 bit UES */
static void ISP20_Ascii2UES(const char* szAscii, char* szUES)
{
  int       i,j;
  int       ch;

  for (i = 0; i < ISP20_UES_CHARS; i++) {
    if (strlen(szAscii) <= (size_t)i) ch = 0;
    else                              ch = szAscii[i];
    for (j = 0; j < 8; j++) {
      *szUES++ = (ch & 0x80) ? '1' : '0';
      ch <<= 1;
    } /* for */
  } /* for */
  *szUES = 0;
} /* ISP20_Ascii2UES */


/* Ascii string  to 80 bit UES */
static void ISP10_Ascii2UES(const char* szAscii, char* szUES)
{
  int       i,j;
  int       ch;

  for (i = 0; i < ISP10_UES_CHARS; i++) {
    if (strlen(szAscii) <= (size_t)i) ch = 0;
    else                              ch = szAscii[i];
    for (j = 0; j < 8; j++) {
      *szUES++ = (ch & 0x80) ? '1' : '0';
      ch <<= 1;
    } /* for */
  } /* for */
  *szUES = 0;
} /* ISP10_Ascii2UES */


/****************************************************************************
 ****************************************************************************

    ISP_LoadFuseMap:

     Loads the Fuse Map from the two JEDEC Files into the ROW data array

     INPUT  : pszJedecFile      jedec file name for EPLD
     OUTPUT : RETURN            error code

 ****************************************************************************/

static int ISPHS_LoadFuseMap(const char* pszJedecFile)
{
  FILE* jedecFile;
  char  sLine[101];
  char* fuseBit;
  char* szRow;
  int   row;
  int   part;

  /* open Jedec files */
  jedecFile = fopen(pszJedecFile, "rt");
  if (jedecFile == NULL) {
    return BDI_ERR_LOGIC_FILE;
  } /* if */

  /* find start of fuse map */
  do {
    if (fgets(sLine, sizeof sLine - 1, jedecFile) == NULL) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    }
  } while (strncmp(sLine,"*L00000", 7));

  /* read fuse map */
  for (row = 0; row < ISPHS_NBR_OF_ROWS; row++) {
    szRow = aszFuseMap[row];
    for (part = 0; part < 2; part++) {
      fgets(sLine, sizeof sLine - 1, jedecFile);
      fuseBit = sLine;
      if (fuseBit) {
        while (*fuseBit == '0' || *fuseBit == '1') {
          *szRow++ = *fuseBit++;
        } /* while */
      } /* if */
    } /* for */
    szRow = 0;
    if (strlen(aszFuseMap[row]) != ISPHS_ROW_BITS) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    } /* if */
  } /* for */

  fclose(jedecFile);
  return BDI_OKAY;
} /* ISPHS_LoadFuseMap */


static int ISP20_LoadFuseMap(const char* pszJedecFile)
{
  FILE* jedecFile;
  char  sLine[101];
  char* fuseBit;
  char* szRow;
  int   row;
  int   part;

  /* open Jedec files */
  jedecFile = fopen(pszJedecFile, "rt");
  if (jedecFile == NULL) {
    return BDI_ERR_LOGIC_FILE;
  } /* if */

  /* find start of fuse map */
  do {
    if (fgets(sLine, sizeof sLine - 1, jedecFile) == NULL) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    }
  } while (strncmp(sLine,"*L00000", 7));

  /* read fuse map */
  for (row = 0; row < ISP20_NBR_OF_ROWS; row++) {
    szRow = aszFuseMap[row];
    for (part = 0; part < 4; part++) {
      fgets(sLine, sizeof sLine - 1, jedecFile);
      fuseBit = sLine;
      if (fuseBit) {
        while (*fuseBit == '0' || *fuseBit == '1') {
          *szRow++ = *fuseBit++;
        } /* while */
      } /* if */
    } /* for */
    szRow = 0;
    if (strlen(aszFuseMap[row]) != ISP20_ROW_BITS) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    } /* if */
  } /* for */

  fclose(jedecFile);
  return BDI_OKAY;
} /* ISP20_LoadFuseMap */


static int ISP10_LoadFuseMap(const char* pszJedecFile)
{
  FILE* jedecFile;
  char  sLine[81];
  char* fuseBit;
  char* szRow;
  int   row;
  int   part;

  /* open Jedec files */
  jedecFile = fopen(pszJedecFile, "rt");
  if (jedecFile == NULL) {
    return BDI_ERR_LOGIC_FILE;
  } /* if */

  /* find start of fuse map */
  do {
    if (fgets(sLine, sizeof sLine - 1, jedecFile) == NULL) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    }
  } while (strncmp(sLine,"*L00000", 7));

  /* read fuse map */
  for (row = 0; row < ISP10_NBR_OF_ROWS; row++) {
    szRow = aszFuseMap[row];
    for (part = 0; part < 4; part++) {
      fgets(sLine, sizeof sLine - 1, jedecFile);
      fuseBit = sLine;
      if (fuseBit) {
        while (*fuseBit == '0' || *fuseBit == '1') {
          *szRow++ = *fuseBit++;
        } /* while */
      } /* if */
    } /* for */
    szRow = 0;
    if (strlen(aszFuseMap[row]) != ISP10_ROW_BITS) {
      fclose(jedecFile);
      return BDI_ERR_LOGIC_FILE;
    } /* if */
  } /* for */

  fclose(jedecFile);
  return BDI_OKAY;
} /* ISP10_LoadFuseMap */


/****************************************************************************
 ****************************************************************************

 Update logic
 The Loader must be activ and waiting for a command

  INPUT:  version     the version number
          fileName    the JEDEC file name
  OUTPUT: return      error code

 ****************************************************************************/

static int BHS_UpdateLogic(WORD version, const char* fileName)
{
  int   result;
  char  szVersion[10 + 1];
  char  szUES[ISPHS_UES_BITS + 1];
  int   row;
  char  szRowProg[ISPHS_ROW_BITS + 1];
  char  szRowErase[ISPHS_ROW_BITS + 1];
  char  szDeviceUES[ISPHS_UES_BITS + 1];

  /* build UES string */
  sprintf(szVersion, "B3201E%04i", version);
  ISPHS_Hex2UES(szVersion, szUES);

  /* load fuse map */
  result = ISPHS_LoadFuseMap(fileName);

  /* enable ISP mode */
  if (result == BDI_OKAY) {
    result = ISP_Enable();
  } /* if */

  /* program fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISPHS_NBR_OF_ROWS; row++) {
      result = ISP_ProgramArrayLine(row, aszFuseMap[row]);
      if (result != BDI_OKAY) break;
    } /* for */
  } /* if */

  /* program UES */
  if (result == BDI_OKAY) {
    result = ISP_ProgramUES(szUES);
  } /* if */

  /* verify fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISPHS_NBR_OF_ROWS; row++) {
      result = ISP_ReadArrayLine(row, szRowProg, szRowErase);
      if (result != BDI_OKAY) break;
      if (    (strcmp(aszFuseMap[row], szRowProg) != 0)
           || (strcmp(aszFuseMap[row], szRowErase) != 0)
           ) {
        result = BDI_ERR_LOGIC_VERIFY;
        break;
      } /* if */
    } /* for */
  } /* if */

  /* verify UES */
  if (result == BDI_OKAY) {
    result = ISP_ReadUES(szDeviceUES);
    if (strcmp(szDeviceUES, szUES) != 0) result = BDI_ERR_LOGIC_VERIFY;
  } /* if */

  /* disable ISP mode */
  if (result == BDI_OKAY) result = ISP_Disable();
  else                    (void)ISP_Disable();

  return result;
} /* BHS_UpdateLogic */


static int B20_UpdateLogic(WORD version, const char* fileName)
{
  int   result;
  char  szVersion[10 + 1];
  char  szUES[ISP20_UES_BITS + 1];
  int   row;
  char  szRowProg[ISP20_ROW_BITS + 1];
  char  szRowErase[ISP20_ROW_BITS + 1];
  char  szDeviceUES[ISP20_UES_BITS + 1];

  /* build UES string */
  sprintf(szVersion, "B6001E%c%03i", ('0' + (version / 1000)), (version % 1000));
  ISP20_Ascii2UES(szVersion, szUES);

  /* load fuse map */
  result = ISP20_LoadFuseMap(fileName);

  /* enable ISP mode */
  if (result == BDI_OKAY) {
    result = ISP_Enable();
  } /* if */

  /* program fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISP20_NBR_OF_ROWS; row++) {
      result = ISP_ProgramArrayLine(row, aszFuseMap[row]);
      if (result != BDI_OKAY) break;
      putchar('.');
      fflush(stdout);
    } /* for */
  } /* if */

  /* program UES */
  if (result == BDI_OKAY) {
    result = ISP_ProgramUES(szUES);
  } /* if */

  /* verify fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISP20_NBR_OF_ROWS; row++) {
      result = ISP_ReadArrayLine(row, szRowProg, szRowErase);
      if (result != BDI_OKAY) break;
      if (    (strcmp(aszFuseMap[row], szRowProg) != 0)
           || (strcmp(aszFuseMap[row], szRowErase) != 0)
           ) {
        result = BDI_ERR_LOGIC_VERIFY;
        break;
      } /* if */
      putchar('.');
      fflush(stdout);
    } /* for */
  } /* if */

  /* verify UES */
  if (result == BDI_OKAY) {
    result = ISP_ReadUES(szDeviceUES);
    if (strcmp(szDeviceUES, szUES) != 0) result = BDI_ERR_LOGIC_VERIFY;
  } /* if */

  /* disable ISP mode */
  if (result == BDI_OKAY) result = ISP_Disable();
  else                    (void)ISP_Disable();

  if (result == BDI_OKAY) {
    printf("\nProgramming CPLD passed\n");
  } /* if */
  else {
    printf("\nProgramming CPLD failed\n");
  } /* else */

  return result;
} /* B20_UpdateLogic */


static int B10_UpdateLogic(WORD version, const char* fileName)
{
  int   result;
  char  szVersion[10 + 1];
  char  szUES[ISP10_UES_BITS + 1];
  int   row;
  char  szRowProg[ISP10_ROW_BITS + 1];
  char  szRowErase[ISP10_ROW_BITS + 1];
  char  szDeviceUES[ISP10_UES_BITS + 1];

  /* build UES string */
  sprintf(szVersion, "B1001E%c%03i", ('0' + (version / 1000)), (version % 1000));
  ISP10_Ascii2UES(szVersion, szUES);

  /* load fuse map */
  result = ISP10_LoadFuseMap(fileName);

  /* enable ISP mode */
  if (result == BDI_OKAY) {
    result = ISP_Enable();
  } /* if */

  /* program fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISP10_NBR_OF_ROWS; row++) {
      result = ISP_ProgramArrayLine(row, aszFuseMap[row]);
      if (result != BDI_OKAY) break;
      putchar('.');
      fflush(stdout);
    } /* for */
  } /* if */

  /* program UES */
  if (result == BDI_OKAY) {
    result = ISP_ProgramUES(szUES);
  } /* if */

  /* verify fuse map */
  if (result == BDI_OKAY) {
    for (row = 0; row < ISP10_NBR_OF_ROWS; row++) {
      result = ISP_ReadArrayLine(row, szRowProg, szRowErase);
      if (result != BDI_OKAY) break;
      if (    (strcmp(aszFuseMap[row], szRowProg) != 0)
           || (strcmp(aszFuseMap[row], szRowErase) != 0)
           ) {
        result = BDI_ERR_LOGIC_VERIFY;
        break;
      } /* if */
      putchar('.');
      fflush(stdout);
    } /* for */
  } /* if */

  /* verify UES */
  if (result == BDI_OKAY) {
    result = ISP_ReadUES(szDeviceUES);
    if (strcmp(szDeviceUES, szUES) != 0) result = BDI_ERR_LOGIC_VERIFY;
  } /* if */

  /* disable ISP mode */
  if (result == BDI_OKAY) result = ISP_Disable();
  else                    (void)ISP_Disable();

  if (result == BDI_OKAY) {
    printf("\nProgramming CPLD passed\n");
  } /* if */
  else {
    printf("\nProgramming CPLD failed\n");
  } /* else */

  return result;
} /* B10_UpdateLogic */


/****************************************************************************
 ****************************************************************************
                helper functions
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
 ****************************************************************************

    B30_VerifyLoaderCode :

    Check if illegal data is stored in the flash

    INPUT:  -
    OUTPUT: return        error code

 ****************************************************************************/

static WORD AccumulateCRC(WORD count, BYTE* data, WORD crc)
{
  static  WORD  crcLockupTable[256] =
              { 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
                0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
                0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
                0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
                0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
                0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
                0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
                0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
                0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
                0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
                0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
                0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
                0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
                0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
                0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
                0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
                0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
                0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
                0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
                0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
                0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
                0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
                0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
                0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
                0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
                0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
                0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
                0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
                0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
                0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
                0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
                0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };
  WORD  result;
  WORD  w;

  while (count--) {
    result = (crc >> 8);
    w = ((crc ^ *data++) & 0xFF);
    crc = (result ^ crcLockupTable[w]);
  } /* while */
  return crc;
} /* AccumulateCRC */

static BOOL AllErased(WORD count, const BYTE* data)
{
  while (count--) {
    if (*data != 0xFF) return FALSE;
    data++;
  } /* while */
  return TRUE;
} /* AllErased */

static int B30_VerifyLoaderCode(void)
{
  DWORD addr;
  WORD  crc;
  BYTE  data[BDI_MAX_BLOCK_SIZE];

  /* check unused part of boot sector */
  addr = 0x00000510;
  while (addr < 0x2000) {
    (void)BDI_ReadMemory(addr, sizeof data, data);
    if (!AllErased(BDI_MAX_BLOCK_SIZE, data)) return BDI_ERR_VERIFY;
    addr += BDI_MAX_BLOCK_SIZE;
  } /* while */

  /* check unused part of loader sector */
  (void)BDI_ReadMemory(0x10000, sizeof data, data);
  (void)BDI_ExtractLong(&addr, (data + 12));
  addr = 0x10040 + (4 * addr);
  while (addr < 0x30000) {
    (void)BDI_ReadMemory(addr, sizeof data, data);
    if (!AllErased(BDI_MAX_BLOCK_SIZE, data)) return BDI_ERR_VERIFY;
    addr += BDI_MAX_BLOCK_SIZE;
  } /* while */

  /* calculate crc16 over boot/loader sectors */
  crc  = 0;
  addr = 0x00000000;
  while (addr < 0x30000) {
    (void)BDI_ReadMemory(addr, sizeof data, data);
    if (addr == 0x00000000) {
      (void)memset(data + 0x20, 0, 8); /* serial number */
    } /* if */
    crc = AccumulateCRC(sizeof data, data, crc);
    addr += BDI_MAX_BLOCK_SIZE;
  } /* while */
  printf("CRC over boot/loader sectors is %i\n", crc);

  return BDI_OKAY;
} /* B30_VerifyLoaderCode */


/****************************************************************************
 ****************************************************************************
                main functions
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
 ****************************************************************************

  BDI_ExitLoader :

  Transfer the exit loader command

  INPUT:  -
  OUTPUT: return        error code

 ****************************************************************************/


static int BDI_ExitLoader()
{
  BYTE     *cmdPtr;
  int       rxCount;

  /* prepare command */
  cmdPtr = BDI_AppendByte(BDI_LDR_EXIT_LOADER, cmdBuffer);

  /* BDI transaction */
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer, cmdBuffer, sizeof ansBuffer, ansBuffer, 200);
  if (rxCount < 0) return rxCount;

  return BDI_OKAY;
} /* BDI_ExitLoader */


/****************************************************************************
 ****************************************************************************

    BDI_ConnectLoader :

    Connects to the Loader an get the current versions

     INPUT  : szPort          the communication port (e.g. "/dev/tty1")
     OUTPUT : bdiType         the BDI type
              loaderVersion   the current loader version
              firmwareVersion the current firmware version
              logicVersion    the current logic version
              RETURN          0 if okay or a negativ number if error

 ****************************************************************************/

static int BDI_ConnectLoader(const char* szPort, DWORD baudrate, BDI_VersionT *pVersion)
{
  BYTE *cmdPtr;
  BYTE *ansPtr;
  int   rxCount;
  BYTE  answer;
  int   result;
  int   i;

  /* connect to BDI */
  result = BDI_OKAY;
  for (i = 0; i < 3; i++) {
    result = BDI_Open(szPort, baudrate);
    if ((result == BDI_OKAY) || (result == BDI_ASYN_SETUP)) break;
  } /* for */
  if (result != BDI_OKAY) return result;

  /* send start loader command */
  cmdPtr  = BDI_AppendByte(BDI_LDR_START_LOADER, cmdBuffer);
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer,
                            cmdBuffer,
                            sizeof ansBuffer,
                            ansBuffer,
                            BDI_DEFAULT_EXEC_TIME);
  if (rxCount < 0) {
    BDI_Close();
    return rxCount;
  } /* if */

  /* delay and connect again if loader not alredy activ */
  if (*ansBuffer == BDI_LDR_START_LOADER) {
    BDI_Close();
    BDI_DoDelay(1000);
    result = BDI_Open(szPort, baudrate);
    if (result != BDI_OKAY) return result;
  } /* if */

  /* read version */
  cmdPtr  = BDI_AppendByte(BDI_LDR_READ_VERSION, cmdBuffer);
  rxCount = BDI_Transaction(cmdPtr-cmdBuffer,
                            cmdBuffer,
                            sizeof ansBuffer,
                            ansBuffer, 1000);
  if (rxCount < 0) {
    BDI_Close();
    return rxCount;
  } /* if */

  /* select BDI type */
  if ((rxCount == 15) || (rxCount == 17)) {
    pVersion->bdi = BDI_TYPE_20;
  } /* if */
  else if (rxCount == 7) {
    pVersion->bdi = BDI_TYPE_HS;
  } /* else if */
  else if (rxCount == 23) {
    pVersion->bdi = BDI_TYPE_10;
  } /* else if */
  else if (rxCount == 21) {
    pVersion->bdi = BDI_TYPE_30;
  } /* else if */
  else {
    BDI_Close();
    return BDI_ERR_UNKNOWN_BDI;
  } /* else */

  /* analyse response */
  ansPtr = BDI_ExtractByte(&answer, ansBuffer);
  if (answer != BDI_LDR_READ_VERSION) {
    BDI_Close();
    return BDI_ERR_INVALID_RESPONSE;
  } /* if */
  ansPtr = BDI_ExtractWord(&pVersion->loader,   ansPtr);
  ansPtr = BDI_ExtractWord(&pVersion->firmware, ansPtr);
  if (pVersion->bdi == BDI_TYPE_30) {
    pVersion->logic = 0;
    ansPtr += 4;                /* skip BDI3000 CPLD UES */
  } /* if */
  else {
    ansPtr = BDI_ExtractWord(&pVersion->logic,    ansPtr);
  } /* else */
  for (i = 0; i < 8; i++) pVersion->sn[i] = (char)(*ansPtr++);
  pVersion->sn[8] = 0;
  if (rxCount == 17) {
    ansPtr++;   /* skip '-' */
    if (*ansPtr == 'C') pVersion->bdi = BDI_TYPE_21;
  } /* if */

  return BDI_OKAY;
} /* BDI_ConnectLoader */


/****************************************************************************
 ****************************************************************************

  BDI_EraseFirmwareLogic :

  Erase firmware and logic.

  INPUT:  szPort        the communication port (e.g. /dev/tty1 )
          baudrate      the baudrate for the port
  OUTPUT: return        error code

 ****************************************************************************/

int BDI_EraseFirmwareLogic(const char* szPort, DWORD baudrate)
{
  int           result;
  int	        sector;
  DWORD	        address;
  BDI_VersionT  version;
  BYTE          ispDeviceId;

  /* connect to BDI loader and read versions */
  printf("Connecting to BDI loader\n");
  result = BDI_ConnectLoader(szPort, baudrate, &version);
  if (result < 0) {
    printf("Connecting to BDI loader failed (%i)\n", result);
    return result;
  } /* if */

  /* first, erase logic */
  if ((result == BDI_OKAY) && (version.bdi != BDI_TYPE_30)) {
    printf("Erasing CPLD\n");
    if (result == BDI_OKAY) result = ISP_Enable();
    if (result == BDI_OKAY) result = ISP_GetDeviceId(&ispDeviceId);
    if (result == BDI_OKAY) result = ISP_Erase();
    if (result == BDI_OKAY) result = ISP_Disable();
    if (result == BDI_OKAY) {
      if (    ((version.bdi == BDI_TYPE_HS) && (ispDeviceId != ISP_2032_ID))
           || ((version.bdi == BDI_TYPE_20) && (ispDeviceId != ISP_2096_ID))
           || ((version.bdi == BDI_TYPE_21) && (ispDeviceId != ISP_2096_ID))
           || ((version.bdi == BDI_TYPE_10) && (ispDeviceId != ISP_2064_ID))
         ) result = BDI_ERR_LOGIC_DEVICE;
    } /* if */
    if (result != BDI_OKAY) printf("Erasing CPLD failed (%i)\n", result);
  } /* if */

  if (result == BDI_OKAY) {
    printf("Erasing all flash sectors\n");
    if (version.bdi == BDI_TYPE_HS) {
      result = BDI_EraseSector(0x0A0000);
    } /* if */
    /* for security reasons, erase all sectors of BDI2000 */
    else if ((version.bdi == BDI_TYPE_20) || (version.bdi == BDI_TYPE_21)) {
      if (result == BDI_OKAY) result = BDI_EraseSector(0x01008000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x0100C000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x01010000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x01040000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x01080000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x010C0000L);
    } /* else if */
    /* for security reasons, erase all sectors of BDI1000 */
    else if (version.bdi == BDI_TYPE_10) {
      if (result == BDI_OKAY) result = BDI_EraseSector(0x084000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x086000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x088000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x0A0000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x0C0000L);
      if (result == BDI_OKAY) result = BDI_EraseSector(0x0E0000L);
    } /* else if */

    /* for security reasons, erase almost all sectors of BDI3000 */
    else if (version.bdi == BDI_TYPE_30) {
      /* erase configuration sectors */
      address = 0x2000;
      for (sector = 1; sector < 8; sector++) {
        if (result == BDI_OKAY) result = BDI_EraseSector(address);
        address += 0x02000;
        putchar('.');
        fflush(stdout);
      } /* for */
      /* erase unused loader sectors */
      address = 0x30000;
      for (sector = 3; sector < 16; sector++) {
        if (result == BDI_OKAY) result = BDI_EraseSector(address);
        address += 0x10000;
        putchar('.');
        fflush(stdout);
      } /* for */
      /* erase firmware sectors */
      address = B30_FIRMWARE_ADDR;
      for (sector = 0; sector < 48; sector++) {
        if (result == BDI_OKAY) result = BDI_EraseSector(address);
        address += 0x10000;
        putchar('.');
        fflush(stdout);
      } /* for */
      printf("\n");

      /* check for illegal data stored in flash */
      if (result == BDI_OKAY) {
        printf("Checking for illegal data in boot/loader sectors\n");
        result = B30_VerifyLoaderCode();
        if (result != BDI_OKAY) {
          printf("Illegal data in boot/loader sectors detected!\n");
        } /* if */
      } /* if */

    } /* else if */
    if (result != BDI_OKAY) printf("Erasing firmware failed (%i)\n", result);
  } /* if */

  if (result == BDI_OKAY) printf("Erasing passed\n");

  /* disconnect */
  BDI_Close();
  return result;
} /* BDI_EraseFirmwareLogic */


/****************************************************************************
 ****************************************************************************

    BDI_UpdateFirmwareLogic:

     Updates the firmware and the logic of the BDI.

     INPUT  : szPort      the communication port (e.g. /dev/tty1 )
              baudrate    the baudrate for the port
              szPath      the directory path to the firmware files
              targetType  the connected target type (e.g. BDI_FWT_MPC)
              updateMode  the update mode
                            BDI_UPDATE_AUTO
                            BDI_UPDATE_FIRMWARE
                            BDI_UPDATE_LOGIC
                            BDI_UPDATE_ALL
     OUTPUT : RETURN      0 if okay or a negativ number if error.

 ****************************************************************************/

int BDI_UpdateFirmwareLogic(const char* szPort,
                                  DWORD baudrate,
                            const char* szPath,
                                  WORD  targetType,
                                  WORD  updateMode)
{
  int             result;
  BDI_VersionT    version;
  WORD            newestFirmware;
  WORD            newestLogic;
  char            szFirmwareName[MAXPATHLEN];
  char            szLogicName[MAXPATHLEN];
  BOOL            updateFirmware;
  BOOL            updateLogic;
  BYTE            ispDeviceId;
  size_t          len;

  const BDI_SetupInfoT* setupInfo;

  /* check parameter */
  if (targetType >= (sizeof B20_SetupInfo / sizeof B20_SetupInfo[0])) {
    return BDI_ERR_INVALID_PARAMETER;
  } /* if */

  /* connect to BDI loader and read versions */
  printf("Connecting to BDI loader\n");
  result = BDI_ConnectLoader(szPort, baudrate, &version);
  if (result < 0) {
    printf("Connecting to BDI loader failed (%i)\n", result);
    return result;
  } /* if */

  /* get setup info based on BDI type */
  if      (version.bdi == BDI_TYPE_HS) setupInfo = &BHS_SetupInfo[targetType];
  else if (version.bdi == BDI_TYPE_20) setupInfo = &B20_SetupInfo[targetType];
  else if (version.bdi == BDI_TYPE_21) setupInfo = &B21_SetupInfo[targetType];
  else if (version.bdi == BDI_TYPE_10) setupInfo = &B10_SetupInfo[targetType];
  else if (version.bdi == BDI_TYPE_30) setupInfo = &B30_SetupInfo[targetType];
  else {
    BDI_Close();
    return BDI_ERR_INVALID_PARAMETER;
  } /* else */

  /* check for full qualified BDI3000 firmware file */
  szFirmwareName[0] = 0;
  len = strlen(szPath);
  if ((version.bdi == BDI_TYPE_30) && (len >= 12)) {
    if (    (szPath[len-12] == 'b')
         && (szPath[len-11] == '3')
         && (szPath[len-10] == '0')
         && (szPath[len-4]  == '.')
        ) {
      (void)strcpy(szFirmwareName, szPath);
      updateMode = BDI_UPDATE_FIRMWARE;
    } /* if */
  } /* if */

  /* get newest firmware */
  if (szFirmwareName[0] == 0) {
    newestFirmware = BDI_GetNewest(szPath, setupInfo->firmwareName, szFirmwareName);
    if (newestFirmware == 0) {
      printf("No valid firmware file found in %s\n", szPath);
      BDI_Close();
      return BDI_ERR_FIRMWARE_FILE;
    } /* if */
  } /* if */

  /* get newest logic */
  if (version.bdi != BDI_TYPE_30) {
    newestLogic = BDI_GetNewest(szPath, setupInfo->logicName, szLogicName);
    if (newestLogic == 0) {
      printf("No valid JEDEC file found in %s\n", szPath);
      BDI_Close();
      return BDI_ERR_LOGIC_FILE;
    } /* if */
  } /* if */

  /* decide if firmware update is neccecary */
  updateFirmware = ((updateMode == BDI_UPDATE_ALL) || (updateMode == BDI_UPDATE_FIRMWARE));
  if (!updateFirmware) {
    updateFirmware = (   ((version.firmware - setupInfo->firmwareType) > BDI_MAX_FW_VERSION)
                      || ((version.firmware - setupInfo->firmwareType) < newestFirmware) );
    if (!updateFirmware) printf("Firmware is already up to date\n");
  } /* if */

  /* decide if logic update is neccecary */
  updateLogic = FALSE;
  if (version.bdi != BDI_TYPE_30) {
    updateLogic = ((updateMode == BDI_UPDATE_ALL) || (updateMode == BDI_UPDATE_LOGIC));
    if (!updateLogic) {
      updateLogic = (   ((version.logic - setupInfo->logicType) > BDI_MAX_LOGIC_VERSION)
                     || ((version.logic - setupInfo->logicType) < newestLogic) );
      if (!updateLogic) printf("CPLD is already up to date\n");
    } /* if */
  } /* if */

  /* first, erase logic */
  if (updateLogic && (result == BDI_OKAY)) {
    printf("Erasing CPLD\n");
    if (result == BDI_OKAY) result = ISP_Enable();
    if (result == BDI_OKAY) result = ISP_GetDeviceId(&ispDeviceId);
    if (result == BDI_OKAY) result = ISP_Erase();
    if (result == BDI_OKAY) result = ISP_Disable();
    if (result == BDI_OKAY) {
      if (    ((version.bdi == BDI_TYPE_HS) && (ispDeviceId != ISP_2032_ID))
           || ((version.bdi == BDI_TYPE_20) && (ispDeviceId != ISP_2096_ID))
           || ((version.bdi == BDI_TYPE_21) && (ispDeviceId != ISP_2096_ID))
           || ((version.bdi == BDI_TYPE_10) && (ispDeviceId != ISP_2064_ID))
         ) result = BDI_ERR_LOGIC_DEVICE;
    } /* if */
    if (result != BDI_OKAY) printf("Erasing CPLD failed (%i)\n", result);
  } /* if */

  /* update firmware */
  if (updateFirmware && (result == BDI_OKAY)) {
    printf("Programming firmware with %s\n", szFirmwareName);
    if      (version.bdi == BDI_TYPE_HS) result = BHS_UpdateFirmware(szFirmwareName);
    else if (version.bdi == BDI_TYPE_20) result = B20_UpdateFirmware(szFirmwareName);
    else if (version.bdi == BDI_TYPE_21) result = B20_UpdateFirmware(szFirmwareName);
    else if (version.bdi == BDI_TYPE_10) result = B10_UpdateFirmware(szFirmwareName);
    else if (version.bdi == BDI_TYPE_30) result = B30_UpdateFirmware(szFirmwareName);
    if (result != BDI_OKAY) printf("Programming firmware failed (%i)\n", result);
  } /* if */

  /* program logic */
  if (updateLogic && (result == BDI_OKAY)) {
    printf("Programming CPLD with %s\n", szLogicName);
    newestLogic += setupInfo->logicType;
    if      (version.bdi == BDI_TYPE_HS) result = BHS_UpdateLogic(newestLogic, szLogicName);
    else if (version.bdi == BDI_TYPE_20) result = B20_UpdateLogic(newestLogic, szLogicName);
    else if (version.bdi == BDI_TYPE_21) result = B20_UpdateLogic(newestLogic, szLogicName);
    else if (version.bdi == BDI_TYPE_10) result = B10_UpdateLogic(newestLogic, szLogicName);
    if (result != BDI_OKAY) printf("Programming CPLD failed (%i)\n", result);
  } /* if */

  if (result == BDI_OKAY) printf("Programming passed\n");

  /* disconnect */
  BDI_Close();
  return result;
} /* BDI_UpdateFirmwareLogic */


/****************************************************************************
 ****************************************************************************

 BDI_UpdateConfig :

   Transfer the BDI network configuration

  INPUT:  szPort                the communication port (e.g. /dev/tty1 )
          baudrate              the baudrate for the port
          szHostIP              the IP address of the host
          szBdiIP               the IP address of the BDI
          szSubnetMask      	the subnet mask
          szDefaultGateway  	the default gateway
          szSetupFileName       the name of the setup file on the host
  OUTPUT: return                error code

 ****************************************************************************/

#define B10_NETWORK_ADDR          0x00084000L /* base address of network configuration */

#define B20_NETWORK_ADDR          0x01008000L /* base address of network configuration */
#define B20_CONFIG_ADDR           0x010C0000L /* base address of configuration */
#define B20_REGDEF_ADDR           0x010D0000L /* base address of register definition */

#define B30_NETWORK_ADDR          0x00006000L /* base address of network configuration */
#define B30_CONFIG_ADDR           0x00200000L /* base address of configuration */
#define B30_REGDEF_ADDR           0x00210000L /* base address of register definition */


int  BDI_UpdateConfig(const char* szPort,
                            DWORD baudrate,
                      const char* szHostIP,
                      const char* szBdiIP,
                      const char* szSubnetMask,
                      const char* szDefaultGateway,
                      const char* szSetupFileName)
{
  int           result;
  BDI_VersionT  version;
  BYTE          configData[104];
  BYTE          configReadBack[104];
  BYTE*         configPtr;
  DWORD         errorAddr;
  DWORD         networkAddr;
  DWORD         configAddr;
  DWORD         regdefAddr;
  size_t        nameLength;
  size_t        i;

  WORD          fwType;
  DWORD         hostIP;
  DWORD         flashAddr;
  int           romConfigSize;
  int           romRegdefSize;
  BYTE          romConfig[BDI_MAX_CONFIG_SIZE];
  BYTE          romRegdef[BDI_MAX_REGDEF_SIZE];

  /* connect to BDI loader and read versions */
  printf("Connecting to BDI loader\n");
  result = BDI_ConnectLoader(szPort, baudrate, &version);
  if (result < 0) {
    printf("Connecting to BDI loader failed (%i)\n", result);
    return result;
  } /* if */

  /* get firmware type */
  if (version.bdi == BDI_TYPE_30) {
    fwType = version.firmware >> 8;
  } /* if */
  else if (version.firmware < 0xC000) {
    fwType = version.firmware >> 12;
  } /* else if */
  else {
    fwType = (version.firmware - 0xC000) >> 8;
  } /* else */

  /* set network configuration addresses */
  if ((version.bdi == BDI_TYPE_20) || (version.bdi == BDI_TYPE_21)) {
    networkAddr = B20_NETWORK_ADDR;
  } /* if */
  else if (version.bdi == BDI_TYPE_10) {
    networkAddr = B10_NETWORK_ADDR;
  } /* else if */
  else if (version.bdi == BDI_TYPE_30) {
    networkAddr = B30_NETWORK_ADDR;
  } /* else if */
  else {
    BDI_Close();
    printf("### invalid BDI connected\n");
    return BDI_ERR_INVALID_PARAMETER;
  } /* else */

  /* build network configuration data */
  printf("Writing network configuration\n");
  configPtr = configData;
  *configPtr++ = 0x00;
  *configPtr++ = 0x0C;
  *configPtr++ = 0x01;
  *configPtr++ = (BYTE)((version.sn[0]-'0') * 16 + (version.sn[1]-'0'));
  *configPtr++ = (BYTE)((version.sn[2]-'0') * 16 + (version.sn[3]-'0'));
  *configPtr++ = (BYTE)((version.sn[4]-'0') * 16 + (version.sn[5]-'0'));

  /* gap */
  *configPtr++ = 0xFF;
  *configPtr++ = 0xFF;

  /* IP addresses */
  configPtr  = BDI_AppendLong(BDI_IPAddrMotorola(szBdiIP), configPtr);
  configPtr  = BDI_AppendLong(BDI_IPAddrMotorola(szSubnetMask), configPtr);
  configPtr  = BDI_AppendLong(BDI_IPAddrMotorola(szDefaultGateway), configPtr);

  /* Host IP and Configuration File */
  hostIP = BDI_IPAddrMotorola(szHostIP);
  configPtr = BDI_AppendLong(hostIP, configPtr);
  nameLength = strlen(szSetupFileName);
  if (nameLength > 79) nameLength = 79;
  for (i=0; i<nameLength; i++) {
    configPtr = BDI_AppendByte((BYTE)szSetupFileName[i], configPtr);
  } /* for */
  configPtr = BDI_AppendByte(0x00, configPtr);  /* terminating zero */

  /* erase configuration flash sector */
  if (result == BDI_OKAY) result = BDI_EraseSector(networkAddr);

  /* program and verify network data */
  if ((configPtr-configData) > (int)(sizeof configData)) result = -9999; /* adjust config data buffers */
  if (result == BDI_OKAY) result = B20_ProgramFlash(networkAddr, sizeof configData, configData, &errorAddr);
  if (result == BDI_OKAY) result = BDI_ReadMemory(networkAddr, sizeof configReadBack, configReadBack);
  if (result == BDI_OKAY) {
    if (memcmp(configData, configReadBack, sizeof configData) != 0) result = BDI_ERR_FLASH_VERIFY;
  } /* if */

  /* program configuration and register definitions into BDI flash */
  if ((result == BDI_OKAY) && (hostIP == INADDR_NONE) && (strlen(szSetupFileName) > 0)) {

    if ((version.bdi == BDI_TYPE_20) || (version.bdi == BDI_TYPE_21)) {
      configAddr  = B20_CONFIG_ADDR;
      regdefAddr  = B20_REGDEF_ADDR;
      if (result == BDI_OKAY) result = BDI_EraseSector(configAddr);
    } /* if */
    else if (version.bdi == BDI_TYPE_30) {
      configAddr  = B30_CONFIG_ADDR;
      regdefAddr  = B30_REGDEF_ADDR;
      if (result == BDI_OKAY) result = BDI_EraseSector(configAddr);
      if (result == BDI_OKAY) result = BDI_EraseSector(regdefAddr);
    } /* else if */
    else {
      BDI_Close();
      printf("### invalid BDI connected\n");
      return BDI_ERR_INVALID_PARAMETER;
    } /* else */

    /* build config data from file */
    memset(romConfig, 0xff, sizeof romConfig);
    romConfigSize = CNF_BuildRomConfig(szSetupFileName, romConfig);
    if (romConfigSize < 0) result = romConfigSize;
    if (romConfigSize >= BDI_MAX_CONFIG_SIZE) result = BDI_ERR_FILE_ACCESS;

    /* program config data */
    configPtr = romConfig;
    flashAddr = configAddr;
    while ((result == BDI_OKAY) && (romConfigSize > 0)) {
      result = B20_ProgramFlash(flashAddr, BDI_MAX_BLOCK_SIZE, configPtr, &errorAddr);
      romConfigSize -= BDI_MAX_BLOCK_SIZE;
      configPtr += BDI_MAX_BLOCK_SIZE;
      flashAddr += BDI_MAX_BLOCK_SIZE;
    } /* while */

    /* build regdef data from file */
    memset(romRegdef, 0xff, sizeof romRegdef);
    romRegdefSize = CNF_BuildRomRegdef(szSetupFileName, romConfig, romRegdef);
    if (romRegdefSize < 0) result = romRegdefSize;
    if (romRegdefSize >= BDI_MAX_REGDEF_SIZE) result = BDI_ERR_FILE_ACCESS;

    // program regdef data
    configPtr = romRegdef;
    flashAddr = regdefAddr;
    while ((result == BDI_OKAY) && (romRegdefSize > 0)) {
      result = B20_ProgramFlash(flashAddr, BDI_MAX_BLOCK_SIZE, configPtr, &errorAddr);
      romRegdefSize -= BDI_MAX_BLOCK_SIZE;
      configPtr += BDI_MAX_BLOCK_SIZE;
      flashAddr += BDI_MAX_BLOCK_SIZE;
    } /* while */

  } /* if */

  if (result == BDI_OKAY) printf("Configuration passed\n");
  else                    printf("Configuration failed (%i)\n", result);

  /* disconnect */
  BDI_Close();
  return result;
} /* BDI_UpdateConfig */


/****************************************************************************
 ****************************************************************************

 BDI_DisplayVersion :

  Display BDI information on standard output

  INPUT:  szPort        the communication port (e.g. /dev/tty1 )
          baudrate      the baudrate for the port
  OUTPUT: return        error code

 ****************************************************************************/

static const char* FirmwareType[] =
{
/* 00 */ "Firmware for CPU32",
/* 01 */ "bdiAda for CPU32",
/* 02 */ "bdiWind for CPU32",
/* 03 */ "bdiAda for MPC8xx",
/* 04 */ "bdiWind for MPC8xx",
/* 05 */ "Firmware for MPC8xx/MPC5xx",
/* 06 */ "Firmware for ColdFire",
/* 07 */ "Firmware for HC12",
/* 08 */ "Firmware for M-CORE",
/* 09 */ "bdiWind for ARM7/9",
/* 10 */ "Firmware for ARM7/9",
/* 11 */ "Firmware for TriCore",
/* 12 */ "bdiGDB for CPU32",
/* 13 */ "bdiGDB for MPC8xx/MPC5xx",
/* 14 */ "bdiGDB for ARM7/9",
/* 15 */ "bdiGDB for M-CORE",
/* 16 */ "bdiWind for M-CORE",
/* 17 */ "Firmware for PPC6xx/PPC7xx",
/* 18 */ "bdiWind for PPC6xx/PPC7xx",
/* 19 */ "bdiGDB for PPC6xx/PPC7xx",
/* 20 */ "Firmware for PPC400",
/* 21 */ "bdiWind for PPC400",
/* 22 */ "bdiGDB for PPC400",
/* 23 */ "bdiGDB for QorIQ P3/P4/P5/T1/T2/T4",
/* 24 */ "bdiGDB for TriCore",
/* 25 */ "Firmware for QorIQ P3/P4/P5/T1/T2/T4",
/* 26 */ "bdiGDB for ColdFire",
/* 27 */ "Firmware for MPC7450",
/* 28 */ "bdiWind for MPC7450",
/* 29 */ "bdiGDB for MPC7450",
/* 30 */ "Firmware for MIPS32",
/* 31 */ "bdiGDB for XLS/XLR",
/* 32 */ "bdiGDB for MIPS32",
/* 33 */ "Firmware for XScale",
/* 34 */ "Firmware for ARMV8",
/* 35 */ "bdiGDB for XScale",
/* 36 */ "bdiGDB for MIPS64",
/* 37 */ "Firmware for MPC85xx",
/* 38 */ "bdiGDB for ARMV8",
/* 39 */ "bdiGDB for MPC85xx",
/* 40 */ "Firmware for ARM11",
/* 41 */ "bdiGDB for ARM11",
/* 42 */ "Firmware for MIPS64",
/* 43 */ "Firmware for MPC5500",
/* 44 */ "bdiGDB for MPC5500",
/* 45 */ "Firmware for PA6T",
/* 46 */ "bdiGDB for PA6T",
/* 47 */ "Firmware for ARM-SWD",
/* 48 */ "bdiGDB for ARM-SWD",
/* 49 */ "Firmware for ARMV8-SWD",
/* 50 */ "bdiGDB for ARMV8-SWD",
};

static const char* LogicType[] =
{
/* 00 */ "CPU32/CPU16",
/* 01 */ "MPC8xx/MPC5xx",
/* 02 */ "ColdFire V2",
/* 03 */ "HC12",
/* 04 */ "M-CORE",
/* 05 */ "ColdFire",
/* 06 */ "TriCore",
/* 07 */ "PPC6xx/PPC7xx",
/* 08 */ "ARM",
/* 09 */ "PPC400",
/* 10 */ "MIPS32/MIPS64",
/* 11 */ "XScale",
/* 12 */ "MPC5500",
/* 13 */ "PA6T",
/* 14 */ "ARM-SWD",
/* 15 */ "unknown 15",
/* 16 */ "unknown 16",
/* 17 */ "unknown 17",
/* 18 */ "unknown 18",
/* 19 */ "unknown 19",
/* 20 */ "unknown 20",
/* 21 */ "MPC8xx spez. for Tornado"
};

int  BDI_DisplayVersion(const char* szPort, DWORD baudrate, BOOL start)
{
  int           result;
  BDI_VersionT  version;
  WORD          fwType;
  const char*   szFwType = "unknown firmware type";
  const char*   szLogicType = "unknown logic type";
  char          szVersion[5];
  DWORD         networkAddr;
  BYTE          cnf[104];

  /* connect to BDI loader and read versions */
  result = BDI_ConnectLoader(szPort, baudrate, &version);
  if (result < 0) {
    printf("Connecting to BDI loader failed (%i)\n", result);
    return result;
  } /* if */

  /* get firmware type */
  if (version.bdi == BDI_TYPE_30) {
    fwType = version.firmware >> 8;
  } /* if */
  else if (version.firmware < 0xC000) {
    fwType = version.firmware >> 12;
  } /* else if */
  else {
    fwType = (version.firmware - 0xC000) >> 8;
  } /* else*/
  if (fwType < (sizeof FirmwareType / sizeof FirmwareType[0])) {
    szFwType = FirmwareType[fwType];
  } /* if */

  /* get logic type */
  if ((version.logic / 1000) < (int)(sizeof LogicType / sizeof LogicType[0])) {
    szLogicType = LogicType[version.logic / 1000];
  } /* if */

  /* display BDI info */
  if (version.bdi == BDI_TYPE_HS) {
    printf("BDI Type : BDI-HS\n");
  } /* if */
  else if (version.bdi == BDI_TYPE_20) {
    printf("BDI Type : BDI2000 (SN: %s)\n", version.sn);
  } /* else if */
  else if (version.bdi == BDI_TYPE_21) {
    printf("BDI Type : BDI2000 Rev.C (SN: %s)\n", version.sn);
  } /* else if */
  else if (version.bdi == BDI_TYPE_10) {
    printf("BDI Type : BDI1000 (SN: %s)\n", version.sn);
  } /* else if */
  else if (version.bdi == BDI_TYPE_30) {
    printf("BDI Type : BDI3000 (SN: %s)\n", version.sn);
  } /* else if */
  else  {
    printf("BDI Type : unknown\n");
  } /* else*/

  BDI_Version2String(version.loader, szVersion);
  printf("Loader   : V%s\n", szVersion);

  if (version.firmware != 0) {
    BDI_Version2String(version.firmware & 0xFF, szVersion);
    printf("Firmware : V%s %s\n", szVersion, szFwType);
  } /* if */
  else {
    printf("Firmware : unknown\n");
  } /* else */

  if (version.bdi != BDI_TYPE_30) {
    if (version.logic != 0) {
      BDI_Version2String(version.logic % 1000, szVersion);
      printf("Logic    : V%s %s\n", szVersion, szLogicType);
    } /* if */
    else {
      printf("Logic    : unknown\n");
    } /* else */
  } /* if */

  /* set network configuration address */
  if ((version.bdi == BDI_TYPE_20) || (version.bdi == BDI_TYPE_21)) {
    networkAddr = B20_NETWORK_ADDR;
  } /* if */
  else if (version.bdi == BDI_TYPE_10) {
    networkAddr = B10_NETWORK_ADDR;
  } /* else if */
  else if (version.bdi == BDI_TYPE_30) {
    networkAddr = B30_NETWORK_ADDR;
  } /* else if */
  else {
    if (start) result = BDI_ExitLoader();
    BDI_Close();
    return result;
  } /* else */

  /* read back configuration data */
  result = BDI_ReadMemory(networkAddr, sizeof cnf, cnf);
  if (result < 0) {
    printf("Reading network configuration failed (%i)\n", result);
    if (start) result = BDI_ExitLoader();
    BDI_Close();
    return result;
  } /* if */

  /* display configuration */
  cnf[sizeof cnf - 1] = 0;
  printf("MAC      : %02x-%02x-%02x-%02x-%02x-%02x\n", cnf[0],cnf[1],cnf[2],cnf[3],cnf[4],cnf[5]);
  printf("IP Addr  : %i.%i.%i.%i\n", cnf[8] ,cnf[9] ,cnf[10],cnf[11]);
  printf("Subnet   : %i.%i.%i.%i\n", cnf[12],cnf[13],cnf[14],cnf[15]);
  printf("Gateway  : %i.%i.%i.%i\n", cnf[16],cnf[17],cnf[18],cnf[19]);
  printf("Host IP  : %i.%i.%i.%i\n", cnf[20],cnf[21],cnf[22],cnf[23]);
  printf("Config   : %s\n", &cnf[24]);

  /* exit loader */
  if (start) result = BDI_ExitLoader();

  /* disconnect */
  BDI_Close();
  return result;
} /* BDI_DisplayVersion */


/****************************************************************************
 ****************************************************************************

   main

 ****************************************************************************/

#define CMD_USAGE       0
#define CMD_VERSION     1
#define CMD_ERASE       2
#define CMD_UPDATE      3
#define CMD_CONFIG      4

#define APP_GDB         0
#define APP_TOR         1
#define APP_ADA         2
#define APP_ACC         3

#define CPU_CPU32       0
#define CPU_MPC800      1
#define CPU_PPC600      2
#define CPU_PPC400      3
#define CPU_ARM         4
#define CPU_TRICORE     5
#define CPU_MCF         6
#define CPU_HC12        7
#define CPU_MCORE       8
#define CPU_MPC7450     9
#define CPU_R4K        10
#define CPU_XSCALE     11
#define CPU_R5K        12
#define CPU_MPC8500    13
#define CPU_ARM11      14
#define CPU_MPC5500    15
#define CPU_PA6T       16
#define CPU_ARMSWD     17
#define CPU_XLS        18
#define CPU_P4080      19
#define CPU_ARMV8      20
#define CPU_SWDV8      21


static const int AppCpuToFw[4][22] =
{
/*          C32 PPC COP PP4 ARM TRI MCF HC12 MCF 7450 R4K XSC R5K PQ3 A11 P55 PA6 SWD XLS QP4 AV8 SV8*/
/* GDB */ { 12, 13, 19, 22, 14, 24, 26, -1,  15, 29,  32, 35, 36, 39, 41, 44, 46, 48, 31, 23, 38, 50 },
/* TOR */ {  2,  4, 18, 21,  9, -1, 25, -1,  -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
/* ADA */ {  1,  3, -1, -1, -1, -1, -1, -1,  -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
/* ACC */ {  0,  5, 17, 20, 10, 11,  6,  7,   8, 27,  30, 33, 42, 37, 40, 43, 45, 47, -1, 25, 34, 49 }
};


int main(int argc, char *argv[ ])
{
  int   result;
  int   command;
  int   i;
  char* arg;
  int   fwType;

  char  port[MAXPATHLEN] = "/dev/ttyS0";
  char  dir[MAXPATHLEN]  = ".";
  char  file[MAXPATHLEN] = "";

  char  ip[32]   = "0.0.0.0";   /* selects bootp */
  char  host[32] = "255.255.255.255";
  char  mask[32] = "255.255.255.255";
  char  gate[32] = "255.255.255.255";

  DWORD baudrate = 38400;       /* default baudrate */
  BOOL  start    = FALSE;       /* default firmware startup */
  int   appType  = APP_GDB;     /* default application type */
  int   cpuType  = CPU_MPC800;  /* default target CPU type  */


  /* get command */
  command = CMD_USAGE;
  if (argc > 1) {
    if      (strcmp(argv[1], "-v") == 0) command = CMD_VERSION;
    else if (strcmp(argv[1], "-e") == 0) command = CMD_ERASE;
    else if (strcmp(argv[1], "-u") == 0) command = CMD_UPDATE;
    else if (strcmp(argv[1], "-c") == 0) command = CMD_CONFIG;
  } /* if */

  /* get parameters */
  for (i = 2; i < argc; i++) {
    arg = argv[i];

    /* serial communication device */
    if (strncmp(arg, "-p", 2) == 0) {
      arg += 2;
      strcpy(port, arg);
    } /* if */

    /* serial baud rate */
    else if (strncmp(arg, "-b", 2) == 0) {
      arg += 2;
      if      (strcmp(arg,   "9") == 0) baudrate = 9600;
      else if (strcmp(arg,  "19") == 0) baudrate = 19200;
      else if (strcmp(arg,  "38") == 0) baudrate = 38400;
      else if (strcmp(arg,  "57") == 0) baudrate = 57600;
      else if (strcmp(arg, "115") == 0) baudrate = 115200;
      else command = CMD_USAGE;
    } /* else if */

    /* application type */
    else if (strncmp(arg, "-a", 2) == 0) {
      arg += 2;
      if      (strcmp(arg, "GDB") == 0) appType = APP_GDB;
      else if (strcmp(arg, "ADA") == 0) appType = APP_ADA;
      else if (strcmp(arg, "TOR") == 0) appType = APP_TOR;
      else if (strcmp(arg, "ACC") == 0) appType = APP_ACC;
      else if (strcmp(arg, "STD") == 0) appType = APP_ACC;
      else command = CMD_USAGE;
    } /* else if */

    /* CPU type */
    else if (strncmp(arg, "-t", 2) == 0) {
      arg += 2;
      if      (strcmp(arg, "CPU32")   == 0) cpuType = CPU_CPU32;
      else if (strcmp(arg, "PPC400")  == 0) cpuType = CPU_PPC400;
      else if (strcmp(arg, "MPC500")  == 0) cpuType = CPU_MPC800;
      else if (strcmp(arg, "PPC600")  == 0) cpuType = CPU_PPC600;
      else if (strcmp(arg, "PPC700")  == 0) cpuType = CPU_PPC600;
      else if (strcmp(arg, "MPC800")  == 0) cpuType = CPU_MPC800;
      else if (strcmp(arg, "MPC8200") == 0) cpuType = CPU_PPC600;
      else if (strcmp(arg, "MPC8300") == 0) cpuType = CPU_PPC600;
      else if (strcmp(arg, "MPC7400") == 0) cpuType = CPU_PPC600;
      else if (strcmp(arg, "ARM")     == 0) cpuType = CPU_ARM;
      else if (strcmp(arg, "TRICORE") == 0) cpuType = CPU_TRICORE;
      else if (strcmp(arg, "MCF")     == 0) cpuType = CPU_MCF;
      else if (strcmp(arg, "HC12")    == 0) cpuType = CPU_HC12;
      else if (strcmp(arg, "MCORE")   == 0) cpuType = CPU_MCORE;
      else if (strcmp(arg, "MPC7450") == 0) cpuType = CPU_MPC7450;
      else if (strcmp(arg, "MPC8641") == 0) cpuType = CPU_MPC7450;
      else if (strcmp(arg, "MIPS32")  == 0) cpuType = CPU_R4K;
      else if (strcmp(arg, "MIPS")    == 0) cpuType = CPU_R4K;
      else if (strcmp(arg, "XSCALE")  == 0) cpuType = CPU_XSCALE;
      else if (strcmp(arg, "MIPS64")  == 0) cpuType = CPU_R5K;
      else if (strcmp(arg, "MPC8500") == 0) cpuType = CPU_MPC8500;
      else if (strcmp(arg, "PQ3")     == 0) cpuType = CPU_MPC8500;
      else if (strcmp(arg, "P2020")   == 0) cpuType = CPU_MPC8500;
      else if (strcmp(arg, "P1020")   == 0) cpuType = CPU_MPC8500;
      else if (strcmp(arg, "ARM11")   == 0) cpuType = CPU_ARM11;
      else if (strcmp(arg, "MPC5500") == 0) cpuType = CPU_MPC5500;
      else if (strcmp(arg, "PA6T")    == 0) cpuType = CPU_PA6T;
      else if (strcmp(arg, "ARMSWD")  == 0) cpuType = CPU_ARMSWD;
      else if (strcmp(arg, "XLS")     == 0) cpuType = CPU_XLS;
      else if (strcmp(arg, "XLR")     == 0) cpuType = CPU_XLS;
      else if (strcmp(arg, "P4080")   == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "QP4")     == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "P5020")   == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "QP5")     == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "P3041")   == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "QP3")     == 0) cpuType = CPU_P4080;
      else if (strcmp(arg, "ARMV8")   == 0) cpuType = CPU_ARMV8;
      else if (strcmp(arg, "SWDV8")   == 0) cpuType = CPU_SWDV8;
      else command = CMD_USAGE;
    } /* else if */

    /* firmware directory */
    else if (strncmp(arg, "-d", 2) == 0) {
      arg += 2;
      strcpy(dir, arg);
    } /* else if */

    /* BDI ip address */
    else if (strncmp(arg, "-i", 2) == 0) {
      arg += 2;
      strcpy(ip, arg);
    } /* else if */

    /* host ip address */
    else if (strncmp(arg, "-h", 2) == 0) {
      arg += 2;
      strcpy(host, arg);
    } /* else if */

    /* subnet mask */
    else if (strncmp(arg, "-m", 2) == 0) {
      arg += 2;
      strcpy(mask, arg);
    } /* else if */

    /* gateway ip address */
    else if (strncmp(arg, "-g", 2) == 0) {
      arg += 2;
      strcpy(gate, arg);
    } /* else if */

    /* configuration file */
    else if (strncmp(arg, "-f", 2) == 0) {
      arg += 2;
      strcpy(file, arg);
    } /* else if */

    /* exit loader and start firmware */
    else if (strncmp(arg, "-s", 2) == 0) {
      start = TRUE;
    } /* else if */

    /* invalid parameter */
    else {
      command = CMD_USAGE;
    } /* else */

  } /* for */


  /* get firmware type based on CPU and application */
  fwType = AppCpuToFw[appType][cpuType];
  if (fwType < 0) command = CMD_USAGE;


  /* execute command */
  switch (command) {

  case CMD_VERSION:
    result = BDI_DisplayVersion(port, baudrate, start);
    break;

  case CMD_ERASE:
    result = BDI_EraseFirmwareLogic(port, baudrate);
    break;

  case CMD_UPDATE:
    result = BDI_UpdateFirmwareLogic(port, baudrate, dir, fwType, BDI_UPDATE_AUTO);
    break;

  case CMD_CONFIG:
    result = BDI_UpdateConfig(port, baudrate, host, ip, mask, gate, file);
    break;

  default:
    result = BDI_OKAY;
    printf("Usage of BDI setup program V1.27:\n");
    printf("bdisetup -v [-pP] [-bB] [-s]\n");
    printf("  -v  Read current versions\n");
    printf("   P  Port (/dev/ttyS0) or IP address\n");
    printf("   B  Baudrate 9, 19, 38, 57 or 115\n");
    printf("  -s  if present, exit loader and start firmware\n");
    printf("\n");
    printf("bdisetup -e [-pP] [-bB]\n");
    printf("  -e  Erase firmware and logic\n");
    printf("   P  Port (/dev/ttyS0) or IP address\n");
    printf("   B  Baudrate 9, 19, 38, 57 or 115\n");
    printf("\n");
    printf("bdisetup -u [-pP] [-bB] [-aA] [-tT] [-dD]\n");
    printf("  -u  Update firmware and/or logic\n");
    printf("   P  Port (/dev/ttyS0) or IP address\n");
    printf("   B  Baudrate 9, 19, 38, 57 or 115\n");
    printf("   A  Application type STD,GDB,ADA,TOR,ACC\n");
    printf("   T  Target type: PPC400,MPC500,MPC5500,PPC600,PPC700,MPC800\n");
    printf("                   MPC7400,MPC7450,MPC8200,MPC8300,MPC8500,PQ3,P2020,MPC8641\n");
    printf("                   ARM,ARM11,ARMSWD,ARMV8,SWDV8,XSCALE,MIPS,MIPS64,XLS,XLR\n");
    printf("                   CPU32,MCF,HC12,MCORE,P3041,P4080,P5020,QP3,QP4,QP5\n");
    printf("   D  Directory with the firmware/logic files\n");
    printf("\n");
    printf("bdisetup -c [-pP] [-bB] [-iI] [-hH] [-mM] [-gG] [-fF]\n");
    printf("  -c  Program network configuration\n");
    printf("   P  Port (/dev/ttyS0) or IP address\n");
    printf("   B  Baudrate 9, 19, 38, 57 or 115\n");
    printf("   I  BDI IP address e.g. 100.100.100.100\n");
    printf("   H  Host IP address\n");
    printf("   M  Subnet mask (default: 255.255.255.255)\n");
    printf("   G  Gateway IP address (default: 255.255.255.255)\n");
    printf("   F  Configuration file name\n");
    printf("\n");
    break;
  } /* switch */

  exit(result);
}


