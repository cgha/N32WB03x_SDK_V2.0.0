#include "rwip_config.h"
#if (BLE_AMS_CLIENT)
#include <string.h>
#include "gap.h"
#include "co_utils.h"
#include "amsc.h"
#include "amsc_task.h"
#include "prf_utils.h"
#include "prf_utils_128.h"
#include "prf.h"
#include "ke_mem.h"

/* Apple Media Service UUIDs. The byte order below is the same 128-bit UUID
 * representation used by prf_utils_128 in the N32WB03x SDK. */
const struct att_uuid_128 amsc_ams_svc =
    {{0xDC,0xF8,0x55,0xAD,0x02,0xC5,0xF4,0x8E,0x3A,0x43,0x36,0x0F,0x2B,0x50,0xD3,0x89}};

const struct prf_char_def_128 amsc_ams_char[AMSC_CHAR_MAX] =
{
    [AMSC_CHAR_REMOTE_CMD] = {{0xC2,0x51,0xCA,0xF7,0x56,0x0E,0xDF,0xB8,0x8A,0x4A,0xB1,0x57,0xD8,0x81,0x3C,0x9B},
                              ATT_MANDATORY, ATT_CHAR_PROP_WR | ATT_CHAR_PROP_NTF},
    [AMSC_CHAR_ENTITY_UPDATE] = {{0x02,0xC1,0x96,0xBA,0x92,0xBB,0x0C,0x9A,0x1F,0x41,0x8D,0x80,0xCE,0xAB,0x7C,0x2F},
                                 ATT_MANDATORY, ATT_CHAR_PROP_WR | ATT_CHAR_PROP_NTF},
    [AMSC_CHAR_ENTITY_ATTRIBUTE] = {{0xD7,0xD5,0xBB,0x70,0xA8,0xA3,0xAB,0xA6,0xD8,0x46,0xAB,0x23,0x8C,0xF3,0xB2,0xC6},
                                    ATT_MANDATORY, ATT_CHAR_PROP_RD | ATT_CHAR_PROP_WR},
};

const struct prf_char_desc_def amsc_ams_desc[AMSC_DESC_MAX] =
{
    [AMSC_DESC_ENTITY_UPDATE_CCC] = {ATT_DESC_CLIENT_CHAR_CFG, ATT_MANDATORY, AMSC_CHAR_ENTITY_UPDATE},
};

static int amsc_enable_req_handler(ke_msg_id_t const msgid, struct amsc_enable_req const *param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    uint8_t conidx = KE_IDX_GET(dest_id);
    NS_LOG_INFO("[AMS] ENABLE_REQ conidx=%u type=%u\r\n", conidx, param->con_type);
    if (!e) { NS_LOG_INFO("[AMS] ERROR: env NULL\r\n"); return KE_MSG_CONSUMED; }

    e->env[conidx] = (struct amsc_cnx_env *)ke_malloc(sizeof(struct amsc_cnx_env), KE_MEM_ENV);
    if (!e->env[conidx]) {
        amsc_enable_rsp_send(e, conidx, GAP_ERR_INSUFF_RESOURCES);
        return KE_MSG_CONSUMED;
    }
    memset(e->env[conidx], 0, sizeof(struct amsc_cnx_env));

    if (param->con_type == PRF_CON_DISCOVERY) {
        NS_LOG_INFO("[AMS] start service discovery\r\n");
        prf_disc_svc_sdp_send_128(&e->prf_env, conidx, amsc_ams_svc.uuid);
        ke_state_set(dest_id, AMSC_DISCOVERING);
    } else {
        e->env[conidx]->ams = param->ams;
        amsc_enable_rsp_send(e, conidx, GAP_ERR_NO_ERROR);
    }
    return KE_MSG_CONSUMED;
}

