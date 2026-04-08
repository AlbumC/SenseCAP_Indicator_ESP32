#include "indicator_wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_log.h"
#include "esp_event.h"
#include "view_data.h"

static const char *TAG = "WIFI_MULTI";
WiFiMulti wifiMulti;

extern esp_event_loop_handle_t view_event_handle;
ESP_EVENT_DECLARE_BASE(VIEW_EVENT_BASE);

// --- Function to perform scan and send to UI ---
void wifi_scan_and_report() {
    ESP_LOGI(TAG, "Starting WiFi Scan...");
    int n = WiFi.scanNetworks();
    
    struct view_data_wifi_list list;
    memset(&list, 0, sizeof(list));
    
    list.is_connect = (WiFi.status() == WL_CONNECTED);
    if (list.is_connect) {
        strncpy(list.connect.ssid, WiFi.SSID().c_str(), sizeof(list.connect.ssid) - 1);
        list.connect.rssi = WiFi.RSSI();
    }

    list.cnt = (n > WIFI_SCAN_LIST_SIZE) ? WIFI_SCAN_LIST_SIZE : n;
    
    for (int i = 0; i < list.cnt; ++i) {
        strncpy(list.aps[i].ssid, WiFi.SSID(i).c_str(), sizeof(list.aps[i].ssid) - 1);
        list.aps[i].rssi = WiFi.RSSI(i);
        list.aps[i].auth_mode = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    ESP_LOGI(TAG, "Scan complete. Found %d networks. Reporting to UI...", n);
    esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_WIFI_LIST, 
                      &list, sizeof(list), portMAX_DELAY);
    
    WiFi.scanDelete(); // Clean up memory
}

// --- Event Handler for UI Requests ---
static void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    if (id == VIEW_EVENT_WIFI_LIST_REQ) {
        wifi_scan_and_report();
    }
}

extern "C" int indicator_wifi_init(void) {
    initArduino();
    WiFi.mode(WIFI_STA);

    // Add your known networks
    wifiMulti.addAP("Home_WiFi", "password123");
    wifiMulti.addAP("Office_WiFi", "office_pass");

    // Register listener for UI scan requests
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, 
                    VIEW_EVENT_BASE, VIEW_EVENT_WIFI_LIST_REQ, wifi_event_handler, NULL, NULL));

    // Background connection task
    xTaskCreate([](void* p){
        while(1) {
            // wifiMulti.run() handles the background connection logic
            wifiMulti.run();
            vTaskDelay(pdMS_TO_TICKS(10000)); 
        }
    }, "wifi_multi_task", 4096, NULL, 5, NULL);

    return 0;
}
