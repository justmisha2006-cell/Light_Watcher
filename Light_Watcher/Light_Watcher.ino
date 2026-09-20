/* Light-Watcher firmware V2.2 (Multi-WiFi, Modern Telegram & Advanced Time/Test Commands Edition + OTA)
Repository: https://github.com/Stanislav-developer/Light_Watcher
Author: Stanislav Turii (GitHub: https://github.com/Stanislav-developer || Youtube: https://www.youtube.com/@TehnoMaisterna)
Date: 2026.09.16

НАЛАШТУВАННЯ ДЛЯ ЗАЛИВКИ ПРОШИВКИ (ESP32-C3):
У Tools:
1. Board: "ESP32C3 Dev Module"
2. USB CDC On Boot: "Enabled" (Обов'язково для Serial Monitor)
3. Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)" або "Minimal SPIFFS (Large APPS with OTA)" для повноцінного OTA
4. Решта налаштувань: за замовчуванням
*/

// Підключення бібліотек
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>

// Конфігурація пінів
#define readPin 3 // Аналоговий пін (ADC1)

// Налаштування фільтрації хибних спрацьовувань
#define ADC_THRESHOLD 3000   
#define DEBOUNCE_SAMPLES 15  
#define DEBOUNCE_DELAY_MS 100 

// Таймаут WDT в секундах
#define WDT_TIMEOUT 15

// Стандартне правило для України (Автоматичний переход: Зима UTC+2, Літо UTC+3)
#define TZ_UKRAINE_AUTO "EET-2EEST,M3.5.0/3,M10.5.0/4"

// Дані конфігурації
String ssid1 = "";
String password1 = "";
String ssid2 = "";
String password2 = "";
String botToken = "";
String chatId = "";

int wifiTimeout = 30; // Таймаут підключення до Wi-Fi у секундах за замовчуванням

// Налаштування 2-х груп та їх веток (Topics)
String groupId1 = "";
String topicId1 = "";
String groupId2 = "";
String topicId2 = "";

const char* ntp1 = "pool.ntp.org";
const char* ntp2 = "time.google.com";
const char* ntp3 = "time.cloudflare.com";

// Об'єкти
Preferences preferences;
WiFiClientSecure client;
UniversalTelegramBot bot("", client);
WebServer server(80);
DNSServer dnsServer;

// Глобальні змінні
bool powerStatus = true;
bool messageFlag = false;
bool lastOutageDetect = false;
bool missMessage = false;
int readValue = 0;
int powerOutageCount = 0;

unsigned long powerOffTime = 0;
unsigned long powerOnTime = 0;
time_t powerOffTimestamp = 0;
time_t powerOnTimestamp = 0;
time_t currentTimestamp = 0;

String currentTZ = "";
String powerOffFormattedTime;
String powerOnFormattedTime;
String lastMissMessage;

