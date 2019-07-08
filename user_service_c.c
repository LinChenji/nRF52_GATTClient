#include "user_service_c.h"
#include <ble_srv_common.h>
#include <sdk_common.h>
#include <sdk_macros.h>
#include <ble.h>
#include <ble_gattc.h>
#include <ble_gap.h>
#include <ble_db_discovery.h>
//#include <nrf_drv_rng.h>

#define NRF_LOG_MODULE_NAME ble_us_c
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

uint16_t testCharVal=0;
uint16_t writeTestVal;
uint8_t ranInx=0;
bool cccdToggle=false;

#define TX_BUFFER_MASK         0x07                  /**< TX Buffer mask, must be a mask of continuous zeroes, followed by continuous sequence of ones: 000...111. */
#define TX_BUFFER_SIZE         (TX_BUFFER_MASK + 1)  /**< Size of send buffer, which is 1 higher than the mask. */

#define WRITE_MESSAGE_LENGTH   BLE_CCCD_VALUE_LEN    /**< Length of the write message for CCCD. */
#define WRITE_MESSAGE_LENGTH   BLE_CCCD_VALUE_LEN    //ble_src_common.h:2

typedef enum
{
    READ_REQ,  /**< Type identifying that this tx_message is a read request. */
    WRITE_REQ  /**< Type identifying that this tx_message is a write request. */
} tx_request_t;

typedef struct
{
    uint8_t                  gattc_value[WRITE_MESSAGE_LENGTH];  /**< The message to write. */
    ble_gattc_write_params_t gattc_params;                       /**< GATTC parameters for this message. */
} write_params_t;

typedef struct
{
    uint16_t     conn_handle;  /**< Connection handle to be used when transmitting this message. */
    tx_request_t type;         /**< Type of this message, i.e. read or write message. */
    union
    {
        uint16_t       read_handle;  /**< Read request message. */
        write_params_t write_req;    /**< Write request message. */
    } req;
} tx_message_t;

static tx_message_t m_tx_buffer[TX_BUFFER_SIZE];  /**< Transmit buffer for messages to be transmitted to the central. */
static uint32_t     m_tx_insert_index = 0;        /**< Current index in the transmit buffer where the next message should be inserted. */
static uint32_t     m_tx_index = 0;               /**< Current index in the transmit buffer from where the next message to be transmitted resides. */

/*static uint8_t random_vector_generate(uint8_t * p_buff, uint8_t size)
{
    uint32_t err_code;
    uint8_t  available;

    nrf_drv_rng_bytes_available(&available);
    uint8_t length = MIN(size, available);//nordic_common.h

    err_code = nrf_drv_rng_rand(p_buff, length);
    APP_ERROR_CHECK(err_code);

    return length;
}*/

static void tx_buffer_process(void)
{
    if (m_tx_index != m_tx_insert_index)
    {
        uint32_t err_code;

        if (m_tx_buffer[m_tx_index].type == READ_REQ)
        {
            err_code = sd_ble_gattc_read(m_tx_buffer[m_tx_index].conn_handle,
                                         m_tx_buffer[m_tx_index].req.read_handle,0);
        }
        else
        {
            err_code = sd_ble_gattc_write(m_tx_buffer[m_tx_index].conn_handle,
                                          &m_tx_buffer[m_tx_index].req.write_req.gattc_params);
        }
        if (err_code == NRF_SUCCESS)
        {
            switch(m_tx_buffer[m_tx_index].type)
            {
                case READ_REQ:

                break;

                case WRITE_REQ:
                {
                    uint8_t writeCharVal[2];
                    memcpy(writeCharVal,m_tx_buffer[m_tx_index].req.write_req.gattc_params.p_value,\
                           m_tx_buffer[m_tx_index].req.write_req.gattc_params.len);
                    testCharVal=writeCharVal[0]|(((uint16_t)writeCharVal[1])<<8);
                }
                break;

                default:break;
            }
            m_tx_index++;
            m_tx_index &= TX_BUFFER_MASK;//0111B
        }
        else
        {
            NRF_LOG_DEBUG("SD Read/Write API returns error. This message sending will be "
                          "attempted again..");
        }
    }
}

