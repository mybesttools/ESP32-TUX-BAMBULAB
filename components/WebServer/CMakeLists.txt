idf_component_register(SRCS "WebServer.cpp" "PrinterDiscovery.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server esp_event esp_wifi json SettingsConfig mqtt espressif__mdns BambuMonitor esp_app_format)

