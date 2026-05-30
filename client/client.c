/* ============================================================
 * client.c
 * 客户端入口
 *
 * 职责：
 *   1. 解析命令行参数（文件路径、传输模式）
 *   2. 创建 UDP socket 并配置服务器地址
 *   3. 调用 send_file() 执行文件传输
 *
 * 用法：
 *   ./client_app <文件路径> variable
 *   ./client_app <文件路径> fixed
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/protocol.h"

/* send_file 在 sender.c 中实现 */
void send_file(int sockfd,
               struct sockaddr_in *server_addr,
               const char *filename,
               int mode);

int main(int argc, char *argv[])
{
    int                sockfd;
    struct sockaddr_in server_addr;
    int                mode;

    /* --------------------------------------------------------
     * 参数检查：必须提供文件路径和模式两个参数
     * -------------------------------------------------------- */
    if (argc != 3)
    {
        printf("Usage:\n");
        printf("  ./client_app <file> variable\n");
        printf("  ./client_app <file> fixed\n");
        return 1;
    }

    /* --------------------------------------------------------
     * 解析模式参数
     * -------------------------------------------------------- */
    if (strcmp(argv[2], "variable") == 0)
    {
        mode = MODE_VARIABLE;
    }
    else if (strcmp(argv[2], "fixed") == 0)
    {
        mode = MODE_FIXED;
    }
    else
    {
        printf("Error: invalid mode \"%s\"\n", argv[2]);
        printf("       use \"variable\" or \"fixed\"\n");
        return 1;
    }

    /* --------------------------------------------------------
     * 创建 UDP socket
     * AF_INET    : IPv4
     * SOCK_DGRAM : 数据报（UDP）
     * -------------------------------------------------------- */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    /* --------------------------------------------------------
     * 配置服务器地址
     * IP 和端口均来自 protocol.h，便于统一修改。
     * -------------------------------------------------------- */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    /* --------------------------------------------------------
     * 执行文件传输
     * -------------------------------------------------------- */
    send_file(sockfd, &server_addr, argv[1], mode);

    close(sockfd);
    return 0;
}
