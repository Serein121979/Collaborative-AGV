#include "car_firmware.h"

#include <string.h>

#include "board_config.h"
#include "mcs51_sdcc.h"

#define RX_BUFFER_SIZE 32

typedef unsigned char u8;
typedef unsigned int u16;

typedef enum {
  MOTION_STOP = 0,
  MOTION_FWD,
  MOTION_BACK,
  MOTION_LEFT,
  MOTION_RIGHT,
  MOTION_SPINL,
  MOTION_SPINR
} Motion;

static volatile u8 g_rx_index = 0;
static volatile u8 g_line_ready = 0;
static volatile char g_rx_working[RX_BUFFER_SIZE];
static volatile char g_rx_latest[RX_BUFFER_SIZE];

static volatile u16 g_command_age_ms = COMMAND_TIMEOUT_MS;
static volatile u8 g_pwm_phase = 0;

static volatile u8 g_left_forward = 1;
static volatile u8 g_right_forward = 1;
static volatile u8 g_left_speed = 0;
static volatile u8 g_right_speed = 0;

static void timer0_reload(void) {
  TH0 = TIMER0_RELOAD_HIGH;
  TL0 = TIMER0_RELOAD_LOW;
}

static void motor_stop(void) {
  g_left_speed = 0;
  g_right_speed = 0;
  MOTOR_LEFT_IN1 = 0;
  MOTOR_LEFT_IN2 = 0;
  MOTOR_RIGHT_IN1 = 0;
  MOTOR_RIGHT_IN2 = 0;
  MOTOR_LEFT_EN = 0;
  MOTOR_RIGHT_EN = 0;
}

static void motor_apply_pwm(void) {
  if (g_left_speed == 0) {
    MOTOR_LEFT_EN = 0;
    MOTOR_LEFT_IN1 = 0;
    MOTOR_LEFT_IN2 = 0;
  } else {
    MOTOR_LEFT_IN1 = g_left_forward;
    MOTOR_LEFT_IN2 = !g_left_forward;
    MOTOR_LEFT_EN = (g_left_speed > g_pwm_phase) ? 1 : 0;
  }

  if (g_right_speed == 0) {
    MOTOR_RIGHT_EN = 0;
    MOTOR_RIGHT_IN1 = 0;
    MOTOR_RIGHT_IN2 = 0;
  } else {
    MOTOR_RIGHT_IN1 = g_right_forward;
    MOTOR_RIGHT_IN2 = !g_right_forward;
    MOTOR_RIGHT_EN = (g_right_speed > g_pwm_phase) ? 1 : 0;
  }
}

static void motion_set(Motion motion, u8 speed) {
  u16 scaled = 0;
  u8 inner_speed = 0;

  if (motion == MOTION_STOP || speed == 0) {
    motor_stop();
    return;
  }

  scaled = (u16)speed * TURN_INNER_SPEED_PERCENT;
  inner_speed = (u8)(scaled / 100u);

  switch (motion) {
    case MOTION_FWD:
      g_left_forward = 1;
      g_right_forward = 1;
      g_left_speed = speed;
      g_right_speed = speed;
      break;

    case MOTION_BACK:
      g_left_forward = 0;
      g_right_forward = 0;
      g_left_speed = speed;
      g_right_speed = speed;
      break;

    case MOTION_LEFT:
      g_left_forward = 1;
      g_right_forward = 1;
      g_left_speed = inner_speed;
      g_right_speed = speed;
      break;

    case MOTION_RIGHT:
      g_left_forward = 1;
      g_right_forward = 1;
      g_left_speed = speed;
      g_right_speed = inner_speed;
      break;

    case MOTION_SPINL:
      g_left_forward = 0;
      g_right_forward = 1;
      g_left_speed = speed;
      g_right_speed = speed;
      break;

    case MOTION_SPINR:
      g_left_forward = 1;
      g_right_forward = 0;
      g_left_speed = speed;
      g_right_speed = speed;
      break;

    default:
      motor_stop();
      break;
  }
}

