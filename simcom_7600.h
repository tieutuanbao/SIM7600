/**
  ******************************************************************************
  * @file           : simcom_a7600.h
  * @brief          : Thư viện driver simcom 7600.
    *    @author                    :    Tiêu Tuấn Bảo
  ******************************************************************************
  * @attention
  *
    * Thư viện lập trình sim A7600 hỗ trợ non-blocking
    *
  ******************************************************************************
  */

#ifndef __SIMCOM_7600_H__
#define __SIMCOM_7600_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define SIM7600_BUFFER_SIZE     500
#define SIM7600_SMS_SIZE        256

#define SIM7600_END_CMD         "\r\n\0"
#define SIM7600_END_SMS         "\X1a\0"

#if defined(STM32)
    #define SIM7600_TickType_t  uint32_t
#elif defined(ESP32)
    #define SIM7600_TickType_t  int64_t
#else
    #define SIM7600_TickType_t  uint32_t
#endif
/**
 *  Trạng thái hoạt động
 */
typedef enum {
    SIM7600_state_WAIT_PWR_ON,    // Chờ khởi động, mặc định 14 giây
    SIM7600_state_IDLE,           // Đang rảnh
    SIM7600_state_BUSY,           // Chờ response
    SIM7600_state_WAIT_RESET,     // Chờ reset
    SIM7600_state_TIMEOUT
} SIM7600_state_t;

typedef enum {
    SIM7600_commandState_OK,
    SIM7600_commandState_ERROR
} SIM7600_commandState_t;

typedef enum {
    SIM7600_response_OK,
    SIM7600_response_ERROR,
    SIM7600_response_NOT_RESPONSE
} SIM7600_response_t;

typedef enum {
    SIM7600_ATE_OFF = 0,
    SIM7600_ATE_ON
} SIM7600_ATE_t;

/**
 *  @brief Trả về sau cuộc gọi
 */
typedef enum {
    SIM7600_CallStatus_NO_ANSWER,
    SIM7600_CallStatus_BUSY,
    SIM7600_CallStatus_NO_DIALTONE,
    SIM7600_CallStatus_NO_CARRIER,
    SIM7600_CallStatus_ERROR,
    SIM7600_CallStatus_RING
} SIM7600_CallStatus_t;

typedef enum {
    SIM7600_COMMAND_AT,
    SIM7600_COMMAND_CMGR,
    SIM7600_COMMAND_CMGS,
    SIM7600_COMMAND_ATD,
    SIM7600_COMMAND_ATH
} SIM7600_CommandName_t;

typedef enum {
    SIM7600_SMS_stat_REC_UNREAD,
    SIM7600_SMS_stat_REC_READ,
    SIM7600_SMS_stat_STO_UNSENT,
    SIM7600_SMS_stat_STO_SENT
} SIM7600_SMS_stat_t;

typedef struct {
    struct {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    } time;
    struct {
        uint8_t day;
        uint8_t month;
        uint8_t year;
    } date;
} SIM7600_SMS_dateTime_t;

typedef struct {
    SIM7600_SMS_stat_t stat;
    char *phoneNumber;
    SIM7600_SMS_dateTime_t dateAndTime;
    char *content;
    void (*callback)(void *module, SIM7600_SMS_stat_t stat, char *phoneNumber, SIM7600_SMS_dateTime_t *dateAndTime, char *content);
} SIM7600_CMGR_param_t;

typedef enum {
    SIM7600_CMGL_stat_REC_UNREAD,
    SIM7600_CMGL_stat_REC_READ,
    SIM7600_CMGL_stat_STO_UNSENT,
    SIM7600_CMGL_stat_STO_SENT,
    SIM7600_CMGL_stat_ALL
} SIM7600_CMGL_stat_t;

typedef enum {
    SIM7600_SMS_contentType_GSM,
    SIM7600_SMS_contentType_IRA,

} SIM7600_SMS_contentType_t;

typedef struct SIM7600_CMGL_paramReturn_struct {
    uint16_t index;
    SIM7600_SMS_stat_t stat;
    union {
        struct {
            char *phoneNumber;
            SIM7600_SMS_dateTime_t dateAndTime;
            char *content;
        } TEXT;
        struct {
            char *data;
            uint16_t len;
        } PDU;
    };
    void (*callback)(void *module, struct SIM7600_CMGL_paramReturn_struct *ret);
} SIM7600_CMGL_paramReturn_t;

typedef enum {
    SIM7600_SMS_messFormat_PDU,
    SIM7600_SMS_messFormat_TEXT
} SIM7600_SMS_messFormat_t;

typedef void (*__SIM7600_responseHandler_t)(void *module, char *pData, uint16_t dataLen);

typedef struct {
    void (*printf)(char *str, ...);
    SIM7600_SMS_messFormat_t SMS_format;
    /* public */
    void (*send)(void *userData, char *pData, uint16_t dataLen); // Hàm gửi dữ liệu giao tiếp UART
    void (*PWRKEY_set)(bool level);  // Hàm điều khiển chân PWRKey
    void (*RESET_set)(bool level);  // Hàm điều khiển chân RESET
    bool (*RI_get)();   // Hàm lấy tín hiệu chân RI
    void *userData;

    SIM7600_TickType_t lastTick;
    /* Cấu trúc tạm lưu SMS */
    struct {
        struct {
            uint8_t available;
            uint8_t phone[15];
            uint8_t time[20];
            uint8_t content[SIM7600_SMS_SIZE];
        } recv;
        struct {
            char *content;
            uint16_t len;
        } send;
    } sms;
    /* Cấu trúc tạm lưu cuộc gọi */
    struct {
        uint8_t phone[15];
        SIM7600_CallStatus_t last_resp;
    } call;
    /* Cấu trúc xử lý non-blocking */
    struct {
        uint32_t timeout;
        SIM7600_response_t last_resp;
        char lastCommand[50];
        __SIM7600_responseHandler_t respHandler;
        void *paramReturn;
    } command;
    volatile SIM7600_state_t state;

    SIM7600_ATE_t echo;

    void (*error_cb)(void *module);
    void (*fullSMS_cb)(void *module);
} SIM7600_t;

typedef void (*SIM7600_responseHandler_t)(SIM7600_t *module, char *pData, uint16_t dataLen);

typedef struct {
    void (*completeCommand_cb)();
} SIM7600_extCallback_t;

void SIM7600_init(SIM7600_t *module,
                void (*PWRKEY_set)(bool state),
                void (*RESET_set)(bool state),
                bool (*RI_get)(),
                void (*send)(void *thisModule, char *pData, uint16_t dataLen),
                void *userData,
                bool isReset);
SIM7600_commandState_t SIM7600_commandRaw(SIM7600_t *module,
                                    char *ATCommand,
                                    SIM7600_responseHandler_t respHandler,
                                    uint16_t timeout);
bool SIM7600_isBusy(SIM7600_t *module);
void SIM7600_powerHardControl(SIM7600_t *module, bool state);
void SIM7600_data_handler(SIM7600_t *module, char *pData, uint16_t dataLen);
void SIM7600_exec(SIM7600_t *module);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SIMCOM_7600_H__ */
