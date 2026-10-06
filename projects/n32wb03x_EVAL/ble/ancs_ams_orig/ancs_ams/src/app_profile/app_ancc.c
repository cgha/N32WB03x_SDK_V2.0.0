/*****************************************************************************
 * Copyright (c) 2019, Nations Technologies Inc.
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Nations' name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY NATIONS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL NATIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file app_ancc.c
 * @author Nations Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */


#include "rwip_config.h"     // SW configuration

#if (BLE_APP_PRESENT)

#if (BLE_ANC_CLIENT)
#include "gapm_task.h"
#include "ns_ble.h"
#include "prf.h"
#include "ke_mem.h"
#include "stdio.h"
#include "ns_ble_task.h"
#include "string.h"
#include "prf_utils_128.h"
#include "app_ancc.h"
#include "ancc_task.h"
#if (BLE_APP_SEC)
#include "ns_sec.h" 
#endif
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static struct co_list notif_queue;
static struct co_list app_queue;
struct notification *pending_notif;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/*
 * GLOBAL FUNCTION DEFINITIONS
 */

/**
 * @brief Send Write Notification Configuration Request
 * @param[in] enable            Notification enable flag
 * @param[in] opcode            Operation code
 * @param[in] conidx            The connection index
 */
static void app_ancc_wr_ntf_cfg_send(bool enable, uint8_t op_code, uint8_t conidx)
{
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);
    struct ancc_wr_cfg_ntf_req *req = KE_MSG_ALLOC(ANCC_WR_CFG_NTF_REQ,
                                                   prf_src_task_get(&ancc_env->prf_env, conidx),
                                                   TASK_APP,
                                                   ancc_wr_cfg_ntf_req);

    req->op_code = op_code;
    req->cfg_val = (enable) ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

    ke_msg_send(req);
}

/**
 * @brief Send Read Notification Configuration Request
 * @param[in] opcode            Operation code
 * @param[in] conidx            The connection index
 */
static void app_ancc_rd_ntf_cfg_send(uint8_t op_code, uint8_t conidx)
{
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);
    struct ancc_rd_cfg_ntf_req *req = KE_MSG_ALLOC(ANCC_RD_CFG_NTF_REQ,
                                                   prf_src_task_get(&ancc_env->prf_env, conidx),
                                                   TASK_APP,
                                                   ancc_rd_cfg_ntf_req);

    req->op_code = op_code;

    ke_msg_send(req);
}

/*
 * GLOBAL FUNCTION DEFINITIONS
 */
void app_ancc_init(void)
{
    //register application subtask to app task
    struct prf_task_t prf;
    prf.prf_task_id = TASK_ID_ANCC;
    prf.prf_task_handler = &app_ancc_handlers;
    ns_ble_prf_task_register(&prf);
    
    //register get itf function to prf.c
    struct prf_get_func_t get_func;
    get_func.task_id = TASK_ID_ANCC;
    get_func.prf_itf_get_func = ancc_prf_itf_get;
    prf_get_itf_func_register(&get_func);
}

/** 
 * @brief Create raw data transfer server profile database.
 * @return void
 */
void app_ancc_add_ancs(void)
{
    NS_LOG_DEBUG("%s\r\n", __func__);
    struct gapm_profile_task_add_cmd *req = KE_MSG_ALLOC(GAPM_PROFILE_TASK_ADD_CMD,
                                                         TASK_GAPM,
                                                         TASK_APP,
                                                         gapm_profile_task_add_cmd);

    // Fill the message
    req->operation   = GAPM_PROFILE_TASK_ADD;
    req->sec_lvl     = PERM(SVC_AUTH, NO_AUTH);
    req->prf_task_id = TASK_ID_ANCC;
    req->app_task    = TASK_APP;
    req->start_hdl   = 0;

    // Send the message
    ke_msg_send(req);
    
    app_ancc_init();
}

void app_ancc_enable(uint8_t conidx)
{
	NS_LOG_DEBUG("%s\r\n", __func__);
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);
    struct ancc_enable_req *req = KE_MSG_ALLOC(ANCC_ENABLE_REQ,
                                               prf_src_task_get(&ancc_env->prf_env, conidx),
                                               TASK_APP,
                                               ancc_enable_req);

    // Fill the parameter structure
    req->con_type = PRF_CON_DISCOVERY;

    // Send the message
    ke_msg_send(req);
}

