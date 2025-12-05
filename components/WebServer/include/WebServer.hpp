/*
MIT License

Copyright (c) 2024 ESP32-TUX Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*/

#ifndef TUX_WEBSERVER_H_
#define TUX_WEBSERVER_H_

#include <esp_http_server.h>
#include <esp_event.h>
#include <vector>
#include <string>

class WebServer {
public:
    WebServer();
    ~WebServer();
    
    esp_err_t start();
    esp_err_t stop();
    bool is_running();
    
private:
    httpd_handle_t server;
    
    // Discovery state management
    static std::vector<std::string> g_discovered_ips;
    static bool g_discovery_in_progress;
    static int g_discovery_progress;
    static TaskHandle_t g_discovery_task;
    
    // Background discovery task
    static void discovery_task_handler(void *pvParameter);
    
    // Handler declarations
    static esp_err_t handle_root(httpd_req_t *req);
    static esp_err_t handle_api_config_get(httpd_req_t *req);
    static esp_err_t handle_api_config_post(httpd_req_t *req);
    static esp_err_t handle_api_weather_get(httpd_req_t *req);
    static esp_err_t handle_api_weather_post(httpd_req_t *req);
    static esp_err_t handle_api_locations_get(httpd_req_t *req);
    static esp_err_t handle_api_locations_post(httpd_req_t *req);
    static esp_err_t handle_api_locations_delete(httpd_req_t *req);
    static esp_err_t handle_api_printers_get(httpd_req_t *req);
    static esp_err_t handle_api_printers_post(httpd_req_t *req);
    static esp_err_t handle_api_printers_delete(httpd_req_t *req);
    static esp_err_t handle_api_printers_discover(httpd_req_t *req);
    static esp_err_t handle_api_printers_discover_status(httpd_req_t *req);
    static esp_err_t handle_api_printer_info(httpd_req_t *req);
    static esp_err_t handle_api_printer_query(httpd_req_t *req);
    static esp_err_t handle_api_test_connection(httpd_req_t *req);
    static esp_err_t handle_api_device_info(httpd_req_t *req);
    static esp_err_t handle_api_networks_get(httpd_req_t *req);
    static esp_err_t handle_api_networks_post(httpd_req_t *req);
    static esp_err_t handle_api_networks_delete(httpd_req_t *req);
};

extern WebServer *web_server;

#endif // TUX_WEBSERVER_H_
