#ifndef APP_AMS_H_
#define APP_AMS_H_
#include "rwip_config.h"
#if (BLE_APP_PRESENT)
void app_ams_init(void);
void app_ams_add_ams(void);
void app_ams_enable(uint8_t conidx);
void app_ams_enable_notifications(uint8_t conidx);
void app_ams_remote_command(uint8_t conidx, uint8_t command);
void app_ams_register(uint8_t conidx, uint8_t entity, const uint8_t *attrs, uint8_t count);
void app_ams_read_attribute(uint8_t conidx, uint8_t entity, uint8_t attr);
void app_ams_test_start(uint8_t conidx);
void app_ams_test_stop(void);
void app_ams_test_timer_proc(void);
extern const struct app_subtask_handlers app_ams_handlers;
#endif
#endif
