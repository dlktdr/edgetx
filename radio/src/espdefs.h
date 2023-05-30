#pragma once

#define ESP_PACKET_TYPE_MSK 0x0F
#define ESP_PACKET_CMD_BIT 6
#define ESP_PACKET_ACK_BIT 7
#define ESP_PACKET_ISCMD(t) (t&(1<<ESP_PACKET_CMD_BIT))
#define ESP_PACKET_ISACKREQ(t) (t&(1<<ESP_PACKET_ACK_BIT))

enum ESPModes {
  ESP_ROOT,  
  ESP_TELEMETRY,
  ESP_TRAINER,
  ESP_JOYSTICK,
  ESP_AUDIO,
  ESP_FTP,
  ESP_IMU, 
  ESP_MAX
};

enum ESPRootCmds {
  ESP_ROOTCMD_ACKNAK=0,
  ESP_ROOTCMD_START_MODE,
  ESP_ROOTCMD_STOP_MODE,
  ESP_ROOTCMD_RESTART,
  ESP_ROOTCMD_VERSION,
  ESP_ROOTCMD_CON_EVENT,
  ESP_ROOTCMD_CON_MGMNT,
};

enum ESPConnectionEvents {
  ESP_EVT_DISCOVER_STARTED,
  ESP_EVT_DISCOVER_COMPLETE,
  ESP_EVT_DEVICE_FOUND,
  ESP_EVT_CONNECTED,
  ESP_EVT_DISCONNECTED,  
  ESP_EVT_PIN_REQUEST,
  ESP_EVT_IP_OBTAINED
};

enum ESPConnectionManagment {
  ESP_CON_DISCOVER_START,
  ESP_CON_DISCOVER_STOP,
  ESP_CON_CONNECT,
  ESP_CON_DISCONNECT,
  ESP_CON_SET_NAME,
  ESP_CON_SET_MAC,
  ESP_CON_SET_SSID,
  ESP_CON_SET_IP,
  ESP_CON_SET_SUBNET_MASK,
  ESP_CON_SET_STATIC_IP,
  ESP_CON_SET_DHCP,
  ESP_CON_SET_WIFI_STATION,
  ESP_CON_SET_WIFI_AP,
};

// Channel Format
typedef struct  {
  int16_t ch[MAX_OUTPUT_CHANNELS];
  uint32_t channelmask; // Valid Channels
} channeldata;

typedef struct {
  uint8_t bteaddr[6];
  uint8_t rssi;
  char name[30];
} scanresult;

typedef struct  {
  uint8_t maj;
  uint8_t min;
  uint8_t rev;
  uint8_t sha[10]; 
} espversion;
