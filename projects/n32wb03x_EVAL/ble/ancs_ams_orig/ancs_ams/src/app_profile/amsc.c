#include "rwip_config.h"
#if (BLE_AMS_CLIENT)
#include <string.h>
#include "amsc.h"
#include "amsc_task.h"
#include "ke_mem.h"
#include "prf_utils.h"
#include "prf_utils_128.h"
#include "prf.h"

static uint8_t amsc_init(struct prf_task_env *env, uint16_t *start_hdl, uint16_t app_task,
                         uint8_t sec_lvl, void *params)
{
    uint8_t i;
    struct amsc_env_tag *amsc_env = (struct amsc_env_tag *)ke_malloc(sizeof(struct amsc_env_tag), KE_MEM_ATT_DB);
    if (!amsc_env)
    {
        return GAP_ERR_INSUFF_RESOURCES;
    }
    memset(amsc_env, 0, sizeof(*amsc_env));
    env->env = (prf_env_t *)amsc_env;
    amsc_env->prf_env.app_task = app_task | (PERM_GET(sec_lvl, SVC_MI) ? PERM(PRF_MI, ENABLE) : PERM(PRF_MI, DISABLE));
    amsc_env->prf_env.prf_task = env->task | PERM(PRF_MI, ENABLE);
    env->id = TASK_ID_AMSC;
    amsc_task_init(&env->desc);
    env->desc.state = amsc_env->state;
    for (i = 0; i < AMSC_IDX_MAX; ++i) {
        amsc_env->env[i] = NULL;
        ke_state_set(KE_BUILD_ID(env->task, i), AMSC_FREE);
    }
    return GAP_ERR_NO_ERROR;
}

static void amsc_create(struct prf_task_env *env, uint8_t conidx)
{
    ke_state_set(KE_BUILD_ID(env->task, conidx), AMSC_IDLE);
}

static void amsc_cleanup(struct prf_task_env *env, uint8_t conidx, uint8_t reason)
{
    struct amsc_env_tag *e = (struct amsc_env_tag *)env->env;
	if (!e) {
        NS_LOG_INFO("[AMS] cleanup: e is NULL, skip\r\n");
        return;
    }
	
    if (e->env[conidx]) {
        prf_unregister_atthdl2gatt(&e->prf_env, conidx, &e->env[conidx]->ams.svc);
        ke_free(e->env[conidx]);
        e->env[conidx] = NULL;
    }
    ke_state_set(KE_BUILD_ID(env->task, conidx), AMSC_FREE);
}

static void amsc_destroy(struct prf_task_env *env)
{
    uint8_t i;
    struct amsc_env_tag *e = (struct amsc_env_tag *)env->env;
    if (!e) return;
    for (i = 0; i < AMSC_IDX_MAX; ++i)
    {
        if (e->env[i])
        {
            amsc_cleanup(env, i, 0);
        }
    }
    env->env = NULL;
    ke_free(e);
}

const struct prf_task_cbs amsc_itf = {
    (prf_init_fnct)amsc_init,
    amsc_destroy,
    amsc_create,
    amsc_cleanup,
};

const struct prf_task_cbs *amsc_prf_itf_get(void)
{
    return &amsc_itf;
}

