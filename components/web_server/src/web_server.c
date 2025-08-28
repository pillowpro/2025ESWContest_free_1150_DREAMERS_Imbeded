#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "utils.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;
static wifi_config_callback_t config_callback = NULL;

static const char* provisioning_html = 
"<!DOCTYPE html>"
"<html><head><title>BaegaePro Setup</title>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:Arial,sans-serif;max-width:600px;margin:0 auto;padding:20px;background:#f0f0f0}"
".container{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
"h1{color:#333;text-align:center}"
".form-group{margin:20px 0}"
"label{display:block;margin:0 0 5px;font-weight:bold}"
"input[type=text],input[type=password]{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}"
"button{background:#007bff;color:white;padding:12px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;margin-top:20px}"
"button:hover{background:#0056b3}"
"button:disabled{background:#ccc;cursor:not-allowed}"
".status{margin:20px 0;padding:10px;border-radius:4px;text-align:center}"
".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}"
"</style></head><body>"
"<div class='container'>"
"<h1>BaegaePro WiFi Setup</h1>"
"<div class='form-group'>"
"<label for='ssid'>WiFi Network Name (SSID):</label>"
"<input type='text' id='ssid' placeholder='Enter WiFi SSID' required>"
"</div>"
"<div class='form-group'>"
"<label for='password'>WiFi Password:</label>"
"<input type='password' id='password' placeholder='Enter WiFi Password'>"
"</div>"
"<div class='form-group'>"
"<label for='token'>Authorization Token:</label>"
"<input type='text' id='token' placeholder='Enter Auth Token' required>"
"</div>"
"<button onclick='submitConfig()'>Connect</button>"
"<div id='status' class='status' style='display:none'></div>"
"</div>"
"<script>"
"function submitConfig(){const ssid=document.getElementById('ssid').value;const password=document.getElementById('password').value;const token=document.getElementById('token').value;if(!ssid||!token){showStatus('Please fill in SSID and Token','error');return;}showStatus('Connecting...','');const btn=document.querySelector('button');btn.disabled=true;btn.textContent='Connecting...';fetch('/api/wifi-config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,password:password,token:token})}).then(response=>response.json()).then(data=>{if(data.success){showStatus('Configuration saved! Device is connecting to WiFi...','success');setTimeout(()=>{window.location.reload();},3000);}else{showStatus('Error: '+data.message,'error');btn.disabled=false;btn.textContent='Connect';}}).catch(error=>{showStatus('Connection failed: '+error.message,'error');btn.disabled=false;btn.textContent='Connect';});}"
"function showStatus(message,type){const status=document.getElementById('status');status.textContent=message;status.className='status '+(type||'');status.style.display='block';}"
"</script></div></body></html>";

static esp_err_t provisioning_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, provisioning_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    char buf[1024];
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

    cJSON *json = cJSON_Parse(buf);
    cJSON *response_json = cJSON_CreateObject();
    
    if (!json) {
        cJSON *success = cJSON_CreateBool(false);
        cJSON *message = cJSON_CreateString("Invalid JSON format");
        cJSON_AddItemToObject(response_json, "success", success);
        cJSON_AddItemToObject(response_json, "message", message);
    } else {
        wifi_credentials_t credentials = {0};
        
        cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
        cJSON *password_json = cJSON_GetObjectItem(json, "password");
        cJSON *token_json = cJSON_GetObjectItem(json, "token");
        
        if (!cJSON_IsString(ssid_json) || !cJSON_IsString(token_json)) {
            cJSON *success = cJSON_CreateBool(false);
            cJSON *message = cJSON_CreateString("Missing required fields: ssid, token");
            cJSON_AddItemToObject(response_json, "success", success);
            cJSON_AddItemToObject(response_json, "message", message);
        } else {
            strncpy(credentials.ssid, ssid_json->valuestring, sizeof(credentials.ssid) - 1);
            if (cJSON_IsString(password_json)) {
                strncpy(credentials.password, password_json->valuestring, sizeof(credentials.password) - 1);
            }
            strncpy(credentials.token, token_json->valuestring, sizeof(credentials.token) - 1);
            
            ESP_LOGI(TAG, "Received WiFi config - SSID: %s", credentials.ssid);
            
            if (config_callback) {
                config_callback(&credentials);
            }
            
            cJSON *success = cJSON_CreateBool(true);
            cJSON *message = cJSON_CreateString("Configuration received, connecting to WiFi...");
            cJSON_AddItemToObject(response_json, "success", success);
            cJSON_AddItemToObject(response_json, "message", message);
        }
        
        cJSON_Delete(json);
    }
    
    char *response_string = cJSON_Print(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response_string, strlen(response_string));
    
    free(response_string);
    cJSON_Delete(response_json);
    
    return ESP_OK;
}

