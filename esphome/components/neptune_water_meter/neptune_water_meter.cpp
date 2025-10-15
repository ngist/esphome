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
  bool data = arg->pin_data.digital_read();
  u_int32_t write_index = arg->write_index;

  // Stuff the bit in the buffer, reader is responsible for clearing out the buffer after it's read.
  if (data) {
    arg->bit_buffer[write_index / BITS_PER_BYTE] |= data << (write_index % BITS_PER_BYTE);
  }
  // Increment and wrap back
  arg->write_index = (write_index + 1) % MAX_BITS;
  arg->water_meter.enable_loop_soon_any_context();
}

void NeptuneWaterMeterSensor::flush_buffer_() {
  this->read_index_ = 0;
  {
    InterruptLock lock;
    this->store_.write_index = 0;
    this->store_.bit_buffer.fill(DEFAULT_BUFFER_VALUE);
  }
}

uint32_t NeptuneWaterMeterSensor::parse_reading_() {
  // TODO IMPLEMENT
  return 0;
}

void NeptuneWaterMeterSensor::setup() {
  this->pin_clock_->setup();
  this->pin_data_->setup();
  this->store_.pin_data = this->pin_data_->to_isr();

  this->pin_clock_->attach_interrupt(NeptuneWaterMeterSensorStore::clock_interrupt, &this->store_,
                                     gpio::INTERRUPT_RISING_EDGE);
}
void NeptuneWaterMeterSensor::dump_config() {
  LOG_SENSOR("", "Neptune Water Meter", this);
  LOG_PIN("  Pin Clock: ", this->pin_clock_);
  LOG_PIN("  Pin Data: ", this->pin_data_);
  ESP_LOGCONFIG(TAG, "  Scale Factor: %f.1", this->scale_factor_);
}
void NeptuneWaterMeterSensor::loop() {
  int32_t bits_captured = this->store_.write_index - this->read_index_;
  if (bits_captured < 0) {
    bits_captured += MAX_BITS;
  }  // Deal with ring buffer wrapping around

  if (!bits_captured) {
    this->disable_loop();
    return;
  }

  // TODO: Find a better way to check for idle for now...
  // Have bits and they haven't changed, transmission must be completed.
  bool bus_idle = this->last_bits_captured_ == bits_captured;
  if (bus_idle && bits_captured) {
    bool buffer_corrupted = false;
    ESP_LOGD(TAG, "Captured %d unprocessed bits.");
    if (bits_captured % BITS_PER_BYTE != 0) {
      // Bits received should be divisible by 4
      ESP_LOGW(TAG, "Incomplete Data Received");
      buffer_corrupted = true;
    }
    if (this->read_index_ % BITS_PER_BYTE != 0) {
      // First bit should be aligned to a byte boundary
      ESP_LOGE(TAG, "Data Alignment Error");
      buffer_corrupted = true;
    }
    if (buffer_corrupted) {
      ESP_LOGD(TAG, "Buffer corrupted flushing");
      this->flush_buffer_();
      return;
    }

    int32_t begin = this->read_index_ / BITS_PER_BYTE;
    int32_t end = (this->read_index_ + bits_captured) / BITS_PER_BYTE;
    this->raw_message_.fill(0);
    for (int i = begin; i < end; i++) {
      this->raw_message_[i - begin] = this->store_.bit_buffer[i % BUFFER_SIZE];
      ESP_LOGD(TAG, "%s", this->raw_message_.data());
    }

    uint32_t reading = this->parse_reading_();
    if (this->last_reading_ != reading) {
      this->last_reading_ = reading;
      this->publish_state(reading);
      this->listeners_.call(reading);
    }
  }
}

float NeptuneWaterMeterSensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace neptune_water_meter
}  // namespace esphome
