#ifndef MEC_V2X_H
#define MEC_V2X_H

#include <stdint.h>  // 添加标准整数类型定义
#include "mec_common.h"

/**
 * @file mec_v2x.h
 * @brief V2X 标准消息封装（参考 GB/T 31024 / J2735）
 * 
 * 本模块负责将融合后的目标航迹转换为标准的路侧安全消息 (RSM)。
 */

#define V2X_MSG_RSM 0x01
#define V2X_PROTOCOL_VER 0x01

/**
 * @brief V2X 消息头部 (简化的标准头)
 */
typedef struct {
    uint8_t magic;      // 魔数，标识 V2X 包
    uint8_t version;    // 协议版本
    uint8_t msg_type;   // 消息类型 (例如 RSM)
    uint32_t device_id; // RSU 设备 ID
    uint64_t timestamp; // 毫秒级时间戳
} v2x_header_t;

/**
 * @brief 标准目标描述 (用于 RSM 消息体)
 */
typedef struct {
    uint16_t target_id;
    uint8_t type;       // 0:未知, 1:小车, 2:大车, 3:行人, 4:非机动车
    int32_t lat;        // 纬度 (单位: 1e-7 度)
    int32_t lon;        // 经度 (单位: 1e-7 度)
    uint16_t speed;     // 速度 (单位: 0.02 m/s)
    uint16_t heading;   // 航向 (单位: 0.0125 度)
    uint8_t confidence; // 置信度 (0-200, 映射到 0-100%)
} v2x_rsm_participant_t;

/**
 * @brief 序列化函数：将航迹列表转换为二进制 V2X 包
 * 
 * @param tracks 融合后的航迹列表
 * @param rsu_id 本机 RSU 标识
 * @param out_buf 输出缓冲区
 * @param out_len 输入为缓冲区大小，输出为实际写入长度
 * @return 0:成功, -1:空间不足或出错
 */
int v2x_encode_rsm(const track_list_t *tracks, uint32_t rsu_id, uint8_t *out_buf, int *out_len);

#endif // MEC_V2X_H
