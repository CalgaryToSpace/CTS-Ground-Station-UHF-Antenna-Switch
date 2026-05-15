/*
 * CTS Ground Station RX/TX UHF Switch Firmware
 * Target: RP2040-Zero
 *
 * Responsibilities:
 *   - Drive K1 (RX/TX path) and K2 (dummy load) relays based on a selected mode.
 *   - Mode is set by a 1-hot 3-position switch: Auto / Force TX / Force RX.
 *   - In Auto mode, sense TX activity via an ADC-coupled detector.
 *   - Provide USB-CDC serial logging with runtime-selectable verbosity.
 *
 * Usage Notes:
 *   - Press keys 0, 1, 2 to set log level (0 = default and least verbose, 3 = most verbose).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// ---------------------------------------------------------------------------
// Pin map
//
// Values are the x in the schematic's GPx pin labels.
// ---------------------------------------------------------------------------
#define PIN_STATUS_LED          0

#define PIN_MODE_AUTO           6
#define PIN_MODE_FORCE_TX       7
#define PIN_MODE_FORCE_RX       8

#define PIN_TX_DETECT_ADC       26   // ADC0
#define ADC_TX_DETECT_CHANNEL   0

#define PIN_TX_EN_RELAY_K1      28   // RX/TX path relay   (LOW = RX path)
#define PIN_TX_EN_RELAY_K2      27   // Dummy load relay   (LOW = dummy applied)

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

// TODO: After testing, reduce this to 10-30 ms.
// How long to wait before switching the relay after TX detection.
// Very critical that this is long enough for the relay to by fully switched before moving on.
// Datasheet: https://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocNm=108-98000&DocType=SS&DocLang=EN
static const uint32_t RELAY_SWITCH_DELAY_MS = 250;

// TX detector threshold. ADC reads ~0 V nominally, ~300 mV when sensing TX.
// 3.3 V reference, 12-bit ADC -> 1 LSB = 0.806 mV.
// 150 mV midpoint -> ~186 counts. Use 150 to be safely above noise floor
// while still catching the 300 mV active level.
static const uint32_t TX_DETECT_THRESHOLD_RAW = 150;

// How often Auto-mode samples the detector.
static const uint32_t AUTO_POLL_INTERVAL_US = 100;

// Status LED heartbeat behaviour at idle (in RX state).
static const uint32_t HEARTBEAT_PERIOD_MS = 1000;
static const uint32_t HEARTBEAT_ON_TIME_MS = 100;

// At LOG_TRACE, dump ADC value at this interval (don't flood the link).
static const uint32_t ADC_TRACE_INTERVAL_MS = 100;

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
typedef enum {
    LOG_NONE  = 0,   // Boot banner + critical only
    LOG_INFO  = 1,   // Mode/state transitions
    LOG_DEBUG = 2,   // + periodic state
    LOG_TRACE = 3,   // + continuous ADC stream
} log_level_t;

static volatile log_level_t g_log_level = LOG_NONE;

#define LOG(level, fmt, ...) \
    do { \
        if (g_log_level >= (level)) { \
            printf("[%lu] " fmt "\n", (unsigned long)to_ms_since_boot(get_absolute_time()), ##__VA_ARGS__); \
        } \
    } while (0)

// Always-printed (boot banner, etc.).
#define LOG_ALWAYS(fmt, ...) \
    printf("[%lu] " fmt "\n", (unsigned long)to_ms_since_boot(get_absolute_time()), ##__VA_ARGS__)

// ---------------------------------------------------------------------------
// Mode + RF state
// ---------------------------------------------------------------------------
typedef enum {
    MODE_AUTO,
    MODE_FORCE_TX,
    MODE_FORCE_RX,
    MODE_INVALID,    // No switch position asserted (shouldn't happen with 1-hot).
} mode_t;

typedef enum {
    RF_RX,           // K1 = RX path,  K2 = dummy load on TX line.
    RF_TX,           // K1 = TX path,  K2 = TX line passthrough.
} rf_state_t;

static const char *mode_name(mode_t m) {
    switch (m) {
        case MODE_AUTO:     return "AUTO";
        case MODE_FORCE_TX: return "FORCE_TX";
        case MODE_FORCE_RX: return "FORCE_RX";
        default:            return "INVALID";
    }
}

static const char *rf_state_name(rf_state_t s) {
    return (s == RF_TX) ? "TX" : "RX";
}

// ---------------------------------------------------------------------------
// GPIO / ADC helpers
// ---------------------------------------------------------------------------
static void relays_set(rf_state_t state) {
    // K1 LOW = RX path,  K1 HIGH = TX path.
    // K2 LOW = dummy applied,  K2 HIGH = TX passthrough.
    // Sequencing: when entering TX, flip K1 first then K2 (after delay).
    //             when entering RX, flip K2 first then K1 (after delay).
    if (state == RF_TX) {
        gpio_put(PIN_TX_EN_RELAY_K1, 1);
        sleep_ms(RELAY_SWITCH_DELAY_MS);
        gpio_put(PIN_TX_EN_RELAY_K2, 1);
        sleep_ms(RELAY_SWITCH_DELAY_MS); // Critical: Wait for the second relay to stabilize too!
    } else {
        gpio_put(PIN_TX_EN_RELAY_K2, 0);
        sleep_ms(RELAY_SWITCH_DELAY_MS);
        gpio_put(PIN_TX_EN_RELAY_K1, 0);
        sleep_ms(RELAY_SWITCH_DELAY_MS); // Critical: Wait for the second relay to stabilize too!
    }
}

/// Read the 1-hot SP3T switch and return the corresponding mode.
static mode_t read_mode_switch(void) {
    // 1-hot, active-high (pull-downs hold unselected pins low).
    const bool a = gpio_get(PIN_MODE_AUTO);
    const bool t = gpio_get(PIN_MODE_FORCE_TX);
    const bool r = gpio_get(PIN_MODE_FORCE_RX);

    if (a && !t && !r) return MODE_AUTO;
    if (!a && t && !r) return MODE_FORCE_TX;
    if (!a && !t && r) return MODE_FORCE_RX;
    return MODE_INVALID;
}

/// Read the ADC input for TX detect. Returns the raw ADC value (0-4095).
///
/// Averages 8 samples.
static uint16_t adc_read_tx_detect(void) {
    adc_select_input(ADC_TX_DETECT_CHANNEL);

    uint sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += adc_read();
    }
    return sum / 8;
}

// Returns true if TX sensed.
// Kept as a single function so it can later be swapped for a PTT GPIO read
// without touching the state machine, if desired.
static bool tx_sensed(void) {
    const uint16_t raw = adc_read_tx_detect();

    if (raw >= TX_DETECT_THRESHOLD_RAW) {
        LOG(LOG_DEBUG, "TX Sensed! tx_sensed: raw=%d", raw);
        return true;
    }

    return false;
}


/// Serial input -> log level.
/// Parses a single digit (0-3) from the serial input and sets that as the log level.
static void poll_serial_input(void) {
    const int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    if (c >= '0' && c <= '3') {
        log_level_t new_level = (log_level_t)(c - '0');
        g_log_level = new_level;
        LOG_ALWAYS("Log level set to %d", new_level);
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
static void setup_gpio(void) {
    gpio_init(PIN_STATUS_LED);
    gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);
    gpio_put(PIN_STATUS_LED, 0);

    // Mode switch inputs with internal pull-downs.
    const uint mode_pins[] = { PIN_MODE_AUTO, PIN_MODE_FORCE_TX, PIN_MODE_FORCE_RX };
    for (size_t i = 0; i < sizeof(mode_pins) / sizeof(mode_pins[0]); i++) {
        gpio_init(mode_pins[i]);
        gpio_set_dir(mode_pins[i], GPIO_IN);
        gpio_pull_down(mode_pins[i]);
    }

    // Relay outputs, default LOW (= RX + dummy applied = safe nominal state).
    gpio_init(PIN_TX_EN_RELAY_K1);
    gpio_set_dir(PIN_TX_EN_RELAY_K1, GPIO_OUT);
    gpio_put(PIN_TX_EN_RELAY_K1, 0);

    gpio_init(PIN_TX_EN_RELAY_K2);
    gpio_set_dir(PIN_TX_EN_RELAY_K2, GPIO_OUT);
    gpio_put(PIN_TX_EN_RELAY_K2, 0);
}

static void setup_adc(void) {
    adc_init();
    adc_gpio_init(PIN_TX_DETECT_ADC);
    adc_select_input(ADC_TX_DETECT_CHANNEL);
}

bool status_led_state = false;
static void set_status_led(bool state) {
    if (state == status_led_state) {
        return;
    }

    LOG(LOG_DEBUG, "set_status_led: %d -> %d", status_led_state, state);

    status_led_state = state;
    gpio_put(PIN_STATUS_LED, state);
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------
int main(void) {
    stdio_init_all();
    setup_gpio();
    setup_adc();

    // Give USB-CDC a moment to enumerate so the user sees the banner.
    sleep_ms(1500);

    LOG_ALWAYS("===============================================");
    LOG_ALWAYS("CTS UHF RX/TX Switch booting...");
    LOG_ALWAYS("Send '0'..'3' to set log level (default = 0)");
    LOG_ALWAYS("  0 = none, 1 = info, 2 = debug, 3 = trace");
    LOG_ALWAYS("===============================================");

    rf_state_t  rf_state    = RF_RX;
    mode_t      last_mode   = MODE_INVALID;
    uint32_t    last_adc_value_trace_time  = 0;

    // Force initial state on the relays.
    relays_set(RF_RX);

    while (true) {
        poll_serial_input();

        const uint32_t now = to_ms_since_boot(get_absolute_time());
        const mode_t desired_mode = read_mode_switch();

        if (desired_mode != last_mode) {
            LOG(LOG_INFO, "Mode -> %s", mode_name(desired_mode));
            last_mode = desired_mode;
        }

        // Decide desired RF state from mode.
        rf_state_t desired_rf_state = rf_state;
        switch (desired_mode) {
            case MODE_FORCE_TX:
                desired_rf_state = RF_TX;
                break;
            case MODE_FORCE_RX:
                desired_rf_state = RF_RX;
                break;
            case MODE_AUTO:
                desired_rf_state = tx_sensed() ? RF_TX : RF_RX;
                break;
            case MODE_INVALID:
            default:
                // Safe fallback: drop to RX.
                {
                    desired_rf_state = RF_RX;
                    LOG(LOG_INFO, "Invalid mode (>1-hot) -> %s. Fallback to RX.", mode_name(desired_mode));
                }
                break;
        }

        if (desired_rf_state != rf_state) {
            LOG(LOG_DEBUG, "Starting RF %s -> %s", rf_state_name(rf_state), rf_state_name(desired_rf_state));
            relays_set(desired_rf_state);
            LOG(LOG_INFO, "Done RF %s -> %s", rf_state_name(rf_state), rf_state_name(desired_rf_state));
            rf_state = desired_rf_state;
        }

        // Trace-level continuous ADC dump (rate-limited so logs stay readable).
        if (g_log_level >= LOG_TRACE && (now - last_adc_value_trace_time) >= ADC_TRACE_INTERVAL_MS) {
            uint16_t raw = adc_read_tx_detect();
            uint32_t mV  = (raw * 3300u) / 4095u;
            LOG(
                LOG_TRACE,
                "ADC: now_ms=%lu raw=%u mV=%lu state=%s mode=%s",
                (unsigned long)now, raw, (unsigned long)mV,
                rf_state_name(rf_state), mode_name(desired_mode)
            );
            last_adc_value_trace_time = now;
        }

        // Heartbeat LED: solid when TX, short-pulse blinking when RX.
        if (rf_state == RF_TX) {
            set_status_led(true);
        }
        else {
            set_status_led((now % HEARTBEAT_PERIOD_MS) < HEARTBEAT_ON_TIME_MS);
        }

        sleep_us(AUTO_POLL_INTERVAL_US);
    }
}
