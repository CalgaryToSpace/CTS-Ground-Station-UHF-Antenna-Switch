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
 *   - Press keys 0/1/2/3 to set log level (0 = default and least verbose, 3 = most verbose).
 *   - Press 't'/'r'/'a' to force TX/RX/Auto, 's' to revert to the physical slide switch.
 *
 * Heartbeat (LOG_INFO, ~2.5 s):
 *   Heartbeat: state=<RX|TX> mode=<AUTO|FORCE_TX|FORCE_RX|INVALID> last_tx_ago_s=<sec.ms|never>
 *   last_tx_ago_s is the time since Auto mode last sensed TX, or "never"
 *   until the first detection.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"

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

// How long to wait before switching the relay after TX detection.
// Very critical that this is long enough for the relay to by fully switched before moving on.
// Datasheet (HF3-1): https://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocNm=108-98000&DocType=SS&DocLang=EN
static const uint32_t RELAY_SWITCH_DELAY_MS = 12;

// TX detector threshold. ADC reads 250-300mV nominally, >=400 mV when sensing TX (at >=-l6 dBm).
// 3.3 V reference, 12-bit ADC -> 1 LSB = 0.806 mV.
// Threshold: 400 mV -> 496 raw counts.
// Datasheet (LTC5507ES6): https://www.analog.com/media/en/technical-documentation/data-sheets/5507f.pdf
static const uint32_t TX_DETECT_THRESHOLD_RAW = 496;

// How often Auto-mode samples the detector.
static const uint32_t AUTO_POLL_INTERVAL_US = 100;

// Status LED heartbeat behaviour at idle (in RX state).
static const uint32_t HEARTBEAT_PERIOD_MS = 1000;
static const uint32_t HEARTBEAT_ON_TIME_MS = 100;

// At LOG_TRACE, dump ADC value at this interval (don't flood the link).
static const uint32_t ADC_TRACE_INTERVAL_MS = 100;

// At LOG_INFO or higher, print a heartbeat message.
static const uint32_t INFO_HEARTBEAT_MESSAGE_INTERVAL_MS = 2500;

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
typedef enum {
    LOG_ALWAYS = 0,   // Boot banner + critical only
    LOG_INFO   = 1,   // Mode/state transitions
    LOG_DEBUG  = 2,   // + periodic state
    LOG_TRACE  = 3,   // + continuous ADC stream
} log_level_t;

static volatile log_level_t g_log_level = LOG_ALWAYS;

static const char *log_level_name(log_level_t level) {
    switch (level) {
        case LOG_ALWAYS:  return "ALWAYS";
        case LOG_INFO:  return " INFO ";
        case LOG_DEBUG: return "DEBUG ";
        case LOG_TRACE: return "TRACE ";
    }
    return "UNKOWN";
}

