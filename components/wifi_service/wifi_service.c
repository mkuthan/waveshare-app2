#include <inttypes.h>
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "app_events.h"
#include "wifi_service.h"

static const char *TAG = "wifi_service";

// A failed attempt already costs 10-15 s of auth/assoc timeouts, and an AP that is steering or
// rate limiting clients refuses the immediate retry as well. So space attempts out and bound the
// whole run by time rather than by count: what matters is that a later attempt gets through.
#define WIFI_RETRY_DELAY_MS  5000
#define WIFI_RETRY_WINDOW_MS 60000
#define STATUS_POST_TICKS    0 // never block the event loop task on its own queue

// Credentials live in our own namespace, written only once a link is actually up, so a mistyped
// passphrase fails without ever being kept for the next boot. Plaintext NVS: enable
// CONFIG_NVS_ENCRYPTION if a flash dump must not reveal the PSK.
#define CRED_NVS_NAMESPACE "wifi_svc"
#define CRED_NVS_KEY       "cred"

// Private to this service, so the reconnect call runs on the event loop task like every other
// state change here. app_events.h stays the only cross-component contract.
ESP_EVENT_DEFINE_BASE(WIFI_SERVICE_BASE);
enum { WIFI_SERVICE_EV_RETRY };

// The four situations this service distinguishes, in the one variable that describes them; every
// transition below writes exactly one. Zero init means ST_IDLE, which is what a boot without saved
// credentials needs.
enum {
    ST_IDLE,       // nothing requested: boot, or the user disconnected
    ST_ATTEMPT,    // connect() issued, the driver may still report on this attempt
    ST_RETRY_WAIT, // the attempt failed and was counted, the retry timer is armed
    ST_LINKED,     // got an IP
};

static esp_timer_handle_t s_retry_timer;
static int64_t s_sequence_start_us; // start of the current run of attempts
static int s_attempt_reason;        // first reason the driver gave for the latest attempt
static int s_state;
// Replacing a live link or an in-flight attempt emits a DISCONNECTED we caused ourselves; without
// this flag the retry logic would treat it as a drop and race the new connect. It only swallows
// WIFI_REASON_ASSOC_LEAVE (what esp_wifi_disconnect() reports) and is cleared on GOT_IP, so a
// flag left over because no event followed cannot hide a genuine failure such as a bad password.
static bool s_ignore_disc;

static void post_status(int32_t id, const void *data, size_t size)
{
    esp_err_t err = esp_event_post(SERVICE_STATUS_BASE, id, data, size, STATUS_POST_TICKS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "status %" PRId32 " dropped: %s", id, esp_err_to_name(err));
    }
}

static void post_failed(int reason)
{
    post_status(STATUS_WIFI_FAILED, &reason, sizeof(reason));
}

static void post_error(esp_err_t err)
{
    post_status(STATUS_WIFI_ERROR, &err, sizeof(err));
}

// Fail closed: open networks, empty SSIDs and unterminated passphrases are rejected.
static bool req_to_config(const wifi_connect_req_t *req, wifi_config_t *cfg)
{
    size_t ssid_len = strnlen(req->ssid, sizeof(req->ssid));
    size_t pass_len = strnlen(req->password, sizeof(req->password));
    if (ssid_len == 0 || pass_len < WIFI_PASS_MIN_LEN || pass_len >= sizeof(req->password)) {
        return false;
    }
    memset(cfg, 0, sizeof(*cfg));
    memcpy(cfg->sta.ssid, req->ssid, ssid_len);
    memcpy(cfg->sta.password, req->password, pass_len);
    cfg->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg->sta.pmf_cfg.capable = true;
    cfg->sta.pmf_cfg.required = false;
    cfg->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    return true;
}

