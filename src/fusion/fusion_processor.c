#include "mec_fusion.h"
#include "mec_logging.h"
#include <math.h>
#include <math.h>

/**
 * @file fusion_processor.c
 * @brief 核心数据融合处理器实现
 * 
 * 采用了标准卡尔曼滤波 (Standard Kalman Filter) 算法，
 * 使用恒定加速度 (Constant Acceleration, CA) 运动模型。
 */

/* --- 矩阵运算辅助函数 (针对 6x6 状态空间优化) --- */

// 矩阵乘法: C = A * B
static void mat_mul(const double *A, const double *B, double *C, int m, int n, int k) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < k; j++) {
            C[i * k + j] = 0;
            for (int l = 0; l < n; l++) {
                C[i * k + j] += A[i * n + l] * B[l * k + j];
            }
        }
    }
}

// 矩阵加法: A = A + B
static void mat_add(double *A, const double *B, int size) {
    for (int i = 0; i < size; i++) A[i] += B[i];
}

// 2x2 矩阵求逆 (用于观测空间)
static int mat_inv_2x2(const double *A, double *B) {
    double det = A[0] * A[3] - A[1] * A[2];
    if (fabs(det) < 1e-12) return -1;
    double inv_det = 1.0 / det;
    B[0] = A[3] * inv_det;
    B[1] = -A[1] * inv_det;
    B[2] = -A[2] * inv_det;
    B[3] = A[0] * inv_det;
    return 0;
}

/* --- 处理器生命周期管理 --- */

fusion_processor_t* fusion_processor_create(const fusion_config_t *config) {
    if (!config) return NULL;
    
    fusion_processor_t *processor = mec_malloc(sizeof(fusion_processor_t));
    if (!processor) return NULL;
    
    processor->config = *config;
    processor->track_capacity = 100;
    processor->tracks = mec_calloc(processor->track_capacity, sizeof(fused_track_t));
    if (!processor->tracks) {
        mec_free(processor);
        return NULL;
    }
    
    processor->track_count = 0;
    processor->next_global_id = 1;
    processor->output_tracks = track_list_create(processor->track_capacity);
    
    LOG_INFO("Fusion: Processor created (Assoc Threshold: %.2f)", config->association_threshold);
    return processor;
}

void fusion_processor_destroy(fusion_processor_t *processor) {
    if (!processor) return;
    fusion_processor_stop(processor);
    track_list_release(processor->output_tracks);
    mec_free(processor->tracks);
    mec_free(processor);
}

int fusion_processor_start(fusion_processor_t *processor) {
    if (!processor) return -1;
    if (thread_create(&processor->thread_ctx, fusion_processing_thread, processor) != 0) {
        LOG_ERROR("Fusion: Failed to start thread");
        return -1;
    }
    return 0;
}

void fusion_processor_stop(fusion_processor_t *processor) {
    if (processor) thread_destroy(&processor->thread_ctx);
}

/* --- 卡尔曼滤波核心算法改进 --- */

/**
 * @brief 初始化卡尔曼滤波器
 * 状态向量 X = [x, y, vx, vy, ax, ay]^T
 */
int initialize_kalman_filter(kalman_state_t *state, const target_track_t *track) {
    if (!state || !track) return -1;
    
    memset(state->state, 0, sizeof(state->state));
    memset(state->covariance, 0, sizeof(state->covariance));

    // 初始位置
    state->state[0] = track->position.longitude;
    state->state[1] = track->position.latitude;
    
    // 初始速度 (根据航向和速率换算)
    double angle = track->heading * M_PI / 180.0;
    state->state[2] = track->velocity * cos(angle);
    state->state[3] = track->velocity * sin(angle);
    
    // 初始协方差矩阵 P (经验值初始化)
    state->covariance[0]  = 0.5;  // x
    state->covariance[7]  = 0.5;  // y
    state->covariance[14] = 2.0;  // vx
    state->covariance[21] = 2.0;  // vy
    state->covariance[28] = 5.0;  // ax
    state->covariance[35] = 5.0;  // ay
    
    state->last_update = track->timestamp;
    state->initialized = 1;
    return 0;
}

/**
 * @brief 预测步 (Prediction)
 * X_k = F * X_{k-1}
 * P_k = F * P_{k-1} * F^T + Q
 */
