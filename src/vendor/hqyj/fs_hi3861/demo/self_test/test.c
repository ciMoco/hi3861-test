#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_i2c.h"
#include "hi_errno.h"
#include "hi_uart.h"

#include "hal_bsp_structAll.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_aw2013.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "hal_bsp_pcf8574.h"
#include "hal_bsp_nfc.h"
#include "hal_bsp_wifi.h"
#include "hal_bsp_mqtt.h"
#include "wifi_device.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/api_shell.h"
#include "sockets.h"

#define SSID "HQYJ_OFFICE" // wifi名
#define PSK "HqyjOffice2021*#&!" // wifi密码
#define IP "192.168.101.82" // 本机ip
#define PORT 8898
#define TASK_STACK_SIZE (1024  * 5)
#define KEY HI_IO_NAME_GPIO_14

typedef struct {
    int cmd;
    float temp;
    float humi;
    float temp_up;   // 温度上限
    float temp_down; // 温度下限
    float humi_up;   // 湿度上限
    float humi_down; // 湿度下限
    int devctrl;
    uint16_t als;
    uint16_t ps;
    uint16_t ir;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    char flag;
    char username[20];
    char password[20];
} msg_t;

typedef struct {
    float temp_up;   // 温度上限
    float temp_down; // 温度下限
    float humi_up;   // 湿度上限
    float humi_down; // 湿度下限
} threshold_t;

osThreadId_t server_task_id = 0; // tcp服务器的任务id
osThreadId_t local_task_id = 0; // 本地任务id
tn_pcf8574_io_t iopin = {0}; // io扩展芯片的外部接口
hi_gpio_value value = 0; // 保存按键引脚状态
osTimerId_t timer_id = 0; // 定时器id
osStatus_t timerstatus = 0; // 定时器状态
int flag = 0;
int n = 0; // 下标
void (*p[2])(void);
msg_t msg;

threshold_t recv_threshold = {
    .temp_up = 50,
    .temp_down = 0,
    .humi_up = 50,
    .humi_down = 0,
};

uint32_t TEMP[20] = {0};
uint32_t HUMI[20] = {0};
uint8_t IR[20] = {0};
uint8_t ALS[20] = {0};
uint8_t PS[20] = {0};

// 创建任务线程
void *task_init(osThreadAttr_t *taskclass, const char *taskname, osThreadFunc_t taskfunc, uint8_t taskpriority) {
    taskclass->name = taskname;
    taskclass->attr_bits = 0;
    taskclass->cb_mem = NULL;
    taskclass->stack_mem = NULL;
    taskclass->stack_size = TASK_STACK_SIZE;
    taskclass->priority = taskpriority;
    
    return osThreadNew((osThreadFunc_t)taskfunc, NULL, taskclass);
}

// 中断处理函数
void gpio_callback() {
    timerstatus = osTimerStart(timer_id, 1U); // 开启，定时10ms
    flag = 1;
    if(timerstatus != osOK) {
        printf("timer start error %d\n", timerstatus);
    }
}

// 定时器回调函数
void timer_callback() {
    hi_gpio_get_input_val(KEY, &value);
    if(value == HI_GPIO_VALUE0) {
        n++;
        if(n>1) {
            n = 0;
        }
    }
    osTimerStop(timer_id); // 关闭定时器
}

// 三合一显示
void show_ir_als_ps() {
    AP3216C_ReadData(&msg.ir, &msg.als, &msg.ps);
    sprintf(IR, "ir:%d", msg.ir);
    sprintf(ALS, "als:%d", msg.als);
    sprintf(PS, "ps:%d", msg.ps);
    SSD1306_ShowStr(28, 0, "YinJiabao", 16);
    SSD1306_ShowStr(15, 1, IR, 16);
    SSD1306_ShowStr(40, 2, ALS, 16);
    SSD1306_ShowStr(60, 1, PS, 16);
}
// 温湿度显示
void show_temp_humi(void) {
    SHT20_ReadData(&msg.temp, &msg.humi);
    sprintf(TEMP, "temp:%.2f", msg.temp);
    sprintf(HUMI, "humi:%.2f", msg.humi);
    SSD1306_ShowStr(25, 0, "YinJiabao", 16);
    SSD1306_ShowStr(25, 1, TEMP, 16);
    SSD1306_ShowStr(25, 2, HUMI, 16);
}

