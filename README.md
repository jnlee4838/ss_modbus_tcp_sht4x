
# ESP32 + KSZ8863RLL + SH4x Modbus TCP Slave Demo

It's a simple demonstation of ESP32 + KSZ8863 + Modbus TCP Slave (TCP Server) + SHT40. it reads and updates the temperature and humidity from Sensirion's SHT40 via I2C every one(1) sec. The reason I adopted the Sensirion's SHT40 why it's pretty accurate and relatively affordable. if you need more accuracy and precise tolerance, consider SHT45.
You could make an scalable Ethernet daisy chain network which supports Modbus TCP at faster speed. by the way, this example is for Modbus Slave only. in case that you are using Ethernet KSZ8863 switch, you'd better off Wi-Fi for better stability.

The RST-ESP32-KSZ8863 Development Board has got the unused I/O ports in right side. GPIO #16 for SDA, #17 for SCL of I2C1.
By the way, before we jump in, please read the previous repos here[https://github.com/jnlee4838/esp32-ksz8863-dual-Ethernet] & here[https://github.com/jnlee4838/iperf-esp32-ksz8863] for better understanding.

## Screenshots

![ESP32 KSZ8863 Daisy Chain Network](images/esp32_ksz8863_network_diagram.png)

![ESP32 KSZ8863 SHT4x I2C Ports](images/ESP32_KSZ8863_SHT4x_Wiring.jpg)

![ESP32 KSZ8863 SHT4x Assembled](images/ESP32_KSZ8863_SHT4x_Demo.jpg)

![SHT40 Module](images/SHT4x_Module.jpg)

## Buy

* For an inquiry, [*email me*](jnlee4838@gmail.com)

### Korea

* Smartstore: [**Buy Here**](https://smartstore.naver.com/radiosystek)

### International

* BuyKorea: [**Buy Here**](https://buykorea.org/ec/prd/selectGoodsDetail.do?goodsSn=3771535)

## Preparation

We are going to use ESP-IDF version 5.4.2 only in this project, you should install and set up your own IDE, toolchain, etc. and the demo environment is Windows 11 & VS Code.

* Prepare the esp32 ksz8863 dual Ethernet board, Sensirion SH450 module, 4pcs of 2.54mm pitch female to female cables, 1 usb B to usb A cable, 2 utp cables, one router, one PC, Modbus Poll Tool, WireShark Tool

* install VS CODE

* Setup ESP-IDF

## Set-up Flow

* Assuming that you have already compiled and flashed from [**the previous repo**](https://github.com/jnlee4838/esp32-ksz8863-dual-Ethernet).

* download and unzip this repo somewehere you prefer.

* the folder ".vscode" is depending on your ESP-IDF installation environment. if you don't want to face a lot of errors or warnings. please modify the files at your end first or you can just move it out from the project first. then, after build, paste and modify them at your end.

* execute VS Code.

* goto "File" > "Open folder..." and select the above folder.

* make sure everything is ok like the following image. and also check what is modified and added from the previous repo.

![esp32 ksz8863 simple switch sht4x project open](images/ESP32_KSZ8863_SHT4x_Step_01.jpg)

### Next Steps

![esp32 ksz8863 simple switch sht4x target config](images/ESP32_KSZ8863_SHT4x_Step_02.jpg)

1. please click from #1 step by step.

2. Set your project path properly.

3. Set your ESP-IDF path properly.

4. Set the FW upload interface UART.

5. Set the FW upload port COMx depending on your pc.

6. Set the target device "esp32" and "ESP32 chip (via ESP Prog)". it takes time. be patient. and please check the "OUTPUT" window in bottom side of your VS Code that will tell whether your config is correct or not. hopefully no problems at all!

7. "SDK Configuration Editor (menuconfig)" in your VS Code. then it takes time and you can see new folders "build" and "managed_compoennts" in the "EXPLORER" window at left side.

8. we will check and modify several parameters in menuconfig.

![esp32 ksz8863 simple switch sht4x sdkconfig](images/ESP32_KSZ8863_SHT4x_Step_07.jpg)

* --> Type "cpu" in the top search box
   Component config > ESP System Settings > CPU frequency -> "240 MHz"
   Modbus configuration > Modbus task affinity -> "CPU1"
   mDNS > mDNS task affinity -> "CPU1"

* --> Type "flash" in the top search box
   Serial flasher config > Flash size -> "4MB"

* --> Click "Example Configuration"
   Example Configuration > Enable external RMII clock oscillator unchecked

* --> Click Component config > "Ethernet"
   Do not modify. make sure "Support ESP32 internal EMAC controller" checked
   make sure Ethernet > PHY interface -> "RMII"
   Ethernet > RMII clock mode -> "input.....from external"

* --> Click Component config
   ESP System Settings > Trace memory > Task Watchdog timeout period (seconds) -> "10"

* --> Click Component config > I2C Device Library (we change it in source code manually, for your info)
   I2C Device Library > Default I2C SDA pin -> "16"
   I2C Device Library > Default I2C SCL pin -> "17"

* --> Click Component config > Modbus configuration
   Modbus configuration > Modbus TCP port number -> "502"

* Done !!! Click "Save".

* Click "build" and try to enjoy a cup of coffee. it takes time depending on your computer performance. anyway, you'd better take a break.

* congrats if you can see a "Memory Type Usage Summary" in "Terminal" window. otherwise, you should fix the errors and warnings...

![esp32 ksz8863 simple switch sht4x build complete](images/ESP32_KSZ8863_SHT4x_Build_Done.jpg)

* neally done !

* click "Flash Device" icon.

* click "Monitor Device" and see what is going on in that "Terminal".

## Console output

the console should be the followings if you have the following connection diagram

* the router should have DHCP server on.

```bash

    ets Jul 29 2019 12:21:46

    rst:0x1 (POWERON_RESET),boot:0x12 (SPI_FAST_FLASH_BOOT)
    configsip: 0, SPIWP:0xee
    clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
    mode:DIO, clock div:2
    load:0x3fff0030,len:6248
    load:0x40078000,len:15736
    load:0x40080400,len:4
    --- 0x40080400: _init at ??:?
    load:0x40080404,len:3836
    entry 0x40080638
    I (29) boot: ESP-IDF v5.4.2 2nd stage bootloader
    I (29) boot: Multicore bootloader
    I (30) boot: chip revision: v3.1
    I (30) boot.esp32: SPI Speed      : 40MHz
    I (33) boot.esp32: SPI Mode       : DIO
    I (37) boot.esp32: SPI Flash Size : 4MB
    I (40) boot: Enabling RNG early entropy source...
    I (45) boot: Partition Table:
    I (47) boot: ## Label            Usage          Type ST Offset   Length
    I (54) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (60) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (67) boot:  2 factory          factory app      00 00 00010000 00100000
    I (73) boot: End of partition table
    I (76) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=1e258h (123480) map
    I (126) esp_image: segment 1: paddr=0002e280 vaddr=3ff80000 size=0001ch (    28) load
    I (126) esp_image: segment 2: paddr=0002e2a4 vaddr=3ffb0000 size=01d74h (  7540) load
    I (133) esp_image: segment 3: paddr=00030020 vaddr=400d0020 size=54c04h (347140) map
    I (256) esp_image: segment 4: paddr=00084c2c vaddr=3ffb1d74 size=00bc8h (  3016) load
    I (258) esp_image: segment 5: paddr=000857fc vaddr=40080000 size=1119ch ( 70044) load
    I (295) boot: Loaded app from partition at offset 0x10000
    I (295) boot: Disabling RNG early entropy source...
    I (306) cpu_start: Multicore app
    I (314) cpu_start: Pro cpu start user code
    I (314) cpu_start: cpu freq: 240000000 Hz
    I (314) app_init: Application information:
    I (315) app_init: Project name:     ss_modbus_sht4x
    I (319) app_init: App version:      1
    I (323) app_init: ELF file SHA256:  a1b8d15fd...
    I (327) app_init: ESP-IDF:          v5.4.2
    I (331) efuse_init: Min chip rev:     v0.0
    I (334) efuse_init: Max chip rev:     v3.99
    I (338) efuse_init: Chip rev:         v3.1
    I (343) heap_init: Initializing. RAM available for dynamic allocation:
    I (349) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
    I (354) heap_init: At 3FFB4CB8 len 0002B348 (172 KiB): DRAM
    I (359) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
    I (364) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
    I (370) heap_init: At 4009119C len 0000EE64 (59 KiB): IRAM
    I (377) spi_flash: detected chip: generic
    I (379) spi_flash: flash io: dio
    I (383) main_task: Started on CPU0
    I (393) main_task: Calling app_main()
    W (393) Modbus TCP Slave Example: Simple Switch Modbus SHT4x Example...

    W (403) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    W (413) ksz8863_eth: SW reset resets all Global, MAC and PHY registers!
    I (413) esp_eth.netif.netif_glue: 84:1f:e8:46:b9:b7
    I (423) esp_eth.netif.netif_glue: ethernet attached to netif
    I (443) Modbus TCP Slave Example: Ethernet Started
    I (443) Modbus TCP Slave Example: Ethernet Link Up
    I (443) Modbus TCP Slave Example: Ethernet HW Addr 00:00:00:00:00:00
    I (2443) Modbus TCP Slave Example: Ethernet Started
    I (2443) Modbus TCP Slave Example: Ethernet Link Up Port 1
    I (2443) Modbus TCP Slave Example: Ethernet HW Addr 00:00:00:00:00:00
    I (5023) esp_netif_handlers: eth ip: 192.168.0.3, mask: 255.255.255.0, gw: 192.168.0.1
    I (5023) Modbus TCP Slave Example: Ethernet Got IP Address
    I (5023) Modbus TCP Slave Example: ~~~~~~~~~~~
    I (5023) Modbus TCP Slave Example: ETHIP:192.168.0.3
    I (5033) Modbus TCP Slave Example: ETHMASK:255.255.255.0
    I (5033) Modbus TCP Slave Example: ETHGW:192.168.0.1
    I (5043) Modbus TCP Slave Example: ~~~~~~~~~~~
    I (5043) Modbus TCP Slave Example: DHCP IP: 192.168.0.3
    I (6453) Modbus TCP Slave Example: Ethernet Started
    I (6453) i2cdev: [Port 1] First initialization. Configuring bus with SDA=16, SCL=17 (Pullups SCL:0 SDA:0)
    W (6453) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    I (6453) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (6473) i2cdev: [Port 1] Successfully installed I2C master bus (Handle: 0x3ffc01cc).
    I (6473) Modbus TCP Slave Example: valid entries 2
    I (6483) Modbus TCP Slave Example: port 3
    I (6483) i2cdev: [0x44 at 1] Device added successfully (Device Handle: 0x3ffc06bc, Speed: 1000000 Hz).
    I (6493) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (6503) Modbus TCP Slave Example: port 1
    I (6503) Modbus TCP Slave Example: 00:00:00:00:00:00

    sht4x Sensor: 23.72 *C, 19.80 %
    I (6753) mdns_mem: mDNS task will be created from internal RAM
    I (6753) port.utils: hostname set to: [mb_slave_tcp_46b9b7]
    I (6753) port.utils: mdns delegate hostname set to: [mb_slave_tcp_01]
    I (6763) port.utils: IP: 192.168.0.3
    I (6763) port.utils: GW: 192.168.0.1
    I (6763) port.utils: NETMASK: 255.255.255.0
    I (6773) Modbus TCP Slave Example: Modbus TCP Slave & mDNS Started.
    I (6773) port.utils: Socket (#54), listener  on port: 502, errno=0
    I (6773) main_task: Returned from app_main()
    I (6783) mb_port.tcp.slave: loop:0x3ffc52c4  mbs_on_ready: fd: -1, bind is done
    sht4x Sensor: 23.73 *C, 19.81 %
    sht4x Sensor: 23.74 *C, 19.85 %
    sht4x Sensor: 23.75 *C, 20.15 %
    sht4x Sensor: 23.74 *C, 20.19 %
    I (11513) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (11513) Modbus TCP Slave Example: valid entries 3
    I (11513) Modbus TCP Slave Example: port 3
    I (11513) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (11523) Modbus TCP Slave Example: port 1
    I (11523) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (11523) Modbus TCP Slave Example: port 1
    I (11533) Modbus TCP Slave Example: 00:00:00:00:00:00

    sht4x Sensor: 23.73 *C, 20.17 %
    sht4x Sensor: 23.75 *C, 20.13 %
    sht4x Sensor: 23.74 *C, 20.04 %
    sht4x Sensor: 23.72 *C, 19.96 %
    sht4x Sensor: 23.73 *C, 19.86 %
    I (16533) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (16533) Modbus TCP Slave Example: valid entries 3
    I (16533) Modbus TCP Slave Example: port 3
    I (16533) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (16543) Modbus TCP Slave Example: port 1
    I (16543) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (16543) Modbus TCP Slave Example: port 1
    I (16553) Modbus TCP Slave Example: 00:00:00:00:00:00

    sht4x Sensor: 23.74 *C, 19.81 %
    sht4x Sensor: 23.74 *C, 19.76 %
    sht4x Sensor: 23.74 *C, 19.67 %
    sht4x Sensor: 23.73 *C, 19.69 %
    sht4x Sensor: 23.74 *C, 19.67 %
    I (21553) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (21553) Modbus TCP Slave Example: valid entries 3
    I (21553) Modbus TCP Slave Example: port 3
    I (21553) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (21563) Modbus TCP Slave Example: port 1
    I (21563) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (21563) Modbus TCP Slave Example: port 1
    sht4x Sensor: 23.72 *C, 19.62 %
    I (21573) Modbus TCP Slave Example: 00:00:00:00:00:00

    sht4x Sensor: 23.73 *C, 19.59 %
    sht4x Sensor: 23.74 *C, 19.63 %
    sht4x Sensor: 23.73 *C, 19.64 %
    sht4x Sensor: 23.71 *C, 19.63 %
    sht4x Sensor: 23.72 *C, 19.64 %
    I (26583) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (26583) Modbus TCP Slave Example: valid entries 3
    I (26583) Modbus TCP Slave Example: port 3
    I (26583) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (26593) Modbus TCP Slave Example: port 1
    I (26593) Modbus TCP Slave Example: 00:00:00:00:00:00
    I (26593) Modbus TCP Slave Example: port 1
    I (26603) Modbus TCP Slave Example: 00:00:00:00:00:00

```

## Test

### IP Assignment

* Please check wheater your pc has a proper IP from from your router in PowerShell or CMD Terminal.

* You can check all device status from your router's networking status as well

```bash

    ipconfig /all

```

### Ping test

* if your laptops are Windows, please make them allow ICMP input in Windows Defender first.

* Try to ping forward and backward.

### ESP-IDF MONITOR Terminal

* The following data should be out every one(1) sec. and also Modbus TCP Slave updates these two values in holding regs every one(1) sec.

```bash

    sht4x Sensor: 23.53 *C, 19.56 %
    sht4x Sensor: 23.52 *C, 19.51 %
    sht4x Sensor: 23.53 *C, 19.52 %
    sht4x Sensor: 23.52 *C, 19.52 %
    sht4x Sensor: 23.54 *C, 19.54 %

```

* The following data should be out every five(5) sec.

```bash

    I (714943) Modbus TCP Slave Example: Dynamic MAC Table content:
    I (714943) Modbus TCP Slave Example: valid entries 4
    I (714943) Modbus TCP Slave Example: port 1
    I (714943) Modbus TCP Slave Example: XX XX XX XX XX XX
    I (714953) Modbus TCP Slave Example: port 3
    I (714953) Modbus TCP Slave Example: XX XX XX XX XX XX
    I (714953) Modbus TCP Slave Example: port 1
    I (714963) Modbus TCP Slave Example: XX XX XX XX XX XX
    I (714963) Modbus TCP Slave Example: port 1
    I (714973) Modbus TCP Slave Example: XX XX XX XX XX XX

```

### Wire Shark & Modbus Poll

Another L2 TAP braodcasts 0x7000 Ethernet frame every two seconds.

* please execute "WireShark" and "Modbus Poll" to check out whether it works properly or not.

* select the "Ethernet" to analyze.

* you can find the "ARP" ethernet frames that they are talking each others.

![esp32 ksz8863 simple switch sht4x Modbus Poll captured](images/ESP32_KSZ8863_SHT4x_Modbus_TCP_Test.jpg)

![esp32 ksz8863 simple switch sht4x WireShark captured](images/ESP32_KSZ8863_SHT4x_Modbus_TCP_Test_WireShark_Capture.jpg)

## Related

Here is another project to perform the Throughput of esp32 ksz8863 dual Ethernet by Iperf2 as well as Iperf3.

[Iperf esp32 ksz8863 dual Ethernet README](https://github.com/jnlee4838/Iperf-esp32-ksz8863/readme)