int predict_track_state(fused_track_t *track, double dt) {
    if (!track || dt <= 0) return -1;
    kalman_state_t *st = &track->filter_state;

    // 1. 构造状态转移矩阵 F (6x6)
    // x = x + vx*dt + 0.5*ax*dt^2
    double F[36] = {0};
    for(int i=0; i<6; i++) F[i*6+i] = 1.0;
    F[2] = dt; F[9] = dt; // vx, vy contributions to pos
    F[4] = 0.5*dt*dt; F[11] = 0.5*dt*dt; // ax, ay to pos
    F[16] = dt; F[23] = dt; // ax, ay to vel

    // 2. 预测状态 X = F * X
    double next_x[6];
    mat_mul(F, st->state, next_x, 6, 6, 1);
    memcpy(st->state, next_x, sizeof(next_x));

    // 3. 预测协方差 P = F * P * F^T + Q
    double FP[36], FPFt[36];
    double FT[36];
    for(int i=0; i<6; i++) for(int j=0; j<6; j++) FT[i*6+j] = F[j*6+i];
    
    mat_mul(F, st->covariance, FP, 6, 6, 6);
    mat_mul(FP, FT, FPFt, 6, 6, 6);
    
    // 叠加过程噪声 Q (简化处理)
    double Q_val = 0.01 * dt;
    for(int i=0; i<36; i+=7) FPFt[i] += Q_val; 

    memcpy(st->covariance, FPFt, sizeof(st->covariance));
    return 0;
}

/**
 * @brief 更新步 (Update/Correction)
 * 使用马氏距离 (Mahalanobis Distance) 改进的更新逻辑
 */
int update_kalman_filter(kalman_state_t *state, const target_track_t *meas) {
    if (!state || !meas || !state->initialized) return -1;

    // 1. 观测矩阵 H (2x6): 我们只观测位置 [x, y]
    double H[12] = {
        1, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0
    };

    // 2. 观测噪声 R (2x2)
    double R[4] = {0.1, 0, 0, 0.1};

    // 3. 计算创新值 (Innovation) y = z - H*X
    double z[2] = {meas->position.longitude, meas->position.latitude};
    double y[2] = {z[0] - state->state[0], z[1] - state->state[1]};

    // 4. 计算创新协方差 S = H * P * H^T + R
    double HP[12], S[4];
    mat_mul(H, state->covariance, HP, 2, 6, 6);
    
    double HT[12]; // H^T (6x2)
    for(int i=0; i<2; i++) for(int j=0; j<6; j++) HT[j*2+i] = H[i*6+j];
    
    mat_mul(HP, HT, S, 2, 6, 2);
    mat_add(S, R, 4);

    // 5. 计算卡尔曼增益 K = P * H^T * S^-1
    double S_inv[4];
    if (mat_inv_2x2(S, S_inv) != 0) return -1;

    double HTSinv[8]; // HT (6x2) * Sinv (2x2) = 6x2
    mat_mul(HT, S_inv, HTSinv, 6, 2, 2);

    double K[12]; // P (6x6) * HTSinv (6x2) = 6x2
    mat_mul(state->covariance, HTSinv, K, 6, 6, 2);

    // 6. 更新状态 X = X + K * y
    double Ky[6];
    mat_mul(K, y, Ky, 6, 2, 1);
    for(int i=0; i<6; i++) state->state[i] += Ky[i];

    // 7. 更新协方差 P = (I - K*H) * P
    double KH[36], I_KH[36];
    mat_mul(K, H, KH, 6, 2, 6);
    memset(I_KH, 0, sizeof(I_KH));
    for(int i=0; i<6; i++) {
        I_KH[i*6+i] = 1.0;
        for(int j=0; j<6; j++) I_KH[i*6+j] -= KH[i*6+j];
    }
    
    double next_P[36];
    mat_mul(I_KH, state->covariance, next_P, 6, 6, 6);
    memcpy(state->covariance, next_P, sizeof(next_P));

    state->last_update = meas->timestamp;
    return 0;
}

/**
 * @brief 数据关联算法：使用马氏距离 (Mahalanobis Distance)
 * 比简单的欧式距离更科学，因为它考虑了目标当前的运动不确定性。
 */
