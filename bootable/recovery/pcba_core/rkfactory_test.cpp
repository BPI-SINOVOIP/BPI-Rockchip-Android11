#include "rkfactory_test.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <bootloader_message/bootloader_message.h>

#include "recovery_ui/ui.h"
#include "debug.h"
#include "Language/language.h"

#include "wlan_test.h"
#include "rtc_test.h"
#include "display_callback.h"
#include "gsensor_test.h"
#include "bt_test.h"
#include "udisk_test.h"
#include "sdcard_test.h"
#include "battery_test.h"
#include "audiodev_test/codec_test.h"
#include "rkhal3_camera/camera_test.h"
#include "ddr_emmc_test.h"

extern "C" {
    #include "script.h"
    #include "test_case.h"
    #include "script_parser.h"
}

static const std::vector<std::pair<std::string, Device::BuiltinAction>> rkFactoryMenuActions{
    { "Power off", Device::SHUTDOWN },
    { "Reboot system now", Device::REBOOT },
    { "Enter recovery", Device::ENTER_RECOVERY },
    { "Reboot to bootloader", Device::REBOOT_BOOTLOADER },
};

RKFactory::RKFactory(){
    bRKFactory = true;
}
bool RKFactory::isRKFactory(){
    return bRKFactory;
}

int manual_p_y = 1;
/* current position for auto test tiem in y direction */

int simCounts = 2;

#define SCRIPT_NAME    "/pcba/test_config.cfg"

RecoveryUI* pcba_ui;
std::vector<std::string> pcba_title_lines;
std::vector<RecoveryUI::TestResultEnum> pcba_result_lines;

void init_title_lines_for_testcase(const char *test_name, struct testcase_info *tc_info) {
    std::string *msg = new std::string();
    msg->append("Device ").append(test_name).append(":[...] {...}");
    pcba_title_lines.push_back(*msg);
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    // save index
    tc_info->y = pcba_title_lines.size() - 1;
}

void refresh_screen_hl_hook(int index, std::string msg, bool highlight) {
    if (highlight) {
        // Test failed
        pcba_result_lines[index] = RecoveryUI::TestResultEnum::FAILED;
    } else {
        // Test passed
        pcba_result_lines[index] = RecoveryUI::TestResultEnum::PASS;
    }
    pcba_title_lines[index] = msg;
    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->SetTitleResult(pcba_result_lines);
    pcba_ui->ShowText(true);
}

void refresh_screen_hook(int index, std::string msg) {
    pcba_title_lines[index] = msg;
    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->SetTitleResult(pcba_result_lines);
    pcba_ui->ShowText(true);
}

display_callback *display_hook = new display_callback {
    .handle_refresh_screen_hl = refresh_screen_hl_hook,
    .handle_refresh_screen = refresh_screen_hook,
};

display_callback *get_display_hook() {
    return display_hook;
}

static int total_testcases;
//static struct testcase_base_info *base_info;
static struct list_head auto_test_list_head;
static struct list_head manual_test_list_head;

int start_test_pthread(struct testcase_info *tc_info)
{
    printf("%s\n", tc_info->base_info->name);
    if (!strcmp(tc_info->base_info->name, "ddr")) {
        // emmc test
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&ddr_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "emmc")) {
        // flash_test
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&flash_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "wifi")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&wlan_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "rtc")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&rtc_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "gsensor")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&gsensor_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "bluetooth")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&bt_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "udisk")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&udisk_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "sdcard")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&sdcard_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "battery")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&battery_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "camera")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&camera_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else if (!strcmp(tc_info->base_info->name, "Codec")) {
        init_title_lines_for_testcase(tc_info->base_info->name, tc_info);
        std::thread *temp = new std::thread(&codec_test, tc_info, get_display_hook());
        if (!temp) {
            printf("create %s test thread error/n", tc_info->base_info->name);
        }
    } else {
        printf("unsupport test item:%s\n", tc_info->base_info->name);
        return -1;
    }

    return 0;
}

int init_manual_test_item(struct testcase_info *tc_info)
{
    printf("start_manual_test_item : %d, %s \r\n", tc_info->y,
           tc_info->base_info->name);

    manual_p_y += 1;
    tc_info->y = manual_p_y;

    start_test_pthread(tc_info);

    return 0;
}
int start_auto_test_item(struct testcase_info *tc_info)
{
    printf("start_auto_test_item : %d, %s \r\n", tc_info->y,
           tc_info->base_info->name);

    start_test_pthread(tc_info);

    return 0;
}