void app_ancc_wr_cfg_ntf_src(uint8_t conidx, bool enable)
{
    app_ancc_wr_ntf_cfg_send(enable, ANCC_WR_NTF_NTF_SRC_OP_CODE, conidx);
}

void app_ancc_wr_cfg_data_src(uint8_t conidx, bool enable)
{
    app_ancc_wr_ntf_cfg_send(enable, ANCC_WR_NTF_DATA_SRC_OP_CODE, conidx);
}

void app_ancc_rd_cfg_ntf_src(uint8_t conidx)
{
    app_ancc_rd_ntf_cfg_send(ANCC_RD_NTF_NTF_SRC_OP_CODE, conidx);
}

void app_ancc_rd_cfg_data_src(uint8_t conidx)
{
    app_ancc_rd_ntf_cfg_send(ANCC_RD_NTF_DATA_SRC_OP_CODE, conidx);
}

void app_ancc_get_ntf_atts(uint8_t conidx, uint32_t uid, uint8_t atts, uint16_t title_len,
                               uint16_t sub_len, uint16_t msg_len)
{
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);
	
	if (!ancc_env) {
        NS_LOG_ERROR("[ANCC] get_ntf_atts: env NULL, conidx=%d, uid=0x%08X\r\n",
                     conidx, uid);
        return;
    }
    if (!ancc_env->env[conidx]) {
        NS_LOG_ERROR("[ANCC] get_ntf_atts: env[%d] NULL, conidx=%d, uid=0x%08X\r\n",
                     conidx, conidx, uid);
        return;
    }

    struct ancc_get_ntf_atts_cmd_req *req = KE_MSG_ALLOC(ANCC_GET_NTF_ATTS_CMD_REQ,
                                                         prf_src_task_get(&ancc_env->prf_env, conidx),
                                                         TASK_APP,
                                                         ancc_get_ntf_atts_cmd_req);

    req->ntf_uid      = uid;
    req->atts         = atts;
    req->title_len    = title_len;
    req->subtitle_len = sub_len;
    req->message_len  = msg_len;

    ke_msg_send(req);
}

void app_ancc_get_app_atts(uint8_t conidx, uint8_t atts, uint8_t *app_id)
{
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);
    struct ancc_get_app_atts_cmd_req *req;
    uint8_t id_len;

    if (!ancc_env || !app_id)
    {
        return;
    }

    id_len = (uint8_t)(strlen((char *)app_id) + 1);

    req = KE_MSG_ALLOC_DYN(ANCC_GET_APP_ATTS_CMD_REQ,
                           prf_src_task_get(&ancc_env->prf_env, conidx),
                           TASK_APP,
                           ancc_get_app_atts_cmd_req,
                           id_len);

    if (!req)
    {
        NS_LOG_ERROR("ANCS: app attribute request alloc failed\r\n");
        return;
    }

    req->atts          = atts;
    req->app_id_length = id_len;

    memcpy(req->app_id, app_id, id_len);

    ke_msg_send(req);
}

void app_ancc_ntf_action(uint8_t conidx, uint32_t uid, bool act_positive)
{
    struct ancc_env_tag *ancc_env = PRF_ENV_GET(ANCC, ancc);

    struct ancc_perf_ntf_act_cmd_req *req = KE_MSG_ALLOC(ANCC_PERF_NTF_ACT_CMD_REQ,
                                                         prf_src_task_get(&ancc_env->prf_env, conidx),
                                                         TASK_APP,
                                                         ancc_perf_ntf_act_cmd_req);

    req->ntf_uid = uid;
    req->action  = (act_positive) ? ACT_ID_POS : ACT_ID_NEG;

    ke_msg_send(req);
}

/**
 * @brief Returns category description string
 *
 * @param[in] category  ANCS category
 * @return category string
 */
