/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_check.h"

#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"

//for EEPROM
#include "nvs_flash.h"

//for Modbus
#include "mdns.h"           
#include "mbcontroller.h"
#include "esp_modbus_common.h"
#include "esp_modbus_slave.h"
//#include "esp_modbus_master.h"

//for SHT4x
#include "sht4x.h"
#include <esp_system.h>
#include <esp_err.h>

// needed for L2 TAP VFS
#include <unistd.h> // read/write
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include "esp_vfs_l2tap.h"
#include "lwip/prot/ethernet.h" // Ethernet headers
#include "errno.h"
#include "arpa/inet.h" // for ntohs, etc.

#include "esp_eth_ksz8863.h"

typedef struct {
    struct eth_hdr header;
    union {
        int cnt;
        char str[44];
    };
} test_vfs_eth_tap_msg_t;

static void* mb_slave = NULL;

// 1. Modbus Holding Register
typedef struct {
    uint16_t temp_x100; // temperature * 100 (eg: 25.43 -> 2543)
    uint16_t humi_x100; // humidity * 100
} holding_reg_params_t;

holding_reg_params_t h_regs = {0}; // holding registers

const mb_register_area_descriptor_t reg_area = {
    .address = (void *)&h_regs,
    .type = MB_PARAM_HOLDING,
    .start_offset = 0,
    .size = sizeof(h_regs)
};

static const char *TAG = "Modbus TCP Slave Example";
static SemaphoreHandle_t init_done;

static void print_dyn_mac(void *pvParameters)
{
    esp_eth_handle_t port_eth_handle = (esp_eth_handle_t) pvParameters;
    ksz8863_dyn_mac_table_t dyn_mac_tbls[5];
    ksz8863_mac_tbl_info_t get_tbl_info = {
        .start_entry = 0,  // read from the first entry
        .etries_num = 5,   // read 5 entries
        .dyn_tbls = dyn_mac_tbls,
    };

    xSemaphoreGive(init_done);

    while (1) {
        esp_eth_ioctl(port_eth_handle, KSZ8863_ETH_CMD_G_MAC_DYN_TBL, &get_tbl_info);
        ESP_LOGI(TAG, "Dynamic MAC Table content:");
        ESP_LOGI(TAG, "valid entries %" PRIu16, dyn_mac_tbls[0].val_entries + 1);
        for (int i = 0; i < (dyn_mac_tbls[0].val_entries + 1) && i < 5; i++) {
            ESP_LOGI(TAG, "port %" PRIu8, dyn_mac_tbls[i].src_port + 1);
            ESP_LOG_BUFFER_HEX(TAG, dyn_mac_tbls[i].mac_addr, 6);
        }
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void transmit_l2test_msg(void *pvParameters)
{
    esp_vfs_l2tap_intf_register(NULL);
    int ret;
    int eth_tap_fd = -1;

    eth_tap_fd = open("/dev/net/tap", O_NONBLOCK);
    if (eth_tap_fd < 0) {
        ESP_LOGE(TAG, "Unable to open L2 TAP interface: errno %i", errno);
        goto err;
    }

    uint16_t eth_type_filter = 0x7000;
    // Set Ethernet interface on which to get raw frames
    if ((ret = ioctl(eth_tap_fd, L2TAP_S_INTF_DEVICE, "ETH_DEF")) == -1) {
        ESP_LOGE(TAG, "Unable to bound L2 TAP with Ethernet device: errno %i", errno);
        goto err;
    }

    if ((ret = ioctl(eth_tap_fd, L2TAP_S_RCV_FILTER, &eth_type_filter)) == -1) {
        ESP_LOGE(TAG, "Unable to configure L2 TAP Ethernet type receive filter: errno %i", errno);
        goto err;
    }

    esp_eth_handle_t host_eth_handle = esp_netif_get_io_driver(esp_netif_get_handle_from_ifkey("ETH_DEF"));

    test_vfs_eth_tap_msg_t test_msg = {
        .header = {
            .src.addr = {0},
            .dest.addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // broadcast
            .type = ntohs(eth_type_filter),
        },
        .str = "This is ESP32 L2 TAP test msg"
    };

    // Set source MAC address in test message
    if ((ret = esp_eth_ioctl(host_eth_handle, ETH_CMD_G_MAC_ADDR, test_msg.header.src.addr)) == -1) {
        ESP_LOGE(TAG, "get MAC addr error");
    }

    while (1) {
        ret = write(eth_tap_fd, &test_msg, sizeof(test_msg));
        if (ret == -1) {
            ESP_LOGE(TAG, "L2 TAP write error, errno: %i\n", errno);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
err:
    if (eth_tap_fd != -1) {
        close(eth_tap_fd);
    }
    vTaskDelete(NULL);
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    int32_t port_num;
    esp_err_t ret;
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    /* we should not try to use KSZ8863-specific ioctl commands with the general host handle, only with port handles */
    esp_eth_handle_t host_eth_handle = *(esp_eth_handle_t *)arg;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ret = eth_handle != host_eth_handle ? esp_eth_ioctl(eth_handle, KSZ8863_ETH_CMD_G_PORT_NUM, &port_num) : ESP_FAIL;
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Ethernet Link Up Port %" PRIi32, port_num + 1);
        } else {
            ESP_LOGI(TAG, "Ethernet Link Up");
        }
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ret = eth_handle != host_eth_handle ? esp_eth_ioctl(eth_handle, KSZ8863_ETH_CMD_G_PORT_NUM, &port_num) : ESP_FAIL;
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Ethernet Link Down Port %" PRIi32, port_num + 1);
        } else {
            ESP_LOGI(TAG, "Ethernet Link Down");
        }
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

#define MB_TCP_PORT_NUMBER      (CONFIG_FMB_TCP_PORT_DEFAULT)
#define MB_SLAVE_ADDR           (CONFIG_MB_SLAVE_ADDR)

// 2. Init of Modbus Slave & mDNS
static void start_modbus_service(esp_netif_t* my_eth_netif)
{
    mb_communication_info_t tcp_slave_config = {
        .tcp_opts.port = MB_TCP_PORT_NUMBER,
        .tcp_opts.mode = MB_TCP,
#if !CONFIG_EXAMPLE_CONNECT_IPV6
        .tcp_opts.addr_type = MB_IPV4,
#else
        .tcp_opts.addr_type = MB_IPV6,
#endif
        .tcp_opts.ip_addr_table = NULL, // Bind to any address
        .tcp_opts.ip_netif_ptr = my_eth_netif, // Must give the Ethernet netif pointer to ensure mDNS is properly bound to Ethernet interface
        .tcp_opts.uid = (uint8_t)MB_SLAVE_ADDR
    };

    ESP_ERROR_CHECK(mbc_slave_create_tcp(&tcp_slave_config, &mb_slave));
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(mb_slave, reg_area));
    ESP_ERROR_CHECK(mbc_slave_start(mb_slave));
    ESP_LOGI(TAG, "Modbus TCP Slave & mDNS Started.");
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "DHCP IP: " IPSTR, IP2STR(&event->ip_info.ip));

}

// board specific initialization routine, user to update per specific needs
esp_err_t ksz8863_board_specific_init(esp_eth_handle_t eth_handle)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_EXAMPLE_CTRL_I2C
    // initialize I2C interface
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = CONFIG_EXAMPLE_I2C_MASTER_PORT,
        .scl_io_num = CONFIG_EXAMPLE_I2C_SCL_GPIO,
        .sda_io_num = CONFIG_EXAMPLE_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&i2c_mst_config, &bus_handle), err, TAG, "I2C initialization failed");
    ksz8863_ctrl_i2c_config_t i2c_dev_config = {
        .bus_handle = bus_handle,
        .dev_addr = KSZ8863_I2C_DEV_ADDR,
        .i2c_port = CONFIG_EXAMPLE_I2C_MASTER_PORT,
        .scl_speed_hz = CONFIG_EXAMPLE_I2C_CLOCK_KHZ * 1000
    };
    ksz8863_ctrl_intf_config_t ctrl_intf_cfg = {
        .host_mode = KSZ8863_I2C_MODE,
        .i2c_dev_config = &i2c_dev_config,
    };
