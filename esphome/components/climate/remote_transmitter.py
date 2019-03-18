import voluptuous as vol

from esphome.components import climate
import esphome.config_validation as cv
from esphome.components.remote_transmitter import RC_SWITCH_RAW_SCHEMA, \
    RC_SWITCH_TYPE_A_SCHEMA, RC_SWITCH_TYPE_B_SCHEMA, RC_SWITCH_TYPE_C_SCHEMA, \
    RC_SWITCH_TYPE_D_SCHEMA, RemoteTransmitterComponent, binary_code, build_rc_switch_protocol, \
    remote_ns
from esphome.const import CONF_MAKE_ID, CONF_ADDRESS, CONF_CARRIER_FREQUENCY, CONF_CHANNEL, CONF_CODE, \
    CONF_COMMAND, CONF_DATA, CONF_DEVICE, CONF_FAMILY, CONF_GROUP, CONF_ID, CONF_INVERTED, \
    CONF_JVC, \
    CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, CONF_PANASONIC, CONF_PROTOCOL, CONF_RAW, \
    CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B, CONF_RC_SWITCH_TYPE_C, \
    CONF_RC_SWITCH_TYPE_D, CONF_REPEAT, CONF_SAMSUNG, CONF_SONY, CONF_STATE, CONF_TIMES, \
    CONF_WAIT_TIME, CONF_RC5
from esphome.cpp_helpers import setup_component
from esphome.cpp_generator import Pvariable, add, get_variable, progmem_array, variable
from esphome.cpp_types import int32, App

DEPENDENCIES = ['remote_transmitter']

CONF_REMOTE_TRANSMITTER_ID = 'remote_transmitter_id'
CONF_TRANSMITTER_ID = 'transmitter_id'

CONF_COOLIX  = 'coolix'
CONF_HEAT = 'heat'

REMOTE_KEYS = [CONF_COOLIX]

RemoteTransmitter = remote_ns.class_('RemoteTransmitter', climate.Climate)
CoolixTransmitter = remote_ns.class_('CoolixTransmitter', RemoteTransmitter)

def validate_raw(value):
    if isinstance(value, dict):
        return cv.Schema({
            cv.GenerateID(): cv.declare_variable_id(int32),
            vol.Required(CONF_DATA): [vol.Any(vol.Coerce(int), cv.time_period_microseconds)],
            vol.Optional(CONF_CARRIER_FREQUENCY): vol.All(cv.frequency, vol.Coerce(int)),
        })(value)
    return validate_raw({
        CONF_DATA: value
    })

PLATFORM_SCHEMA = cv.nameable(climate.CLIMATE_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(climate.MakeClimate),
    vol.Optional(CONF_COOLIX): cv.Schema({
        vol.Optional(CONF_HEAT): cv.boolean
    }),
    cv.GenerateID(CONF_REMOTE_TRANSMITTER_ID): cv.use_variable_id(RemoteTransmitterComponent),
    cv.GenerateID(CONF_TRANSMITTER_ID): cv.declare_variable_id(RemoteTransmitter),
}), cv.has_exactly_one_key(*REMOTE_KEYS))


def transmitter_base(full_config):
    name = full_config[CONF_NAME]
    key, config = next((k, v) for k, v in full_config.items() if k in REMOTE_KEYS)

    if key == CONF_COOLIX:
        return CoolixTransmitter.new(name, config[CONF_HEAT])

    raise NotImplementedError("Unknown transmitter type {}".format(config))


def to_code(config):
    rhs = App.make_climate(config[CONF_NAME])
    climate_struct = variable(config[CONF_MAKE_ID], rhs)
    climate.setup_climate(climate_struct.Pstate, config)
    setup_component(climate_struct.Poutput, config)


BUILD_FLAGS = '-DUSE_REMOTE_TRANSMITTER'


def to_hass_config(data, config):
    return climate.core_to_hass_config(data, config)
