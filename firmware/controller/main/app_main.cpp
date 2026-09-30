// Glugg: offline Matter controller on the ESP32-S3 that shows which workshop windows are open.
// Started from esp-matter release/v1.6 examples/controller (Public Domain / CC0). See ../README.md.

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <common_macros.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_console.h>
#include <esp_ot_config.h>
#include <platform/ESP32/OpenthreadLauncher.h>

#ifdef CONFIG_CUSTOM_REVOKED_DAC_CHAIN_CHECK
#include <esp_matter_da_revocation_delegate.h>
#include <revocation_set/json_set_da_revocation_delegate.h>
extern const uint8_t revocation_set_json_start[] asm("_binary_revocation_set_json_start");
extern const uint8_t revocation_set_json_end[] asm("_binary_revocation_set_json_end");
static chip::Credentials::json_set_da_revocation_delegate s_custom_delegate((const char *)revocation_set_json_start,
                                                                            (const char *)revocation_set_json_end);
#endif

#include "app_state.h"
#include "console_cmds.h"
#include "led_task.h"
#include "sensor_link.h"
#include "thread_network.h"

static const char *TAG = "app_main";

// Our controller's own Matter identity on the fabric it creates.
static constexpr chip::NodeId kControllerNodeId = 112233;
static constexpr chip::FabricId kFabricId = 1;
static constexpr uint16_t kListenPort = 5580;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kThreadStateChange:
        if (event->ThreadStateChange.RoleChanged) {
            ESP_LOGI(TAG, "Thread role changed");
        }
        break;
    case chip::DeviceLayer::DeviceEventType::kDnssdInitialized:
        // Matter's resolver works only after the S3's own SRP client has finished one exchange with its own SRP
        // server. Subscribing earlier fails every sensor and sends it into backoff.
        sensor_link_network_ready();
        break;
    default:
        break;
    }
}

extern "C" void app_main()
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS unusable (%s); erasing it", esp_err_to_name(nvs_err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Slots and LEDs first: the startup sweep runs until the network is up, and keeps running if it never comes up.
    app_state_init();
    led_task_start();

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
    esp_matter::console::controller_register_commands(); // `matter esp controller ...`, for debugging
    esp_matter::console::otcli_register_commands();      // `matter esp ot_cli ...`, for debugging
    console_cmds_register();                             // pair, list, remove, identify, factory-reset confirm
#endif

    thread_network_watch_radio(); // before OpenThread starts talking to the H2

    esp_openthread_platform_config_t ot_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&ot_config);

    esp_err_t err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto &controller = esp_matter::controller::matter_controller_client::get_instance();
    err = controller.init(kControllerNodeId, kFabricId, kListenPort);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialise the Matter controller, err:%d", err));
#ifdef CONFIG_CUSTOM_REVOKED_DAC_CHAIN_CHECK
    chip::Credentials::set_custom_da_revocation_delegate(&s_custom_delegate);
#endif
    err = controller.setup_commissioner();
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to set up the commissioner, err:%d", err));

    thread_network_start();
    sensor_link_start();
}
