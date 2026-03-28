#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "web.h"
#include "data_manage.h"
#include "icon.h"

#define VERSION "1.0.0"
#define WIFI_SSID "ESP32_00001"
#define WIFI_PSWD "f_00001"
#define AUTH_PASSWORD "f_00001"
#define DEV_AUTH_PASSWORD "123456"
#define RELAY_ON  HIGH
#define RELAY_OFF LOW
#define PIN_TO_WAIT LOW

AsyncWebServer server(80);
HardwareSerial mySerial(2); // UART2
DFRobotDFPlayerMini myDFPlayer;

bool isLocked = false; // dev lock
uint8_t currentVolume = 10; // 0-30, 10 по умолчанию
uint8_t selectedTrack = 1; // 1-20, 1 по умолчанию
int currentStep = 0;
unsigned long lastStepTime = 0;
bool stepInitialized = false;
uint32_t repeatIntervalMs = 0; // Интервал из конфига (в мс)
uint32_t nextAutoStartTime = 0; // Время следующего запуска
bool isSequenceRunning = false; // Флаг: крутится ли программа сейчас
bool autoStartActive = false; // Флаг, что текущий запуск автоматический
volatile bool playRequested = false; 
uint32_t lastPlayerCmdTime = 0;

constexpr uint8_t ledPin = 2; // pin на плате есп(синий)
const uint8_t relayPins[] = {23, 22, 1, 3, 21, 19, 18, 5}; //юарт0 рх тх 1 3 
const uint8_t inputMap[] = {36, 39, 34, 35, 32, 33, 25, 26}; // 34, 35, 36, 39 только входы 

bool isAuthenticated(AsyncWebServerRequest *request){
    if (request->hasHeader("Cookie")) {
        return request->header("Cookie").indexOf("ESPSESSIONID=1234") != -1;
    }
    return false;
}

bool isDEV(AsyncWebServerRequest *request){
    if (request->hasHeader("Cookie")) {
        return request->header("Cookie").indexOf("ESPDEVSESSIONID=12345") != -1;
    }
    return false;
}

void checkInitialLock() {
  if (!LittleFS.exists("/dev.json")) return;
  File f = LittleFS.open("/dev.json", "r");
  if (f) {
    String content = f.readString();
    f.close();
    if (content.indexOf("\"blocked\":1") != -1 || content.indexOf("\"blocked\": 1") != -1) {
      isLocked = true;
    } else {
      isLocked = false;
    }
  }
}

