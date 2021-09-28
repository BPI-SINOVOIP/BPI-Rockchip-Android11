/******************************************************************************
 *
 * Copyright 2019, Fuzhou Rockchip Electronics Co.Ltd. All rights reserved.
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of:
 * Fuzhou Rockchip Electronics Co.Ltd .
 *
 *
 *****************************************************************************/
/**
 * @file    UvcMsgQ.h
 *
 *****************************************************************************/
#ifndef __UVCMSGQ_H__
#define __UVCMSGQ_H__

namespace android {
struct Message_cam
{
    unsigned int command;
    unsigned int type;
    void*        arg1;
    void*        arg2;
    void*        arg3;
    void*        arg4;
    Message_cam(){
        arg1 = 0;
        arg2 = 0;
        arg3 = 0;
        arg4 = 0;
    }
};
class MsgQueue
{
public:
    MsgQueue();
    MsgQueue(const char *name);
    ~MsgQueue();
    int get(Message_cam*);
    int get(Message_cam*, int);
    int put(Message_cam*);
    bool isEmpty();
private:
    char MsgQueName[30];
    int fd_read;
    int fd_write;
};
}
#endif

