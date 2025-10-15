#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace neptune_water_meter {

constexpr size_t BUFFER_SIZE = 512;
constexpr char DEFAULT_BUFFER_VALUE = 0x30;

struct NeptuneWaterMeterSensorStore {
  ISRInternalGPIOPin pin_data;

  // Setup a simple ring buffer
  volatile u_int32_t write_index{0};
  int32_t read_index{0};
  std::array<char, BUFFER_SIZE> bit_buffer{DEFAULT_BUFFER_VALUE};

  static void clock_interrupt(NeptuneWaterMeterSensorStore *arg);
};

class NeptuneWaterMeterSensor : public sensor::Sensor, public Component {
 public:
  void set_pin_clock(InternalGPIOPin *pin_clock) { pin_clock_ = pin_clock; }
  void set_pin_data(InternalGPIOPin *pin_data) { pin_data_ = pin_data; }

  /** Set scale factor of the reading
   *
   * Not all neptune meters report the same units so some readings need to be multiplied by a scale factor.
   */
  void set_scale_factor(double_t scale_factor) { scale_factor_ = scale_factor; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;

  void register_listener(std::function<void(uint32_t)> listener) { this->listeners_.add(std::move(listener)); }

 protected:
  InternalGPIOPin *pin_clock_;
  InternalGPIOPin *pin_data_;
  int32_t last_bits_captured_{0};
  int32_t read_index_{0};
  std::array<char, BUFFER_SIZE> raw_message_{0};
  uint32_t last_reading_{0};
  double_t scale_factor_;

  NeptuneWaterMeterSensorStore store_{};

  CallbackManager<void(int32_t)> listeners_{};
  void flush_buffer_();
  uint32_t parse_reading_();
};

}  // namespace neptune_water_meter
}  // namespace esphome
