#ifndef VERSION_H
#define VERSION_H
/**
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(VERSION_CL)
#define VERSION_CL "?"
#endif

#if !defined(VERSION_PROD)
#define VERSION_PROD "unknown"
#endif

#if !defined(VERSION_BRANCH)
#define VERSION_BRANCH "?"
#endif

#if !defined(VERSION_NUM)
#define VERSION_NUM "?.?.?.?"
#endif

#define VERSION_STRING                                              \
   VERSION_PROD " "                                                 \
   VERSION_NUM " "                                                  \
   "(br=" VERSION_BRANCH "; cl=" VERSION_CL ")"

/*
=======================================================================
MACROS DOCUMENTATION
=======================================================================

VERSION_MAJOR

Description:
	Defines the major release number of the version.

Comments:
    It has to be a valid numerical value
=======================================================================

VERSION_MINOR

Description:
	Defines the minor release number of the version.

Comments:
    It has to be a valid numerical value
=======================================================================

VERSION_MAINT

Description:
	Defines the maintenance release of the version.

Comments:
    It has to be a valid numerical value
=======================================================================

VERSION_BUILD

Description:
	Defines the build ID of the version.

Comments:
    It has to be a valid numerical value
=======================================================================

VERSION_STRING

Description:
	Defines the version string that specifies the version number.

Definition:

   #define VERSION_STRING "a.b.c.d (name=value;name=value;...)"
	where a=major release number
	      b=minor release number
	      c=maintenance release number
	      d=build number

	name=value pair provides additional information about the build.
	Example:
	patch/feature=comma separated list of features/patches that have been installed.
	br=p4 branch that was used for the build
	cl=p4 change list number
	machine=hostname of the machine that was used for the build.

Comments:

=======================================================================
*/

#endif // VERSION_H
