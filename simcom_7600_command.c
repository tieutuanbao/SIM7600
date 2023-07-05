/**
  ******************************************************************************
  * @file           : simcom_a7600_handler.c
  * @brief          : Thư viện handler simcom 7600.
  * @author         : Tiêu Tuấn Bảo
  ******************************************************************************
  * @attention
  *
  * Thư viện lập trình sim A7600 hỗ trợ non-blocking
  *
  ******************************************************************************
  */

#include "simcom_7600_command.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char GSM7bit_table[] = "@£$¥èéùìòÇ\nØø\rÅåΔ_ÆæßÉ !\"#¤%&'()*+,-./0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ`¿abcdefghijklmnopqrstuvwxyzäöñüà";

void SIM7600_AT_Command(SIM7600_t *module,
                         char *atCommand,
                         SIM7600_responseHandler_t respHandler,
                         void *paramRet,
                        uint16_t waitSec) {
    /* Cấu hình nơi trả về dữ liệu */
    module->command.paramReturn = paramRet;
    /* Cài đặt lệnh */
    SIM7600_commandRaw(module, atCommand, respHandler, waitSec);
}

/* ------------------------------------------------- ATE ------------------------------------------------- */
void SIM7600_ATE_resp_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    if(memcmp(pData, "OK\r\r\n", 6)) {
        module->state = SIM7600_state_IDLE;
        module->command.respHandler = 0;
    }
}
void SIM7600_ATE_get(SIM7600_t *module) {
    SIM7600_commandRaw(module, (char *)"ATE", SIM7600_ATE_resp_handler, 1000);
}
void SIM7600_ATE_set(SIM7600_t *module, SIM7600_ATE_t ATE) {
    char command[4];
    if(ATE == SIM7600_ATE_OFF) {
        memcpy(command, "ATE0", 4);
        module->echo = SIM7600_ATE_OFF;
    }
    else {
        memcpy(command, "ATE1", 4);
        module->echo = SIM7600_ATE_ON;
    }
    SIM7600_commandRaw(module, command, SIM7600_ATE_resp_handler, 1000);
}

/* ------------------------------------------------- AT ------------------------------------------------- */
void SIM7600_AT_resp_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    if(memcmp(pData, "OK\r\n", 4)) {
        module->state = SIM7600_state_IDLE;
        module->command.respHandler = 0;
        return;
    }
    else if(memcmp(pData, "ERROR\r\n", 7)) {
        module->state = SIM7600_state_IDLE;
        module->command.respHandler = 0;
        return;
    }
}
void SIM7600_AT_get(SIM7600_t *module) {
    SIM7600_commandRaw(module, (char *)"AT", SIM7600_AT_resp_handler, 1000);
}

/* ------------------------------------------------- AT+CMGF ------------------------------------------------- */
void SIM7600_AT_CMGF_get_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    uint8_t smsFormat = 0;
    if(memcmp("+CMGF: ", pData, 7) == 0) {
        pData += strlen("+CMGF: ");
        smsFormat = atoi(pData);
        if(smsFormat == 0) {
            module->SMS_format = SIM7600_SMS_messFormat_PDU;
        }
        else if(smsFormat == 1) {
            module->SMS_format = SIM7600_SMS_messFormat_TEXT;
        }
    }
}
void SIM7600_AT_CMGF_set_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    if(memcmp("+CMGF: ", pData, 7) == 0) {
        pData += strlen("+CMGF: ");
        uint8_t smsFormat = atoi(pData);
        if(smsFormat == 0) {
            module->SMS_format = SIM7600_SMS_messFormat_PDU;
        }
        else if(smsFormat == 1) {
            module->SMS_format = SIM7600_SMS_messFormat_TEXT;
        }
    }
}
void SIM7600_AT_CMGF_get(SIM7600_t *module) {
    SIM7600_commandRaw(module, (char *)"AT+CMGF?", SIM7600_AT_CMGF_get_handler, 1000);
}
void SIM7600_AT_CMGF_set(SIM7600_t *module, SIM7600_SMS_messFormat_t format) {
    char command[10];
    memcpy(command, "AT+CMGF=", 8);
    if(format == SIM7600_SMS_messFormat_PDU) {
        module->command.lastCommand[8] = '0';
    }
    else {
        module->command.lastCommand[8] = '1';
    }
    module->command.lastCommand[9] = 0;
    SIM7600_commandRaw(module, command, (SIM7600_responseHandler_t)0, 1000);
}
/* ------------------------------------------------- AT+CMGR ------------------------------------------------- */
typedef struct {
    char *pDataStart;
    char *pDataEnd;
    uint16_t dataLen;
} SIM7600_content_t;

