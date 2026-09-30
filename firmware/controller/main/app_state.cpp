#include "app_state.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>

static const char *TAG = "app_state";
static const char *NVS_NAMESPACE = "windows";
static const char *NVS_KEY_SLOTS = "slots";
// A copy of the next node ID outside the slot blob, so an unreadable blob can't make node IDs start over while the
// sensors that own them are still on the fabric.
static const char *NVS_KEY_NEXT_NODE = "next_node";

static sensor_mgr_t s_mgr;
static SemaphoreHandle_t s_lock;

uint32_t app_now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void load_table(slot_table_t *table)
{
    slot_table_init(table);
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGI(TAG, "No saved slots (first boot)");
        return;
    }
    uint8_t blob[SLOT_TABLE_BLOB_SIZE];
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(nvs, NVS_KEY_SLOTS, blob, &len);
    uint64_t next_node = 0;
    bool have_next_node =
        nvs_get_u64(nvs, NVS_KEY_NEXT_NODE, &next_node) == ESP_OK && slot_table_next_id_valid(next_node);
    nvs_close(nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved slots (first boot)");
    } else if (err != ESP_OK || !slot_table_decode(table, blob, len)) {
        // Starting empty loses the slot mapping, but the sensors stay on the Matter fabric: factory reset them and
        // `pair` them again.
        ESP_LOGE(TAG, "Saved slots are unreadable (%s, %u bytes); starting with no sensors", esp_err_to_name(err),
                 (unsigned)len);
        slot_table_init(table);
        if (!have_next_node) {
            table->next_node_id = SLOT_RECOVERY_NODE_ID; // well clear of any ID a lost sensor may still use
        }
    }
    if (have_next_node && next_node > table->next_node_id) {
        table->next_node_id = next_node;
    }
}

void app_state_init()
{
    s_lock = xSemaphoreCreateMutex();
    slot_table_t table;
    load_table(&table);
    sensor_mgr_init(&s_mgr, &table, app_now_ms());
    int used = 0;
    for (int i = 0; i < SLOT_COUNT; i++) {
        used += table.slot[i].used;
    }
    ESP_LOGI(TAG, "%d of %d slots paired", used, SLOT_COUNT);
}

sensor_mgr_t *app_state_lock()
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    return &s_mgr;
}

void app_state_unlock()
{
    xSemaphoreGive(s_lock);
}

void app_state_save(const sensor_mgr_t *m)
{
    uint8_t blob[SLOT_TABLE_BLOB_SIZE];
    size_t len = slot_table_encode(&m->table, blob, sizeof(blob));
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs, NVS_KEY_SLOTS, blob, len);
        if (err == ESP_OK) {
            err = nvs_set_u64(nvs, NVS_KEY_NEXT_NODE, m->table.next_node_id);
        }
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Saving slots failed: %s", esp_err_to_name(err));
    }
}