static const char *notifcategory2str(uint8_t category)
{
    switch (category) {
        case CAT_ID_OTHER:
                return "Other";
        case CAT_ID_CALL:
                return "Incoming call";
        case CAT_ID_MISSED_CALL:
                return "Missed call";
        case CAT_ID_VOICE_MAIL:
                return "Voicemail";
        case CAT_ID_SOCIAL:
                return "Social";
        case CAT_ID_SCHEDULE:
                return "Schedule";
        case CAT_ID_EMAIL:
                return "E-mail";
        case CAT_ID_NEWS:
                return "News";
        case CAT_ID_HEALTH_FITNESS:
                return "Health and Fitness";
        case CAT_ID_BUSINESS_FINANCE:
                return "Business and Finance";
        case CAT_ID_LOCATION:
                return "Location";
        case CAT_ID_ENTERTAINMENT:
                return "Entertainment";
        }

        return "<unknown>";
}

/**
 * @brief Print notification details
 *
 * @param[in] notif    notification data
 * @param[in] app      application data
 */
static void print_notification(const struct notification *notif, const struct application *app)
{
    const char *app_name = app ? app->display_name : "<unknown>";

    NS_LOG_INFO("Notification from %s (%s)\r\n", app_name, (app && notif->app_id) ? notif->app_id : "<unknown>");
    NS_LOG_INFO("\tCategory: %s\r\n", notifcategory2str(notif->ntf.cat_id));
    NS_LOG_INFO("\t    Date: %s\r\n", notif->date);
    NS_LOG_INFO("\t   Title: %s\r\n", notif->title);
    NS_LOG_INFO("\t Message: %s\r\n", notif->message);
    NS_LOG_INFO("\n");
}

/**
 * @brief Free memory allocated for notification data
 *
 * @param[in] notif    notification data
 */
static void free_notification(struct notification *notif)
{
    if (notif)
    {
        if (notif->app_id)
        {
            ke_free(notif->app_id);
        }
        ke_free(notif);
    }
}

/**
 * @brief Clear notification queue
 */
static void clear_notification_queue(void)
{
    while (!co_list_is_empty(&notif_queue))
    {
        struct notification *notif = (struct notification *)co_list_pop_front(&notif_queue);
        free_notification(notif);
    }
}

/**
 * @brief Store notification data if possible
 *
 * @param[in] ntf    notification data
 */
static void add_notification(const struct anc_ntf_src *ntf)
{
    if (ke_check_malloc(sizeof(struct notification), KE_MEM_ENV))
    {
        struct notification *notif = (struct notification *)ke_malloc(sizeof(struct notification), KE_MEM_ENV);

        if (!notif)
        {
            NS_LOG_ERROR("ANCS: notification alloc failed\r\n");
            return;
        }

        memset(notif, 0, sizeof(struct notification));
        notif->ntf = *ntf;
        co_list_push_back(&notif_queue, (void *)notif);
    }
}

/**
 * @brief Fetch notification from list and get attributes
 */
static void fetch_next_notification(uint8_t conidx)
{
    pending_notif = (struct notification *)co_list_pop_front(&notif_queue);
    if (!pending_notif)
    {
        return;
    }

    uint8_t atts = NTF_ATT_ID_APP_ID_PRESENT | NTF_ATT_ID_DATE_PRESENT |
                   NTF_ATT_ID_TITLE_PRESENT | NTF_ATT_ID_MSG_PRESENT;

    app_ancc_get_ntf_atts(conidx, pending_notif->ntf.ntf_uid, atts, ANCC_ATTS_MAX_LEN, 0, ANCC_ATTS_MSG_MAX_LEN);
}

/**
 * @brief Free pending notification and check if fetch attributes for next one.
 */
static void free_pending_notif(uint8_t conidx)
{
    if (pending_notif)
    {
        free_notification(pending_notif);
        pending_notif = NULL;
        fetch_next_notification(conidx);
    }
}

/**
 * @brief Store application data if possible
 *
 * @param[in] ntf    notification data
 */
static struct application *add_application(const char *app_id)
{
    struct application *app = NULL;
    uint32_t size = strlen(app_id);

    if (ke_check_malloc(sizeof(struct application), KE_MEM_ENV))
    {
        app = (struct application *)ke_malloc(sizeof(struct application), KE_MEM_ENV);
        if (!app)
        {
            return NULL;
        }

        memset(app, 0, sizeof(struct application));
        app->app_id = (char *)ke_malloc(size + 1, KE_MEM_ENV);
        if (!app->app_id)
        {
            ke_free(app);
            return NULL;
        }

        memcpy(app->app_id, app_id, size);
        app->app_id[size] = 0;
        co_list_push_back(&app_queue, (void *)app);
    }