// Only called once the station got an IP, which is the one moment we know the passphrase works.
// Rewriting an unchanged entry would churn flash on every rejoin, hence the compare.
static void save_credentials(void)
{
    wifi_config_t cfg;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read config: %s", esp_err_to_name(err));
        return;
    }
    wifi_connect_req_t req = {0};
    size_t ssid_len = strnlen((const char *)cfg.sta.ssid, sizeof(cfg.sta.ssid));
    size_t pass_len = strnlen((const char *)cfg.sta.password, sizeof(cfg.sta.password));
    if (ssid_len > sizeof(req.ssid) || pass_len > sizeof(req.password)) {
        explicit_bzero(&cfg, sizeof(cfg));
        return; // never store a truncated passphrase
    }
    memcpy(req.ssid, cfg.sta.ssid, ssid_len);
    memcpy(req.password, cfg.sta.password, pass_len);
    explicit_bzero(&cfg, sizeof(cfg));

    nvs_handle_t h;
    err = nvs_open(CRED_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        explicit_bzero(&req, sizeof(req));
        ESP_LOGW(TAG, "open credentials: %s", esp_err_to_name(err));
        return;
    }
    wifi_connect_req_t stored = {0};
    size_t len = sizeof(stored);
    bool same = nvs_get_blob(h, CRED_NVS_KEY, &stored, &len) == ESP_OK && len == sizeof(stored) &&
                memcmp(&stored, &req, sizeof(req)) == 0;
    explicit_bzero(&stored, sizeof(stored));
    if (!same) {
        err = nvs_set_blob(h, CRED_NVS_KEY, &req, sizeof(req));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
    }
    nvs_close(h);
    explicit_bzero(&req, sizeof(req));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save credentials: %s", esp_err_to_name(err));
    } else if (!same) {
        ESP_LOGI(TAG, "saved credentials for the next boot");
    }
}

static bool load_credentials(wifi_connect_req_t *req)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CRED_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false; // nothing saved yet: a first boot
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open credentials: %s", esp_err_to_name(err));
        return false;
    }
    size_t len = sizeof(*req);
    err = nvs_get_blob(h, CRED_NVS_KEY, req, &len);
    nvs_close(h);
    if (err == ESP_OK && len == sizeof(*req)) {
        return true;
    }
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "saved credentials have the wrong size");
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "read credentials: %s", esp_err_to_name(err));
    }
    explicit_bzero(req, sizeof(*req));
    return false;
}

static void stop_retry_timer(void)
{
    esp_err_t err = esp_timer_stop(s_retry_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { // INVALID_STATE: it was not armed
        ESP_LOGW(TAG, "stop retry timer: %s", esp_err_to_name(err));
    }
}

// A new run of attempts: fresh time window, and no retry left over from the previous one.
static void begin_sequence(void)
{
    stop_retry_timer();
    s_sequence_start_us = esp_timer_get_time();
}

// Applies the config and starts an attempt. Takes ownership of cfg and scrubs it.
static void start_attempt(wifi_config_t *cfg)
{
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, cfg);
    explicit_bzero(cfg, sizeof(*cfg));
    if (err == ESP_OK) {
        s_state = ST_ATTEMPT;
        begin_sequence();
        post_status(STATUS_WIFI_CONNECTING, NULL, 0);
        err = esp_wifi_connect();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect: %s", esp_err_to_name(err));
        s_state = ST_IDLE;
        post_error(err);
    }
}

// One attempt failed: wait for the next one, or stop trying. The reason reported on give-up is the
// first one the driver sent for that attempt; it follows up with a generic
// WIFI_REASON_CONNECTION_FAIL that says nothing about why the AP refused us.
static void retry_or_give_up(int reason)
{
    if (s_state == ST_IDLE) {
        return; // cancelled while the failure was in flight
    }
    if (esp_timer_get_time() - s_sequence_start_us >= (int64_t)WIFI_RETRY_WINDOW_MS * 1000) {
        s_state = ST_IDLE;
        ESP_LOGW(TAG, "gave up after %d s (reason %d)", WIFI_RETRY_WINDOW_MS / 1000, reason);
        post_failed(reason);
        return;
    }
    ESP_LOGW(TAG, "retry in %d s (reason %d)", WIFI_RETRY_DELAY_MS / 1000, reason);
    post_status(STATUS_WIFI_CONNECTING, NULL, 0);
    s_state = ST_RETRY_WAIT;
    esp_err_t err = esp_timer_start_once(s_retry_timer, (uint64_t)WIFI_RETRY_DELAY_MS * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start retry timer: %s", esp_err_to_name(err));
        s_state = ST_IDLE;
        post_error(err);
    }
}

// Only hands the delay over to the event loop: a full queue for a whole second means the loop is
// gone, and nothing here can fix that.
static void on_retry_timer(void *arg)
{
    esp_err_t err = esp_event_post(WIFI_SERVICE_BASE, WIFI_SERVICE_EV_RETRY, NULL, 0, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "retry event lost: %s", esp_err_to_name(err));
    }
}

