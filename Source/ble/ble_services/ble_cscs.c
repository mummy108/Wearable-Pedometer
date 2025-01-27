/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "ble_cscs.h"
#include <string.h>
#include "nordic_common.h"
#include "ble_l2cap.h"
#include "ble_srv_common.h"
#include "app_util.h"

#define OPCODE_LENGTH  1                                                    /**< Length of opcode inside Cycling Speed and Cadence Measurement packet. */
#define HANDLE_LENGTH  2                                                    /**< Length of handle inside Cycling Speed and Cadence Measurement packet. */
#define MAX_CSCM_LEN   (BLE_L2CAP_MTU_DEF - OPCODE_LENGTH - HANDLE_LENGTH)  /**< Maximum size of a transmitted Cycling Speed and Cadence Measurement. */

// Cycling Speed and Cadence Measurement flag bits
#define CSC_MEAS_FLAG_MASK_WHEEL_REV_DATA_PRESENT   (0x01 << 0)             /**< Wheel revolution data present flag bit. */
#define CSC_MEAS_FLAG_MASK_CRANK_REV_DATA_PRESENT   (0x01 << 1)             /**< Crank revolution data present flag bit. */


/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_cscs      Cycling Speed and Cadence Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_cscs_t * p_cscs, ble_evt_t * p_ble_evt)
{
    p_cscs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_cscs      Cycling Speed and Cadence Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_cscs_t * p_cscs, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_cscs->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling write events to the CSCS Measurement characteristic.
 *
 * @param[in]   p_cscs        Cycling Speed and Cadence Service structure.
 * @param[in]   p_evt_write   Write event received from the BLE stack.
 */
static void on_meas_cccd_write(ble_cscs_t * p_cscs, ble_gatts_evt_write_t * p_evt_write)
{
    if (p_evt_write->len == 2)
    {
        // CCCD written, update notification state
        if (p_cscs->evt_handler != NULL)
        {
            ble_cscs_evt_t evt;
            
            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_CSCS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_CSCS_EVT_NOTIFICATION_DISABLED;
            }
            
            p_cscs->evt_handler(p_cscs, &evt);
        }
    }
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_cscs      Cycling Speed and Cadence Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_cscs_t * p_cscs, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
    
    if (p_evt_write->handle == p_cscs->meas_handles.cccd_handle)
    {
        on_meas_cccd_write(p_cscs, p_evt_write);
    }
}


void ble_cscs_on_ble_evt(ble_cscs_t * p_cscs, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_cscs, p_ble_evt);
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_cscs, p_ble_evt);
            break;
            
        case BLE_GATTS_EVT_WRITE:
            on_write(p_cscs, p_ble_evt);
            break;
            
        default:
            break;
    }
}


/**@brief Function for encoding a CSCS Measurement.
 *
 * @param[in]   p_cscs              Cycling Speed and Cadence Service structure.
 * @param[in]   p_csc_measurement   Measurement to be encoded.
 * @param[out]  p_encoded_buffer    Buffer where the encoded data will be written.
 *
 * @return      Size of encoded data.
 */
static uint8_t csc_measurement_encode(ble_cscs_t *       p_cscs,
                                      ble_cscs_meas_t *  p_csc_measurement,
                                      uint8_t *          p_encoded_buffer)
{
    uint8_t flags = 0;
    uint8_t len   = 1;

    // Cumulative Wheel Revolutions and Last Wheel Event Time Fields
    if (p_cscs->feature & BLE_CSCS_FEATURE_WHEEL_REV_BIT)
    {
        if (p_csc_measurement->is_wheel_rev_data_present)
        {
            flags |= CSC_MEAS_FLAG_MASK_WHEEL_REV_DATA_PRESENT;
            len += uint32_encode(p_csc_measurement->cumulative_wheel_revs, &p_encoded_buffer[len]);
            len += uint16_encode(p_csc_measurement->last_wheel_event_time, &p_encoded_buffer[len]);
        }
    }
    
    // Cumulative Crank Revolutions and Last Crank Event Time Fields
    if (p_cscs->feature & BLE_CSCS_FEATURE_WHEEL_REV_BIT)
    {
        if (p_csc_measurement->is_crank_rev_data_present)
        {
            flags |= CSC_MEAS_FLAG_MASK_CRANK_REV_DATA_PRESENT;
            len += uint32_encode(p_csc_measurement->cumulative_crank_revs, &p_encoded_buffer[len]);
            len += uint16_encode(p_csc_measurement->last_crank_event_time, &p_encoded_buffer[len]);
        }
    }
    
    // Flags Field
    p_encoded_buffer[0] = flags;
    
    return len;
}


