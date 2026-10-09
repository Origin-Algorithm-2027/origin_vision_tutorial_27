#ifndef ARMOR_HPP
#define ARMOR_HPP

#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <vector>

enum class Color { red, blue };

inline const char * color_name(Color color)
{
    return color == Color::blue ? "blue" : "red";
}

struct Lightbar
{
    std::size_t id = 0;
    Color color = Color::red;
    cv::Point2f center, top, bottom, top2bottom;
    std::vector<cv::Point2f> points;
    double angle = 0, angle_error = 0, length = 0, width = 0, ratio = 0;

    Lightbar() = default;

    Lightbar(const cv::RotatedRect & rotated_rect, std::size_t lightbar_id) : id(lightbar_id)
    {
        std::vector<cv::Point2f> corners(4);
        rotated_rect.points(corners.data());
        std::sort(corners.begin(), corners.end(),
                  [](const cv::Point2f & a, const cv::Point2f & b) { return a.y < b.y; });

        center = rotated_rect.center;
        top = (corners[0] + corners[1]) / 2;
        bottom = (corners[2] + corners[3]) / 2;
        top2bottom = bottom - top;
        points = {top, bottom};

        width = cv::norm(corners[0] - corners[1]);
        angle = std::atan2(top2bottom.y, top2bottom.x);
        angle_error = std::abs(angle - CV_PI / 2);
        length = cv::norm(top2bottom);
        ratio = (width > 1e-3) ? (length / width) : 0;
    }
};

struct Armor
{
    Color color = Color::red;
    Lightbar left, right;
    cv::Point2f center;
    std::vector<cv::Point2f> points;
    double ratio = 0;              // 两灯条中点距离 / 较长灯条
    double side_ratio = 0;         // 长灯条 / 短灯条
    double rectangular_error = 0;  // 灯条与中点连线相对垂直的偏差
    bool duplicated = false;

    Armor(const Lightbar & left_bar, const Lightbar & right_bar)
        : color(left_bar.color), left(left_bar), right(right_bar)
    {
        center = (left.center + right.center) / 2;
        points = {left.top, right.top, right.bottom, left.bottom};

        const auto left2right = right.center - left.center;
        const double width = cv::norm(left2right);
        const double max_len = std::max(left.length, right.length);
        const double min_len = std::min(left.length, right.length);
        ratio = (max_len > 1e-3) ? (width / max_len) : 0;
        side_ratio = (min_len > 1e-3) ? (max_len / min_len) : 1e9;

        const double roll = std::atan2(left2right.y, left2right.x);
        const double left_err = std::abs(left.angle - roll - CV_PI / 2);
        const double right_err = std::abs(right.angle - roll - CV_PI / 2);
        rectangular_error = std::max(left_err, right_err);
    }
};

#endif  // ARMOR_HPP
