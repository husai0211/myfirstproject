/*************************************************************************
|  COPYRIGHT (c) 2000 BY ABATRON AG
|*************************************************************************
|
|  PROJECT NAME: BDI Communication Driver
|  FILENAME    : bdilnk.c
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
|*************************************************************************/

/*************************************************************************
|  INCLUDES
|*************************************************************************/

#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "bdierror.h"
#include "bdicmd.h"
#include "bdidll.h"


/*************************************************************************
|  DEFINES
|*************************************************************************/

#define DLE     16
#define STX      2
#define ETX      3

#define NET_TRANSFER_TIMEOUT            100

/* Frame control */
#define FRAME_COUNT_FIELD               (3<<6)
#define FRAME_LENGTH_MASK               7
#define FRAME_TYPE_MASK                 (7<<3)
#define FRAME_LNK_TYPE                  (0<<3)
#define FRAME_ATT_TYPE                  (1<<3)
#define FRAME_STD_TYPE                  (2<<3)

/* link management commands */
#define LNK_RESET                       1
#define LNK_ECHO                        2
#define LNK_SET_BAUDRATE                3


/*************************************************************************
|  MACROS
|*************************************************************************/

/*************************************************************************
|  TYPEDEFS
|*************************************************************************/

typedef struct {BOOL    connected;
                BOOL    asynConnection;
                int     lastError;      /* store last error until reopen      */
                int     fd;
                DWORD   asynBaudrate;
                BYTE    frameType;
                BYTE    frameCount;
                DWORD   repeatCount;
               } BDI_ChannelT;


/*************************************************************************
|  LOCALS
|*************************************************************************/

/* channel info */
static  BDI_ChannelT    channelInfo;

/* frame buffers */
static  BYTE            txFrame[BDI_MAX_FRAME_SIZE];
static  BYTE            rxFrame[BDI_MAX_FRAME_SIZE];


/****************************************************************************
 ****************************************************************************

    BDI_Append___ :

    Host independent helper function to append a numeric value to a buffer.
    The bytes will be stored in the link format order (Motorola byte order).

     INPUT  : value     value
              buffer    pointer to buffer
     OUTPUT : RETURN    pointer to next byte after the stored value
     INOUT  :

 ****************************************************************************/

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
    BDI_DoDelay
    Helper function to delay an amount of time

     INPUT:  delay      the delay time in ms
     OUTPUT: -
 ****************************************************************************/

void BDI_DoDelay(DWORD delay)
{
  usleep(1000 * delay);
} /* BDI_DoDelay */


/****************************************************************************
 ****************************************************************************
    BDI_IPAddrMotorola
    BDI_IPAddrIntel
    Helper function to convert an IP address.

     INPUT:  ipAddress      the IP address as string (e.g. "151.120.25.101")
     OUTPUT: RETURN         the IP address as DWORD
 ****************************************************************************/

DWORD BDI_IPAddrMotorola(const char* ipAddress)
{
  DWORD     netAddr;
  char      number[4];
  char      c;
  int       fields;
  int       digits;
  int       value;

  /* get the four numbers */
  netAddr = 0;
  fields  = 0;
  for (;;) {
    if (fields == 4) return INADDR_NONE;
    digits = 0;
    c = *ipAddress++;
    while (isdigit(c)) {
      if (digits < 3) {
        number[digits++] = c;
      } /* if */
      else return INADDR_NONE;
      c = *ipAddress++;
    } /* while */
    number[digits] = 0;
    value = atoi(number);
    if ((value < 0) || (value > 255)) return INADDR_NONE;
    netAddr <<= 8;
    netAddr += (DWORD)value;
    fields++;
    if ((c != '.') && (c != 0)) return INADDR_NONE;
    if (c == 0) break;
  } /* while */
  if (fields != 4) return INADDR_NONE;
  return netAddr;
} /* BDI_IPAddrMotorola */