SIM7600_content_t SIM7600_AT_CMGR_getNextParam(char *pData, uint8_t dataLen) {
    SIM7600_content_t ret;
    ret.pDataStart = strchr((char const *)pData, '"');
    if(ret.pDataStart == 0) {
        ret.dataLen = 0;
    }
    else {
        ret.pDataStart++; // bỏ qua "
        ret.pDataEnd = strchr((char const *)ret.pDataStart, '"');
        if((pData + dataLen) > ret.pDataEnd) { // param vẫn trong đoạn dữ liệu cần tìm
            ret.dataLen = ret.pDataEnd - ret.pDataStart;
        }
        else {
            ret.pDataStart = 0;
            ret.pDataEnd = 0;
            ret.dataLen = 0;
        }
    }
    return ret;
}
static inline char *SIM7600_AT_CMGR_findComma(char *pData, uint16_t dataLen) {
    char *pDataRet = strchr(pData, ',');
    if(pDataRet == 0) { // check not null
        return 0;
    }
    if(pDataRet >= (pData + dataLen)) { // check not oversize
        return 0;
    }
    return pDataRet;
}
static inline bool SIM7600_isValidPointerData(char *pStart, char *pNow, char *pStop) {
    if(pNow == 0) {
        return false;
    }
    if((pNow >= pStart) && (pNow <= pStop)) {
        return true;
    }
    return false;
}
static uint8_t SIM7600_PDU_stringToHex(char *pData) {
    switch(*pData) {
        case 'a': case 'A': {
            return 0x0A;
            break;
        }
        case 'b': case 'B': {
            return 0x0B;
            break;
        }
        case 'c': case 'C': {
            return 0x0C;
            break;
        }
        case 'd': case 'D': {
            return 0x0D;
            break;
        }
        case 'e': case 'E': {
            return 0x0E;
            break;
        }
        case 'f': case 'F': {
            return 0x0F;
            break;
        }
        default: {
            if((*pData >= '0') && (*pData <= '9')) {
                return (*pData - '0');
            }
            break;
        }
    }
    return 0;
}
static uint16_t SIM7600_GSM7bit_parser(char *src, uint16_t srcLen, char *desc) {
    uint16_t indexChar = 0;
    uint16_t tempByte = 0;
    uint32_t tempShift = 0;
    uint8_t bitShift = 0;
    uint16_t indexByte = 0;
    for(;; indexByte++) {
        if(bitShift < 7) {
            /* - Chuyển đổi char hex thành byte - */
            /* Lấy LSB */
            if(indexChar >= srcLen) {break;}
            tempByte = SIM7600_PDU_stringToHex(src + 1);
            indexChar++;
            /* Lấy MSB */
            if(indexChar < srcLen) {
                tempByte |= SIM7600_PDU_stringToHex(src) << 4;
            }
            else {
                tempByte &= ~0xF0;
            }
            indexChar++;
            src+=2;
            /* Biến lưu tạm bit đã dịch */
            tempShift |= (tempByte << bitShift);
            bitShift++;
        }
        else {
            bitShift = 0;
        }
        /* Dịch trái */
        desc[indexByte] = tempShift & 0x7F;
        tempShift >>= 7;
    }
    desc[indexByte] = 0;
    return indexByte;
}

