#include "rwip_config.h"
#if (BLE_APP_PRESENT)
#include <string.h>
#include <stdio.h>
#include "gapm_task.h"
#include "ns_ble.h"
#include "prf.h"
#include "prf_utils.h"
#include "ke_mem.h"
#include "app_ams.h"
#include "amsc_task.h"
#include "amsc.h"
#include "ke_timer.h"
#include "app_ble.h"


#define AMS_TEST_AUTO_CMD_ENABLE       1
#define AMS_TEST_CMD_INTERVAL          1000

static uint8_t s_ams_test_conidx = 0xFF;
static uint8_t s_ams_test_cmd_index = 0;
static uint8_t s_ams_test_started = 0;

static const uint8_t s_ams_test_commands[] =
{
    AMS_CMD_PREVIOUS_TRACK,
    AMS_CMD_NEXT_TRACK,
    AMS_CMD_VOLUME_UP,
    AMS_CMD_VOLUME_DOWN,
    AMS_CMD_TOGGLE_PLAY_PAUSE,
};

static const char *s_ams_test_command_name(uint8_t command)
{
    switch (command)
    {
    case AMS_CMD_PREVIOUS_TRACK:
        return "PREVIOUS_TRACK";
    case AMS_CMD_NEXT_TRACK:
        return "NEXT_TRACK";
    case AMS_CMD_VOLUME_UP:
        return "VOLUME_UP";
    case AMS_CMD_VOLUME_DOWN:
        return "VOLUME_DOWN";
    case AMS_CMD_TOGGLE_PLAY_PAUSE:
        return "TOGGLE_PLAY_PAUSE";
    default:
        return "UNKNOWN";
    }
}

void app_ams_test_start(uint8_t conidx)
{
#if (AMS_TEST_AUTO_CMD_ENABLE)
    s_ams_test_conidx = conidx;
    s_ams_test_cmd_index = 0;
    s_ams_test_started = 1;
    NS_LOG_INFO("[AMS TEST] start auto command test, conidx=%u\r\n", conidx);
    ke_timer_set(APP_AMS_TEST_CMD_EVT, TASK_APP, AMS_TEST_CMD_INTERVAL);
#else
    (void)conidx;
#endif
}

void app_ams_test_stop(void)
{
    if (s_ams_test_started)
    {
        ke_timer_clear(APP_AMS_TEST_CMD_EVT, TASK_APP);
    }

    s_ams_test_started = 0;
    s_ams_test_conidx = 0xFF;
    s_ams_test_cmd_index = 0;
}

void app_ams_test_timer_proc(void)
{
    uint8_t command;

    if (!s_ams_test_started)
    {
        return;
    }

    if (s_ams_test_conidx == 0xFF)
    {
        app_ams_test_stop();
        return;
    }

    command = s_ams_test_commands[s_ams_test_cmd_index];

    NS_LOG_INFO("[AMS TEST] send cmd=%u %s, conidx=%u\r\n",
                command,
                s_ams_test_command_name(command),
                s_ams_test_conidx);

    app_ams_remote_command(s_ams_test_conidx, command);

    s_ams_test_cmd_index++;

    if (s_ams_test_cmd_index >= sizeof(s_ams_test_commands))
    {
        s_ams_test_cmd_index = 0;
    }

    ke_timer_set(APP_AMS_TEST_CMD_EVT, TASK_APP, AMS_TEST_CMD_INTERVAL);
}

void app_ams_init(void)
{
    struct prf_task_t prf = {0};
    struct prf_get_func_t get_func = {0};
    prf.prf_task_id = TASK_ID_AMSC;
    prf.prf_task_handler = &app_ams_handlers;
    /* ns_ble_prf_task_register expects the application-side message handler table. */
    ns_ble_prf_task_register(&prf);

    get_func.task_id = TASK_ID_AMSC;
    get_func.prf_itf_get_func = amsc_prf_itf_get;
    prf_get_itf_func_register(&get_func);
}

