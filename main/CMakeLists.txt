idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_netif esp_eth ksz8863 esp-modbus mdns sht4x)
