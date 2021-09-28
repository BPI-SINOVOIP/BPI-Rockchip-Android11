#!/usr/bin/python2

# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reference[1]: IT680x example code:
# https://drive.google.com/corp/drive/u/0/folders/0B8Lcp5hqbjaqaE5WdDA5alVWOXc

# Reference[2]: IT6803 Programming Guide:
# https://docs.google.com/viewer?a=v&pid=sites&srcid=\
# Y2hyb21pdW0ub3JnfGRldnxneDoyNGVmNGFiMDE4ZWJiZDM2

# This code is a library for using IT680X chip in chameleon.

import sys
import util
from time import sleep

usage = """\
Usage:
  it6803                        -- print command usage
  it6803 cec_reg_print          -- print all cec registers value
  it6803 cec_msg_receive        -- print receiving cec message
  it6803 cec_msg {cmd}          -- send cec message
"""

QUEUE_SIZE = 3
q_head = 0
q_tail = 0
regTxOutState = 3

logicalAddr = 0
initiatorAddr = 0x0F
cecTxState = 0

txCmdBuf = [0x00] * 19
rxCecBuf = [0x00] * 19
queue = [[0x00 for i in range(19)] for j in range(QUEUE_SIZE)]

# Chameleon register address
I2C_HDMI = 0x48
I2C_CEC = 0x4A

# Chameleon CEC control registers
# (name starts with REG is register addr, followings are values for this reg)
REG06         = 0x06
REG_EMPTY     = 0x00
REG07         = 0x07
ENABLE_CEC_INTERRUPT_PIN = 0x40

REG08         = 0x08
FIRE_FRAME          = 0x80
DEBUG_CEC_CLEAR     = 0x40
CEC_SCHMITT_TRIGGER = 0x08
CEC_INTERRUPT       = 0x01

REG09         = 0x09
REGION_SELECT    = 0x40
INITAITOR_RX_CEC = 0x20
ACKNOWLEDGE      = 0x01

REG_MIN_BIT   = 0x0B
REG_TIME_UNIT = 0x0C

REG0F         = 0x0F
IO_PULL_UP    = 0x50

REG_TARG_ADDR = 0x22
REG_MSCOUNT_L = 0x45
REG_MSCOUNT_M = 0x46
REG_MSCOUNT_H = 0x47
REF_INT_STATUS= 0x4C

def main(cmdline):
    """ Main function. """
    args = [''] * 4
    for i, x in enumerate(cmdline):
        args[i] = x
    cmd = args[1]

    if cmd == '': cmd = 'help'
    fname = 'cmd_' + cmd

    cec_open()
    if fname in globals():
        if args[2] == '':
            globals()[fname]()
        else:
            globals()[fname](args[2])
    else:
        print 'Unknown command', cmd
    cec_close()


def cmd_help():
    """ Print help message. """
    print usage


def cec_open():
    """ Enable cec port. """
    # enable IT6803 CEC port: enable cec clock and assign slave addr
    i2cset(I2C_HDMI, 0x0E, 0xFF)
    i2cset(I2C_HDMI, 0x86, 0x95)

def cec_close():
    """ Close cec port. """
    # disable cec slave addr
    i2cset(I2C_HDMI, 0x86, 0x94)


def cec_init():
    """ Initialize cec port in chameleon. """
    # initial CEC register. From reference[1] Ln480

    # enable it680x cec
    i2cset(I2C_CEC, 0xF8, 0xC3)
    i2cset(I2C_CEC, 0xF8, 0xA5)
    q_head = 0
    q_tail = 0
    regTxOutState = 3

    # get 100ms timer, according to ref [1,2]
    i2cset(I2C_CEC, REG09, ACKNOWLEDGE)
    sleep(0.099)
    i2cset(I2C_CEC, REG09, REG_EMPTY)
    high  = util.i2c_read(0, I2C_CEC, REG_MSCOUNT_H, 1)[0] * 0x10000
    mid   = util.i2c_read(0, I2C_CEC, REG_MSCOUNT_M, 1)[0] * 0x100
    low   = util.i2c_read(0, I2C_CEC, REG_MSCOUNT_L, 1)[0]
    tus = (high + mid + low) / 1000
    # print tus

    # CEC configuration
    i2cset(I2C_CEC, REG09, INITAITOR_RX_CEC | REGION_SELECT)
    i2cset(I2C_CEC, REG_MIN_BIT, 0x14)
    i2cset(I2C_CEC, REG_TIME_UNIT, tus)
    i2cset(I2C_CEC, REG_TARG_ADDR, logicalAddr)
    i2cset(I2C_CEC, REG08, CEC_SCHMITT_TRIGGER)
    uc = util.i2c_read(0, I2C_CEC, REG09, 1)[0]
    # i2cset(I2C_CEC, REG09, uc|0x02)
    # cec_clr_int
    i2cset(I2C_CEC, REG08, CEC_INTERRUPT|DEBUG_CEC_CLEAR|CEC_SCHMITT_TRIGGER)
    i2cset(I2C_CEC, REG08, CEC_SCHMITT_TRIGGER|DEBUG_CEC_CLEAR)
    # print 'logicalAddr: {}, TimeUnit: {}'.format(logicalAddr,tus)

    # Enable CEC interrupt pin
    reg07_val = util.i2c_read(0, I2C_CEC, REG07, 1)[0]
    i2cset(I2C_CEC, REG07, reg07_val | ENABLE_CEC_INTERRUPT_PIN)

    # Enable ALL interrupt mask
    i2cset(I2C_CEC, REG06, REG_EMPTY)

    # IO pull up enable
    i2cset(I2C_CEC, REG0F, IO_PULL_UP)

