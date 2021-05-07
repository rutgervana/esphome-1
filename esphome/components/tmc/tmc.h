#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/a4988/a4988.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tmc {

class TMC : public a4988::A4988, public uart::UARTDevice {};
}  // namespace tmc
}  // namespace esphome