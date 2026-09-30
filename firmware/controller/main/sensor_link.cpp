#include "sensor_link.h"

#include <inttypes.h>
#include <memory>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/InteractionModelEngine.h>
#include <app/ReadClient.h>
#include <app/data-model/Decode.h>
#include <controller/CHIPDeviceController.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ThreadStackManager.h>

#include "app_state.h"

using namespace chip;
using namespace chip::app;
using chip::Controller::CommissioningParameters;
using chip::Controller::CommissioningStage;
using chip::Controller::DiscoveryType;
using esp_matter::controller::matter_controller_client;

static const char *TAG = "sensor_link";

// MYGGBETT: endpoint 1 is a Contact Sensor with BooleanState.StateValue (true = closed).
static constexpr EndpointId kSensorEndpoint = 1;
// The sensor (an ICD) may raise the max interval to its idle duration; this is only our ceiling.
static constexpr uint16_t kMaxIntervalCeilingS = 300;
static constexpr uint16_t kIdentifySeconds = 10;
static constexpr System::Clock::Seconds32 kTickInterval{1};
static constexpr System::Clock::Seconds32 kPairingTimeout{5 * 60};
// Longest a factory reset waits for sleepy sensors to confirm they left our fabric.
static constexpr System::Clock::Seconds32 kFactoryResetMaxWait{120};
static constexpr uint32_t kNetworkWarnAfterMs = 60 * 1000;

static Controller::DeviceCommissioner *commissioner()
{
    return matter_controller_client::get_instance().get_commissioner();
}

// ---------------------------------------------------------------------------------------------------------------
// Subscriptions

class Subscription final : public ReadClient::Callback {
public:
    Subscription(int slot, NodeId node)
        : m_slot(slot)
        , m_node(node)
        , m_on_connected(on_connected, this)
        , m_on_connection_failure(on_connection_failure, this)
    {
    }

    void start()
    {
        CHIP_ERROR err = commissioner()->GetConnectedDevice(
            m_node, &m_on_connected, &m_on_connection_failure);
        if (err != CHIP_NO_ERROR) {
            report_down(err);
        }
    }

private:
    static void on_connected(void *context, Messaging::ExchangeManager &exchange_mgr, const SessionHandle &session)
    {
        auto *self = static_cast<Subscription *>(context);
        self->m_path = AttributePathParams(kSensorEndpoint, Clusters::BooleanState::Id,
                                           Clusters::BooleanState::Attributes::StateValue::Id);
        self->m_client = std::make_unique<ReadClient>(InteractionModelEngine::GetInstance(), &exchange_mgr, *self,
                                                      ReadClient::InteractionType::Subscribe);
        ReadPrepareParams params(session);
        params.mpAttributePathParamsList = &self->m_path;
        params.mAttributePathParamsListSize = 1;
        params.mMinIntervalFloorSeconds = 0;
        params.mMaxIntervalCeilingSeconds = kMaxIntervalCeilingS;
        params.mKeepSubscriptions = false; // replaces any subscription we left behind on the sensor
        // No auto-resubscribe: sensor_mgr decides when to retry, with unlimited backoff.
        CHIP_ERROR err = self->m_client->SendRequest(params);
        if (err != CHIP_NO_ERROR) {
            self->m_client.reset();
            self->report_down(err);
        }
    }

    static void on_connection_failure(void *context, const ScopedNodeId &, CHIP_ERROR error)
    {
        static_cast<Subscription *>(context)->report_down(error);
    }

    // True while this object's node still owns its slot; late callbacks after a remove are dropped.
    bool current(const sensor_mgr_t *m) const
    {
        return m->table.slot[m_slot].used && m->table.slot[m_slot].node_id == m_node;
    }

    void OnAttributeData(const ConcreteDataAttributePath &path, TLV::TLVReader *data, const StatusIB &status) override
    {
        if (path.mClusterId != Clusters::BooleanState::Id ||
            path.mAttributeId != Clusters::BooleanState::Attributes::StateValue::Id || data == nullptr ||
            !status.IsSuccess()) {
            return;
        }
        bool closed = false;
        if (DataModel::Decode(*data, closed) != CHIP_NO_ERROR) {
            ESP_LOGW(TAG, "Slot %d: undecodable StateValue", m_slot + 1);
            return;
        }
        ESP_LOGI(TAG, "Slot %d: %s", m_slot + 1, closed ? "closed" : "OPEN");
        AppStateLock m;
        if (current(m.get())) {
            sensor_mgr_on_state_value(m.get(), m_slot, closed, app_now_ms());
        }
    }

