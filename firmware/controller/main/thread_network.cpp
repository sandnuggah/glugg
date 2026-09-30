#include "thread_network.h"

#include <esp_log.h>
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <esp_openthread_spinel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>
#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/srp_server.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ThreadStackManager.h>

#include "app_state.h"
#include "thread_channel.h"

static const char *TAG = "thread_network";

using chip::DeviceLayer::ThreadStackMgr;

// Time spent listening on each channel before forming the network (16 channels, first boot only).
static constexpr uint16_t kEnergyScanMsPerChannel = 300;
static constexpr uint32_t kAllChannels = 0x07FFF800; // bits 11-26
static constexpr uint32_t kRadioFailedShowMs = 3000;

// ---------------------------------------------------------------------------------------------------------------
// The H2 radio

// OpenThread stops the S3 right after these return (esp_openthread_radio_spinel.cpp asserts they are set).
static void radio_failed(const char *what)
{
    ESP_LOGE(TAG, "The Thread radio %s. Check the UART wiring (H2 TX -> S3 GPIO5, S3 GPIO6 -> H2 RX, common GND), "
             "that the H2 runs ot_rcp built from this ESP-IDF, and that both sides use 460800 baud. Restarting.",
             what);
    {
        AppStateLock m;
        sensor_mgr_set_radio_failed(m.get());
    }
    vTaskDelay(pdMS_TO_TICKS(kRadioFailedShowMs)); // the LED task shows red meanwhile
}

static void on_rcp_incompatible()
{
    radio_failed("(H2) runs an incompatible RCP firmware version");
}

static void on_rcp_reset_failed()
{
    radio_failed("(H2) doesn't answer");
}

void thread_network_watch_radio()
{
    esp_openthread_set_compatibility_error_callback(on_rcp_incompatible);
    esp_openthread_set_coprocessor_reset_failure_callback(on_rcp_reset_failed);
}

// ---------------------------------------------------------------------------------------------------------------
// Forming and starting the network

// The S3 is the network's only SRP server. Once it has a role, the server publishes itself in Network Data.
// Sensors register there, and Matter's DNS client resolves them through the DNS-SD server on the same node
// (sim/README.md). There is no Wi-Fi backbone, so esp_openthread_border_router_init() is never called.
static void start_srp_server()
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *instance = esp_openthread_get_instance();
    otSrpServerSetAddressMode(instance, OT_SRP_SERVER_ADDRESS_MODE_UNICAST);
    otSrpServerSetEnabled(instance, true);
    esp_openthread_lock_release();
    ESP_LOGI(TAG, "SRP server enabled");
}

static void enable_thread()
{
    CHIP_ERROR err = ThreadStackMgr().SetThreadEnabled(true);
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Starting Thread failed: %" CHIP_ERROR_FORMAT, err.Format());
        return;
    }
    start_srp_server();
}

// channel < 0 keeps OpenThread's random pick.
static bool create_dataset(otOperationalDatasetTlvs *tlvs, int channel)
{
    otOperationalDataset dataset;
    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetCreateNewNetwork(esp_openthread_get_instance(), &dataset);
    if (err == OT_ERROR_NONE) {
        if (channel >= 0) {
            dataset.mChannel = (uint16_t)channel;
        }
        // Thread 1.4's Wake-up Channel TLV is new; MYGGBETT's older OpenThread has never seen it. Leave it out.
        dataset.mComponents.mIsWakeupChannelPresent = false;
        otDatasetConvertToTlvs(&dataset, tlvs);
    }
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Creating a Thread network failed: %s", otThreadErrorToString(err));
        return false;
    }
    ESP_LOGI(TAG, "Created Thread network \"%s\" on channel %u", dataset.mNetworkName.m8, dataset.mChannel);
    return true;
}

// Runs on the Matter thread.
static void form_network(intptr_t channel)
{
    otOperationalDatasetTlvs tlvs;
    if (!create_dataset(&tlvs, (int)channel)) {
        return;
    }
    CHIP_ERROR err = ThreadStackMgr().SetThreadProvision(chip::ByteSpan(tlvs.mTlvs, tlvs.mLength));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Saving the Thread dataset failed: %" CHIP_ERROR_FORMAT, err.Format());
        return;
    }
    enable_thread();
}

static int8_t s_max_rssi[THREAD_CHANNEL_COUNT];
static uint16_t s_scanned;

// Runs on the OpenThread task: once per channel, then with nullptr when the scan is done.
static void on_energy_scan(otEnergyScanResult *result, void *)
{
    if (result != nullptr) {
        int i = result->mChannel - THREAD_CHANNEL_FIRST;
        if (i >= 0 && i < THREAD_CHANNEL_COUNT) {
            s_max_rssi[i] = result->mMaxRssi;
            s_scanned |= 1u << i;
        }
        return;
    }
    char line[THREAD_CHANNEL_COUNT * 9 + 1];
    int len = 0;
    for (int i = 0; i < THREAD_CHANNEL_COUNT; i++) {
        if (s_scanned >> i & 1) {
            len += snprintf(line + len, sizeof(line) - len, " %d:%d", THREAD_CHANNEL_FIRST + i, s_max_rssi[i]);
        }
    }
    int channel = thread_channel_pick(s_max_rssi, s_scanned);
    ESP_LOGI(TAG, "Energy scan (channel:max dBm):%s -> channel %d", len ? line : " nothing measured", channel);
    LogErrorOnFailure(chip::DeviceLayer::PlatformMgr().ScheduleWork(form_network, channel));
}

// First boot: listen to every channel and form the network on the quietest. Falls back to a random channel.
static void scan_then_form_network()
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *instance = esp_openthread_get_instance();
    otError err = otIp6SetEnabled(instance, true); // the radio only listens while the interface is up
    if (err == OT_ERROR_NONE) {
        err = otLinkEnergyScan(instance, kAllChannels, kEnergyScanMsPerChannel, on_energy_scan, nullptr);
    }
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "Energy scan failed (%s); picking a random channel", otThreadErrorToString(err));
        form_network(-1);
        return;
    }
    ESP_LOGI(TAG, "No Thread network yet: listening on each channel for %u ms to pick the quietest",
             kEnergyScanMsPerChannel);
}

void thread_network_start()
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    bool running = otInstanceIsInitialized(esp_openthread_get_instance());
    esp_openthread_lock_release();
    if (!running) {
        ESP_LOGE(TAG, "OpenThread didn't start; no Thread network");
        return;
    }
    if (ThreadStackMgr().IsThreadProvisioned()) {
        enable_thread();
    } else {
        scan_then_form_network();
    }
}