    return app;
}

/**
 * @brief Store application data if possible
 *
 * @param[in] ntf    notification data
 */
static struct application *find_application(const char *app_id)
{
    if (!app_id)
    {
        return NULL;
    }

    if (!co_list_is_empty(&app_queue))
    {
        struct co_list_hdr *el = app_queue.first;
        struct application *app = NULL;

        while (true)
        {
            app = (struct application *)el;
            if (!strcmp(app->app_id, app_id))
            {
                return (struct application*)el;
            }
            else if (el == app_queue.last)
            {
                return NULL;
            }
            else
            {
                el = co_list_next(el);
            }
        }
    }

    return NULL;
}

/**
 * @brief Free memory allocated for application data
 *
 * @param[in] notif    application data
 */
static void free_application(struct application *app)
{
    if (!app)
    {
        return;
    }
    if (app->app_id)
    {
        ke_free(app->app_id);
    }
    if(app->display_name)
    {
        ke_free(app->display_name);
    }

    ke_free(app);
}

/**
 * @brief Clear application queue
 */
static void clear_application_queue(void)
{
    while (!co_list_is_empty(&app_queue))
    {
        free_application((struct application *)co_list_pop_front(&app_queue));
    }
}

/**
 * @brief Clear ancs related data
 */
void clear_ancs_data(void)
{
    clear_notification_queue();
    clear_application_queue();
    if (pending_notif)
    {
        free_notification(pending_notif);
        pending_notif = NULL;
    }
}

/*
 * Handler FUNCTION DEFINITIONS
 */

