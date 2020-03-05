#ifndef __BDIERROR_H__
#define __BDIERROR_H__


/* BDI error from communication layer */
#define BDI_OKAY                     0
#define BDI_ASYN_SETUP              -1001
#define BDI_ASYN_RX_TIMEOUT         -1002
#define BDI_ASYN_RX_ERROR           -1003
#define BDI_ASYN_RX_FORMAT          -1004
#define BDI_ASYN_RX_BCC             -1005
#define BDI_ASYN_RX_OVERFLOW        -1006
#define BDI_ASYN_TX_ERROR           -1007

#define BDI_SOCKET_ERROR            -1101
#define BDI_SOCKET_RX_TIMEOUT       -1102

#define BDI_ERR_NO_RESPONSE         -1201
#define BDI_ERR_INVALID_PARAMETER   -1202
#define BDI_ERR_NOT_CONNECTED       -1203
#define BDI_ERR_ANSWER_TOO_BIG      -1204
#define BDI_ERR_INVALID_RESPONSE    -1205
#define BDI_ERR_MEM_ACCESS          -1206
#define BDI_ERR_FILE_ACCESS         -1207

#define BDI_ERR_FLASH_ERASE         -1208 /* cannot erase flash sector              */
#define BDI_ERR_FLASH_PROGRAM       -1209 /* cannot program flash                   */
#define BDI_ERR_VERIFY              -1210 /* verify error                           */
#define BDI_ERR_INVALID_MODE        -1211 /* invalid write mode                     */
#define BDI_ERR_WORKING_RAM         -1212 /* cannot write to working RAM            */

#define BDI_ERR_UNKNOWN_BDI         -1301
#define BDI_ERR_FIRMWARE_FILE       -1302
#define BDI_ERR_FLASH_VERIFY        -1303
#define BDI_ERR_LOGIC_DEVICE        -1304
#define BDI_ERR_LOGIC_VERIFY        -1305
#define BDI_ERR_LOGIC_FILE          -1306


#endif
