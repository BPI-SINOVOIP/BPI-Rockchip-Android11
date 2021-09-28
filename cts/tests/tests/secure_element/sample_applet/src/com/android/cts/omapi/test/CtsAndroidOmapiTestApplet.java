package com.android.cts.omapi.test;

import javacard.framework.APDU;
import javacard.framework.ISO7816;
import javacard.framework.Applet;
import javacard.framework.ISOException;
import javacard.framework.Util;

public class CtsAndroidOmapiTestApplet extends Applet {

  final private static byte NO_DATA_INS_1 = (byte) 0x06;
  final private static byte NO_DATA_INS_2 = (byte) 0x0A;

  final private static byte DATA_INS_1 = (byte) 0x08;
  final private static byte DATA_INS_2 = (byte) 0x0C;

  final private static byte SW_62xx_APDU_INS = (byte) 0xF3;
  final private static byte SW_62xx_DATA_APDU_P2 = (byte) 0x08;
  final private static byte SW_62xx_VALIDATE_DATA_P2 = (byte) 0x0C;
  private final static byte[] SW_62xx_VALIDATE_DATA_RESP =
            new byte[]{0x01, (byte) 0xF3, 0x00, 0x0C, 0x01, (byte) 0xAA, 0x00};
  private final static short[] SW_62xx_resp = new short[]{
      (short)0x6200, (short)0x6281, (short)0x6282, (short)0x6283,
      (short)0x6285, (short)0x62F1, (short)0x62F2, (short)0x63F1,
      (short)0x63F2, (short)0x63C2, (short)0x6202, (short)0x6280,
      (short)0x6284, (short)0x6286, (short)0x6300, (short)0x6381,
  };

  final public static byte SEGMENTED_RESP_INS_1 = (byte) 0xC2;
  final public static byte SEGMENTED_RESP_INS_2 = (byte) 0xC4;
  final public static byte SEGMENTED_RESP_INS_3 = (byte) 0xC6;
  final public static byte SEGMENTED_RESP_INS_4 = (byte) 0xC8;
  final public static byte SEGMENTED_RESP_INS_5 = (byte) 0xCF;

  final private static byte CHECK_SELECT_P2_APDU = (byte)0xF4;
  final public static byte GET_RESPONSE_INS = (byte) 0xC0;

  final private static byte BER_TLV_TYPE = (byte) 0x1F;
  final private static short SELECT_RESPONSE_DATA_LENGTH = (short)252;

  private byte[] respBuf;
  private short respBuffOffset = 0;

