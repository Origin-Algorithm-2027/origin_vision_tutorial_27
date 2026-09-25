#include "ovgimbal.hpp"

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tools/yaml.hpp"


namespace io
{
//=== 初始化 ===
OVGimbal::OVGimbal(const std::string & config_path){
    //--- 读取串口姿态固定时间补偿 ---
    try {
        auto yaml = YAML::LoadFile(config_path);
        double offset_s = -0.001;
        if (yaml["timing"].IsDefined() && yaml["timing"]["offset"].IsDefined()) {
            offset_s = yaml["timing"]["offset"].as<double>();
        }
        imu_quiry_offset_ = std::chrono::microseconds(static_cast<int64_t>(std::llround(offset_s * 1e6)));
    } catch (const std::exception & e){
        tools::logger()->warn(
        "[OVGimbal] Failed to load timing config from {}: {}", config_path, e.what());
    }

    // tools::logger()->info(
    // "[OVGimbal] timing offset={:.2f}ms", static_cast<double>(imu_quiry_offset_.count()) / 1000.0);

    //--- 打开串口 ---
    try {
        serial_.setPort("/dev/ttyACM0");
        serial_.setBaudrate(115200);
        auto timeout = serial::Timeout::simpleTimeout(1000);
        serial_.setTimeout(timeout);
        serial_.open();
        tools::logger()->info("[OVGimbal] Serial port opened successfully: /dev/ttyACM0 @ 115200");
    } catch (const std::exception & e) {
        tools::logger()->error("[OVGimbal] Failed to open serial: {}", e.what());
        throw;
    }
        
    read_thread_ = std::thread(&OVGimbal::receiving_date, this); // 打开接收数据的线程

    //--- 读取首批imu数据 ---
    tools::logger()->info("[OVGimbal] Waiting for IMU...");
    if (queue_.pop(data_ahead_, std::chrono::milliseconds(800)) &&
        queue_.pop(data_behind_, std::chrono::milliseconds(800))){
        data_prev_ = data_ahead_;
        has_prev_ = true;
        tools::logger()->info("[OVGimbal] First IMU pair received.");
    } else {
        const auto now = std::chrono::steady_clock::now();
        data_prev_ = {Eigen::Quaterniond::Identity(), now};
        data_ahead_ = data_prev_;
        data_behind_ = data_prev_;
        tools::logger()->warn("[OVGimbal] IMU startup timeout, using latest-available fallback.");
    }
}
OVGimbal::~OVGimbal() {
    quit_ = true;
    if (read_thread_.joinable()) read_thread_.join();
    try {
        serial_.close();
    } catch (...) {
    }
}
//=== 数据访问 ===
Eigen::Quaterniond OVGimbal::q_at(std::chrono::steady_clock::time_point image_timestamp) {
    // 由于照片时刻和imu传入数据的时刻不一定相同，需要计算出拍照时imu的四元数。
    if (!is_receiving_data_) return Eigen::Quaterniond::Identity(); // 没收到数据则返回无

    auto timestamp = image_timestamp + imu_quiry_offset_;

    auto interpolate = [&](const IMUData & a, const IMUData & b) { 
        // 球面线性插值计算两 IMUData 之间最可能的四元数
        auto q_a = a.q.normalized();
        auto q_b = b.q.normalized();
        const double dt = tools::delta_time(a.timestamp, b.timestamp);
        if (dt <= 1e-6) return q_b;
        const double k = std::clamp(tools::delta_time(a.timestamp, timestamp) / dt, 0.0, 1.0);
        return q_a.slerp(k, q_b).normalized();
    };

    if (timestamp <= data_ahead_.timestamp) {
        if (has_prev_ && timestamp >= data_prev_.timestamp) {
            return interpolate(data_prev_, data_ahead_);
        }
        return data_ahead_.q.normalized();
    }
    
    while (data_behind_.timestamp < timestamp) {
        // "滑动窗口"移动
        data_prev_ = data_ahead_;
        data_ahead_ = data_behind_;
        has_prev_ = true;
        if (!queue_.pop(data_behind_, std::chrono::milliseconds(2))) { // 从队列中读取data_behind_
            // 读取 data_behind_ 失败的处理
            std::lock_guard<std::mutex> lk(mutex_);
            if (is_receiving_data_ && latest_data_.timestamp > data_ahead_.timestamp) {
                return interpolate(data_ahead_, latest_data_);
            }
            return data_ahead_.q.normalized();
        }
    }
    
    // 若正常退出while，则说明 timestamp 在 data_ahead_ 和 data_behind_ 之间
    return interpolate(data_ahead_, data_behind_);
}

AutoAimMode OVGimbal::mode() const {
    std::lock_guard<std::mutex> lk(mutex_);
    switch (mode_byte_) {
        case 0x00:
            return AutoAimMode::normal;
        case 0x01:
            return AutoAimMode::small_energy;
        case 0x02:
            return AutoAimMode::big_energy;
        case 0x03:
            return AutoAimMode::anti_top;
        default:
            return AutoAimMode::unknown;
    }
}

std::string OVGimbal::mode_name() const {
    std::lock_guard<std::mutex> lk(mutex_);
    switch (mode_byte_) {
        case 0x00:
            return "normal";
        case 0x01:
            return "small_energy";
        case 0x02:
            return "big_energy";
        case 0x03:
            return "anti_top";
        default:
            return "unknown";
    }
}

OVGimbalState OVGimbal::state() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return {yaw_, yaw_vel_, pitch_, pitch_vel_, bullet_speed_};
}

