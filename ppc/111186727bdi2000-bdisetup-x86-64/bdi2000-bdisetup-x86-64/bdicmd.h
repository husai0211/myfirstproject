
#ifndef __BDICMD_H__
#define __BDICMD_H__

/* maximal command/answer sizes */
#define BDI_MAX_BLOCK_SIZE        1024
#define BDI_MAX_FRAME_SIZE        1500

/* Host commands */
#define BDI_CMD_NO_COMMAND           0
#define BDI_CMD_SET_BYTE             1
#define BDI_CMD_SET_WORD             2
#define BDI_CMD_SET_LONG             3
#define BDI_CMD_SET_BLOCK            4
#define BDI_CMD_FETCH_BYTE           5
#define BDI_CMD_FETCH_WORD           6
#define BDI_CMD_FETCH_LONG           7
#define BDI_CMD_SET_PC               8
#define BDI_CMD_START                9
#define BDI_CMD_HALT                10
#define BDI_CMD_RESET               11
#define BDI_CMD_STATUS_REQUEST      12
#define BDI_CMD_READ_TERMINAL       13
#define BDI_CMD_FETCH_VERSION       14
#define BDI_CMD_FETCH_XSUM          15
#define BDI_CMD_SET_BAUDRATE        16  /* do not use */
#define BDI_CMD_SET_BREAK           17
#define BDI_CMD_GET_BREAK           18
#define BDI_CMD_DELETE_BREAK        19
#define BDI_CMD_DELETE_ALL_BREAKS   20
#define BDI_CMD_GET_ALL_BREAKS      21
#define BDI_CMD_SET_APPL_NAME       22
#define BDI_CMD_GET_APPL_NAME       23
#define BDI_CMD_GET_REGISTERS       24
#define BDI_CMD_GET_BLOCK           25
#define BDI_CMD_GET_PC              26
#define BDI_CMD_SET_WAIT            27
#define BDI_CMD_SINGLE_STEP         28
#define BDI_CMD_SET_REGISTER        29

/* new HICROSS commands */
#define BDI_CMD_SET_HW_BREAK        30  /* set    hardware breakpint (bdiBreak)  */
#define BDI_CMD_DELETE_HW_BREAK     31  /* delete hardware breakpint (bdiBreak)  */
#define BDI_CMD_GET_TERMINAL        32  /* get terminal characters, answer is 69 */

/* fast load commands, do not use as BDI transaction command */
#define BDI_CMD_LOAD_INIT           40
#define BDI_CMD_LOAD_BLOCK          41

/* HICROSS answers */
#define BDI_ANS_NOT_IMPLEMENTED      0
#define BDI_ANS_NAK                 63
#define BDI_ANS_ACK                 64
#define BDI_ANS_SEND_BYTE           65
#define BDI_ANS_SEND_WORD           66
#define BDI_ANS_SEND_LONG           67
#define BDI_ANS_SEND_STATUS         68
#define BDI_ANS_WRITE_TERMINAL      69
#define BDI_ANS_SEND_BREAK          70
#define BDI_ANS_SEND_ALL_BREAK      71
#define BDI_ANS_SEND_APP_NAME       72
#define BDI_ANS_SEND_REGISTERS      73
#define BDI_ANS_SEND_BLOCK          74
#define BDI_ANS_SEND_VERSION        75
#define BDI_ANS_SEND_PC             76

/* loader commands and answers */
#define BDI_LDR_READ_VERSION        100
#define BDI_LDR_READ_MEMORY         101
#define BDI_LDR_WRITE_MEMORY        102
#define BDI_LDR_ERASE_FLASH         103
#define BDI_LDR_PROGRAM_FLASH       104
#define BDI_LDR_ISP_ENABLE          105
#define BDI_LDR_ISP_READ_ID         106
#define BDI_LDR_ISP_READ_LINE       107
#define BDI_LDR_ISP_PROGRAM_LINE    108
#define BDI_LDR_ISP_READ_UES        109
#define BDI_LDR_ISP_PROGRAM_UES     110
#define BDI_LDR_ISP_SET_SECURITY    111
#define BDI_LDR_ISP_ERASE           112
#define BDI_LDR_EXIT_LOADER         113
#define BDI_LDR_START_LOADER        114

/* bdiNet loader commands and answers */
#define BDI_NET_READ_VERSION        120
#define BDI_NET_READ_MEMORY         121
#define BDI_NET_WRITE_MEMORY        122
#define BDI_NET_ERASE_FLASH         123
#define BDI_NET_PROGRAM_FLASH       124
#define BDI_NET_EXIT_LOADER         125
#define BDI_NET_RAM_TEST            126
#define BDI_NET_LAN_TEST            127

/* common non HI-CROSS commands */
#define BDI_CMD_RESET_TARGET        130
#define BDI_CMD_SET_BDM_SPEED       131
#define BDI_CMD_SET_MEM_SPACE       132
#define BDI_CMD_LOAD_SINGLE_BLOCK   133
#define BDI_CMD_DUMP_SINGLE_BLOCK   134
#define BDI_CMD_WRITE_REGISTER      135
#define BDI_CMD_READ_REGISTER       136

/* bdiPro specific commands */
#define BDI_PRO_SET_PRO_MODE        140
#define BDI_PRO_PROGRAM_SETUP       141
#define BDI_PRO_LOAD_PAGE           142
#define BDI_PRO_PROGRAM_PAGE        143
#define BDI_PRO_START_ERASE         144
#define BDI_PRO_GET_ERASE_STATE     145

/* bdiSpy specific commands */
#define BDI_SPY_SET_SPY_MODE        150
#define BDI_SPY_RESET_SPA           151
#define BDI_SPY_SET_RANGES          152
#define BDI_SPY_START_SPA           153
#define BDI_SPY_GET_SPA             154
#define BDI_SPY_START_LOOP          155
#define BDI_SPY_START_FILL          156
#define BDI_SPY_GET_FILL_STATE      157
#define BDI_SPY_STOP_LOOP_FILL      158

#define BDI_SPY_START_TIME_MEAS     160
#define BDI_SPY_STOP_TIME_MEAS      161
#define BDI_SPY_READ_TIME_MEAS      162
#define BDI_BRK_LOAD_DPC8           163

#define BDI_TST_ENTER_TEST_MODE     170
#define BDI_TST_READ_PORT           171
#define BDI_TST_WRITE_PORT          172
#define BDI_TST_PULS_PORT           173
#define BDI_TST_GET_CTS             174
#define BDI_TST_SET_RTS             175
#define BDI_TST_RAM_TEST            176
#define BDI_TST_LEAVE_TEST_MODE     177


#endif
