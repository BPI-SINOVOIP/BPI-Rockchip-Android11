/*************************************************************************
	> File Name: test_sparse.cpp
	> Author: jkand.huang
	> Mail: jkand.huang@rock-chips.com
	> Created Time: Mon 21 Aug 2017 10:50:50 AM CST
 ************************************************************************/

#include <iostream>
#include "RKSparse.h"
#include "Upgrade.h"
using namespace std;
int main(int argc, char *argv[]){
    printf("======= start test RKSparse. ======\n");
    do_rk_sparse_update("system", "/data/media/0/system.img");
    printf("======= end test RKSparse. ======\n");

    return 0;
}