bool OVGimbal::has_imu() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return is_receiving_data_;
}

double OVGimbal::offset_ms() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return static_cast<double>(imu_quiry_offset_.count()) / 1000.0;
}

double OVGimbal::bullet_speed() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return bullet_speed_;
}

uint8_t OVGimbal::mode_byte() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return mode_byte_;
}

uint8_t OVGimbal::invincibility() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return invincibility_;
}

//=== 发送 ===
void OVGimbal::send(const io::Command & command) {
    if (is_receiving_data_) return ;
    
    const uint8_t header = 0xAA;
    const uint8_t cmd_id = 0x81;

    uint8_t fire_byte = command.shoot ? 1 : 0;
    float yaw_tx = -command.yaw;
    float yaw_vel_tx = 0.0f;
    float yaw_acc_tx = 0.0f;
    float pitch_tx = -command.pitch;
    float pitch_vel_tx = 0.0f;
    float pitch_acc_tx = 0.0f;
    uint8_t armor_name_num_tx = command.armor_name_num;

    uint8_t payload[sizeof(float) * 6 + 2];
    std::memcpy(payload + 0, &pitch_tx, sizeof(float));
    std::memcpy(payload + 4, &yaw_tx, sizeof(float));
    payload[8] = fire_byte;
    std::memcpy(payload + 9, &yaw_vel_tx, sizeof(float));
    std::memcpy(payload + 13, &yaw_acc_tx, sizeof(float));
    std::memcpy(payload + 17, &pitch_vel_tx, sizeof(float));
    std::memcpy(payload + 21, &pitch_acc_tx, sizeof(float));
    std::memcpy(payload + 25, &armor_name_num_tx, sizeof(uint8_t));

    const uint8_t length = static_cast<uint8_t>(3 + sizeof(payload));
    uint8_t frame[3 + sizeof(payload)];
    frame[0] = header;
    frame[1] = length;
    frame[2] = cmd_id;
    std::memcpy(frame + 3, payload, sizeof(payload));

    try {
        serial_.write(frame, sizeof(frame));
    } catch (const std::exception & e) {
        tools::logger()->warn("[OVGimbal] Failed to write serial: {}", e.what());
    }
}

