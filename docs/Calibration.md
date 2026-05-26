# Calibration

No calibration should be required. However, there are two configurable parameters hardcoded in the firmware which should be validated:

1. ADC detection threshold of the RF power sensing.
2. Relay activation/deactivation latency.

If you switch hardware components, this "calibration" procedure may be required. The values can easily be determined from datasheets.

## Preface

* This guide assumes you have a working board and have followed the `PCB_Test_Plan.md` guide.
* Be aware that damage may occur if followed improperly, or if the values are set incorrectly.

## Calibrating the RF Power Sense Threshold

This step pertains to the `TX_DETECT_THRESHOLD_RAW` value in firmware, which is used to auto-sense an attempt to TX and switch into TX mode.

1. Flash firmware and open serial terminal.
2. Connect a high-power RF transmitter of 30 dBm (1 watt) transmit power.
  * We use 1 watt as the minimum detection threshold. Calibrating for 1 watt will ensure that higher wattages also trigger the detector.
3. Activate max logging mode (press 3).
4. Observe the log messages showing the raw ADC value to get an **Idle** baseline (expected to be around 250mV to 300mV, or raw ADC values of 310-372).
5. Transit with RF (between 100ms to 500ms transmission is ideal).
6. Observe the log messages showing the raw ADC value to get the **active TX** value (expected to be >=400mV or raw ADC value 496).
7. Set `TX_DETECT_THRESHOLD_RAW` value to slightly below the ADC value observed during Step 6 (with TX), which should be comfortably above the value in Step 4 (idle).

The ADC on the RP2040 has a full-scale raw value of 4095 at 3.3v.

## Calibrating Relay Switching Threshold

This step pertains to the `RELAY_SWITCH_DELAY_MS` value in firmware, which is used to ensure the relays are switched in such a way that RF power is always sent to the Antenna or the RF Dummy load (or, very briefly, to both).

1. Assuming you trust the value in the relay datasheets, set `RELAY_SWITCH_DELAY_MS` to the maximum of the "max switching time" values.
  * If the datasheet gives separate Operate Time vs. Release Time, use the max value.
  * While it may be tempting to add a large margin (e.g., 2x to 10x) on this value for safety, avoid the temptation. Keeping the value very low is ideal because it minimizes the duration the transmit power is sent into the attenuator, and minimizes the duration the system has a 2:1 SWR (due to the dummy load and antenna effectively being in parallel).
2. If you don't trust the datasheet, devise an elaborate plan to send a voltage into the relay and measure it with a scope.