DWORD BDI_IPAddrIntel(const char* ipAddress)
{
  DWORD     netAddr;
  char      number[4];
  char      c;
  int       fields;
  int       digits;
  int       value;

  /* get the four numbers */
  netAddr = 0;
  fields  = 0;
  for (;;) {
    if (fields == 4) return INADDR_NONE;
    digits = 0;
    c = *ipAddress++;
    while (isdigit(c)) {
      if (digits < 3) {
        number[digits++] = c;
      } /* if */
      else return INADDR_NONE;
      c = *ipAddress++;
    } /* while */
    number[digits] = 0;
    value = atoi(number);
    if ((value < 0) || (value > 255)) return INADDR_NONE;
    netAddr >>= 8;
    netAddr += ((DWORD)value << 24);
    fields++;
    if ((c != '.') && (c != 0)) return INADDR_NONE;
    if (c == 0) break;
  } /* while */
  if (fields != 4) return INADDR_NONE;
  return netAddr;
} /* BDI_IPAddrIntel */


/****************************************************************************
 ****************************************************************************
                Asynchronous Communication Functions
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
    SetBaudrate

     INPUT:  channel        pointer to channel info
             baudrate       sets the baudrate of the communication port
     OUTPUT: return         error code
 ****************************************************************************/

static int AsynSetBaudrate(BDI_ChannelT* channel, DWORD baudrate)
{
  struct termios        tios;
  speed_t               speed;

  /* get terminal attributes */
  if (tcgetattr(channel->fd, &tios) < 0) return BDI_ASYN_SETUP;

  /* set new baudrate */
  channel->asynBaudrate = baudrate;
  if      (baudrate ==   9600) speed = B9600;
  else if (baudrate ==  19200) speed = B19200;
  else if (baudrate ==  38400) speed = B38400;
  else if (baudrate ==  57600) speed = B57600;
  else if (baudrate == 115200) speed = B115200;
  else                         speed = B9600;
  cfsetospeed(&tios, speed);
  cfsetispeed(&tios, speed);

  /* set terminal attributes */
  if (tcsetattr(channel->fd, TCSAFLUSH, &tios) < 0) return BDI_ASYN_SETUP;

  return BDI_OKAY;
} /* AsynSetBaudrate */


/****************************************************************************
    Flush the receive queue of the communication port

     INPUT:  channel    pointer to channel info
     OUTPUT:
 ****************************************************************************/

static void AsynFlushRxQueue(BDI_ChannelT* channel)
{
  tcflush(channel->fd, TCIFLUSH);
} /* AsynFlushRxQueue */


/****************************************************************************
    AsynOpen
    Opens the asynchronous communication channel

     INPUT:  channel        pointer to channel info
             port           the communication port e.g. "COM1"
     OUTPUT: return         error code
 ****************************************************************************/