static int gattc_sdp_svc_ind_handler(ke_msg_id_t const msgid, struct gattc_sdp_svc_ind const *ind,
                                     ke_task_id_t const dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    uint8_t conidx = KE_IDX_GET(dest_id);
    if (!e || !e->env[conidx]) return KE_MSG_CONSUMED;

    NS_LOG_INFO("[AMS] service found: start=0x%04X end=0x%04X\r\n", ind->start_hdl, ind->end_hdl);

    /* Temporary discovery dump: print every characteristic UUID returned by iOS.
     * This lets us verify the N32 SDK's 128-bit UUID byte order from the actual ATT response. */
    {
        uint8_t i;
        for (i = 0; i < (ind->end_hdl - ind->start_hdl); i++) {
            if (ind->info[i].att_type == GATTC_SDP_ATT_CHAR) {
                uint8_t vi = (uint8_t)(ind->info[i].att_char.handle - ind->start_hdl - 1);
                uint8_t j;
                NS_LOG_INFO("[AMS] CHAR hdl=0x%04X val=0x%04X prop=0x%02X uuid=",
                            (uint16_t)(ind->start_hdl + 1 + i),
                            ind->info[i].att_char.handle,
                            ind->info[i].att_char.prop);
                if (vi < (ind->end_hdl - ind->start_hdl) && ind->info[vi].att.uuid_len == ATT_UUID_128_LEN) {
                    for (j = 0; j < ATT_UUID_128_LEN; j++)
                        NS_LOG_INFO("%02X", ind->info[vi].att.uuid[j]);
                }
                NS_LOG_INFO("\r\n");
            }
        }
    }

    if (e->env[conidx]->nb_svc == 0) {
        prf_extract_svc_sdp_info_128(ind, AMSC_CHAR_MAX, amsc_ams_char,
                                     e->env[conidx]->ams.chars,
                                     AMSC_DESC_MAX, amsc_ams_desc,
                                     e->env[conidx]->ams.descs);
        e->env[conidx]->ams.svc.shdl = ind->start_hdl;
        e->env[conidx]->ams.svc.ehdl = ind->end_hdl;

        /* ============================================================
         * DIAG: 打印 SDK 解析出的 CCCD 句柄，方便验证
         * 注意：不再强制修正句柄！测试证明 iOS 端 Entity Update 的 CCCD
         * 描述符句柄恰好位于 SDK 解析到的位置(0x0035)，而非 val_hdl+1(0x0034)。
         * 不同服务的 GATT 表描述符排列顺序可能不同，不能假设 CCCD 一定在 val_hdl+1。
         * ============================================================ */
        NS_LOG_INFO("[AMS] DIAG: parsed CCCD hdl=0x%04X (SDK resolved)\r\n",
                    e->env[conidx]->ams.descs[AMSC_DESC_ENTITY_UPDATE_CCC].desc_hdl);
    }
    e->env[conidx]->nb_svc++;
    NS_LOG_INFO("[AMS] service count=%u RC=0x%04X EU=0x%04X EA=0x%04X CCC=0x%04X\r\n",
                 e->env[conidx]->nb_svc,
                 e->env[conidx]->ams.chars[AMSC_CHAR_REMOTE_CMD].val_hdl,
                 e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_UPDATE].val_hdl,
                 e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_ATTRIBUTE].val_hdl,
                 e->env[conidx]->ams.descs[AMSC_DESC_ENTITY_UPDATE_CCC].desc_hdl);
    return KE_MSG_CONSUMED;
}

static void amsc_send_default_registration(struct amsc_env_tag *e, uint8_t conidx, uint8_t step)
{
    /* ============================================================
     * FIX: 修正注册请求数据格式
     * Apple Media Service 规范要求的格式:
     *   Opcode(1字节=0x01) | EntityID(1字节) | AttributeCount(1字节) | AttrIDs...
     * ============================================================ */
    uint8_t buf[7]; uint8_t len = 0;
    if (!e->env[conidx]) return;

    buf[len++] = 0x01;  /* Opcode: Subscribe */

    switch (step) {
    case 0: /* Player: name, playback info, volume */
        buf[len++] = AMS_ENTITY_PLAYER;
        buf[len++] = 3;                     /* Attribute Count */
        buf[len++] = AMS_PLAYER_NAME;
        buf[len++] = AMS_PLAYER_PLAYBACK_INFO;
        buf[len++] = AMS_PLAYER_VOLUME; break;
    case 1: /* Queue: index, count, shuffle, repeat */
        buf[len++] = AMS_ENTITY_QUEUE;
        buf[len++] = 4;                     /* Attribute Count */
        buf[len++] = AMS_QUEUE_INDEX;
        buf[len++] = AMS_QUEUE_COUNT;
        buf[len++] = AMS_QUEUE_SHUFFLE_MODE;
        buf[len++] = AMS_QUEUE_REPEAT_MODE; break;
    case 2: /* Track: artist, album, title, duration */
        buf[len++] = AMS_ENTITY_TRACK;
        buf[len++] = 4;                     /* Attribute Count */
        buf[len++] = AMS_TRACK_ARTIST;
        buf[len++] = AMS_TRACK_ALBUM;
        buf[len++] = AMS_TRACK_TITLE;
        buf[len++] = AMS_TRACK_DURATION; break;
    default: return;
    }

    e->env[conidx]->reg_step = step;
    e->env[conidx]->last_op = AMSC_REG_PLAYER + step;
    prf_gatt_write(&e->prf_env, conidx,
                   e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_UPDATE].val_hdl,
                   buf, len, GATTC_WRITE);
    ke_state_set(KE_BUILD_ID(e->prf_env.prf_task, conidx), AMSC_BUSY);
}