#define LOG(level, fmt, ...) \
    do { \
        if (g_log_level >= (level)) { \
            printf("[%lu] [%s] " fmt "\n", (unsigned long)to_ms_since_boot(get_absolute_time()), log_level_name(level), ##__VA_ARGS__); \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// Mode + RF state
// ---------------------------------------------------------------------------
typedef enum {
    MODE_AUTO,
    MODE_FORCE_TX,
    MODE_FORCE_RX,
    MODE_INVALID,    // No switch position asserted (shouldn't happen with 1-hot).
} mode_t;

// MODE_INVALID means "no serial override; use the physical switch".
static volatile mode_t g_serial_mode_override = MODE_INVALID;

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

static const char *mode_name_sourced(mode_t m, bool from_serial) {
    if (from_serial) {
        switch (m) {
            case MODE_AUTO:     return "AUTO_BY_SERIAL";
            case MODE_FORCE_TX: return "FORCE_TX_BY_SERIAL";
            case MODE_FORCE_RX: return "FORCE_RX_BY_SERIAL";
            default:            return "INVALID";
        }
    } else {
        switch (m) {
            case MODE_AUTO:     return "AUTO_BY_SWITCH";
            case MODE_FORCE_TX: return "FORCE_TX_BY_SWITCH";
            case MODE_FORCE_RX: return "FORCE_RX_BY_SWITCH";
            default:            return "INVALID";
        }
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

static void print_help(void) {
    LOG(LOG_ALWAYS, "================= HELP ========================");
    LOG(LOG_ALWAYS, "Send '0'/'1'/'2'/'3' to set log level (default = 0)");
    LOG(LOG_ALWAYS, "  0 = nearly none, 1 = info, 2 = debug, 3 = trace");
    LOG(LOG_ALWAYS, "Send 't'/'r'/'a' to force TX/RX/Auto, 's' to go back to using the slide switch (default = slide switch)");
    LOG(LOG_ALWAYS, "Send 'f' to reboot into flash bootloader mode");
    LOG(LOG_ALWAYS, "Send 'x' to reboot");
    LOG(LOG_ALWAYS, "Send 'h' to print this help message");
    LOG(LOG_ALWAYS, "===============================================");
}

/// Parse single-character serial input commands and execute/handle them.
static void poll_serial_input(void) {
    const int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    if (c >= '0' && c <= '3') {
        log_level_t new_level = (log_level_t)(c - '0');
        g_log_level = new_level;
        LOG(LOG_ALWAYS, "Log level set to %d", new_level);
    } else if (c == 't') {
        g_serial_mode_override = MODE_FORCE_TX;
        LOG(LOG_ALWAYS, "Serial override -> FORCE_TX");
    } else if (c == 'r') {
        g_serial_mode_override = MODE_FORCE_RX;
        LOG(LOG_ALWAYS, "Serial override -> FORCE_RX");
    } else if (c == 'a') {
        g_serial_mode_override = MODE_AUTO;
        LOG(LOG_ALWAYS, "Serial override -> AUTO");
    } else if (c == 's') {
        g_serial_mode_override = MODE_INVALID;
        LOG(LOG_ALWAYS, "Serial override cleared, using slide switch");
    } else if (c == 'h') {
        print_help();
    } else if (c == 'f') {
        LOG(LOG_ALWAYS, "Rebooting into flash bootloader mode...");
        sleep_ms(1000);
        reset_usb_boot(0, 0);
    } else if (c == 'x') {
        LOG(LOG_ALWAYS, "Rebooting...");
        sleep_ms(1000);
        watchdog_reboot(0, 0, 0);  // Triggers an immediate watchdog reset -> firmware restart.
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
    sleep_ms(500);

    LOG(LOG_ALWAYS, "===============================================");
    LOG(LOG_ALWAYS, "CTS UHF RX/TX Switch booting...");
    LOG(LOG_ALWAYS, "===============================================");
    print_help();

    rf_state_t  rf_state              = RF_RX;
    mode_t      last_mode             = MODE_INVALID;
    bool        last_mode_from_serial = false;
    uint32_t    last_adc_value_trace_time  = 0;
    uint32_t    last_info_heartbeat_time = 0;
    // Uptime (ms) when Auto mode last sensed TX. 0 = never sensed.
    // Reported by the heartbeat as seconds-since so a ground-station
    // host can confirm the switch actually saw its uplink burst.
    uint32_t    last_tx_detect_ms = 0;

    // Force initial state on the relays.
    relays_set(RF_RX);

    // [WATCHDOG] Detect if we woke from a watchdog reset and log it.
    if (watchdog_caused_reboot()) {
        LOG(LOG_ALWAYS, "*** WATCHDOG RESET DETECTED ***");
    }

    // [WATCHDOG] Enable. Timeout is 5000 ms.
    // pause_on_debug=true freezes the counter while a debugger is halted.
    watchdog_enable(5000, true);
    LOG(LOG_ALWAYS, "Watchdog enabled (5000 ms timeout)");

    while (true) {
        poll_serial_input();

        watchdog_update();  // [WATCHDOG] Feed dog. Must run < every 5000 ms.

        const uint32_t now = to_ms_since_boot(get_absolute_time());
        const bool   mode_from_serial = (g_serial_mode_override != MODE_INVALID);
        const mode_t desired_mode     = mode_from_serial
                                        ? g_serial_mode_override
                                        : read_mode_switch();

        if (desired_mode != last_mode || mode_from_serial != last_mode_from_serial) {
            LOG(LOG_INFO, "Mode -> %s", mode_name_sourced(desired_mode, mode_from_serial));
            last_mode             = desired_mode;
            last_mode_from_serial = mode_from_serial;
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
            case MODE_AUTO: {
                const bool sensed = tx_sensed();
                desired_rf_state = sensed ? RF_TX : RF_RX;
                // Stamp every poll that crosses the threshold so the
                // heartbeat's "seconds ago" tracks the most recent
                // detection. Only Auto mode counts as an auto-detect;
                // Force TX deliberately doesn't update this.
                if (sensed) {
                    last_tx_detect_ms = now;
                }
                break;
            }
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
                rf_state_name(rf_state), mode_name_sourced(desired_mode, mode_from_serial)
            );
            last_adc_value_trace_time = now;
        }

        // Info-level heartbeat.
        if (g_log_level >= LOG_INFO && (now - last_info_heartbeat_time >= INFO_HEARTBEAT_MESSAGE_INTERVAL_MS)) {
            if (last_tx_detect_ms == 0) {
                LOG(
                    LOG_INFO,
                    "Heartbeat: state=%s mode=%s last_tx_ago_s=never",
                    rf_state_name(rf_state), mode_name_sourced(desired_mode, mode_from_serial)
                );
            } else {
                const uint32_t age_ms = now - last_tx_detect_ms;
                LOG(
                    LOG_INFO,
                    "Heartbeat: state=%s mode=%s last_tx_ago_s=%lu.%03lu",
                    rf_state_name(rf_state), mode_name_sourced(desired_mode, mode_from_serial),
                    (unsigned long)(age_ms / 1000u),
                    (unsigned long)(age_ms % 1000u)
                );
            }

            last_info_heartbeat_time = now;
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
