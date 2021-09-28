/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/

/*****************************************************************************/
/*                                                                           */
/*  File Name         : osal_error.c                                         */
/*                                                                           */
/*  Description       : This file contains all the error code mappings across*/
/*                      platforms.                                           */
/*                                                                           */
/*  List of Functions : get_windows_error                                    */
/*                      get_linux_error                                      */
/*                      get_ti_bios_error                                    */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System includes files */

#include <errno.h>

/* User include files */
#include "cast_types.h"
#include "osal_errno.h"

/*****************************************************************************/
/* Global Variable Definitions                                               */
/*****************************************************************************/

WORD32 osal_errno[] = {
    OSAL_NOERROR,
    OSAL_PERM, /*  EPERM                   */
    OSAL_NOENT, /*  ENOENT                  */
    OSAL_SRCH, /*  ESRCH                   */
    OSAL_INTR, /*  EINTR                   */
    OSAL_IO, /*  EIO                     */
    OSAL_NXIO, /*  ENXIO                   */
    OSAL_2BIG, /*  E2BIG                   */
    OSAL_NOEXEC, /*  ENOEXEC                 */
    OSAL_BADF, /*  EBADF                   */
    OSAL_CHILD, /*  ECHILD                  */
    OSAL_AGAIN, /*  EAGAIN, EDEADLOCK       */
    OSAL_NOMEM, /*  ENOMEM                  */
    OSAL_ACCES, /*  EACCES                  */
    OSAL_FAULT, /*  EFAULT                  */
    OSAL_NOTBLK, /*  ENOTBLK                 */
    OSAL_BUSY, /*  EBUSY                   */
    OSAL_EXIST, /*  EEXIST                  */
    OSAL_XDEV, /*  EXDEV                   */
    OSAL_NODEV, /*  ENODEV                  */
    OSAL_NOTDIR, /*  ENOTDIR                 */
    OSAL_ISDIR, /*  EISDIR                  */
    OSAL_INVAL, /*  EINVAL                  */
    OSAL_NFILE, /*  ENFILE                  */
    OSAL_MFILE, /*  EMFILE                  */
    OSAL_NOTTY, /*  ENOTTY                  */
    OSAL_TXTBSY, /*  ETXTBSY                 */
    OSAL_FBIG, /*  EFBIG                   */
    OSAL_NOSPC, /*  ENOSPC                  */
    OSAL_SPIPE, /*  ESPIPE                  */
    OSAL_ROFS, /*  EROFS                   */
    OSAL_MLINK, /*  EMLINK                  */
    OSAL_PIPE, /*  EPIPE                   */
    OSAL_DOM, /*  EDOM                    */
    OSAL_RANGE, /*  ERANGE                  */
    OSAL_DEADLK, /*  EDEADLK, EDEADLOCK      */
    OSAL_NAMETOOLONG, /*  ENAMETOOLONG            */
    OSAL_NOLCK, /*  ENOLCK                  */
    OSAL_NOSYS, /*  ENOSYS                  */
    OSAL_NOTEMPTY, /*  ENOTEMPTY               */
    OSAL_LOOP, /*  ELOOP                   */
    OSAL_NOERROR,
    OSAL_NOMSG, /*  ENOMSG                  */
    OSAL_IDRM, /*  EIDRM                   */
    OSAL_CHRNG, /*  ECHRNG                  */
    OSAL_L2NSYNC, /*  EL2NSYNC                */
    OSAL_L3HLT, /*  EL3HLT                  */
    OSAL_L3RST, /*  EL3RST                  */
    OSAL_LNRNG, /*  ELNRNG                  */
    OSAL_UNATCH, /*  EUNATCH                 */
    OSAL_NOCSI, /*  ENOCSI                  */
    OSAL_L2HLT, /*  EL2HLT                  */
    OSAL_BADE, /*  EBADE                   */
    OSAL_BADR, /*  EBADR                   */
    OSAL_XFULL, /*  EXFULL                  */
    OSAL_NOANO, /*  ENOANO                  */
    OSAL_BADRQC, /*  EBADRQC                 */
    OSAL_BADSLT, /*  EBADSLT                 */
    OSAL_NOERROR,
    OSAL_BFONT, /*  EBFONT                  */
    OSAL_NOSTR, /*  ENOSTR                  */
    OSAL_NODATA, /*  ENODATA                 */
    OSAL_TIME, /*  ETIME                   */
    OSAL_NOSR, /*  ENOSR                   */
    OSAL_NONET, /*  ENONET                  */
    OSAL_NOPKG, /*  ENOPKG                  */
    OSAL_REMOTE, /*  EREMOTE                 */
    OSAL_NOLINK, /*  ENOLINK                 */
    OSAL_ADV, /*  EADV                    */
    OSAL_SRMNT, /*  ESRMNT                  */
    OSAL_COMM, /*  ECOMM                   */
    OSAL_PROTO, /*  EPROTO                  */
    OSAL_MULTIHOP, /*  EMULTIHOP               */
    OSAL_DOTDOT, /*  EDOTDOT                 */
    OSAL_BADMSG, /*  EBADMSG                 */
    OSAL_OVERFLOW, /*  EOVERFLOW               */
    OSAL_NOTUNIQ, /*  ENOTUNIQ                */
    OSAL_BADFD, /*  EBADFD                  */
    OSAL_REMCHG, /*  EREMCHG                 */
    OSAL_LIBACC, /*  ELIBACC                 */
    OSAL_LIBBAD, /*  ELIBBAD                 */
    OSAL_LIBSCN, /*  ELIBSCN                 */
    OSAL_LIBMAX, /*  ELIBMAX                 */
    OSAL_LIBEXEC, /*  ELIBEXEC                */
    OSAL_ILSEQ, /*  EILSEQ                  */
    OSAL_RESTART, /*  ERESTART                */
    OSAL_STRPIPE, /*  ESTRPIPE                */
    OSAL_USERS, /*  EUSERS                  */
    OSAL_NOTSOCK, /*  ENOTSOCK                */
    OSAL_DESTADDRREQ, /*  EDESTADDRREQ            */
    OSAL_MSGSIZE, /*  EMSGSIZE                */
    OSAL_PROTOTYPE, /*  EPROTOTYPE              */
    OSAL_NOPROTOOPT, /*  ENOPROTOOPT             */
    OSAL_PROTONOSUPPORT, /*  EPROTONOSUPPORT         */
    OSAL_SOCKTNOSUPPORT, /*  ESOCKTNOSUPPORT         */
    OSAL_OPNOTSUPP, /*  EOPNOTSUPP              */
    OSAL_PFNOSUPPORT, /*  EPFNOSUPPORT            */
    OSAL_AFNOSUPPORT, /*  EAFNOSUPPORT            */
    OSAL_ADDRINUSE, /*  EADDRINUSE              */
    OSAL_ADDRNOTAVAIL, /*  EADDRNOTAVAIL           */
    OSAL_NETDOWN, /*  ENETDOWN                */
    OSAL_NETUNREACH, /*  ENETUNREACH             */
    OSAL_NETRESET, /*  ENETRESET               */
    OSAL_CONNABORTED, /*  ECONNABORTED            */
    OSAL_CONNRESET, /*  ECONNRESET              */
    OSAL_NOBUFS, /*  ENOBUFS                 */
    OSAL_ISCONN, /*  EISCONN                 */
    OSAL_NOTCONN, /*  ENOTCONN                */
    OSAL_SHUTDOWN, /*  ESHUTDOWN               */
    OSAL_TOOMANYREFS, /*  ETOOMANYREFS            */
    OSAL_TIMEDOUT, /*  ETIMEDOUT               */
    OSAL_CONNREFUSED, /*  ECONNREFUSED            */
    OSAL_HOSTDOWN, /*  EHOSTDOWN               */
    OSAL_HOSTUNREACH, /*  EHOSTUNREACH            */
    OSAL_ALREADY, /*  EALREADY                */
    OSAL_INPROGRESS, /*  EINPROGRESS             */
    OSAL_STALE, /*  ESTALE                  */
    OSAL_UCLEAN, /*  EUCLEAN                 */
    OSAL_NOTNAM, /*  ENOTNAM                 */
    OSAL_NAVAIL, /*  ENAVAIL                 */
    OSAL_ISNAM, /*  EISNAM                  */
    OSAL_REMOTEIO, /*  EREMOTEIO               */
    OSAL_DQUOT, /*  EDQUOT                  */
    OSAL_NOMEDIUM, /*  ENOMEDIUM               */
    OSAL_MEDIUMTYPE, /*  EMEDIUMTYPE             */
    OSAL_CANCELED, /*  ECANCELED               */
    OSAL_NOKEY, /*  ENOKEY                  */
    OSAL_KEYEXPIRED, /*  EKEYEXPIRED             */
    OSAL_KEYREVOKED, /*  EKEYREVOKED             */
    OSAL_KEYREJECTED, /*  EKEYREJECTED            */
};

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_linux_error                                          */
/*                                                                           */
/*  Description   : This function returns the error code for Redhat Linux    */
/*                  platform.                                                */
/*                                                                           */
/*  Inputs        : None                                                     */
/*  Globals       : None                                                     */
/*                                                                           */
/*  Processing    : Returns OSAL error code if error is a listed OSAL error  */
/*                  code. Or else returns platform depedent error code.      */
/*                                                                           */
/*  Outputs       : Error code                                               */
/*                                                                           */
/*  Returns       : If error is one of OSAL listed error code - OSAL_<ERROR> */
/*                  Else system error code.                                  */
/*                                                                           */
/*  Issues        : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 get_linux_error(void)
{
    /* Under Linux platform, error codes 0 - 130 are supported */
    if(130 > errno)
        return osal_errno[errno];

    return errno;
}
