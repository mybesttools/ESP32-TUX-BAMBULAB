ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:6992
load:0x40078000,len:15452
load:0x40080400,len:3840
entry 0x4008064c
[0;32mI (26) boot: ESP-IDF v5.0 2nd stage bootloader[0m
[0;32mI (26) boot: compile time 12:05:33[0m
[0;32mI (27) boot: chip revision: v1.0[0m
[0;32mI (29) boot_comm: chip revision: 1, min. bootloader chip revision: 0[0m
[0;32mI (36) boot.esp32: SPI Speed      : 40MHz[0m
[0;32mI (41) boot.esp32: SPI Mode       : DIO[0m
[0;32mI (45) boot.esp32: SPI Flash Size : 4MB[0m
[0;32mI (50) boot: Enabling RNG early entropy source...[0m
[0;32mI (55) boot: Partition Table:[0m
[0;32mI (59) boot: ## Label            Usage          Type ST Offset   Length[0m
[0;32mI (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000[0m
[0;32mI (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000[0m
[0;32mI (81) boot:  2 factory          factory app      00 00 00010000 00250000[0m
[0;32mI (89) boot:  3 storage          Unknown data     01 82 00260000 00180000[0m
[0;32mI (96) boot: End of partition table[0m
[0;32mI (100) boot_comm: chip revision: 1, min. application chip revision: 0[0m
[0;32mI (107) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=6f68ch (456332) map[0m
[0;32mI (281) esp_image: segment 1: paddr=0007f6b4 vaddr=3ffb0000 size=00964h (  2404) load[0m
[0;32mI (282) esp_image: segment 2: paddr=00080020 vaddr=400d0020 size=14d15ch (1364316) map[0m
[0;32mI (780) esp_image: segment 3: paddr=001cd184 vaddr=3ffb0964 size=0476ch ( 18284) load[0m
[0;32mI (787) esp_image: segment 4: paddr=001d18f8 vaddr=40080000 size=1d33ch (119612) load[0m
[0;32mI (836) esp_image: segment 5: paddr=001eec3c vaddr=50000000 size=00010h (    16) load[0m
[0;32mI (851) boot: Loaded app from partition at offset 0x10000[0m
[0;32mI (851) boot: Disabling RNG early entropy source...[0m
[0;32mI (863) quad_psram: This chip is ESP32-D0WD[0m
[0;32mI (866) esp_psram: Found 8MB PSRAM device[0m
[0;32mI (866) esp_psram: Speed: 40MHz[0m
[0;32mI (867) esp_psram: PSRAM initialized, cache is in low/high (2-core) mode.[0m
[0;33mW (875) esp_psram: Virtual address not enough for PSRAM, map as much as we can. 4MB is mapped[0m
[0;32mI (884) cpu_start: Pro cpu up.[0m
[0;32mI (888) cpu_start: Starting app cpu, entry point is 0x40081788[0m
[0;32mI (0) cpu_start: App cpu up.[0m
[0;32mI (1797) esp_psram: SPI SRAM memory test OK[0m
[0;32mI (1805) cpu_start: Pro cpu start user code[0m
[0;32mI (1805) cpu_start: cpu freq: 160000000 Hz[0m
[0;32mI (1805) cpu_start: Application information:[0m
[0;32mI (1808) cpu_start: Project name:     ESP32-TUX[0m
[0;32mI (1814) cpu_start: App version:      0.11.0[0m
[0;32mI (1819) cpu_start: Compile time:     Dec  4 2025 14:31:46[0m
[0;32mI (1825) cpu_start: ELF file SHA256:  5f2583cb29f00a57...[0m
[0;32mI (1831) cpu_start: ESP-IDF:          v5.0[0m
[0;32mI (1836) heap_init: Initializing. RAM available for dynamic allocation:[0m
[0;32mI (1843) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM[0m
[0;32mI (1849) heap_init: At 3FFC74A0 len 00018B60 (98 KiB): DRAM[0m
[0;32mI (1855) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM[0m
[0;32mI (1862) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM[0m
[0;32mI (1868) heap_init: At 4009D33C len 00002CC4 (11 KiB): IRAM[0m
[0;32mI (1875) esp_psram: Adding pool of 4096K of PSRAM memory to heap allocator[0m
[0;32mI (1883) spi_flash: detected chip: generic[0m
[0;32mI (1887) spi_flash: flash io: dio[0m
[0;32mI (1896) cpu_start: Starting scheduler on PRO CPU.[0m
[0;32mI (0) cpu_start: Starting scheduler on APP CPU.[0m
[0;32mI (1902) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations[0m
[0;31mE (1912) ESP32-TUX: 
Firmware Ver : 0.11.0
Project Name : ESP32-TUX
IDF Version  : v5.0

Controller   : esp32 Rev.100
CPU Cores    : Dual Core
CPU Speed    : 160Mhz
Flash Size   : 4MB [external]
PSRAM Size   : 4MB [external]
Connectivity : 2.4GHz WIFI/BT/BLE
[0m
[0;32mI (1962) ESP32-TUX: Initializing SPIFFS[0m
[0;32mI (2142) ESP32-TUX: Partition size: total: 1438481, used: 298188[0m
[0;32mI (2172) SettingsConfig: Loaded timezone: UTC0[0m
[0;32mI (2172) SettingsConfig: Loaded network: Local Network (10.13.13.0/24)[0m
[0;32mI (2172) SettingsConfig: Loaded weather location: Home (Kleve, Germany)[0m
[0;32mI (2182) SettingsConfig: Loaded weather location: Amsterdam (Amsterdam, Netherlands)[0m
[0;32mI (2192) ESP32-TUX: Loaded existing configuration[0m
[0;32mI (2202) BambuHelper: Initializing Bambu Monitor helper[0m
[0;32mI (2202) BambuMonitor: Initializing Bambu Monitor for device: bambu_lab_printer[0m
[0;31mE (2212) mqtt_client: No scheme found[0m
[0;31mE (2212) mqtt_client: Failed to create transport list[0m
[0;31mE (2222) BambuMonitor: Failed to start MQTT client: ESP_FAIL[0m
[0;31mE (2232) BambuHelper: Failed to initialize Bambu Monitor: ESP_FAIL[0m
[0;33mW (2232) ESP32-TUX: Bambu Monitor initialization optional - continuing without it[0m
[0;32mI (2712) ESP32-TUX: Start to run LVGL[0m
[0;31mE (2712) ESP32-TUX: F Drive is ready[0m
[0;32mI (2772) ESP32-TUX: Read from F:/readme.txt : ESP32-TUX - A Touch UX Template@X�@[0m
[0;31mE (2772) ESP32-TUX: S Drive is ready[0m
[0;31mE (2772) ESP32-TUX: Failed to open S:/readme.txt, 12[0m
[0;33mW (3022) ESP32-TUX: Using Gradient[0m
[0;33mW (3192) ESP32-TUX: Slideshow mode enabled - auto-cycling every 8000 ms[0m
[0;32mI (3202) WebServer: HTTP server started on http://esp32-tux.local[0m
[0;32mI (3202) ESP32-TUX: Web server started successfully[0m
[0;32mI (3202) ESP32-TUX: [APP] Free memory: 4265140 bytes[0m
[0;32mI (3212) system_api: Base MAC address is not set[0m
[0;32mI (3212) system_api: read default base MAC address from EFUSE[0m
[0;32mI (3242) wifi_init: rx ba win: 16[0m
[0;32mI (3242) ESP32-TUX: Bambu Monitor task started[0m
[0;32mI (3242) wifi_init: tcpip mbox: 32[0m
[0;32mI (3242) wifi_init: udp mbox: 6[0m
[0;32mI (3252) wifi_init: tcp mbox: 6[0m
[0;32mI (3252) wifi_init: tcp tx win: 5744[0m
[0;32mI (3262) wifi_init: tcp rx win: 5744[0m
[0;32mI (3262) wifi_init: tcp mss: 1440[0m
[0;32mI (3262) wifi_init: WiFi/LWIP prefer SPIRAM[0m
[0;32mI (3272) wifi_init: WiFi IRAM OP enabled[0m
[0;32mI (3272) wifi_init: WiFi RX IRAM OP enabled[0m
[0;32mI (3282) ESP32-TUX: Already provisioned, starting Wi-Fi STA[0m
[0;32mI (3292) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07[0m
[0;33mW (3542) ESP32-TUX: WIFI_EVENT_STA_CONNECTED[0m
[0;33mW (3542) ESP32-TUX: [51] MSG_WIFI_CONNECTED[0m
W (3562) wifi:<ba-add>idx:0 (ifx:0, 74:83:c2:3d:2e:c3), tid:0, ssn:1, winSize:64
[0;33mW (4192) ESP32-TUX: IP_EVENT_STA_GOT_IP[0m
[0;32mI (4192) ESP32-TUX: Web UI available at: http://10.13.13.188[0m
[0;32mI (4192) ESP32-TUX: Connected with IP Address:10.13.13.188[0m
[0;32mI (4202) ESP32-TUX: Connected with IP Address:10.13.13.188[0m
[0;32mI (4202) esp_netif_handlers: sta ip: 10.13.13.188, mask: 255.255.255.0, gw: 10.13.13.1[0m
[0;32mI (4202) ESP32-TUX: Time is not set yet. Connecting and getting time over NTP.[0m
[0;32mI (4212) ESP32-TUX: Wifi Connected - Self-destruct Task :)[0m
[0;32mI (4222) ESP32-TUX: Initializing SNTP[0m
[0;32mI (4232) ESP32-TUX: Waiting for system time to be set... (1/10)[0m
[0;32mI (6072) ESP32-TUX: Notification of a time synchronization event[0m
[0;32mI (6102) OpenWeatherMap: HTTP request to get weather[0m
[0;31mE (6102) OpenWeatherMap: URL: http://api.openweathermap.org/data/2.5/weather?q=Kleve,Germany&units=metric&APPID=ed96c8275cbc2e85c4b3d7180d245bec[0m
[0;32mI (6142) OpenWeatherMap: HTTP_EVENT_ON_CONNECTED[0m
[0;32mI (6142) OpenWeatherMap: HTTP_EVENT_HEADER_SENT[0m
[0;32mI (6162) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6162) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6172) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6172) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6172) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6182) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6182) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6192) OpenWeatherMap: HTTP_EVENT_ON_HEADER[0m
[0;32mI (6192) OpenWeatherMap: HTTP_EVENT_ON_DATA, len=151[0m
[0;32mI (6202) OpenWeatherMap: HTTP_EVENT_ON_DATA, len=359[0m
[0;32mI (6212) OpenWeatherMap: HTTP_EVENT_ON_FINISH[0m
[0;32mI (6212) OpenWeatherMap: HTTP_EVENT_ON_FINISH, Total len=510[0m
[0;32mI (6222) OpenWeatherMap: Status = 200, content_length = 510[0m
[0;32mI (6222) OpenWeatherMap: HTTP_EVENT_DISCONNECTED[0m
[0;32mI (6232) OpenWeatherMap: Updating and writing into cache - weather.json[0m
[0;32mI (6292) OpenWeatherMap: Reading - weather.json[0m
[0;32mI (6292) OpenWeatherMap: Loading - weather.json[0m
[0;33mW (6292) OpenWeatherMap: load_json() 
{"coord":{"lon":6.15,"lat":51.7833},"weather":[{"id":803,"main":"Clouds","description":"broken clouds","icon":"04d"}],"base":"stations","main":{"temp":4.69,"feels_like":2.29,"temp_min":3.93,"temp_max":5.72,"pressure":1010,"humidity":92,"sea_level":1010,"grnd_level":1008},"visibility":10000,"wind":{"speed":2.79,"deg":221,"gust":5.1},"clouds":{"all":75},"dt":1764923744,"sys":{"type":2,"id":2007618,"country":"DE","sunrise":1764919538,"sunset":1764948408},"timezone":3600,"id":2887835,"name":"Kleve","cod":200}[0m
[0;33mW (6342) OpenWeatherMap: root: Kleve / 10000[0m
[0;33mW (6342) OpenWeatherMap: main: 4.7°С / 2.3°С / 3.9°С / 5.7°С / 1010 / 92hpa[0m
[0;33mW (6352) OpenWeatherMap: weather: Clouds / broken clouds / 04d[0m
[0;33mW (6362) OpenWeatherMap: coord: 6.150000 / 51.783300 [0m
[0;33mW (6362) OpenWeatherMap: wind: 2.8 m/s / 221 [0m
[0;32mI (7242) ESP32-TUX: Got time - Self-destruct Task :)[0m
[0;32mI (14592) WebServer: [Discovery Task] Starting printer discovery...[0m
[0;32mI (14592) WebServer: [Discovery Start] Total: 4151108, Internal: 28907, SPIRAM: 4131776[0m
[0;32mI (14592) PrinterDiscovery: === Starting Bambu Lab printer discovery ===[0m
[0;32mI (14602) PrinterDiscovery: Progress callback registered[0m
[0;32mI (14612) PrinterDiscovery: SettingsConfig available. Network count: 1[0m
[0;32mI (14612) PrinterDiscovery: Scanning 1 configured network(s)...[0m
[0;32mI (14622) PrinterDiscovery: [Network 1/1] Local Network (10.13.13.0/24) enabled=1[0m
[0;32mI (14632) PrinterDiscovery:   → Starting IP scan...[0m
[0;32mI (14632) PrinterDiscovery: === Starting IP scan for subnet: 10.13.13.0/24 ===[0m
[0;32mI (14642) PrinterDiscovery: Generated 254 IPs from subnet 10.13.13.0/24[0m
[0;32mI (14652) PrinterDiscovery: Scanning 254 IPs from subnet 10.13.13.0/24 using batch processing[0m
[0;32mI (14662) PrinterDiscovery: Batch 0-1: Creating 2 scan tasks...[0m
[0;32mI (14662) PrinterDiscovery: Batch created: 2/2 tasks started[0m
[0;31mE (14682) FreeRTOS: FreeRTOS Task "scan_ip" should not return, Aborting now![0m

abort() was called at PC 0x400933e7 on core 1


Backtrace: 0x4008246d:0x3fffc700 0x4008feb1:0x3fffc720 0x400979d9:0x3fffc740 0x400933e7:0x3fffc7b0




ELF file SHA256: 5f2583cb29f00a57

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:6992
load:0x40078000,len:15452
load:0x40080400,len:3840
entry 0x4008064c
[0;32mI (27) boot: ESP-IDF v5.0 2nd stage bootloader[0m
[0;32mI (27) boot: compile time 12:05:33[0m
[0;32mI (27) boot: chip revision: v1.0[0m
[0;32mI (30) boot_comm: chip revision: 1, min. bootloader chip revision: 0[0m
[0;32mI (37) boot.esp32: SPI Speed      : 40MHz[0m
[0;32mI (42) boot.esp32: SPI Mode       : DIO[0m
[0;32mI (46) boot.esp32: SPI Flash Size : 4MB[0m
[0;32mI (51) boot: Enabling RNG early entropy source...[0m
[0;32mI (56) boot: Partition Table:[0m
[0;32mI (60) boot: ## Label            Usage          Type ST Offset   Length[0m
[0;32mI (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000[0m
[0;32mI (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000[0m
[0;32mI (82) boot:  2 factory          factory app      00 00 00010000 00250000[0m
[0;32mI (89) boot:  3 storage          Unknown data     01 82 00260000 00180000[0m
[0;32mI (97) boot: End of partition table[0m
[0;32mI (101) boot_comm: chip revision: 1, min. application chip revision: 0[0m
[0;32mI (108) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=6f68ch (456332) map[0m
[0;32mI (282) esp_image: segment 1: paddr=0007f6b4 vaddr=3ffb0000 size=00964h (  2404) load[0m
[0;32mI (283) esp_image: segment 2: paddr=00080020 vaddr=400d0020 size=14d15ch (1364316) map[0m
[0;32mI (780) esp_image: segment 3: paddr=001cd184 vaddr=3ffb0964 size=0476ch ( 18284) load[0m
[0;32mI (788) esp_image: segment 4: paddr=001d18f8 vaddr=40080000 size=1d33ch (119612) load[0m
[0;32mI (837) esp_image: segment 5: paddr=001eec3c vaddr=50000000 size=00010h (    16) load[0m
[0;32mI (852) boot: Loaded app from partition at offset 0x10000[0m
[0;32mI (852) boot: Disabling RNG early entropy source...[0m
[0;32mI (864) quad_psram: This chip is ESP32-D0WD[0m
[0;32mI (866) esp_psram: Found 8MB PSRAM device[0m
[0;32mI (866) esp_psram: Speed: 40MHz[0m
[0;32mI (868) esp_psram: PSRAM initialized, cache is in low/high (2-core) mode.[0m
[0;33mW (876) esp_psram: Virtual address not enough for PSRAM, map as much as we can. 4MB is mapped[0m
[0;32mI (885) cpu_start: Pro cpu up.[0m
[0;32mI (889) cpu_start: Starting app cpu, entry point is 0x40081788[0m
[0;32mI (882) cpu_start: App cpu up.[0m
[0;32mI (1798) esp_psram: SPI SRAM memory test OK[0m
[0;32mI (1806) cpu_start: Pro cpu start user code[0m
[0;32mI (1806) cpu_start: cpu freq: 160000000 Hz[0m
[0;32mI (1806) cpu_start: Application information:[0m
[0;32mI (1809) cpu_start: Project name:     ESP32-TUX[0m
[0;32mI (1814) cpu_start: App version:      0.11.0[0m
[0;32mI (1819) cpu_start: Compile time:     Dec  4 2025 14:31:46[0m
[0;32mI (1825) cpu_start: ELF file SHA256:  5f2583cb29f00a57...[0m
[0;32mI (1831) cpu_start: ESP-IDF:          v5.0[0m
[0;32mI (1837) heap_init: Initializing. RAM available for dynamic allocation:[0m
[0;32mI (1844) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM[0m
[0;32mI (1850) heap_init: At 3FFC74A0 len 00018B60 (98 KiB): DRAM[0m
[0;32mI (1856) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM[0m
[0;32mI (1862) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM[0m
[0;32mI (1869) heap_init: At 4009D33C len 00002CC4 (11 KiB): IRAM[0m
[0;32mI (1876) esp_psram: Adding pool of 4096K of PSRAM memory to heap allocator[0m
[0;32mI (1884) spi_flash: detected chip: generic[0m
[0;32mI (1888) spi_flash: flash io: dio[0m
[0;32mI (1897) cpu_start: Starting scheduler on PRO CPU.[0m
[0;32mI (0) cpu_start: Starting scheduler on APP CPU.[0m
[0;32mI (1902) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations[0m
[0;31mE (1912) ESP32-TUX: 
Firmware Ver : 0.11.0
Project Name : ESP32-TUX
IDF Version  : v5.0

Controller   : esp32 Rev.100
CPU Cores    : Dual Core
CPU Speed    : 160Mhz
Flash Size   : 4MB [external]
PSRAM Size   : 4MB [external]
Connectivity : 2.4GHz WIFI/BT/BLE
[0m
[0;32mI (1962) ESP32-TUX: Initializing SPIFFS[0m
[0;32mI (2142) ESP32-TUX: Partition size: total: 1438481, used: 298188[0m
[0;32mI (2172) SettingsConfig: Loaded timezone: UTC0[0m
[0;32mI (2172) SettingsConfig: Loaded network: Local Network (10.13.13.0/24)[0m
[0;32mI (2172) SettingsConfig: Loaded weather location: Home (Kleve, Germany)[0m
[0;32mI (2182) SettingsConfig: Loaded weather location: Amsterdam (Amsterdam, Netherlands)[0m
[0;32mI (2192) ESP32-TUX: Loaded existing configuration[0m
[0;32mI (2202) BambuHelper: Initializing Bambu Monitor helper[0m
[0;32mI (2202) BambuMonitor: Initializing Bambu Monitor for device: bambu_lab_printer[0m
[0;31mE (2212) mqtt_client: No scheme found[0m
[0;31mE (2212) mqtt_client: Failed to create transport list[0m
[0;31mE (2222) BambuMonitor: Failed to start MQTT client: ESP_FAIL[0m
[0;31mE (2232) BambuHelper: Failed to initialize Bambu Monitor: ESP_FAIL[0m
[0;33mW (2232) ESP32-TUX: Bambu Monitor initialization optional - continuing without it[0m
[0;32mI (2712) ESP32-TUX: Start to run LVGL[0m
[0;31mE (2712) ESP32-TUX: F Drive is ready[0m
[0;32mI (2772) ESP32-TUX: Read from F:/readme.txt : ESP32-TUX - A Touch UX Template@X�@[0m
[0;31mE (2772) ESP32-TUX: S Drive is ready[0m
[0;31mE (2772) ESP32-TUX: Failed to open S:/readme.txt, 12[0m
[0;33mW (3022) ESP32-TUX: Using Gradient[0m
[0;33mW (3192) ESP32-TUX: Slideshow mode enabled - auto-cycling every 8000 ms[0m
[0;32mI (3202) WebServer: HTTP server started on http://esp32-tux.local[0m
[0;32mI (3202) ESP32-TUX: Web server started successfully[0m
[0;32mI (3202) ESP32-TUX: [APP] Free memory: 4265140 bytes[0m
[0;32mI (3212) system_api: Base MAC address is not set[0m
[0;32mI (3212) system_api: read default base MAC address from EFUSE[0m
[0;32mI (3242) wifi_init: rx ba win: 16[0m
[0;32mI (3242) ESP32-TUX: Bambu Monitor task started[0m
[0;32mI (3242) wifi_init: tcpip mbox: 32[0m
[0;32mI (3242) wifi_init: udp mbox: 6[0m
[0;32mI (3252) wifi_init: tcp mbox: 6[0m
[0;32mI (3252) wifi_init: tcp tx win: 5744[0m
[0;32mI (3252) wifi_init: tcp rx win: 5744[0m
[0;32mI (3262) wifi_init: tcp mss: 1440[0m
[0;32mI (3262) wifi_init: WiFi/LWIP prefer SPIRAM[0m
[0;32mI (3272) wifi_init: WiFi IRAM OP enabled[0m
[0;32mI (3272) wifi_init: WiFi RX IRAM OP enabled[0m
[0;32mI (3282) ESP32-TUX: Already provisioned, starting Wi-Fi STA[0m
[0;32mI (3292) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07[0m
[0;33mW (3522) ESP32-TUX: WIFI_EVENT_STA_CONNECTED[0m
[0;33mW (3522) ESP32-TUX: [51] MSG_WIFI_CONNECTED[0m
W (3542) wifi:<ba-add>idx:0 (ifx:0, 74:83:c2:3d:2e:c3), tid:0, ssn:1, winSize:64
[0;33mW (4192) ESP32-TUX: IP_EVENT_STA_GOT_IP[0m
[0;32mI (4192) ESP32-TUX: Web UI available at: http://10.13.13.188[0m
[0;32mI (4192) ESP32-TUX: Connected with IP Address:10.13.13.188[0m
[0;32mI (4202) ESP32-TUX: Connected with IP Address:10.13.13.188[0m
[0;32mI (4202) esp_netif_handlers: sta ip: 10.13.13.188, mask: 255.255.255.0, gw: 10.13.13.1[0m
[0;32mI (4202) ESP32-TUX: Got time - Self-destruct Task :)[0m
[0;32mI (4212) ESP32-TUX: Wifi Connected - Self-destruct Task :)[0m
