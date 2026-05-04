#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace {

// 1 号车 ESP8266 工作在 AP 模式，手机和 2 号车都连这个热点。
constexpr char kApSsid[] = "AGV_CTRL";
constexpr char kApPassword[] = "12345678";
constexpr uint16_t kTcpPort = 8080;
constexpr uint8_t kApChannel = 1;

// ESP 和 51 单片机之间用 9600 串口通信。
constexpr long kUartBaud = 9600;

// 手机命令不带速度时使用这个默认速度；当前 51 侧只接单字符，速度暂未下发。
constexpr int kDefaultSpeed = 180;

// 主站把命令转发给 2 号车后，最多等 800ms 收 DONE 回执。
constexpr unsigned long kCar2AckTimeoutMs = 800;

// 前进/后退属于持续动作，主站会周期性重发给本车 51，减少串口丢字节导致的停顿。
constexpr unsigned long kLocalRepeatIntervalMs = 120;

// 最多同时保留 4 个 TCP 连接槽位，实际有效角色主要是 PHONE 和 CAR2。
constexpr uint8_t kMaxClients = 4;

// AP 固定 IP，手机 TCP Client 连接 192.168.4.1:8080。
const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApSubnet(255, 255, 255, 0);

// 每个 TCP 连接注册后的身份。
enum ClientRole : uint8_t {
  ROLE_UNKNOWN = 0,
  ROLE_PHONE,
  ROLE_CAR2
};

// 手机发来的高层运动命令，例如 CMD,12,FWD,180。
struct MotionCommand {
  String seq;
  String action;
  int speed;
};

// 等待 2 号车 DONE 回执的状态。
struct PendingAck {
  bool active = false;
  String seq;
  unsigned long startedAt = 0;
};

// 一个 TCP 客户端槽位：连接对象、身份、以及尚未凑成一整行的输入缓存。
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

// 按角色查找已经注册的连接槽位。
int findSlotByRole(ClientRole role) {
  int i;

  for (i = 0; i < kMaxClients; ++i) {
    if (clientSlots[i].client && clientSlots[i].role == role) {
      return i;
    }
  }
  return -1;
}

// TCP 协议按行发送，每条消息用 \n 结尾。
void sendLine(WiFiClient& client, const String& line) {
  if (client && client.connected()) {
    client.print(line);
    client.print('\n');
  }
}

// 给某个角色发送一行消息，例如给手机发 ACK 或给 2 号车发 CMD。
void sendToRole(ClientRole role, const String& line) {
  const int slot = findSlotByRole(role);

  if (slot >= 0) {
    sendLine(clientSlots[slot].client, line);
  }
}

// 把高层动作翻译成发给本车 51 单片机的单字符串口命令。
void sendLocalMotion(const MotionCommand& command) {
  lastLocalMotionSentAt = millis();

  if (command.action == "STOP") {
    // S = Stop，51 收到后停车。
    Serial.print("S\n");
    return;
  }

  if (command.action == "FWD") {
    // F = Forward。
    Serial.print("F\n");
  } else if (command.action == "BACK") {
    // B = Back。
    Serial.print("B\n");
  } else if (command.action == "LEFT") {
    // L = Left。当前 51 侧会左转一段时间后继续前进。
    Serial.print("L\n");
  } else if (command.action == "RIGHT") {
    // R = Right。当前 51 侧会右转一段时间后继续前进。
    Serial.print("R\n");
  } else if (command.action == "SPINL") {
    // 暂时复用 L；如果 51 后续实现原地旋转，这里可以改成新命令。
    Serial.print("L\n");
  } else if (command.action == "SPINR") {
    // 暂时复用 R。
    Serial.print("R\n");
  } else {
    // 未知动作按停车处理，保证安全。
    Serial.print("S\n");
  }
}

// 紧急停车：本车 51 停车，同时通知 2 号车停车，并清掉等待中的回执。
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

// 手机协议允许的动作名。
bool isValidAction(const String& action) {
  return action == "STOP" || action == "FWD" || action == "BACK" ||
         action == "LEFT" || action == "RIGHT" || action == "SPINL" ||
         action == "SPINR";
}

// 解析速度字段：空字段用默认速度，非数字判定为非法，超过 0~255 时截断。
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

// 解析手机/主站之间的 CMD 行，成功后填入 command。
bool parseMotionCommand(const String& line, MotionCommand& command) {
  const int first = line.indexOf(',', 4);
  const int second = line.indexOf(',', first + 1);
  String actionToken;
  String speedToken;
  int speed = 0;

  if (!line.startsWith("CMD,")) {
    // 必须以 CMD, 开头。
    return false;
  }

  if (first < 0) {
    // 没有序号和动作之间的逗号，格式不完整。
    return false;
  }

  command.seq = line.substring(4, first);
  if (command.seq.length() == 0) {
    // 序号不能为空，否则手机端难以对应 ACK/STATE。
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
    // 动作名不在白名单里，直接拒绝。
    return false;
  }

  if (!parseSpeed(speedToken, speed)) {
    // 速度不是纯数字。
    return false;
  }

  if (actionToken == "STOP") {
    // 停止命令速度固定为 0。
    speed = 0;
  }

  command.action = actionToken;
  command.speed = speed;
  return true;
}

