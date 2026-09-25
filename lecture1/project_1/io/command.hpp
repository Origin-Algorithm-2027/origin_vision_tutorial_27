#ifndef IO__COMMAND_HPP
#define IO__COMMAND_HPP

namespace io
{
struct Command
{
  bool control;   // 是否瞄准到目标
  bool shoot;     // 是否开火
  double yaw; 
  double pitch;
  double horizon_distance = 0;  //无人机专有
};

}  // namespace io

#endif  // IO__COMMAND_HPP