double calculate_track_distance(const fused_track_t *track, const target_track_t *meas) {
    if (!track || !meas) return 1e10;
    
    const kalman_state_t *st = &track->filter_state;
    
    // 残差 y = z - H*X
    double dy[2] = {
        meas->position.longitude - st->state[0],
        meas->position.latitude - st->state[1]
    };

    // 简化版马氏距离：使用协方差矩阵的位置分量作为权重
    // d^2 = y^T * S^-1 * y
    // 这里我们简单使用位置方差进行归一化，作为进阶的第一步
    double var_x = st->covariance[0] + 0.1; // 加上观测噪声
    double var_y = st->covariance[7] + 0.1;
    
    double dist_sq = (dy[0]*dy[0]/var_x) + (dy[1]*dy[1]/var_y);
    return sqrt(dist_sq);
}

/* --- 融合线程逻辑 (保持异步架构) --- */

int fusion_processor_add_tracks(fusion_processor_t *processor, const track_list_t *tracks, int sensor_id) {
    if (!processor || !tracks) return -1;
    
    thread_lock(&processor->thread_ctx);
    for (int i = 0; i < tracks->count; i++) {
        const target_track_t *s_track = &tracks->tracks[i];
        
        int best_idx = -1;
        double min_dist = processor->config.association_threshold;
        
        for (int j = 0; j < processor->track_count; j++) {
            double dist = calculate_track_distance(&processor->tracks[j], s_track);
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = j;
            }
        }
        
        if (best_idx >= 0) {
            update_fused_track(&processor->tracks[best_idx], s_track);
            processor->tracks[best_idx].sensor_mask |= (1 << (sensor_id - 1));
        } else if (processor->track_count < processor->track_capacity) {
            // 创建新航迹
            fused_track_t *new_t = &processor->tracks[processor->track_count++];
            new_t->global_id = processor->next_global_id++;
            new_t->type = s_track->type;
            new_t->confidence = s_track->confidence;
            new_t->age = 0;
            new_t->sensor_mask = (1 << (sensor_id - 1));
            initialize_kalman_filter(&new_t->filter_state, s_track);
        }
    }
    thread_unlock(&processor->thread_ctx);
    return 0;
}

int update_fused_track(fused_track_t *fused_track, const target_track_t *sensor_track) {
    update_kalman_filter(&fused_track->filter_state, sensor_track);
    fused_track->confidence = 0.7 * fused_track->confidence + 0.3 * sensor_track->confidence;
    fused_track->age = 0;
    fused_track->last_update = sensor_track->timestamp;
    return 0;
}

void* fusion_processing_thread(void *arg) {
    fusion_processor_t *proc = (fusion_processor_t*)arg;
    while (proc->thread_ctx.running) {
        thread_lock(&proc->thread_ctx);
        struct timeval now;
        gettimeofday(&now, NULL);
        
        track_list_clear(proc->output_tracks);
        for (int i = 0; i < proc->track_count; i++) {
            fused_track_t *t = &proc->tracks[i];
            double dt = (now.tv_sec - t->last_update.tv_sec) + (now.tv_usec - t->last_update.tv_usec)/1000000.0;
            
            predict_track_state(t, dt);
            t->age++;

            // 航迹管理：超时或置信度过低则删除
            if (t->age > proc->config.max_track_age || t->confidence < proc->config.confidence_threshold) {
                if (i < proc->track_count - 1) proc->tracks[i] = proc->tracks[proc->track_count - 1];
                proc->track_count--; i--;
                continue;
            }

            // 转换输出格式
            target_track_t out;
            out.id = t->global_id;
            out.type = t->type;
            out.position.longitude = t->filter_state.state[0];
            out.position.latitude = t->filter_state.state[1];
            out.velocity = sqrt(t->filter_state.state[2]*t->filter_state.state[2] + t->filter_state.state[3]*t->filter_state.state[3]);
            out.heading = atan2(t->filter_state.state[3], t->filter_state.state[2]) * 180.0 / M_PI;
            out.confidence = t->confidence;
            out.timestamp = now;
            track_list_add(proc->output_tracks, &out);
        }
        thread_unlock(&proc->thread_ctx);
        usleep(50000); // 20Hz 融合频率
    }
    return NULL;
}

track_list_t* fusion_processor_get_tracks(fusion_processor_t *processor) {
    return (processor) ? processor->output_tracks : NULL;
}