def cec_msg_receive():
    """ Read message received. """
    # 0x3F means all interrupts are on
    cecInt = cec_reg_read(REF_INT_STATUS) & 0x3F
    if 0 != (cecInt & 0x10):
        if not cec_msg_read():
            raise Exception('Queue is full!')
    ## TODO check interrupt register Status
    i2c_cec_set(REF_INT_STATUS, cecInt)
    # Decode received message
    return cec_decode()


def cmd_cec_msg(message):
    """ parent function for a cec message. """
    cec_init()
    fname = 'cec_msg_' + message
    globals()[fname]()
    cec_transmit()

def cec_msg_standby():
    """ Send a stand by message. """
    # F = boardcast, 0x36 = stand by message
    cec_cmd_set(0xF, 0x36, None, None)
    # other operations need more assignments

def cec_msg_viewon():
    """ Send a view on message. """
    # 0 = TV, 0x04 = image on
    cec_cmd_set(0x0, 0x04, None, None)

def cec_msg_poweron():
    """ Make a power on cec message. """
    global initiatorAddr
    # 0x90 = power status message
    cec_cmd_set(initiatorAddr, 0x90, 0x00, None)

def cec_msg_poweroff():
    """ Make a power off cec message. """
    global initiatorAddr
    # 0x90 = power status message
    cec_cmd_set(initiatorAddr, 0x90, 0x01, None)

def cec_reg_read(offset):
    """ read it6803's register value from i2c line. """
    return util.i2c_read(0, I2C_CEC, offset, 1)[0]

def cec_cmd_set(follower, txCmd, operand1, operand2):
    """ Compose a cec message. """
    # print 'follower: {}, cmd: {}'.format(follower, txCmd)
    # TODO set variables
    txCmdBuf[0] = 2
    txCmdBuf[1] = (logicalAddr<<4) + follower
    txCmdBuf[2] = txCmd
    txCmdBuf[3] = 0
    txCmdBuf[4] = 0
    if operand1 is not None:
        txCmdBuf[3] = operand1
        txCmdBuf[0] = 3
    if operand2 is not None:
        txCmdBuf[4] = operand2
        txCmdBuf[0] = 4
    # print txCmdBuf
    return

def cec_transmit():
    """ File a cec message out. """
    # Assume the state is cecTransfer
    # Set values from 0x10 to 0x23
    i2c_cec_set(0x23, txCmdBuf[0])
    for i in range (0, txCmdBuf[0]):
        i2c_cec_set(0x10+i, txCmdBuf[i+1])

    # Fire command
    i2c_cec_set(REG08, FIRE_FRAME | CEC_SCHMITT_TRIGGER | DEBUG_CEC_CLEAR)
    i2c_cec_set(REG08, CEC_SCHMITT_TRIGGER | DEBUG_CEC_CLEAR)
    return

def cec_msg_read():
    """ Read incoming cec messages from memory. """
    global q_head, q_tail
    if (q_head % QUEUE_SIZE) != (q_tail % QUEUE_SIZE):
        return False
    q_tail += 1
    i = q_tail % QUEUE_SIZE
    # 0x30 is starting point for receiving message
    data = util.i2c_read(0, I2C_CEC, 0x30, 19)
    for j in range(1, 19):
        queue[i][j] = data[j-1]
    queue[i][0] = data[18]
    return True

def cec_decode():
    """ Process incoming cec message. """
    global q_head, q_tail, initiatorAddr
    if (q_head % QUEUE_SIZE) == (q_tail % QUEUE_SIZE):
        # Queue is empty
        return
    q_head += 1
    rxCecBuf = queue[q_head % QUEUE_SIZE]
    #print rxCecBuf

    if (rxCecBuf[0] == 1):
        if logicalAddr == (rxCecBuf[1] & 0x0F):
            # eReportPhysicalAddress
            return
    # Validate message
    initiatorAddr = (rxCecBuf[1] >> 4) & 0x0F
    followerAddr = rxCecBuf[1] & 0x0F
    print 'Initiator: {} Follower: {}'.format(initiatorAddr, followerAddr)

    if (rxCecBuf[2] == 0x04):
        print 'received image-view-on'
    elif (rxCecBuf[2] == 0x36):
        print 'received standby'
    else:
        print 'other command: {}'.format(rxCecBuf[2])
    return rxCecBuf[2]

def i2cset(addr, offset, value):
    """ set some register value via i2c line. """
    util.i2c_write(0, addr, offset, [value])

def i2c_cec_set(offset, value):
    """ set it6803's register value via i2c line. """
    i2cset(I2C_CEC, offset, value)

if __name__ == '__main__':
    main(sys.argv)
