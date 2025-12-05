idf_component_register(
    SRCS "BambuMonitor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event
    PRIV_REQUIRES json nvs_flash mqtt
)