/**
 * @brief Handles ANCS enable message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_enable_handler(const ke_msg_id_t msgid,
                               const struct ancc_enable_rsp *param,
                               const ke_task_id_t dest_id,
                               const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(dest_id);
    uint8_t status = param->status;
	NS_LOG_DEBUG("%s,status= 0x%x\r\n", __func__,status);

    if (status == GAP_ERR_NO_ERROR)
    {
        app_ancc_wr_cfg_data_src(conidx, true);
    }

    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS notification configuration write response message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_wr_cfg_ntf_rsp_handler(const ke_msg_id_t msgid,
                                       const struct ancc_wr_cfg_ntf_rsp *param,
                                       const ke_task_id_t dest_id,
                                       const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(dest_id);
    uint8_t status = param->status;
	NS_LOG_DEBUG("%s,status= 0x%x\r\n", __func__,status);
    if (param->op_code == ANCC_WR_NTF_NTF_SRC_OP_CODE)
    {
		if (status != GAP_ERR_NO_ERROR)
		{
			ns_ble_disconnect();
		}
    }
    else if (param->op_code == ANCC_WR_NTF_DATA_SRC_OP_CODE)
    {
		if (status == GAP_ERR_NO_ERROR)
		{
			app_ancc_wr_cfg_ntf_src(conidx, true);
		}
	#if (BLE_APP_SEC)
		else if (status == ATT_ERR_INSUFF_AUTHEN)
		{
		    NS_LOG_DEBUG("INSUFF_AUTH\r\n");
			ns_sec_send_security_req(conidx);
		}
	#endif
		else
		{
			// Disconnect in case of error
			ns_ble_disconnect();
		}
    }

    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS notification configuration read response message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_rd_cfg_ntf_rsp_handler(const ke_msg_id_t msgid,
                                       const struct ancc_rd_cfg_ntf_rsp *param,
                                       const ke_task_id_t dest_id,
                                       const ke_task_id_t src_id)
{

    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS notification of the Notification Source characteristic message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_ntf_src_ind_handler(const ke_msg_id_t msgid,
                                    const struct ancc_ntf_src_ind *param,
                                    const ke_task_id_t dest_id,
                                    const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(dest_id);
	NS_LOG_DEBUG("%s\r\n", __func__);

    if (param->value.event_id == EVT_ID_NTF_ADDED)
    {
        add_notification( &(param->value));
		NS_LOG_DEBUG("%s,1\r\n", __func__);
        if (!pending_notif)
        {
			NS_LOG_DEBUG("%s,2\r\n", __func__);
            fetch_next_notification(conidx);
			NS_LOG_DEBUG("%s,3\r\n", __func__);
        }
    }
    else if (param->value.event_id == EVT_ID_NTF_MODIFIED)
    {
    }
    else if (param->value.event_id == EVT_ID_NTF_REMOVED)
    {
    }
	NS_LOG_DEBUG("5555\r\n");	
    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS notification attribute value indication message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_ntf_att_ind_handler(const ke_msg_id_t msgid,
                                    const struct ancc_ntf_att_ind *param,
                                    const ke_task_id_t dest_id,
                                    const ke_task_id_t src_id)
{
    uint16_t copy_len;
    uint8_t *buf;

    NS_LOG_DEBUG("%s len=%u att=%u\r\n", __func__, param->length, param->att_id);

    if (!pending_notif)
    {
        return KE_MSG_CONSUMED;
    }

    buf = (uint8_t *)ke_malloc(param->length + 1, KE_MEM_NON_RETENTION);
    if (!buf)
    {
        NS_LOG_ERROR("ANCS: attribute buffer alloc failed, len=%u\r\n", param->length);
        return KE_MSG_CONSUMED;
    }

    if (param->length > 0)
    {
        memcpy(buf, param->value, param->length);
    }
    buf[param->length] = 0;

    switch (param->att_id)
    {
        case NTF_ATT_ID_TITLE:
            copy_len = (param->length < (ANCC_ATTS_MAX_LEN - 1)) ? param->length : (ANCC_ATTS_MAX_LEN - 1);
            memcpy(pending_notif->title, buf, copy_len);
            pending_notif->title[copy_len] = 0;
            break;

        case NTF_ATT_ID_DATE:
            copy_len = (param->length < (ANCC_ATTS_MAX_LEN - 1)) ? param->length : (ANCC_ATTS_MAX_LEN - 1);
            memcpy(pending_notif->date, buf, copy_len);
            pending_notif->date[copy_len] = 0;
            break;

        case NTF_ATT_ID_MSG:
            copy_len = (param->length < (ANCC_ATTS_MSG_MAX_LEN - 1)) ? param->length : (ANCC_ATTS_MSG_MAX_LEN - 1);
            memcpy(pending_notif->message, buf, copy_len);
            pending_notif->message[copy_len] = 0;
            break;

        case NTF_ATT_ID_APP_ID:
        {
            uint16_t len = param->length + 1;
            char *app_id = (char *)ke_malloc(len, KE_MEM_ENV);

            if (app_id)
            {
                memcpy(app_id, buf, param->length);
                app_id[param->length] = 0;

                if (pending_notif->app_id)
                {
                    ke_free(pending_notif->app_id);
                }
                pending_notif->app_id = app_id;
            }
            else
            {
                NS_LOG_ERROR("ANCS: app_id alloc failed, len=%u\r\n", param->length);
            }
        }
        break;

        default:
            break;
    }

    ke_free(buf);
    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS notification application value indication message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_app_att_ind_handler(const ke_msg_id_t msgid,
                                    const struct ancc_app_att_ind *param,
                                    const ke_task_id_t dest_id,
                                    const ke_task_id_t src_id)
{
    struct application *app;
    uint8_t *att;
    uint8_t *app_id;
    uint16_t size;

    NS_LOG_DEBUG("%s app_id_len=%u att_len=%u\r\n",
                 __func__, param->app_id_length, param->att_length);

    att = (uint8_t *)ke_malloc(param->att_length + 1, KE_MEM_NON_RETENTION);
    app_id = (uint8_t *)ke_malloc(param->app_id_length + 1, KE_MEM_NON_RETENTION);

    if (!att || !app_id)
    {
        NS_LOG_ERROR("ANCS: app attribute buffer alloc failed\r\n");
        if (att)
        {
            ke_free(att);
        }
        if (app_id)
        {
            ke_free(app_id);
        }
        return KE_MSG_CONSUMED;
    }

    if (param->att_length > 0)
    {
        memcpy(att, &param->values[param->app_id_length], param->att_length);
    }
    att[param->att_length] = 0;

    if (param->app_id_length > 0)
    {
        memcpy(app_id, param->values, param->app_id_length);
    }
    app_id[param->app_id_length] = 0;

    app = find_application((char *)app_id);
    if (!app)
    {
        app = add_application((char *)app_id);
    }

    if (app && param->att_id == APP_ATT_ID_DISPLAY_NAME)
    {
        if (!app->display_name)
        {
            size = (uint16_t)(strlen((char *)att) + 1);
            app->display_name = (char *)ke_malloc(size, KE_MEM_ENV);
            if (app->display_name)
            {
                memcpy(app->display_name, att, size);
            }
            else
            {
                NS_LOG_ERROR("ANCS: display_name alloc failed, len=%u\r\n", size);
            }
        }
    }

    ke_free(att);
    ke_free(app_id);
    return KE_MSG_CONSUMED;
}

/**
 * @brief Handles ANCS procedure complete information message
 * @param[in] msgid     Id of the message received.
 * @param[in] param     Pointer to the parameters of the message.
 * @param[in] dest_id   ID of the receiving task instance (TASK_GAP).
 * @param[in] src_id    ID of the sending task instance.
 * @return If the message was consumed or not.
 */
