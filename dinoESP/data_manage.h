#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <DFRobotDFPlayerMini.h>

extern uint8_t currentVolume;
extern DFRobotDFPlayerMini myDFPlayer;
extern uint32_t repeatIntervalMs;
extern uint32_t nextAutoStartTime;

struct Step {
  bool outPins[8];
  uint32_t duration;
  uint8_t waitPin;
  uint8_t soundNum;
};

std::vector<Step> sequence;

bool loadConfig() {
  if (!LittleFS.exists("/config.json")) return false;
  
  File file = LittleFS.open("/config.json", "r");
  if (!file) return false;

  JsonDocument doc;
  doc.clear(); 
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) return false;

  if (doc.containsKey("vol")) {
    currentVolume = constrain(doc["vol"].as<int>(), 0, 30);
  }

  if (doc.containsKey("interval")) {
    int minutes = doc["interval"].as<int>();
    repeatIntervalMs = minutes * 60 * 1000;
    if (repeatIntervalMs > 0) {
      nextAutoStartTime = millis() + repeatIntervalMs;
    }
  } else {
    repeatIntervalMs = 0;
  }

  if (!doc["steps"].is<JsonArray>()) return false;

  JsonArray arr = doc["steps"].as<JsonArray>();
  // sequence.clear();
  std::vector<Step>().swap(sequence);
  sequence.reserve(arr.size());
  
  for (JsonObject obj : arr) {
    Step s;
    JsonArray pArr = obj["p"];
    for (int i = 0; i < 8; i++) {
      s.outPins[i] = pArr[i].as<bool>(); 
    }
    s.soundNum = obj["s"] | 0;
    s.duration = obj["t"] | 1000;
    s.waitPin = obj["in"] | 0; 
    
    sequence.push_back(s);
  }
  myDFPlayer.volume(currentVolume);
  return true;
}

void saveConfig(uint8_t *data, size_t len) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);
  
  if (error) {return;}

  File file = LittleFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}
