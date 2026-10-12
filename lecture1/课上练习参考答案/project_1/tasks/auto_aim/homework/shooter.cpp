#include "shooter.hpp"

#include <algorithm>
#include <cmath>

#include <yaml-cpp/yaml.h>

#include "tools/logger.hpp"
#include "tools/math_tools.hpp"

namespace auto_aim
{
Shooter::Shooter(const std::string & config_path)
: last_command_{false, false, 0, 0}, high_spin_force_fire_active_{false}
{
  auto yaml = YAML::LoadFile(config_path);
  first_tolerance_ = yaml["first_tolerance"].as<double>() / 57.3;    // degree to rad
  second_tolerance_ = yaml["second_tolerance"].as<double>() / 57.3;  // degree to rad
  judge_distance_ = yaml["judge_distance"].as<double>();

  shooter_judge_distance_ = yaml["shooter_judge_distance"].as<std::vector<double>>();
  shooter_tolerance_ = yaml["shooter_tolerance"].as<std::vector<double>>();
  is_multiple_tolerance_ = yaml["is_multiple_tolerance"].as<bool>();

  auto_fire_ = yaml["auto_fire"].as<bool>();
  high_spin_force_fire_enabled_ = yaml["high_spin_force_fire_enabled"].as<bool>(false);
  high_spin_force_fire_enter_speed_ = yaml["high_spin_force_fire_enter_speed"].as<double>(12.0);
  high_spin_force_fire_exit_speed_ = yaml["high_spin_force_fire_exit_speed"].as<double>(9.0);
  if (high_spin_force_fire_exit_speed_ > high_spin_force_fire_enter_speed_) {
    high_spin_force_fire_exit_speed_ = std::max(0.0, high_spin_force_fire_enter_speed_ * 0.75);
  }
}

bool Shooter::shoot(
  const io::Command & command, const auto_aim::Aimer & aimer,
  const std::list<auto_aim::Target> & targets, const Eigen::Vector3d & gimbal_pos,
  bool tracker_tracking)
{
  if (!command.control || targets.empty() || !auto_fire_) {
    high_spin_force_fire_active_ = false;
    return false;
  }

  const auto & target = targets.front();
  const auto angular_speed = std::abs(target.ekf_x()[7]);
  const bool aim_locked = tracker_tracking && aimer.debug_aim_point.valid;

  if (!high_spin_force_fire_enabled_ || !aim_locked) {
    high_spin_force_fire_active_ = false;
  } else if (high_spin_force_fire_active_) {
    if (angular_speed < high_spin_force_fire_exit_speed_) high_spin_force_fire_active_ = false;
  } else if (angular_speed > high_spin_force_fire_enter_speed_) {
    high_spin_force_fire_active_ = true;
  }

  if (high_spin_force_fire_active_) {
    last_command_ = command;
    return true;
  }

  auto target_x = target.ekf_x()[0];
  auto target_y = target.ekf_x()[2];
  double distance = std::sqrt(tools::square(target_x) + tools::square(target_y));
  auto tolerance =  distance > judge_distance_ ? second_tolerance_ : first_tolerance_;
  
  if(is_multiple_tolerance_) { // 使用多个阈值分段的方式
    for(int i = 0; i < shooter_judge_distance_.size() ; i ++) {
      if(distance < shooter_judge_distance_.at(i)){
        tolerance = shooter_tolerance_.at(i) / 57.3;
        break;
      }
    }
  }
  // tools::logger()->debug("d(command.yaw) is {:.4f}", std::abs(last_command_.yaw - command.yaw));
  if (
    std::abs(last_command_.yaw - command.yaw) < tolerance * 2 &&  //此时认为command突变不应该射击
    std::abs(gimbal_pos[0] - command.yaw) < tolerance &&    //应该减去上一次command的yaw值
    std::abs(gimbal_pos[1] - command.pitch) < tolerance &&
    aimer.debug_aim_point.valid) {
    last_command_ = command;
    return true;
  }

  last_command_ = command;
  return false;
}

}  // namespace auto_aim