// 服务器通信任务线程
void server_task(void) {
    int sfd;
    int led_flag = 0, buzzer_flag = 0, fan_flag = 0;
    struct sockaddr_in serveraddr;
    socklen_t serveraddr_len = sizeof(serveraddr);
    char name[20] = "niuma";
    char passwd[20] = "12345678";
    
    WiFi_connectHotspots(SSID, PSK);
    if((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("socket create error!!!\n");
        return;
    }
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(IP);
    serveraddr.sin_port = htons(PORT);
    
    // 连接服务器
    if(connect(sfd, (const struct sockaddr *)&serveraddr, serveraddr_len) == -1) {
        printf("connect failed!!!\n");
        close(sfd);
        return;
    }
    printf("connect server success!!!\n");
    
    memset(&msg, 0, sizeof(msg_t));
    strcpy(msg.username, name);
    strcpy(msg.password, passwd);
    send(sfd, &msg, sizeof(msg_t), 0);
    memset(&msg, 0, sizeof(msg_t));
    recv(sfd, &msg, sizeof(msg_t), 0);
    
    if(msg.flag == 1) {
        printf("login success!!!\n");
        while(1) {
            memset(&msg, 0, sizeof(msg_t));
            printf("waiting for cmd...\n");
            recv(sfd, &msg, sizeof(msg_t), 0);
            
            switch(msg.cmd) {
            case 1: // 设置阈值
                printf("set threshold...\n");
                recv_threshold.temp_up = msg.temp_up;
                recv_threshold.humi_up = msg.humi_up;
                recv_threshold.temp_down= msg.temp_down;
                recv_threshold.humi_down = msg.humi_down ;
                msg.flag = 1;
                send(sfd, &msg, sizeof(msg_t), 0);
                printf("temp_up:%.2f, humi_up:%.2f temp_down:%.2f humi_down:%.2f\n", recv_threshold.temp_up, recv_threshold.humi_up, recv_threshold.temp_down, recv_threshold.humi_down);
                printf("threshold update success!!!\n");
                break;
            case 2: // 获取环境数据
                SHT20_ReadData(&msg.temp, &msg.humi);
                AP3216C_ReadData(&msg.ir, &msg.als, &msg.ps);
                msg.flag = 1;
                send(sfd, &msg, sizeof(msg_t), 0);
                printf("send env_data success!\n");
                printf("temp:%.2f, humi:%.2f ir:%d als:%d ps:%d\n", msg.temp, msg.humi, msg.ir, msg.als, msg.ps);
                break;
            case 3: // 控制设备
                printf("dev ctrl\n");
                if(msg.devctrl == 0x01) {
                    printf("ctrl led...\n");
                    led_flag = !led_flag;
                    set_led(led_flag);
                } else if(msg.devctrl == 0x02) {
                    printf("ctrl buzzer...\n");
                    buzzer_flag = !buzzer_flag;
                    set_buzzer(buzzer_flag);
                } else if(msg.devctrl == 0x03) {
                    printf("ctrl fan...\n");
                    fan_flag = !fan_flag;
                    set_fan(fan_flag);
                } else if(msg.devctrl == 0x04) {
                    set_led(false);
                    set_buzzer(false);
                    set_fan(false);
                } else {
                    printf("cmd error!!!\n");
                    msg.flag = 0;
                    send(sfd, &msg, sizeof(msg_t), 0);
                    break;
                }
                msg.flag = 1;
                send(sfd, &msg, sizeof(msg_t), 0);
                break;
            case 4: // 结束通信
                printf("exit...\n");
                close(sfd);
                osThreadTerminate(server_task_id);
                return;
            case 255:
                printf("Heartbeat packet arrives...\n");
                msg.flag = 1;
                send(sfd, &msg, sizeof(msg_t), 0);
                break;
            }
        }
    } else {
        printf("login failed!!!\n");
    }
}

// 维护本地任务线程
void local_task(void) {
    iopin.bit.p1 = 1; // 关闭蜂鸣器
    SSD1306_CLS();
    hi_gpio_init();
    hi_io_set_pull(KEY, HI_IO_PULL_UP); // io引脚上拉
    hi_io_set_func(KEY, HI_IO_FUNC_GPIO_14_GPIO); // 设置io为gpio
    hi_gpio_set_dir(KEY, HI_GPIO_DIR_IN); // io引脚输入
    hi_gpio_register_isr_function(KEY, HI_INT_TYPE_EDGE, HI_GPIO_EDGE_FALL_LEVEL_LOW, gpio_callback, NULL);
    
    while(1) {
        if(flag) {
            SSD1306_CLS();
            flag = 0;
        }
        p[n]();
        if(msg.temp > recv_threshold.temp_up) {
            AW2013_Control_RGB(255,0,0);
        } else if(msg.temp < recv_threshold.temp_down) {
            AW2013_Control_RGB(0,0,255);
        } else {
            AW2013_Control_RGB(0,255,0);
        }
        usleep(500 * 1000); // 100ms
    }
}

static void base_test(void) {
    printf("bast_test is ok！！！\n");

    PCF8574_Init(); // led 蜂鸣器 风扇
    SHT20_Init(); // 温湿度传感器
    AW2013_Init(); // RGB
    AP3216C_Init(); // 三合一
    SSD1306_Init(); // OLED
    
    p[0] = show_temp_humi;
    p[1] = show_ir_als_ps;
    
    // 定义任务线程结构体
    osThreadAttr_t options;
                              
    // 创建TCP服务器通信任务线程
    server_task_id = task_init(&options, "task1", server_task, osPriorityAboveNormal);
    if (server_task_id != NULL) {
        printf("server_task_id create ok!!!\n");
    }
    
    // 创建本地任务线程
    local_task_id = task_init(&options, "task2", local_task, osPriorityAboveNormal);
    if (local_task_id != NULL) {
        printf("local_task_id create ok!!!\n");
    }
    
    // 创建定时器
    timer_id = osTimerNew(timer_callback, osTimerPeriodic, NULL, NULL);
    if(timer_id != NULL) {
        printf("ID = %d, timer_id create ok!!!\n", timer_id);
    }
}

SYS_RUN(base_test);