void SIM7600_AT_CMGR_content_resp_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    SIM7600_CMGR_param_t *paramParse = (SIM7600_CMGR_param_t *)module->command.paramReturn;
    memcpy(paramParse->content, pData, strstr(pData, "\r\n") - pData);
    if(paramParse->callback) {
        paramParse->callback(module, paramParse->stat, paramParse->phoneNumber, &paramParse->dateAndTime, paramParse->content);
    }
}

void SIM7600_AT_CMGR_resp_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    SIM7600_CMGR_param_t *paramParse = (SIM7600_CMGR_param_t *)module->command.paramReturn;
    SIM7600_content_t param;
    char *pDataNow = pData;
    char *pDataStop = pData + dataLen;
    if(memcmp((char const *)pDataNow, "+CMGR: ", 7) == 0) {
        pDataNow += strlen("+CMGR: ");
        dataLen -= strlen("+CMGR: ");
        /* --------- stat --------- */
        param = SIM7600_AT_CMGR_getNextParam(pDataNow, dataLen);
        pDataNow = param.pDataStart;
        if(pDataNow == 0) { return; }
        /* Lấy trạng thái tin nhắn */
        if(memcmp(pDataNow, "REC UNREAD", 10) == 0) {
            paramParse->stat = SIM7600_SMS_stat_REC_UNREAD;
        }
        else if(memcmp(pDataNow, "REC READ", 8) == 0) {
            paramParse->stat = SIM7600_SMS_stat_REC_READ;
        }
        else if(memcmp(pDataNow, "STO UNSENT", 10) == 0) {
            paramParse->stat = SIM7600_SMS_stat_STO_UNSENT;
        }
        else if(memcmp(pDataNow, "STO SENT", 8) == 0) {
            paramParse->stat = SIM7600_SMS_stat_STO_SENT;
        }
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- Tìm dấu dấu phẩy --------- */
        pDataNow = SIM7600_AT_CMGR_findComma(param.pDataEnd, dataLen);
        if(pDataNow == 0) { return; }
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- Phone number --------- */
        param = SIM7600_AT_CMGR_getNextParam(pDataNow, dataLen);
        pDataNow = param.pDataStart;
        if(pDataNow == 0) { return; }
        /* Lấy số điện thoại */
        memcpy(paramParse->phoneNumber, param.pDataStart, param.dataLen);
        pDataNow += param.dataLen;
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- Tìm dấu dấu phẩy --------- */
        pDataNow = SIM7600_AT_CMGR_findComma(param.pDataEnd, dataLen);
        if(pDataStop == 0) { return; }
        dataLen -= param.dataLen;
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- null --------- */
        param = SIM7600_AT_CMGR_getNextParam(pDataNow, dataLen);
        pDataNow = param.pDataStart;
        if(pDataNow == 0) { return; }
        pDataNow += param.dataLen;
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- Tìm dấu dấu phẩy --------- */
        pDataNow = SIM7600_AT_CMGR_findComma(param.pDataEnd, dataLen);
        if(pDataStop == 0) { return; }
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* --------- Date and Time --------- */
        param = SIM7600_AT_CMGR_getNextParam(pDataNow, dataLen);
        pDataNow = param.pDataStart;
        if(pDataNow == 0) { return; }
        dataLen = pDataStop - pDataNow; // Cập nhật dataLen
        /* Lấy date and time */
//        memcpy(paramParse->dateAndTime, param.pDataStart, param.dataLen);
        /* Chuyển handler tiếp theo */
        module->command.respHandler = (void (*)(void *, char *, uint16_t))SIM7600_AT_CMGR_content_resp_handler;
    }
}
void SIM7600_AT_CMGR_get(SIM7600_t *module, uint8_t index, SIM7600_CMGR_param_t *paramRet) {
    char command[10];
    /* Cấu hình nơi trả về dữ liệu */
    module->command.paramReturn = paramRet;
    /* Cài đặt lệnh */
    memcpy(command, "AT+CMGR=", 8);
    command[8] = index + '0';
    command[9] = 0;
    SIM7600_commandRaw(module, command, (SIM7600_responseHandler_t)SIM7600_AT_CMGR_resp_handler, 1000);
}

