#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_wint_t
#define __NEED_wctype_t

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
#define __NEED_locale_t
#endif

#include <bits/alltypes.h>

typedef const int* wctrans_t;

#undef WEOF
#define WEOF 0xffffffffU

#undef iswdigit

int iswalnum(wint_t);
int iswalpha(wint_t);
int iswblank(wint_t);
int iswcntrl(wint_t);
int iswdigit(wint_t);
int iswgraph(wint_t);
int iswlower(wint_t);
int iswprint(wint_t);
int iswpunct(wint_t);
int iswspace(wint_t);
int iswupper(wint_t);
int iswxdigit(wint_t);
int iswctype(wint_t, wctype_t);
wint_t towctrans(wint_t, wctrans_t);
wint_t towlower(wint_t);
wint_t towupper(wint_t);
wctrans_t wctrans(const char*);
wctype_t wctype(const char*);

#ifndef __cplusplus
#undef iswdigit
#define iswdigit(a) (0 ? iswdigit(a) : ((unsigned)(a) - '0') < 10)
#endif

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_GNU_SOURCE) || defined(_BSD_SOURCE)

#endif

#ifdef __cplusplus
}
#endif
