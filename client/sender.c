/* ============================================================
 * sender.c
 * 客户端文件发送逻辑
 *
 * 协议说明：
 *
 *   VARIABLE 模式（可变批大小）：
 *     批大小按 1→2→3→1→... 循环，每批发完等一次 ACK。
 *     例：第1批发1个DU，等ACK；第2批发2个DU，等ACK；
 *         第3批发3个DU，等ACK；第4批回到1个DU，如此往复。
 *
 *   FIXED 模式（固定批大小）：
 *     每批固定发送 2 个 DU，发完等一次 ACK。
 *
 * 数据包格式（见 protocol.h）：
 *   - 每个包 data 字段固定 DU_SIZE 字节（不足补 0）
 *   - length 字段记录本包实际有效字节数（用于接收端截断）
 *   - is_last=1 表示文件已读完，接收端收到后结束接收
 *
 * 校验和说明：
 *   checksum 按 DU_SIZE 计算（含补零），接收端口径相同，
 *   传输结束后两端打印值相等即表示文件完整传输。
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/utils.h"

void send_file(int                sockfd,
               struct sockaddr_in *server_addr,
               const char         *filename,
               int                mode)
{
    FILE      *fp;
    Packet     packet;
    AckPacket  ack;

    socklen_t addr_len = sizeof(*server_addr);

    int seq        = 0;   /* 包序号，单调递增 */
    int batch_size;       /* 当前批大小 */
    int send_count;       /* 当前批已发包数 */

    long     total_bytes = 0; /* 实际读取的有效字节数（不含补零）*/
    uint32_t checksum    = 0; /* 发送端校验和（按 DU_SIZE 计算）  */

    double start_ms, end_ms, time_sec, throughput;

    /* --------------------------------------------------------
     * 打开文件
     * -------------------------------------------------------- */
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("Error: cannot open file \"%s\"\n", filename);
        return;
    }

    /* --------------------------------------------------------
     * 获取文件大小（仅用于打印信息）
     * -------------------------------------------------------- */
    long file_size = get_file_size(filename);

    /* --------------------------------------------------------
     * 初始化批大小
     *
     * Bug 修复：原代码两种模式统一初始化为 1，
     * 导致 FIXED 模式第一轮客户端只发 1 个包，
     * 而服务端等 2 个包才回 ACK，造成死锁。
     *
     * 修复后：FIXED 从 2 开始，VARIABLE 从 1 开始。
     * -------------------------------------------------------- */
    if (mode == MODE_FIXED)
        batch_size = 2;
    else
        batch_size = 1;

    /* --------------------------------------------------------
     * 打印传输信息
     * -------------------------------------------------------- */
    printf("\n====================================\n");
    printf(" UDP File Sender Started\n");
    printf("====================================\n");

    if (mode == MODE_VARIABLE)
    {
        printf("Protocol Mode : VARIABLE\n");
        printf("Batch Pattern : 1 -> 2 -> 3 -> repeat\n");
    }
    else
    {
        printf("Protocol Mode : FIXED\n");
        printf("Batch Size    : 2\n");
    }

    printf("DU Size       : %d Bytes\n", DU_SIZE);
    printf("File Size     : %ld Bytes\n", file_size);
    printf("\nStarting transmission...\n");

    /* --------------------------------------------------------
     * 记录传输开始时间（挂钟时间，含网络等待）
     * 使用 get_time_ms() 而非 clock()，避免阻塞期间不计时
     * 导致吞吐量虚高的问题。
     * -------------------------------------------------------- */
    start_ms = get_time_ms();

    /* ========================================================
     * 主发送循环
     *
     * 每轮发送 batch_size 个 DU，然后等待一次 ACK，
     * 收到 ACK 后调整下一轮批大小，直到文件发完。
     * ======================================================== */
    while (1)
    {
        send_count = 0;

        /* ----------------------------------------------------
         * 内层循环：发送当前批次的所有 DU
         * ---------------------------------------------------- */
        while (send_count < batch_size)
        {
            memset(&packet, 0, sizeof(packet));

            /* 从文件读取最多 DU_SIZE 字节 */
            int n = fread(packet.data, 1, DU_SIZE, fp);

            if (n > 0)
            {
                /* ----------------------------------------
                 * 正常数据包
                 *
                 * length 记录本次实际读到的字节数。
                 * Bug 修复：原代码始终写 DU_SIZE，
                 * 导致接收端无法得知最后一包的真实长度，
                 * received.bin 尾部会多出补零字节。
                 * ---------------------------------------- */
                packet.is_last = 0;
                packet.length  = n;   /* 实际字节数，可能 < DU_SIZE */

                /* 不足 DU_SIZE 的部分已由 memset 填 0，
                 * 保证每次 sendto 传输固定大小的 struct。 */

                total_bytes += n;

                /* checksum 按 DU_SIZE 计算（含补零），
                 * 与接收端口径一致，末尾补零不影响比对。 */
                checksum += calculate_checksum(packet.data, DU_SIZE);
            }
            else
            {
                /* ----------------------------------------
                 * 文件读完（n == 0）或读取出错（n < 0）
                 * 发送一个结束标志包：is_last=1, length=0
                 * ---------------------------------------- */
                packet.is_last = 1;
                packet.length  = 0;
            }

            packet.seq  = seq;
            packet.mode = mode;

            sendto(sockfd,
                   &packet,
                   sizeof(packet),
                   0,
                   (struct sockaddr *)server_addr,
                   addr_len);

            seq++;
            send_count++;

            /* 发出最后一个包后退出内层循环，
             * 下面仍然会等 ACK，确保服务端收到结束标志。 */
            if (packet.is_last)
                break;
        }

        /* ----------------------------------------------------
         * 等待服务端 ACK
         *
         * 本实验不处理超时重传，若 ACK 丢失会永久阻塞。
         * 如需健壮性，可用 setsockopt(SO_RCVTIMEO) 设超时。
         * ---------------------------------------------------- */
        int n = recvfrom(sockfd,
                         &ack,
                         sizeof(ack),
                         0,
                         NULL,
                         NULL);

        if (n < 0)
        {
            perror("recvfrom (ACK)");
            break;
        }

        /* 收到最后一包的 ACK，传输结束 */
        if (packet.is_last)
            break;

        /* ----------------------------------------------------
         * 更新下一轮批大小
         *
         * VARIABLE：1→2→3→1→...
         * FIXED   ：始终为 2
         * ---------------------------------------------------- */
        if (mode == MODE_VARIABLE)
        {
            batch_size++;
            if (batch_size > 3)
                batch_size = 1;
        }
        /* FIXED 模式 batch_size 初始即为 2，此处无需修改 */
    }

    end_ms    = get_time_ms();
    time_sec  = (end_ms - start_ms) / 1000.0;
    throughput = (time_sec > 0)
                 ? (double)total_bytes / time_sec / 1024.0 / 1024.0
                 : 0.0;

    /* --------------------------------------------------------
     * 输出统计结果
     * -------------------------------------------------------- */
    printf("\n====================================\n");
    printf(" Transmission Completed\n");
    printf("====================================\n");
    printf("Total Bytes       : %ld Bytes\n",  total_bytes);
    printf("Transmission Time : %.4f seconds\n", time_sec);
    printf("Throughput        : %.4f MB/s\n",  throughput);
    printf("Sender Checksum   : %u\n",          checksum);

    fclose(fp);
}
