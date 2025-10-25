#include "neptune_water_meter.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace neptune_water_meter {

static const char *const TAG = "neptune_water_meter";

constexpr size_t MESSAGE_LEN = 31;

double_t NeptuneWaterMeterSensor::parse_reading_() {
  std::string raw_message(this->raw_message_.begin(), this->raw_message_.end());
  ESP_LOGI(TAG, "Raw Message: %s", format_hex_pretty(raw_message).c_str());
  std::string reading{"0123456.7"};
  for (int i = 7; i < 13; i++) {
    reading[i - 7] = this->raw_message_[i];
  }
  reading[6] = this->raw_message_[27];
  reading[8] = this->raw_message_[28];

  return std::stod(reading.c_str());
}

void NeptuneWaterMeterSensor::dump_config() { LOG_SENSOR("", "Neptune Water Meter", this); }
void NeptuneWaterMeterSensor::loop() {
  int bytes_available = this->available();
  if (bytes_available > 0) {
    ESP_LOGD(TAG, "%d bytes received", bytes_available);
    this->last_byte_time_ = millis();
    if (this->bytes_read_ + bytes_available > MESSAGE_LEN) {
      // Deal with casewhere there are too many bytes available
      bytes_available = MESSAGE_LEN - this->bytes_read_;
    }
    if (this->read_array(&this->raw_message_[this->bytes_read_], bytes_available)) {
      ESP_LOGI(TAG, "Read %d bytes.", bytes_available);
      this->bytes_read_ += bytes_available;
    } else {
      ESP_LOGE(TAG, "Failed reading buffer");
    }
  }

  // Deal with timeout/incomplete message
  if (millis() - this->last_byte_time_ > this->timeout_ && this->bytes_read_) {
    ESP_LOGW(TAG, "rx timeout");
    std::string partial(this->raw_message_.begin(), this->raw_message_.begin() + this->bytes_read_);
    ESP_LOGW(TAG, "Buffer: %s bytes_read: %d", format_hex_pretty(partial).c_str(), this->bytes_read_);
    this->bytes_read_ = 0;
  }

  if (this->bytes_read_ == MESSAGE_LEN) {
    ESP_LOGI(TAG, "reading rx'd");
    double reading = this->parse_reading_();
    this->bytes_read_ = 0;
    this->publish_state(reading);
    this->listeners_.call(reading);
  }
  if (this->bytes_read_ >= MESSAGE_LEN) {
    ESP_LOGE(TAG, "invalid state");
  }
}

}  // namespace neptune_water_meter
}  // namespace esphome
