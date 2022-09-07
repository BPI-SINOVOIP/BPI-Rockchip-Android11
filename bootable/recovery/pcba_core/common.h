#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>
enum { INSTALL_SUCCESS, INSTALL_ERROR, INSTALL_CORRUPT };

// These are just the defines for the non-const internal variables
#include "variables.h"

#if (defined ROTATE_SCREEN_90) || (defined ROTATE_SCREEN_270)
#define MAX_COLS 66
#define MAX_ROWS 96
#else
#define MAX_COLS 96
#define MAX_ROWS 66
#endif

#define  kMaxTiles  50

#if (defined ROTATE_SCREEN_90) || (defined ROTATE_SCREEN_270)
#define MENU_MAX_COLS 500
#define MENU_MAX_ROWS 50
#else
#define MENU_MAX_COLS 50
#define MENU_MAX_ROWS 500
#endif



#ifdef RK3288_PCBA
#define CHAR_WIDTH 30
#define CHAR_HEIGHT 60
#else
#define CHAR_WIDTH 18
#define CHAR_HEIGHT 32
#endif

int ensure_path_mounted(const char* path);
char** prepend_title(const char** headers);
static const char *SDCARD_ROOT = "/sdcard";

// Initialize the graphics system.
void ui_init();
void ui_print_init(void);
void ui_print_xy_rgba(int t_col,int t_row,int r,int g,int b,int a,const char * fmt,...);
void ui_display_sync(int t_col,int t_row,int r,int g,int b,int a,const char* fmt,...);
void FillColor(int r,int g,int b,int a,int left,int top,int width,int height);

struct display_info {
	int col;
	int row;
	int r;
	int g;
	int b;
	int a;
	char string[128];
};

void ui_print_xy_rgba_multi(struct display_info *info, int count);

extern int notError;

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ui_print_overwrite(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
void ui_start_menu(char** headers, char** items, int initial_selection);
// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);
// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_ICON_MAIN,
  BACKGROUND_ICON_WIPE,
  BACKGROUND_ICON_WIPE_CHOOSE,
  BACKGROUND_ICON_FLASH_ZIP,
  BACKGROUND_ICON_INSTALLING,
  BACKGROUND_ICON_NANDROID,
  BACKGROUND_ICON_ERROR,
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

#define LOGE(...) fprintf(stdout, "E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

typedef struct {
    const char* mount_point;  // eg. "/cache".  must live in the root directory.

    const char* fs_type;      // "yaffs2" or "ext4" or "vfat"

    const char* device;       // MTD partition name if fs_type == "yaffs"
                              // block device if fs_type == "ext4" or "vfat"

    const char* device2;      // alternative device to try if fs_type
                              // == "ext4" or "vfat" and mounting
                              // 'device' fails

    long long length;         // (ext4 partition only) when
                              // formatting, size to use for the
                              // partition.  0 or negative number
                              // means to format all but the last
                              // (that much).
} Volume;

void wipe_data(int confirm);

int gui_set_variable(const char*, const char*);


// From ICS common, allows building custom recovery_ui.c files
typedef struct {
    // number of frames in indeterminate progress bar animation
    int indeterminate_frames;

    // number of frames per second to try to maintain when animating
    int update_fps;

    // number of frames in installing animation.  may be zero for a
    // static installation icon.
    int installing_frames;

    // the install icon is animated by drawing images containing the
    // changing part over the base icon.  These specify the
    // coordinates of the upper-left corner.
    int install_overlay_offset_x;
    int install_overlay_offset_y;

} UIParameters;



// This handles the special partitions
#ifndef SP1_NAME
#define SP1_NAME
#define SP1_BACKUP_METHOD none
#define SP1_MOUNTABLE 0
#endif
#ifndef SP1_DISPLAY_NAME
#define SP1_DISPLAY_NAME SP1_NAME
#endif
#ifndef SP2_NAME
#define SP2_NAME
#define SP2_BACKUP_METHOD none
#define SP2_MOUNTABLE 0
#endif
#ifndef SP2_DISPLAY_NAME
#define SP2_DISPLAY_NAME SP2_NAME
#endif
#ifndef SP3_NAME
#define SP3_NAME
#define SP3_BACKUP_METHOD none
#define SP3_MOUNTABLE 0
#endif
#ifndef SP3_DISPLAY_NAME
#define SP3_DISPLAY_NAME SP3_NAME
#endif


struct manual_item
{
	const char *name;
	const int x;
	const int y;
	const int w;
	const int h;
	void *argc;
	int (*func)(void *argv);
};
int start_manual_test_item(int x,int y);
int get_cur_print_y(void);


#endif  // RECOVERY_COMMON_H