// HTML код розмітки WEB Інтерфейсу
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="uk">
<head>
  <title>Light Watcher V2.2 Setup</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-color: #f1f5f9;
      --card-bg: #ffffff;
      --accent: #2563eb;
      --accent-hover: #1d4ed8;
      --text-main: #0f172a;
      --text-muted: #64748b;
      --border-color: #e2e8f0;
      --input-bg: #f8fafc;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', sans-serif; }
    body { background-color: var(--bg-color); color: var(--text-main); display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: var(--card-bg); border-radius: 16px; box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.05), 0 8px 10px -6px rgba(0, 0, 0, 0.01); width: 100%; max-width: 520px; padding: 32px; border: 1px solid var(--border-color); }
    .header { text-align: center; margin-bottom: 24px; }
    .header h1 { font-size: 24px; font-weight: 700; color: var(--text-main); letter-spacing: -0.5px; }
    .header p { font-size: 14px; color: var(--text-muted); margin-top: 4px; }
    .badge { display: inline-block; background: #eff6ff; color: #2563eb; font-size: 12px; font-weight: 600; padding: 4px 12px; border-radius: 20px; margin-bottom: 12px; border: 1px solid #bfdbfe; }
    .section-title { font-size: 12px; font-weight: 700; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.8px; margin: 20px 0 10px 0; border-bottom: 1px solid var(--border-color); padding-bottom: 4px; }
    .form-group { margin-bottom: 14px; }
    .form-row { display: flex; gap: 10px; }
    .form-row .form-group { flex: 1; }
    label { display: block; font-size: 13px; font-weight: 600; margin-bottom: 6px; color: var(--text-main); }
    input[type=text], input[type=password], input[type=number] { width: 100%; padding: 10px 14px; background: var(--input-bg); border: 1px solid var(--border-color); border-radius: 8px; font-size: 14px; transition: all 0.2s ease; outline: none; }
    input:focus { border-color: var(--accent); box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15); background: #fff; }
    .btn-submit { width: 100%; padding: 12px; background-color: var(--accent); color: white; border: none; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; transition: background-color 0.2s ease; margin-top: 15px; }
    .btn-submit:hover { background-color: var(--accent-hover); }
    .footer { margin-top: 24px; text-align: center; border-top: 1px solid var(--border-color); padding-top: 16px; font-size: 12px; color: var(--text-muted); }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <span class="badge">Light Watcher v2.2</span>
      <h1>Налаштування системи</h1>
      <p>Заповніть конфігураційні дані пристрою</p>
    </div>
    <form action="/save" method="POST">
      
      <div class="section-title">Основна мережа Wi-Fi (№1)</div>
      <div class="form-group">
        <label>SSID 1 (Назва мережі)</label>
        <input type="text" name="ssid1" placeholder="Основна мережа" required>
      </div>
      <div class="form-group">
        <label>Пароль Wi-Fi 1</label>
        <input type="password" name="pass1" placeholder="Пароль мережі" required>
      </div>

      <div class="section-title">Резервна мережа Wi-Fi (№2)</div>
      <div class="form-group">
        <label>SSID 2 (Необов'язково)</label>
        <input type="text" name="ssid2" placeholder="Резервна мережа">
      </div>
      <div class="form-group">
        <label>Пароль Wi-Fi 2</label>
        <input type="password" name="pass2" placeholder="Пароль резервної мережі">
      </div>

      <div class="section-title">Налаштування таймауту мережі</div>
      <div class="form-group">
        <label>Час очікування підключення (сек)</label>
        <input type="number" name="wftime" placeholder="30" value="30" min="10" max="300" required>
      </div>

      <div class="section-title">Telegram Основне</div>
      <div class="form-group">
        <label>Bot Token</label>
        <input type="text" name="token" placeholder="Токен бота" required>
      </div>
      <div class="form-group">
        <label>ID Власника (Chat ID)</label>
        <input type="text" name="owner" placeholder="Ваш особистий Telegram ID" required>
      </div>

      <div class="section-title">Група №1 та Ветка (Topic)</div>
      <div class="form-row">
        <div class="form-group">
          <label>ID Групи 1</label>
          <input type="text" name="g1" placeholder="-100123456789">
        </div>
        <div class="form-group">
          <label>ID Ветки 1 (Topic)</label>
          <input type="number" name="t1" placeholder="0 якщо немає">
        </div>
      </div>

      <div class="section-title">Група №2 та Ветка (Topic)</div>
      <div class="form-row">
        <div class="form-group">
          <label>ID Групи 2</label>
          <input type="text" name="g2" placeholder="-100987654321">
        </div>
        <div class="form-group">
          <label>ID Ветки 2 (Topic)</label>
          <input type="number" name="t2" placeholder="0 якщо немає">
        </div>
      </div>

      <button type="submit" class="btn-submit">ЗБЕРЕГТИ ТА ПЕРЕЗАПУСТИТИ</button>
    </form>

    <div class="footer">
      Розробник: <b>Stanislav Turii</b>
    </div>
  </div>
</body>
</html>
)rawliteral";

// Перевірка та фільтрація напруги (усереднення вибірки)
bool checkPowerStatus() {
  int highCount = 0;
  for (int i = 0; i < DEBOUNCE_SAMPLES; i++) {
    if (analogRead(readPin) >= ADC_THRESHOLD) {
      highCount++;
    }
    delay(DEBOUNCE_DELAY_MS / DEBOUNCE_SAMPLES);
  }
  return (highCount >= (DEBOUNCE_SAMPLES * 0.75));
}

// Конвертація секунд у дні, години та хвилини
String formatDuration(time_t seconds) {
  if (seconds < 60) return "менше хвилини";

  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;

  String result = "";
  if (days > 0) {
    result += String(days) + " д. ";
  }
  if (hours > 0 || days > 0) {
    result += String(hours) + " год. ";
  }
  result += String(minutes) + " хв.";

  return result;
}

// Отримання точного часу у форматі ГОД:ХВ
String getFormattedShortTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--";
  }
  char timeStr[16];
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  return String(timeStr);
}

