#ifndef AUTO_AIM__SHOOTER_HPP
#define AUTO_AIM__SHOOTER_HPP

#include <string>

#include "io/command.hpp"
#include "tasks/auto_aim/aimer.hpp"

namespace auto_aim
{
class Shooter
{
public:
  Shooter(const std::string & config_path);

  bool shoot(
    const io::Command & command, const auto_aim::Aimer & aimer,
    const std::list<auto_aim::Target> & targets, const Eigen::Vector3d & gimbal_pos,
    bool tracker_tracking);

private:
  io::Command last_command_;
  double judge_distance_;
  double first_tolerance_;
  double second_tolerance_;

  std::vector<double> shooter_judge_distance_;
  std::vector<double> shooter_tolerance_;
  bool is_multiple_tolerance_;

  double high_spin_force_fire_enter_speed_;
  double high_spin_force_fire_exit_speed_;
  bool auto_fire_;
  bool high_spin_force_fire_enabled_;
  bool high_spin_force_fire_active_;
};
}  // namespace auto_aim

#endif  // AUTO_AIM__SHOOTER_HPP
