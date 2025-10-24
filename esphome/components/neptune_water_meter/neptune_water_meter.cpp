#include "neptune_water_meter.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace neptune_water_meter {

static const char *const TAG = "neptune_water_meter";
// The raw data is trasmitted in 11 bits per word
constexpr uint8_t BITS_PER_WORD = 8;
constexpr size_t MAX_BITS = BUFFER_SIZE * BITS_PER_WORD;
constexpr uint16_t DEFAULT_BUFFER_VALUE = 0;
constexpr uint16_t FILTER_DURATION = 13;

void IRAM_ATTR HOT NeptuneWaterMeterSensorStorage::clock_interrupt(NeptuneWaterMeterSensorStorage *arg) {
  // Capture data as quickly as possible when clock rises
  arg->last_bit_time = micros();
  bool has_been_read = false;
  while ((micros() - arg->last_bit_time) < 5000) {
    bool clock_value = arg->pin_clock.digital_read();
    if (arg->clock_falling_edge_) {
      clock_value = !clock_value;
    }
    if (!clock_value) {
      has_been_read = false;
    }
    if (!has_been_read && clock_value) {
      bool data = arg->pin_data.digital_read();
      has_been_read = true;
      arg->last_bit_time = micros();
      uint32_t write_index = arg->write_index;

      // Stuff the bit in the buffer, reader is responsible for clearing out the buffer after it's read.
      if (data) {
        arg->bit_buffer[write_index / BITS_PER_WORD] |= data << (write_index % BITS_PER_WORD);
      }
      // Increment and wrap back
      arg->write_index = (write_index + 1) % MAX_BITS;
    }
  }

  // bool data = arg->pin_data.digital_read();
  // auto now = micros();
  // if (!arg->pin_clock.digital_read()) {
  //   // Falling edges are unexpected so log and return.
  //   arg->falling_edge_triggers++;
  //   return;
  // }
  // if (now - arg->last_bit_time < FILTER_DURATION) {
  //   // Very fast transitions also unexpected so log and return
  //   arg->filtered_out_triggers++;
  //   return;
  // }

  // arg->last_bit_time = now;
  // uint32_t write_index = arg->write_index;

  // // Stuff the bit in the buffer, reader is responsible for clearing out the buffer after it's read.
  // if (data) {
  //   arg->bit_buffer[write_index / BITS_PER_WORD] |= data << (write_index % BITS_PER_WORD);
  // }
  // Increment and wrap back
  // arg->write_index = (write_index + 1) % MAX_BITS;
  arg->water_meter.enable_loop_soon_any_context();
}

void NeptuneWaterMeterSensor::flush_buffer_() {
  this->read_index_ = 0;
  {
    InterruptLock lock;
    this->storage_.write_index = 0;
    this->storage_.falling_edge_triggers = 0;
    this->storage_.filtered_out_triggers = 0;
    this->storage_.bit_buffer.fill(DEFAULT_BUFFER_VALUE);
  }
}

uint8_t nibble_to_hex(uint8_t nibble) { return nibble < 10 ? 0x30 + nibble : 0x41 + (nibble - 10); }

void pack_byte(uint8_t byte, std::string &buffer) {
  uint8_t upper_nibble = byte >> 4 & 0xF;
  uint8_t lower_nibble = byte & 0xF;
  buffer.push_back(nibble_to_hex(upper_nibble));
  buffer.push_back(nibble_to_hex(lower_nibble));
}

void NeptuneWaterMeterSensor::dump_raw_buffer_() {
  std::string buffer_data = "";
  for (auto it : this->storage_.bit_buffer) {
    if (BITS_PER_WORD > 8) {
      pack_byte(it >> 8 & 0xFF, buffer_data);
    }
    pack_byte(it & 0xFF, buffer_data);
  }
  ESP_LOGD(TAG, "Buffer Data: %s", buffer_data.c_str());
}

