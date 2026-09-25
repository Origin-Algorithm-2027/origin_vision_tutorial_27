#ifndef IO__OVGIMBAL_HPP
#define IO__OVGIMBAL_HPP

#include <Eigen/Geometry>
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include "io/command.hpp"
#include "serial/serial.h"
#include "tools/thread_safe_queue.hpp"

namespace io
{
struct OVGimbalState{
    double yaw;
    double yaw_vel;
    double pitch;
    double pitch_vel;
    double bullet_speed;
};

enum class AutoAimMode : uint8_t{
    normal = 0x00,
    small_energy = 0x01,
    big_energy = 0x02,
    anti_top = 0x03,
    outpost = 0x04,
    unknown = 0xff
};

/*  这是云台类，用于和电控之间通信。
 *  包括发送和接收数据。 */
class OVGimbal{
    public:
        explicit OVGimbal(const std::string & config_path);
        ~OVGimbal(); // 析构函数，对象销毁时的操作
            
        void send(const io::Command & command);
        void send(const io::MPCCommand & mpc_command);
        void send(bool control, bool fire, float yaw, float pitch, float yaw_vel, float yaw_acc, 
                float pitch_vel, float pitch_acc, uint8_t armor_name_num = 0); // 手动组织发送数据

        Eigen::Quaterniond q_at(std::chrono::steady_clock::time_point image_timestamp);
        AutoAimMode mode() const;
        std::string mode_name() const;
        OVGimbalState state() const;
        bool has_imu() const;
        double offset_ms() const;
        double bullet_speed() const;
        uint8_t mode_byte() const;
        uint8_t invincibility() const;
        
    private:
        struct IMUData {
            Eigen::Quaterniond q;
            std::chrono::steady_clock::time_point timestamp;
        };
        tools::ThreadSafeQueue<IMUData> queue_{1000};

        serial::Serial serial_; // 串口
        std::thread read_thread_; // 读取电控数据的线程
        std::atomic<bool> quit_{false}; // 退出标志
        mutable std::mutex mutex_; // 互斥锁

        std::chrono::microseconds imu_quiry_offset_{-1000}; // 一个延迟，用于补偿通信间的时间差
        
        // 三个数据组成"滑动窗口"，用于计算给定时刻四元数
        // ——————————————————————————————————————————————————————————————> t
        // data_prev_   data_ahead_   data_behind_
        IMUData data_ahead_;
        IMUData data_behind_;
        IMUData data_prev_;
        IMUData latest_data_;
        bool has_prev_{false};
            
        // 读取来自电控的数据
        bool is_receiving_data_{false};
        uint8_t mode_byte_{0};
        double yaw_ = 0.0;
        double pitch_ = 0.0;
        double yaw_vel_ = 0.0;
        double pitch_vel_ = 0.0;
        double bullet_speed_ = 23.0;
        uint8_t invincibility_ = 0; // 读取一个8位二进制，每一位表示一种机器人是否无敌

        void receiving_date(); // 接收线程的函数
        bool read_exact(uint8_t * buffer, size_t size); // 从串口读取一定长度的数据，并返回是否成功
        void reconnect(); // 当多次读取错误时，尝试重新连接串口
};

}

#endif // IO__OVGIMBAL_HPP