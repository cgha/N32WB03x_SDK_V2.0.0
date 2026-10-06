#ifndef AMSC_H_
#define AMSC_H_

#include "rwip_config.h"
#include "rwip_task.h"
#include "ke_task.h"
#include "prf.h"
#include "prf_types.h"
#include "ams_common.h"


#define AMSC_IDX_MAX BLE_CONNECTION_MAX

enum amsc_chars {
    AMSC_CHAR_REMOTE_CMD,
    AMSC_CHAR_ENTITY_UPDATE,
    AMSC_CHAR_ENTITY_ATTRIBUTE,
    AMSC_CHAR_MAX
};

enum amsc_descs {
    AMSC_DESC_ENTITY_UPDATE_CCC,
    AMSC_DESC_MAX
};

struct amsc_content {
    struct prf_svc svc;
    struct prf_char_inf chars[AMSC_CHAR_MAX];
    struct prf_char_desc_inf descs[AMSC_DESC_MAX];
};

struct amsc_cnx_env {
    uint8_t last_op;
    uint8_t nb_svc;
    uint8_t reg_step;
    uint8_t pending_entity;
    uint8_t pending_attr;
    struct amsc_content ams;
};

struct amsc_env_tag {
    prf_env_t prf_env;
    struct amsc_cnx_env *env[AMSC_IDX_MAX];
    ke_state_t state[AMSC_IDX_MAX];
};

const struct prf_task_cbs *amsc_prf_itf_get(void);
void amsc_enable_rsp_send(struct amsc_env_tag *env, uint8_t conidx, uint8_t status);
void amsc_cfg_rsp_send(struct amsc_env_tag *env, uint8_t conidx, uint8_t op, uint8_t status);
void amsc_entity_value_ind(struct amsc_env_tag *env, uint8_t conidx, uint8_t entity,
                           uint8_t attr, uint8_t flags, const uint8_t *value, uint16_t len);
void amsc_remote_cmd_ind(struct amsc_env_tag *env, uint8_t conidx, const uint8_t *value, uint16_t len);
void amsc_attr_read_rsp(struct amsc_env_tag *env, uint8_t conidx, uint8_t entity,
                        uint8_t attr, const uint8_t *value, uint16_t len, uint8_t status);
void amsc_task_init(struct ke_task_desc *p_task_desc);

#endif
