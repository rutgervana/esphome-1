from esphome import automation, pins
from esphome.components import stepper
from esphome.components import uart
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_CURRENT,
    CONF_DIR_PIN,
    CONF_ID,
    CONF_SLEEP_PIN,
    CONF_STEP_PIN,
)


tmc_ns = cg.esphome_ns.namespace("tmc")
TMC2209 = tmc_ns.class_("TMC2209", stepper.Stepper, cg.Component)
TMC2209SetupAction = tmc_ns.class_("TMC2209SetupAction", automation.Action)

CONF_REVERSE_DIRECTION = "reverse_direction"
CONF_MICROSTEPS = "microsteps"
CONF_TCOOL_THRESHOLD = "tcool_threshold"
CONF_STALL_THRESHOLD = "stall_threshold"

CONFIG_SCHEMA = (
    stepper.STEPPER_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(TMC2209),
            cv.Required(CONF_STEP_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DIR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SLEEP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_REVERSE_DIRECTION, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


@automation.register_action(
    "tmc2209.setup",
    TMC2209SetupAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TMC2209),
            cv.Optional(CONF_MICROSTEPS): cv.templatable(
                cv.one_of(256, 128, 64, 32, 16, 8, 4, 2, 0)
            ),
            cv.Optional(CONF_TCOOL_THRESHOLD): cv.templatable(cv.int_),
            cv.Optional(CONF_STALL_THRESHOLD): cv.templatable(cv.int_),
            cv.Optional(CONF_CURRENT): cv.templatable(cv.current),
        }
    ),
)
def tmc2209_setup_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    if CONF_MICROSTEPS in config:
        template_ = yield cg.templatable(config[CONF_MICROSTEPS], args, int)
        cg.add(var.set_microsteps(template_))
    if CONF_TCOOL_THRESHOLD in config:
        template_ = yield cg.templatable(config[CONF_TCOOL_THRESHOLD], args, int)
        cg.add(var.set_tcool_threshold(template_))
    if CONF_STALL_THRESHOLD in config:
        template_ = yield cg.templatable(config[CONF_STALL_THRESHOLD], args, int)
        cg.add(var.set_stall_threshold(template_))
    if CONF_CURRENT in config:
        template_ = yield cg.templatable(config[CONF_CURRENT], args, float)
        cg.add(var.set_current(template_))

    yield var


def to_code(config):
    step_pin = yield cg.gpio_pin_expression(config[CONF_STEP_PIN])
    dir_pin = yield cg.gpio_pin_expression(config[CONF_DIR_PIN])

    var = cg.new_Pvariable(
        config[CONF_ID], step_pin, dir_pin, config[CONF_REVERSE_DIRECTION]
    )
    yield cg.register_component(var, config)
    yield stepper.register_stepper(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_SLEEP_PIN in config:
        sleep_pin = yield cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(sleep_pin))

    cg.add_library("SPI", None)
    cg.add_library("teemuatlut/TMCStepper", "0.7.1")