    // Every report on an established subscription, including the empty keep-alives a closed window sends each max
    // interval. (OnReportEnd only fires for reports that carry data.)
    void NotifySubscriptionStillActive(const ReadClient &) override
    {
        AppStateLock m;
        if (current(m.get())) {
            sensor_mgr_on_heard(m.get(), m_slot, app_now_ms());
        }
    }

    void OnSubscriptionEstablished(SubscriptionId id) override
    {
        uint16_t min_s = 0, max_s = kMaxIntervalCeilingS;
        if (m_client->GetReportingIntervals(min_s, max_s) != CHIP_NO_ERROR) {
            max_s = kMaxIntervalCeilingS; // can't happen for an active subscription; assume our ceiling
        }
        ESP_LOGI(TAG, "Slot %d: subscribed (0x%" PRIx32 ", max interval %u s)", m_slot + 1, id, max_s);
        AppStateLock m;
        if (current(m.get())) {
            sensor_mgr_on_subscribed(m.get(), m_slot, max_s, app_now_ms());
        }
    }

    void OnError(CHIP_ERROR error) override { m_last_error = error; }

    void OnDone(ReadClient *) override
    {
        m_client.reset(); // allowed inside OnDone
        report_down(m_last_error);
    }

    void report_down(CHIP_ERROR error)
    {
        ESP_LOGW(TAG, "Slot %d: subscription down: %" CHIP_ERROR_FORMAT, m_slot + 1, error.Format());
        AppStateLock m;
        if (current(m.get())) {
            sensor_mgr_on_subscription_down(m.get(), m_slot, app_now_ms(), esp_random());
        }
    }

    const int m_slot;
    const NodeId m_node;
    AttributePathParams m_path;
    CHIP_ERROR m_last_error = CHIP_ERROR_INTERNAL;
    // Callbacks cancel themselves on destruction, so deleting a Subscription mid-connect is safe.
    chip::Callback::Callback<chip::OnDeviceConnected> m_on_connected;
    chip::Callback::Callback<chip::OnDeviceConnectionFailure> m_on_connection_failure;
    std::unique_ptr<ReadClient> m_client; // destroyed first; its destructor tears down the subscription
};

static std::unique_ptr<Subscription> s_subs[SLOT_COUNT];
static esp_task_wdt_user_handle_t s_wdt;
static bool s_network_warned;

static void tick(System::Layer *, void *)
{
    int slots[SLOT_COUNT];
    NodeId nodes[SLOT_COUNT];
    int n = 0;
    uint32_t now;
    bool ready;
    {
        AppStateLock m;
        now = app_now_ms();
        ready = m->network_ready;
        int slot;
        while ((slot = sensor_mgr_next_subscribe(m.get(), now)) >= 0) {
            slots[n] = slot;
            nodes[n] = m->table.slot[slot].node_id;
            n++;
        }
    }
    if (!ready && !s_network_warned && now >= kNetworkWarnAfterMs) {
        s_network_warned = true;
        ESP_LOGW(TAG, "No Thread network / lookup service after %" PRIu32 " s. Check `matter esp ot_cli state` "
                 "(leader?), `matter esp ot_cli srp server state` and `matter esp ot_cli srp client state`.",
                 now / 1000);
    }
    // Outside the lock: starting a subscription may call back synchronously.
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "Slot %d: subscribing to node 0x%" PRIx64, slots[i] + 1, nodes[i]);
        s_subs[slots[i]] = std::make_unique<Subscription>(slots[i], nodes[i]);
        s_subs[slots[i]]->start();
    }
    if (s_wdt) {
        esp_task_wdt_reset_user(s_wdt); // the Matter event loop is still turning
    }
    DeviceLayer::SystemLayer().StartTimer(kTickInterval, tick, nullptr);
}

void sensor_link_start()
{
    if (esp_task_wdt_add_user("matter_loop", &s_wdt) != ESP_OK) {
        ESP_LOGW(TAG, "Couldn't watch the Matter event loop with the task watchdog");
        s_wdt = nullptr;
    }
    DeviceLayer::SystemLayer().StartTimer(kTickInterval, tick, nullptr);
}

