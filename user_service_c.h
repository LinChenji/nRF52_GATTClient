#ifndef USER_SERVICE_H_
#define USER_SERVICE_H_

#include <stdbool.h>    //bool
#include <stdint.h>     //uint16_t
#include <sdk_errors.h> //ret_code_t
#include <ble.h>        //ble_evt_t
#include <ble_db_discovery.h>
//#include <nrf_drv_rng.h>

//873226dc-f5d8-4586-94f9-6e794c2f6820(Little-endian)
#define BLE_UUID128_USER_SERVICE_BASE_UUID \
{0x20,0x68,0x2f,0x4c,0x79,0x6e,0xf9,0x94,\
 0x86,0x45,0xd8,0xf5,0xdc,0x26,0x32,0x87}

//#define BLE_UUID_USER_SERVICE_SHORT_UUID 0x26dc
//Octet[12:13]
#define BLE_UUID_USER_SERVICE_UUID 0x26dd  
#define BLE_UUID_TEST_CHAR_UUID    0x26de

#define BLE_US_C_DEF(_Obj)                        \
static ble_us_c_t _Obj;                           \
NRF_SDH_BLE_OBSERVER(_Obj ## _obs,                \
                     BLE_US_C_BLE_OBSERVER_PRIO,  \
                     ble_us_c_on_ble_evt, &_Obj)

extern uint16_t testCharVal;// 

typedef enum
{
    BLE_US_C_EVT_DISCOVERY_COMPLETE = 1,  /**< Event indicating that the Heart Rate Service has been discovered at the peer. */
    BLE_US_C_EVT_USTEST_NOTIFICATION ,    /**< Event indicating that a notification of the Heart Rate Measurement characteristic has been received from the peer. */
    BLE_US_C_EVT_USTEST_READ_RESP
} ble_us_c_evt_type_t;

typedef struct
{
    uint16_t testchar_cccd_handle;
    uint16_t testchar_handle;//us
} us_c_db_t;

typedef struct
{
    uint16_t testchar_value;
} ble_us_t;

typedef struct
{
    ble_us_c_evt_type_t evt_type;
    uint16_t conn_handle;
    union{
        us_c_db_t  peer_db;
        ble_us_t us;
    } params;
} ble_us_c_evt_t; 

typedef struct ble_us_c_s ble_us_c_t;
typedef void (*ble_us_c_evt_handler_t)(ble_us_c_t *p_us,ble_us_c_evt_t *p_evt);


typedef struct{
    ble_us_c_evt_handler_t       evt_handler;
    //uint16_t test_char_val;
    bool is_us_notify_support;
}ble_us_c_init_t;

struct ble_us_c_s{
    ble_us_c_evt_handler_t       evt_handler;
    uint16_t conn_handle;
    uint16_t peerTestCharVal;
    us_c_db_t peer_us_db;
    uint8_t  uuid_type;
    //uint16_t test_char_val;
};

ret_code_t ble_us_c_init(ble_us_c_t *p_us_c,ble_us_c_init_t *p_us_c_init);//sdk_errors.h
void ble_us_on_db_disc_evt(ble_us_c_t * p_ble_us_c, const ble_db_discovery_evt_t * p_evt);
void ble_us_c_on_ble_evt(ble_evt_t const *p_ble_evt,void *p_context);

uint32_t ble_us_write_testchar_change(ble_us_c_t * p_ble_us_c);
uint32_t ble_us_c_tcv_notify_config(ble_us_c_t * p_ble_us_c);//notify test char value
uint32_t ble_us_c_tcv_read(ble_us_c_t * p_ble_us_c);//write test char value
uint32_t ble_us_c_handles_assign(ble_us_c_t *    p_ble_us_c,
                                 uint16_t         conn_handle,
                                 us_c_db_t * p_peer_handles);
#endif