#elif CONFIG_EXAMPLE_CTRL_SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ksz8863_ctrl_spi_config_t spi_dev_config = {
        .host_id = CONFIG_EXAMPLE_ETH_SPI_HOST,
        .clock_speed_hz = CONFIG_EXAMPLE_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_ETH_SPI_CS_GPIO,
    };
    ksz8863_ctrl_intf_config_t ctrl_intf_cfg = {
        .host_mode = KSZ8863_SPI_MODE,
        .spi_dev_config = &spi_dev_config,
    };
#endif
    ESP_GOTO_ON_ERROR(ksz8863_ctrl_intf_init(&ctrl_intf_cfg), err, TAG, "KSZ8863 control interface initialization failed");

#ifdef CONFIG_EXAMPLE_EXTERNAL_CLK_EN
    // Enable KSZ's external CLK
    esp_rom_gpio_pad_select_gpio(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO);
    gpio_set_direction(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO, 1);
#endif

    ESP_GOTO_ON_ERROR(ksz8863_hw_reset(CONFIG_EXAMPLE_KSZ8863_RST_GPIO), err, TAG, "hardware reset failed");
    // it does not make much sense to execute SW reset right after HW reset but it is present here for demonstration purposes
    ESP_GOTO_ON_ERROR(ksz8863_sw_reset(eth_handle), err, TAG, "software reset failed");
#if CONFIG_EXAMPLE_P3_RMII_CLKI_INTERNAL
    ESP_GOTO_ON_ERROR(ksz8863_p3_rmii_internal_clk(eth_handle, true), err, TAG, "P3 internal clk config failed");
#endif

#if CONFIG_EXAMPLE_P3_RMII_CLKI_INVERT
    ESP_GOTO_ON_ERROR(ksz8863_p3_rmii_clk_invert(eth_handle, true), err, TAG, "P3 invert ckl failed");
#endif
err:
    return ret;
}

/* For SH4x */
#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static sht4x_t dev;


#if 1