static u8 parse_u8(const char* text, u8* out_value) {
  u16 value = 0;
  const char* cursor = text;

  if (*cursor == '\0') {
    return 0;
  }

  while (*cursor != '\0') {
    if (*cursor < '0' || *cursor > '9') {
      return 0;
    }
    value = (u16)(value * 10u + (u16)(*cursor - '0'));
    if (value > 255u) {
      return 0;
    }
    ++cursor;
  }

  *out_value = (u8)value;
  return 1;
}

static u8 match_action(const char* action, Motion* motion) {
  if (strcmp(action, "FWD") == 0) {
    *motion = MOTION_FWD;
    return 1;
  }
  if (strcmp(action, "BACK") == 0) {
    *motion = MOTION_BACK;
    return 1;
  }
  if (strcmp(action, "LEFT") == 0) {
    *motion = MOTION_LEFT;
    return 1;
  }
  if (strcmp(action, "RIGHT") == 0) {
    *motion = MOTION_RIGHT;
    return 1;
  }
  if (strcmp(action, "SPINL") == 0) {
    *motion = MOTION_SPINL;
    return 1;
  }
  if (strcmp(action, "SPINR") == 0) {
    *motion = MOTION_SPINR;
    return 1;
  }
  return 0;
}

static void handle_line(char* line) {
  Motion motion = MOTION_STOP;
  u8 speed = 0;
  char* action = 0;
  char* speed_text = 0;

  if (strcmp(line, "#STOP") == 0) {
    motor_stop();
    g_command_age_ms = 0;
    return;
  }

  if (strncmp(line, "#RUN,", 5) != 0) {
    return;
  }

  action = line + 5;
  speed_text = strchr(action, ',');
  if (speed_text == 0) {
    return;
  }

  *speed_text = '\0';
  ++speed_text;

  if (!match_action(action, &motion)) {
    return;
  }

  if (!parse_u8(speed_text, &speed)) {
    return;
  }

  motion_set(motion, speed);
  g_command_age_ms = 0;
}

static u8 fetch_latest_line(char* out_line) {
  u8 has_line = 0;
  u8 index = 0;

  EA = 0;
  if (g_line_ready) {
    while (index < RX_BUFFER_SIZE) {
      out_line[index] = g_rx_latest[index];
      if (g_rx_latest[index] == '\0') {
        break;
      }
      ++index;
    }
    out_line[RX_BUFFER_SIZE - 1] = '\0';
    g_line_ready = 0;
    has_line = 1;
  }
  EA = 1;

  return has_line;
}

void CarFirmware_Init(void) {
  TMOD = 0x21;
  PCON &= 0x7F;

  SCON = 0x50;
  TI = 0;
  RI = 0;
  TH1 = UART_TIMER1_RELOAD;
  TL1 = UART_TIMER1_RELOAD;
  TR1 = 1;

  timer0_reload();
  ET0 = 1;
  TR0 = 1;

  ES = 1;
  EA = 1;

  motor_stop();
}

void CarFirmware_Task(void) {
  char line[RX_BUFFER_SIZE];

  if (fetch_latest_line(line)) {
    handle_line(line);
  }

  if (g_command_age_ms >= COMMAND_TIMEOUT_MS) {
    motor_stop();
  }
}

void timer0_isr(void) __interrupt(1) {
  timer0_reload();

  if (g_command_age_ms < 60000u) {
    ++g_command_age_ms;
  }

  ++g_pwm_phase;
  motor_apply_pwm();
}

void uart_isr(void) __interrupt(4) {
  u8 value = 0;
  u8 index = 0;

  if (TI) {
    TI = 0;
  }

  if (!RI) {
    return;
  }

  RI = 0;
  value = SBUF;

  if (value == '\r') {
    return;
  }

  if (value == '\n') {
    if (g_rx_index > 0) {
      for (index = 0; index < g_rx_index; ++index) {
        g_rx_latest[index] = g_rx_working[index];
      }
      g_rx_latest[g_rx_index] = '\0';
      g_line_ready = 1;
    }
    g_rx_index = 0;
    return;
  }

  if (g_rx_index >= RX_BUFFER_SIZE - 1) {
    g_rx_index = 0;
    return;
  }

  g_rx_working[g_rx_index] = (char)value;
  ++g_rx_index;
}
