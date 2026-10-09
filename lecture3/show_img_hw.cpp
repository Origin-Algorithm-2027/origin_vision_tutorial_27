#include <opencv2/opencv.hpp>

int main()
{
    cv::Mat bgr_img;
    bgr_img = cv::imread("imgs/red_3.jpg");

    // 分离三个通道并显示，调试用
    std::vector<cv::Mat> channels;
    












    
    cv::waitKey(0);
    return 0;
}