// Отримання повного часу (ДД.ММ.РРРР HH:MM)
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Час не синхронізовано";
  }
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M", &timeinfo);
  return String(timeStr);
}

// Генерація шаблону повідомлення про відключення (Мінімалістичний)
String buildPowerOffMessage(String eventTime, String lightDuration) {
  String msg = "⚡❌ <b>Світло зникло</b>\n";
  msg += "⏰ Час: <code>" + eventTime + "</code> | Було: <code>" + lightDuration + "</code>";
  return msg;
}

// Генерація шаблону повідомлення про відновлення (Мінімалістичний)
String buildPowerOnMessage(String eventTime, String duration, bool wasOffline) {
  String msg = "⚡✅ <b>Світло з'явилося</b>\n";
  msg += "⏰ Час: <code>" + eventTime + "</code> | Не було: <code>" + duration + "</code>";
  if (wasOffline) {
    msg += "\n⚠️ <i>(Після відновлення зв'язку)</i>";
  }
  return msg;
}

// Виправлена функція відправки у Telegram із підтримкою веток (Topics) та ArduinoJson
bool sendTelegramMessage(String targetChatId, String targetTopicId, String text, String parseMode = "HTML") {
  targetChatId.trim();
  targetTopicId.trim();

  if (targetChatId == "" || targetChatId == "0") return false;

  bool result = false;
  int topicNum = targetTopicId.toInt();

  delay(300); // Запобігання бана/помилки 429 з боку Telegram

  if (topicNum > 0) {
    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["chat_id"] = targetChatId;
    payload["message_thread_id"] = topicNum;
    payload["text"] = text;
    payload["parse_mode"] = parseMode;

    result = bot.sendPostMessage(payload);
  } else {
    result = bot.sendMessage(targetChatId, text, parseMode);
  }

  if (!result) {
    Serial.print("🔴 Помилка відправки в ID ");
    Serial.print(targetChatId);
    if (topicNum > 0) Serial.print(" (Topic: " + String(topicNum) + ")");
    Serial.println(". Перевірте налаштування!");
  } else {
    Serial.println("🟢 Повідомлення успішно відправлено в ID: " + targetChatId + (topicNum > 0 ? " [Topic: " + String(topicNum) + "]" : ""));
  }
  return result;
}

// Розсилка повідомлення у всі налаштовані чати
void broadcastMessage(String text, String parseMode = "HTML") {
  if (chatId.length() > 2) {
    sendTelegramMessage(chatId, "", text, parseMode);
  }
  if (groupId1.length() > 2) {
    sendTelegramMessage(groupId1, topicId1, text, parseMode);
  }
  if (groupId2.length() > 2) {
    sendTelegramMessage(groupId2, topicId2, text, parseMode);
  }
}