static int gattc_cmp_evt_handler(ke_msg_id_t const msgid, struct gattc_cmp_evt const *param,
                                 ke_task_id_t const dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    uint8_t conidx = KE_IDX_GET(dest_id);

    // ====== 【调试打印1】函数入口，打印原始参数 ======
    NS_LOG_INFO("[AMS] CMP_EVT: msgid=%u dest=%u src=%u op=%u status=0x%02X\r\n",
                msgid, dest_id, src_id, param->operation, param->status);

    if (!e || !e->env[conidx]) {
        NS_LOG_INFO("[AMS] CMP_EVT dropped: env NULL or conidx=%u invalid\r\n", conidx);
        return KE_MSG_CONSUMED;
    }

    // ====== 【调试打印2】打印当前状态机状态 ======
    uint8_t current_state = ke_state_get(dest_id);
    NS_LOG_INFO("[AMS] CMP_EVT current_state=%u (0=IDLE 1=DISC 2=ENABLING 3=BUSY)\r\n", current_state);

    if (ke_state_get(dest_id) == AMSC_DISCOVERING) {
        uint8_t status = param->status;
        NS_LOG_INFO("[AMS] discovery complete cmp: status=0x%02X svc=%u\r\n", param->status, e->env[conidx]->nb_svc);
        if (status == ATT_ERR_ATTRIBUTE_NOT_FOUND || status == GAP_ERR_NO_ERROR) {
            if (e->env[conidx]->nb_svc != 1) status = (e->env[conidx]->nb_svc > 1) ? PRF_ERR_MULTIPLE_SVC : PRF_ERR_STOP_DISC_CHAR_MISSING;
            else {
                status = prf_check_svc_char_validity_128(AMSC_CHAR_MAX, e->env[conidx]->ams.chars, amsc_ams_char);
                if (status == GAP_ERR_NO_ERROR)
                    status = prf_check_svc_char_desc_validity(AMSC_DESC_MAX, e->env[conidx]->ams.descs,
                                                              amsc_ams_desc, e->env[conidx]->ams.chars);
            }
        }
        amsc_enable_rsp_send(e, conidx, status);
        ke_state_set(dest_id, AMSC_IDLE);
        NS_LOG_INFO("[AMS] CMP_EVT discovery -> set IDLE\r\n");
        return KE_MSG_CONSUMED;
    }

    // ====== 【修复】加上 uint8_t 类型声明 ======
    uint8_t op = e->env[conidx]->last_op;
    NS_LOG_INFO("[AMS] CMP_EVT last_op=%u gatt_op=%u status=0x%02X\r\n", op, param->operation, param->status);

    if (ke_state_get(dest_id) == AMSC_BUSY) {
        if (param->operation == GATTC_WRITE || param->operation == GATTC_WRITE_NO_RESPONSE) {
            NS_LOG_INFO("[AMS] write cmp op=%u gatt_op=%u status=0x%02X state=%u\r\n",
                        op, param->operation, param->status, ke_state_get(dest_id));
            if (op == AMSC_CFG_REMOTE_CMD || op == AMSC_CFG_ENTITY_UPDATE) {
                NS_LOG_INFO("[AMS] CFG complete op=%u status=0x%02X -> IDLE\r\n", op, param->status);
                amsc_cfg_rsp_send(e, conidx, op, param->status);
                /* The configuration write occupied AMSC_BUSY.  Return to IDLE
                 * before the application sends the first registration command. */
                ke_state_set(dest_id, AMSC_IDLE);
                NS_LOG_INFO("[AMS] CMP_EVT CFG -> set IDLE\r\n");
            } else if (op >= AMSC_REG_PLAYER && op <= AMSC_REG_TRACK) {
                if (param->status == GAP_ERR_NO_ERROR && op < AMSC_REG_TRACK) {
                    NS_LOG_INFO("[AMS] CMP_EVT reg success, sending next: step=%u\r\n",
                                e->env[conidx]->reg_step + 1);
                    amsc_send_default_registration(e, conidx, e->env[conidx]->reg_step + 1);
                } else {
                    NS_LOG_INFO("[AMS] CMP_EVT reg failed or last: status=0x%02X op=%u -> IDLE\r\n",
                                param->status, op);
                    ke_state_set(dest_id, AMSC_IDLE);
                    NS_LOG_INFO("[AMS] CMP_EVT reg done -> set IDLE\r\n");
                }
            } else if (op == AMSC_REMOTE_COMMAND || op == AMSC_ENTITY_ATTRIBUTE_READ) {
                if (op == AMSC_ENTITY_ATTRIBUTE_READ && param->status == GAP_ERR_NO_ERROR) {
                    prf_read_char_send(&e->prf_env, conidx, e->env[conidx]->ams.svc.shdl,
                                       e->env[conidx]->ams.svc.ehdl,
                                       e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_ATTRIBUTE].val_hdl);
                } else {
                    ke_state_set(dest_id, AMSC_IDLE);
                    NS_LOG_INFO("[AMS] CMP_EVT cmd/attr -> set IDLE\r\n");
                }
            }
        } else if (param->operation == GATTC_READ) {
            if (param->status != GAP_ERR_NO_ERROR) {
                if (op == AMSC_ENTITY_ATTRIBUTE_READ) {
                    amsc_attr_read_rsp(e, conidx, e->env[conidx]->pending_entity,
                                       e->env[conidx]->pending_attr, NULL, 0, param->status);
                }
                ke_state_set(dest_id, AMSC_IDLE);
                NS_LOG_INFO("[AMS] CMP_EVT read fail -> set IDLE\r\n");
            }
        }
    } else {
        NS_LOG_INFO("[AMS] CMP_EVT WARNING: state=%u != BUSY, skipping\r\n", current_state);
    }
    return KE_MSG_CONSUMED;
}

