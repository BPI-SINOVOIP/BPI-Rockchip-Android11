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
/*  File Name         : osal_errno.h                                         */
/*                                                                           */
/*  Description       : This file error codes supported by OSAL              */
/*                                                                           */
/*  List of Functions : None                                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         30 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef OSAL_ERRNO_H
#define OSAL_ERRNO_H

#define OSAL_SOCKERR_BASE 0x1000

#define OSAL_NOERROR (OSAL_SOCKERR_BASE + 0)
#define OSAL_INTR (OSAL_SOCKERR_BASE + 1)
#define OSAL_BADF (OSAL_SOCKERR_BASE + 2)
#define OSAL_ACCES (OSAL_SOCKERR_BASE + 3)
#define OSAL_FAULT (OSAL_SOCKERR_BASE + 4)
#define OSAL_INVAL (OSAL_SOCKERR_BASE + 5)
#define OSAL_MFILE (OSAL_SOCKERR_BASE + 6)
#define OSAL_WOULDBLOCK (OSAL_SOCKERR_BASE + 7)
#define OSAL_INPROGRESS (OSAL_SOCKERR_BASE + 8)
#define OSAL_ALREADY (OSAL_SOCKERR_BASE + 9)
#define OSAL_NOTSOCK (OSAL_SOCKERR_BASE + 10)
#define OSAL_DESTADDRREQ (OSAL_SOCKERR_BASE + 11)
#define OSAL_MSGSIZE (OSAL_SOCKERR_BASE + 12)
#define OSAL_PROTOTYPE (OSAL_SOCKERR_BASE + 13)
#define OSAL_NOPROTOOPT (OSAL_SOCKERR_BASE + 14)
#define OSAL_PROTONOSUPPORT (OSAL_SOCKERR_BASE + 15)
#define OSAL_SOCKTNOSUPPORT (OSAL_SOCKERR_BASE + 16)
#define OSAL_OPNOTSUPP (OSAL_SOCKERR_BASE + 17)
#define OSAL_PFNOSUPPORT (OSAL_SOCKERR_BASE + 18)
#define OSAL_AFNOSUPPORT (OSAL_SOCKERR_BASE + 19)
#define OSAL_ADDRINUSE (OSAL_SOCKERR_BASE + 20)
#define OSAL_ADDRNOTAVAIL (OSAL_SOCKERR_BASE + 21)
#define OSAL_NETDOWN (OSAL_SOCKERR_BASE + 22)
#define OSAL_NETUNREACH (OSAL_SOCKERR_BASE + 23)
#define OSAL_NETRESET (OSAL_SOCKERR_BASE + 24)
#define OSAL_CONNABORTED (OSAL_SOCKERR_BASE + 25)
#define OSAL_CONNRESET (OSAL_SOCKERR_BASE + 26)
#define OSAL_NOBUFS (OSAL_SOCKERR_BASE + 27)
#define OSAL_ISCONN (OSAL_SOCKERR_BASE + 28)
#define OSAL_NOTCONN (OSAL_SOCKERR_BASE + 29)
#define OSAL_SHUTDOWN (OSAL_SOCKERR_BASE + 30)
#define OSAL_TOOMANYREFS (OSAL_SOCKERR_BASE + 31)
#define OSAL_TIMEDOUT (OSAL_SOCKERR_BASE + 32)
#define OSAL_CONNREFUSED (OSAL_SOCKERR_BASE + 33)
#define OSAL_LOOP (OSAL_SOCKERR_BASE + 34)
#define OSAL_NAMETOOLONG (OSAL_SOCKERR_BASE + 35)
#define OSAL_HOSTDOWN (OSAL_SOCKERR_BASE + 36)
#define OSAL_HOSTUNREACH (OSAL_SOCKERR_BASE + 37)
#define OSAL_NOTEMPTY (OSAL_SOCKERR_BASE + 38)
#define OSAL_PROCLIM (OSAL_SOCKERR_BASE + 39)
#define OSAL_USERS (OSAL_SOCKERR_BASE + 40)
#define OSAL_DQUOT (OSAL_SOCKERR_BASE + 41)
#define OSAL_STALE (OSAL_SOCKERR_BASE + 42)
#define OSAL_REMOTE (OSAL_SOCKERR_BASE + 43)
#define OSAL_SYSNOTREADY (OSAL_SOCKERR_BASE + 44)
#define OSAL_VERNOTSUPPORTED (OSAL_SOCKERR_BASE + 45)
#define OSAL_NOTINITIALISED (OSAL_SOCKERR_BASE + 46)
#define OSAL_DISCON (OSAL_SOCKERR_BASE + 47)
#define OSAL_NOMORE (OSAL_SOCKERR_BASE + 48)
#define OSAL_CANCELLED (OSAL_SOCKERR_BASE + 49)
#define OSAL_INVALIDPROCTABLE (OSAL_SOCKERR_BASE + 50)
#define OSAL_INVALIDPROVIDER (OSAL_SOCKERR_BASE + 51)
#define OSAL_PROVIDERFAILEDINIT (OSAL_SOCKERR_BASE + 52)
#define OSAL_SYSCALLFAILURE (OSAL_SOCKERR_BASE + 53)
#define OSAL_SERVICE_NOT_FOUND (OSAL_SOCKERR_BASE + 54)
#define OSAL_TYPE_NOT_FOUND (OSAL_SOCKERR_BASE + 55)
#define OSAL_E_NO_MORE (OSAL_SOCKERR_BASE + 56)
#define OSAL_E_CANCELLED (OSAL_SOCKERR_BASE + 57)
#define OSAL_REFUSED (OSAL_SOCKERR_BASE + 58)
#define OSAL_HOST_NOT_FOUND (OSAL_SOCKERR_BASE + 59)
#define OSAL_TRY_AGAIN (OSAL_SOCKERR_BASE + 60)
#define OSAL_NO_RECOVERY (OSAL_SOCKERR_BASE + 61)
#define OSAL_NO_DATA (OSAL_SOCKERR_BASE + 62)
#define OSAL_NO_ADDRESS (OSAL_SOCKERR_BASE + 63)
#define OSAL_QOS_RECEIVERS (OSAL_SOCKERR_BASE + 64)
#define OSAL_QOS_SENDERS (OSAL_SOCKERR_BASE + 65)
#define OSAL_QOS_NO_SENDERS (OSAL_SOCKERR_BASE + 66)
#define OSAL_QOS_NO_RECEIVERS (OSAL_SOCKERR_BASE + 67)
#define OSAL_QOS_REQUEST_CONFIRMED (OSAL_SOCKERR_BASE + 68)
#define OSAL_QOS_ADMISSION_FAILURE (OSAL_SOCKERR_BASE + 69)
#define OSAL_QOS_POLICY_FAILURE (OSAL_SOCKERR_BASE + 70)
#define OSAL_QOS_BAD_STYLE (OSAL_SOCKERR_BASE + 71)
#define OSAL_QOS_BAD_OBJECT (OSAL_SOCKERR_BASE + 72)
#define OSAL_QOS_TRAFFIC_CTRL_ERROR (OSAL_SOCKERR_BASE + 73)
#define OSAL_QOS_GENERIC_ERROR (OSAL_SOCKERR_BASE + 74)

/* POSIX Error codes */
#define OSAL_PERM (OSAL_SOCKERR_BASE + 75)
#define OSAL_NOENT (OSAL_SOCKERR_BASE + 76)
#define OSAL_SRCH (OSAL_SOCKERR_BASE + 77)
#define OSAL_IO (OSAL_SOCKERR_BASE + 78)
#define OSAL_NXIO (OSAL_SOCKERR_BASE + 79)
#define OSAL_2BIG (OSAL_SOCKERR_BASE + 80)
#define OSAL_NOEXEC (OSAL_SOCKERR_BASE + 81)
#define OSAL_CHILD (OSAL_SOCKERR_BASE + 82)
#define OSAL_AGAIN (OSAL_SOCKERR_BASE + 83)
#define OSAL_NOMEM (OSAL_SOCKERR_BASE + 84)
#define OSAL_NOTBLK (OSAL_SOCKERR_BASE + 85)
#define OSAL_BUSY (OSAL_SOCKERR_BASE + 86)
#define OSAL_EXIST (OSAL_SOCKERR_BASE + 87)
#define OSAL_XDEV (OSAL_SOCKERR_BASE + 88)
#define OSAL_NODEV (OSAL_SOCKERR_BASE + 89)
#define OSAL_NOTDIR (OSAL_SOCKERR_BASE + 90)
#define OSAL_ISDIR (OSAL_SOCKERR_BASE + 91)
#define OSAL_NFILE (OSAL_SOCKERR_BASE + 92)
#define OSAL_NOTTY (OSAL_SOCKERR_BASE + 93)
#define OSAL_TXTBSY (OSAL_SOCKERR_BASE + 94)
#define OSAL_FBIG (OSAL_SOCKERR_BASE + 95)
#define OSAL_NOSPC (OSAL_SOCKERR_BASE + 96)
#define OSAL_SPIPE (OSAL_SOCKERR_BASE + 97)
#define OSAL_ROFS (OSAL_SOCKERR_BASE + 98)
#define OSAL_MLINK (OSAL_SOCKERR_BASE + 99)
#define OSAL_PIPE (OSAL_SOCKERR_BASE + 100)
#define OSAL_DOM (OSAL_SOCKERR_BASE + 101)
#define OSAL_RANGE (OSAL_SOCKERR_BASE + 102)
#define OSAL_DEADLK (OSAL_SOCKERR_BASE + 103)
#define OSAL_NOLCK (OSAL_SOCKERR_BASE + 104)
#define OSAL_NOSYS (OSAL_SOCKERR_BASE + 105)
#define OSAL_NOMSG (OSAL_SOCKERR_BASE + 106)
#define OSAL_IDRM (OSAL_SOCKERR_BASE + 107)
#define OSAL_CHRNG (OSAL_SOCKERR_BASE + 108)
#define OSAL_L2NSYNC (OSAL_SOCKERR_BASE + 109)
#define OSAL_L3HLT (OSAL_SOCKERR_BASE + 110)
#define OSAL_L3RST (OSAL_SOCKERR_BASE + 111)
#define OSAL_LNRNG (OSAL_SOCKERR_BASE + 112)
#define OSAL_UNATCH (OSAL_SOCKERR_BASE + 113)
#define OSAL_NOCSI (OSAL_SOCKERR_BASE + 114)
#define OSAL_L2HLT (OSAL_SOCKERR_BASE + 115)
#define OSAL_BADE (OSAL_SOCKERR_BASE + 116)
#define OSAL_BADR (OSAL_SOCKERR_BASE + 117)
#define OSAL_XFULL (OSAL_SOCKERR_BASE + 118)
#define OSAL_NOANO (OSAL_SOCKERR_BASE + 119)
#define OSAL_BADRQC (OSAL_SOCKERR_BASE + 120)
#define OSAL_BADSLT (OSAL_SOCKERR_BASE + 121)
#define OSAL_BFONT (OSAL_SOCKERR_BASE + 122)
#define OSAL_NOSTR (OSAL_SOCKERR_BASE + 123)
#define OSAL_NODATA (OSAL_SOCKERR_BASE + 124)
#define OSAL_TIME (OSAL_SOCKERR_BASE + 125)
#define OSAL_NOSR (OSAL_SOCKERR_BASE + 126)
#define OSAL_NONET (OSAL_SOCKERR_BASE + 127)
#define OSAL_NOPKG (OSAL_SOCKERR_BASE + 128)
#define OSAL_NOLINK (OSAL_SOCKERR_BASE + 129)
#define OSAL_ADV (OSAL_SOCKERR_BASE + 130)
#define OSAL_SRMNT (OSAL_SOCKERR_BASE + 131)
#define OSAL_COMM (OSAL_SOCKERR_BASE + 132)
#define OSAL_PROTO (OSAL_SOCKERR_BASE + 133)
#define OSAL_MULTIHOP (OSAL_SOCKERR_BASE + 134)
#define OSAL_DOTDOT (OSAL_SOCKERR_BASE + 135)
#define OSAL_BADMSG (OSAL_SOCKERR_BASE + 136)
#define OSAL_OVERFLOW (OSAL_SOCKERR_BASE + 137)
#define OSAL_NOTUNIQ (OSAL_SOCKERR_BASE + 138)
#define OSAL_BADFD (OSAL_SOCKERR_BASE + 139)
#define OSAL_REMCHG (OSAL_SOCKERR_BASE + 140)
#define OSAL_LIBACC (OSAL_SOCKERR_BASE + 141)
#define OSAL_LIBBAD (OSAL_SOCKERR_BASE + 142)
#define OSAL_LIBSCN (OSAL_SOCKERR_BASE + 143)
#define OSAL_LIBMAX (OSAL_SOCKERR_BASE + 144)
#define OSAL_LIBEXEC (OSAL_SOCKERR_BASE + 145)
#define OSAL_ILSEQ (OSAL_SOCKERR_BASE + 146)
#define OSAL_RESTART (OSAL_SOCKERR_BASE + 147)
#define OSAL_STRPIPE (OSAL_SOCKERR_BASE + 148)
#define OSAL_UCLEAN (OSAL_SOCKERR_BASE + 149)
#define OSAL_NOTNAM (OSAL_SOCKERR_BASE + 150)
#define OSAL_NAVAIL (OSAL_SOCKERR_BASE + 151)
#define OSAL_ISNAM (OSAL_SOCKERR_BASE + 152)
#define OSAL_REMOTEIO (OSAL_SOCKERR_BASE + 153)
#define OSAL_NOMEDIUM (OSAL_SOCKERR_BASE + 154)
#define OSAL_MEDIUMTYPE (OSAL_SOCKERR_BASE + 155)
#define OSAL_CANCELED (OSAL_SOCKERR_BASE + 156)
#define OSAL_NOKEY (OSAL_SOCKERR_BASE + 157)
#define OSAL_KEYEXPIRED (OSAL_SOCKERR_BASE + 158)
#define OSAL_KEYREVOKED (OSAL_SOCKERR_BASE + 159)
#define OSAL_KEYREJECTED (OSAL_SOCKERR_BASE + 160)

#endif /* OSAL_ERRNO_H */
