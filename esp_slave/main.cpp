#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace {

// 2 号车作为 STA 连接 1 号车创建的 AGV_CTRL 热点。
constexpr char kSsid[] = "AGV_CTRL";
constexpr char kPassword[] = "12345678";

// ESP 和 51 单片机之间的串口速度，必须和 51 的 UART_TIMER1_RELOAD 对上。
constexpr long kUartBaud = 9600;

// 主站 TCP 服务端端口。
constexpr uint16_t kTcpPort = 8080;

// 主站命令不带速度时使用默认速度；当前转给 51 时暂时只发单字符。
constexpr int kDefaultSpeed = 180;

// WiFi/TCP 重连间隔，避免连接失败时疯狂重试占满 CPU。
constexpr unsigned long kWifiRetryMs = 8000;
constexpr unsigned long kTcpRetryMs = 1500;

// 状态灯闪烁节奏：WiFi 未连时快闪，WiFi 已连但 TCP 未注册时慢闪。
constexpr unsigned long kWifiBlinkMs = 150;
constexpr unsigned long kTcpBlinkMs = 650;

// 1 号车热点的固定 IP。
const IPAddress kMasterIp(192, 168, 4, 1);

// 主站转发过来的运动命令，例如 CMD,8,FWD,180。
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

// ESP8266 板载 LED 通常是低电平点亮，所以 on=true 时写 LOW。
void setStatusLed(bool on) {
  statusLedOn = on;
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

// 根据当前连接状态刷新 LED，方便不用串口也能看出卡在哪一步。
void updateStatusLed() {
  const unsigned long now = millis();
  const unsigned long blinkMs =
      (WiFi.status() == WL_CONNECTED) ? kTcpBlinkMs : kWifiBlinkMs;

  if (registeredWithMaster) {
    // 已经向主站 HELLO,CAR2 注册成功：LED 常亮。
    setStatusLed(true);
    return;
  }

  if (now - lastStatusBlinkMs >= blinkMs) {
    lastStatusBlinkMs = now;
    setStatusLed(!statusLedOn);
  }
}

// 主站允许的动作名白名单。
bool isValidAction(const String& action) {
  return action == "STOP" || action == "FWD" || action == "BACK" ||
         action == "LEFT" || action == "RIGHT" || action == "SPINL" ||
         action == "SPINR";
}

// 解析速度字段，空字段使用默认值，非法字符会导致整条命令被丢弃。
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

// 解析主站发来的 CMD 行，成功时填入 command。
bool parseMotionCommand(const String& line, MotionCommand& command) {
  const int first = line.indexOf(',', 4);
  const int second = line.indexOf(',', first + 1);
  String actionToken;
  String speedToken;
  int speed = 0;

  if (!line.startsWith("CMD,")) {
    // 必须是 CMD 协议行。
    return false;
  }

  if (first < 0) {
    // 没找到序号后的逗号，格式错误。
    return false;
  }

  command.seq = line.substring(4, first);
  if (command.seq.length() == 0) {
    // 序号为空时不执行，因为回 DONE 时无法对应原命令。
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
    // 非白名单动作，忽略。
    return false;
  }

  if (!parseSpeed(speedToken, speed)) {
    // 速度不是纯数字，忽略。
    return false;
  }

  if (actionToken == "STOP") {
    // 停车命令速度固定为 0。
    speed = 0;
  }

  command.action = actionToken;
  command.speed = speed;
  return true;
}

// 把主站的高层动作翻译成发给 2 号车 51 的单字符命令。
void sendLocalMotion(const MotionCommand& command) {
  if (command.action == "STOP") {
    // S = Stop。
    Serial.print("S\n");
    return;
  }

  if (command.action == "FWD") {
    // F = Forward。
    Serial.print("F\n");
  } else if (command.action == "BACK") {
    // B = Back。注意当前 2 号车 51 程序暂时没有处理 B。
    Serial.print("B\n");
  } else if (command.action == "LEFT") {
    // L = Left。当前 2 号车 51 程序暂时没有处理 L。
    Serial.print("L\n");
  } else if (command.action == "RIGHT") {
    // R = Right。当前 2 号车 51 程序暂时没有处理 R。
    Serial.print("R\n");
  } else if (command.action == "SPINL") {
    // 暂时复用 L。
    Serial.print("L\n");
  } else if (command.action == "SPINR") {
    // 暂时复用 R。
    Serial.print("R\n");
  } else {
    // 兜底停车。
    Serial.print("S\n");
  }
}

// 本地停车命令，常用于断网、断 TCP 或刚上电时的安全保护。
void issueLocalStop() {
  MotionCommand stopCommand;

  stopCommand.seq = "0";
  stopCommand.action = "STOP";
  stopCommand.speed = 0;
  sendLocalMotion(stopCommand);
}

// 保持 WiFi 连接。掉线时先停车，然后按固定间隔尝试重连热点。
void ensureWifi() {
  const unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (wasLinkHealthy) {
    // 之前连通过，现在断了，说明控制链路失效，立即停车。
    issueLocalStop();
    wasLinkHealthy = false;
  }

  if (now - lastWifiAttemptMs < kWifiRetryMs) {
    return;
  }

  lastWifiAttemptMs = now;
  if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
    // 失败状态下先断开再 begin，避免 ESP8266 卡在旧状态里。
    WiFi.disconnect();
  }
  WiFi.begin(kSsid, kPassword);
}

