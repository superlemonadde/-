/* ============================================================
 * receiver.c
 * 服务端文件接收逻辑
 *
 * 协议说明：
 *
 *   VARIABLE 模式（可变批大小）：
 *     与发送端同步，按 1→2→3→1→... 循环计数，
 *     每凑满一批（或收到 is_last 包）发送一次 ACK。
 *
 *   FIXED 模式（固定批大小）：
 *     每收到 2 个 DU（或收到 is_last 包）发送一次 ACK。
 *
 * 文件写入说明：
 *   - 普通包按 DU_SIZE 写入（含补零，保证对齐）
 *   - 最后一个有效包（is_last=1 且 length>0）按 packet.length 写入，
 *     去除尾部补零，保证 received.bin 与原始文件字节数完全一致。
 *
 * 校验和说明：
 *   所有包（含最后包）均按 DU_SIZE 计算 checksum，
 *   与发送端口径一致，结束后两端值相等即表示传输正确。
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/utils.h"

void receive_file(int sockfd)
{
    FILE      *fp;
    Packet     packet;
    AckPacket  ack;

    struct sockaddr_in client_addr;
    socklen_t          addr_len = sizeof(client_addr);

    int recv_count   = 0; /* 当前批次已收包数          */
    int batch_size   = 1; /* 当前批大小（与发送端同步） */

    long     total_bytes  = 0; /* 写入磁盘的有效字节数（不含补零）*/
    int      total_packets = 0; /* 收到的总包数（含最后标志包）     */
    uint32_t checksum     = 0; /* 接收端校验和（按 DU_SIZE 计算）  */

    int mode = MODE_VARIABLE; /* 初始占位，第一包到达后从 packet.mode 读取 */

    /* --------------------------------------------------------
     * 创建输出文件
     * -------------------------------------------------------- */
    fp = fopen("received.bin", "wb");
    if (fp == NULL)
    {
        printf("Error: cannot create received.bin\n");
        return;
    }

    printf("\n====================================\n");
    printf(" UDP File Receiver Started\n");
    printf("====================================\n");
    printf("Waiting for file...\n");

    /* ========================================================
     * 主接收循环
     * ======================================================== */
    while (1)
    {
        /* 阻塞等待下一个数据包 */
        recvfrom(sockfd,
                 &packet,
                 sizeof(packet),
                 0,
                 (struct sockaddr *)&client_addr,
                 &addr_len);

        /* 从包中读取模式（发送端每个包都携带） */
        mode = packet.mode;

        /* --------------------------------------------------------
         * 第一个包到达时打印模式信息
         * -------------------------------------------------------- */
        if (total_packets == 0)
        {
            if (mode == MODE_VARIABLE)
                printf("\nProtocol Mode : VARIABLE\n");
            else
                printf("\nProtocol Mode : FIXED\n");

            printf("DU Size       : %d Bytes\n", DU_SIZE);
            printf("\nReceiving data...\n");
        }

        /* --------------------------------------------------------
         * 写文件 & 更新 checksum
         *
         * 只处理携带有效数据的包：
         *   - length > 0 : 普通包或最后一个有效包
         *   - is_last=1 且 length=0 : 纯结束标志包，不写文件
         * -------------------------------------------------------- */
        if (packet.length > 0)
        {
            if (packet.is_last)
            {
                /* ----------------------------------------
                 * 最后一个有效包：按实际 length 写入，
                 * 去除发送端补的 0，保证文件大小精确。
                 *
                 * Bug 修复：原代码始终写 DU_SIZE，
                 * 导致 received.bin 比原始文件大。
                 * ---------------------------------------- */
                fwrite(packet.data, 1, packet.length, fp);
                total_bytes += packet.length;
            }
            else
            {
                /* 普通包：按 DU_SIZE 写入（数据对齐）*/
                fwrite(packet.data, 1, DU_SIZE, fp);
                total_bytes += DU_SIZE;
            }

            /* checksum 统一按 DU_SIZE 计算（含补零），
             * 与发送端口径一致。 */
            checksum += calculate_checksum(packet.data, DU_SIZE);
        }

        recv_count++;
        total_packets++;

        /* --------------------------------------------------------
         * ACK 发送逻辑
         *
         * 两种模式均遵循：凑满本批 OR 收到最后一包 → 发 ACK。
         *
         * FIXED 模式特别说明：
         *   原代码在 is_last 条件上有歧义，此处统一为
         *   "recv_count >= batch_size || packet.is_last"，
         *   防止文件末尾不足一批时 ACK 永远不发送，导致客户端挂死。
         * -------------------------------------------------------- */
        if (mode == MODE_VARIABLE)
        {
            if (recv_count >= batch_size || packet.is_last)
            {
                ack.ack = packet.seq;

                sendto(sockfd,
                       &ack,
                       sizeof(ack),
                       0,
                       (struct sockaddr *)&client_addr,
                       addr_len);

                recv_count = 0;

                /* 更新下一批大小：1→2→3→1→... */
                batch_size++;
                if (batch_size > 3)
                    batch_size = 1;
            }
        }
        else /* MODE_FIXED */
        {
            if (recv_count >= 2 || packet.is_last)
            {
                ack.ack = packet.seq;

                sendto(sockfd,
                       &ack,
                       sizeof(ack),
                       0,
                       (struct sockaddr *)&client_addr,
                       addr_len);

                recv_count = 0;
                /* FIXED 模式 batch_size 固定为 2，无需修改 */
            }
        }

        /* --------------------------------------------------------
         * 收到最后一个包（无论有无数据），结束接收循环
         * -------------------------------------------------------- */
        if (packet.is_last)
            break;
    }

    fclose(fp);

    /* --------------------------------------------------------
     * 输出统计结果
     * -------------------------------------------------------- */
    printf("\n====================================\n");
    printf(" File Receive Completed\n");
    printf("====================================\n");
    printf("Total Packets     : %d\n",   total_packets);
    printf("File Size         : %ld Bytes\n", total_bytes);
    printf("Receiver Checksum : %u\n",   checksum);
}