static int amsc_wr_cfg_req_handler(ke_msg_id_t const msgid, const struct amsc_wr_cfg_req *param,
                                   const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc);
    uint8_t conidx = KE_IDX_GET(dest_id);
    uint16_t hdl = ATT_INVALID_SEARCH_HANDLE;
    if (!e || !e->env[conidx] || ke_state_get(dest_id) != AMSC_IDLE) return KE_MSG_CONSUMED;

    if (param->op_code == AMSC_CFG_ENTITY_UPDATE) hdl = e->env[conidx]->ams.descs[AMSC_DESC_ENTITY_UPDATE_CCC].desc_hdl;
    else {
        amsc_cfg_rsp_send(e, conidx, param->op_code, PRF_ERR_INEXISTENT_HDL);
        return KE_MSG_CONSUMED;
    }
    if (hdl == ATT_INVALID_SEARCH_HANDLE) {
        amsc_cfg_rsp_send(e, conidx, param->op_code, PRF_ERR_INEXISTENT_HDL);
        return KE_MSG_CONSUMED;
    }
    /* ============================================================
     * FIX 2: 打印实际写入的CCCD句柄，便于验证
     * ============================================================ */
    NS_LOG_INFO("[AMS] CCCD write: hdl=0x%04X val=%u\r\n", hdl, param->enable ? 1 : 0);
    prf_gatt_write_ntf_ind(&e->prf_env, conidx, hdl,
                           param->enable ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND);
    e->env[conidx]->last_op = param->op_code;
    ke_state_set(dest_id, AMSC_BUSY);
    return KE_MSG_CONSUMED;
}