void OVGimbal::send(const io::MPCCommand & mpc_command) {
    if (is_receiving_data_) return ;

    const uint8_t header = 0xAA;
    const uint8_t cmd_id = 0x81;

    uint8_t fire_byte = mpc_command.shoot ? 1 : 0;
    float yaw_tx = -mpc_command.yaw;
    float yaw_vel_tx = -mpc_command.yaw_vel;
    float yaw_acc_tx = -mpc_command.yaw_acc;
    float pitch_tx = -mpc_command.pitch;
    float pitch_vel_tx = -mpc_command.pitch_vel;
    float pitch_acc_tx = -mpc_command.pitch_acc;
    uint8_t armor_name_num_tx = mpc_command.armor_name_num;

    uint8_t payload[sizeof(float) * 6 + 2];
    std::memcpy(payload + 0, &pitch_tx, sizeof(float));
    std::memcpy(payload + 4, &yaw_tx, sizeof(float));
    payload[8] = fire_byte;
    std::memcpy(payload + 9, &yaw_vel_tx, sizeof(float));
    std::memcpy(payload + 13, &yaw_acc_tx, sizeof(float));
    std::memcpy(payload + 17, &pitch_vel_tx, sizeof(float));
    std::memcpy(payload + 21, &pitch_acc_tx, sizeof(float));
    std::memcpy(payload + 25, &armor_name_num_tx, sizeof(uint8_t));

    const uint8_t length = static_cast<uint8_t>(3 + sizeof(payload));
    uint8_t frame[3 + sizeof(payload)];
    frame[0] = header;
    frame[1] = length;
    frame[2] = cmd_id;
    std::memcpy(frame + 3, payload, sizeof(payload));

    try {
        serial_.write(frame, sizeof(frame));
    } catch (const std::exception & e) {
        tools::logger()->warn("[OVGimbal] Failed to write serial: {}", e.what());
    }
}

void OVGimbal::send(bool control, bool fire, float yaw, float pitch, float yaw_vel,
    float yaw_acc, float pitch_vel, float pitch_acc, uint8_t armor_name_num) {
    (void)control;

    if (is_receiving_data_) return ;
    const uint8_t header = 0xAA;
    const uint8_t cmd_id = 0x81;

    uint8_t fire_byte = fire ? 1 : 0;
    float pitch_tx = -pitch;
    float pitch_vel_tx = -pitch_vel;
    float pitch_acc_tx = -pitch_acc;
    uint8_t armor_name_num_tx = armor_name_num;

    uint8_t payload[sizeof(float) * 6 + 2];
    std::memcpy(payload + 0, &pitch_tx, sizeof(float));
    std::memcpy(payload + 4, &yaw, sizeof(float));
    payload[8] = fire_byte;
    std::memcpy(payload + 9, &yaw_vel, sizeof(float));
    std::memcpy(payload + 13, &yaw_acc, sizeof(float));
    std::memcpy(payload + 17, &pitch_vel_tx, sizeof(float));
    std::memcpy(payload + 21, &pitch_acc_tx, sizeof(float));
    std::memcpy(payload + 25, &armor_name_num_tx, sizeof(uint8_t));

    const uint8_t length = static_cast<uint8_t>(3 + sizeof(payload));
    uint8_t frame[3 + sizeof(payload)];
    frame[0] = header;
    frame[1] = length;
    frame[2] = cmd_id;
    std::memcpy(frame + 3, payload, sizeof(payload));

    try {
        serial_.write(frame, sizeof(frame));
    } catch (const std::exception & e) {
        tools::logger()->warn("[OVGimbal] Failed to write serial: {}", e.what());
    }
}

//=== 接收 ===
bool OVGimbal::read_exact(uint8_t * buffer, size_t size) {
    try {
        return serial_.read(buffer, size) == size;
    } catch (...) {
        return false;
    }
}

