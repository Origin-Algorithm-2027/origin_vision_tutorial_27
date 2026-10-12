#include "shooter.hpp"

#include <cmath>

#include <yaml-cpp/yaml.h>

namespace auto_aim
{
Shooter::Shooter(const std::string & config_path) : last_command_{false, false, 0, 0}
{
  auto yaml = YAML::LoadFile(config_path);
  first_tolerance_ = yaml["first_tolerance"].as<double>() / 57.3;    // degree to rad
  second_tolerance_ = yaml["second_tolerance"].as<double>() / 57.3;  // degree to rad
  judge_distance_ = yaml["judge_distance"].as<double>();
  auto_fire_ = yaml["auto_fire"].as<bool>();
}

bool Shooter::shoot(
  const io::Command & command, const Eigen::Vector3d & gimbal_pos, const double distance)
{
  if (!(command.control && auto_fire_)) return false;
  
  const double tolerance =
    distance > judge_distance_ ? second_tolerance_ : first_tolerance_;

  // 指令突然大幅变化时不开火。
  const bool command_is_stable =
    std::abs(last_command_.yaw - command.yaw) < tolerance * 2;

  // 云台实际位置反映的是它对上一帧 command 的响应，因此与 last_command_ 比较。
  const bool gimbal_is_stable = 
    std::abs(gimbal_pos[0] - last_command_.yaw) < tolerance;

  last_command_ = command;
  return command_is_stable && gimbal_is_stable;
}

}  // namespace auto_aim
