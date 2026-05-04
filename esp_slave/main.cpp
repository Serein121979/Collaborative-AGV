#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace {

constexpr char kSsid[] = "AGV_CTRL";
constexpr char kPassword[] = "12345678";
constexpr long kUartBaud = 9600;
constexpr uint16_t kTcpPort = 8080;
constexpr int kDefaultSpeed = 180;
constexpr unsigned long kWifiRetryMs = 8000;
constexpr unsigned long kTcpRetryMs = 1500;
constexpr unsigned long kWifiBlinkMs = 150;
constexpr unsigned long kTcpBlinkMs = 650;

const IPAddress kMasterIp(192, 168, 4, 1);

struct MotionCommand {
  String seq;
  String action;
  int speed;
};

WiFiClient masterClient;
String inputBuffer;
bool registeredWithMaster = false;
bool wasLinkHealthy = false;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastTcpAttemptMs = 0;
unsigned long lastStatusBlinkMs = 0;
bool statusLedOn = false;

void setStatusLed(bool on) {
  statusLedOn = on;
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

void updateStatusLed() {
  const unsigned long now = millis();
  const unsigned long blinkMs =
      (WiFi.status() == WL_CONNECTED) ? kTcpBlinkMs : kWifiBlinkMs;

  if (registeredWithMaster) {
    setStatusLed(true);
    return;
  }

  if (now - lastStatusBlinkMs >= blinkMs) {
    lastStatusBlinkMs = now;
    setStatusLed(!statusLedOn);
  }
}

bool isValidAction(const String& action) {
  return action == "STOP" || action == "FWD" || action == "BACK" ||
         action == "LEFT" || action == "RIGHT" || action == "SPINL" ||
         action == "SPINR";
}

bool parseSpeed(const String& input, int& speed) {
  unsigned int i;

  if (input.length() == 0) {
    speed = kDefaultSpeed;
    return true;
  }

  for (i = 0; i < input.length(); ++i) {
    if (!isDigit(input[i])) {
      return false;
    }
  }

  speed = input.toInt();
  if (speed < 0) {
    speed = 0;
  } else if (speed > 255) {
    speed = 255;
  }
  return true;
}

bool parseMotionCommand(const String& line, MotionCommand& command) {
  const int first = line.indexOf(',', 4);
  const int second = line.indexOf(',', first + 1);
  String actionToken;
  String speedToken;
  int speed = 0;

  if (!line.startsWith("CMD,")) {
    return false;
  }

  if (first < 0) {
    return false;
  }

  command.seq = line.substring(4, first);
  if (command.seq.length() == 0) {
    return false;
  }

  if (second < 0) {
    actionToken = line.substring(first + 1);
    speedToken = "";
  } else {
    actionToken = line.substring(first + 1, second);
    speedToken = line.substring(second + 1);
  }

  actionToken.trim();
  actionToken.toUpperCase();
  if (!isValidAction(actionToken)) {
    return false;
  }

  if (!parseSpeed(speedToken, speed)) {
    return false;
  }

  if (actionToken == "STOP") {
    speed = 0;
  }

  command.action = actionToken;
  command.speed = speed;
  return true;
}

void sendLocalMotion(const MotionCommand& command) {
  if (command.action == "STOP") {
    Serial.print("S\n");
    return;
  }

  if (command.action == "FWD") {
    Serial.print("F\n");
  } else if (command.action == "BACK") {
    Serial.print("B\n");
  } else if (command.action == "LEFT") {
    Serial.print("L\n");
  } else if (command.action == "RIGHT") {
    Serial.print("R\n");
  } else if (command.action == "SPINL") {
    Serial.print("L\n");
  } else if (command.action == "SPINR") {
    Serial.print("R\n");
  } else {
    Serial.print("S\n");
  }
}

void issueLocalStop() {
  MotionCommand stopCommand;

  stopCommand.seq = "0";
  stopCommand.action = "STOP";
  stopCommand.speed = 0;
  sendLocalMotion(stopCommand);
}

void ensureWifi() {
  const unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (wasLinkHealthy) {
    issueLocalStop();
    wasLinkHealthy = false;
  }

  if (now - lastWifiAttemptMs < kWifiRetryMs) {
    return;
  }

  lastWifiAttemptMs = now;
  if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
    WiFi.disconnect();
  }
  WiFi.begin(kSsid, kPassword);
}

void ensureTcpConnection() {
  const unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (masterClient.connected()) {
      masterClient.stop();
    }
    registeredWithMaster = false;
    return;
  }

  if (masterClient.connected()) {
    return;
  }

  if (now - lastTcpAttemptMs < kTcpRetryMs) {
    return;
  }

  lastTcpAttemptMs = now;
  issueLocalStop();
  masterClient.stop();

  if (masterClient.connect(kMasterIp, kTcpPort)) {
    masterClient.setNoDelay(true);
    masterClient.print("HELLO,CAR2\n");
    registeredWithMaster = false;
    inputBuffer = "";
  }
}

void processLine(const String& rawLine) {
  String line = rawLine;
  MotionCommand command;

  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (!registeredWithMaster) {
    if (line == "OK,CAR2") {
      registeredWithMaster = true;
      wasLinkHealthy = true;
    }
    return;
  }

  if (!parseMotionCommand(line, command)) {
    return;
  }

  sendLocalMotion(command);
  if (masterClient.connected()) {
    masterClient.print("DONE,");
    masterClient.print(command.seq);
    masterClient.print("\n");
  }
}

void pollMasterMessages() {
  if (!masterClient.connected()) {
    if (wasLinkHealthy) {
      issueLocalStop();
      wasLinkHealthy = false;
    }
    registeredWithMaster = false;
    return;
  }

  while (masterClient.available()) {
    const char ch = static_cast<char>(masterClient.read());

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      processLine(inputBuffer);
      inputBuffer = "";
      continue;
    }

    if (inputBuffer.length() < 64) {
      inputBuffer += ch;
    }
  }
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setStatusLed(false);
  Serial.begin(kUartBaud);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(kSsid, kPassword);
  lastWifiAttemptMs = millis();
  issueLocalStop();
}

void loop() {
  ensureWifi();
  ensureTcpConnection();
  pollMasterMessages();
  updateStatusLed();
}
