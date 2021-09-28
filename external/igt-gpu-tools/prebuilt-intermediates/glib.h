#pragma once

#define G_KEY_FILE_NONE 0

typedef void GError;
typedef void GKeyFile;

static inline void g_clear_error(GError *error) { }
static inline void g_error_free(GError *error) { }
static inline char *g_get_home_dir(void) { return "/data/local/tmp"; }
static inline void g_key_file_free(GKeyFile *file) { }
static inline GKeyFile *g_key_file_new(void) { return NULL; }
static inline int g_key_file_get_integer(GKeyFile *key_file,
    const char *group_name, const char *key, GError **error) { return 0; }
static inline char *g_key_file_get_string(GKeyFile *key_file,
    const char *group_name, const char *key, GError **error) { return NULL; }
static inline bool g_key_file_load_from_file(GKeyFile *key_file,
    const char *file, int flags, GError **error) { return false; }
