idf_component_register(SRCS "ota.c" 
                    INCLUDE_DIRS "." 
                    REQUIRES esp_https_ota app_update esp_event esp-tls mbedtls
                    PRIV_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/main"
                    # Embed the server root certificate into the final binary
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
