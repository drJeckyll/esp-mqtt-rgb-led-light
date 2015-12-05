/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "user_light.h"
#include "pwm.h"

// next define is from hw_timer.h
#define FRC1_ENABLE_TIMER BIT7
int in_pwm = 0;
void _pwm_stop()
{
	in_pwm = 0;
	RTC_CLR_REG_MASK(FRC1_CTRL_ADDRESS, FRC1_ENABLE_TIMER);
}
void _pwm_start()
{
	if (!in_pwm) {
		uint32 io_info[][3] = { 
		                        {PWM_0_OUT_IO_MUX, PWM_0_OUT_IO_FUNC ,PWM_0_OUT_IO_NUM},
		                        {PWM_1_OUT_IO_MUX, PWM_1_OUT_IO_FUNC, PWM_1_OUT_IO_NUM},
		                        {PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC, PWM_2_OUT_IO_NUM},
		                        {PWM_3_OUT_IO_MUX, PWM_3_OUT_IO_FUNC, PWM_3_OUT_IO_NUM},
		                        {PWM_4_OUT_IO_MUX, PWM_4_OUT_IO_FUNC, PWM_4_OUT_IO_NUM},
		                      };

		/*PIN FUNCTION INIT FOR PWM OUTPUT*/
		pwm_init(sysCfg.pwm_period, sysCfg.pwm_duty, PWM_CHANNEL, io_info);

		set_pwm_debug_en(0); //disable debug print in pwm driver
		os_printf("PWM version : %08x \r\n",get_pwm_version());		
	}
	in_pwm = 1;
	pwm_start();
}

