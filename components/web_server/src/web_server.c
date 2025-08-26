#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;
static wifi_config_callback_t config_callback = NULL;

static const char* provisioning_html = 
"<!DOCTYPE html>"
"<html><head><title>BaegaePro Setup</title>"
"<meta charset='UTF-8'>"
"<style>"
"body{font-family:Arial,sans-serif;max-width:600px;margin:0 auto;padding:20px;background:#f0f0f0}"
".container{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
"h1{color:#333;text-align:center}"
"form{margin-top:20px}"
"label{display:block;margin:10px 0 5px;font-weight:bold}"
"input[type=text],input[type=password]{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}"
"button{background:#007bff;color:white;padding:12px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;margin-top:20px}"
"button:hover{background:#0056b3}"
"</style></head><body>"
"<div class='container'>"
"<h1>BaegaePro WiFi Setup</h1>"
"<form action='/wifi-config' method='post'>"
"<label for='ssid'>WiFi Network Name (SSID):</label>"
"<input type='text' id='ssid' name='ssid' required>"
"<label for='password'>WiFi Password:</label>"
"<input type='password' id='password' name='password'>"
"<label for='token'>Authorization Token:</label>"
"<input type='text' id='token' name='token' required>"
"<button type='submit'>Connect</button>"
"</form></div></body></html>";

static esp_err_t provisioning_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, provisioning_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret, remaining = req->content_len;

    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    wifi_credentials_t credentials = {0};
    
    char *ssid_start = strstr(buf, "ssid=");
    char *password_start = strstr(buf, "password=");
    char *token_start = strstr(buf, "token=");
    
    if (ssid_start) {
        ssid_start += 5;
        char *ssid_end = strchr(ssid_start, '&');
        if (!ssid_end) ssid_end = buf + strlen(buf);
        size_t ssid_len = ssid_end - ssid_start;
        if (ssid_len < sizeof(credentials.ssid)) {
            strncpy(credentials.ssid, ssid_start, ssid_len);
            credentials.ssid[ssid_len] = '\0';
        }
    }
    
    if (password_start) {
        password_start += 9;
        char *password_end = strchr(password_start, '&');
        if (!password_end) password_end = buf + strlen(buf);
        size_t password_len = password_end - password_start;
        if (password_len < sizeof(credentials.password)) {
            strncpy(credentials.password, password_start, password_len);
            credentials.password[password_len] = '\0';
        }
    }
    
    if (token_start) {
        token_start += 6;
        char *token_end = strchr(token_start, '&');
        if (!token_end) token_end = buf + strlen(buf);
        size_t token_len = token_end - token_start;
        if (token_len < sizeof(credentials.token)) {
            strncpy(credentials.token, token_start, token_len);
            credentials.token[token_len] = '\0';
        }
    }

    ESP_LOGI(TAG, "Received WiFi config - SSID: %s", credentials.ssid);

    if (config_callback) {
        config_callback(&credentials);
    }

    const char* response = "<!DOCTYPE html><html><head><title>BaegaePro</title></head>"
                          "<body><h1>Configuration Received</h1><p>Connecting to WiFi...</p></body></html>";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateString("provisioning");
    cJSON_AddItemToObject(json, "status", status);
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Web server initialized");
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = provisioning_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t wifi_config = {
            .uri       = "/wifi-config",
            .method    = HTTP_POST,
            .handler   = wifi_config_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &wifi_config);

        httpd_uri_t status = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status);
        
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

esp_err_t web_server_stop(void)
{
    if (server) {
        esp_err_t ret = httpd_stop(server);
        server = NULL;
        return ret;
    }
    return ESP_OK;
}

esp_err_t web_server_set_callback(wifi_config_callback_t callback)
{
    config_callback = callback;
    return ESP_OK;
}