void OVGimbal::receiving_date() {
    tools::logger()->info("[OVGimbal] read_thread started.");
    int err_count = 0;
    const uint8_t header = 0xAA;
        
    while (!quit_) {
        if (err_count > 5000) { // 多次错误将尝试重新连接串口 
            err_count = 0;
            tools::logger()->warn("[OVGimbal] Too many errors, attempting to reconnect...");
            {
                std::lock_guard<std::mutex> lk(mutex_);
                is_receiving_data_ = false;
            }
            reconnect();
            continue;
        }

        uint8_t head = 0;// 读取数据头
        if (!read_exact(&head, 1)) {
            err_count++;
            continue;
        }
        if (head != header) continue;

        uint8_t len_byte = 0;// 读取数据长度
        if (!read_exact(&len_byte, 1)) {
            err_count++;
            continue;
        }
        if (len_byte < 4 || len_byte > 64) {
            err_count++;
            continue;
        }
            
        std::vector<uint8_t> rest(len_byte - 2); // 读取剩余数据
        if (!read_exact(rest.data(), rest.size()) || rest.empty()) {
            err_count++;
            continue;
        }

        const uint8_t cmd = rest[0];
        const auto now = std::chrono::steady_clock::now();
            
        if (cmd == 0x14) {
            if (rest.size() < 1 + 10 * sizeof(float) + 1 + 1) {
                err_count++;
                continue;
            }

            float pitch_rad = 0.0f;
            float roll_rad = 0.0f;
            float yaw_rad = 0.0f;
            std::memcpy(&pitch_rad, rest.data() + 1, sizeof(float));
            std::memcpy(&roll_rad, rest.data() + 5, sizeof(float));
            std::memcpy(&yaw_rad, rest.data() + 9, sizeof(float));

            const uint8_t mode_byte = rest[13];

            float q_data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            std::memcpy(&q_data[0], rest.data() + 14, sizeof(float));
            std::memcpy(&q_data[1], rest.data() + 18, sizeof(float));
            std::memcpy(&q_data[2], rest.data() + 22, sizeof(float));
            std::memcpy(&q_data[3], rest.data() + 26, sizeof(float));

            float yaw_vel = 0.0f;
            float pitch_vel = 0.0f;
            std::memcpy(&yaw_vel, rest.data() + 30, sizeof(float));
            std::memcpy(&pitch_vel, rest.data() + 34, sizeof(float));

            float bullet_speed = 0.0f;
            std::memcpy(&bullet_speed, rest.data() + 38, sizeof(float));

            uint8_t invincibility = 0; // 读取一个8位整型，表示敌方机器人的无敌状态
            std::memcpy(&invincibility, rest.data() + 42, sizeof(uint8_t));

            Eigen::Quaterniond q(q_data[0], q_data[1], q_data[2], q_data[3]);
            queue_.push({q, now});

            { // 写入数据
                std::lock_guard<std::mutex> lk(mutex_);
                latest_data_ = {q, now};
                is_receiving_data_ = true;
                mode_byte_ = mode_byte;
                yaw_ = static_cast<double>(yaw_rad);
                pitch_ = static_cast<double>(pitch_rad);
                yaw_vel_ = static_cast<double>(yaw_vel);
                pitch_vel_ = static_cast<double>(pitch_vel);
                if (bullet_speed > 10 && bullet_speed_ < 25) bullet_speed_ = static_cast<double>(bullet_speed);
                invincibility_ = static_cast<uint8_t>(invincibility);
            }
                
            err_count = 0;
        } else if (cmd == 0x15) {
            err_count = 0;
        } else {
            static std::unordered_map<uint8_t, int> unknown_cmd_counts;
            unknown_cmd_counts[cmd]++;
            if (unknown_cmd_counts[cmd] <= 3) {
                tools::logger()->warn(
                "[OVGimbal] Unknown command: 0x{:02x} (count: {})", cmd, unknown_cmd_counts[cmd]);
            }
            err_count++;
        }
            
    }
    tools::logger()->info("[OVGimbal] read_thread stopped.");
}

void OVGimbal::reconnect() {
    for (int i = 0; i < 10 && !quit_; ++i) {
        try {
            serial_.close();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            serial_.open();
            queue_.clear();
            tools::logger()->info("[OVGimbal] Reconnected serial.");
            return;
        } catch (const std::exception & e) {
            tools::logger()->warn("[OVGimbal] Reconnect failed: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

}  // namespace io