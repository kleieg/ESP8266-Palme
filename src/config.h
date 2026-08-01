// set hostname used for MQTT tag and WiFi
#define HOSTNAME "ESP-Palme"
#define MQTT_BROKER "sym_mqtt"
#define VERSION "v 1.1.0"

#define MQTT_INTERVAL 120000
#define RECONNECT_INTERVAL 5000
#define LED_BLINK_INTERVAL 500

#define GPIO_switch D1
#define NUM_OUTPUTS 1

#define GPIO_LED_INTERN D4

#if CORE_DEBUG_LEVEL > 0
  #define LOG_INIT() Serial.begin(115200);
  #define LOG_PRINTF Serial.printf
  #define LOG_PRINTLN(line) Serial.println(line)
  #define LOG_PRINT(text) Serial.print(text)
#else
  #define LOG_INIT()
  #define LOG_PRINTF(format, ...)
  #define LOG_PRINTLN(line)
  #define LOG_PRINT(text)
#endif