static int parse_testcase(void)
{
    int i, j, mainkey_cnt;
    struct testcase_base_info *info;
    char mainkey_name[32], display_name[64], binary[16];
    int activated, category, run_type, sim_counts;
    int len;

    mainkey_cnt = script_mainkey_cnt();
    info = (struct testcase_base_info *)
    malloc(sizeof(struct testcase_base_info) * mainkey_cnt);
    if (info == NULL) {
        db_error("core: allocate memory for temporary test case basic "
                 "information failed(%s)\n", strerror(errno));
        return -1;
    }
    memset(info, 0, sizeof(struct testcase_base_info) * mainkey_cnt);

    for (i = 0, j = 0; i < mainkey_cnt; i++) {
        struct testcase_info *tc_info;

        memset(mainkey_name, 0, 32);
        script_mainkey_name(i, mainkey_name);

        if (script_fetch(mainkey_name, (char*)"display_name", (int *)display_name, 16))
            continue;

        if (script_fetch(mainkey_name, (char*)"activated", &activated, 1))
            continue;

        if (display_name[0] && activated == 1) {
            strncpy(info[j].name, mainkey_name, 32);
            strncpy(info[j].display_name, display_name, 64);
            info[j].activated = activated;

            if (script_fetch
                (mainkey_name, (char*)"program", (int *)binary, 4) == 0) {
                    strncpy(info[j].binary, binary, 16);
                }

            info[j].id = j;

            if (script_fetch(mainkey_name, (char*)"category", &category, 1)
                == 0) {
                    info[j].category = category;
                }

            if (script_fetch(mainkey_name, (char*)"run_type", &run_type, 1)
                == 0) {
                    info[j].run_type = run_type;
                }

            if (script_fetch
                (mainkey_name, (char*)"sim_counts", &sim_counts, 1) == 0) {
                    simCounts = sim_counts;
                }
            tc_info = (struct testcase_info *)
            malloc(sizeof(struct testcase_info));
            if (tc_info == NULL) {
                printf("malloc for tc_info[%d] fail\n", j);
                return -1;
            }
            tc_info->x = 0;
            tc_info->y = 0;
            tc_info->base_info = &info[j];
            if (tc_info->base_info->category)
            list_add(&tc_info->list,
                     &manual_test_list_head);
            else
            list_add(&tc_info->list, &auto_test_list_head);
            j++;
        }
    }
    total_testcases = j;
    db_msg("core: total test cases #%d\n", total_testcases);
    if (total_testcases == 0) return 0;
    len = sizeof(struct testcase_base_info) * total_testcases;
    return total_testcases;
}

int RKFactory::StartFactorytest(Device* device) {
    int ret;
    char *script_buf;
    struct list_head *pos;
    //int success = 0;
    //char rfCalResult[10];

    pcba_ui = device->GetUI();
    pcba_ui->SetRkFactoryStart(true);
    pcba_ui->SetEnableTouchEvent(true, false);
    pcba_title_lines = { PCBA_VERSION_NAME};
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    pcba_title_lines.push_back("Serial number - " + android::base::GetProperty("ro.serialno", ""));
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);

    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->ShowText(true);
    printf("parse script failed\n");
    INIT_LIST_HEAD(&manual_test_list_head);
    INIT_LIST_HEAD(&auto_test_list_head);
    pcba_title_lines.push_back("=======================================================");
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);

    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->ShowText(true);
    script_buf = parse_script((char*)SCRIPT_NAME);
    if (!script_buf) {
        printf("parse script failed\n");
        return -1;
    }

    ret = init_script(script_buf);
    if (ret) {
        db_error("core: init script failed(%d)\n", ret);
        return -1;
    }

    ret = parse_testcase();
    if (ret < 0) {
        db_error("core: parse all test case from script failed(%d)\n",
                 ret);
        return -1;
    } else if (ret == 0) {
        db_warn("core: NO TEST CASE to be run\n");
        return -1;
    }

    printf("manual testcase:\n");
    pcba_title_lines.push_back(PCBA_MANUAL_TEST);
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    list_for_each(pos, &manual_test_list_head) {
        struct testcase_info *tc_info =
        list_entry(pos, struct testcase_info, list);
        init_manual_test_item(tc_info);
    }

    pcba_title_lines.push_back("=======================================================");
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    pcba_title_lines.push_back(PCBA_AUTO_TEST);
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->ShowText(true);
    printf("\n\nauto testcase:\n");
    list_for_each(pos, &auto_test_list_head) {
        struct testcase_info *tc_info =
        list_entry(pos, struct testcase_info, list);
        start_auto_test_item(tc_info);
    }
    //start_input_thread();

    printf("pcba test over!\n");
    pcba_title_lines.push_back("=======================================================");
    pcba_result_lines.push_back(RecoveryUI::TestResultEnum::TESTING);
    pcba_ui->ResetKeyInterruptStatus();
    pcba_ui->SetTitle(pcba_title_lines);
    pcba_ui->ShowText(true);

  //std::string err;
  //if (!clear_bootloader_message(&err)) {
  //    LOG(ERROR) << "Failed to clear BCB message: " << err;
  //}
    printf("display menukey\n");
    std::vector<std::string> rkFactory_menu_items;
    std::transform(rkFactoryMenuActions.cbegin(), rkFactoryMenuActions.cend(),
                    std::back_inserter(rkFactory_menu_items),
                    [](const auto& entry) { return entry.first; });

    auto chosen_item = pcba_ui->ShowMenu(
                                {}, rkFactory_menu_items, 0, false,
                                std::bind(&Device::HandleMenuKey, device,
                                std::placeholders::_1, std::placeholders::_2));

    if (chosen_item == static_cast<size_t>(RecoveryUI::KeyError::INTERRUPTED)) {
        return 0;//Device::KEY_INTERRUPTED;
    }

    return 0;//kFastbootMenuActions[chosen_item].second;
}
