# nRF52 BLE_Central扫描对应自定义服务属性： #

Central对应ble_app_template_pca10040_s132(new service)，扫描自定义user_service并由central的三个按键来完成ATTR的读取、写入、开启通知CCCD操作。由于central没有模板例程，我干脆以ble_app_hrs_c为蓝本来修改，步骤如下：

1、	复制ble_app_hrs_c整个文件出来，重命名为ble_app_template_c_pca10040_s132(user)，
移动pca0040/s132/ses下的.emProject和flash_placement.xml到ble_app_template_c_pca10040_s132(user)文件夹下，同时pca10040/s132/config文件夹也移动到ble_app_template_c_pca10040_s132(user)文件夹下，把工程文件重命名为ble_app_template_pca10040_s132(new service).emProject：

![11.png](https://github.com/LinChenji/nRF52_GATTServer/blob/master/%E8%BF%87%E7%A8%8B%E6%88%AA%E5%9B%BE/11.png)
 
2、修改工程配置和路径。由于路径移动，相对位置改变也要同步改变。notepad打开ble_app_template_c_pca10040_s132(user).emProject同ble_peripheral修改一样，把所有“../../../../../../”替换为“../../../”，标签project configuration里的c_user_include_directories最后一项由“../config”改为“config/”，然后子标签“<folder Name="Application">”里“../../../main.c”改作“main.c”，“../config/sdk_config.h”改作“config/sdk_config.h”，同时更改工程名把标签solution name和project name都该做ble_app_template_c_pca10040_s132(user)，打开工程，发现SES标题栏变成了自己命名的solution name。编译查看是否报错。

 ![12.png](https://github.com/LinChenji/nRF52_GATTServer/blob/master/%E8%BF%87%E7%A8%8B%E6%88%AA%E5%9B%BE/12.png)

3、打开工程，同样右击工程菜单选择New Folder，取名BLE_Services，然后右击该文件夹
新建文件user_services_c.c和user_service_c.h，此时notepad打开ble_app_template_c_pca10040_s132(user).emProject可以看到多了标签

    <folder Name="nRF_BLE_Services">
      <file file_name="../../../components/ble/ble_services/ble_bas_c/ble_bas_c.c" />
      <file file_name="user_service_c.c" />
      <file file_name="user_service_c.h" />
    </folder>
4、	在main.c添加自定义服务client端实例对象
BLE_US_C_DEF(m_us_c);
然后在user_services_c.h添加该宏定义和回调函数ble_us_c_on_ble_evt：

    #define BLE_US_C_DEF(_Obj)  static ble_us_c_t _Obj;   \
    						NRF_SDH_BLE_OBSERVER(_Obj ## _obs,\
     											 BLE_US_C_BLE_OBSERVER_PRIO,  \
     											 ble_us_c_on_ble_evt, &_Obj)
    void ble_us_c_on_ble_evt(ble_evt_t const *p_ble_evt,void *p_context);
同时在sdk_config.h添加宏定义

    #ifndef BLE_US_C_BLE_OBSERVER_PRIO
    #define BLE_US_C_BLE_OBSERVER_PRIO 2  //ble_us_c
    #endif

 ![13.png](https://github.com/LinChenji/nRF52_GATTServer/blob/master/%E8%BF%87%E7%A8%8B%E6%88%AA%E5%9B%BE/13.png)

5、	接着定义自定义服务和特征UUID

    //873226dc-f5d8-4586-94f9-6e794c2f6820(Little-endian)
    #define BLE_UUID128_USER_SERVICE_BASE_UUID \
    {0x20,0x68,0x2f,0x4c,0x79,0x6e,0xf9,0x94,\
     0x86,0x45,0xd8,0xf5,0xdc,0x26,0x32,0x87}
    
    //Octet[12:13]
    #define BLE_UUID_USER_SERVICE_UUID  0x26dd  
    #define BLE_UUID_TEST_CHAR_UUID 0x26de

6、	声明服务client端类型和服务client端初始化类型

    typedef enum
    {
    	BLE_US_C_EVT_DISCOVERY_COMPLETE = 1,  /**< Event indicating that the Heart Rate Service has been discovered at the peer. */
    	BLE_US_C_EVT_USTEST_NOTIFICATION ,/**< Event indicating that a notification of the Heart Rate Measurement characteristic has been received from the peer. */
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
    	ble_us_c_evt_handler_t   evt_handler;
    	//uint16_t test_char_val;
    	bool is_us_notify_support;
    }ble_us_c_init_t;
    
    struct ble_us_c_s{
    	ble_us_c_evt_handler_t   evt_handler;
    	uint16_t conn_handle;
    	uint16_t peerTestCharVal;
    	us_c_db_t peer_us_db;
    	uint8_t  uuid_type;
    	//uint16_t test_char_val;
    };
7、	声明服务client端初始化函数；特征读取、写入、通知、分配函数：

    ret_code_t ble_us_c_init(ble_us_c_t *p_us_c,ble_us_c_init_t *p_us_c_init);//sdk_errors.h
    void ble_us_on_db_disc_evt(ble_us_c_t * p_ble_us_c, const ble_db_discovery_evt_t * p_evt);
    void ble_us_c_on_ble_evt(ble_evt_t const *p_ble_evt,void *p_context);
    
    uint32_t ble_us_write_testchar_change(ble_us_c_t * p_ble_us_c);
    uint32_t ble_us_c_tcv_notify_config(ble_us_c_t * p_ble_us_c);//notify test char value
    uint32_t ble_us_c_tcv_read(ble_us_c_t * p_ble_us_c);//write test char value
    uint32_t ble_us_c_handles_assign(ble_us_c_t *p_ble_us_c,uint16_t conn_handle,
     								 us_c_db_t * p_peer_handles);
然后在user_services_c.c实现相应函数功能，具体参考源码。

8、	在主函数main.c添加#include "user_service_c.h"//user_service_c，屏蔽掉hrs_c_init和
bas_c_init（也可保留），然后添加client端的扫描、连接、按键处理、操作函数。首先修改目的设备蓝牙地址和添加服务client端实例定义：

    #define TARGET_UUID BLE_UUID_USER_SERVICE_UUID
    BLE_US_C_DEF(m_us_c);

修改按键处理函数bsp_event_handler，添加读取、写入、CCCD使能处理分支：

        case BSP_EVENT_KEY_0://Read test char value
            NRF_LOG_INFO("Change testchar value.");
            err_code = ble_us_write_testchar_change(&m_us_c);
            if (err_code != NRF_SUCCESS &&
                err_code != BLE_ERROR_INVALID_CONN_HANDLE &&
                err_code != NRF_ERROR_INVALID_STATE &&
                err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_KEY_1://Write test char value
            NRF_LOG_INFO("Get testchar value.");
            err_code = ble_us_c_tcv_read(&m_us_c);
            if (err_code != NRF_SUCCESS &&
                err_code != BLE_ERROR_INVALID_CONN_HANDLE &&
                err_code != NRF_ERROR_INVALID_STATE &&
                err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_KEY_2://Enable/Disable CCCD of test char value
            NRF_LOG_DEBUG("Enable/Disable cccd of testchar.");
            err_code = ble_us_c_tcv_notify_config(&m_us_c);
            APP_ERROR_CHECK(err_code);
            break;
9、	主函数修改BLE事件处理函数：ble_evt_handler的BLE_GAP_EVT_CONNECTED分支添加：

    printf("Connected to Device ");
    uint8_t i;
    for(i=5;i>0;i--)
    printf("%2x:",p_gap_evt->params.connected.peer_addr.addr[i]);
    printf("%2x\n",p_gap_evt->params.connected.peer_addr.addr[i]);

然后在广播监听函数on_adv_report做修改：

    ble_uuid_t target_uuid = {.uuid = TARGET_UUID,.type=BLE_UUID_TYPE_VENDOR_BEGIN};
    uint8_t i;
    printf("RSSI:%ddBm ",p_adv_report->rssi);
    for(i=5;i>0;i--)
    printf("%2x:",p_adv_report->peer_addr.addr[i]);
    printf("%2x\n",p_adv_report->peer_addr.addr[i]);
修改db扫描回调函数db_disc_handler：
 
![14.png](https://github.com/LinChenji/nRF52_GATTServer/blob/master/%E8%BF%87%E7%A8%8B%E6%88%AA%E5%9B%BE/14.png)

10、初始化自定义服务client。在main主函数添加：

    ble_us_c_init_t p_us_init_c;
    p_us_init_c.evt_handler=us_c_evt_handler;
    err_code=ble_us_c_init(&m_us_c,&p_us_init_c);
	APP_ERROR_CHECK(err_code);/**/
然后实现事件处理函数：

    static void us_c_evt_handler(ble_us_c_t * p_us_c, ble_us_c_evt_t * p_us_c_evt)
    {
    	ret_code_t err_code;

	    switch (p_us_c_evt->evt_type)
	    {
	        case BLE_US_C_EVT_DISCOVERY_COMPLETE:
	        {
	            err_code = ble_us_c_handles_assign(p_us_c, p_us_c_evt->conn_handle,
	                                                &p_us_c_evt->params.peer_db);
	            APP_ERROR_CHECK(err_code);
	
	            // Initiate bonding.
	            err_code = pm_conn_secure(p_us_c_evt->conn_handle, false);
	            if (err_code != NRF_ERROR_INVALID_STATE)
	            {
	                APP_ERROR_CHECK(err_code);
	            }
	
	            NRF_LOG_DEBUG("User Service discovered.");//Reading testchar level.
	        } break;
	
	        case BLE_US_C_EVT_USTEST_NOTIFICATION:
	            NRF_LOG_INFO("Testchar value Notified: %x .", p_us_c_evt->params.us.testchar_value);
	            break;
	
	        case BLE_US_C_EVT_USTEST_READ_RESP:
	            NRF_LOG_INFO("Testchar value Read: %x .", p_us_c_evt->params.us.testchar_value);//testCharVal
	            break;
	
	        default:
	            break;
	    }
    }

11、同样此时仍要修改sdk_config.h。搜索NRF_SDH_BLE_VS_UUID_COUNT，把值改成2。
添加

    #ifndef BLE_US_C_BLE_OBSERVER_PRIO
    #define BLE_US_C_BLE_OBSERVER_PRIO 2  //ble_us_c
    #endif
编译运行提示存储不足，可按提示进行修改。