static void on_read_rsp(ble_us_c_t * p_us_c, ble_evt_t const * p_ble_evt)
{
    const ble_gattc_evt_read_rsp_t * p_readrsp;

    // Check if the event if on the link for this instance
    if (p_us_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle)
    {
        return;
    }

    p_readrsp = &p_ble_evt->evt.gattc_evt.params.read_rsp;

    if (p_readrsp->handle == p_us_c->peer_us_db.testchar_handle)
    {
        ble_us_c_evt_t evt;

        evt.conn_handle = p_ble_evt->evt.gattc_evt.conn_handle;
        evt.evt_type = BLE_US_C_EVT_USTEST_READ_RESP;

        evt.params.us.testchar_value = p_readrsp->data[0];
        testCharVal = p_readrsp->data[0];

        p_us_c->evt_handler(p_us_c, &evt);//us_c_evt_handler
    }
    tx_buffer_process();
}

static void on_write_rsp(ble_us_c_t * p_ble_us_c, const ble_evt_t * p_ble_evt)
{
    // Check if the event if on the link for this instance
    if (p_ble_us_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle)
    {
        return;
    }
    // Check if there is any message to be sent across to the peer and send it.
    tx_buffer_process();
}

static void on_hvx(ble_us_c_t * p_ble_us_c, const ble_evt_t * p_ble_evt)
{
    // Check if the event is on the link for this instance
    if (p_ble_us_c->conn_handle != p_ble_evt->evt.gattc_evt.conn_handle)
    {
        NRF_LOG_DEBUG("Received HVX on link 0x%x, not associated to this instance, ignore",
                      p_ble_evt->evt.gattc_evt.conn_handle);
        return;
    }

    NRF_LOG_DEBUG("Received HVX on link 0x%x, testchar_handle 0x%x",
                    p_ble_evt->evt.gattc_evt.params.hvx.handle,
                    p_ble_us_c->peer_us_db.testchar_handle);

    // Check if this is a heart rate notification.
    if (p_ble_evt->evt.gattc_evt.params.hvx.handle == p_ble_us_c->peer_us_db.testchar_handle)
    {
        ble_us_c_evt_t  ble_us_c_evt;
        uint32_t        index = 0;

        ble_us_c_evt.evt_type                    = BLE_US_C_EVT_USTEST_NOTIFICATION;
        ble_us_c_evt.conn_handle                 = p_ble_us_c->conn_handle;
        ble_us_c_evt.params.us.testchar_value    = \
            p_ble_evt->evt.gattc_evt.params.hvx.data[0];
        testCharVal=ble_us_c_evt.params.us.testchar_value;
        p_ble_us_c->evt_handler(p_ble_us_c, &ble_us_c_evt);//us_c_evt_handler
    }
}

static void on_disconnected(ble_us_c_t * p_ble_us_c, const ble_evt_t * p_ble_evt)
{
    if (p_ble_us_c->conn_handle == p_ble_evt->evt.gap_evt.conn_handle)
    {
        p_ble_us_c->conn_handle                 = BLE_CONN_HANDLE_INVALID;
        p_ble_us_c->peer_us_db.testchar_cccd_handle = BLE_GATT_HANDLE_INVALID;
        p_ble_us_c->peer_us_db.testchar_handle      = BLE_GATT_HANDLE_INVALID;
    }
}


ret_code_t ble_us_c_init(ble_us_c_t *p_us_c,ble_us_c_init_t *p_us_c_init)
{
    ret_code_t    err_code;
    ble_uuid_t us_uuid;
    VERIFY_PARAM_NOT_NULL(p_us_c);
    VERIFY_PARAM_NOT_NULL(p_us_c_init);
    //VERIFY_PARAM_NOT_NULL(p_us_c_init->evt_handler);
   
    ble_uuid128_t us_base_uuid = {BLE_UUID128_USER_SERVICE_BASE_UUID};

    p_us_c->evt_handler                 = p_us_c_init->evt_handler;
    p_us_c->conn_handle                 = BLE_CONN_HANDLE_INVALID;
    p_us_c->peer_us_db.testchar_handle  = BLE_GATT_HANDLE_INVALID;
    p_us_c->peer_us_db.testchar_cccd_handle  = BLE_GATT_HANDLE_INVALID;
    //p_us_c->peer_hrs_db.hrm_cccd_handle = BLE_GATT_HANDLE_INVALID;
    //p_us_c->peer_hrs_db.hrm_handle      = BLE_GATT_HANDLE_INVALID;
    err_code = sd_ble_uuid_vs_add(&us_base_uuid, &p_us_c->uuid_type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }/**/
    us_uuid.type = p_us_c->uuid_type;//BLE_UUID_TYPE_VENDOR_BEGIN;
    us_uuid.uuid = BLE_UUID_USER_SERVICE_UUID;

    return ble_db_discovery_evt_register(&us_uuid);//ble_db_discovery.c
}