void sensor_link_network_ready()
{
    ESP_LOGI(TAG, "Thread network and lookup service are up");
    AppStateLock m;
    sensor_mgr_set_network_ready(m.get(), app_now_ms());
}

// ---------------------------------------------------------------------------------------------------------------
// Pairing
//
// Matter ends a pairing through different callbacks depending on how far it got:
//   OnStatusUpdate(SecurePairingFailed)   discovery found nothing, or PASE failed with everything it found
//   OnPairingComplete(error)              PASE with the sensor failed
//   OnCommissioningSuccess / ...Failure   the last callbacks once commissioning ran
// Whichever comes first ends it (s_pairing_active). The delegate stays registered afterwards: the SDK calls it again
// right after OnCommissioningComplete, and SetUpCodePairer swaps itself in and out around it during PASE.

static void finish_pairing(bool success, const char *what, CHIP_ERROR error);
static void clean_up_failed_commissioning(NodeId node, CommissioningStage stage);

class PairingDelegate final : public Controller::DevicePairingDelegate {
public:
    void OnStatusUpdate(DevicePairingDelegate::Status status) override
    {
        // SetUpCodePairer holds this back while it still has devices to try, so here it is final.
        if (status == DevicePairingDelegate::Status::SecurePairingFailed) {
            finish_pairing(false, "no sensor found over BLE, or the secure session with it failed (see the log above)",
                           CHIP_NO_ERROR);
        }
    }

    void OnPairingComplete(CHIP_ERROR error) override
    {
        if (error == CHIP_NO_ERROR) {
            ESP_LOGI(TAG, "Pairing: connected over BLE, commissioning...");
        } else {
            finish_pairing(false, "secure session (PASE)", error);
        }
    }

    void OnCommissioningSuccess(PeerId) override { finish_pairing(true, nullptr, CHIP_NO_ERROR); }

    void OnCommissioningFailure(PeerId peer, CHIP_ERROR error, CommissioningStage stage,
                                Optional<Credentials::AttestationVerificationResult> attestation) override
    {
        if (attestation.HasValue()) {
            ESP_LOGE(TAG, "Pairing: attestation result %u (is IKEA's PAA in paa_cert/?)",
                     static_cast<unsigned>(attestation.Value()));
        }
        finish_pairing(false, Controller::StageToString(stage), error);
        clean_up_failed_commissioning(peer.GetNodeId(), stage);
    }
};

static PairingDelegate s_pairing_delegate;
static bool s_pairing_active;
static NodeId s_pairing_node;

static void pairing_timeout(System::Layer *, void *)
{
    NodeId node = s_pairing_node;
    finish_pairing(false, "no result after 5 min", CHIP_ERROR_TIMEOUT);
    LogErrorOnFailure(commissioner()->StopPairing(node)); // its callbacks are ignored: the pairing is already over
}

static void finish_pairing(bool success, const char *what, CHIP_ERROR error)
{
    if (!s_pairing_active) {
        return; // a later callback for a pairing that already ended
    }
    s_pairing_active = false;
    DeviceLayer::SystemLayer().CancelTimer(pairing_timeout, nullptr);

    int slot;
    {
        AppStateLock m;
        slot = m->pairing_slot;
        sensor_mgr_end_pairing(m.get(), success, app_now_ms());
        app_state_save(m.get());
    }
    if (success) {
        ESP_LOGI(TAG, "Pairing: sensor is now slot %d (node 0x%" PRIx64 ")", slot + 1, s_pairing_node);
    } else if (error != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Pairing into slot %d failed: %s: %" CHIP_ERROR_FORMAT, slot + 1, what, error.Format());
    } else {
        ESP_LOGE(TAG, "Pairing into slot %d failed: %s", slot + 1, what);
    }
}

static bool request_unpair(NodeId node);

static NodeId s_cleanup_node;
static bool s_cleanup_unpair;

