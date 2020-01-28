#include "library.h"

#include "config.h"
#include "managers/services.h"
#include "periphery/periphery.h"
#include "periphery/peripheryFactory.h"
#include "periphery/peripheryTask.h"

namespace bernd_box {
namespace library {

Library Library::library_ = Library(Services::getMqtt());

Library& Library::getLibrary() { return library_; }

Library::Library(Mqtt& mqtt) : mqtt_(mqtt) {}

Result Library::add(const JsonObjectConst& doc) {
  const char* who = __PRETTY_FUNCTION__;

  JsonVariantConst name = doc[F("name")];
  if (name.isNull() || !name.is<char*>()) {
    mqtt_.sendError(who, "Missing property: name (string)");
    return Result::kFailure;
  }

  // Check if element is present
  if (peripheries_.count(name) > 0) {
    mqtt_.sendError(who, "This name has already been added.");
    return Result::kFailure;
  }

  // Not present, so create new
  std::shared_ptr<Periphery> periphery(
      Services::getPeripheryFactory().createPeriphery(doc));
  if (periphery->isValid() == false) {
    return Result::kFailure;
  }

  peripheries_.insert({name.as<String>(), periphery});

  return Result::kSuccess;
}

Result Library::remove(const JsonObjectConst& doc) {
  const char* who = __PRETTY_FUNCTION__;

  JsonVariantConst name = doc[F("name")];
  if (name.isNull() || !name.is<char*>()) {
    mqtt_.sendError(who, String(F("Missing property: name (string)")));
    return Result::kFailure;
  }

  auto iterator = peripheries_.find(name.as<String>());
  if (iterator != peripheries_.end()) {
    if (iterator->second.use_count() > 1) {
      mqtt_.sendError(
          who, String(F("Object is still in use. Try again later for ")) +
                   name.as<String>());
      return Result::kFailure;
    }
    peripheries_.erase(iterator);
  }

  return Result::kSuccess;
}

Result Library::execute(const JsonObjectConst& doc) {
  const char* who = __PRETTY_FUNCTION__;

  JsonVariantConst name = doc[F("name")];
  if (name.isNull() || !name.is<char*>()) {
    mqtt_.sendError(who, "Missing property: name (string)");
    return Result::kFailure;
  }

  std::map<String, std::shared_ptr<Periphery>>::iterator iterator =
      peripheries_.find(name.as<String>());
  if (iterator == peripheries_.end()) {
    mqtt_.sendError(who, "No object found with name " + name.as<String>());
    return Result::kFailure;
  }
  Serial.println("Periphery = null ");
  TaskFactory& task_factory = iterator->second->getTaskFactory(doc);

  JsonVariantConst parameter = doc[F("parameter")];
  if (parameter.isNull()) {
    return Result::kFailure;
  }

  std::unique_ptr<PeripheryTask> peripheryTask(
      task_factory.createTask(iterator->second, parameter));
  Result result = peripheryTask->execute();
  if (result != Result::kSuccess) {
    mqtt_.sendError(who, "The chosen periphery of type " +
                             String(iterator->second->getType()) +
                             " can not execute this task.");
  }
  return result;
}

std::shared_ptr<Periphery> Library::getPeriphery(const String& name) {
  return peripheries_.find(name)->second;
}

Mqtt& Library::getMQTT() { return mqtt_; }

Result Library::handleCallback(char* topic, uint8_t* payload,
                               unsigned int length) {
  const char* who = __PRETTY_FUNCTION__;

  char* command = strrchr(topic, '/');
  command++;

  DynamicJsonDocument doc(BB_MQTT_JSON_PAYLOAD_SIZE);
  const DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    mqtt_.sendError(who, String(F("Deserialize failed: ")) + error.c_str());
    return Result::kFailure;
  }

  int action_topic_add = strncmp(command, BB_MQTT_TOPIC_ADD_SUFFIX,
                                 strlen(BB_MQTT_TOPIC_ADD_SUFFIX));
  if (action_topic_add == 0) return add(doc.as<JsonVariantConst>());
  int action_topic_remove = strncmp(command, BB_MQTT_TOPIC_REMOVE_PREFIX,
                                    strlen(BB_MQTT_TOPIC_REMOVE_PREFIX));
  if (action_topic_remove == 0) return remove(doc.as<JsonVariantConst>());
  int action_topic_execute = strncmp(command, BB_MQTT_TOPIC_EXECUTE_PREFIX,
                                     strlen(BB_MQTT_TOPIC_EXECUTE_PREFIX));
  if (action_topic_execute == 0) return execute(doc.as<JsonVariantConst>());
  mqtt_.sendError(
      who,
      String(F("Topic was neither add, nor remove, nor execute: ")) + command);
  return Result::kFailure;
}
}  // namespace library
}  // namespace bernd_box