void sht4x_task(void *pvParameters)
{
    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

    // get the measurement duration for high repeatability;
    // duration is nearly 3 ms, so connectivity between each node should be more than 10 ms to ensure that the measurement is ready when the results are retrieved
    uint8_t duration = sht4x_get_measurement_duration(&dev);

    while (1)
    {
        // print the measurement duration and trigger measurement
        // printf("duration: %d ms\n", duration);
        // Trigger one measurement in single shot mode with high repeatability.
        ESP_ERROR_CHECK(sht4x_start_measurement(&dev));

        // Wait until measurement is ready (duration returned from *sht4x_get_measurement_duration*).
        vTaskDelay(duration);

        // retrieve the values and do something with them
        ESP_ERROR_CHECK(sht4x_get_results(&dev, &temperature, &humidity));
        h_regs.temp_x100 = (uint16_t)(temperature * 100);
        h_regs.humi_x100 = (uint16_t)(humidity * 100);
        printf("sht4x Sensor: %.2f *C, %.2f %%\n", temperature, humidity);

        // wait until 1 second are over
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000));
    }
}

#else
void sht4x_task(void *pvParameters)
{
    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // perform one measurement and do something with the results
        ESP_ERROR_CHECK(sht4x_measure(&dev, &temperature, &humidity));

        printf("sht4x Sensor: %.2f *C, %.2f %%\n", temperature, humidity);

        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000));
    }
}

#endif

void app_main(void)
{
    ESP_LOGW(TAG, "Simple Switch Modbus SHT4x Example...\n");
    ESP_ERROR_CHECK(nvs_flash_init());
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    phy_config.reset_gpio_num = -1; // KSZ8863 is reset by separate function call since multiple instances exist
    // MIIM interface is not used since does not provide access to all registers
    esp32_emac_config.smi_gpio.mdc_num = -1;
    esp32_emac_config.smi_gpio.mdio_num = -1;

    // Init Host Ethernet Interface (Port 3)
    esp_eth_mac_t *host_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    phy_config.phy_addr = -1; // this PHY is entry point to host
    esp_eth_phy_t *host_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t host_config = ETH_KSZ8863_DEFAULT_CONFIG(host_mac, host_phy);
    host_config.on_lowlevel_init_done = ksz8863_board_specific_init;
    esp_eth_handle_t host_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&host_config, &host_eth_handle));

    // Create new default instance of esp-netif for Host Ethernet Port (P3)
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(host_eth_handle)));

    // p1/2_eth_handle are going to be used basically only for Link Status indication and for configuration access
    // Init P1 Ethernet Interface
    ksz8863_eth_mac_config_t ksz8863_pmac_config = {
        .pmac_mode = KSZ8863_SWITCH_MODE,
        .port_num = KSZ8863_PORT_1,
    };
    esp_eth_mac_t *p1_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_1;
    esp_eth_phy_t *p1_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p1_config = ETH_KSZ8863_DEFAULT_CONFIG(p1_mac, p1_phy);
    esp_eth_handle_t p1_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&p1_config, &p1_eth_handle));

    // Init P2 Ethernet Interface
    ksz8863_pmac_config.port_num = KSZ8863_PORT_2;
    esp_eth_mac_t *p2_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_2;
    esp_eth_phy_t *p2_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p2_config = ETH_KSZ8863_DEFAULT_CONFIG(p2_mac, p2_phy);
    esp_eth_handle_t p2_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&p2_config, &p2_eth_handle));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, &host_eth_handle));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // start Ethernet driver state machines
    ESP_ERROR_CHECK(esp_eth_start(host_eth_handle));
    ESP_ERROR_CHECK(esp_eth_start(p1_eth_handle));
    ESP_ERROR_CHECK(esp_eth_start(p2_eth_handle));

    // Sync semaphore is needed since main task local variables are used during initialization in other tasks
    init_done = xSemaphoreCreateBinary();
    assert(init_done);

    // Periodically print content of Dynamic MAC table
    xTaskCreate(print_dyn_mac, "print_dyn_mac", 4096, p1_eth_handle, 5, NULL);
    xSemaphoreTake(init_done, portMAX_DELAY);

    vSemaphoreDelete(init_done);

    /* For SHT4x */
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&dev, 0, sizeof(sht4x_t));

    // Write down the proper GPIO numbers for SDA and SCL, and the I2C port number used in your project.
    ESP_ERROR_CHECK(sht4x_init_desc(&dev, 1, 16, 17));
    ESP_ERROR_CHECK(sht4x_init(&dev));
    // Assigned CPU1 for SHT4x task since it is not time critical and to avoid interference with Ethernet and Modbus tasks running on CPU0
    xTaskCreatePinnedToCore(sht4x_task, "sht4x_test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);

    // Everything is ready, start Modbus TCP Slave and mDNS services
    start_modbus_service(eth_netif);

    // I didn't modify it, as you can use it a kind of heartbeat to check if the main task is still running. You can also use it to transmit some test messages over L2 TAP interface if you want.
    xTaskCreate(transmit_l2test_msg, "tx_test_msg", 4096, NULL, 4, NULL);
}