static void clean_up_failed_commissioning_later(intptr_t)
{
    CHIP_ERROR err = commissioner()->StopPairing(s_cleanup_node);
    if (err != CHIP_NO_ERROR && err != CHIP_ERROR_INVALID_DEVICE_DESCRIPTOR) {
        LogErrorOnFailure(err);
    }
    if (s_cleanup_unpair) {
        ESP_LOGW(TAG, "Pairing: node 0x%" PRIx64 " may have joined our fabric anyway; asking it to leave",
                 s_cleanup_node);
        request_unpair(s_cleanup_node);
    }
}

// From network setup on, the SDK keeps the PASE session open for a retry (CHIPDeviceController.cpp,
// CommissioningStageComplete), so release it; the sensor drops the half-finished setup when its fail-safe expires.
// If CommissioningComplete may already have reached the sensor, it could be on our fabric: ask it to leave.
static void clean_up_failed_commissioning(NodeId node, CommissioningStage stage)
{
    if (stage < CommissioningStage::kWiFiNetworkSetup) {
        return; // the SDK already disarmed the fail-safe and released the session
    }
    // Not from inside the SDK's callback: StopPairing changes the state it is still walking through.
    s_cleanup_node = node;
    s_cleanup_unpair = stage == CommissioningStage::kSendComplete || stage == CommissioningStage::kICDSendStayActive;
    LogErrorOnFailure(DeviceLayer::PlatformMgr().ScheduleWork(clean_up_failed_commissioning_later));
}

esp_err_t sensor_link_pair(const pairing_code_t *code)
{
    int slot;
    uint64_t node;
    sensor_mgr_err_t err;
    {
        AppStateLock m;
        err = sensor_mgr_begin_pairing(m.get(), &slot, &node);
        if (err == SENSOR_MGR_OK) {
            app_state_save(m.get()); // the node ID is used up even if pairing fails
        }
    }
    if (err == SENSOR_MGR_NOT_READY) {
        printf("The Thread network isn't up yet; try again in a few seconds.\n");
        return ESP_ERR_INVALID_STATE;
    }
    if (err == SENSOR_MGR_BUSY) {
        printf("Already pairing; wait for it to finish.\n");
        return ESP_ERR_INVALID_STATE;
    }
    if (err == SENSOR_MGR_FULL) {
        printf("All %d slots are used; remove a sensor first.\n", SLOT_COUNT);
        return ESP_ERR_NO_MEM;
    }

    s_pairing_active = true;
    s_pairing_node = node;
    // Before PairDevice, which may already report a result.
    DeviceLayer::SystemLayer().StartTimer(kPairingTimeout, pairing_timeout, nullptr);
    Thread::OperationalDataset dataset;
    CHIP_ERROR chip_err = DeviceLayer::ThreadStackMgr().GetThreadProvision(dataset);
    if (chip_err == CHIP_NO_ERROR) {
        CommissioningParameters params = CommissioningParameters().SetThreadOperationalDataset(dataset.AsByteSpan());
        commissioner()->RegisterPairingDelegate(&s_pairing_delegate);
        chip_err = commissioner()->PairDevice(node, code->digits, params, DiscoveryType::kAll);
    }
    if (chip_err != CHIP_NO_ERROR) {
        finish_pairing(false, "couldn't start", chip_err);
        return ESP_FAIL;
    }
    printf("Pairing into slot %d (node 0x%" PRIx64 "). The sensor must be in pairing mode; see the log for the "
           "result.\n", slot + 1, node);
    return ESP_OK;
}

// ---------------------------------------------------------------------------------------------------------------
// Remove, identify, factory reset

// Factory reset: sensors asked to leave our fabric that haven't answered yet (0 = none).
static NodeId s_leaving[SLOT_COUNT];
static bool s_factory_reset_pending;

static void do_factory_reset(System::Layer *, void *)
{
    esp_matter::factory_reset(); // erases NVS (Matter fabric, Thread dataset, our slots) and restarts
}

static void reboot_if_all_left()
{
    for (NodeId node : s_leaving) {
        if (node != kUndefinedNodeId) {
            return;
        }
    }
    printf("Every sensor has answered. Erasing all settings and rebooting...\n");
    DeviceLayer::SystemLayer().CancelTimer(do_factory_reset, nullptr);
    // A moment for the last acknowledgement to go out and the log to flush.
    DeviceLayer::SystemLayer().StartTimer(System::Clock::Seconds32(1), do_factory_reset, nullptr);
}