void app_ams_add_ams(void)
{
    NS_LOG_INFO("[AMS] add profile\r\n");
    struct gapm_profile_task_add_cmd *req = KE_MSG_ALLOC(GAPM_PROFILE_TASK_ADD_CMD,
                                                         TASK_GAPM, TASK_APP,
                                                         gapm_profile_task_add_cmd);
    req->operation = GAPM_PROFILE_TASK_ADD;
    req->sec_lvl = PERM(SVC_AUTH, NO_AUTH);
    req->prf_task_id = TASK_ID_AMSC;
    req->app_task = TASK_APP;
    req->start_hdl = 0;
    ke_msg_send(req);
    app_ams_init();
    NS_LOG_INFO("[AMS] profile task registered, TASK_ID_AMSC=%u\r\n", TASK_ID_AMSC);
}

void app_ams_enable(uint8_t conidx)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    struct amsc_enable_req *req;
    NS_LOG_INFO("[AMS] enable request, conidx=%u\r\n", conidx);
    if (!e)
    {
        NS_LOG_INFO("[AMS] ERROR: profile env is NULL\r\n");
        return;
    }
    req = KE_MSG_ALLOC(AMSC_ENABLE_REQ, prf_src_task_get(&e->prf_env, conidx), TASK_APP, amsc_enable_req);
    req->con_type = PRF_CON_DISCOVERY;
    ke_msg_send(req);
    NS_LOG_INFO("[AMS] discovery request sent\r\n");
}

void app_ams_enable_notifications(uint8_t conidx)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    struct amsc_wr_cfg_req *req;
    if (!e)
    {
        return;
    }
    /* Entity Update must be subscribed before AMS accepts registration commands. */
    req = KE_MSG_ALLOC(AMSC_WR_CFG_REQ, prf_src_task_get(&e->prf_env, conidx), TASK_APP, amsc_wr_cfg_req);
    req->op_code = AMSC_CFG_ENTITY_UPDATE;
    req->enable = 1;
    ke_msg_send(req);
}

void app_ams_remote_command(uint8_t conidx, uint8_t command)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    struct amsc_remote_cmd_req *req;
    if (!e)
    {
        return;
    }
    req = KE_MSG_ALLOC(AMSC_REMOTE_CMD_REQ, prf_src_task_get(&e->prf_env, conidx), TASK_APP, amsc_remote_cmd_req);
    req->command = command;
    ke_msg_send(req);
}

void app_ams_register(uint8_t conidx, uint8_t entity, const uint8_t *attrs, uint8_t count)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    struct amsc_entity_update_req *req;
    uint8_t i;
    if (!e || count > 4)
    {
        NS_LOG_INFO("[AMS] app register rejected entity=%u count=%u\r\n", entity, count);
        return;
    }
    NS_LOG_INFO("[AMS] app register entity=%u count=%u -> send REG_REQ\r\n",
                entity,
                count);
    req = KE_MSG_ALLOC(AMSC_ENTITY_UPDATE_REQ, prf_src_task_get(&e->prf_env, conidx), TASK_APP, amsc_entity_update_req);
    req->entity = entity;
    req->attr_count = count;
    for (i = 0; i < count; ++i)
    {
        req->attr[i] = attrs[i];
    }
    ke_msg_send(req);
}

void app_ams_read_attribute(uint8_t conidx, uint8_t entity, uint8_t attr)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    struct amsc_entity_attribute_req *req;
    if (!e)
    {
        return;
    }
    req = KE_MSG_ALLOC(AMSC_ENTITY_ATTRIBUTE_REQ, prf_src_task_get(&e->prf_env, conidx), TASK_APP, amsc_entity_attribute_req);
    req->entity = entity;
    req->attr = attr;
    ke_msg_send(req);
}

