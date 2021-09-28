/* jconfigint.h.  Customized for android on the basis of jconfigint.h.in. */

#ifndef __JCONFIGINT_H__
#define __JCONFIGINT_H__

/* libjpeg-turbo build number */
#define BUILD  ""

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#ifndef INLINE
  #ifndef TURBO_FOR_WINDOWS
    #define INLINE inline __attribute__((always_inline))
  #else
    #if defined(__GNUC__)
      #define INLINE inline __attribute__((always_inline))
    #elif defined(_MSC_VER)
      #define INLINE __forceinline
    #else
      #define INLINE
    #endif
  #endif
#endif

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "2.0.2"

/* The size of `size_t', as computed by sizeof. */
/* The size of `size_t', as reported by the compiler through the
 * builtin macro __SIZEOF_SIZE_T__. If the compiler does not
 * report __SIZEOF_SIZE_T__ add a custom rule for the compiler
 * here. */
#ifdef __SIZEOF_SIZE_T__
  #define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#else
  #error cannot determine the size of size_t
#endif

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#define HAVE_BUILTIN_CTZL

/* Define to 1 if you have the <intrin.h> header file. */
/* #undef HAVE_INTRIN_H */

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#endif  // __JCONFIGINT_H__
