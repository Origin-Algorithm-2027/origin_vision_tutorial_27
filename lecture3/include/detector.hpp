#ifndef DETECTOR_HPP
#define DETECTOR_HPP

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>
#include "armor.hpp"
#include "img_tools.hpp"

// 几何阈值来自 sp_vision_25 传统检测配置（configs/example.yaml）
constexpr double kThreshold = 130;              // 二值化阈值
constexpr double kMaxAngleErrorDeg = 45;        // 灯条相对竖直的最大偏角
constexpr double kMinLightbarRatio = 1.5;       // 灯条长 / 宽
constexpr double kMaxLightbarRatio = 20;
constexpr double kMinLightbarLength = 15;       // 像素
constexpr double kMinArmorRatio = 1;            // 灯条间距 / 较长灯条
constexpr double kMaxArmorRatio = 5;
constexpr double kMaxSideRatio = 1.5;           // 两灯条长度比
constexpr double kMaxRectangularErrorDeg = 25;  // 两灯条与连线是否接近垂直

struct DetectResult
{
    cv::Mat binary_img;
    std::vector<Lightbar> lightbars;
    std::vector<Armor> armors;
};

inline bool check_lightbar(const Lightbar & lightbar)
{
    const bool angle_ok = lightbar.angle_error < kMaxAngleErrorDeg * CV_PI / 180.0;
    const bool ratio_ok =
        lightbar.ratio > kMinLightbarRatio && lightbar.ratio < kMaxLightbarRatio;
    const bool length_ok = lightbar.length > kMinLightbarLength;
    return angle_ok && ratio_ok && length_ok;
}

inline bool check_armor(const Armor & armor)
{
    const bool ratio_ok = armor.ratio > kMinArmorRatio && armor.ratio < kMaxArmorRatio;
    const bool side_ok = armor.side_ratio < kMaxSideRatio;
    const bool rect_ok = armor.rectangular_error < kMaxRectangularErrorDeg * CV_PI / 180.0;
    return ratio_ok && side_ok && rect_ok;
}

inline bool get_color(const cv::Mat & bgr_img, const std::vector<cv::Point> & contour, Color & color)
{
    int red_sum = 0;
    int blue_sum = 0;
    for (const auto & point : contour) {
        blue_sum += bgr_img.at<cv::Vec3b>(point)[0];
        red_sum += bgr_img.at<cv::Vec3b>(point)[2];
    }
    // 白色数字 / 反光的 B、R 接近，不能当灯条
    if (red_sum > blue_sum * 1.2) {
        color = Color::red;
        return true;
    }
    if (blue_sum > red_sum * 1.2) {
        color = Color::blue;
        return true;
    }
    return false;
}

inline void remove_duplicated(std::vector<Armor> & armors)
{
    for (std::size_t i = 0; i < armors.size(); ++i) {
        for (std::size_t j = i + 1; j < armors.size(); ++j) {
            const bool share_bar = armors[i].left.id == armors[j].left.id ||
                                   armors[i].left.id == armors[j].right.id ||
                                   armors[i].right.id == armors[j].left.id ||
                                   armors[i].right.id == armors[j].right.id;
            if (!share_bar) {
                continue;
            }
            const double err_i = std::abs(armors[i].ratio - 2.5);
            const double err_j = std::abs(armors[j].ratio - 2.5);
            if (err_i < err_j) {
                armors[j].duplicated = true;
            } else {
                armors[i].duplicated = true;
            }
        }
    }
    armors.erase(std::remove_if(armors.begin(), armors.end(),
                                [](const Armor & a) { return a.duplicated; }),
                 armors.end());
}

inline DetectResult detect_armor(const cv::Mat & bgr_img)
{
    DetectResult result;

    cv::Mat gray_img;
    cv::cvtColor(bgr_img, gray_img, cv::COLOR_BGR2GRAY);
    cv::threshold(gray_img, result.binary_img, kThreshold, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(result.binary_img, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    for (const auto & contour : contours) {
        Lightbar lightbar(cv::minAreaRect(contour), result.lightbars.size());
        if (!check_lightbar(lightbar)) {
            continue;
        }
        if (!get_color(bgr_img, contour, lightbar.color)) {
            continue;
        }
        result.lightbars.emplace_back(lightbar);
    }

    std::sort(result.lightbars.begin(), result.lightbars.end(),
              [](const Lightbar & a, const Lightbar & b) { return a.center.x < b.center.x; });

    for (std::size_t i = 0; i < result.lightbars.size(); ++i) {
        for (std::size_t j = i + 1; j < result.lightbars.size(); ++j) {
            if (result.lightbars[i].color != result.lightbars[j].color) {
                continue;
            }
            Armor armor(result.lightbars[i], result.lightbars[j]);
            if (check_armor(armor)) {
                result.armors.emplace_back(armor);
            }
        }
    }
    remove_duplicated(result.armors);
    return result;
}

inline void print_result(const std::string & img_path, const DetectResult & result)
{
    std::cout << "图片: " << img_path << "\n";
    std::cout << "灯条: " << result.lightbars.size() << "  装甲板: " << result.armors.size() << "\n";
    for (std::size_t i = 0; i < result.armors.size(); ++i) {
        const auto & armor = result.armors[i];
        std::cout << "  [" << i << "] " << color_name(armor.color)
                  << "  center=(" << armor.center.x << ", " << armor.center.y << ")"
                  << "  ratio=" << armor.ratio << "\n";
        std::cout << "      corners:";
        for (const auto & p : armor.points) {
            std::cout << " (" << p.x << ", " << p.y << ")";
        }
        std::cout << "\n";
    }
}

inline cv::Mat draw_result(const cv::Mat & bgr_img, const DetectResult & result)
{
    cv::Mat detection = bgr_img.clone();
    for (const auto & lightbar : result.lightbars) {
        const cv::Scalar color = (lightbar.color == Color::blue) ? cv::Scalar(255, 0, 0)
                                                                 : cv::Scalar(0, 0, 255);
        tools::draw_points(detection, lightbar.points, color, 3);
        tools::draw_text(detection, color_name(lightbar.color), lightbar.top, color, 0.5, 1);
    }
    for (std::size_t i = 0; i < result.armors.size(); ++i) {
        const auto & armor = result.armors[i];
        tools::draw_points(detection, armor.points, {0, 255, 0}, 2);
        const std::string info =
            std::string("armor") + std::to_string(i) + " " + color_name(armor.color);
        tools::draw_text(detection, info, armor.left.bottom, {0, 255, 0}, 0.6, 2);
    }
    return detection;
}

#endif  // DETECTOR_HPP
