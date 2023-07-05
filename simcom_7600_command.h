#ifndef __SIM7600_HANDLER_H__
#define __SIM7600_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "simcom_7600.h"
#include <stdint.h>
#include <stdbool.h>

void SIM7600_AT_Command(SIM7600_t *module,
                        char *atCommand,
                        SIM7600_responseHandler_t respHandler,
                        void *paramRet,
                        uint16_t waitSec
                        );

void SIM7600_ATE_get(SIM7600_t *module);
void SIM7600_ATE_set(SIM7600_t *module, SIM7600_ATE_t ATE);
void SIM7600_AT_get(SIM7600_t *module);

/* AT+CMGF */
void SIM7600_AT_CMGF_get_handler(SIM7600_t *module, char *pData, uint16_t dataLen);
void SIM7600_AT_CMGF_set_handler(SIM7600_t *module, char *pData, uint16_t dataLen);
void SIM7600_AT_CMGF_get(SIM7600_t *module);
void SIM7600_AT_CMGF_set(SIM7600_t *module, SIM7600_SMS_messFormat_t format);
/* AT+CMGR */
void SIM7600_AT_CMGR_get(SIM7600_t *module, uint8_t index, SIM7600_CMGR_param_t *paramRet);
/* AT+CMGL */
void SIM7600_AT_CMGL_TEXTMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen);
void SIM7600_AT_CMGL_PDUMode_handler(SIM7600_t *module, char *pData, uint16_t dataLen);
void SIM7600_AT_CMGL_get(SIM7600_t *module, SIM7600_CMGL_stat_t stat, SIM7600_responseHandler_t respHandler, SIM7600_CMGL_paramReturn_t *paramRet);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SIM7600_HANDLER_H__ */
