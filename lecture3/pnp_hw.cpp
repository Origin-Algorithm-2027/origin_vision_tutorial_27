#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include "detector.hpp"

// PnP 作业：识别、内参、尺寸已写好（对照 Class3.md 第七节）
// 按 Task 01～03 填写 object_points、img_points、solvePnP
// 在 class_3/ 下运行: build/pnp_hw
// 对照完整示例:     build/pnp

constexpr double LIGHTBAR_LENGTH = 0.056;  // m
constexpr double ARMOR_WIDTH = 0.135;      // 小装甲；大装甲改成 0.230

// 旋转矩阵 → yaw / pitch / roll（度），ZYX；
void rmat_to_ypr(const cv::Mat & R, double & yaw_deg, double & pitch_deg, double & roll_deg)
{
    const double r00 = R.at<double>(0, 0);
    const double r10 = R.at<double>(1, 0);
    const double r20 = R.at<double>(2, 0);
    const double r21 = R.at<double>(2, 1);
    const double r22 = R.at<double>(2, 2);
    const double sy = std::sqrt(r00 * r00 + r10 * r10);
    pitch_deg = std::atan2(-r20, sy) * 180.0 / CV_PI;
    yaw_deg = std::atan2(r10, r00) * 180.0 / CV_PI;
    roll_deg = std::atan2(r21, r22) * 180.0 / CV_PI;
}

int main(int argc, char ** argv)
{
    const std::string img_path = (argc > 1) ? argv[1] : "imgs/armor2.jpg";
    cv::Mat bgr_img = cv::imread(img_path);
    if (bgr_img.empty()) {
        std::cerr << "无法读取图片: " << img_path << "\n"
                  << "请在 class_3/ 目录下运行，例如: build/pnp_hw imgs/red_3.jpg\n";
        return 1;
    }

    const DetectResult result = detect_armor(bgr_img);
    print_result(img_path, result);

    // 内参已给好
    const cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 1785.49, 0, 672.48,
                                                             0, 1785.03, 559.90,
                                                             0, 0, 1);  //内参矩阵
    const cv::Mat dist_coeffs = (cv::Mat_<double>(5, 1) << -0.076, 0.112, 0.0005, -0.0028, 0); //畸变系数

    // #### Task 01 ################################################
    // 填写 object_points（物体坐标系，装甲板中心为原点，Z = 0）
    // 顺序：左上 → 右上 → 右下 → 左下，单位米
    // 用 ARMOR_WIDTH、LIGHTBAR_LENGTH
    static const std::vector<cv::Point3f> object_points{
        // {                ,                     , 0}, 
        // {                ,                     , 0}, 
        // {                ,                     , 0}, 
        // {                ,                     , 0}
    };

    cv::Mat detection = draw_result(bgr_img, result);

    for (std::size_t i = 0; i < result.armors.size(); ++i) {
        const auto & armor = result.armors[i];

        // #### Task 02 ################################################
        // img_points 是照片上装甲板 4 个点的像素坐标
        // 用 armor 的灯条端点，顺序和 Task 01 一致
        std::vector<cv::Point2f> img_points{
            // 与 Task 01 同一顺序
        
        
        
        };
        // #### Task 03 ################################################
        cv::Mat rvec, tvec;
        // 调用 solvePnP，参数：3D 点、像素点、内参、畸变、rvec、tvec
        //cv::solvePnP(     ,       ,       ,       ,       ,       ,)




        if (rvec.empty() || tvec.empty()) {
            std::cout << "  [" << i << "] 还没调用 solvePnP，先填完 Task 01～03。\n";
            continue;
        }

        const double dist_m = cv::norm(tvec);
        const double tx = tvec.at<double>(0);
        const double ty = tvec.at<double>(1);
        const double tz = tvec.at<double>(2);
        const double rx = rvec.at<double>(0);
        const double ry = rvec.at<double>(1);
        const double rz = rvec.at<double>(2);

        cv::Mat R;
        cv::Rodrigues(rvec, R);
        double yaw_deg = 0, pitch_deg = 0, roll_deg = 0;
        rmat_to_ypr(R, yaw_deg, pitch_deg, roll_deg);

        std::cout << "  [" << i << "] dist=" << dist_m << " m\n"
                  << "      tvec=(" << tx << ", " << ty << ", " << tz << ")\n"
                  << "      rvec=(" << rx << ", " << ry << ", " << rz << ")\n"
                  << "      yaw=" << yaw_deg << "  pitch=" << pitch_deg
                  << "  roll=" << roll_deg << " deg\n";

        std::vector<cv::Point3f> axis = {{0, 0, 0}, {0.05, 0, 0}, {0, 0.05, 0}, {0, 0, 0.05}};
        std::vector<cv::Point2f> proj;
        cv::projectPoints(axis, rvec, tvec, camera_matrix, dist_coeffs, proj);
        cv::line(detection, proj[0], proj[1], {0, 0, 255}, 2);
        cv::line(detection, proj[0], proj[2], {0, 255, 0}, 2);
        cv::line(detection, proj[0], proj[3], {255, 0, 0}, 2);
    }

    if (result.armors.empty()) {
        std::cout << "没有装甲板，跳过 PnP。\n";
    }

    if (std::getenv("DISPLAY") == nullptr) {
        std::cout << "未检测到 DISPLAY，跳过窗口显示。\n";
        return 0;
    }

    cv::Mat binary_show, detection_show;
    cv::resize(result.binary_img, binary_show, {}, 0.5, 0.5);
    cv::resize(detection, detection_show, {}, 0.5, 0.5);
    cv::imshow("binary", binary_show);
    cv::imshow("pnp", detection_show);
    cv::waitKey(0);
    return 0;
}