static void unpaired(NodeId node, CHIP_ERROR status)
{
    if (status == CHIP_NO_ERROR) {
        ESP_LOGI(TAG, "Node 0x%" PRIx64 " left our fabric", node);
    } else {
        ESP_LOGW(TAG, "Node 0x%" PRIx64 " couldn't be told to leave (%" CHIP_ERROR_FORMAT "); factory reset it by hand "
                 "before pairing it again", node, status.Format());
    }
    if (s_factory_reset_pending) {
        for (NodeId &leaving : s_leaving) {
            if (leaving == node) {
                leaving = kUndefinedNodeId;
            }
        }
        reboot_if_all_left();
    }
}

// Asks a sensor to leave our fabric; the answer arrives in unpaired(). False if the request couldn't be sent.
static bool request_unpair(NodeId node)
{
    esp_err_t err = matter_controller_client::get_instance().unpair(node, unpaired);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Node 0x%" PRIx64 " couldn't be asked to leave our fabric (%s); factory reset it by hand before "
                 "pairing it again", node, esp_err_to_name(err));
        return false;
    }
    return true;
}

esp_err_t sensor_link_remove(int slot)
{
    uint64_t node;
    sensor_mgr_err_t err;
    {
        AppStateLock m;
        err = sensor_mgr_remove(m.get(), slot, &node);
        if (err == SENSOR_MGR_OK) {
            app_state_save(m.get());
        }
    }
    if (err != SENSOR_MGR_OK) {
        printf("Slot %d is empty.\n", slot + 1);
        return ESP_ERR_NOT_FOUND;
    }
    s_subs[slot].reset();
    printf("Slot %d is free. Asking node 0x%" PRIx64 " to leave our fabric; see the log for its answer.\n", slot + 1,
           node);
    request_unpair(node);
    return ESP_OK;
}

esp_err_t sensor_link_identify(int slot)
{
    uint64_t node = 0;
    {
        AppStateLock m;
        if (m->table.slot[slot].used) {
            node = m->table.slot[slot].node_id;
        }
    }
    if (node == 0) {
        printf("Slot %d is empty.\n", slot + 1);
        return ESP_ERR_NOT_FOUND;
    }
    char args[24];
    snprintf(args, sizeof(args), "{\"0:U16\": %u}", kIdentifySeconds);
    esp_err_t err = esp_matter::controller::send_invoke_cluster_command(
        node, kSensorEndpoint, Clusters::Identify::Id, Clusters::Identify::Commands::Identify::Id, args);
    if (err != ESP_OK) {
        printf("Couldn't send Identify to slot %d: %s\n", slot + 1, esp_err_to_name(err));
        return err;
    }
    // Connection failures are only logged by esp-matter.
    printf("Identify sent to slot %d. The sensor blinks when it wakes up, if it supports Identify; errors show in the "
           "log.\n", slot + 1);
    return ESP_OK;
}

void sensor_link_factory_reset()
{
    if (s_factory_reset_pending) {
        printf("A factory reset is already under way.\n");
        return;
    }
    if (s_pairing_active) {
        NodeId node = s_pairing_node;
        finish_pairing(false, "cancelled by factory-reset", CHIP_ERROR_CANCELLED);
        LogErrorOnFailure(commissioner()->StopPairing(node));
    }
    NodeId nodes[SLOT_COUNT];
    int n = 0;
    {
        AppStateLock m;
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (m->table.slot[i].used) {
                nodes[n++] = m->table.slot[i].node_id;
            }
        }
        sensor_mgr_clear_all(m.get());
        app_state_save(m.get());
    }
    for (auto &sub : s_subs) {
        sub.reset();
    }
    s_factory_reset_pending = true;
    for (int i = 0; i < n; i++) {
        s_leaving[i] = nodes[i];
    }
    int asked = 0;
    for (int i = 0; i < n; i++) {
        if (request_unpair(nodes[i])) {
            asked++;
        } else {
            s_leaving[i] = kUndefinedNodeId;
        }
    }
    printf("Asked %d of %d sensor(s) to leave our fabric. Sleepy sensors answer when they wake up; rebooting once all "
           "have answered, or after %" PRIu32 " s.\n", asked, n, kFactoryResetMaxWait.count());
    DeviceLayer::SystemLayer().StartTimer(kFactoryResetMaxWait, do_factory_reset, nullptr);
    reboot_if_all_left();
}