  final private static short LENGTH_256 = (short) 0x0100;
  private final static byte[] resp_bytes256 = new byte[]{
    (byte)0x00, (byte)0x01, (byte)0x02, (byte)0x03, (byte)0x04, (byte)0x05,
    (byte)0x06, (byte)0x07, (byte)0x08, (byte)0x09, (byte)0x0A, (byte)0x0B,
    (byte)0x0C, (byte)0x0D, (byte)0x0E, (byte)0x0F,
    (byte)0x10, (byte)0x11, (byte)0x12, (byte)0x13, (byte)0x14, (byte)0x15,
    (byte)0x16, (byte)0x17, (byte)0x18, (byte)0x19, (byte)0x1A, (byte)0x1B,
    (byte)0x1C, (byte)0x1D, (byte)0x1E, (byte)0x1F,
    (byte)0x20, (byte)0x21, (byte)0x22, (byte)0x23, (byte)0x24, (byte)0x25,
    (byte)0x26, (byte)0x27, (byte)0x28, (byte)0x29, (byte)0x2A, (byte)0x2B,
    (byte)0x2C, (byte)0x2D, (byte)0x2E, (byte)0x2F,
    (byte)0x30, (byte)0x31, (byte)0x32, (byte)0x33, (byte)0x34, (byte)0x35,
    (byte)0x36, (byte)0x37, (byte)0x38, (byte)0x39, (byte)0x3A, (byte)0x3B,
    (byte)0x3C, (byte)0x3D, (byte)0x3E, (byte)0x3F,
    (byte)0x40, (byte)0x41, (byte)0x42, (byte)0x43, (byte)0x44, (byte)0x45,
    (byte)0x46, (byte)0x47, (byte)0x48, (byte)0x49, (byte)0x4A, (byte)0x4B,
    (byte)0x4C, (byte)0x4D, (byte)0x4E, (byte)0x4F,
    (byte)0x50, (byte)0x51, (byte)0x52, (byte)0x53, (byte)0x54, (byte)0x55,
    (byte)0x56, (byte)0x57, (byte)0x58, (byte)0x59, (byte)0x5A, (byte)0x5B,
    (byte)0x5C, (byte)0x5D, (byte)0x5E, (byte)0x5F,
    (byte)0x60, (byte)0x61, (byte)0x62, (byte)0x63, (byte)0x64, (byte)0x65,
    (byte)0x66, (byte)0x67, (byte)0x68, (byte)0x69, (byte)0x6A, (byte)0x6B,
    (byte)0x6C, (byte)0x6D, (byte)0x6E, (byte)0x6F,
    (byte)0x70, (byte)0x71, (byte)0x72, (byte)0x73, (byte)0x74, (byte)0x75,
    (byte)0x76, (byte)0x77, (byte)0x78, (byte)0x79, (byte)0x7A, (byte)0x7B,
    (byte)0x7C, (byte)0x7D, (byte)0x7E, (byte)0x7F,
    (byte)0x80, (byte)0x81, (byte)0x82, (byte)0x83, (byte)0x84, (byte)0x85,
    (byte)0x86, (byte)0x87, (byte)0x88, (byte)0x89, (byte)0x8A, (byte)0x8B,
    (byte)0x8C, (byte)0x8D, (byte)0x8E, (byte)0x8F,
    (byte)0x90, (byte)0x91, (byte)0x92, (byte)0x93, (byte)0x94, (byte)0x95,
    (byte)0x96, (byte)0x97, (byte)0x98, (byte)0x99, (byte)0x9A, (byte)0x9B,
    (byte)0x9C, (byte)0x9D, (byte)0x9E, (byte)0x9F,
    (byte)0xA0, (byte)0xA1, (byte)0xA2, (byte)0xA3, (byte)0xA4, (byte)0xA5,
    (byte)0xA6, (byte)0xA7, (byte)0xA8, (byte)0xA9, (byte)0xAA, (byte)0xAB,
    (byte)0xAC, (byte)0xAD, (byte)0xAE, (byte)0xAF,
    (byte)0xB0, (byte)0xB1, (byte)0xB2, (byte)0xB3, (byte)0xB4, (byte)0xB5,
    (byte)0xB6, (byte)0xB7, (byte)0xB8, (byte)0xB9, (byte)0xBA, (byte)0xBB,
    (byte)0xBC, (byte)0xBD, (byte)0xBE, (byte)0xBF,
    (byte)0xC0, (byte)0xC1, (byte)0xC2, (byte)0xC3, (byte)0xC4, (byte)0xC5,
    (byte)0xC6, (byte)0xC7, (byte)0xC8, (byte)0xC9, (byte)0xCA, (byte)0xCB,
    (byte)0xCC, (byte)0xCD, (byte)0xCE, (byte)0xCF,
    (byte)0xD0, (byte)0xD1, (byte)0xD2, (byte)0xD3, (byte)0xD4, (byte)0xD5,
    (byte)0xD6, (byte)0xD7, (byte)0xD8, (byte)0xD9, (byte)0xDA, (byte)0xDB,
    (byte)0xDC, (byte)0xDD, (byte)0xDE, (byte)0xDF,
    (byte)0xE0, (byte)0xE1, (byte)0xE2, (byte)0xE3, (byte)0xE4, (byte)0xE5,
    (byte)0xE6, (byte)0xE7, (byte)0xE8, (byte)0xE9, (byte)0xEA, (byte)0xEB,
    (byte)0xEC, (byte)0xED, (byte)0xEE, (byte)0xEF,
    (byte)0xF0, (byte)0xF1, (byte)0xF2, (byte)0xF3, (byte)0xF4, (byte)0xF5,
    (byte)0xF6, (byte)0xF7, (byte)0xF8, (byte)0xF9, (byte)0xFA, (byte)0xFB,
    (byte)0xFC, (byte)0xFD, (byte)0xFE, (byte)0xFF
  };

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    // GP-compliant JavaCard applet registration
    new com.android.cts.omapi.test.CtsAndroidOmapiTestApplet().register(bArray, (short) (bOffset + 1), bArray[bOffset]);
  }

  public void process(APDU apdu) {
    byte[] buf = apdu.getBuffer();
    short le, lc;
    short sendLen;
    byte p1 = buf[ISO7816.OFFSET_P1];
    byte p2 = buf[ISO7816.OFFSET_P2];

    if (selectingApplet()) {
      lc = buf[ISO7816.OFFSET_LC];
      if (buf[(short)(lc + ISO7816.OFFSET_CDATA - 1)]  == 0x31) {
        //AID: A000000476416E64726F696443545331
        ISOException.throwIt(ISO7816.SW_NO_ERROR);
      } else {
        //A000000476416E64726F696443545332
        sendLen = fillBerTLVBytes(SELECT_RESPONSE_DATA_LENGTH);
        le = apdu.setOutgoing();
        apdu.setOutgoingLength((short)sendLen);
        Util.arrayCopy(respBuf, (short)0, buf, (short)0, (short)respBuf.length);
        apdu.sendBytesLong(buf, respBuffOffset, sendLen);
        return;
      }
    }
    switch (buf[ISO7816.OFFSET_INS]) {
    case NO_DATA_INS_1:
    case NO_DATA_INS_2:
      ISOException.throwIt(ISO7816.SW_NO_ERROR);
      break;
    case DATA_INS_2:
      apdu.setIncomingAndReceive();
      //case 3 APDU, return 256 bytes data
      sendLen = fillBytes((short)256);
      le = apdu.setOutgoing();
      apdu.setOutgoingLength((short)sendLen);
      Util.arrayCopy(respBuf, (short)0, buf, (short)0, (short)respBuf.length);
      apdu.sendBytesLong(buf, respBuffOffset, sendLen);
      break;   
    case GET_RESPONSE_INS:
      apdu.setIncomingAndReceive();
        //ISO GET_RESPONSE command
        if (respBuf == null) {
          ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        } else {
          le = apdu.setOutgoing();
          sendLen = (short) (respBuf.length - respBuffOffset);
          sendLen = le > sendLen ? sendLen : le;
          apdu.setOutgoingLength(sendLen);
          apdu.sendBytesLong(respBuf, respBuffOffset, sendLen);
          respBuffOffset += sendLen;
          sendLen = (short) (respBuf.length - respBuffOffset);
          if (sendLen > 0) {
            if (sendLen > 256) sendLen = 0x00;
            ISOException.throwIt((short)(ISO7816.SW_BYTES_REMAINING_00 | sendLen));
          } else {
            respBuf = null;
          }
        }
      break;
    case DATA_INS_1:
      // return 256 bytes data
      sendLen = fillBytes((short)256);
      le = apdu.setOutgoing();
      apdu.setOutgoingLength((short) sendLen);
      Util.arrayCopy(respBuf, (short)0, buf, (short)0, (short)respBuf.length);
      apdu.sendBytesLong(buf, respBuffOffset, sendLen);
      break;
    case SW_62xx_APDU_INS:
      if (p1 > 16 || p1 < 1) {
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
      } else if (p2 == SW_62xx_DATA_APDU_P2){
        sendLen = fillBytes((short)3);
        le = apdu.setOutgoing();
        apdu.setOutgoingLength((short)sendLen);
        Util.arrayCopy(respBuf, (short)0, buf, (short)0, (short) respBuf.length);
        apdu.sendBytesLong(buf, respBuffOffset, sendLen);
        ISOException.throwIt(SW_62xx_resp[p1 -1]);
      } else if (p2 == SW_62xx_VALIDATE_DATA_P2){
        le = apdu.setOutgoing();
        apdu.setOutgoingLength((short) (SW_62xx_VALIDATE_DATA_RESP.length));
        Util.arrayCopy(SW_62xx_VALIDATE_DATA_RESP, (short) 0, buf, (short) 0,
          (short) SW_62xx_VALIDATE_DATA_RESP.length);
        buf[ISO7816.OFFSET_P1] = p1;
        apdu.sendBytesLong(buf, (short)0, (short) SW_62xx_VALIDATE_DATA_RESP.length);
        ISOException.throwIt(SW_62xx_resp[p1 -1]);
      } else {
        ISOException.throwIt(SW_62xx_resp[p1 -1]);
      }
      break;
    case SEGMENTED_RESP_INS_1:
    case SEGMENTED_RESP_INS_2:
      le = (short)((short)((p1 & 0xFF)<< 8) | (short)(p2 & 0xFF));
      sendLen = fillBytes(le);
      le = apdu.setOutgoing();
      sendLen = le > sendLen ? sendLen : le;
      if (sendLen > 0xFF) sendLen = 0xFF;
      apdu.setOutgoingLength(sendLen);
      apdu.sendBytesLong(respBuf, respBuffOffset, sendLen);
      respBuffOffset += sendLen;
      sendLen = (short) (respBuf.length - respBuffOffset);
      if (sendLen > 0) {
        if (sendLen > 256) sendLen = 0x00;
        ISOException.throwIt((short) (ISO7816.SW_BYTES_REMAINING_00 | sendLen));
      }
      break;
    case SEGMENTED_RESP_INS_3:
    case SEGMENTED_RESP_INS_4:
      le = (short)((short)((p1 & 0xFF)<< 8) | (short)(p2 & 0xFF));
      sendLen = fillBytes(le);
      le = apdu.setOutgoing();
      sendLen = le > sendLen ? sendLen : le;
      apdu.setOutgoingLength(sendLen);
      apdu.sendBytesLong(respBuf, respBuffOffset, sendLen);
      respBuffOffset += sendLen;
      sendLen = (short) (respBuf.length - respBuffOffset);
      if (sendLen > 0) {
        if (sendLen > 256) sendLen = 0x00;
        ISOException.throwIt((short) (ISO7816.SW_BYTES_REMAINING_00 | sendLen));
      }
      break;

    case SEGMENTED_RESP_INS_5:
      le = apdu.setOutgoing();
      if (le != 0xFF) {
        short buffer_len = (short)((short)((p1 & 0xFF)<< 8) | (short)(p2 & 0xFF));
        sendLen = fillBytes(buffer_len);
        sendLen = le > sendLen ? sendLen : le;
        apdu.setOutgoingLength(sendLen);
        apdu.sendBytesLong(respBuf, respBuffOffset, sendLen);
        respBuffOffset += sendLen;
        sendLen = (short) (respBuf.length - respBuffOffset);
        if (sendLen > 0) {
          if (sendLen > 256) sendLen = 0x00;
          ISOException.throwIt((short)(ISO7816.SW_BYTES_REMAINING_00 | sendLen));
        }
      } else {
        ISOException.throwIt((short)(ISO7816.SW_CORRECT_LENGTH_00 | 0xFF));
      }
      break;
    case CHECK_SELECT_P2_APDU:
      byte[] p2_00 = new byte[] {0x00};
      le = apdu.setOutgoing();
      apdu.setOutgoingLength((short) p2_00.length);
      Util.arrayCopy(p2_00, (short) 0, buf, (short) 0, (short) p2_00.length);
      apdu.sendBytesLong(buf, (short)0, (short)p2_00.length);
      break;
    default:
      // Case is not known.
      ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
  }

  /**
   * Fills APDU buffer with a pattern.
   *
   * @param le - length of outgoing data.
   */
  public short fillBerTLVBytes(short le) {
    //support length from 0x00 - 0x7FFF
    short total_len;
    short le_len = 1;
    if (le < (short)0x80) {
      le_len = 1;
    } else if (le < (short)0x100) {
      le_len = 2;
    } else {
      le_len = 3;
    }
    total_len = (short)(le + 2 + le_len);
    short i = 0;
    respBuf = new byte[total_len];
    respBuf[(short)i] = BER_TLV_TYPE;
    i = (short)(i + 1);
    respBuf[(short)i] = 0x00; //second byte of Type
    i = (short)(i + 1);
    if (le < (short)0x80) {
      respBuf[(short)i] = (byte)le;
      i = (short)(i + 1);
    } else if (le < (short)0x100) {
      respBuf[(short)i] = (byte)0x81;
      i = (short)(i + 1);
      respBuf[(short)i] = (byte)le;
      i = (short)(i + 1);
    } else {
      respBuf[(short)i] = (byte)0x82;
      i = (short)(i + 1);
      respBuf[(short)i] = (byte)(le >> 8);
      i = (short)(i + 1);
      respBuf[(short)i] = (byte)(le & 0xFF);
      i = (short)(i + 1);
    }
    while (i < total_len) {
      respBuf[i] = (byte)((i - 2 - le_len) & 0xFF);
      i = (short)(i + 1);
    }
    respBuf[(short)(respBuf.length - 1)] = (byte)0xFF;
    respBuffOffset = (short) 0;
    return total_len;
  }

  public short fillBytes(short total_len) {
    short i = 0;
    short len = total_len;
    respBuf = new byte[total_len];
    while (i < total_len) {
      if (len >= LENGTH_256) {
        Util.arrayCopyNonAtomic(resp_bytes256, (short)0, respBuf,
          (short)i, LENGTH_256);
        i = (short)(i + LENGTH_256);
        len = (short)(len - LENGTH_256);
      } else {
        respBuf[i] = (byte)(i & 0xFF);
        i = (short)(i + 1);
      }
    }
    respBuffOffset = (short) 0;
    //set the last byte to 0xFF for CTS validation
    respBuf[(short)(respBuf.length - 1)] = (byte)0xFF;
    return total_len;
  }
}
