#include "link.h"

struct timeval timeout;
extern link_t *L;

/**
 * @brief 简单的校验用户名密码
 * @param username 用户名
 * @param pwd 密码
 * @return 0:校验成功，-1:校验失败
 */
int checkUsernamePwd(const char *username, const char *pwd) {
    const char *user = "niuma";
    const char *password = "12345678";
    
    if (strcmp(username, user) == 0 && strcmp(pwd, password) == 0) {
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief 处理任务线程
 * @author ciMoco
 */
void *func_thread(void *arg) {
    int afd = *(int *)arg;
    msg_t msg;
    
    recv(afd, &msg, sizeof(msg_t), 0);
    if(checkUsernamePwd(msg.username, msg.password) == 0) {
        msg.flag = 1;
        printf("%s 原神登录成功\n", msg.username);
        insert_data(L, afd, msg.username, pthread_self());
        send(afd, &msg, sizeof(msg_t), 0);
        
        while(1) {
            memset(&msg, 0, sizeof(msg_t));
            printf("请输入原神指令:\n");
            scanf("%d", &msg.cmd);
            getchar();
            switch (msg.cmd) {
            case 1:
                printf("输入原神温湿度的上下限，4个浮点数\n");
                if(scanf("%f%f%f%f",&msg.temp_up, &msg.temp_down, &msg.humi_up, &msg.humi_down) != 4) {
                    printf("输入错误\n");
                    break;
                }
                getchar();
                send(afd, &msg, sizeof(msg_t), 0);
                recv(afd, &msg, sizeof(msg_t), 0);
                if(msg.flag == 1) {
                    printf("设置成功\n");
                } else {
                    printf("设置失败\n");
                }
                break;
            case 2:
                printf("开始获取原神数据\n");
                send(afd, &msg, sizeof(msg_t), 0);
                recv(afd, &msg, sizeof(msg_t), 0);
                printf("temp:%.2f, humi:%.2f, ir:%d, als:%d, ps:%d\n", msg.temp, msg.humi, msg.ir, msg.als, msg.ps);
                break;
            case 3:
                printf("请控制原神外设:\n");
                scanf("%d", &msg.devctrl);
                getchar();
                send(afd, &msg, sizeof(msg_t), 0);
                recv(afd, &msg, sizeof(msg_t), 0);
                if(msg.flag == 1) {
                    printf("控制成功\n");
                } else {
                    printf("控制失败\n");
                }
                break;
            case 4:
                printf("踢出原神\n");
                send(afd, &msg, sizeof(msg_t), 0);
                delete_data(L, afd);
                pthread_exit(NULL);
                break;
            default:
                printf("指令错误\n");
                break;
            }
        }
    } else {
        printf("登录失败\n");
        msg.flag = 0;
        send(afd, &msg, sizeof(msg_t), 0);
        close(afd);
        pthread_exit(NULL);
    }
}

/**
 * @brief 心跳检测线程
 * 
 * 该线程每隔10s检查并显示链表内容,模拟心跳检测机制,
 * 用于监控客户端是否在线
 * 
 * @return 返回NULL, 表示线程正常退出
 */
void *heartbeat_thread() {
    while(1) {
        show_list(L);
        sleep(10);
    }
    pthread_exit(NULL);
}

/**
 * @brief 初始化链表头节点
 * 
 * @return 成功返回指针, 失败 NULL
 */
link_t *link_init(void) {
    link_t *L = (link_t *)malloc(sizeof(link_t));
    if(L == NULL) {
        return L;
    }
    L->next = NULL;
    return L;
}

/**
 * @brief 插入新节点
 * 
 * @param L 指向头节点的指针
 * @param fd 新节点的文件描述符
 * @param name 新节点名
 * @param tid 新节点的线程id
 * @return 返回0
 */
int insert_data(link_t *L, int fd, const char *name, pthread_t tid) {
    link_t *q = link_init();
    q->fd = fd;
    strcpy(q->id, name);
    q->tid = tid;
    
    q->next = L->next;
    L->next = q;
    return 0;
}

/**
 * @brief 在链表中查找具有指定id的节点
 * 
 * @param L 指向链表头节点的指针
 * @param name 要查找的id
 * @return 成功 0, 失败 -1
 */
int id_find(link_t *L, const char *name) {
    link_t *q = L->next;
    printf("查找id:%s\n", name);
    while(q) {
        if(strcmp(q->id, name) == 0) {
            printf("%s还在玩\n", name);
            return 0;
        }
        q = q->next;
    }
    printf("%s退出原神\n", name);
    
    return -1;
}

/**
 * @brief 从链表中删除指定文件符的节点
 * 
 * @param L 指向链表头节点的指针
 * @param fd 要删除的节点的文件描述符
 * @return 成功 0, 失败 -1
 */
int delete_data(link_t *L, int fd) {
    link_t *q = L;
    link_t *p = NULL;
    while(q->next) {
        if(q->next->fd == fd) {
            p = q->next;
            q->next = p->next;
            free(p);
            return 0;
        }
        q = q->next;
    }
    return -1;
}

int show_list(link_t *L) {
    msg_t buf;
    
    link_t *q = L;
    while(q->next) {
        buf.cmd = 255;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        setsockopt(q->next->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        printf("正在确认原神玩家在线状态...\n");
        send(q->next->fd, &buf, sizeof(msg_t), 0);
        
        if(recv(q->next->fd, &buf, sizeof(msg_t), 0) > 0) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            setsockopt(q->next->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
            printf("%s居然还在玩原神!!!\n", q->next->id);
            q = q->next;

        } else {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            setsockopt(q->next->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
            printf("%s 已退出原神\n", q->next->id);
            pthread_cancel(q->next->tid);
            close(q->next->fd);
            delete_data(L, q->next->fd);
        }
    }
    return 0;
}

/**
 * @brief 释放链表内存
 * 
 * @param L 指向链表头节点指针的指针
 * @return 成功返回0
 */ 
int free_link(link_t **L) {
    link_t *p = (*L)->next;
    while(p) {
        free(*L);
        *L = p;
        p = p->next;
    }
    
    free(*L);
    *L = NULL;
    return 0;
}