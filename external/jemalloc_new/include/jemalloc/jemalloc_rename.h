/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#if defined(__BIONIC__)
#  define je_aligned_alloc je_aligned_alloc
#  define je_calloc je_calloc
#  define je_dallocx je_dallocx
#  define je_free je_free
#  define je_mallctl je_mallctl
#  define je_mallctlbymib je_mallctlbymib
#  define je_mallctlnametomib je_mallctlnametomib
#  define je_malloc je_malloc
#  define je_malloc_conf je_malloc_conf
#  define je_malloc_message je_malloc_message
#  define je_malloc_stats_print je_malloc_stats_print
#  define je_malloc_usable_size je_malloc_usable_size
#  define je_mallocx je_mallocx
#  define je_nallocx je_nallocx
#  define je_posix_memalign je_posix_memalign
#  define je_rallocx je_rallocx
#  define je_realloc je_realloc
#  define je_sallocx je_sallocx
#  define je_sdallocx je_sdallocx
#  define je_xallocx je_xallocx
#  define je_memalign je_memalign
#  define je_valloc je_valloc
#else
#  define je_aligned_alloc aligned_alloc
#  define je_calloc calloc
#  define je_dallocx dallocx
#  define je_free free
#  define je_mallctl mallctl
#  define je_mallctlbymib mallctlbymib
#  define je_mallctlnametomib mallctlnametomib
#  define je_malloc malloc
#  define je_malloc_conf malloc_conf
#  define je_malloc_message malloc_message
#  define je_malloc_stats_print malloc_stats_print
#  define je_malloc_usable_size malloc_usable_size
#  define je_mallocx mallocx
#  define je_nallocx nallocx
#  define je_posix_memalign posix_memalign
#  define je_rallocx rallocx
#  define je_realloc realloc
#  define je_sallocx sallocx
#  define je_sdallocx sdallocx
#  define je_xallocx xallocx
#  define je_memalign memalign
#  define je_valloc valloc
#endif
#endif