void ble_us_on_db_disc_evt(ble_us_c_t * p_ble_us_c, const ble_db_discovery_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE &&
        p_evt->params.discovered_db.srv_uuid.uuid == BLE_UUID_USER_SERVICE_UUID &&
        p_evt->params.discovered_db.srv_uuid.type == p_ble_us_c->uuid_type)
    {
        ble_us_c_evt_t evt;

        evt.evt_type    = BLE_US_C_EVT_DISCOVERY_COMPLETE;
        evt.conn_handle = p_evt->conn_handle;

        for (uint32_t i = 0; i < p_evt->params.discovered_db.char_count; i++)
        {
            const ble_gatt_db_char_t * p_char = &(p_evt->params.discovered_db.charateristics[i]);
            switch (p_char->characteristic.uuid.uuid)
            {
                case BLE_UUID_TEST_CHAR_UUID:
                    evt.params.peer_db.testchar_handle      = p_char->characteristic.handle_value;
                    evt.params.peer_db.testchar_cccd_handle = p_char->cccd_handle;
                    break;

                default:
                    break;
            }
        }

        NRF_LOG_INFO("User Service discovered at peer.");
        //If the instance has been assigned prior to db_discovery, assign the db_handles
        if (p_ble_us_c->conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            if ((p_ble_us_c->peer_us_db.testchar_handle      == BLE_GATT_HANDLE_INVALID)&&
                (p_ble_us_c->peer_us_db.testchar_cccd_handle == BLE_GATT_HANDLE_INVALID))
            {
                p_ble_us_c->peer_us_db = evt.params.peer_db;
            }
        }

        p_ble_us_c->evt_handler(p_ble_us_c, &evt);//us_c_evt_handler

    }
}

