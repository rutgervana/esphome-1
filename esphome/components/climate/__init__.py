import voluptuous as vol

from esphome.automation import ACTION_REGISTRY, maybe_simple_id
from esphome.components import mqtt
from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_MQTT_ID
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_types import Application, Action, App, Nameable, esphome_ns

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

climate_ns = esphome_ns.namespace('climate')

Climate = climate_ns.class_('Climate', Nameable)
MQTTClimateComponent = climate_ns.class_('MQTTClimateComponent', mqtt.MQTTComponent)
MakeClimate = Application.struct('MakeClimate')

CLIMATE_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Climate),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTClimateComponent),
})

CLIMATE_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(CLIMATE_SCHEMA.schema)

def setup_climate_core_(climate_var, config):
    if CONF_INTERNAL in config:
        add(climate_var.set_internal(config[CONF_INTERNAL]))
    setup_mqtt_component(climate_var.Pget_mqtt(), config)


def setup_climate(climate_obj, config):
    CORE.add_job(setup_climate_core_, climate_obj, config)


def register_climate(var, config):
    climate_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    add(App.register_climate(climate_var))
    CORE.add_job(setup_climate_core_, climate_var, config)

BUILD_FLAGS = '-DUSE_CLIMATE'

def core_to_hass_config(data, config):
    ret = mqtt.build_hass_config(data, 'climate', config, include_state=True, include_command=True)
    if ret is None:
        return None
    return ret