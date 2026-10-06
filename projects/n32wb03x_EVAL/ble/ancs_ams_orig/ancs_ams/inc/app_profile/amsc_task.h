#ifndef AMSC_TASK_H_
#define AMSC_TASK_H_

#include "ke_task.h"
#include "gattc_task.h"
#include "co_error.h"
#include "amsc.h"

enum amsc_state {
    AMSC_FREE,
    AMSC_IDLE,
    AMSC_DISCOVERING,
    AMSC_BUSY,
    AMSC_STATE_MAX
};

enum {
    AMSC_ENABLE_REQ = TASK_FIRST_MSG(TASK_ID_AMSC),
    AMSC_ENABLE_RSP,
    AMSC_WR_CFG_REQ,
    AMSC_WR_CFG_RSP,
    AMSC_REMOTE_CMD_REQ,
    AMSC_ENTITY_UPDATE_REQ,
    AMSC_ENTITY_UPDATE_IND,
    AMSC_REMOTE_CMD_IND,
    AMSC_ENTITY_ATTRIBUTE_REQ,
    AMSC_ENTITY_ATTRIBUTE_IND,
    AMSC_CMP_EVT,
};

enum {
    AMSC_INVALID_OP = 0,
    AMSC_CFG_REMOTE_CMD,
    AMSC_CFG_ENTITY_UPDATE,
    AMSC_REG_PLAYER,
    AMSC_REG_QUEUE,
    AMSC_REG_TRACK,
    AMSC_REMOTE_COMMAND,
    AMSC_ENTITY_ATTRIBUTE_READ,
};

struct amsc_enable_req {
    uint8_t con_type;
    struct amsc_content ams;
};
struct amsc_enable_rsp { uint8_t status; };
struct amsc_wr_cfg_req { uint8_t op_code; uint8_t enable; };
struct amsc_wr_cfg_rsp { uint8_t op_code; uint8_t status; };
struct amsc_remote_cmd_req { uint8_t command; };
struct amsc_entity_update_req {
    uint8_t entity;
    uint8_t attr[4];
    uint8_t attr_count;
};
struct amsc_entity_update_ind {
    uint8_t entity;
    uint8_t attr;
    uint8_t flags;
    uint16_t length;
    uint8_t value[__ARRAY_EMPTY];
};
struct amsc_remote_cmd_ind {
    uint16_t length;
    uint8_t value[__ARRAY_EMPTY];
};
struct amsc_entity_attribute_req { uint8_t entity; uint8_t attr; };
struct amsc_entity_attribute_ind {
    uint8_t entity;
    uint8_t attr;
    uint8_t status;
    uint16_t length;
    uint8_t value[__ARRAY_EMPTY];
};
struct amsc_cmp_evt { uint8_t operation; uint8_t status; };

extern const struct ke_state_handler amsc_default_handler;

#endif
