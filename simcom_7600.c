/**
  ******************************************************************************
  * @file           : simcom_7600.c
  * @brief          : Thư viện driver simcom 7600.
  * @author         : Tiêu Tuấn Bảo
  ******************************************************************************
  * @attention
  *
  * Thư viện lập trình sim 7600 non-blocking
  *
  ******************************************************************************
  */

#include "simcom_7600.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "millis.h"
#include "delay.h"

/**
 *  @brief Hàm khởi tạo module sim800
 *  @param module: module SIM7600 đang dùng
 */
void SIM7600_init(SIM7600_t *module,
                  void (*PWRKEY_set)(bool state),
                  void (*RESET_set)(bool state),
                  bool (*RI_get)(),
                  void (*send)(void *thisModule, char *pData, uint16_t dataLen),
                  void *userData,
                  bool isReset)
{
    /* Cài đặt driver IO - nếu có */
    module->PWRKEY_set = PWRKEY_set;
    module->RESET_set = RESET_set;
    module->RI_get = RI_get;
    module->send = send;
    module->userData = userData;
    /* set null param */
    module->command.respHandler = 0;
    /* Power on module */
    if(isReset == true) {
        SIM7600_powerHardControl(module, true);
    }
    else {
        module->state = SIM7600_state_IDLE;
    }
}

/**
 * @brief Hàm điều khiển nguồn SIM7600 qua chân PWRKEY
 * @param state true:On false:Off
 */
void SIM7600_powerHardControl(SIM7600_t *module, bool state) {
    if(state == true) {
        module->state = SIM7600_state_WAIT_PWR_ON;
        module->PWRKEY_set(false);
        module->lastTick = millis();
    }
}
/**
 * @brief Kiểm tra module SIM7600 đang Busy
 * 
 * @param module 
 * @return true 
 * @return false 
 */
bool SIM7600_isBusy(SIM7600_t *module) {
    if(module->state != SIM7600_state_IDLE) {
        return true;
    }
    return false;
}
/**
 *  @brief: Hàm giao tiếp dữ liệu ATCommand dạng Raw
 *  @param module Module đang sử dụng
 *  @param ATCommand Lệnh AT kèm theo \r\n
 *  @param responseHandler Callback cho response
 *  @param timeout Thời gian tối đa chờ phản hồi
 */
SIM7600_commandState_t SIM7600_commandRaw(SIM7600_t *module,
                                    char *ATCommand,
                                    SIM7600_responseHandler_t respHandler,
                                    uint16_t timeout)
{
    uint8_t atLen = 0;
    if(module->state != SIM7600_state_IDLE) {
        return SIM7600_commandState_ERROR;
    }
    module->lastTick = millis();
    module->command.timeout = timeout;
    module->state = SIM7600_state_BUSY;
    module->command.respHandler = (__SIM7600_responseHandler_t)respHandler;
    memset(module->command.lastCommand, 0, sizeof(module->command.lastCommand));
    memcpy(module->command.lastCommand, ATCommand, strlen(ATCommand));
    atLen = strlen((const char *)module->command.lastCommand);
    module->command.lastCommand[atLen] = '\r';
    module->command.lastCommand[atLen + 1] = '\n';
    module->send(module, module->command.lastCommand, strlen((char const *)module->command.lastCommand));
    module->command.lastCommand[atLen] = 0;
    return SIM7600_commandState_OK;
}

/**
 * @brief Hàm xử lý phản hồi đột ngột từ module
 * 
 * @param module 
 */
void SIM7600_suddenly_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    if(strstr((char const *)pData, "+CMTI:")) {        // Must be config AT+CNMI=2,1
        if((pData[8] == 'S') && (pData[9] == 'M')) {
            module->sms.recv.available = atoi((char const *)&pData[12]);
        }
    }
    else if(strstr((char const *)pData, "+SMS FULL\r\n")) {
        if(module->fullSMS_cb) {
            module->fullSMS_cb(module);
        }
    }
    else if(strstr((char const *)pData, "RING")) {
        return;
    }
}

void SIM7600_data_handler(SIM7600_t *module, char *pData, uint16_t dataLen) {
    if(dataLen > 0) {
        if(module->echo == SIM7600_ATE_ON) {
            if(memcmp(pData, module->command.lastCommand, strlen(module->command.lastCommand)) != 0) {
                return;
            }
        }
        if(dataLen > 2) {
            if(module->command.respHandler != 0) {
                module->command.respHandler(module, pData, dataLen);
            }
        }
        else {
            module->command.respHandler = 0;
        }
        SIM7600_suddenly_handler(module, pData, dataLen);
    }
}

void SIM7600_exec(SIM7600_t *module) {
    switch(module->state) {
        case SIM7600_state_WAIT_PWR_ON: {
            if((millis() - module->lastTick) >= 125000) { // Power on trong 12.5s
                module->state = SIM7600_state_IDLE;
            }
            break;
        }
        case SIM7600_state_BUSY: {
            if((millis() - module->lastTick) >= module->command.timeout) {
                module->state = SIM7600_state_IDLE;
                module->command.respHandler = 0;
            }
            break;
        }
        default: {
            break;
        }
    }
}