/* ------------------------------------------------- AT+CMGL ------------------------------------------------- */
uint8_t SIM7600_ucs2ToUtf8(uint16_t ucs2Char, char* utf8Array) {
    if (ucs2Char < 0x80) {
        utf8Array[0] = ucs2Char;
        utf8Array[1] = '\0';
        return 1;
    } else if (ucs2Char < 0x800) {
        utf8Array[0] = 0xC0 | (ucs2Char >> 6);
        utf8Array[1] = 0x80 | (ucs2Char & 0x3F);
        utf8Array[2] = '\0';
        return 2;
    } else {
        utf8Array[0] = 0xE0 | (ucs2Char >> 12);
        utf8Array[1] = 0x80 | ((ucs2Char >> 6) & 0x3F);
        utf8Array[2] = 0x80 | (ucs2Char & 0x3F);
        utf8Array[3] = '\0';
        return 3;
    }
}
void SIM7600_AT_CMGL_2_PDUMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    SIM7600_CMGL_paramReturn_t *paramParse = (SIM7600_CMGL_paramReturn_t *)module->command.paramReturn;
    paramParse->PDU.data = pData;
    paramParse->PDU.len = dataLen;
    /* Chuyển handler tiếp theo */
    module->command.respHandler = (__SIM7600_responseHandler_t)SIM7600_AT_CMGL_PDUMode_handler;
    if(paramParse->callback) {
        paramParse->callback(module, paramParse);
    }
}
void SIM7600_AT_CMGL_PDUMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    SIM7600_CMGL_paramReturn_t *paramParse = (SIM7600_CMGL_paramReturn_t *)module->command.paramReturn;
    char *pDataStart = pData;
    char *pDataEnd = pData + dataLen - 1;
    int statTemp = 0;
    if(memcmp("+CMGL: ", pData, 7) == 0) {
        pData += strlen("+CMGL: ");
        /* Index */
        paramParse->index = atoi(pData);
        /* Stat */
        pData = strchr(pData, ','); // Tìm dấu ,
        pData++; // bỏ qua dấu ,
        if(SIM7600_isValidPointerData(pDataStart, pData, pDataEnd) == false) {
            return;
        }
        statTemp = atoi(pData);
        if((statTemp >= 0) && (statTemp <= 3)) {
            paramParse->stat = (SIM7600_SMS_stat_t)statTemp;
        }
        /* "" */
        pData = strchr(pData, ','); // Tìm dấu ,
        pData++; // bỏ qua dấu ,
        if(SIM7600_isValidPointerData(pDataStart, pData, pDataEnd) == false) {
            return;
        }
        /* unknow param - ignore */
        /* Next handler */
        module->command.respHandler = (__SIM7600_responseHandler_t)SIM7600_AT_CMGL_2_PDUMode_handler;
    }
}

void SIM7600_AT_CMGL_2_TEXTMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    SIM7600_CMGL_paramReturn_t *paramParse = (SIM7600_CMGL_paramReturn_t *)module->command.paramReturn;
    uint16_t contentLen = strstr(pData, "\r\n") - pData;
    memcpy(paramParse->TEXT.content, pData, contentLen);
    paramParse->TEXT.content[contentLen] = 0;
    /* Chuyển handler tiếp theo */
    module->command.respHandler = (__SIM7600_responseHandler_t)SIM7600_AT_CMGL_TEXTMode_handler;
    if(paramParse->callback) {
        paramParse->callback(module, paramParse);
    }
}
void SIM7600_AT_CMGL_TEXTMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
#define CMGL_TEXTMode_NEXT_HANDLER module->command.respHandler = (__SIM7600_responseHandler_t)SIM7600_AT_CMGL_2_TEXTMode_handler
#define CMGL_TEXTMode_NEXT_NBYTE(n) \
    if(dataLen >= (n)) {dataLen -= (n); pData += (n);} \
    if(dataLen == 0) {CMGL_TEXTMode_NEXT_HANDLER; return;}
#define CMGL_TEXTMode_CHAR_ISEXISTED(n) \
    if(*pData != n) {CMGL_TEXTMode_NEXT_HANDLER; return;}
