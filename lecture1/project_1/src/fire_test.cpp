#include <fmt/core.h>

#include <Eigen/Core>
#include <string>

#include "io/command.hpp"
#include "tasks/auto_aim/shooter.hpp"

int main()
{
  // 创建 Shooter 对象，构造时从 yaml 文件读取参数
  std::string config_path = "configs/shooter.yaml";
  auto_aim::Shooter shooter(config_path);

  // 模拟自瞄程序连续运行的三帧
  // 第 1 帧：指令刚出现（yaw 突变），云台还没跟上，不应开火
  // 第 2 帧：指令稳定，云台已跟上上一帧指令，应该开火
  // 第 3 帧：持续稳定跟踪，继续开火
  struct Frame
  {
    io::Command command;
    double distance;
    Eigen::Vector3d gimbal_pos;
  };

  const Frame frames[3] = {
    {{true, false, 1.0, 0.1}, 3.0, {0.0, 0.0, 0.0}},
    {{true, false, 1.0, 0.1}, 3.0, {1.0, 0.1, 0.0}},
    {{true, false, 1.0, 0.1}, 3.0, {1.0, 0.1, 0.0}},
  };

  for (int i = 0; i < 3; i++) {
    const bool fire = shooter.shoot(frames[i].command, frames[i].gimbal_pos, frames[i].distance);
    fmt::print("第 {} 帧: {}\n", i + 1, fire ? "开火" : "不开火");
  }

  return 0;
}