// 保持到主站的 TCP 连接，连接成功后先发送 HELLO,CAR2 注册身份。
void ensureTcpConnection() {
  const unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    // 没有 WiFi 时 TCP 一定不可用，清理状态。
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
    // TCP 已连接，但必须等主站回 OK,CAR2 才算注册成功。
    masterClient.setNoDelay(true);
    masterClient.print("HELLO,CAR2\n");
    registeredWithMaster = false;
    inputBuffer = "";
  }
}

// 处理主站发来的一整行消息。
void processLine(const String& rawLine) {
  String line = rawLine;
  MotionCommand command;

  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (!registeredWithMaster) {
    if (line == "OK,CAR2") {
      // 注册成功后才开始执行主站的 CMD 命令。
      registeredWithMaster = true;
      wasLinkHealthy = true;
    }
    return;
  }

  if (!parseMotionCommand(line, command)) {
    // 非法命令直接忽略，不向 51 下发。
    return;
  }

  sendLocalMotion(command);
  if (masterClient.connected()) {
    // 执行完立即回 DONE,<SEQ>，主站再把状态转给手机。
    masterClient.print("DONE,");
    masterClient.print(command.seq);
    masterClient.print("\n");
  }
}

// 从 TCP 字节流中按 \n 拼出协议行。
void pollMasterMessages() {
  if (!masterClient.connected()) {
    if (wasLinkHealthy) {
      // TCP 断开也属于控制链路失效，先停车。
      issueLocalStop();
      wasLinkHealthy = false;
    }
    registeredWithMaster = false;
    return;
  }

  while (masterClient.available()) {
    const char ch = static_cast<char>(masterClient.read());

    if (ch == '\r') {
      // 兼容 \r\n 行尾。
      continue;
    }

    if (ch == '\n') {
      // 一行结束，交给 processLine 处理。
      processLine(inputBuffer);
      inputBuffer = "";
      continue;
    }

    if (inputBuffer.length() < 64) {
      // 限制缓存长度，防止异常输入占用过多内存。
      inputBuffer += ch;
    }
  }
}

}  // namespace

void setup() {
  // 初始化状态灯。
  pinMode(LED_BUILTIN, OUTPUT);
  setStatusLed(false);

  // 初始化与 51 单片机通信的串口。
  Serial.begin(kUartBaud);

  // 不保存 WiFi 配置到 Flash，避免频繁写入。
  WiFi.persistent(false);

  // STA 模式：连接 1 号车热点，而不是自己开热点。
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(kSsid, kPassword);
  lastWifiAttemptMs = millis();

  // 上电默认停车。
  issueLocalStop();
}

void loop() {
  // 主循环不断维护 WiFi、TCP、收消息和状态灯。
  ensureWifi();
  ensureTcpConnection();
  pollMasterMessages();
  updateStatusLed();
}