static esp_err_t cors_options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateString("provisioning");
    char device_id_str[16];
    utils_get_mac_based_device_id(device_id_str, sizeof(device_id_str));
    cJSON *device_id = cJSON_CreateString(device_id_str);
    cJSON *firmware_version = cJSON_CreateString("1.0.0");
    cJSON *uptime = cJSON_CreateNumber(esp_timer_get_time() / 1000);
    
    cJSON_AddItemToObject(json, "status", status);
    cJSON_AddItemToObject(json, "device_id", device_id);
    cJSON_AddItemToObject(json, "firmware_version", firmware_version);
    cJSON_AddItemToObject(json, "uptime_ms", uptime);
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

static esp_err_t device_info_get_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    char device_id_str[16];
    utils_get_mac_based_device_id(device_id_str, sizeof(device_id_str));
    cJSON *device_id = cJSON_CreateString(device_id_str);
    
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON *mac_address = cJSON_CreateString(mac_str);
    cJSON *chip_model = cJSON_CreateString("ESP32-C6");
    cJSON *firmware_version = cJSON_CreateString("1.0.0");
    cJSON *heap_free = cJSON_CreateNumber(esp_get_free_heap_size());
    cJSON *uptime = cJSON_CreateNumber(esp_timer_get_time() / 1000);
    
    cJSON_AddItemToObject(json, "device_id", device_id);
    cJSON_AddItemToObject(json, "mac_address", mac_address);
    cJSON_AddItemToObject(json, "chip_model", chip_model);
    cJSON_AddItemToObject(json, "firmware_version", firmware_version);
    cJSON_AddItemToObject(json, "heap_free", heap_free);
    cJSON_AddItemToObject(json, "uptime_ms", uptime);
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

static esp_err_t reset_post_handler(httpd_req_t *req)
{
    cJSON *response_json = cJSON_CreateObject();
    cJSON *success = cJSON_CreateBool(true);
    cJSON *message = cJSON_CreateString("Device will reset in 3 seconds");
    
    cJSON_AddItemToObject(response_json, "success", success);
    cJSON_AddItemToObject(response_json, "message", message);
    
    char *response_string = cJSON_Print(response_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response_string, strlen(response_string));
    
    free(response_string);
    cJSON_Delete(response_json);
    
    // 3초 후 재시작
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
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
            .uri       = "/api/wifi-config",
            .method    = HTTP_POST,
            .handler   = wifi_config_post_handler,
            .user_ctx  = NULL
        };
        
        httpd_uri_t wifi_config_cors = {
            .uri       = "/api/wifi-config",
            .method    = HTTP_OPTIONS,
            .handler   = cors_options_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &wifi_config);
        httpd_register_uri_handler(server, &wifi_config_cors);

        httpd_uri_t status = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status);
        
        httpd_uri_t device_info = {
            .uri       = "/api/device-info",
            .method    = HTTP_GET,
            .handler   = device_info_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &device_info);
        
        httpd_uri_t reset = {
            .uri       = "/api/reset",
            .method    = HTTP_POST,
            .handler   = reset_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &reset);
        
        httpd_uri_t reset_cors = {
            .uri       = "/api/reset",
            .method    = HTTP_OPTIONS,
            .handler   = cors_options_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &reset_cors);
        
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