static int amsc_remote_cmd_req_handler(ke_msg_id_t const msgid, const struct amsc_remote_cmd_req *param,
                                       const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc); uint8_t conidx = KE_IDX_GET(dest_id);
    uint8_t v = param->command;
    if (!e || !e->env[conidx] || ke_state_get(dest_id) != AMSC_IDLE) return KE_MSG_CONSUMED;
    prf_gatt_write(&e->prf_env, conidx, e->env[conidx]->ams.chars[AMSC_CHAR_REMOTE_CMD].val_hdl, &v, 1, GATTC_WRITE);
    e->env[conidx]->last_op = AMSC_REMOTE_COMMAND; ke_state_set(dest_id, AMSC_BUSY);
    return KE_MSG_CONSUMED;
}

static int amsc_entity_update_req_handler(ke_msg_id_t const msgid, const struct amsc_entity_update_req *param,
                                          const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc); uint8_t conidx = KE_IDX_GET(dest_id);
    /* ============================================================
     * FIX 3b: 缓冲区从 buf[5] 增大到 buf[7]
     * 原因: Apple Media Service 规范的注册请求格式为:
     *   [Opcode=0x01(1字节)] [EntityID(1字节)] [AttrCount(1字节)] [AttrIDs...]
     *   最大需要: 1 + 1 + 1 + 4 = 7 字节
     * ============================================================ */
    uint8_t buf[7]; uint8_t i, len;
    if (!e || !e->env[conidx]) {
        NS_LOG_INFO("[AMS] REG request rejected: env NULL conidx=%u\r\n", conidx);
        return KE_MSG_CONSUMED;
    }
    NS_LOG_INFO("[AMS] REG request entity=%u count=%u state=%u\r\n",
                param->entity, param->attr_count, ke_state_get(dest_id));
    if (ke_state_get(dest_id) != AMSC_IDLE || param->attr_count > 4) {
        NS_LOG_INFO("[AMS] REG request rejected: state=%u count=%u\r\n",
                    ke_state_get(dest_id), param->attr_count);
        return KE_MSG_CONSUMED;
    }
    /* ============================================================
     * FIX 4: 修正注册请求数据格式
     *
     * 原代码(错误):
     *   buf[0] = param->entity;
     *   for (i=0; i<param->attr_count; ++i) buf[1+i] = param->attr[i];
     *   len = 1 + param->attr_count;
     * 发送的数据: [EntityID][Attr1][Attr2]...  (缺少Opcode和Count)
     *
     * Apple Media Service 规范要求写入 Entity Update 特征值的格式为:
     *   Opcode       (1 octet): 0x01 = Subscribe
     *   Entity ID    (1 octet)
     *   Attribute Count (1 octet)
     *   Attribute ID List (variable, 每个1 octet)
     *
     * 修正后: [0x01][EntityID][Count][Attr1][Attr2]...
     * ============================================================ */
    buf[0] = 0x01;                          /* Opcode: Subscribe */
    buf[1] = param->entity;                 /* Entity ID */
    buf[2] = param->attr_count;             /* Attribute Count */
    for (i = 0; i < param->attr_count; ++i) buf[3 + i] = param->attr[i];
    len = 3 + param->attr_count;

    NS_LOG_INFO("[AMS] REG write entity=%u len=%u hdl=0x%04X op=%u\r\n",
                param->entity, len, e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_UPDATE].val_hdl,
                e->env[conidx]->last_op);
    prf_gatt_write(&e->prf_env, conidx,
                   e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_UPDATE].val_hdl,
                   buf, len, GATTC_WRITE);
    e->env[conidx]->last_op = AMSC_REG_PLAYER;
    ke_state_set(dest_id, AMSC_BUSY);
    return KE_MSG_CONSUMED;
}

static int amsc_entity_attribute_req_handler(ke_msg_id_t const msgid, const struct amsc_entity_attribute_req *param,
                                             const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc); uint8_t conidx = KE_IDX_GET(dest_id);
    uint8_t buf[2];
    if (!e || !e->env[conidx] || ke_state_get(dest_id) != AMSC_IDLE) return KE_MSG_CONSUMED;
    buf[0] = param->entity; buf[1] = param->attr;
    e->env[conidx]->pending_entity = param->entity; e->env[conidx]->pending_attr = param->attr;
    prf_gatt_write(&e->prf_env, conidx, e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_ATTRIBUTE].val_hdl, buf, 2, GATTC_WRITE);
    e->env[conidx]->last_op = AMSC_ENTITY_ATTRIBUTE_READ; ke_state_set(dest_id, AMSC_BUSY);
    return KE_MSG_CONSUMED;
}