void amsc_enable_rsp_send(struct amsc_env_tag *e, uint8_t conidx, uint8_t status)
{
    struct amsc_enable_rsp *rsp = KE_MSG_ALLOC(AMSC_ENABLE_RSP,
        prf_dst_task_get(&e->prf_env, conidx),
        prf_src_task_get(&e->prf_env, conidx),
        amsc_enable_rsp);

    if (!rsp)
    {
        NS_LOG_ERROR("[AMS] enable response alloc failed\r\n");
        return;
    }

    rsp->status = status;

    if (status == GAP_ERR_NO_ERROR) {
        /* Register the discovered AMS handle range with GATT so peer
         * notifications/indications are routed to this profile task.
         * Without this call the CCCD write can succeed, but GATTC_EVENT_IND
         * will never be delivered to amsc_task. */
		 if (!e->env[conidx]) {
            NS_LOG_INFO("[AMS] enable_rsp: env[conidx] is NULL (already cleaned up), skip register\r\n");
            ke_msg_send(rsp);
            return;
        }
		 
        prf_register_atthdl2gatt(&e->prf_env, conidx, &e->env[conidx]->ams.svc);
        NS_LOG_INFO("[AMS] GATT event range registered: 0x%04X-0x%04X\r\n",
                    e->env[conidx]->ams.svc.shdl, e->env[conidx]->ams.svc.ehdl);
        ke_state_set(prf_src_task_get(&e->prf_env, conidx), AMSC_IDLE);
    }

    ke_msg_send(rsp);
}

void amsc_cfg_rsp_send(struct amsc_env_tag *e, uint8_t conidx, uint8_t op, uint8_t status)
{
    struct amsc_wr_cfg_rsp *rsp = KE_MSG_ALLOC(AMSC_WR_CFG_RSP,
        prf_dst_task_get(&e->prf_env, conidx),
        prf_src_task_get(&e->prf_env, conidx),
        amsc_wr_cfg_rsp);

    if (!rsp)
    {
        NS_LOG_ERROR("[AMS] cfg response alloc failed\r\n");
        return;
    }

    rsp->op_code = op;
    rsp->status = status;
    ke_msg_send(rsp);
}

void amsc_entity_value_ind(struct amsc_env_tag *e, uint8_t conidx, uint8_t entity,
                           uint8_t attr, uint8_t flags, const uint8_t *value, uint16_t len)
{
    struct amsc_entity_update_ind *ind = KE_MSG_ALLOC_DYN(AMSC_ENTITY_UPDATE_IND,
        prf_dst_task_get(&e->prf_env, conidx), prf_src_task_get(&e->prf_env, conidx),
        amsc_entity_update_ind, len);

    if (!ind)
    {
        NS_LOG_ERROR("[AMS] entity update indication alloc failed, len=%u\r\n", len);
        return;
    }

    ind->entity = entity;
    ind->attr = attr;
    ind->flags = flags;
    ind->length = len;
    if (len)
    {
        memcpy(ind->value, value, len);
    }
    ke_msg_send(ind);
}

void amsc_remote_cmd_ind(struct amsc_env_tag *e, uint8_t conidx, const uint8_t *value, uint16_t len)
{
    struct amsc_remote_cmd_ind *ind = KE_MSG_ALLOC_DYN(AMSC_REMOTE_CMD_IND,
        prf_dst_task_get(&e->prf_env, conidx), prf_src_task_get(&e->prf_env, conidx),
        amsc_remote_cmd_ind, len);

    if (!ind)
    {
        NS_LOG_ERROR("[AMS] remote command indication alloc failed, len=%u\r\n", len);
        return;
    }

    ind->length = len;
    if (len)
    {
        memcpy(ind->value, value, len);
    }
    ke_msg_send(ind);
}

void amsc_attr_read_rsp(struct amsc_env_tag *e, uint8_t conidx, uint8_t entity,
                        uint8_t attr, const uint8_t *value, uint16_t len, uint8_t status)
{
    struct amsc_entity_attribute_ind *ind = KE_MSG_ALLOC_DYN(AMSC_ENTITY_ATTRIBUTE_IND,
        prf_dst_task_get(&e->prf_env, conidx), prf_src_task_get(&e->prf_env, conidx),
        amsc_entity_attribute_ind, len);

    if (!ind)
    {
        NS_LOG_ERROR("[AMS] attribute indication alloc failed, len=%u\r\n", len);
        return;
    }

    ind->entity = entity;
    ind->attr = attr;
    ind->status = status;
    ind->length = len;
    if (len)
    {
        memcpy(ind->value, value, len);
    }
    ke_msg_send(ind);
}
#endif
