#ifndef TOOLS__IMG_TOOLS_HPP
#define TOOLS__IMG_TOOLS_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace tools
{

inline void draw_point(
  cv::Mat & img, const cv::Point & point, const cv::Scalar & color = {0, 0, 255}, int radius = 3)
{
  cv::circle(img, point, radius, color, cv::FILLED);
}

// 把一条轮廓上的每个点画出来，便于观察 findContours 得到的边界点
inline void drawContour(
  cv::Mat & img, const std::vector<cv::Point> & contour, const cv::Scalar & color = {0, 0, 255},
  int radius = 1)
{
  for (const auto & point : contour) {
    draw_point(img, point, color, radius);
  }
}

inline void draw_points(
  cv::Mat & img, const std::vector<cv::Point> & points, const cv::Scalar & color = {0, 0, 255},
  int thickness = 2)
{
  std::vector<std::vector<cv::Point>> contours = {points};
  cv::drawContours(img, contours, -1, color, thickness);
}

inline void draw_points(
  cv::Mat & img, const std::vector<cv::Point2f> & points, const cv::Scalar & color = {0, 0, 255},
  int thickness = 2)
{
  std::vector<cv::Point> int_points(points.begin(), points.end());
  draw_points(img, int_points, color, thickness);
}

inline void draw_text(
  cv::Mat & img, const std::string & text, const cv::Point & point,
  const cv::Scalar & color = {0, 255, 255}, double font_scale = 1.0, int thickness = 2)
{
  cv::putText(img, text, point, cv::FONT_HERSHEY_SIMPLEX, font_scale, color, thickness);
}

}  // namespace tools

#endif  // TOOLS__IMG_TOOLS_HPP
