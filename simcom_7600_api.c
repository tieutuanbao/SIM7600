#include "simcom_7600_api.h"
#include "simcom_7600_command.h"
#include <string.h>
#include <stdlib.h>

void SIM7600_dial(SIM7600_t *module, char *phoneNumber, uint32_t timeout) {
//    module->send("ATD", 3);
//    module->send(phoneNumber, strlen((char const *)phoneNumber));
//    module->send(";", 1);
//    module->send(SIM7600_END_CMD, 2);
//    module->command.timeout = timeout;
}

void SIM7600_readSMSAt(SIM7600_t *module, uint16_t index) {
    SIM7600_CMGR_param_t paramGet;
    paramGet.content = (char *)malloc(1024);
//    paramGet.dateAndTime = (char *)malloc(50);
    paramGet.phoneNumber = (char *)malloc(50);
    SIM7600_AT_CMGR_get(module, 0, &paramGet);
}

void SIM7600_readAllSMS(SIM7600_t *module) {

}
