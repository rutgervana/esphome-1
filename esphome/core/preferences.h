#pragma once

#include <string>

#include "esphome/core/esphal.h"
#include "esphome/core/defines.h"

namespace esphome {

enum Retain {
  RETAIN_YES,
  RETAIN_RTC,
  RETAIN_NO,
};

#define LOG_STATEFUL_COMPONENT(this) \
  const char* retain = ""; \
  switch (this->retain_) { \
    case RETAIN_YES: \
      retain = "YES from Flash"; \
      break; \
    case RETAIN_RTC: \
      retain = "RTC"; \
      break; \
    case RETAIN_NO: \
      retain = "NO: Uses intial value"; \
      break; \
  } \
  ESP_LOGCONFIG(TAG, "  Retain: %s", retain);

class ESPPreferenceObject {
 public:
  ESPPreferenceObject();
  ESPPreferenceObject(size_t offset, size_t length, uint32_t type);

  template<typename T> bool save(const T* src);

  template<typename T> bool load(T* dest);

  bool is_initialized() const;

 protected:
  friend class ESPPreferences;

  bool save_();
  bool load_();
  bool save_internal_();
  bool load_internal_();

  uint32_t calculate_crc_() const;

  size_t offset_;
  size_t length_words_;
  uint32_t type_;
  uint32_t* data_;
#ifdef ARDUINO_ARCH_ESP8266
  bool in_flash_{false};
#endif
};

class ESPPreferences {
 public:
  ESPPreferences();
  void begin();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash);
  template<typename T>
  ESPPreferenceObject make_preference(uint32_t type, bool in_flash = false /* TODO Remove default when all migrated */);

#ifdef ARDUINO_ARCH_ESP8266
  /** On the ESP8266, we can't override the first 128 bytes during OTA uploads
   * as the eboot parameters are stored there. Writing there during an OTA upload
   * would invalidate applying the new firmware. During normal operation, we use
   * this part of the RTC user memory, but stop writing to it during OTA uploads.
   *
   * @param prevent Whether to prevent writing to the first 32 words of RTC user memory.
   */
  void prevent_write(bool prevent);
  bool is_prevent_write();
#endif

 protected:
  friend ESPPreferenceObject;

  uint32_t current_offset_;
#ifdef ARDUINO_ARCH_ESP32
  uint32_t nvs_handle_;
#endif
#ifdef ARDUINO_ARCH_ESP8266
  void save_esp8266_flash_();
  bool prevent_write_{false};
  uint32_t* flash_storage_;
  uint32_t current_flash_offset_;
#endif
};

extern ESPPreferences global_preferences;

template<typename T> ESPPreferenceObject ESPPreferences::make_preference(uint32_t type, bool in_flash) {
  return this->make_preference((sizeof(T) + 3) / 4, type, in_flash);
}

template<typename T> bool ESPPreferenceObject::save(const T* src) {
  if (!this->is_initialized())
    return false;
  memset(this->data_, 0, this->length_words_ * 4);
  memcpy(this->data_, src, sizeof(T));
  return this->save_();
}

template<typename T> bool ESPPreferenceObject::load(T* dest) {
  memset(this->data_, 0, this->length_words_ * 4);
  if (!this->load_())
    return false;

  memcpy(dest, this->data_, sizeof(T));
  return true;
}

template<typename T> class RetainState {
 public:
  RetainState() {}
  RetainState(ESPPreferenceObject&& base) : ESPPreferenceObject(base) {}
  void init_retain(uint32_t type, Retain retain, T initial_state_);

 protected:
  void save_state_(const T& value);
  T&& get_state_();

  ESPPreferenceObject preference_;
  // TODO: it's so much easier to follow the code when we make a copy of the initial value rather
  // than immediately storing it in the base class data_. But this does double the memory usage, so
  // how concerned with memory are we? (especially with the additional copy below?)
  T initial_state_;
  Retain retain_;
};

template<typename T> void RetainState<T>::init_retain(uint32_t type, Retain retain, T initial_state_) {
  this->retain_ = retain;
  this->initial_state_ = initial_state_;
  if (retain != RETAIN_NO) {
    this->preference_ = global_preferences.make_preference<T>(type, retain == RETAIN_YES);
  }
}

template<typename T> void RetainState<T>::save_state_(const T& value) {
  if (this->retain_ == RETAIN_NO)
    return;

  // Store the new state if we couldn't load the old state, or it's a new value and we're not always
  // just restoring the initial value anyway. It's very important we don't store the state if it's
  // the same as the old state because there are a limited number of writes to the esp8266 flash.
  T current_state;
  if (!this->preference_.load(&current_state) || value != current_state) {
    this->preference_.save(&value);
  }
}

template<typename T> T&& RetainState<T>::get_state_() {
  T result = this->initial_state_;

  if (this->retain_ != RETAIN_NO)
    this->preference_.load(&result);

  return std::move(result);
}

}  // namespace esphome
