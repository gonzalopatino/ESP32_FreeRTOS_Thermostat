#ifndef TASK_NET_H
#define TASK_NET_H

/**
 * @brief Start the Wi-Fi / NET task.
 *
 * The task:
 *  - Initializes NVS (required by Wi-Fi)
 *  - Initializes esp_netif and event loop
 *  - Brings up Wi-Fi STA and attempts to connect to WIFI_SSID
 *  - Logs connection / disconnection / IP events
 */
void task_net_start(void);

#endif  // TASK_NET_H
