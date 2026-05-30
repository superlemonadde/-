/* ============================================================
 * server.c
 * 服务端入口
 *
 * 职责：
 *   1. 创建 UDP socket
 *   2. 绑定本地端口（SERVER_PORT，来自 protocol.h）
 *   3. 调用 receive_file() 接收并保存文件
 *
 * 运行方式：
 *   ./server_app
 *   （无需命令行参数，监听 0.0.0.0:SERVER_PORT）
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/protocol.h"

/* receive_file 在 receiver.c 中实现 */
void receive_file(int sockfd);

int main(void)
{
    int                sockfd;
    struct sockaddr_in server_addr;

    /* --------------------------------------------------------
     * 创建 UDP socket
     * -------------------------------------------------------- */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    /* --------------------------------------------------------
     * 配置绑定地址
     * INADDR_ANY : 监听所有本地网卡
     * -------------------------------------------------------- */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* --------------------------------------------------------
     * 绑定端口
     * -------------------------------------------------------- */
    if (bind(sockfd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    /* --------------------------------------------------------
     * 开始接收文件
     * -------------------------------------------------------- */
    receive_file(sockfd);

    close(sockfd);
    return 0;
}
