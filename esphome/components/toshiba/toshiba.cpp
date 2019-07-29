#include "toshiba.h"
#include "esphome/core/log.h"

namespace esphome {
namespace toshiba {

static const char *TAG = "toshiba.climate";

const uint32_t TOSHIBA_HDR_MARK = 4400;
const uint32_t TOSHIBA_HDR_SPACE = 4400;
const uint32_t TOSHIBA_BIT_MARK = 550;
const uint32_t TOSHIBA_ONE_SPACE = 1600;
const uint32_t TOSHIBA_ZERO_SPACE = 550;

const uint8_t TOSHIBA_FAN_AUTO = 0x00;
const uint8_t TOSHIBA_MODE_OFF = 0xE0;
const uint8_t TOSHIBA_MODE_AUTO = 0x00;
const uint8_t TOSHIBA_MODE_HEAT = 0xC0;
const uint8_t TOSHIBA_MODE_COOL = 0x80;
const uint8_t TOSHIBA_MODE_DRY = 0x40;
const uint8_t TOSHIBA_FAN1 = 0x02;
const uint8_t TOSHIBA_FAN2 = 0x06;
const uint8_t TOSHIBA_FAN3 = 0x01;
const uint8_t TOSHIBA_FAN4 = 0x05;
const uint8_t TOSHIBA_FAN5 = 0x03;

const uint8_t TOSHIBA_TEMP_MIN = 17;
const uint8_t TOSHIBA_TEMP_MAX = 30;

climate::ClimateTraits ToshibaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(TOSHIBA_TEMP_MIN);
  traits.set_visual_max_temperature(TOSHIBA_TEMP_MAX);
  traits.set_visual_temperature_step(1);
  return traits;
}

void ToshibaClimate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else
    this->current_temperature = NAN;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_AUTO;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature = roundf(this->current_temperature);
  }
}

void ToshibaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void ToshibaClimate::transmit_state_() {
  uint8_t operating_mode;
  uint8_t fan_speed = TOSHIBA_FAN_AUTO;
  uint8_t temperature = 23;

  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      operating_mode = TOSHIBA_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_COOL:
      operating_mode = TOSHIBA_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_AUTO:
      operating_mode = TOSHIBA_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = TOSHIBA_MODE_OFF;
      break;
  }

  temperature = (uint8_t) roundf(clamp(this->target_temperature, TOSHIBA_TEMP_MIN, TOSHIBA_TEMP_MAX));

  uint8_t remote_state[] = {0x4F, 0xB0, 0xC0, 0x3F, 0x80, (uint8_t)((temperature - 17) << 4),
                           (uint8_t)(operating_mode | fan_speed), 0x00, 0x00};

  uint8_t checksum{0};
  for (uint8_t i = 0; i < 8; i++) {
    checksum ^= remote_state[i];
  }
  remote_state[8] = checksum;

  ESP_LOGV(TAG, "Sending toshiba code: %u", remote_state);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(TOSHIBA_HDR_MARK);
  data->space(TOSHIBA_HDR_SPACE);
  // Data
  for (uint8_t i : remote_state)
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(TOSHIBA_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? TOSHIBA_ONE_SPACE : TOSHIBA_ZERO_SPACE);
    }
  // End mark
  data->mark(TOSHIBA_BIT_MARK);
  data->space(0);

  transmit.perform();
}

}  // namespace toshiba
}  // namespace esphome
