#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace {

constexpr char kApSsid[] = "AGV_CTRL";
constexpr char kApPassword[] = "12345678";
constexpr uint16_t kTcpPort = 8080;
constexpr uint8_t kApChannel = 1;
constexpr long kUartBaud = 9600;
constexpr int kDefaultSpeed = 180;
constexpr unsigned long kCar2AckTimeoutMs = 800;
constexpr unsigned long kLocalRepeatIntervalMs = 120;
constexpr uint8_t kMaxClients = 4;

const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApSubnet(255, 255, 255, 0);

enum ClientRole : uint8_t {
  ROLE_UNKNOWN = 0,
  ROLE_PHONE,
  ROLE_CAR2
};

struct MotionCommand {
  String seq;
  String action;
  int speed;
};

struct PendingAck {
  bool active = false;
  String seq;
  unsigned long startedAt = 0;
};

struct ClientSlot {
  WiFiClient client;
  ClientRole role = ROLE_UNKNOWN;
  String buffer;
};

WiFiServer server(kTcpPort);
ClientSlot clientSlots[kMaxClients];
PendingAck pendingAck;
MotionCommand lastLocalCommand;
bool localMotionActive = false;
unsigned long lastLocalMotionSentAt = 0;

int findSlotByRole(ClientRole role) {
  int i;

  for (i = 0; i < kMaxClients; ++i) {
    if (clientSlots[i].client && clientSlots[i].role == role) {
      return i;
    }
  }
  return -1;
}

void sendLine(WiFiClient& client, const String& line) {
  if (client && client.connected()) {
    client.print(line);
    client.print('\n');
  }
}

void sendToRole(ClientRole role, const String& line) {
  const int slot = findSlotByRole(role);

  if (slot >= 0) {
    sendLine(clientSlots[slot].client, line);
  }
}