void applyTimezone(String tz) {
  setenv("TZ", tz.c_str(), 1);
  tzset();
  currentTZ = tz;
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleSave() {
  if (server.hasArg("ssid1") && server.hasArg("pass1") && server.hasArg("token") && server.hasArg("owner")) {
    preferences.putString("ssid1", server.arg("ssid1"));
    preferences.putString("pass1", server.arg("pass1"));
    preferences.putString("ssid2", server.arg("ssid2"));
    preferences.putString("pass2", server.arg("pass2"));
    preferences.putInt("wftime", server.arg("wftime").toInt());
    preferences.putString("token", server.arg("token"));
    preferences.putString("chatId", server.arg("owner"));
    preferences.putString("groupId1", server.arg("g1"));
    preferences.putString("topicId1", server.arg("t1"));
    preferences.putString("groupId2", server.arg("g2"));
    preferences.putString("topicId2", server.arg("t2"));

    preferences.putBool("isConfigured", true);

    String response = "<html><head><meta charset='utf-8'></head><body style='text-align:center; font-family:sans-serif; padding-top:50px;'>";
    response += "<h1 style='color:#16a34a;'>Налаштування збережено! ✅</h1>";
    response += "<p>Пристрій перезавантажується для застосування змін...</p></body></html>";
    server.send(200, "text/html", response);

    delay(2000);
    ESP.restart();
  }
  server.send(400, "text/plain", "Помилка: Відсутні необхідні дані");
}

void launchWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Light-Watcher-Setup");

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound(handleRoot);
  server.begin();

  Serial.println("Запуск режиму налаштування через Web AP!");
  Serial.println("Підключіться до Wi-Fi 'Light-Watcher-Setup'. IP: " + WiFi.softAPIP().toString());

  while (true) {
    esp_task_wdt_reset();
    dnsServer.processNextRequest();
    server.handleClient();
    delay(5);
  }
}

// Спроба підключення до конкретної мережі з заданим таймаутом
bool connectToWiFiNetwork(String targetSsid, String targetPass, int timeoutSec) {
  if (targetSsid == "") return false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid.c_str(), targetPass.c_str());
  Serial.print("Спроба підключення до Wi-Fi: " + targetSsid + " ");
  
  int attempts = 0;
  int maxAttempts = timeoutSec * 2; // Кожна спроба = 500мс
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    esp_task_wdt_reset();
    attempts++;
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Успішно підключено до " + targetSsid + "! IP: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("Неможливо підключитися до " + targetSsid);
  return false;
}

// Логіка перевірки Wi-Fi з використанням основної та резервної мережі
void connectWiFiWithFallback() {
  // Спроба 1: Основний Wi-Fi
  if (connectToWiFiNetwork(ssid1, password1, wifiTimeout)) return;

  // Спроба 2: Резервний Wi-Fi (якщо заданий)
  if (ssid2.length() > 0) {
    Serial.println("Перехід на резервну мережу Wi-Fi...");
    if (connectToWiFiNetwork(ssid2, password2, wifiTimeout)) return;
  }

  // Якщо жодна мережа не підключилася -> Точка доступу
  Serial.println("Не вдалося підключитися до жодної Wi-Fi мережі! Запуск Web AP...");
  launchWebServer();
}

void checkWiFi() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi втрачено. Спроба перепід'єднання...");
      if (WiFi.status() != WL_CONNECTED && ssid1 != "") {
        WiFi.begin(ssid1.c_str(), password1.c_str());
      }
      if (WiFi.status() != WL_CONNECTED && ssid2 != "") {
        WiFi.begin(ssid2.c_str(), password2.c_str());
      }
    }

    if (missMessage && WiFi.status() == WL_CONNECTED) {
      broadcastMessage(lastMissMessage, "HTML");
      missMessage = false;
      Serial.println("Пропущене повідомлення про появу світла успішно надіслано!");
    }
  }
}

// Налаштування функцій ArduinoOTA
void setupOTA() {
  ArduinoOTA.setHostname("Light-Watcher-ESP32");
  
  // Можна встановити пароль для захисту від випадкового завантаження чужої прошивки:
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Початок оновлення OTA: " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nОновлення OTA успішно завершено!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    esp_task_wdt_reset(); // Скидання WDT під час прошивки
    Serial.printf("Прогрес OTA: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Помилка OTA [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Помилка авторизації");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Помилка початку");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Помилка з'єднання");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Помилка прийому даних");
    else if (error == OTA_END_ERROR) Serial.println("Помилка завершення");
  });

  ArduinoOTA.begin();
  Serial.println("Служба ArduinoOTA запущена!");
}