#define CMGL_TEXTMode_GETBYTE \
    *pData; \
    CMGL_TEXTMode_NEXT_NBYTE(1);

    SIM7600_CMGL_paramReturn_t *paramParse = (SIM7600_CMGL_paramReturn_t *)module->command.paramReturn;

    if(dataLen <= 2) {
        return;
    }
    dataLen -= 2; // ignore \r\n
    if(memcmp((char const *)pData, "+CMGL: ", 7) == 0) {
        CMGL_TEXTMode_NEXT_NBYTE(strlen("+CMGL: "));
        /* 1 --------- index --------- */
        paramParse->index = atoi(pData);
        CMGL_TEXTMode_NEXT_NBYTE(1); // Mặc định là phải có một số
        /* Check và bỏ qua dấu , */
        while(1) {
            if(*pData == ',') {
                break;
            }
            CMGL_TEXTMode_NEXT_NBYTE(1);
        }
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* 2 --------- stat --------- */
        /* Lấy trạng thái tin nhắn */
        if(memcmp(pData, "\"REC UNREAD\"", 12) == 0) {
            paramParse->stat = SIM7600_SMS_stat_REC_UNREAD;
            CMGL_TEXTMode_NEXT_NBYTE(12);
        }
        else if(memcmp(pData, "\"REC READ\"", 10) == 0) {
            paramParse->stat = SIM7600_SMS_stat_REC_READ;
            CMGL_TEXTMode_NEXT_NBYTE(10);
        }
        else if(memcmp(pData, "\"STO UNSENT\"", 12) == 0) {
            paramParse->stat = SIM7600_SMS_stat_STO_UNSENT;
            CMGL_TEXTMode_NEXT_NBYTE(12);
        }
        else if(memcmp(pData, "\"STO SENT\"", 10) == 0) {
            paramParse->stat = SIM7600_SMS_stat_STO_SENT;
            CMGL_TEXTMode_NEXT_NBYTE(10);
        }
        else {
            CMGL_TEXTMode_NEXT_HANDLER;
        }
        /* Check và bỏ qua dấu , */
        CMGL_TEXTMode_CHAR_ISEXISTED(',');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* 3 --------- Phone number --------- */
        /* Check và bỏ qua dấu " */
        CMGL_TEXTMode_CHAR_ISEXISTED('"');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Lấy số điện thoại */
        uint8_t indexChar = 0;
        for(indexChar = 0; *pData != '"'; indexChar++) {
            paramParse->TEXT.phoneNumber[indexChar] = CMGL_TEXTMode_GETBYTE;
        }
        paramParse->TEXT.phoneNumber[indexChar] = 0;
        /* Check và bỏ qua dấu " */
        CMGL_TEXTMode_CHAR_ISEXISTED('"');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Check và bỏ qua dấu , */
        CMGL_TEXTMode_CHAR_ISEXISTED(',');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* --------- Thông số chưa rõ - null --------- */
        /* Check và bỏ qua dấu " */
        CMGL_TEXTMode_CHAR_ISEXISTED('"');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Check và bỏ qua dấu " */
        CMGL_TEXTMode_CHAR_ISEXISTED('"');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Check và bỏ qua dấu , */
        CMGL_TEXTMode_CHAR_ISEXISTED(',');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* --------- Date and Time --------- */
        /* Check và bỏ qua dấu " */
        CMGL_TEXTMode_CHAR_ISEXISTED('"');
        CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Lấy date and time */
        paramParse->TEXT.dateAndTime.date.year = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.date.year += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        CMGL_TEXTMode_CHAR_ISEXISTED('/'); CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.date.month = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.date.month += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        CMGL_TEXTMode_CHAR_ISEXISTED('/'); CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.date.day = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.date.day += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        CMGL_TEXTMode_CHAR_ISEXISTED(','); CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.hour = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.hour += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        CMGL_TEXTMode_CHAR_ISEXISTED(':'); CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.minute = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.minute += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        CMGL_TEXTMode_CHAR_ISEXISTED(':'); CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.second = (*pData - '0') * 10; CMGL_TEXTMode_NEXT_NBYTE(1);
        paramParse->TEXT.dateAndTime.time.second += (*pData - '0'); CMGL_TEXTMode_NEXT_NBYTE(1);
        /* Bỏ qua các Char còn lại */
        /* Chuyển handler tiếp theo */
        CMGL_TEXTMode_NEXT_HANDLER;
    }
