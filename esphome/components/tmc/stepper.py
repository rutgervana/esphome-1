from typing_extensions import Required
from esphome import pins
from esphome.components import stepper
from esphome.components import uart
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DIR_PIN,
    CONF_ID,
    CONF_MODEL,
    CONF_SLEEP_PIN,
    CONF_STEP_PIN,
)
import esphome.components.a4988.stepper as a4988

AUTO_LOAD = ["a4988"]

tmc_ns = cg.global_ns
TMC = tmc_ns.class_("TMC", a4988.A4988, cg.Component)

tmc_driver_ns = cg.global_ns.namespace("tmc")
TMC_MODELS = {
    "TMC2200": tmc_ns.class_("TMC2208Stepper"),
    "TMC2209": tmc_ns.class_("TMC2209Stepper"),
}

CONF_SETUP = "setup"

CONFIG_SCHEMA = (
    stepper.STEPPER_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(TMC),
            cv.Required(CONF_MODEL): cv.enum(TMC_MODELS, upper=True),
            cv.Required(CONF_STEP_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DIR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SLEEP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SETUP): cv.lambda_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield stepper.register_stepper(var, config)
    yield uart.register_uart_device(var, config)

    step_pin = yield cg.gpio_pin_expression(config[CONF_STEP_PIN])
    cg.add(var.set_step_pin(step_pin))
    dir_pin = yield cg.gpio_pin_expression(config[CONF_DIR_PIN])
    cg.add(var.set_dir_pin(dir_pin))

    if CONF_SLEEP_PIN in config:
        sleep_pin = yield cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(sleep_pin))

    if CONF_SETUP in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_SETUP],
            [(TMC_MODELS[config[CONF_MODEL]], "driver")],
            return_type=cg.void,
        )
        cg.add(var.set_setup(lambda_))

    cg.add_library("teemuatlut/TMCStepper", "0.7.1")