void handleNewMessages() {
  String chat_id = String(bot.messages[0].chat_id);
  String text = bot.messages[0].text;
  String from_name = bot.messages[0].from_name;

  chat_id.trim();
  text.trim();

  Serial.println("Отримано: " + text + " від " + from_name + " ID: " + chat_id);

  if (text == "Світло є чи нема?") {
    time_t now;
    time(&now);
    if (checkPowerStatus()) {
      String duration = formatDuration(now - powerOnTimestamp);
      String msg = "🟢 <b>ЕЛЕКТРОМЕРЕЖА АКТИВНА</b>\n";
      msg += "━━━━━━━━━━━━━━━━━━━\n";
      msg += "⏱ <b>Світло є вже:</b> " + duration + "\n";
      msg += "🕒 <b>Поточний час:</b> " + getFormattedShortTime();
      sendTelegramMessage(chat_id, "", msg, "HTML");
    } else {
      String duration = formatDuration(now - powerOffTimestamp);
      String msg = "🔴 <b>ЕЛЕКТРОМЕРЕЖА ЗНЕСТРУМЛЕНА</b>\n";
      msg += "━━━━━━━━━━━━━━━━━━━\n";
      msg += "⏱ <b>Світла немає:</b> " + duration + "\n";
      msg += "⏰ <b>Вимкнено о:</b> " + powerOffFormattedTime;
      sendTelegramMessage(chat_id, "", msg, "HTML");
    }
    return;
  }

  // Перевірка ID власника
  if (chat_id == chatId || chatId == "") {
    if (text == "/help" || text == "/start") {
      String help = "✨ <b>Панель керування Light Watcher v2.2</b>\n\n";
      help += "<b>Основні команди:</b>\n";
      help += "🔹 /status - Стан живлення, мережі та часу\n";
      help += "🔹 /info - Інформація про систему\n\n";
      help += "<b>Тестування сповіщень (Розсилка у всі чати):</b>\n";
      help += "🧪 /test_off - Тест сповіщення «Світло вимкнули»\n";
      help += "🧪 /test_on - Тест сповіщення «Світло з'явилося»\n";
      help += "🧪 /test_groups - Перевірка відправки в групи\n\n";
      help += "<b>Налаштування часу:</b>\n";
      help += "🌐 /set_time_auto - Авто-синхронізація (NTP мережа)\n";
      help += "❄️ /set_winter_time - Фіксований зимовий час (UTC+2)\n";
      help += "☀️ /set_summer_time - Фіксований літній час (UTC+3)\n\n";
      help += "<b>Системні команди:</b>\n";
      help += "🧹 /clear_data - Скинути налаштування\n";
      help += "🔄 /restart - Перезавантажити пристрій";
      sendTelegramMessage(chat_id, "", help, "HTML");
    }
    else if (text == "/info") {
      String info = "⚡ <b>Light Watcher v2.2</b>\n\n";
      info += "<b>Розумний модуль моніторингу електромережі</b>\n\n";
      info += "• Підтримка 2-х Wi-Fi мереж (Основна + Резервна)\n";
      info += "• Авто-перехід у точку доступу за таймаутом\n";
      info += "• Розумний облік часу наявності/відсутності світла\n";
      info += "• Підтримка 2-х Telegram груп та веток (Topics)\n";
      info += "• Захист від хибних спрацьовувань (Smart Debounce)\n";
      info += "• Гнучке керування часом (Авто NTP / Зима / Літо)\n";
      info += "• Підтримка оновлення по повітрю (ArduinoOTA)";
      sendTelegramMessage(chat_id, "", info, "HTML");
    }
    else if (text == "/test_off") {
      String testMsg = "🧪⚡❌ <b>Тест: Світло зникло</b>\n";
      testMsg += "⏰ Час: <code>" + getFormattedShortTime() + "</code> | Було: <code>3 год. 45 хв.</code>";
      broadcastMessage(testMsg, "HTML");
    }
    else if (text == "/test_on") {
      String testMsg = "🧪⚡✅ <b>Тест: Світло з'явилося</b>\n";
      testMsg += "⏰ Час: <code>" + getFormattedShortTime() + "</code> | Не було: <code>2 год. 10 хв.</code>";
      broadcastMessage(testMsg, "HTML");
    }
    else if (text == "/test_groups") {
      String report = "🧪 <b>Результат тестування відправки в групи:</b>\n\n";

      if (groupId1.length() > 2) {
        bool res1 = sendTelegramMessage(groupId1, topicId1, "🧪 <b>Тест зв'язку (Група 1)</b>\nСистема працює справно!");
        report += "📍 <b>Група 1</b> (ID: <code>" + groupId1 + "</code>" + (topicId1 != "0" && topicId1 != "" ? ", Topic: " + topicId1 : "") + "): " + (res1 ? "🟢 УСПІШНО" : "🔴 ПОМИЛКА") + "\n";
      } else {
        report += "📍 <b>Група 1</b>: Не налаштована ⚪\n";
      }

      if (groupId2.length() > 2) {
        bool res2 = sendTelegramMessage(groupId2, topicId2, "🧪 <b>Тест зв'язку (Група 2)</b>\nСистема працює справно!");
        report += "📍 <b>Група 2</b> (ID: <code>" + groupId2 + "</code>" + (topicId2 != "0" && topicId2 != "" ? ", Topic: " + topicId2 : "") + "): " + (res2 ? "🟢 УСПІШНО" : "🔴 ПОМИЛКА") + "\n";
      } else {
        report += "📍 <b>Група 2</b>: Не налаштована ⚪\n";
      }

      sendTelegramMessage(chat_id, "", report, "HTML");
    }
    else if (text == "/status") {
      int rssi = WiFi.RSSI();
      time_t now;
      time(&now);

      String st = "📊 <b>Поточний стан системи:</b>\n\n";
      st += "⚡ <b>Електромережа:</b> " + String(checkPowerStatus() ? "ПРИСУТНЯ 🟢" : "ВІДСУТНЯ 🔴") + "\n";
      if (checkPowerStatus()) {
        st += "⏱ <b>Світло є вже:</b> " + formatDuration(now - powerOnTimestamp) + "\n";
      } else {
        st += "⏱ <b>Світла немає:</b> " + formatDuration(now - powerOffTimestamp) + "\n";
      }
      st += "📶 <b>Wi-Fi мережа:</b> " + WiFi.SSID() + " (" + String(rssi) + " dBm)\n";
      st += "🌐 <b>IP пристрою:</b> " + WiFi.localIP().toString() + "\n";
      st += "🕒 <b>Системний час:</b> " + getFormattedTime() + "\n";
      st += "🌍 <b>Режим часу:</b> " + currentTZ + "\n";
      st += "⚙️ <b>Аптайм модуля:</b> " + formatDuration(millis() / 1000);
      sendTelegramMessage(chat_id, "", st, "HTML");
    }
    else if (text == "/set_time_auto") {
      currentTZ = TZ_UKRAINE_AUTO;
      preferences.putString("tz-rule", currentTZ);
      applyTimezone(currentTZ);
      sendTelegramMessage(chat_id, "", "🌐 <b>Встановлено авто-синхронізацію часу з мережі (NTP UTC+2/UTC+3)</b>", "HTML");
    }
    else if (text == "/set_summer_time") {
      currentTZ = "EEST-3";
      preferences.putString("tz-rule", currentTZ);
      applyTimezone(currentTZ);
      sendTelegramMessage(chat_id, "", "☀️ <b>Встановлено фіксований літній час (UTC+3)</b>", "HTML");
    }
    else if (text == "/set_winter_time") {
      currentTZ = "EET-2";
      preferences.putString("tz-rule", currentTZ);
      applyTimezone(currentTZ);
      sendTelegramMessage(chat_id, "", "❄️ <b>Встановлено фіксований зимовий час (UTC+2)</b>", "HTML");
    }
    else if (text == "/clear_data") {
      preferences.clear();
      sendTelegramMessage(chat_id, "", "🧹 Налаштування очищено. Перезавантаження у режим Web AP...");
      delay(1000);
      ESP.restart();
    }
    else if (text == "/restart") {
      sendTelegramMessage(chat_id, "", "🔄 Перезавантаження пристрою...");
      delay(1000);
      ESP.restart();
    }
    else {
      sendTelegramMessage(chat_id, "", "❓ Невідома команда. Отримайте список: /help");
    }
  } else {
    Serial.println("Відхилено ID (" + chat_id + "), очікувався: " + chatId);
  }
}