static void on_retry_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (s_state != ST_RETRY_WAIT) {
        return; // cancelled, or a new attempt already took over
    }
    s_state = ST_ATTEMPT;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        // Over esp_hosted the previous attempt may still be settling when the timer fires, so a
        // refused connect is not fatal: the next retry is a few seconds away.
        ESP_LOGW(TAG, "connect: %s", esp_err_to_name(err));
        retry_or_give_up(s_attempt_reason);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *evt = data;
        if (s_ignore_disc && evt->reason == WIFI_REASON_ASSOC_LEAVE) {
            s_ignore_disc = false;
            return;
        }
        if (s_state == ST_IDLE) {
            return;
        }
        if (s_state == ST_RETRY_WAIT) {
            // The attempt is already counted: the driver reports a failed one twice (the
            // auth/assoc reason, then a generic WIFI_REASON_CONNECTION_FAIL once it settles) and
            // no new connect was issued in between.
            ESP_LOGD(TAG, "follow-up disconnect (reason %d) ignored", evt->reason);
            return;
        }
        if (s_state == ST_LINKED) {
            ESP_LOGW(TAG, "link lost (reason %d)", evt->reason);
            begin_sequence(); // attempts after a working link get their own window
        }
        s_attempt_reason = evt->reason;
        retry_or_give_up(evt->reason);
        break;
    }
    default:
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    const ip_event_got_ip_t *evt = data;
    s_state = ST_LINKED;
    s_ignore_disc = false; // the old link's DISCONNECTED precedes this event on the same loop
    post_status(STATUS_WIFI_CONNECTED, &evt->ip_info.ip, sizeof(evt->ip_info.ip));
    save_credentials();
}

static void handle_connect(const wifi_connect_req_t *req)
{
    wifi_config_t cfg;
    if (!req_to_config(req, &cfg)) {
        ESP_LOGW(TAG, "rejected connect request (empty SSID or passphrase not 8-63 chars)");
        post_status(STATUS_WIFI_REJECTED, NULL, 0);
        return;
    }

    // Tear down the live link or the attempt in flight before reconfiguring; a retry still waiting
    // in the timer has nothing on the air, and begin_sequence() cancels it. Only expect an event
    // for the teardown if the disconnect was accepted.
    if (s_state == ST_ATTEMPT || s_state == ST_LINKED) {
        esp_err_t derr = esp_wifi_disconnect();
        s_ignore_disc = (derr == ESP_OK);
        if (derr != ESP_OK) {
            ESP_LOGW(TAG, "disconnect before reconnect: %s", esp_err_to_name(derr));
        }
    }
    start_attempt(&cfg);
}

static void on_ui_action(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case UI_ACTION_WIFI_CONNECT:
        if (data) {
            handle_connect(data);
            // `data` is the event loop's own heap copy of the request, shared with the other
            // handlers (none of which use the passphrase) and freed unscrubbed after dispatch.
            explicit_bzero(data, sizeof(wifi_connect_req_t));
        }
        break;
    case UI_ACTION_WIFI_DISCONNECT:
        s_state = ST_IDLE;
        stop_retry_timer();
        esp_wifi_disconnect();
        post_status(STATUS_WIFI_DISCONNECTED, NULL, 0);
        break;
    default:
        break; // audio actions are not ours
    }
}

// Rejoin the network that last produced an IP. The request is replayed through req_to_config(),
// so the security settings in force now apply, not whatever was current when it was saved.
static void connect_saved(void)
{
    wifi_connect_req_t req = {0};
    if (!load_credentials(&req)) {
        return; // first boot: wait for the UI
    }
    wifi_config_t cfg;
    bool usable = req_to_config(&req, &cfg);
    ESP_LOGI(TAG, "rejoining saved ssid=\"%.32s\"", req.ssid);
    explicit_bzero(&req, sizeof(req));
    if (!usable) {
        ESP_LOGW(TAG, "saved ssid no longer meets the rules, ignoring it");
        return;
    }
    start_attempt(&cfg);
}

esp_err_t wifi_service_init(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta(), ESP_FAIL, TAG, "create sta netif");

    const esp_timer_create_args_t timer_args = {
        .callback = on_retry_timer,
        .name = "wifi_retry",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_retry_timer), TAG, "create retry timer");

    // esp_wifi_remote forwards these calls to the ESP32-C6 over SDIO (esp_hosted)
    const wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "wifi init");
    // The driver keeps nothing on its side: credentials are saved by save_credentials() after a
    // link came up, which is why a mistyped passphrase is never written. esp_wifi_remote forwards
    // this setting to the C6.
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL), TAG, "reg wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip_event, NULL), TAG, "reg ip");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(UI_ACTION_BASE, ESP_EVENT_ANY_ID, on_ui_action, NULL), TAG, "reg ui");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_SERVICE_BASE, WIFI_SERVICE_EV_RETRY, on_retry_event, NULL), TAG, "reg retry");

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    connect_saved();
    return ESP_OK;
}