#undef CMGL_TEXTMode_NEXT_HANDLER
#undef CMGL_TEXTMode_NEXT_NBYTE
#undef CMGL_TEXTMode_CHAR_ISEXISTED
#undef CMGL_TEXTMode_GETBYTE
}

void SIM7600_AT_CMGL_get(SIM7600_t *module, SIM7600_CMGL_stat_t stat, SIM7600_responseHandler_t respHandler, SIM7600_CMGL_paramReturn_t *paramRet) {
    char command[25];
    /* Cấu hình nơi trả về dữ liệu */
    module->command.paramReturn = paramRet;
    /* Cài đặt lệnh */
    memset(command, 0, sizeof(command));
    memcpy(command, "AT+CMGL=", 8);
    switch (stat) {
        case SIM7600_CMGL_stat_REC_UNREAD: {
            if(module->SMS_format == SIM7600_SMS_messFormat_PDU) {
                strcat(command, "0");
            }
            else {
                strcat(command, "\"REC UNREAD\"");
            }
            break;
        }
        case SIM7600_CMGL_stat_REC_READ: {
            if(module->SMS_format == SIM7600_SMS_messFormat_PDU) {
                strcat(command, "1");
            }
            else {
                strcat(command, "\"REC READ\"");
            }
            break;
        }
        case SIM7600_CMGL_stat_STO_UNSENT: {
            if(module->SMS_format == SIM7600_SMS_messFormat_PDU) {
                strcat(command, "2");
            }
            else {
                strcat(command, "\"REC UNSEND\"");
            }
            break;
        }
        case SIM7600_CMGL_stat_STO_SENT: {
            if(module->SMS_format == SIM7600_SMS_messFormat_PDU) {
                strcat(command, "3");
            }
            else {
                strcat(command, "\"REC SEND\"");
            }
            break;
        }
        case SIM7600_CMGL_stat_ALL: {
            if(module->SMS_format == SIM7600_SMS_messFormat_PDU) {
                strcat(command, "4");
            }
            else {
                strcat(command, "\"ALL\"");
            }
            break;
        }
    }
    SIM7600_commandRaw(module, command, respHandler, 3000);
}

/* ------------------------------------------------- ATD ------------------------------------------------- */
//void sim7600_ATD_handler(SIM7600_t *module) {
//    if(strstr((char const *)module->serial.pData,(char const *)"NO ANSWER")!=0) {
//        module->call.last_resp = CALL_NO_ANSWER;
//        module->command.timeout = 0;
//    }
//    else if(strstr((char const *)module->serial.pData,(char const *)"BUSY")!=0) {
//        module->call.last_resp = CALL_BUSY;
//        module->command.timeout = 0;
//    }
//    else if(strstr((char const *)module->serial.pData,(char const *)"NO DIALTONE")!=0) {
//        module->call.last_resp = CALL_NO_DIALTONE;
//        module->command.timeout = 0;
//    }
//    else if(strstr((char const *)module->serial.pData,(char const *)"NO CARRIER")!=0) {
//        module->call.last_resp = CALL_NO_CARRIER;
//        module->command.timeout = 0;
//    }
//    else if(strstr((char const *)module->serial.pData,(char const *)"ERROR")!=0) {
//        module->call.last_resp = CALL_ERROR;
//        module->command.timeout = 0;
//    }
//    else if(strstr((char const *)module->serial.pData,(char const *)"RING")!=0) {
//        module->call.last_resp = CALL_RING;
//        module->command.timeout = 0;
//    }
//    else {
//        if(module->command.timeout == 0) {
//            module->state = IDLE;
//            module->command.last_resp = AT_NOTRESP;
//            module->command.resp_handler = 0;
//        }
//    }
//}
