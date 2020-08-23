#include "web_server_spa.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/components/json/json_util.h"

#include "StreamString.h"

#include <cstdlib>

#ifdef USE_LOGGER
#include <esphome/components/logger/logger.h>
#endif

namespace esphome {
namespace web_server_spa {

static const char *TAG = "web_server_spa";

UrlMatch match_url(const std::string &url, bool only_domain = false) {
  UrlMatch match;
  match.valid = false;
  size_t domain_end = url.find('/', 1);
  if (domain_end == std::string::npos)
    return match;
  match.domain = url.substr(1, domain_end - 1);
  if (only_domain) {
    match.valid = true;
    return match;
  }
  if (url.length() == domain_end - 1)
    return match;
  size_t id_begin = domain_end + 1;
  size_t id_end = url.find('/', id_begin);
  match.valid = true;
  if (id_end == std::string::npos) {
    match.id = url.substr(id_begin, url.length() - id_begin);
    return match;
  }
  match.id = url.substr(id_begin, id_end - id_begin);
  size_t method_begin = id_end + 1;
  match.method = url.substr(method_begin, url.length() - method_begin);
  return match;
}

void WebServerSpa::file(const char *file_name, const std::string content_type, const uint8_t *prog_mem_p,
                        uint32_t length) {
  StaticResource resource;

  resource.content_type = content_type;
  resource.length = length;
  resource.progmem_data = prog_mem_p;

  served_files_[file_name] = resource;
}

void WebServerSpa::setup() {
  ESP_LOGCONFIG(TAG, "Setting up web server...");
  this->setup_controller();
  this->base_->init();

  this->events_.onConnect([this](AsyncEventSourceClient *client) {
    // Configure reconnect timeout
    client->send("", "ping", millis(), 30000);

#ifdef USE_SENSOR
    for (auto *obj : App.get_sensors())
      if (!obj->is_internal())
        client->send(this->sensor_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_SWITCH
    for (auto *obj : App.get_switches())
      if (!obj->is_internal())
        client->send(this->switch_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_BINARY_SENSOR
    for (auto *obj : App.get_binary_sensors())
      if (!obj->is_internal())
        client->send(this->binary_sensor_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_FAN
    for (auto *obj : App.get_fans())
      if (!obj->is_internal())
        client->send(this->fan_json(obj).c_str(), "state");
#endif

#ifdef USE_LIGHT
    for (auto *obj : App.get_lights())
      if (!obj->is_internal())
        client->send(this->light_json(obj).c_str(), "state");
#endif

#ifdef USE_TEXT_SENSOR
    for (auto *obj : App.get_text_sensors())
      if (!obj->is_internal())
        client->send(this->text_sensor_json(obj, obj->state).c_str(), "state");
#endif

#ifdef USE_COVER
    for (auto *obj : App.get_covers())
      if (!obj->is_internal())
        client->send(this->cover_json(obj).c_str(), "state");
#endif
  });

#ifdef USE_LOGGER
  if (logger::global_logger != nullptr)
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *message) { this->events_.send(message, "log", millis()); });
#endif
  this->base_->add_handler(&this->events_);
  this->base_->add_handler(this);
  this->base_->add_ota_handler();

  this->set_interval(10000, [this]() { this->events_.send("", "ping", millis(), 30000); });
}

void WebServerSpa::dump_config() {
  ESP_LOGCONFIG(TAG, "Web Server Spa:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), this->base_->get_port());
  if (this->using_auth()) {
    ESP_LOGCONFIG(TAG, "  Basic authentication enabled");
  }
}

float WebServerSpa::get_setup_priority() const { return setup_priority::WIFI - 1.0f; }

void WebServerSpa::static_serve(AsyncWebServerRequest *request, const char *static_resource) {
  for (auto it = served_files_.begin(); it != served_files_.end(); it++) {
    if (strcmp((*it).first, static_resource) == 0) {
      auto data = (*it).second;
      ESP_LOGD(TAG, "serving %s", static_resource);  //, r.content_type.c_str(), r.length);
      request->send_P(200, data.content_type.c_str(), data.progmem_data, (size_t) data.length);
      return;
    }
  }

  ESP_LOGD(TAG, "not serving %s not found", static_resource);
}

bool WebServerSpa::canHandle(AsyncWebServerRequest *request) {
  if (request->url() == "/")
    return true;

  for (auto it = served_files_.begin(); it != served_files_.end(); it++)
    if (request->url().substring(1) == (*it).first)
      return true;

  UrlMatch match = match_url(request->url().c_str(), true);
  if (!match.valid)
    return false;
#ifdef USE_SENSOR
  if (request->method() == HTTP_GET && match.domain == "sensor")
    return true;
#endif

#ifdef USE_SWITCH
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "switch")
    return true;
#endif

#ifdef USE_BINARY_SENSOR
  if (request->method() == HTTP_GET && match.domain == "binary_sensor")
    return true;
#endif

#ifdef USE_FAN
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "fan")
    return true;
#endif

#ifdef USE_LIGHT
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "light")
    return true;
#endif

#ifdef USE_TEXT_SENSOR
  if (request->method() == HTTP_GET && match.domain == "text_sensor")
    return true;
#endif

#ifdef USE_COVER
  if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "cover")
    return true;
#endif

