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
 * @file app_ancc.h
 * @author Nations Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */

#ifndef APP_ANCC_H_
#define APP_ANCC_H_

/* Includes ------------------------------------------------------------------*/
#if (BLE_ANC_CLIENT)

#include "anc_common.h"
#include "ancc.h"

/* Public define ------------------------------------------------------------*/
#define ANCC_ATTS_MAX_LEN       (25)
#define ANCC_ATTS_MSG_MAX_LEN   (75)
#define APP_ANCC_DELAY          (1000)
/* Public typedef -----------------------------------------------------------*/
// ANCS notification details info
struct notification
{
    struct co_list_hdr hdr;
    struct anc_ntf_src ntf;
    char title[ANCC_ATTS_MAX_LEN];
    char date[ANCC_ATTS_MAX_LEN];
    char message[ANCC_ATTS_MSG_MAX_LEN];
    char *app_id;
};

// ANCS application info
struct application
{
    struct co_list_hdr hdr;
    char *app_id;
    char *display_name;
};
/* Public function prototypes -----------------------------------------------*/

/**
 * @brief Initialize ANC Service Application
 */
void app_ancc_init(void);

/** 
 * @brief Create raw data transfer server profile database.
 * @return void
 */

void app_ancc_add_ancs(void);


/**
 * @brief Enable ANC Service client
 * @param[in] conidx            The connection id number
 */
void app_ancc_enable(uint8_t conidx);

/**
 * @brief Write Notification Source configuration
 * @param[in] conidx            The connection id number
 * @param[in] enable            Notification enable or disable
 */
void app_ancc_wr_cfg_ntf_src(uint8_t conidx, bool enable);

/**
 * @brief Write Data Source configuration
 * @param[in] conidx            The connection id number
 * @param[in] enable            Notification enable or disable
 */
void app_ancc_wr_cfg_data_src(uint8_t conidx, bool enable);

/**
 * @brief Read Notification Source configuration
 * @param[in] conidx            The connection id number
 */
void app_ancc_rd_cfg_ntf_src(uint8_t conidx);

/**
 * @brief Read Data Source configuration
 * @param[in] conidx            The connection id number
 */
void app_ancc_rd_cfg_data_src(uint8_t conidx);

/**
 * @brief Execute Get notification attributes command
 * @param[in] conidx            The connection id number
 * @param[in] uid               Notification UID
 * @param[in] atts              Requested attributes mask
 * @param[in] title_len         Length of Title attribute
 * @param[in] sub_len           Length of Subtitle attribute
 * @param[in] msg_len           Length of Message attribute
 */
void app_ancc_get_ntf_atts(uint8_t conidx, uint32_t uid, uint8_t atts, uint16_t title_len,
                               uint16_t sub_len, uint16_t msg_len);

/**
 * @brief Execute Get application attributes command
 * @param[in] conidx            The connection id number
 * @param[in] atts              Requested attributes mask
 * @param[in] app_id            Application ID - must be null terminated
 */
void app_ancc_get_app_atts(uint8_t conidx, uint8_t atts, uint8_t *app_id);

/**
 * @brief Execute Get application attributes command
 * @param[in] conidx            The connection id number
 * @param[in] uid               Notification UID
 * @param[in] act_positive      True if action shall be positive
 */
void app_ancc_ntf_action(uint8_t conidx, uint32_t uid, bool act_positive);

void clear_ancs_data(void);

/// Table of message handlers

extern const struct app_subtask_handlers app_ancc_handlers;


#endif // (BLE_ANC_CLIENT)

#endif // APP_ANCC_H_

///@}
///@}
///@}
