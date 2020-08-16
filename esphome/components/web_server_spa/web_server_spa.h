#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/components/web_server_base/web_server_base.h"

#include <map>

namespace esphome {
namespace web_server_spa {

/// Internal helper struct that is used to parse incoming URLs
struct UrlMatch {
  std::string domain;  ///< The domain of the component, for example "sensor"
  std::string id;      ///< The id of the device that's being accessed, for example "living_room_fan"
  std::string method;  ///< The method that's being called, for example "turn_on"
  bool valid;          ///< Whether this match is valid
};

struct StaticResource {
  uint32_t length;
  std::string content_type;
  const uint8_t *progmem_data;
};

/** This class allows users to create a Single Page Apllication (SPA) web server with their ESP nodes.
 *
 * Behind the scenes it's using AsyncWebServer to set up the server.
 */
class WebServerSpa : public Controller, public Component, public AsyncWebHandler {
 public:
  WebServerSpa(web_server_base::WebServerBase *base) : base_(base) {}

  void file(const char *file_name, const std::string content_type, const uint8_t *prog_mem_p, uint32_t length);
  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override;

  /// Override the web handler's canHandle method.
  bool canHandle(AsyncWebServerRequest *request) override;
  /// Override the web handler's handleRequest method.
  void handleRequest(AsyncWebServerRequest *request) override;
  /// This web handle is not trivial.
  bool isRequestHandlerTrivial() override;

  void static_serve(AsyncWebServerRequest *request, const char *static_resource);

  bool using_auth() { return username_ != nullptr && password_ != nullptr; }

#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
  /// Handle a sensor request under '/sensor/<id>'.
  void handle_sensor_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the sensor state with its value as a JSON string.
  std::string sensor_json(sensor::Sensor *obj, float value);
#endif

#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;

  /// Handle a switch request under '/switch/<id>/</turn_on/turn_off/toggle>'.
  void handle_switch_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the switch state with its value as a JSON string.
  std::string switch_json(switch_::Switch *obj, bool value);
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;

  /// Handle a binary sensor request under '/binary_sensor/<id>'.
  void handle_binary_sensor_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the binary sensor state with its value as a JSON string.
  std::string binary_sensor_json(binary_sensor::BinarySensor *obj, bool value);
#endif

#ifdef USE_FAN
  void on_fan_update(fan::FanState *obj) override;

  /// Handle a fan request under '/fan/<id>/</turn_on/turn_off/toggle>'.
  void handle_fan_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the fan state as a JSON string.
  std::string fan_json(fan::FanState *obj);
#endif

#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;

  /// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
  void handle_light_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the light state as a JSON string.
  std::string light_json(light::LightState *obj);
#endif

#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, std::string state) override;

  /// Handle a text sensor request under '/text_sensor/<id>'.
  void handle_text_sensor_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the text sensor state with its value as a JSON string.
  std::string text_sensor_json(text_sensor::TextSensor *obj, const std::string &value);
#endif

#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;

  /// Handle a cover request under '/cover/<id>/<open/close/stop/set>'.
  void handle_cover_request(AsyncWebServerRequest *request, UrlMatch match);

  /// Dump the cover state as a JSON string.
  std::string cover_json(cover::Cover *obj);
#endif

 protected:
  web_server_base::WebServerBase *base_;
  std::map<const char *, StaticResource> served_files_{};
  AsyncEventSource events_{"/events"};
  const char *username_{nullptr};
  const char *password_{nullptr};
};

}  // namespace web_server_spa
}  // namespace esphome