static int gattc_read_ind_handler(ke_msg_id_t const msgid, const struct gattc_read_ind *param,
                                  const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc); uint8_t conidx = KE_IDX_GET(dest_id);
    if (!e || !e->env[conidx]) return KE_MSG_CONSUMED;
    if (param->handle == e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_ATTRIBUTE].val_hdl) {
        amsc_attr_read_rsp(e, conidx, e->env[conidx]->pending_entity, e->env[conidx]->pending_attr,
                           param->value, param->length, GAP_ERR_NO_ERROR);
        ke_state_set(dest_id, AMSC_IDLE);
    }
    return KE_MSG_CONSUMED;
}

static int gattc_event_ind_handler(ke_msg_id_t const msgid, const struct gattc_event_ind *param,
                                   const ke_task_id_t dest_id, const ke_task_id_t src_id)
{
	 NS_LOG_INFO("[AMS] RAW EVENT: handle=0x%04X, type=%u, len=%u\r\n", 
                ((struct gattc_event_ind *)param)->handle, 
                ((struct gattc_event_ind *)param)->type, 
                ((struct gattc_event_ind *)param)->length);
				
    struct amsc_env_tag *e = PRF_ENV_GET(AMSC, amsc); uint8_t conidx = KE_IDX_GET(dest_id);
    /* ============================================================
     * FIX 5: 添加调试日志 + 兼容 GATTC_INDICATE
     *
     * 原因1: 原始代码缺少调试日志，通知到达时也看不到
     * 原因2: 某些BLE协议栈可能将Notify报告为Indicate，导致静默丢弃
     * ============================================================ */
    if (!e || !e->env[conidx]) {
        NS_LOG_INFO("[AMS] EVENT_IND dropped: env NULL or conidx invalid\r\n");
        return KE_MSG_CONSUMED;
    }
    if (param->type != GATTC_NOTIFY && param->type != GATTC_INDICATE) {
        NS_LOG_INFO("[AMS] EVENT_IND dropped: type=%u (expected NOTIFY/INDICATE)\r\n", param->type);
        return KE_MSG_CONSUMED;
    }
    NS_LOG_INFO("[AMS] EVENT_IND recv: handle=0x%04X type=%u len=%u\r\n",
                param->handle, param->type, param->length);

    if (param->handle == e->env[conidx]->ams.chars[AMSC_CHAR_REMOTE_CMD].val_hdl) {
        amsc_remote_cmd_ind(e, conidx, param->value, param->length);
    } else if (param->handle == e->env[conidx]->ams.chars[AMSC_CHAR_ENTITY_UPDATE].val_hdl && param->length >= 3) {
        uint8_t entity = param->value[0], attr = param->value[1], flags = param->value[2];
        uint16_t len = param->length - 3;
        amsc_entity_value_ind(e, conidx, entity, attr, flags, &param->value[3], len);
    }
    return KE_MSG_CONSUMED;
}

KE_MSG_HANDLER_TAB(amsc)
{
    {AMSC_ENABLE_REQ, (ke_msg_func_t)amsc_enable_req_handler},
    {AMSC_WR_CFG_REQ, (ke_msg_func_t)amsc_wr_cfg_req_handler},
    {AMSC_REMOTE_CMD_REQ, (ke_msg_func_t)amsc_remote_cmd_req_handler},
    {AMSC_ENTITY_UPDATE_REQ, (ke_msg_func_t)amsc_entity_update_req_handler},
    {AMSC_ENTITY_ATTRIBUTE_REQ, (ke_msg_func_t)amsc_entity_attribute_req_handler},
    {GATTC_SDP_SVC_IND, (ke_msg_func_t)gattc_sdp_svc_ind_handler},
    {GATTC_CMP_EVT, (ke_msg_func_t)gattc_cmp_evt_handler},
    {GATTC_READ_IND, (ke_msg_func_t)gattc_read_ind_handler},
    {GATTC_EVENT_IND, (ke_msg_func_t)gattc_event_ind_handler},
};

void amsc_task_init(struct ke_task_desc *p_task_desc)
{
    p_task_desc->msg_handler_tab = amsc_msg_handler_tab;
    p_task_desc->msg_cnt = ARRAY_LEN(amsc_msg_handler_tab);
    p_task_desc->state = NULL;
    p_task_desc->idx_max = AMSC_IDX_MAX;
}
#endif
