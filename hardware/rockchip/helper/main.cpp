#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <log/log.h>
#include <cutils/properties.h>

#define MPP_BUF_TYPE_DRM 1
#define MPP_BUF_TYPE_ION_LEGACY 2
#define MPP_BUF_TYPE_ION_404 3
#define MPP_BUF_TYPE_ION_419 4
#define MPP_BUF_TYPE_DMA_BUF 5

void print_mpp_buf_type(int type) {
    switch (type) {
        case 1:
            printf("MPP_BUF_TYPE_DRM=1\n");
            break;
        case 2:
            printf("MPP_BUF_TYPE_ION_LEGACY=2\n");
            break;
        case 3:
            printf("MPP_BUF_TYPE_ION_404=3\n");
            break;
        case 4:
            printf("MPP_BUF_TYPE_ION_419=4\n");
            break;
        case 5:
            printf("MPP_BUF_TYPE_DMA_BUF=5\n");
            break;
        default:
            break;
    }
}

void print_mpp_buf_type() {
    char prop_mpp_buf_type[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.vendor.mpp_buf_type", prop_mpp_buf_type, "1");
    int value_mpp_buf_type = atoi(prop_mpp_buf_type);
    printf("=====Current mpp buf type======\n");
    print_mpp_buf_type(value_mpp_buf_type);
    printf("=====Avaliable mpp buf type======\n");
    for (int i = 0; i <= 5; i++) {
        print_mpp_buf_type(i);
    }
}

int main() {
    print_mpp_buf_type();
    return 0;
}