void setup() {
  // ждем стабильное питание(критично для плеера)
  delay(3000);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Выключен по умолчанию

  // RX модуля=17пинЕСП TX модуля=16пинЕСП
  mySerial.begin(9600, SERIAL_8N1, 4, 15);

  if (!myDFPlayer.begin(mySerial)) {
    digitalWrite(ledPin, HIGH);
  }

  for (int i = 0; i < 8; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
    pinMode(relayPins[i], OUTPUT);
  }

  for (int i = 0; i < 8; i++) {
    // INPUT_PULLUP +, INPUT_PULLDOWN -
    pinMode(inputMap[i], INPUT); 
  }

  // программный ресет, для корректной инициализации
  myDFPlayer.reset();
  delay(2000);

  if (!LittleFS.begin(true)) return;
  loadConfig();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP(WIFI_SSID, WIFI_PSWD);

  server.on("/icon.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "image/png", icon_png, sizeof(icon_png));
  });

  server.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasArg("delta")) {
      currentVolume = request->arg("delta").toInt();
    } else {
      currentVolume = 10;
    }
    if (currentVolume > 30) currentVolume = 30;
    if (currentVolume < 0) currentVolume = 0;
    myDFPlayer.volume(currentVolume);
    request->send(200, "text/plain", String(currentVolume));
  });

  server.on("/playtest", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasArg("track")) {
      selectedTrack = request->arg("track").toInt();
      playRequested = true;
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing track");
    }
  });

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", login_html);
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasArg("pass") && request->arg("pass") == AUTH_PASSWORD) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Ok");
        response->addHeader("Location", "/");
        //; Max-Age=300
        response->addHeader("Set-Cookie", "ESPSESSIONID=1234; Path=/; HttpOnly"); 
        request->send(response);
    } else {
        request->redirect("/login?error=1");
    }
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!isAuthenticated(request)) return request->redirect("/login");
    request->send_P(200, "text/html", index_html);
  });

  server.on("/get-data", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!isAuthenticated(request)) return request->send(401);
    request->send(LittleFS, "/config.json", "application/json");
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(!isAuthenticated(request)) return request->send(401);

    File file = LittleFS.open("/config.json", (index == 0) ? "w" : "a");
    if (file) {
        file.write(data, len);
        file.close();
    }
    if (index + len == total) {
        if (loadConfig()) {
            currentStep = 0; 
            stepInitialized = false;
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Config Error");
        }
    }
  });

  // dev security
  server.on("/devt00ls", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!isDEV(request)) return request->redirect("/devlogin");
    request->send_P(200, "text/html", devtools_html);
  });

  server.on("/devlogin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", devlogin_html);
  });

  server.on("/devlogin", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasArg("pass") && request->arg("pass") == DEV_AUTH_PASSWORD) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Ok");
        response->addHeader("Location", "/devt00ls");
        response->addHeader("Set-Cookie", "ESPDEVSESSIONID=12345; Path=/; HttpOnly; Max-Age=300"); // 300 sec = 5 min max age
        request->send(response);
    } else {
        request->redirect("/devlogin?error=1");
    }
  });

  server.on("/get-dev-data", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!isDEV(request)) return request->send(401);

    String blockedValue = "0";
    if (LittleFS.exists("/dev.json")) {
        File f = LittleFS.open("/dev.json", "r");
        String content = f.readString();
        f.close();

        if (content.indexOf("\"blocked\":1") != -1) {
            blockedValue = "1";
        }
    }

    String json = "{";
    json += "\"blocked\":" + blockedValue + ",";
    json += "\"build\":\"" + String(__DATE__) + " " + String(__TIME__) + "\",";
    json += "\"ver\":\"" + String(VERSION) + "\",";
    json += "\"ssid\":\"" + String(WIFI_SSID) + "\",";
    json += "\"wifi_pswd\":\"" + String(WIFI_PSWD) + "\",";
    json += "\"dev_auth_pswd\":\"" + String(DEV_AUTH_PASSWORD) + "\",";
    json += "\"auth_pswd\":\"" + String(AUTH_PASSWORD) + "\"";
    json += "}";

    request->send(200, "application/json", json);
  });

  server.on("/save-status", HTTP_POST, [](AsyncWebServerRequest *request) {
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      
      if(!isDEV(request)) return request->send(401);

      String body = String((char*)data).substring(0, len);
      String val = (body.indexOf("\"blocked\":1") != -1) ? "1" : "0";

      File f = LittleFS.open("/dev.json", "w");
      if (f) {
          f.print("{\"blocked\":" + val + "}");
          f.close();
          request->send(200, "text/plain", "OK. Rebooting...");
          delay(200); 
          ESP.restart(); 
      } else {
          request->send(500, "text/plain", "FS Error");
      }
  });

  checkInitialLock();
  server.begin();
}