static int ancc_cmp_evt_handler(const ke_msg_id_t msgid,
                                const struct ancc_cmp_evt *param,
                                const ke_task_id_t dest_id,
                                const ke_task_id_t src_id)
{
    uint8_t conidx = KE_IDX_GET(dest_id);
	uint8_t status = param->status;
	NS_LOG_DEBUG("%s,status= 0x%x\r\n", __func__,status);

    switch (param->operation)
    {
        case ANCC_GET_NTF_ATTS_CMD_OP_CODE:
        {
          struct application *app = NULL;

          if (!pending_notif)
          {
             return KE_MSG_CONSUMED;
          }

       if (status != ATT_ERR_NO_ERROR)
       {
          NS_LOG_DEBUG("| FAILED to get attributes for 0x%08x\r\n\n", pending_notif->ntf.ntf_uid);
          free_pending_notif(conidx);
          return KE_MSG_CONSUMED;
       }

       if (pending_notif->app_id)
       {
          app = (struct application *)find_application(pending_notif->app_id);
          if (!app)
          {
              app_ancc_get_app_atts(conidx, APP_ATT_ID_DISPLAY_NAME_PRESENT, (uint8_t*)pending_notif->app_id);
              return KE_MSG_CONSUMED;
          }
       }

       print_notification(pending_notif, app);
       free_pending_notif(conidx);
      }break;

        case ANCC_GET_APP_ATTS_CMD_OP_CODE:
        {
         if (status != ATT_ERR_NO_ERROR)
        {
           NS_LOG_DEBUG("| FAILED to get attributes for %s\r\n\n", pending_notif->app_id);
        }

        if (pending_notif)
        {
          struct application *app = find_application(pending_notif->app_id);
          print_notification(pending_notif, app);
          free_pending_notif(conidx);
        }
       }break;

        case ANCC_PERF_NTF_ACT_CMD_OP_CODE:
        {
          NS_LOG_DEBUG("| Perform notification action status: %d\r\n", status);
        }break;

        default:
            break;
    }
    return KE_MSG_CONSUMED;
}

/*
 * LOCAL VARIABLES DEFINITION
 */

static const struct ke_msg_handler app_ancc_msg_handler_list[] =
{
    {ANCC_ENABLE_RSP,      (ke_msg_func_t)ancc_enable_handler},
    {ANCC_WR_CFG_NTF_RSP,  (ke_msg_func_t)ancc_wr_cfg_ntf_rsp_handler},
    {ANCC_RD_CFG_NTF_RSP,  (ke_msg_func_t)ancc_rd_cfg_ntf_rsp_handler},
    {ANCC_NTF_SRC_IND,     (ke_msg_func_t)ancc_ntf_src_ind_handler},
    {ANCC_NTF_ATT_IND,     (ke_msg_func_t)ancc_ntf_att_ind_handler},
    {ANCC_APP_ATT_IND,     (ke_msg_func_t)ancc_app_att_ind_handler},
    {ANCC_CMP_EVT,         (ke_msg_func_t)ancc_cmp_evt_handler},
};

/*
 * FUNCTION DEFINITIONS
 */
const struct app_subtask_handlers app_ancc_handlers = APP_HANDLERS(app_ancc);


#endif // (BLE_ANC_CLIENT)

#endif // BLE_APP_PRESENT

/// @} APP