double_t NeptuneWaterMeterSensor::parse_reading_() {
  std::string text{};
  for (uint16_t it : this->raw_message_) {
    // Discard start bit and stop bits leave parity bit
    // if (it & 0x001) {
    //   ESP_LOGW(TAG, "Bad start bit");
    // }
    // if (!(it & 0x200)) {
    //   ESP_LOGW(TAG, "Bad first stop bit");
    // }
    // if (!(it & 0x400)) {
    //   ESP_LOGW(TAG, "Bad second stop bit");
    // }
    uint8_t word = (it & 0x1FF) >> 1;
    ESP_LOGD(TAG, "word: %x", word);
    text.push_back(word);
  }
  ESP_LOGD(TAG, "SemiProcessed: %s", text.c_str());
  if (text.size() != 31) {
    ESP_LOGE(TAG, "Unexpected messages size %d", text.size());
    return -1;
  }
  std::string reading{"0123456.7"};
  for (int i = 7; i < 13; i++) {
    reading[i - 7] = text[i] & 0x7f;
  }
  reading[6] = text[27] & 0x7F;
  reading[8] = text[28] & 0x7F;

  return std::stod(reading.c_str());
}

void NeptuneWaterMeterSensor::setup() {
  this->pin_clock_->setup();
  this->pin_data_->setup();
  this->storage_.pin_data = this->pin_data_->to_isr();
  this->storage_.pin_clock = this->pin_clock_->to_isr();
  this->storage_.bit_buffer.fill(DEFAULT_BUFFER_VALUE);
  this->storage_.write_index = 0;
  this->storage_.last_bit_time = micros();
  this->storage_.falling_edge_triggers = 0;
  this->storage_.filtered_out_triggers = 0;

  this->pin_clock_->attach_interrupt(NeptuneWaterMeterSensorStorage::clock_interrupt, &this->storage_,
                                     gpio::INTERRUPT_RISING_EDGE);
}
void NeptuneWaterMeterSensor::dump_config() {
  LOG_SENSOR("", "Neptune Water Meter", this);
  LOG_PIN("  Pin Clock: ", this->pin_clock_);
  LOG_PIN("  Pin Data: ", this->pin_data_);
  ESP_LOGCONFIG(TAG, "  Scale Factor: %f", this->scale_factor_);
}
void NeptuneWaterMeterSensor::loop() {
  uint32_t time_since_last_bit = micros() - this->storage_.last_bit_time;
  bool bus_idle = time_since_last_bit > 25000;
  // Capture volatile value once to maintain a consistent state throughout the loop.
  uint32_t write_index = this->storage_.write_index;
  uint32_t bits_captured = write_index - this->read_index_;

  if (!bits_captured) {
    this->disable_loop();
    ESP_LOGD(TAG, "Loop disabled");
    return;
  }

  if (!bus_idle) {
    ESP_LOGD(TAG, "Time since last bit %d", time_since_last_bit);
    return;
  }

  bool buffer_corrupted = false;
  ESP_LOGD(TAG, "Filtered transitions %d, Falling transitions: %d", this->storage_.filtered_out_triggers,
           this->storage_.falling_edge_triggers);
  ESP_LOGD(TAG, "Captured %d unprocessed bits.", bits_captured);
  if (bits_captured % BITS_PER_WORD != 0) {
    // Bits received should be divisible by word length
    ESP_LOGW(TAG, "Incomplete Data Received");
    buffer_corrupted = true;
  }
  if (bits_captured / BITS_PER_WORD > BUFFER_SIZE) {
    ESP_LOGE(TAG, "Buffer overflow");
    buffer_corrupted = true;
  }
  if (this->read_index_ % BITS_PER_WORD != 0) {
    // First bit should be aligned to a word boundary
    ESP_LOGE(TAG, "Data Alignment Error");
    buffer_corrupted = true;
  }
  if (buffer_corrupted) {
    ESP_LOGD(TAG, "Buffer corrupted flushing");
    this->dump_raw_buffer_();
    this->flush_buffer_();
    return;
  }
  this->dump_raw_buffer_();
  this->raw_message_.fill(0);
  for (int i = 0; i < (bits_captured / BITS_PER_WORD); i++) {
    this->raw_message_[i] = this->storage_.bit_buffer[i];
  }
  this->flush_buffer_();

  uint32_t reading = this->parse_reading_();
  if (this->last_reading_ != reading && reading > 0) {
    this->last_reading_ = reading;
    this->publish_state(reading);
    this->listeners_.call(reading);
  }
}

float NeptuneWaterMeterSensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace neptune_water_meter
}  // namespace esphome
