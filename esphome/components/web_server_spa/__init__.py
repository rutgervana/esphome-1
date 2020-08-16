import esphome.codegen as cg
import esphome.config_validation as cv
import glob
from esphome.core import HexInt, ID
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import (
    CONF_ID, CONF_PORT,
    CONF_AUTH, CONF_USERNAME, CONF_PASSWORD)
from esphome.core import coroutine_with_priority

AUTO_LOAD = ['json', 'web_server_base']

CONF_APP_PATH = 'app_path'

web_server_spa_ns = cg.esphome_ns.namespace('web_server_spa')
WebServerSpa = web_server_spa_ns.class_('WebServerSpa', cg.Component, cg.Controller)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebServerSpa),
    cv.Optional(CONF_PORT, default=80): cv.port,
    cv.Required(CONF_APP_PATH): cv.directory,

    cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    paren = yield cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    yield cg.register_component(var, config)

    cg.add(paren.set_port(config[CONF_PORT]))

    for file_name in glob.iglob(config[CONF_APP_PATH] + '/**/*.*', recursive=True):

        if file_name.endswith(".map"):
            continue

        relative_path = file_name[len(config[CONF_APP_PATH]):].replace('\\', '/')
        file_id = relative_path.replace('/', '_').replace('-', '_').replace('.', '_')
        print(file_name)
        gen_id = ID(file_id, is_declaration=True, type=cg.uint8)

        data = bytearray(open(file_name, "rb").read())
        rhs = [HexInt(x) for x in data]
        file_content = cg.progmem_array(gen_id, rhs)

        content_type = 'text/html'

        cg.add(var.file(relative_path, content_type, file_content, len(data)))
