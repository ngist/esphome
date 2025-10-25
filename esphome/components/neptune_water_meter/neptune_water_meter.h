#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace neptune_water_meter {

constexpr size_t BUFFER_SIZE = 64;

class NeptuneWaterMeterSensor : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void dump_config() override;
  void loop() override;

  void register_listener(std::function<void(uint32_t)> listener) { this->listeners_.add(std::move(listener)); }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

 protected:
  std::array<uint8_t, BUFFER_SIZE> raw_message_{0};
  size_t bytes_read_{0};
  uint32_t last_byte_time_{0};
  uint32_t timeout_;

  CallbackManager<void(int32_t)> listeners_{};
  double_t parse_reading_();
};

}  // namespace neptune_water_meter
}  // namespace esphome