MQTT_Client mqttClient;

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

	MQTT_Subscribe(client, MQTT_TOPIC_PERIOD, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_ALL, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_ALL_COLORS, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_ALL_WHITE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_RED, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_GREEN, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_BLUE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_COLD_WHITE, 0);
	MQTT_Subscribe(client, MQTT_TOPIC_WARM_WHITE, 0);

	MQTT_Subscribe(client, MQTT_TOPIC_SETTINGS, 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttSendSettings(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;

	char buff[100] = "";

	os_sprintf(buff, "{\"r\":%d,\"g\":%d,\"b\":%d,\"cw\":%d,\"ww\":%d,\"p\":%d,\"pwm\":%d}",
		sysCfg.pwm_duty[LIGHT_RED],
		sysCfg.pwm_duty[LIGHT_GREEN],
		sysCfg.pwm_duty[LIGHT_BLUE],
		sysCfg.pwm_duty[LIGHT_COLD_WHITE],
		sysCfg.pwm_duty[LIGHT_WARM_WHITE],
		sysCfg.pwm_period,
		in_pwm
	);
	MQTT_Publish(client, MQTT_TOPIC_SETTINGS_REPLY, buff, os_strlen(buff), 0, 0);
}

void setLight(uint32_t light, uint32_t pin, uint32_t value, uint32_t save)
{
	sysCfg.pwm_duty[light] = value;
	if (sysCfg.pwm_duty[light] >= (int)(sysCfg.pwm_period * 1000 / 45)) {
		_pwm_stop();
		GPIO_OUTPUT_SET(pin, 1);
	} else
	if (sysCfg.pwm_duty[light] == 0) {
		_pwm_stop();
		GPIO_OUTPUT_SET(pin, 0);
	} else {
		pwm_set_duty(sysCfg.pwm_duty[light], light);
		_pwm_start();
	}
	if (save) CFG_Save();
}

void setPeriod(uint32_t value, uint32_t save)
{
	sysCfg.pwm_period = value;
	if ((sysCfg.pwm_period < 1000) || (sysCfg.pwm_period > 10000)) sysCfg.pwm_period = 1000;
	pwm_set_period(sysCfg.pwm_period);
	_pwm_start();
	if (save) CFG_Save();
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len + 1),
	     *dataBuf  = (char*)os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if (os_strcmp(topicBuf, MQTT_TOPIC_PERIOD) == 0)
	{
		setPeriod(atoi(dataBuf), 1);
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL) == 0)
	{
		sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);
		if (sysCfg.pwm_duty[LIGHT_RED] >= (int)(sysCfg.pwm_period * 1000 / 45)) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 1);
		} else
		if (sysCfg.pwm_duty[LIGHT_RED] == 0) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
		} else {
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL_COLORS) == 0)
	{
		sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		if (sysCfg.pwm_duty[LIGHT_RED] >= (int)(sysCfg.pwm_period * 1000 / 45)) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 1);
		} else
		if (sysCfg.pwm_duty[LIGHT_RED] == 0) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
		} else {
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_ALL_WHITE) == 0)
	{
		sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);
		if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] >= (int)(sysCfg.pwm_period * 1000 / 45)) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 1);
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 1);
		} else
		if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] == 0) {
			_pwm_stop();
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
		} else {
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
			_pwm_start();
		}
		CFG_Save();
		mqttSendSettings(args);
	} else
	if (
		(os_strcmp(topicBuf, MQTT_TOPIC_RED) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_GREEN) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_BLUE) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_COLD_WHITE) == 0) ||
		(os_strcmp(topicBuf, MQTT_TOPIC_WARM_WHITE) == 0)
	)
	{
		uint32_t pwm = 0;

		if (os_strcmp(topicBuf, MQTT_TOPIC_RED) == 0) sysCfg.pwm_duty[LIGHT_RED] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_GREEN) == 0) sysCfg.pwm_duty[LIGHT_GREEN] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_BLUE) == 0) sysCfg.pwm_duty[LIGHT_BLUE] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_COLD_WHITE) == 0) sysCfg.pwm_duty[LIGHT_COLD_WHITE] = atoi(dataBuf);
		if (os_strcmp(topicBuf, MQTT_TOPIC_WARM_WHITE) == 0) sysCfg.pwm_duty[LIGHT_WARM_WHITE] = atoi(dataBuf);

		if (sysCfg.pwm_duty[LIGHT_RED] >= (int)(sysCfg.pwm_period * 1000 / 45))
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 1);
		else
		if (sysCfg.pwm_duty[LIGHT_RED] == 0)
			GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
		else {
			pwm = 1;
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
		}

		if (sysCfg.pwm_duty[LIGHT_GREEN] >= (int)(sysCfg.pwm_period * 1000 / 45))
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 1);
		else
		if (sysCfg.pwm_duty[LIGHT_GREEN] == 0)
			GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
		else {
			pwm = 1;
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
		}

		if (sysCfg.pwm_duty[LIGHT_BLUE] >= (int)(sysCfg.pwm_period * 1000 / 45))
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 1);
		else
		if (sysCfg.pwm_duty[LIGHT_BLUE] == 0)
			GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
		else {
			pwm = 1;
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
		}

		if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] >= (int)(sysCfg.pwm_period * 1000 / 45))
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 1);
		else
		if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] == 0)
			GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
		else {
			pwm = 1;
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
		}

		if (sysCfg.pwm_duty[LIGHT_WARM_WHITE] >= (int)(sysCfg.pwm_period * 1000 / 45))
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 1);
		else
		if (sysCfg.pwm_duty[LIGHT_WARM_WHITE] == 0)
			GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
		else {
			pwm = 1;
			pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
		}

		if (pwm) _pwm_start(); else _pwm_stop();

		CFG_Save();
		mqttSendSettings(args);
	} else
	if (os_strcmp(topicBuf, MQTT_TOPIC_SETTINGS) == 0)
	{
		mqttSendSettings(args);
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

/******************************************************************************
 * FunctionName : user_light_init
 * Description  : light demo init, mainy init pwm
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_light_init(void)
{
	uint32_t pwm = 0;
	CFG_Load();

	pwm_set_period(sysCfg.pwm_period);

	if (sysCfg.pwm_duty[LIGHT_RED] >= (int)(sysCfg.pwm_period * 1000 / 45))
		GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 1);
	else
	if (sysCfg.pwm_duty[LIGHT_RED] == 0)
		GPIO_OUTPUT_SET(PWM_0_OUT_IO_NUM, 0);
	else {
		pwm = 1;
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_RED], LIGHT_RED);
	}

	if (sysCfg.pwm_duty[LIGHT_GREEN] >= (int)(sysCfg.pwm_period * 1000 / 45))
		GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 1);
	else
	if (sysCfg.pwm_duty[LIGHT_GREEN] == 0)
		GPIO_OUTPUT_SET(PWM_1_OUT_IO_NUM, 0);
	else {
		pwm = 1;
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_GREEN], LIGHT_GREEN);
	}

	if (sysCfg.pwm_duty[LIGHT_BLUE] >= (int)(sysCfg.pwm_period * 1000 / 45))
		GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 1);
	else
	if (sysCfg.pwm_duty[LIGHT_BLUE] == 0)
		GPIO_OUTPUT_SET(PWM_2_OUT_IO_NUM, 0);
	else {
		pwm = 1;
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_BLUE], LIGHT_BLUE);
	}

	if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] >= (int)(sysCfg.pwm_period * 1000 / 45))
		GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 1);
	else
	if (sysCfg.pwm_duty[LIGHT_COLD_WHITE] == 0)
		GPIO_OUTPUT_SET(PWM_3_OUT_IO_NUM, 0);
	else {
		pwm = 1;
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_COLD_WHITE], LIGHT_COLD_WHITE);
	}

	if (sysCfg.pwm_duty[LIGHT_WARM_WHITE] >= (int)(sysCfg.pwm_period * 1000 / 45))
		GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 1);
	else
	if (sysCfg.pwm_duty[LIGHT_WARM_WHITE] == 0)
		GPIO_OUTPUT_SET(PWM_4_OUT_IO_NUM, 0);
	else {
		pwm = 1;
		pwm_set_duty(sysCfg.pwm_duty[LIGHT_WARM_WHITE], LIGHT_WARM_WHITE);
	}

	if (pwm) _pwm_start();

    os_printf("LIGHT PARAM: R: %d \r\n", sysCfg.pwm_duty[LIGHT_RED]);
    os_printf("LIGHT PARAM: G: %d \r\n", sysCfg.pwm_duty[LIGHT_GREEN]);
    os_printf("LIGHT PARAM: B: %d \r\n", sysCfg.pwm_duty[LIGHT_BLUE]);
    os_printf("LIGHT PARAM: CW: %d \r\n", sysCfg.pwm_duty[LIGHT_COLD_WHITE]);
    os_printf("LIGHT PARAM: WW: %d \r\n", sysCfg.pwm_duty[LIGHT_WARM_WHITE]);
    os_printf("LIGHT PARAM: P: %d \r\n", sysCfg.pwm_period);
}

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_delay_us(1000000);

	CFG_Load();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	//MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	user_light_init();

	INFO("\r\nSystem started ...\r\n");
}
