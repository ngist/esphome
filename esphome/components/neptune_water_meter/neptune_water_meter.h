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

class NeptuneWaterMeterSensor;

struct NeptuneWaterMeterSensorStorage {
  ISRInternalGPIOPin clock_pin;
  ISRInternalGPIOPin enable_pin;

  NeptuneWaterMeterSensor &water_meter;

  volatile uint32_t clock_count;

  static void clock_interrupt(NeptuneWaterMeterSensorStorage *arg);
};

class NeptuneWaterMeterSensor : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_listener(std::function<void(uint32_t)> listener) { this->listeners_.add(std::move(listener)); }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void set_enable_count(uint32_t enable_count_) { this->storage_.enable_count = enable_count; }

  void set_clock_pin(InternalGPIOPin *clock_pin) { this->clock_pin_ = clock_pin; }
  void set_enable_pin(InternalGPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }

 protected:
  InternalGPIOPin *clock_pin_{nullptr};
  InternalGPIOPin *enable_pin_{nullptr};
  uint32_t timeout_{1000};
  std::array<uint8_t, BUFFER_SIZE> raw_message_{0};
  size_t bytes_read_{0};
  uint32_t last_byte_time_{0};

  NeptuneWaterMeterSensorStorage storage_{.water_meter = *this};

  CallbackManager<void(int32_t)> listeners_{};

  // Functions
  double_t parse_reading_();
  void reset_state_();
  void log_buffer_();
};

}  // namespace neptune_water_meter
}  // namespace esphome