void ble_us_c_on_ble_evt(ble_evt_t const *p_ble_evt,void *p_context)
{
    ble_us_c_t * p_ble_us_c = (ble_us_c_t *)p_context;

    if ((p_ble_us_c == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    NRF_LOG_DEBUG(" US characteristic operation ID:%x ",p_ble_evt->header.evt_id);
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED://ble_gap.h:0x11
            on_disconnected(p_ble_us_c, p_ble_evt);
            break;

        case BLE_GATTC_EVT_HVX://ble_gattc.h:0x39
            on_hvx(p_ble_us_c, p_ble_evt);
            break;

        case BLE_GATTC_EVT_WRITE_RSP://ble_gattc.h:0x38
            on_write_rsp(p_ble_us_c, p_ble_evt);
            break;

        case BLE_GATTC_EVT_READ_RSP://ble_gattc.h:0x36
            on_read_rsp(p_ble_us_c, p_ble_evt);
            break;

        default:
            break;
    }
}

uint32_t ble_us_write_testchar_change(ble_us_c_t * p_ble_us_c)
{
    VERIFY_PARAM_NOT_NULL(p_ble_us_c);

    if (p_ble_us_c->conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    tx_message_t * p_msg;
    p_msg              = &m_tx_buffer[m_tx_insert_index++];
    m_tx_insert_index &= TX_BUFFER_MASK;
    uint8_t randomUnpacked[8]={0x86,0xa7,0xb5,0xa8,0x27,0x62,0xa5,0x7b};
    //uint8_t length = random_vector_generate(randomUnpacked,2);
        writeTestVal=randomUnpacked[ranInx];
        //(((uint16_t)randomUnpacked[ranInx+1])<<8);
    if(ranInx<8) ranInx+=1;
    else ranInx=0;

    NRF_LOG_INFO("writing testchar value:0x%x", writeTestVal);

    p_msg->req.write_req.gattc_params.handle   = p_ble_us_c->peer_us_db.testchar_handle;
    p_msg->req.write_req.gattc_params.len      = sizeof(writeTestVal);
    p_msg->req.write_req.gattc_params.p_value  = p_msg->req.write_req.gattc_value;
    p_msg->req.write_req.gattc_params.offset   = 0;
    p_msg->req.write_req.gattc_params.write_op = BLE_GATT_OP_WRITE_CMD;
    p_msg->req.write_req.gattc_value[0]        = (uint8_t)writeTestVal;
    p_msg->conn_handle                         = p_ble_us_c->conn_handle;
    p_msg->type                                = WRITE_REQ;

    tx_buffer_process();
    return NRF_SUCCESS;
}
static uint32_t cccd_configure(uint16_t conn_handle, uint16_t handle_cccd, bool notification_enable)
{
    NRF_LOG_DEBUG("Configuring CCCD. CCCD Handle = %d, Connection Handle = %d",
                                                            handle_cccd,conn_handle);

    tx_message_t * p_msg;
    uint16_t       cccd_val = notification_enable ? BLE_GATT_HVX_NOTIFICATION : 0;

    p_msg              = &m_tx_buffer[m_tx_insert_index++];
    m_tx_insert_index &= TX_BUFFER_MASK;

    p_msg->req.write_req.gattc_params.handle   = handle_cccd;
    p_msg->req.write_req.gattc_params.len      = WRITE_MESSAGE_LENGTH;
    p_msg->req.write_req.gattc_params.p_value  = p_msg->req.write_req.gattc_value;
    p_msg->req.write_req.gattc_params.offset   = 0;
    p_msg->req.write_req.gattc_params.write_op = BLE_GATT_OP_WRITE_REQ;
    p_msg->req.write_req.gattc_value[0]        = LSB_16(cccd_val);
    p_msg->req.write_req.gattc_value[1]        = MSB_16(cccd_val);
    p_msg->conn_handle                         = conn_handle;
    p_msg->type                                = WRITE_REQ;

    if((cccd_val & BLE_GATT_HVX_NOTIFICATION)!=0)
    {
        NRF_LOG_INFO("Test Char cccd:Enable.");
    }
    else
        NRF_LOG_INFO("Test Char cccd:Disable.");

    tx_buffer_process();
    return NRF_SUCCESS;
}
uint32_t ble_us_c_tcv_notify_config(ble_us_c_t * p_ble_us_c)
{
    VERIFY_PARAM_NOT_NULL(p_ble_us_c);

    if (p_ble_us_c->conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    if(cccdToggle==true) cccdToggle=false;
    else cccdToggle=true;
    return cccd_configure(p_ble_us_c->conn_handle, \
                          p_ble_us_c->peer_us_db.testchar_cccd_handle, cccdToggle);
}


uint32_t ble_us_c_tcv_read(ble_us_c_t * p_ble_us_c)
{
    VERIFY_PARAM_NOT_NULL(p_ble_us_c);
    if (p_ble_us_c->conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    tx_message_t * msg;

    msg                  = &m_tx_buffer[m_tx_insert_index++];
    m_tx_insert_index   &= TX_BUFFER_MASK;

    msg->req.read_handle = p_ble_us_c->peer_us_db.testchar_handle;
    msg->conn_handle     = p_ble_us_c->conn_handle;
    msg->type            = READ_REQ;

    tx_buffer_process();//sd_ble_gattc_read
    //NRF_LOG_INFO("ble_us_c_tcv_read\n");
    return NRF_SUCCESS;
}

uint32_t ble_us_c_handles_assign(ble_us_c_t *    p_ble_us_c,
                                 uint16_t         conn_handle,
                                 us_c_db_t * p_peer_handles)
{    
    VERIFY_PARAM_NOT_NULL(p_ble_us_c);

    p_ble_us_c->conn_handle = conn_handle;
    if (p_peer_handles != NULL)
    {
        p_ble_us_c->peer_us_db = *p_peer_handles;
    }
    //NRF_LOG_INFO("ble_us_c_handles_assign\n");
    return NRF_SUCCESS;
}