void loop() {
  if (playRequested) {
    if (millis() - lastPlayerCmdTime >= 1250) {
      myDFPlayer.playFolder(1, selectedTrack);
      lastPlayerCmdTime = millis();
      playRequested = false;
    } else {
      playRequested = false;
    }
  }

  if (isLocked) {
    // Останавливаем выполнение программы, если она шла
    isSequenceRunning = false;
    // Выключаем все реле, кроме первого
    for (int i = 1; i < 8; i++) digitalWrite(relayPins[i], RELAY_OFF);
    // Мигаем первым реле без delay
    static uint32_t lastFlashTime = 0;
    if (millis() - lastFlashTime >= 5000) {
      lastFlashTime = millis();
      digitalWrite(relayPins[0], !digitalRead(relayPins[0]));
    }
    // Выходим из loop, чтобы код ниже не выполнялся
    return;
  }

  if (sequence.empty()) return;

  // --- 2. ЛОГИКА ОЖИДАНИЯ СТАРТА (Если программа НЕ идет) ---
  
  if (!isSequenceRunning) {
    bool startTriggered = false;
    autoStartActive = false;

    // 1. ПРИОРИТЕТ: Проверка датчика (физический приход)
    if (sequence[0].waitPin > 0) {
      uint8_t firstPin = inputMap[sequence[0].waitPin - 1];
      if (digitalRead(firstPin) == PIN_TO_WAIT) {
        delay(50); // Антидребезг
        startTriggered = true;
        autoStartActive = false; // Это живой человек, НЕ автостарт
      }
    }
    // 2. ЗАПАСНОЙ ВАРИАНТ: Таймер (если игрока нет)
    // Сработает только если старт еще не произошел от датчика
    if (!startTriggered && repeatIntervalMs > 0) {
      if (millis() >= nextAutoStartTime) {
        startTriggered = true;
        autoStartActive = true; // ПОМЕЧАЕМ как автостарт, чтобы пропустить ожидание входа
      }
    }
    // 3. ЕСЛИ НИЧЕГО НЕ ЗАДАНО: Крутим без остановки
    if (!startTriggered && sequence[0].waitPin == 0 && repeatIntervalMs == 0) {
      startTriggered = true;
    }

    if (startTriggered) {
        isSequenceRunning = true;
        currentStep = 0;
        stepInitialized = false;
        // Сдвигаем следующий автостарт сразу при запуске
        if (repeatIntervalMs > 0) {
          nextAutoStartTime = millis() + repeatIntervalMs;
        }
      } else {
        return; // Продолжаем ждать входа или таймера
      }
  }

  // --- 3. ВЫПОЛНЕНИЕ ТЕКУЩЕГО ШАГА (Только если isSequenceRunning == true) ---

  // Инициализация шага (срабатывает один раз в начале каждого шага)
  if (!stepInitialized) {
    // Включаем реле согласно конфигу
    for (int i = 0; i < 8; i++) {
      digitalWrite(relayPins[i], sequence[currentStep].outPins[i] ? RELAY_ON : RELAY_OFF);
    }
    // Запускаем звук, если указан трек
    if (sequence[currentStep].soundNum > 0) {
      myDFPlayer.playFolder(1, sequence[currentStep].soundNum);
    }
    lastStepTime = millis();
    stepInitialized = true;
  }

  bool stepDone = false;

  // Проверка: завершен ли текущий шаг?
  if (sequence[currentStep].waitPin > 0) {
    
    // --- ВОТ ТУТ МЫ ЮЗАЕМ autoStartActive! ---
    if (currentStep == 0 && autoStartActive) {
      // Если это автостарт по таймеру — пролетаем ожидание датчика на 1-м шаге
      stepDone = true; 
      autoStartActive = false; // Сбрасываем, чтобы на 2-м шаге датчик работал реально
    } 
    else {
      // Обычный режим: честно ждем сигнал с датчика
      uint8_t pinToWait = inputMap[sequence[currentStep].waitPin - 1];
      if (digitalRead(pinToWait) == PIN_TO_WAIT) {
        delay(50); 
        stepDone = true;
        autoStartActive = false; // На всякий случай гасим флаг, если зашел человек
      }
    }
    
  } else {
    // Если шаг просто по времени — ждем таймер
    if (millis() - lastStepTime >= sequence[currentStep].duration) {
      stepDone = true;
    }
  }

  // --- 4. ПЕРЕХОД К СЛЕДУЮЩЕМУ ШАГУ ИЛИ ЗАВЕРШЕНИЕ ---
  if (stepDone) {
    currentStep++;
    stepInitialized = false;

    // Если прошли все шаги
    if (currentStep >= sequence.size()) {
      // Выключаем всё оборудование
      for (int i = 0; i < 8; i++) digitalWrite(relayPins[i], RELAY_OFF);
      
      isSequenceRunning = false;
      autoStartActive = false;
      currentStep = 0;
      
      // На всякий случай обновляем таймер еще раз после окончания всей работы
      if (repeatIntervalMs > 0) {
        nextAutoStartTime = millis() + repeatIntervalMs;
      }
    }
  }
}

