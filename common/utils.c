/* ============================================================
 * utils.c
 * 公共工具函数实现
 *
 * 包含：
 *   - calculate_checksum : 字节流累加校验
 *   - get_time_ms        : 高精度挂钟时间戳
 *   - get_file_size      : 文件大小查询
 *
 * 原 checksum.c 的实现已合并至此，checksum.h 废弃不再使用。
 * ============================================================ */

#include "utils.h"

#include <stdio.h>
#include <sys/time.h>

/* ------------------------------------------------------------
 * calculate_checksum
 *
 * 实现：对缓冲区内每个字节做无符号累加。
 *
 * 强制转换为 unsigned char 的原因：
 *   C 语言中 char 可能是有符号类型，直接累加负值会导致
 *   checksum 偏小，转为 unsigned char 后每字节范围 [0,255]，
 *   结果与平台无关。
 * ------------------------------------------------------------ */
uint32_t calculate_checksum(char *data, int len)
{
    uint32_t sum = 0;
    int i;

    for (i = 0; i < len; i++)
    {
        sum += (unsigned char)data[i];
    }

    return sum;
}

/* ------------------------------------------------------------
 * get_time_ms
 *
 * 实现：调用 gettimeofday 获取秒 + 微秒，转换为毫秒。
 *
 * 为什么不用 clock()：
 *   clock() 返回 CPU 时间，在 recvfrom 等阻塞调用期间
 *   不会计时，导致统计的传输时间偏低、吞吐量虚高。
 *   gettimeofday 返回真实挂钟时间，能正确反映网络延迟。
 * ------------------------------------------------------------ */
double get_time_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    /* 秒转毫秒 + 微秒转毫秒 */
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* ------------------------------------------------------------
 * get_file_size
 *
 * 实现：以二进制只读方式打开文件，seek 到末尾后 ftell
 *       即为文件字节数，随后关闭文件。
 * ------------------------------------------------------------ */
long get_file_size(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    long  size;

    if (fp == NULL)
    {
        return -1; /* 文件不存在或无权限 */
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);

    return size;
}