void setup() {
  pinMode(readPin, INPUT);
  analogSetAttenuation(ADC_11db);
  pinMode(9, INPUT_PULLUP);

  Serial.begin(115200);
  delay(1000);
  Serial.println("Start Light Watcher V2.2");

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000, // Перевод секунд в миллисекунды
    .idle_core_mask = (1 << configNUM_CORES) - 1, // Мониторинг всех ядер
    .trigger_panic = true
};
esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  preferences.begin("light-watcher", false);

  bool isConfigured = preferences.getBool("isConfigured", false);

  if (!isConfigured || digitalRead(9) == LOW) {
    launchWebServer();
  }

  ssid1 = preferences.getString("ssid1", "");
  password1 = preferences.getString("pass1", "");
  ssid2 = preferences.getString("ssid2", "");
  password2 = preferences.getString("pass2", "");
  wifiTimeout = preferences.getInt("wftime", 30);
  botToken = preferences.getString("token", "");
  chatId = preferences.getString("chatId", "");
  groupId1 = preferences.getString("groupId1", "");
  topicId1 = preferences.getString("topicId1", "0");
  groupId2 = preferences.getString("groupId2", "");
  topicId2 = preferences.getString("topicId2", "0");

  ssid1.trim();
  password1.trim();
  ssid2.trim();
  password2.trim();
  botToken.trim();
  chatId.trim();

  if (ssid1 == "" || password1 == "" || botToken == "" || chatId == "") {
    launchWebServer();
  }

  bot.updateToken(botToken);

  powerOutageCount = preferences.getInt("powerOutageCnt", 0);
  currentTZ = preferences.getString("tz-rule", TZ_UKRAINE_AUTO);
  powerOffTimestamp = preferences.getLong64("pwrOffTmstmp", 0);
  powerOnTimestamp = preferences.getLong64("pwrOnTmstmp", time(NULL));
  lastOutageDetect = preferences.getBool("lastUotDetect", false);

  // Підключення до Wi-Fi з логікою резервування
  connectWiFiWithFallback();

  // Запуск ініціалізації ArduinoOTA після підключення до Wi-Fi
  setupOTA();

  configTime(0, 0, ntp1, ntp2, ntp3);
  applyTimezone(currentTZ);

  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    esp_task_wdt_reset();
    delay(500);
    attempts++;
  }

  client.setInsecure();
  client.setTimeout(5000); 

  powerStatus = checkPowerStatus();

  if (lastOutageDetect && powerStatus) {
    messageFlag = false;
    time(&powerOnTimestamp);
    preferences.putLong64("pwrOnTmstmp", (int64_t)powerOnTimestamp);
    unsigned long outageSeconds = powerOnTimestamp - powerOffTimestamp;

    String message = buildPowerOnMessage(getFormattedShortTime(), formatDuration(outageSeconds), true);

    lastOutageDetect = false;
    preferences.putBool("lastUotDetect", false);

    broadcastMessage(message, "HTML");
  } else {
    String startMessage = "🚀 <b>МОДУЛЬ LIGHT WATCHER V2.2 УСПІШНО ЗАПУЩЕНО</b>\n";
    startMessage += "━━━━━━━━━━━━━━━━━━━\n";
    startMessage += "📅 <b>Дата та час:</b> " + getFormattedTime() + "\n";
    startMessage += "🌐 <b>Підключено до:</b> " + WiFi.SSID() + "\n";
    startMessage += "⚡ <b>Стан мережі:</b> " + String(powerStatus ? "ПРИСУТНЯ 🟢" : "ВІДСУТНЯ 🔴") + "\n";
    startMessage += "━━━━━━━━━━━━━━━━━━━\n";
    if (!powerStatus) {
      startMessage += "⚠️ <i>Пристрій запущено від резервного живлення!</i>";
      powerOffTime = millis();
      messageFlag = true;
    } else {
      startMessage += "✅ <i>Система готова до моніторингу.</i>";
      if (powerOnTimestamp == 0) {
        time(&powerOnTimestamp);
        preferences.putLong64("pwrOnTmstmp", (int64_t)powerOnTimestamp);
      }
    }

    if (!sendTelegramMessage(chatId, "", startMessage, "HTML")) {
      Serial.println("Помилка відправки стартового повідомлення. Перевірте токен або ID.");
    }
  }

  int newMessage = bot.getUpdates(-1);
  if (newMessage > 0) {
    bot.last_message_received = bot.messages[0].update_id;
  }
}