static int AsynOpen(BDI_ChannelT* channel, const char* port)
{
  int                   fd;
  struct termios        tios;

  /* open device */
  fd = open(port, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if (fd < 0) {
    perror("Error opening serial device");
    if (errno == EACCES) {
      fprintf(stderr, "Root permissions may be required to open %s\n", port);
    } /* if */
    return BDI_ASYN_SETUP;
  } /* if */
  channel->fd = fd;

  /* get terminal attributes */
  if (tcgetattr(fd, &tios) < 0) return BDI_ASYN_SETUP;

  /* set terminal attributes */
  channel->asynBaudrate = 9600;
  tios.c_iflag = 0;
  tios.c_oflag = 0;
  tios.c_cflag = CLOCAL | CREAD | CS8;
  tios.c_lflag = 0;
  tios.c_cc[VMIN]  = 0;
  tios.c_cc[VTIME] = 1;
  cfsetospeed(&tios, B9600);
  cfsetispeed(&tios, B9600);
  if (tcsetattr(fd, TCSAFLUSH, &tios) < 0) return BDI_ASYN_SETUP;

  return BDI_OKAY;
} /* AsynOpen */


/****************************************************************************
    Closes the communication port

     INPUT:  channel    pointer to channel info
     OUTPUT:
 ****************************************************************************/

static void AsynClose(BDI_ChannelT* channel)
{
  close(channel->fd);
} /* AsynClose */


/****************************************************************************
    Reads a byte from the communication port

     INPUT:  channel        pointer to channel info
             timeout        timeout in milliseconds
     OUTPUT: c              received byte
             return         0 = okay, else error
 ****************************************************************************/

static int AsynReadChar(BDI_ChannelT* channel, BYTE *c, DWORD timeout)
{
  struct tms    tbuf;
  DWORD         startTime;

  if (read(channel->fd, c, 1) == 1) return BDI_OKAY; /* high speed exit */
  timeout = (timeout * sysconf(_SC_CLK_TCK)) / 1000L;
  startTime = times(&tbuf);
  for (;;) {
    if ((times(&tbuf) - startTime) > timeout)            return BDI_ASYN_RX_TIMEOUT;
    if (read(channel->fd, c, 1) == 1)                    return BDI_OKAY;
  }
} /* AsynReadChar */


/****************************************************************************
    Writes a block to the communication port

     INPUT:  channel        pointer to channel info
             count          number of bytes to send
             data           the data to send
     OUTPUT: return         0 = okay, else error

 ****************************************************************************/

static int AsynWriteBlock(BDI_ChannelT* channel, int count, BYTE* data)
{
  int written;

  while (count > 0) {
    written = write(channel->fd, data, count);
    if (written > 0) {
      count -= written;
      data  += written;
    } /* if */
    else if ((written == -1) && (errno == EAGAIN)) {
      continue;
    } /* else if */
    else if (written < 0) {
      return BDI_ASYN_TX_ERROR;
    } /* else */
  } /* while */
  return BDI_OKAY;
} /* AsynWriteBlock */


/****************************************************************************
    Sends a BDI frame

     INPUT:  channel        pointer to channel info
             count          number of bytes to send
             frame          the frame to send
     OUTPUT: return         0 = okay, else error

     OUTPUT:
 ****************************************************************************/

static int AsynSendFrame(BDI_ChannelT* channel, int count, BYTE* frame)
{
  BYTE  bcc;
  BYTE  txChar;
  BYTE* buffer;
  BYTE  asynTxBuffer[BDI_MAX_FRAME_SIZE];

  buffer = asynTxBuffer;

  /* send start sequence */
  *buffer++ = DLE;
  *buffer++ = STX;

  /* send frame data */
  bcc = 0;
  while (count--) {
    txChar    = *frame++;
    *buffer++ = txChar;
    bcc      ^= txChar;
    if (txChar == DLE) *buffer++ = DLE;
  } /* while */

  /* send end sequence */
  *buffer++ = DLE;
  *buffer++ = ETX;
  *buffer++ = bcc;
  if (bcc == DLE) *buffer++ = DLE;

  /* send prepared frame */
  return AsynWriteBlock(channel, buffer - asynTxBuffer, asynTxBuffer);
} /* AsynSendFrame */


/****************************************************************************
    Gets a BDI frame

     INPUT:  channel        pointer to channel info
             count          the maximal number of byte to receive
             timeout        the maximal time to wait for the frame in ms
     OUTPUT: frame          the received frame
             return         the size of the received frame or error

     OUTPUT:
 ****************************************************************************/

static int AsynWaitFrame(BDI_ChannelT* channel, int count, BYTE* frame, DWORD timeout)
{
  BYTE  bcc;
  BYTE* putPtr;
  int   result;
  int   rxCount;
  BYTE  rxChar;

  /* wait for start sequence */
  for (;;) {
    result = AsynReadChar(channel, &rxChar, timeout);
    if (result == BDI_ASYN_RX_TIMEOUT) return BDI_ASYN_RX_TIMEOUT;
    if ((result == BDI_OKAY) && (rxChar == DLE)) {
      result = AsynReadChar(channel, &rxChar, timeout);
      if (result == BDI_ASYN_RX_TIMEOUT) return BDI_ASYN_RX_TIMEOUT;
      if ((result == BDI_OKAY) && (rxChar == STX)) break;
    } /* if */
  } /* for */

  /* receive frame data */
  bcc        = 0;
  rxCount    = 0;
  putPtr     = frame;
  for (;;) {

    /* get next char */
    result = AsynReadChar(channel, &rxChar, timeout);
    if (result != BDI_OKAY) return result;

    /* process link escape char */
    if (rxChar == DLE) {
      result = AsynReadChar(channel, &rxChar, timeout);
      if (result != BDI_OKAY) return result;

      /* process end sequence */
      if (rxChar == ETX) {
        result = AsynReadChar(channel, &rxChar, timeout);
        if (result != BDI_OKAY) return result;
        if (rxChar == DLE) {
          result = AsynReadChar(channel, &rxChar, timeout);
          if (result != BDI_OKAY) return result;
          if (rxChar != DLE)      return BDI_ASYN_RX_FORMAT;
        } /* if */
        if (rxChar == bcc) return rxCount;
        else               return BDI_ASYN_RX_BCC;
      } /* if */
      else if (rxChar != DLE) return BDI_ASYN_RX_FORMAT;
    } /* if */

    /* store data in receive buffer */
    if (rxCount < count) {
      bcc      ^= rxChar;
      *putPtr++ = rxChar;
      rxCount++;
    } /* if */
    else return BDI_ASYN_RX_OVERFLOW;
  } /* for */

} /* AsynWaitFrame */


/****************************************************************************
    Reset Link
    Search the current baudrate, reset the link and set new baudrate.

     INPUT:  channel        pointer to channel info
             baudrate       the requested baudrate
     OUTPUT: return         error code

     OUTPUT:
 ****************************************************************************/

#define NBR_OF_BAUDRATES     5

static int AsynResetLink(BDI_ChannelT* channel, DWORD baudrate)
{
  static const DWORD rateTable[NBR_OF_BAUDRATES] = {9600,19200,38400,57600,115200};

  int   topRate;
  int   rate;
  int   rxCount;
  int   txCount;
  int   result;
  BYTE* txPtr;
  BYTE* rxPtr;
  BYTE  slipEsc[2];
  DWORD confirmedRate;

  /* check if 115200 is supported */
  topRate = NBR_OF_BAUDRATES - 1;
  if (AsynSetBaudrate(channel, rateTable[topRate]) != BDI_OKAY) topRate--;

  /* adjust requested rate if too high */
  if (baudrate > rateTable[topRate]) baudrate = rateTable[topRate];
  result = AsynSetBaudrate(channel, baudrate);

  /* try to connect with requested baudrate */
  txPtr    = txFrame;
  *txPtr++ = FRAME_LNK_TYPE;
  *txPtr++ = 1;
  *txPtr++ = LNK_RESET;
  txCount  = txPtr - txFrame;
  rxCount  = 0;
  if (result == BDI_OKAY) result  = AsynSendFrame(channel, txCount, txFrame);
  if (result == BDI_OKAY) rxCount = AsynWaitFrame(channel, sizeof rxFrame, rxFrame, 200);
  if (rxCount == txCount) return BDI_OKAY;

  /* try all baudrates */
  for (rate=0; rate<=topRate; rate++) {
    result = AsynSetBaudrate(channel, rateTable[rate]);
    if (result == BDI_OKAY) result  = AsynWriteBlock(channel, sizeof slipEsc, slipEsc);
    BDI_DoDelay(300);
    if (result == BDI_OKAY) result  = AsynSendFrame(channel, txCount, txFrame);
    if (result == BDI_OKAY) rxCount = AsynWaitFrame(channel, sizeof rxFrame, rxFrame, 200);
    if (rxCount == txCount) break;
  } /* for */
  if (rxCount != txCount) return BDI_ERR_NO_RESPONSE;

  /* change baudrate */
  txPtr    = txFrame;
  *txPtr++ = FRAME_LNK_TYPE;
  *txPtr++ = 5;
  *txPtr++ = LNK_SET_BAUDRATE;
  txPtr    = BDI_AppendLong(baudrate, txPtr);
  txCount  = txPtr - txFrame;
  rxCount  = 0;
  result = AsynSendFrame(channel, txCount, txFrame);
  if (result == BDI_OKAY) rxCount = AsynWaitFrame(channel, sizeof rxFrame, rxFrame, 200);
  if (rxCount != txCount) return BDI_ASYN_SETUP;

  /* give BDI time to change baudrate */
  BDI_DoDelay(300);

  /* extract confirmed baudrate */
  rxPtr = BDI_ExtractLong(&confirmedRate, rxFrame+3);

  /* check if success */
  txPtr    = txFrame;
  *txPtr++ = FRAME_LNK_TYPE;
  *txPtr++ = 1;
  *txPtr++ = LNK_RESET;
  txCount  = txPtr - txFrame;
  rxCount  = 0;
  result = AsynSetBaudrate(channel, confirmedRate);
  if (result == BDI_OKAY) result  = AsynSendFrame(channel, txCount, txFrame);
  if (result == BDI_OKAY) rxCount = AsynWaitFrame(channel, sizeof rxFrame, rxFrame, 200);
  if (rxCount == txCount) return BDI_OKAY;

} /* AsynResetLink */


/****************************************************************************
 ****************************************************************************
                Network Communication Functions
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
    NetOpen
    Opens a TCP/IP socket

     INPUT:  szAddr         the ip address as string
             port           the ip port
     OUTPUT: return         error code
 ****************************************************************************/

static int NetOpen(BDI_ChannelT* channel, const char* szAddr, WORD port)
{
  struct    sockaddr_in   bdiNetAddr;
  int       err;
  DWORD     ipAddr;
  struct    hostent *pHost;  /* pointer to host info for remote host */


  /* first check to see if the host name was passed in as dot notation */
  /* A return value of INADDR_NONE means inet_addr() failed, so try gethostbyname() */
  ipAddr = inet_addr(szAddr);
  if (ipAddr == INADDR_NONE) {
    pHost = gethostbyname(szAddr);
    if (pHost != NULL) {
      ipAddr = ((struct in_addr *)(pHost->h_addr))->s_addr;
    } /* if */
  } /* if */

  /* return with error if no host information */
  if (ipAddr == INADDR_NONE) {
    return BDI_ERR_INVALID_PARAMETER;
  } /* if */

  /* open socket */
  channel->fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (channel->fd == -1) {
    return BDI_SOCKET_ERROR;
  } /* if */

  /* open connection */
  bdiNetAddr.sin_family      = AF_INET;
  bdiNetAddr.sin_port        = htons(port);
  bdiNetAddr.sin_addr.s_addr = ipAddr;
  err = connect(channel->fd, (struct sockaddr*)&bdiNetAddr, sizeof bdiNetAddr);
  if (err == -1) {
    err = close(channel->fd);
    channel->fd = -1;
    return BDI_SOCKET_ERROR;
  }

  return BDI_OKAY;
} /* NetOpen */

/****************************************************************************
    Closes the TCP/IP socket

     INPUT:  -
     OUTPUT:
 ****************************************************************************/

static void NetClose(BDI_ChannelT* channel)
{
  int       err;

  if (channel->fd != -1) {
    err = close(channel->fd);
  } /* if */
} /* NetClose */


/****************************************************************************
    Sends a BDI frame

     INPUT:  count          number of bytes to send
             frame          the frame to send
     OUTPUT: return         0 = okay, else error

     OUTPUT:
 ****************************************************************************/

static int NetSendFrame(BDI_ChannelT* channel, int count, BYTE* frame)
{
  int result;

  result = send(channel->fd, frame, count, 0);
  if (result != count) return BDI_SOCKET_ERROR;
  else                 return BDI_OKAY;
} /* NetSendFrame */


/****************************************************************************
    Gets a BDI frame

     INPUT:  count          the maximal number of byte to receive
             timeout        the maximal time to wait for the frame in ms
     OUTPUT: frame          the received frame
             return         the size of the received frame or error

     OUTPUT:
 ****************************************************************************/

static int NetWaitFrame(BDI_ChannelT* channel, int count, BYTE* frame, DWORD timeout)
{
  int            rxCount;
  fd_set         readset;
  struct timeval sockTimeout;

  sockTimeout.tv_sec  = timeout / 1000;
  sockTimeout.tv_usec = (timeout % 1000) * 1000;
  FD_ZERO(&readset);
  FD_SET(channel->fd, &readset);
  rxCount = select(channel->fd + 1, &readset, NULL, NULL, &sockTimeout);
  if (rxCount > 0) {
    rxCount = recv(channel->fd, frame, count, 0);
    if (rxCount !=  -1) return rxCount;
    else                return BDI_SOCKET_ERROR;
  } /* if */
  else if (rxCount == 0) {
    return BDI_SOCKET_RX_TIMEOUT;
  } /* else if */
  else {
    return BDI_SOCKET_ERROR;
  } /* else */
} /* NetWaitFrame */


/****************************************************************************
    Reset Link

     INPUT:  -
     OUTPUT: return         error code

 ****************************************************************************/

static int NetResetLink(BDI_ChannelT* channel)
{
  int   result;
  int   rxCount;
  int   txCount;
  int   repeats;
  BYTE* txPtr;

  txPtr    = txFrame;
  *txPtr++ = FRAME_LNK_TYPE;
  *txPtr++ = 1;
  *txPtr++ = LNK_RESET;
  txCount  = txPtr - txFrame;
  repeats = 0;
  while (repeats++ < 6) {
    rxCount = 0;
    result  = NetSendFrame(channel, txCount, txFrame);
    if (result == BDI_OKAY) rxCount = NetWaitFrame(channel, sizeof rxFrame, rxFrame, 500);
    if (rxCount == txCount) return BDI_OKAY;
  } /* while */
  return BDI_ERR_NO_RESPONSE;
} /* NetResetLink */


/****************************************************************************
 ****************************************************************************
                Common Communication Functions
 ****************************************************************************
 ****************************************************************************/


/****************************************************************************
 ****************************************************************************

    BDI_Open:

     Opens the connection to the BDI.

     INPUT  : port          a string with the port name (e.g. /dev/com1)
              baudrate      the baudrate to connect
     OUTPUT : RETURN        0 if okay or a negativ number if error

 ****************************************************************************/

int BDI_Open(const char* port, DWORD baudrate)
{
  int           result;

  /* asyn channel */
  if (strncmp(port, "/dev", 4) == 0) {
    channelInfo.asynConnection = TRUE;
    result = AsynOpen(&channelInfo, port);
    if (result != BDI_OKAY) return result;
    result = AsynResetLink(&channelInfo, baudrate);
    if (result != BDI_OKAY) {
      AsynClose(&channelInfo);
      return result;
    } /* if */
  } /* if */

  /* network channel */
  else {
    channelInfo.asynConnection = FALSE;
    result = NetOpen(&channelInfo, port, 2001);
    if (result != BDI_OKAY) return result;
    result = NetResetLink(&channelInfo);
    if (result != BDI_OKAY) {
      NetClose(&channelInfo);
      return result;
    } /* if */
  } /* else */

  channelInfo.connected     = TRUE;
  channelInfo.lastError     = BDI_OKAY;
  channelInfo.frameCount    = 0;
  channelInfo.repeatCount   = 0;
  channelInfo.frameType     = FRAME_STD_TYPE;
  return BDI_OKAY;
} /* BDI_Open */


/****************************************************************************
 ****************************************************************************

    BDI_Close:

     Closes the connection to the BDI.

     INPUT  : -
     OUTPUT : -

 ****************************************************************************/

void BDI_Close(void)
{
  if (channelInfo.connected) {
    if (channelInfo.asynConnection) AsynClose(&channelInfo);
    else                            NetClose(&channelInfo);
    channelInfo.connected = FALSE;
  } /* if */
} /* BDI_Close */


/****************************************************************************
 ****************************************************************************

    BDI_Transaction:

     Executes a command / answer transaction with the BDI
     commandTime: The time the command needs to execute, not the transfer time.
                  The time is used to calculate the timeout before the command
                  is repeated.

     INPUT  : commandLength the length of the command block
              commandData   the command <code,parameter>
              answerSize    the size of the answer buffer
              commandTime   the time in ms the command needs to execute
     OUTPUT : answerData    the answer <code,parameter>
              RETURN        the length of the answer block
                            or a negativ number if error.

 ****************************************************************************/

int  BDI_Transaction(      int       commandLength,
                     const void     *commandData,
                           int       answerSize,
                           void     *answerData,
                           DWORD     commandTime)
{
        BYTE*   framePtr;
  const BYTE*   commandPtr;
        BYTE*   answerPtr;
        BYTE    frameControl;
        int     txFrameLength;
        int     rxFrameLength;
        int     sendCount;
        int     result;
        int     rxCount;
        DWORD   answerTimeout;
        BOOL    sendFrame;

  /* check if everthing is okay */
  result = BDI_OKAY;
  if (!channelInfo.connected)                    result = BDI_ERR_NOT_CONNECTED;
  else if (channelInfo.lastError != BDI_OKAY)    result = channelInfo.lastError;
  else if (commandLength > (sizeof txFrame - 2)) result = BDI_ERR_INVALID_PARAMETER;
  if (result != BDI_OKAY) {
	return result;
  } /* if */

  /* build frame */
  txFrameLength = commandLength + 2;
  framePtr      = txFrame;
  frameControl  = (BYTE)(channelInfo.frameType | (commandLength>>8));
  frameControl |= (channelInfo.frameCount<<6);
  channelInfo.frameCount++;

  commandPtr  = (const BYTE*)commandData;
  *framePtr++ = frameControl;
  *framePtr++ = (BYTE)commandLength;
  while (commandLength--) *framePtr++ = *commandPtr++;

  /* do transaction */
  sendCount = 0;
  sendFrame = TRUE;
  rxCount   = 0;
  do {

    /* send command frame */
    if (sendFrame) {
      if (channelInfo.asynConnection) {
        result = AsynSendFrame(&channelInfo, txFrameLength, txFrame);
      } /* if */
      else {
        result = NetSendFrame(&channelInfo, txFrameLength, txFrame);
      } /* else */
      sendCount++;
    } /* if */

    /* wait for answer */
    if (result == BDI_OKAY) {
      if (channelInfo.asynConnection) {
        answerTimeout = (DWORD)(txFrameLength + 1500);  /* total number of characters */
        answerTimeout = answerTimeout * 10000L / channelInfo.asynBaudrate + commandTime + 200L;
        rxFrameLength = AsynWaitFrame(&channelInfo, sizeof rxFrame, rxFrame, answerTimeout);
      } /* if */
      else {
        answerTimeout = NET_TRANSFER_TIMEOUT + commandTime;
        rxFrameLength = NetWaitFrame(&channelInfo, sizeof rxFrame, rxFrame, answerTimeout);
      } /* else */

      /* check if attention frame received */
      if ((rxFrameLength == 3) && (rxFrame[0] == FRAME_ATT_TYPE) &&  (rxFrame[1] == 1)) {
        sendFrame = !sendFrame;  /* recover from lost command frame */
      } /* if */

      /* check received frame */
      else if (    (rxFrameLength > 2)
                && ((frameControl & FRAME_COUNT_FIELD) == (rxFrame[0] & FRAME_COUNT_FIELD))
              ) {
        rxFrameLength -= 2;
        rxCount = 256 * (rxFrame[0] & FRAME_LENGTH_MASK) + rxFrame[1];
        if (rxFrameLength == rxCount) {
          if (rxCount <= answerSize) {
            framePtr  = rxFrame + 2;
            answerPtr = (BYTE*)answerData;
            while (rxFrameLength--) *answerPtr++ = *framePtr++;
          } /* if */
        } /* if */
        else {
          rxCount = 0;
          channelInfo.repeatCount++;
          sendFrame = TRUE;
        } /* else */
      } /* if */

      /* discard wrong frame */
      else if (rxFrameLength > 0) {
        sendFrame = FALSE;
      } /* else if */

      else {
        channelInfo.repeatCount++;
        commandTime += 500;  /* increase timeout */
        sendFrame = TRUE;
      } /* if */

    } /* if */
  } while ((rxCount == 0) && (sendCount < 5));

  if (rxCount > answerSize) {
    return BDI_ERR_ANSWER_TOO_BIG;
  } /* if */
  else if (rxCount > 0) {
    return rxCount;
  } /* else if */
  else {
    if (channelInfo.frameType == FRAME_STD_TYPE) channelInfo.lastError = BDI_ERR_NO_RESPONSE;
    return BDI_ERR_NO_RESPONSE;
  } /* else */
} /* BDI_Transaction */


