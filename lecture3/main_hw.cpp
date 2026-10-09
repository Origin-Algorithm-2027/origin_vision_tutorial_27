#include <opencv2/opencv.hpp>
#include <vector>
#include "img_tools.hpp"

int main()
{
    // 读取图片
    cv::Mat bgr_img;
    bgr_img = cv::imread("imgs/red_3.jpg");

    // Task1: 用 cv::cvtColor 把 bgr_img 转成灰度图 gray_img, 
    // 转换码是 cv::COLOR_BGR2GRAY，并显示灰度图，窗口名是 "gray"，窗口大小为 0.5x0.5
    cv::Mat gray_img;



    // Task2: 用 cv::threshold 把 gray_img 二值化到 binary_img, 
    // 阈值 130，最大值 255，类型 cv::THRESH_BINARY，并显示二值图，窗口名是 "binary"，窗口大小为 0.5x0.5
    cv::Mat binary_img;



    // Task3: 用 cv::findContours 从 binary_img 提取轮廓到 contours, 
    // 模式 cv::RETR_EXTERNAL，方法 cv::CHAIN_APPROX_NONE
    std::vector<std::vector<cv::Point>> contours;



    // Task4: 用 cv::drawContours 把 contours 画到 drawcontours 上, 
    // 颜色可用 {0, 0, 255}，线宽 5，并显示轮廓图，窗口名是 "drawcontours"，窗口大小为 0.5x0.5
    cv::Mat drawcontours = bgr_img.clone();
    for (const auto & contour : contours) {
        tools::drawContour(drawcontours, contour);
    }



    // Task5: 用 cv::minAreaRect 拟合当前 contour，结果放进 rotated_rects
    std::vector<cv::RotatedRect> rotated_rects;
    for (const auto & contour : contours) {

    }

    // Task6: 用 rotated_rect.points(...) 取出 4 个角点，
    // 并显示旋转矩形图，窗口名是 "drawrect"，窗口大小为 0.5x0.5
    cv::Mat drawrect = bgr_img.clone();
    for (const auto & rotated_rect : rotated_rects) {
        std::vector<cv::Point2f> points(4);

        tools::draw_points(drawrect, points);
    }

    cv::waitKey(0);
    return 0;
}
