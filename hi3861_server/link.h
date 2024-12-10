#ifndef __LINK_H__
#define __LINK_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define IP "192.168.250.100"
#define PORT 8899

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

typedef struct node {
    int fd;
    char id[20];
    pthread_t tid;
    struct node *next;
} link_t;

void *func_thread(void *arg);

void *heartbeat_thread();

link_t *link_init(void);

int insert_data(link_t *link, int fd, const char *name, pthread_t tid);

int id_find(link_t *link, const char *name);

int delete_data(link_t *link, int fd);

int show_list(link_t *link);

int free_link(link_t **L);

int checkUsernamePwd(const char *username, const char *pwd);

#endif /* __LINK_H__ */