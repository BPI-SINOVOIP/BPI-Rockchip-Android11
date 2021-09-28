#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ftw.h>

#include "config.h"
#include "gpg.h"
#include "../common/types.h"
#include "../common/iobuf.h"
#include "keydb.h"
#include "keyedit.h"
#include "../common/util.h"
#include "main.h"
#include "call-dirmngr.h"
#include "trustdb.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mount.h>

// 8kb should be enough ;-)
#define MAX_LEN 0x2000

static bool initialized = false;
ctrl_t ctrlGlobal;
int fd;
char *filename;

//hack not to include gpg.c which has main function
int g10_errors_seen = 0;

void
g10_exit( int rc )
{
    gcry_control (GCRYCTL_UPDATE_RANDOM_SEED_FILE);
    gcry_control (GCRYCTL_TERM_SECMEM );
    exit (rc);
}

static void
gpg_deinit_default_ctrl (ctrl_t ctrl)
{
#ifdef USE_TOFU
    tofu_closedbs (ctrl);
#endif
    gpg_dirmngr_deinit_session_data (ctrl);

    keydb_release (ctrl->cached_getkey_kdb);
}

static void
my_gcry_logger (void *dummy, int level, const char *format, va_list arg_ptr)
{
    return;
}

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag)
{
    if (typeflag == FTW_F){
        unlink(fpath);
    }
    return 0;
}

static void rmrfdir(char *path)
{
    ftw(path, unlink_cb, 16);
    if (rmdir(path) != 0) {
        printf("failed rmdir, errno=%d\n", errno);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (! initialized) {
        ctrlGlobal = (ctrl_t) malloc(sizeof(*ctrlGlobal));
        if (!ctrlGlobal) {
            exit(1);
        }
        //deletes previous tmp dir and (re)create it as a ramfs
        //system("umount /tmp/fuzzdirimport");
        rmrfdir("/tmp/fuzzdirimport");
        if (mkdir("/tmp/fuzzdirimport", 0700) < 0) {
            printf("failed mkdir, errno=%d\n", errno);
            if (errno != EEXIST) {
                return 0;
            }
        }
        //system("mount -t tmpfs -o size=64M tmpfs /tmp/fuzzdirimport");
        filename=strdup("/tmp/fuzzdirimport/fuzz.gpg");
        if (!filename) {
            free(ctrlGlobal);
            return 0;
        }
        fd = open(filename, O_RDWR | O_CREAT, 0666);
        if (fd == -1) {
            free(filename);
            free(ctrlGlobal);
            printf("failed open, errno=%d\n", errno);
            return 0;
        }
        gnupg_set_homedir("/tmp/fuzzdirimport/");
        gpg_error_t gpgerr = keydb_add_resource ("pubring" EXTSEP_S GPGEXT_GPG, KEYDB_RESOURCE_FLAG_DEFAULT);
        if (gpgerr != GPG_ERR_NO_ERROR) {
            free(filename);
            free(ctrlGlobal);
            close(fd);
            printf("failed keydb_add_resource, errno=%d\n", gpgerr);
            return 0;
        }
        gpgerr = setup_trustdb (1, NULL);
        if (gpgerr != GPG_ERR_NO_ERROR) {
            free(filename);
            free(ctrlGlobal);
            close(fd);
            printf("failed setup_trustdb, errno=%d\n", gpgerr);
            return 0;
        }
        //populate /tmp/fuzzdirimport/ as homedir ~/.gnupg
        strlist_t sl = NULL;
        public_key_list (ctrlGlobal, sl, 0, 0);
        free_strlist(sl);
        //no output for stderr
        log_set_file("/dev/null");
        gcry_set_log_handler (my_gcry_logger, NULL);
        gnupg_initialize_compliance (GNUPG_MODULE_NAME_GPG);
        initialized = true;
    }

    memset(ctrlGlobal, 0, sizeof(*ctrlGlobal));
    ctrlGlobal->magic = SERVER_CONTROL_MAGIC;
    if (Size > MAX_LEN) {
        // limit maximum size to avoid long computing times
        Size = MAX_LEN;
    }

    if (ftruncate(fd, Size) == -1) {
        return 0;
    }
    if (lseek (fd, 0, SEEK_SET) < 0) {
        return 0;
    }
    if (write (fd, Data, Size) != Size) {
        return 0;
    }

    import_keys (ctrlGlobal, &filename, 1, NULL, IMPORT_REPAIR_KEYS, 0, NULL);
    gpg_deinit_default_ctrl (ctrlGlobal);
    /*memset(ctrlGlobal, 0, sizeof(*ctrlGlobal));
    ctrlGlobal->magic = SERVER_CONTROL_MAGIC;
    PKT_public_key pk;
    get_pubkey_fromfile (ctrlGlobal, &pk, filename);
    release_public_key_parts (&pk);
    gpg_deinit_default_ctrl (ctrlGlobal);*/

    return 0;
}
