#include "neptune_water_meter.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace neptune_water_meter {

static const char *const TAG = "neptune_water_meter";

constexpr size_t MESSAGE_LEN = 31;

void IRAM_ATTR HOT NeptuneWaterMeterSensorStorage::clock_interrupt(NeptuneWaterMeterSensorStorage *arg) {
  // Capture data as quickly as possible when clock rises
  arg->clock_count++;
  if (!arg->enable_pin.digital_read() && arg->clock_count >= arg->enable_count) {
    arg->enable_pin.digital_write(true);
  }
  arg->water_meter.enable_loop_soon_any_context();
}

double_t NeptuneWaterMeterSensor::parse_reading_() {
  std::string raw_message(this->raw_message_.begin(), this->raw_message_.end());
  ESP_LOGI(TAG, "Raw Message: %s", format_hex_pretty(raw_message).c_str());
  std::string reading = "0123456.7";
  for (int i = 7; i < 13; i++) {
    reading[i - 7] = this->raw_message_[i];
  }
  reading[6] = this->raw_message_[27];
  reading[8] = this->raw_message_[28];

  return std::stod(reading.c_str());
}

void NeptuneWaterMeterSensor::setup() {
  this->clock_pin_->setup();
  this->storage_.clock_pin = this->clock_pin_->to_isr();
  this->enable_pin_->setup();
  this->storage_.enable_pin = this->enable_pin_->to_isr();
  this->storage_.enable_pin.digital_write(false);
  this->storage_.last_clock_time = micros();
  this->storage_.clock_count = 0;
  this->clock_pin_->attach_interrupt(NeptuneWaterMeterSensorStorage::clock_interrupt, &this->storage_,
                                     gpio::INTERRUPT_RISING_EDGE);
}

void NeptuneWaterMeterSensor::reset_state_() {
  ESP_LOGI("Resetting state and disabling loop.")
  this->bytes_read_ = 0;
  this->raw_message_.fill(0);
  this->enable_pin_.digital_write(false);
  {
    InterruptLock lock;
    this->storage_.clock_count = 0;
  }
  // Clear out any unprocessed dangling bytes
  while (this->available()) {
    this->read_byte();
  }
  this->disable_loop();
}

void NeptuneWaterMeterSensor::log_buffer_() {
  std::string partial(this->raw_message_.begin(), this->raw_message_.begin() + this->bytes_read_);
  ESP_LOGW(TAG, "Buffer: %s bytes_read: %d", format_hex_pretty(partial).c_str(), this->bytes_read_);
}

void NeptuneWaterMeterSensor::dump_config() { LOG_SENSOR("", "Neptune Water Meter", this); }
void NeptuneWaterMeterSensor::loop() {
  int bytes_available = this->available();
  if (bytes_available > 0) {
    this->last_byte_time_ = millis();
    if (this->bytes_read_ + bytes_available > BUFFER_SIZE) {
      // Deal with casewhere there are too many bytes available
      bytes_available = BUFFER_SIZE - this->bytes_read_;
      ESP_LOGW(TAG, "Message exceeds buffer size truncating.");
    }
    if (this->read_array(&this->raw_message_[this->bytes_read_], bytes_available)) {
      ESP_LOGD(TAG, "Read %d bytes.", bytes_available);
      this->bytes_read_ += bytes_available;
    } else {
      ESP_LOGE(TAG, "Failed reading buffer");
    }
  }

  // Deal with timeout/incomplete message
  if (millis() - this->last_byte_time_ > this->timeout_ && this->bytes_read_) {
    ESP_LOGW(TAG, "rx timeout");
    this->log_buffer_();
    this->reset_state_();
  }

  if (this->bytes_read_ >= MESSAGE_LEN) {
    ESP_LOGI(TAG, "Read %d bytes message received", this->bytes_read_);
    double reading = this->parse_reading_();
    this->reset_state_();
    this->publish_state(reading);
    this->listeners_.call(reading);
  }
  if (this->bytes_read_ == BUFFER_SIZE) {
    ESP_LOGE(TAG, "invalid state");
    this->log_buffer_();
    this->reset_state_();
  }
}

}  // namespace neptune_water_meter
}  // namespace esphome
