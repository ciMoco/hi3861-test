#include "link.h"

pthread_t tid;
link_t *L = NULL;

int main(int argc, const char * argv[]) {
    int sfd;
    struct sockaddr_in serveraddr, clientaddr;
    socklen_t serveraddr_len = sizeof(serveraddr);
    socklen_t clientaddr_len = sizeof(clientaddr);
    
    // 创建套接字
    if((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("socket error\n");
        return -1;
    }
    int flag = 1;
    if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))) {
        perror("setsockopt error");
    }
    // 配置服务器
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(IP);
    serveraddr.sin_port = htons(PORT);
    
    // 绑定套接字
    if(bind(sfd, (struct sockaddr *)&serveraddr, serveraddr_len) == -1) {
        perror("setsockopt error");
        return -1;
    }
    
    // 监听
    if(listen(sfd, 5) == -1) {
        perror("listen error");
        return -1;
    }
    
    L = link_init();
    
    pthread_create(&tid, NULL, heartbeat_thread, NULL);
    pthread_detach(tid);
    
    int afd = 0;
    printf("原神启动，等待玩家上线...\n");
    
    while(1) {
        if((afd = accept(sfd, (struct sockaddr *)&clientaddr, &clientaddr_len)) == -1) {
            printf("accept error\n");
            return -1;
        }
        
        printf("玩家[%s]:[%d]登录原神\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
        
        pthread_create(&tid, NULL, func_thread, &afd);
        pthread_detach(tid);
    }

    close(sfd);
    return 0;
}