void loop() {
  esp_task_wdt_reset();

  // Обробка запитів оновлення по повітрю
  ArduinoOTA.handle();

  checkWiFi();

  bool currentPowerStatus = checkPowerStatus();

  if (!currentPowerStatus && !messageFlag) {
    messageFlag = true;
    powerOffTime = millis();

    time(&powerOffTimestamp);
    preferences.putLong64("pwrOffTmstmp", (int64_t)powerOffTimestamp);
    lastOutageDetect = true;
    preferences.putBool("lastUotDetect", true);

    powerOffFormattedTime = getFormattedShortTime();
    
    // Обчислення часу тривалості присутності світла
    time_t now;
    time(&now);
    time_t lightOnDurationSeconds = (powerOnTimestamp > 0 && now > powerOnTimestamp) ? (now - powerOnTimestamp) : 0;
    String lightDurationStr = formatDuration(lightOnDurationSeconds);

    String message = buildPowerOffMessage(powerOffFormattedTime, lightDurationStr);

    broadcastMessage(message, "HTML");
    Serial.println("Повідомлення про відключення надіслано");
  }
  else if (currentPowerStatus && messageFlag) {
    messageFlag = false;
    powerOnTime = millis();
    time(&powerOnTimestamp);
    preferences.putLong64("pwrOnTmstmp", (int64_t)powerOnTimestamp);

    lastOutageDetect = false;
    preferences.putBool("lastUotDetect", false);

    unsigned long outageSeconds = (powerOnTime - powerOffTime) / 1000;
    if (powerOffTimestamp > 0 && powerOnTimestamp > powerOffTimestamp) {
      outageSeconds = powerOnTimestamp - powerOffTimestamp;
    }

    powerOnFormattedTime = getFormattedShortTime();

    bool wasOffline = (WiFi.status() != WL_CONNECTED);
    String message = buildPowerOnMessage(powerOnFormattedTime, formatDuration(outageSeconds), wasOffline);

    if (wasOffline) {
      missMessage = true;
      lastMissMessage = message;
      Serial.println("Повідомлення про відновлення збережено в архів");
    } else {
      broadcastMessage(message, "HTML");
      Serial.println("Повідомлення про відновлення надіслано");
    }
  }

  static unsigned long lastBotCheck = 0;
  if (millis() - lastBotCheck > 1500) {
    if (WiFi.status() == WL_CONNECTED) {
      int newMessage = bot.getUpdates(bot.last_message_received + 1);
      while (newMessage) {
        esp_task_wdt_reset();
        handleNewMessages();
        newMessage = bot.getUpdates(bot.last_message_received + 1);
      }
    }
    lastBotCheck = millis();
  }
}