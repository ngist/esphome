#include "neptune_water_meter.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace neptune_water_meter {

static const char *const TAG = "neptune_water_meter";
// The raw data is BCD binary coded decimal so every 4 bits represents one 0-9 decimal value so only
// pack 4 bits per byte, the upper nibble contains 0x3 so that the packed values are directly encoded
// as ascii characters this will save work later.
constexpr u_int8_t BITS_PER_BYTE = 4;
constexpr size_t MAX_BITS = BUFFER_SIZE * BITS_PER_BYTE;

void IRAM_ATTR HOT NeptuneWaterMeterSensorStore::clock_interrupt(NeptuneWaterMeterSensorStore *arg) {
  // Capture data as quickly as possible when clock rises
  bool_t data = arg->pin_data.digital_read();
  u_int32_t write_index = args->write_index;

  // Stuff the bit in the buffer, reader is responsible for clearing out the buffer after it's read.
  if (data) {
    arg->bit_buffer[write_index / BITS_PER_BYTE] |= data << (write_index % BITS_PER_BYTE);
  }
  // Increment and wrap back
  arg->write_index = (write_index + 1) % MAX_BITS;
}

void NeptumeWaterMeterSensor::setup() {
  int32_t initial_value = -1;

  this->pin_clock_->setup();
  this->store_.pin_clock = this->pin_clock_->to_isr();
  this->pin_data_->setup();
  this->store_.pin_data = this->pin_data_->to_isr();

  this->pin_a_->attach_interrupt(NeptuneWaterMeterSensorStore::clock_interrupt, &this->store_, gpio::INTERRUPT_RISING_EDGE;
}
void NeptuneWaterMeterSensor::dump_config() {
  LOG_SENSOR("", "Neptune Water Meter", this);
  LOG_PIN("  Pin Clock: ", this->pin_clock_);
  LOG_PIN("  Pin Data: ", this->pin_data_);
  ESP_LOGCONFIG(TAG, "  Scale Factor: %f.1", this->scale_factor_);
}
void NeptuneWaterMeterSensor::loop() {
  int32_t bits_captured = this->store_.write_index - this->store_.read_index;
  if (bits_captured < 0) {
    bits_captured += MAX_BITS;
  }  // Deal with ring buffer wrapping around
  ESP_LOGD(TAG, "Captured %d unprocessed bits.");

  // TODO: Find a better way to check for idle
  bool bus_idle = this->store_.last_count == bits_captured if (bus_idle && bits_captured) {
    uint32_t raw_reading = 0;
    // Have bits and they haven't changed, transmission must be completed.
    if (bit_captured % 4 != 0) {
      ESP_LOGW(TAG, "The number of bits collected per reading should be divisible by 4 got %d.", bits_captured);
    }
    for (char &iterator : this->store_.bit_buffer) {
      // Reset value after read
      iterator = 0x30;
    }
  }

  int counter = this->store_.counter;
  if (this->store_.last_read != counter || this->publish_initial_value_) {
    if (this->restore_mode_ == ROTARY_ENCODER_RESTORE_DEFAULT_ZERO) {
      this->rtc_.save(&counter);
    }
    this->store_.last_read = counter;
    this->publish_state(counter);
    this->listeners_.call(counter);
    this->publish_initial_value_ = false;
  }
}

float RotaryEncoderSensor::get_setup_priority() const { return setup_priority::DATA; }
void RotaryEncoderSensor::set_restore_mode(RotaryEncoderRestoreMode restore_mode) {
  this->restore_mode_ = restore_mode;
}
void RotaryEncoderSensor::set_resolution(RotaryEncoderResolution mode) { this->store_.resolution = mode; }
void RotaryEncoderSensor::set_min_value(int32_t min_value) { this->store_.min_value = min_value; }
void RotaryEncoderSensor::set_max_value(int32_t max_value) { this->store_.max_value = max_value; }

}  // namespace neptune_water_meter
}  // namespace esphome
