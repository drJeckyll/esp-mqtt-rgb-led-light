/*
	settings for HOME
*/

#ifdef LOCATION_HOME

#define MQTT_HOST			"1.1.2.2" //or "mqtt.yourdomain.com"
#define MQTT_PORT			1883
#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE		120	 /*second*/

#define MQTT_CLIENT_ID		"HOME_ROOM_LAMP"
#define MQTT_USER			"user"
#define MQTT_PASS			"pass"

#define MQTT_TOPIC_PERIOD			"/home/myroom/lamp/period"
#define MQTT_TOPIC_ALL				"/home/myroom/lamp"
#define MQTT_TOPIC_ALL_COLORS		"/home/myroom/lamp/colors"
#define MQTT_TOPIC_ALL_WHITE		"/home/myroom/lamp/white"
#define MQTT_TOPIC_RED				"/home/myroom/lamp/r"
#define MQTT_TOPIC_GREEN			"/home/myroom/lamp/g"
#define MQTT_TOPIC_BLUE				"/home/myroom/lamp/b"
#define MQTT_TOPIC_COLD_WHITE		"/home/myroom/lamp/cw"
#define MQTT_TOPIC_WARM_WHITE		"/home/myroom/lamp/ww"

#define MQTT_TOPIC_SETTINGS			"/home/myroom/lamp/settings"
#define MQTT_TOPIC_SETTINGS_REPLY	"/home/myroom/lamp/settings/reply"

#define STA_SSID "ssid"
#define STA_PASS "pass"
//#define STA_TYPE AUTH_WPA2_PSK
#define STA_TYPE AUTH_OPEN

#endif