// 处理 2 号车返回的 DONE,<SEQ>，并把完成状态通知手机。
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

// 把手机命令镜像转发给 2 号车，并启动回执等待计时。
bool mirrorToCar2(const MotionCommand& command) {
  const int car2Slot = findSlotByRole(ROLE_CAR2);
  String outgoing;

  if (car2Slot < 0) {
    // 2 号车不在线，调用者会给手机 WARN,CAR2_OFFLINE。
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

// 处理手机发来的控制命令：先控制本车，再通知手机 ACK，最后转发给 2 号车。
void handlePhoneCommand(const String& line) {
  MotionCommand command;
  const bool validCommand = parseMotionCommand(line, command) ? true : false;

  if (!validCommand) {
    sendToRole(ROLE_PHONE, "STATE,INVALID_CMD");
    return;
  }

  sendLocalMotion(command);
  lastLocalCommand = command;
  // 只有前进/后退需要周期性重发；转弯命令在 51 侧自己完成延时动作。
  localMotionActive = command.action == "FWD" || command.action == "BACK";
  sendToRole(ROLE_PHONE, "ACK," + command.seq);
  if (!mirrorToCar2(command)) {
    sendToRole(ROLE_PHONE, "WARN,CAR2_OFFLINE");
  }
}

// 当前进/后退持续执行时，周期性重发最后一条本车运动命令。
void repeatLocalMotion() {
  if (!localMotionActive) {
    return;
  }

  if (millis() - lastLocalMotionSentAt >= kLocalRepeatIntervalMs) {
    sendLocalMotion(lastLocalCommand);
  }
}

// 关闭指定客户端槽位。手机掉线时可触发急停，2 号车掉线时通知手机。
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
    // 手机断开连接时认为控制端失联，立即让两台车停车。
    stopAllCars();
  } else if (oldRole == ROLE_CAR2 && pendingAck.active) {
    // 正在等 2 号车回执时断线，向手机报告中途断开。
    sendToRole(ROLE_PHONE, "STATE," + pendingAck.seq + ",CAR2_DISCONNECTED");
    pendingAck.active = false;
  }
}

// 完成 HELLO 注册。相同角色重复上线时，踢掉旧连接，保留新连接。
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

// 处理一整行 TCP 消息。未注册连接只能先发 HELLO。
void processLine(int slotIndex, const String& rawLine) {
  String line = rawLine;

  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (clientSlots[slotIndex].role == ROLE_UNKNOWN) {
    if (line == "HELLO,PHONE") {
      // 手机控制端注册。
      registerRole(slotIndex, ROLE_PHONE);
    } else if (line == "HELLO,CAR2") {
      // 2 号车 ESP 从站注册。
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

// 接收新的 TCP 连接，并放入第一个空槽位。
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
      // 槽位满了就拒绝新连接。
      incoming.stop();
      continue;
    }

    clientSlots[targetSlot].client = incoming;
    clientSlots[targetSlot].client.setNoDelay(true);
    clientSlots[targetSlot].role = ROLE_UNKNOWN;
    clientSlots[targetSlot].buffer = "";
  }
}

// 轮询所有客户端数据，把 TCP 字节流按 \n 切成一行一行的协议消息。
void pollClientData() {
  int i;

  for (i = 0; i < kMaxClients; ++i) {
    if (!clientSlots[i].client) {
      continue;
    }

    if (!clientSlots[i].client.connected()) {
      // 断线时按角色执行清理逻辑。
      closeSlot(i, true);
      continue;
    }

    while (clientSlots[i].client.available()) {
      char ch = static_cast<char>(clientSlots[i].client.read());

      if (ch == '\r') {
        // 兼容 Windows/部分调试工具发送的 \r\n。
        continue;
      }

      if (ch == '\n') {
        // 收到换行符表示一条协议消息结束。
        processLine(i, clientSlots[i].buffer);
        clientSlots[i].buffer = "";
        continue;
      }

      if (clientSlots[i].buffer.length() < 64) {
        // 限制单行长度，避免异常客户端一直发数据撑爆内存。
        clientSlots[i].buffer += ch;
      }
    }
  }
}

// 检查 2 号车是否在超时时间内返回 DONE。
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
  // 初始化与 51 单片机通信的串口。
  Serial.begin(kUartBaud);

  // 不把 WiFi 配置写入 Flash，减少反复烧写/运行造成的 Flash 磨损。
  WiFi.persistent(false);

  // 设置为热点模式，手机和 2 号车都作为客户端连接进来。
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kApIp, kApGateway, kApSubnet);
  WiFi.softAP(kApSsid, kApPassword, kApChannel, false, 4);

  // 启动 TCP 服务端。
  server.begin();
  server.setNoDelay(true);

  // 上电先让两台车处于停止状态。
  stopAllCars();
}

void loop() {
  // 主循环必须短而快：接连接、收消息、查超时、必要时重发本车运动命令。
  acceptNewClients();
  pollClientData();
  checkPendingAck();
  repeatLocalMotion();
}
