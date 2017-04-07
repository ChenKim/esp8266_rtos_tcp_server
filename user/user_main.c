/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"
// #include "FreeRTOS.h"
// #include "task.h"
#include "lwip/sockets.h"
#include "gpio.h"
#include "hw_timer.h"
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

#define DEMO_AP_SSID "LeonWifi"
#define DEMO_AP_PASSWORD "lsw123456789"
void delay(void)
{
    int i = 0, j = 0;

    for (i = 0; i < 500000; i++)
        for (j = 0; j < 100000; j++)
            ;
}

static os_timer_t timer;
static int state = 0;
static int count = 0;
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void set_cmd(int cmd)
{
    int bit0, bit1, bit2;

    bit0 = 0x1 & cmd;
    bit1 = !!(0x2 & cmd);
    bit2 = !!(0x4 & cmd);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(0), bit0);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(2), bit1);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(3), bit2);
    printf("11cmd is %d\n", cmd);
    // delay();
}

void timer_callback(void)
{
    if ((count%5) != 0) {
        count++;
        return;
    }

    switch (state) {
    case 0:
        set_cmd(0);
        state = 1;
        break;
    case 1:
        set_cmd(1);
        state = 2;
        break;
    case 2:
        set_cmd(2);
        state = 3;
        break;
    case 3:
        set_cmd(3);
        state = 4;
        break;
    case 4:
        set_cmd(4);
        state = 5;
        break;
    case 5:
        set_cmd(5);
        state = 0;
        break;
    }

    count++;
    if (count == 50)
        count = 0;
}

#define START 0
#define STOP  1
#define FORWARD 2
#define BACKWARD 3
#define LEFT 4
#define RIGHT 5

void parse_cmd_and_send(char* cmd)
{
    if (!strcmp(cmd, "start"))
        set_cmd(START);
    if (!strcmp(cmd, "stop"))
        set_cmd(STOP);
    if (!strcmp(cmd, "forward"))
        set_cmd(FORWARD);
    if (!strcmp(cmd, "backward"))
        set_cmd(BACKWARD);
    if (!strcmp(cmd, "left"))
        set_cmd(LEFT);
    if (!strcmp(cmd, "right"))
        set_cmd(RIGHT);
}

#define SERVER_PORT 1002
#define MAX_CONN 3
LOCAL int httpd_server_port = SERVER_PORT;

LOCAL void httpserver_task(void *pvParameters)
{
    int32 listenfd;
    int32 ret;
    struct sockaddr_in server_addr,remote_addr;
    int stack_counter=0;
    int recbytes;

    /* Construct local address structure */
    memset(&server_addr, 0, sizeof(server_addr)); /* Zero out structure */
    server_addr.sin_family = AF_INET; /* Internet address family */
    server_addr.sin_addr.s_addr = INADDR_ANY; /* Any incoming interface */
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_port = htons(httpd_server_port); /* Local port */
/* Create socket for incoming connections */
    do{
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd == -1) {
            printf("ESP8266 TCP server task > socket error\n");
            vTaskDelay(1000/portTICK_RATE_MS);
        }
    }while(listenfd == -1);
/* Bind to the local port */
    do{
        ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret != 0) {
            printf("ESP8266 TCP server task > bind fail\n");
            vTaskDelay(1000/portTICK_RATE_MS);
        }
    }while(ret != 0);
    printf("ESP8266 TCP server task > port:%d\n", ntohs(server_addr.sin_port));

    do{
/* Listen to the local connection */
        ret = listen(listenfd, MAX_CONN);
        if (ret != 0) {
            printf("ESP8266 TCP server task > failed to set listen queue!\n");
            vTaskDelay(1000/portTICK_RATE_MS);
        }
    }while(ret != 0);
    printf("ESP8266 TCP server task > listen ok\n");

    int32 client_sock;
    int32 len = sizeof(struct sockaddr_in);
    for (;;) {
        printf("ESP8266 TCP server task > wait client\n");
/*block here waiting remote connect request*/
        if ((client_sock = accept(listenfd,
                                  (struct sockaddr *)&remote_addr,
                                  (socklen_t *)&len)) < 0) {
            printf("ESP8266 TCP server task > accept fail\n");
            continue;
        }
        printf("ESP8266 TCP server task > Client from %s %d\n",
               inet_ntoa(remote_addr.sin_addr),
               htons(remote_addr.sin_port));

        char *recv_buf = (char *)zalloc(128);
        while ((recbytes = read(client_sock , recv_buf, 128)) > 0) {
            recv_buf[recbytes] = 0;
            printf("ESP8266 TCP server task > read data success %d!\nESP8266 TCP server task > %s\n", recbytes, recv_buf);
            parse_cmd_and_send(recv_buf);
        }
        free(recv_buf);
        if (recbytes <= 0) {
            printf("ESP8266 TCP server task > read data fail!\n");
            close(client_sock);
        }
    }
}



void user_init(void)
{
    struct station_config * config = (struct station_config *)zalloc(sizeof(struct station_config));
    struct ip_info info;

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 1);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(2), 0);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(3), 1);
    printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_opmode(STATION_MODE);
    sprintf(config->ssid, DEMO_AP_SSID);
    sprintf(config->password, DEMO_AP_PASSWORD);
    wifi_station_set_config(config);
    free(config);
    wifi_station_connect();

    //set static ip
    IP4_ADDR(&info.ip, 192,168,1,200);
    IP4_ADDR(&info.gw, 192,168,1,1);
    IP4_ADDR(&info.netmask, 255,255,255,0);
    wifi_station_dhcpc_stop();
    wifi_set_ip_info(STATION_IF,&info);

    // hw_timer_init(1);
    // hw_timer_set_func(timer_callback);
    // hw_timer_arm(1000000);

    //create tcp server task
    vTaskDelay(500);
    xTaskCreate(httpserver_task, (const signed char *)"httpdserver", 500, NULL, 4, NULL);
}