/**@brief Function for adding CSC Measurement characteristics.
 *
 * @param[in]   p_cscs        Cycling Speed and Cadence Service structure.
 * @param[in]   p_cscs_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t csc_measurement_char_add(ble_cscs_t * p_cscs, const ble_cscs_init_t * p_cscs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    ble_cscs_meas_t     initial_scm;
    uint8_t             encoded_scm[MAX_CSCM_LEN];
    
    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    cccd_md.write_perm = p_cscs_init->csc_meas_attr_md.cccd_write_perm;
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;
    
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CSC_MEASUREMENT_CHAR);
    
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_cscs_init->csc_meas_attr_md.read_perm;
    attr_md.write_perm = p_cscs_init->csc_meas_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = csc_measurement_encode(p_cscs, &initial_scm, encoded_scm);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = MAX_CSCM_LEN;
    attr_char_value.p_value      = encoded_scm;
    
    return sd_ble_gatts_characteristic_add(p_cscs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cscs->meas_handles);
}


/**@brief Function for adding CSC Feature characteristics.
 *
 * @param[in]   p_cscs        Cycling Speed and Cadence Service structure.
 * @param[in]   p_cscs_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t csc_feature_char_add(ble_cscs_t * p_cscs, const ble_cscs_init_t * p_cscs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             init_value_encoded[2];
    uint8_t             init_value_len;
    
    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.read  = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf        = NULL;
    char_md.p_user_desc_md   = NULL;
    char_md.p_cccd_md        = NULL;
    char_md.p_sccd_md        = NULL;
    
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CSC_FEATURE_CHAR);
    
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_cscs_init->csc_feature_attr_md.read_perm;
    attr_md.write_perm = p_cscs_init->csc_feature_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    init_value_len = uint16_encode(p_cscs_init->feature, &init_value_encoded[0]);

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = init_value_len;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = init_value_len;
    attr_char_value.p_value      = init_value_encoded;
    
    return sd_ble_gatts_characteristic_add(p_cscs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cscs->feature_handles);
}


uint32_t ble_cscs_init(ble_cscs_t * p_cscs, const ble_cscs_init_t * p_cscs_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

    // Initialize service structure
    p_cscs->evt_handler = p_cscs_init->evt_handler;
    p_cscs->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_cscs->feature     = p_cscs_init->feature;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CYCLING_SPEED_AND_CADENCE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_cscs->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Add cycling speed and cadence measurement characteristic
    err_code = csc_measurement_char_add(p_cscs, p_cscs_init);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Add cycling speed and cadence feature characteristic
    err_code = csc_feature_char_add(p_cscs, p_cscs_init);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    return NRF_SUCCESS;
}


uint32_t ble_cscs_measurement_send(ble_cscs_t * p_cscs, ble_cscs_meas_t * p_measurement)
{
    uint32_t err_code;
    
    // Send value if connected and notifying
    if (p_cscs->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint8_t                encoded_csc_meas[MAX_CSCM_LEN];
        uint16_t               len;
        uint16_t               hvx_len;
        ble_gatts_hvx_params_t hvx_params;
        
        len     = csc_measurement_encode(p_cscs, p_measurement, encoded_csc_meas);
        hvx_len = len;

        memset(&hvx_params, 0, sizeof(hvx_params));
        
        hvx_params.handle   = p_cscs->meas_handles.value_handle;
        hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset   = 0;
        hvx_params.p_len    = &hvx_len;
        hvx_params.p_data   = encoded_csc_meas;
        
        err_code = sd_ble_gatts_hvx(p_cscs->conn_handle, &hvx_params);
        if ((err_code == NRF_SUCCESS) && (hvx_len != len))
        {
            err_code = NRF_ERROR_DATA_SIZE;
        }
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

    return err_code;
}
