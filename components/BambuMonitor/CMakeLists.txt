idf_component_register(
    SRCS "BambuMonitor.cpp" "BambuMqttClient.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event mbedtls mqtt
    PRIV_REQUIRES json nvs_flash
    # Embed Bambu Lab printer certificate
    EMBED_TXTFILES ${project_dir}/server_certs/bambu_combined.cert
)