void sendLocalMotion(const MotionCommand& command) {
  lastLocalMotionSentAt = millis();

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

void stopAllCars() {
  MotionCommand stopCommand;

  stopCommand.seq = "0";
  stopCommand.action = "STOP";
  stopCommand.speed = 0;

  sendLocalMotion(stopCommand);
  sendToRole(ROLE_CAR2, "CMD,0,STOP,0");
  localMotionActive = false;
  pendingAck.active = false;
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

void handleCar2Ack(const String& line) {
  const String seq = line.substring(5);

  if (!line.startsWith("DONE,")) {
    return;
  }

  if (pendingAck.active && seq == pendingAck.seq) {
    sendToRole(ROLE_PHONE, "STATE," + seq + ",CAR2_DONE");
    pendingAck.active = false;
  }
}

bool mirrorToCar2(const MotionCommand& command) {
  const int car2Slot = findSlotByRole(ROLE_CAR2);
  String outgoing;

  if (car2Slot < 0) {
    pendingAck.active = false;
    return false;
  }

  outgoing = "CMD," + command.seq + "," + command.action + "," + String(command.speed);
  sendLine(clientSlots[car2Slot].client, outgoing);
  pendingAck.active = true;
  pendingAck.seq = command.seq;
  pendingAck.startedAt = millis();
  return true;
}

void handlePhoneCommand(const String& line) {
  MotionCommand command;
  const bool car2Online = parseMotionCommand(line, command) ? true : false;

  if (!car2Online) {
    sendToRole(ROLE_PHONE, "STATE,INVALID_CMD");
    return;
  }

  sendLocalMotion(command);
  lastLocalCommand = command;
  localMotionActive = command.action == "FWD" || command.action == "BACK";
  sendToRole(ROLE_PHONE, "ACK," + command.seq);
  if (!mirrorToCar2(command)) {
    sendToRole(ROLE_PHONE, "WARN,CAR2_OFFLINE");
  }
}

void repeatLocalMotion() {
  if (!localMotionActive) {
    return;
  }

  if (millis() - lastLocalMotionSentAt >= kLocalRepeatIntervalMs) {
    sendLocalMotion(lastLocalCommand);
  }
}

void closeSlot(int slotIndex, bool emergencyStop) {
  ClientRole oldRole;

  if (slotIndex < 0 || slotIndex >= kMaxClients) {
    return;
  }

  oldRole = clientSlots[slotIndex].role;
  if (clientSlots[slotIndex].client) {
    clientSlots[slotIndex].client.stop();
  }
  clientSlots[slotIndex].buffer = "";
  clientSlots[slotIndex].role = ROLE_UNKNOWN;

  if (oldRole == ROLE_PHONE && emergencyStop) {
    stopAllCars();
  } else if (oldRole == ROLE_CAR2 && pendingAck.active) {
    sendToRole(ROLE_PHONE, "STATE," + pendingAck.seq + ",CAR2_DISCONNECTED");
    pendingAck.active = false;
  }
}

void registerRole(int slotIndex, ClientRole role) {
  const int existing = findSlotByRole(role);

  if (existing >= 0 && existing != slotIndex) {
    closeSlot(existing, role == ROLE_PHONE);
  }

  clientSlots[slotIndex].role = role;
  if (role == ROLE_PHONE) {
    sendLine(clientSlots[slotIndex].client, "OK,PHONE");
  } else if (role == ROLE_CAR2) {
    sendLine(clientSlots[slotIndex].client, "OK,CAR2");
  }
}

void processLine(int slotIndex, const String& rawLine) {
  String line = rawLine;

  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (clientSlots[slotIndex].role == ROLE_UNKNOWN) {
    if (line == "HELLO,PHONE") {
      registerRole(slotIndex, ROLE_PHONE);
    } else if (line == "HELLO,CAR2") {
      registerRole(slotIndex, ROLE_CAR2);
    }
    return;
  }

  if (clientSlots[slotIndex].role == ROLE_PHONE) {
    handlePhoneCommand(line);
    return;
  }

  if (clientSlots[slotIndex].role == ROLE_CAR2) {
    handleCar2Ack(line);
  }
}

void acceptNewClients() {
  while (server.hasClient()) {
    WiFiClient incoming = server.available();
    int targetSlot = -1;
    int i;

    for (i = 0; i < kMaxClients; ++i) {
      if (!clientSlots[i].client || !clientSlots[i].client.connected()) {
        targetSlot = i;
        break;
      }
    }

    if (targetSlot < 0) {
      incoming.stop();
      continue;
    }

    clientSlots[targetSlot].client = incoming;
    clientSlots[targetSlot].client.setNoDelay(true);
    clientSlots[targetSlot].role = ROLE_UNKNOWN;
    clientSlots[targetSlot].buffer = "";
  }
}

void pollClientData() {
  int i;

  for (i = 0; i < kMaxClients; ++i) {
    if (!clientSlots[i].client) {
      continue;
    }

    if (!clientSlots[i].client.connected()) {
      closeSlot(i, true);
      continue;
    }

    while (clientSlots[i].client.available()) {
      char ch = static_cast<char>(clientSlots[i].client.read());

      if (ch == '\r') {
        continue;
      }

      if (ch == '\n') {
        processLine(i, clientSlots[i].buffer);
        clientSlots[i].buffer = "";
        continue;
      }

      if (clientSlots[i].buffer.length() < 64) {
        clientSlots[i].buffer += ch;
      }
    }
  }
}

void checkPendingAck() {
  if (!pendingAck.active) {
    return;
  }

  if (millis() - pendingAck.startedAt >= kCar2AckTimeoutMs) {
    sendToRole(ROLE_PHONE, "STATE," + pendingAck.seq + ",CAR2_TIMEOUT");
    pendingAck.active = false;
  }
}

}  // namespace

void setup() {
  Serial.begin(kUartBaud);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kApIp, kApGateway, kApSubnet);
  WiFi.softAP(kApSsid, kApPassword, kApChannel, false, 4);

  server.begin();
  server.setNoDelay(true);

  stopAllCars();
}

void loop() {
  acceptNewClients();
  pollClientData();
  checkPendingAck();
  repeatLocalMotion();
}