  return false;
}

#ifdef USE_SENSOR
void WebServerSpa::on_sensor_update(sensor::Sensor *obj, float state) {
  this->events_.send(this->sensor_json(obj, state).c_str(), "state");
}
void WebServerSpa::handle_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (sensor::Sensor *obj : App.get_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServerSpa::sensor_json(sensor::Sensor *obj, float value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "sensor-" + obj->get_object_id();
    std::string state = value_accuracy_to_string(value, obj->get_accuracy_decimals());
    if (!obj->get_unit_of_measurement().empty())
      state += " " + obj->get_unit_of_measurement();
    root["state"] = state;
    root["value"] = value;
  });
}
#endif

#ifdef USE_TEXT_SENSOR
void WebServerSpa::on_text_sensor_update(text_sensor::TextSensor *obj, std::string state) {
  this->events_.send(this->text_sensor_json(obj, state).c_str(), "state");
}
void WebServerSpa::handle_text_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (text_sensor::TextSensor *obj : App.get_text_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->text_sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
std::string WebServerSpa::text_sensor_json(text_sensor::TextSensor *obj, const std::string &value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "text_sensor-" + obj->get_object_id();
    root["state"] = value;
    root["value"] = value;
  });
}
#endif

#ifdef USE_SWITCH
void WebServerSpa::on_switch_update(switch_::Switch *obj, bool state) {
  this->events_.send(this->switch_json(obj, state).c_str(), "state");
}
std::string WebServerSpa::switch_json(switch_::Switch *obj, bool value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "switch-" + obj->get_object_id();
    root["state"] = value ? "ON" : "OFF";
    root["value"] = value;
  });
}
void WebServerSpa::handle_switch_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (switch_::Switch *obj : App.get_switches()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->switch_json(obj, obj->state);
      request->send(200, "text/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      this->defer([obj]() { obj->turn_on(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      this->defer([obj]() { obj->turn_off(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_BINARY_SENSOR
void WebServerSpa::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  if (obj->is_internal())
    return;
  this->events_.send(this->binary_sensor_json(obj, state).c_str(), "state");
}
std::string WebServerSpa::binary_sensor_json(binary_sensor::BinarySensor *obj, bool value) {
  return json::build_json([obj, value](JsonObject &root) {
    root["id"] = "binary_sensor-" + obj->get_object_id();
    root["state"] = value ? "ON" : "OFF";
    root["value"] = value;
  });
}
void WebServerSpa::handle_binary_sensor_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (binary_sensor::BinarySensor *obj : App.get_binary_sensors()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;
    std::string data = this->binary_sensor_json(obj, obj->state);
    request->send(200, "text/json", data.c_str());
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_FAN
void WebServerSpa::on_fan_update(fan::FanState *obj) {
  if (obj->is_internal())
    return;
  this->events_.send(this->fan_json(obj).c_str(), "state");
}
std::string WebServerSpa::fan_json(fan::FanState *obj) {
  return json::build_json([obj](JsonObject &root) {
    root["id"] = "fan-" + obj->get_object_id();
    root["state"] = obj->state ? "ON" : "OFF";
    root["value"] = obj->state;
    if (obj->get_traits().supports_speed()) {
      switch (obj->speed) {
        case fan::FAN_SPEED_LOW:
          root["speed"] = "low";
          break;
        case fan::FAN_SPEED_MEDIUM:
          root["speed"] = "medium";
          break;
        case fan::FAN_SPEED_HIGH:
          root["speed"] = "high";
          break;
      }
    }
    if (obj->get_traits().supports_oscillation())
      root["oscillation"] = obj->oscillating;
  });
}
void WebServerSpa::handle_fan_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (fan::FanState *obj : App.get_fans()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->fan_json(obj);
      request->send(200, "text/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("speed")) {
        String speed = request->getParam("speed")->value();
        call.set_speed(speed.c_str());
      }
      if (request->hasParam("oscillation")) {
        String speed = request->getParam("oscillation")->value();
        auto val = parse_on_off(speed.c_str());
        switch (val) {
          case PARSE_ON:
            call.set_oscillating(true);
            break;
          case PARSE_OFF:
            call.set_oscillating(false);
            break;
          case PARSE_TOGGLE:
            call.set_oscillating(!obj->oscillating);
            break;
          case PARSE_NONE:
            request->send(404);
            return;
        }
      }
      this->defer([call]() { call.perform(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      this->defer([obj]() { obj->turn_off().perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
#endif

#ifdef USE_LIGHT
void WebServerSpa::on_light_update(light::LightState *obj) {
  if (obj->is_internal())
    return;
  this->events_.send(this->light_json(obj).c_str(), "state");
}
void WebServerSpa::handle_light_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (light::LightState *obj : App.get_lights()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->light_json(obj);
      request->send(200, "text/json", data.c_str());
    } else if (match.method == "toggle") {
      this->defer([obj]() { obj->toggle().perform(); });
      request->send(200);
    } else if (match.method == "turn_on") {
      auto call = obj->turn_on();
      if (request->hasParam("brightness"))
        call.set_brightness(request->getParam("brightness")->value().toFloat() / 255.0f);
      if (request->hasParam("r"))
        call.set_red(request->getParam("r")->value().toFloat() / 255.0f);
      if (request->hasParam("g"))
        call.set_green(request->getParam("g")->value().toFloat() / 255.0f);
      if (request->hasParam("b"))
        call.set_blue(request->getParam("b")->value().toFloat() / 255.0f);
      if (request->hasParam("white_value"))
        call.set_white(request->getParam("white_value")->value().toFloat() / 255.0f);
      if (request->hasParam("color_temp"))
        call.set_color_temperature(request->getParam("color_temp")->value().toFloat());

      if (request->hasParam("flash")) {
        float length_s = request->getParam("flash")->value().toFloat();
        call.set_flash_length(static_cast<uint32_t>(length_s * 1000));
      }

      if (request->hasParam("transition")) {
        float length_s = request->getParam("transition")->value().toFloat();
        call.set_transition_length(static_cast<uint32_t>(length_s * 1000));
      }

      if (request->hasParam("effect")) {
        const char *effect = request->getParam("effect")->value().c_str();
        call.set_effect(effect);
      }

      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else if (match.method == "turn_off") {
      auto call = obj->turn_off();
      if (request->hasParam("transition")) {
        auto length = (uint32_t) request->getParam("transition")->value().toFloat() * 1000;
        call.set_transition_length(length);
      }
      this->defer([call]() mutable { call.perform(); });
      request->send(200);
    } else {
      request->send(404);
    }
    return;
  }
  request->send(404);
}
std::string WebServerSpa::light_json(light::LightState *obj) {
  return json::build_json([obj](JsonObject &root) {
    root["id"] = "light-" + obj->get_object_id();
    root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";
    obj->dump_json(root);
  });
}
#endif

#ifdef USE_COVER
void WebServerSpa::on_cover_update(cover::Cover *obj) {
  if (obj->is_internal())
    return;
  this->events_.send(this->cover_json(obj).c_str(), "state");
}
void WebServerSpa::handle_cover_request(AsyncWebServerRequest *request, UrlMatch match) {
  for (cover::Cover *obj : App.get_covers()) {
    if (obj->is_internal())
      continue;
    if (obj->get_object_id() != match.id)
      continue;

    if (request->method() == HTTP_GET) {
      std::string data = this->cover_json(obj);
      request->send(200, "text/json", data.c_str());
      continue;
    }

    auto call = obj->make_call();
    if (match.method == "open") {
      call.set_command_open();
    } else if (match.method == "close") {
      call.set_command_close();
    } else if (match.method == "stop") {
      call.set_command_stop();
    } else if (match.method != "set") {
      request->send(404);
      return;
    }

    auto traits = obj->get_traits();
    if ((request->hasParam("position") && !traits.get_supports_position()) ||
        (request->hasParam("tilt") && !traits.get_supports_tilt())) {
      request->send(409);
      return;
    }

    if (request->hasParam("position"))
      call.set_position(request->getParam("position")->value().toFloat());
    if (request->hasParam("tilt"))
      call.set_tilt(request->getParam("tilt")->value().toFloat());

    this->defer([call]() mutable { call.perform(); });
    request->send(200);
    return;
  }
  request->send(404);
}
std::string WebServerSpa::cover_json(cover::Cover *obj) {
  return json::build_json([obj](JsonObject &root) {
    root["id"] = "cover-" + obj->get_object_id();
    root["state"] = obj->is_fully_closed() ? "CLOSED" : "OPEN";
    root["value"] = obj->position;
    root["current_operation"] = cover::cover_operation_to_str(obj->current_operation);

    if (obj->get_traits().get_supports_tilt())
      root["tilt"] = obj->tilt;
  });
}
#endif

void WebServerSpa::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "handleRequest %s", request->url().c_str());
  // TODO: Auth support
  //   if (this->using_auth() && !request->authenticate(this->username_, this->password_)) {
  //     return request->requestAuthentication();
  //   }

  if (request->url() == "/") {
    this->static_serve(request, "index.html");
    return;
  }

  UrlMatch match = match_url(request->url().c_str());

  ESP_LOGD(TAG, "handleRequest Url match: %d domain: %s", match.valid, match.domain.c_str());
#ifdef USE_SENSOR
  if (match.domain == "sensor") {
    this->handle_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_SWITCH
  if (match.domain == "switch") {
    this->handle_switch_request(request, match);
    return;
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (match.domain == "binary_sensor") {
    this->handle_binary_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_FAN
  if (match.domain == "fan") {
    this->handle_fan_request(request, match);
    return;
  }
#endif

#ifdef USE_LIGHT
  if (match.domain == "light") {
    this->handle_light_request(request, match);
    return;
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (match.domain == "text_sensor") {
    this->handle_text_sensor_request(request, match);
    return;
  }
#endif

#ifdef USE_COVER
  if (match.domain == "cover") {
    this->handle_cover_request(request, match);
    return;
  }
#endif

  // substring 1 to remove first '/'
  this->static_serve(request, request->url().substring(1).c_str());
}

bool WebServerSpa::isRequestHandlerTrivial() { return false; }

}  // namespace web_server_spa
}  // namespace esphome
