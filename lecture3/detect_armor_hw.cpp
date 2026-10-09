#include <cstdlib>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>
#include "detector.hpp"

// 装甲板识别作业：灰度 → 二值 → 轮廓已写好（main_hw 练过）
// 按 Task 填写几何过滤、灯条配对
// 在 class_3/ 下运行: build/detect_armor_hw
// 对照完整示例:     build/detect_armor

int main(int argc, char ** argv)
{
    const std::string img_path = (argc > 1) ? argv[1] : "imgs/blue_4.jpg";
    cv::Mat bgr_img = cv::imread(img_path);
    if (bgr_img.empty()) {
        std::cerr << "无法读取图片: " << img_path << "\n"
                  << "请在 class_3/ 目录下运行，例如: build/detect_armor_hw imgs/red_2.jpg\n";
        return 1;
    }

    DetectResult result;

    // 前面几步已写好：灰度 → 二值 → 轮廓
    cv::Mat gray_img;
    cv::cvtColor(bgr_img, gray_img, cv::COLOR_BGR2GRAY);
    cv::threshold(gray_img, result.binary_img, kThreshold, 255, cv::THRESH_BINARY);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(result.binary_img, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    // ---------- 几何过滤：单看这一根，像不像灯条 ----------
    for (const auto & contour : contours) {
        Lightbar lightbar(cv::minAreaRect(contour), result.lightbars.size());

        // Task1: 写出三个 bool：
        //   angle_ok：lightbar.angle_error < kMaxAngleErrorDeg * CV_PI / 180.0
        //   ratio_ok：ratio 在 kMinLightbarRatio 和 kMaxLightbarRatio 之间
        //   length_ok：lightbar.length > kMinLightbarLength




        // Task2: 三项不都成立则 continue




        // Task3: 调用 get_color(bgr_img, contour, lightbar.color)，返回 false 则 continue




        result.lightbars.emplace_back(lightbar);
    }

    // 按灯条中心 x 从左到右排好，后面配对时 i 一定在 j 左边
    std::sort(result.lightbars.begin(), result.lightbars.end(),
              [](const Lightbar & a, const Lightbar & b) { return a.center.x < b.center.x; });

    // ---------- 灯条配对：哪两根同色灯条构成一块板 ----------
    for (std::size_t i = 0; i < result.lightbars.size(); ++i) {
        for (std::size_t j = i + 1; j < result.lightbars.size(); ++j) {

            // Task4: 两根灯条颜色不同则 continue




            // Task5: 用 result.lightbars[i] 和 result.lightbars[j] 构造 Armor armor




            // Task6: 写出三个 bool，三项都成立才 result.armors.emplace_back(armor)
            //   ratio_ok：armor.ratio 在 kMinArmorRatio 和 kMaxArmorRatio 之间
            //   side_ok：armor.side_ratio < kMaxSideRatio
            //   rect_ok：armor.rectangular_error < kMaxRectangularErrorDeg * CV_PI / 180.0




        }
    }

    remove_duplicated(result.armors);

    print_result(img_path, result);

    if (std::getenv("DISPLAY") == nullptr) {
        std::cout << "未检测到 DISPLAY，跳过窗口显示。\n";
        return 0;
    }

    cv::Mat detection = draw_result(bgr_img, result);
    cv::Mat binary_show, detection_show;
    cv::resize(result.binary_img, binary_show, {}, 0.5, 0.5);
    cv::resize(detection, detection_show, {}, 0.5, 0.5);
    cv::imshow("binary", binary_show);
    cv::imshow("detection", detection_show);
    cv::waitKey(0);
    return 0;
}