static int app_ams_enable_handler(const ke_msg_id_t msgid, const struct amsc_enable_rsp *param,
                                  const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(src_id);
    NS_LOG_INFO("[AMS] discovery complete: status=0x%02X conidx=%u\r\n", param->status, conidx);
    if (param->status == GAP_ERR_NO_ERROR)
    {
        app_ams_enable_notifications(conidx);
    }
    return KE_MSG_CONSUMED;
}

static int app_ams_cfg_handler(const ke_msg_id_t msgid, const struct amsc_wr_cfg_rsp *param,
                               const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(src_id);
    NS_LOG_INFO("[AMS] CCCD write: op=%u status=0x%02X conidx=%u\r\n", param->op_code, param->status, conidx);
    if (param->status != GAP_ERR_NO_ERROR)
    {
        NS_LOG_INFO("[AMS] CCCD failed, stop registration\r\n");
        return KE_MSG_CONSUMED;
    }
    if (param->op_code == AMSC_CFG_ENTITY_UPDATE)
    {
        static const uint8_t player_attrs[] =
        {
            AMS_PLAYER_NAME,
            AMS_PLAYER_PLAYBACK_INFO,
            AMS_PLAYER_VOLUME
        };
        NS_LOG_INFO("[AMS] Entity Update CCCD OK -> register PLAYER\r\n");
        app_ams_register(conidx, AMS_ENTITY_PLAYER, player_attrs, 3);
    }
    return KE_MSG_CONSUMED;
}

static int app_ams_entity_handler(const ke_msg_id_t msgid, const struct amsc_entity_update_ind *ind,
                                  const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    char s[129];
    uint16_t n = ind->length > 128 ? 128 : ind->length;
    memcpy(s, ind->value, n);
    s[n] = 0;
    NS_LOG_INFO("AMS Entity=%u Attr=%u Flags=0x%02X Value=%s\r\n",
                ind->entity,
                ind->attr,
                ind->flags,
                s);

#if (AMS_TEST_AUTO_CMD_ENABLE)
    if (!s_ams_test_started)
    {
        app_ams_test_start(KE_IDX_GET(src_id));
    }
#endif

    if (ind->flags & AMS_UPDATE_FLAG_TRUNCATED)
    {
        app_ams_read_attribute(KE_IDX_GET(src_id),
                                ind->entity,
                                ind->attr);
    }
    return KE_MSG_CONSUMED;
}

static int app_ams_remote_handler(const ke_msg_id_t msgid, const struct amsc_remote_cmd_ind *ind,
                                  const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    uint16_t i;
    NS_LOG_INFO("AMS supported commands:");
    for (i = 0; i < ind->length; ++i)
    {
        NS_LOG_INFO(" %u", ind->value[i]);
    }
    NS_LOG_INFO("\r\n");
    return KE_MSG_CONSUMED;
}

static int app_ams_attr_handler(const ke_msg_id_t msgid, const struct amsc_entity_attribute_ind *ind,
                                const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    char s[129];
    uint16_t n = ind->length > 128 ? 128 : ind->length;
    memcpy(s, ind->value, n);
    s[n] = 0;
    NS_LOG_INFO("AMS AttrRead Entity=%u Attr=%u status=0x%02X Value=%s\r\n",
                ind->entity,
                ind->attr,
                ind->status,
                s);
    return KE_MSG_CONSUMED;
}

static const struct ke_msg_handler app_ams_msg_handler_list[] = {
    {AMSC_ENABLE_RSP, (ke_msg_func_t)app_ams_enable_handler},
    {AMSC_WR_CFG_RSP, (ke_msg_func_t)app_ams_cfg_handler},
    {AMSC_ENTITY_UPDATE_IND, (ke_msg_func_t)app_ams_entity_handler},
    {AMSC_REMOTE_CMD_IND, (ke_msg_func_t)app_ams_remote_handler},
    {AMSC_ENTITY_ATTRIBUTE_IND, (ke_msg_func_t)app_ams_attr_handler},
};
const struct app_subtask_handlers app_ams_handlers = APP_HANDLERS(app_ams);
#endif
