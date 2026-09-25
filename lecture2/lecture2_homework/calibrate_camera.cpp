#include <fmt/core.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>
#include <opencv2/opencv.hpp>

const std::string keys =
    "{help h usage ? |                                  | 输出命令行参数说明}"
    "{config-path c  | configs/calibration.yaml | yaml配置文件路径 }"
    "{@input-folder  | assets/chessboard_calib          | 输入文件夹路径   }";

namespace {

std::vector<cv::Point3f> corners_in_world(
    const cv::Size & pattern_size, const float square_size) {
    std::vector<cv::Point3f> corners_3d;

    for (int i = 0; i < pattern_size.height; i++) {
        for (int j = 0; j < pattern_size.width; j++) {
            corners_3d.push_back({j * square_size, i * square_size, 0});
        }
    }

    return corners_3d;
}

void load(const std::string & input_folder, const std::string & config_path, cv::Size & img_size,
    std::vector<std::vector<cv::Point3f>> & obj_points,
    std::vector<std::vector<cv::Point2f>> & img_points){

    auto yaml = YAML::LoadFile(config_path);
    const int pattern_cols = yaml["pattern_cols"].as<int>();
    const int pattern_rows = yaml["pattern_rows"].as<int>();
    const double square_size_mm = yaml["square_size_mm"].as<double>();
    const double preview_scale = yaml["preview_scale"].as<double>(0.5);
    cv::Size pattern_size(pattern_cols, pattern_rows);
    if (preview_scale > 1.0 || preview_scale <= 0.0) {
        fmt::print("preview_scale 必须在 (0, 1]，当前值为 {}", preview_scale);
        exit(1);
    }

    fmt::print("标定板参数: {}x{} 内角点, 方格大小 {}mm\n", pattern_cols, pattern_rows, square_size_mm);
    fmt::print("开始加载标定数据...\n\n");

    int success_count = 0;
    for (int i = 1; true; i++) {
        std::vector<cv::Point2f> corners;
        auto img_path = fmt::format("{}/{}.jpg", input_folder, i);
        auto img = cv::imread(img_path);
        if (img.empty()) break;
        img_size = img.size();

        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        cv::Mat compressed_gray;
        cv::resize(gray, compressed_gray, cv::Size(), preview_scale, preview_scale, cv::INTER_AREA);

        const int flags =
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
        std::vector<cv::Point2f> search_corners;
        bool found = cv::findChessboardCorners(compressed_gray, pattern_size, search_corners, flags);


        if (found) {
            corners = search_corners;
            for (auto & corner : corners) {
                corner.x /= preview_scale;
                corner.y /= preview_scale;
            }
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 30, 0.01));
        }

        auto preview = img.clone();
        cv::drawChessboardCorners(preview, pattern_size, corners, found);
        cv::resize(preview, preview, cv::Size(), preview_scale, preview_scale);
        cv::imshow("Press any key to continue", preview);
        cv::waitKey(0);
        fmt::print(
            "[{}] {} - {}\n", found ? "success" : "failure", img_path,
            found ? "已添加到标定数据" : "跳过");
        if (!found) continue;
        success_count++;
        img_points.emplace_back(corners);
        obj_points.emplace_back(corners_in_world(pattern_size, square_size_mm));
    }
    fmt::print("\n成功加载 {} 张有效图像\n", success_count);
    if (success_count < 10) {
        fmt::print("警告: 图像数量较少（建议至少15张），可能影响标定精度\n");
    }
}

void print_result(const cv::Mat & camera_matrix, const cv::Mat & distort_coeffs, double error){
    YAML::Emitter result;
    std::vector<double> camera_matrix_data(
        camera_matrix.begin<double>(), camera_matrix.end<double>());
    std::vector<double> distort_coeffs_data(
        distort_coeffs.begin<double>(), distort_coeffs.end<double>());

    result << YAML::BeginMap;
    result << YAML::Comment(fmt::format("重投影误差: {:.4f}px", error));
    result << YAML::Key << "camera_matrix";
    result << YAML::Value << YAML::Flow << camera_matrix_data;
    result << YAML::Key << "distort_coeffs";
    result << YAML::Value << YAML::Flow << distort_coeffs_data;
    result << YAML::Newline;
    result << YAML::EndMap;

    fmt::print("\n==================== 相机内参标定结果 ====================\n");
    fmt::print("{}\n", result.c_str());
    fmt::print("=========================================================\n\n");
    fmt::print("请将上述结果复制到 configs/calibration.yaml 中\n");
    fmt::print("然后进行手眼标定（运行 calibrate_chessboard_handeye）\n");
}

} // namespace

int main(int argc,char* argv[]) {
    cv::CommandLineParser cli(argc, argv, keys);
    if (cli.has("help")) {
        cli.printMessage();
        return 0;
    }
    auto input_folder = cli.get<std::string>(0);
    auto config_path = cli.get<std::string>("config-path");
    
    fmt::print("\n=== 棋盘格相机内参标定程序 ===\n\n");
    cv::Size img_size;
    std::vector<std::vector<cv::Point3f>> obj_points;
    std::vector<std::vector<cv::Point2f>> img_points;
    load(input_folder, config_path, img_size, obj_points, img_points);

    if (obj_points.empty()) {
        fmt::print("错误: 没有有效的标定数据\n");
        exit(1);
    }

    fmt::print("\n开始相机标定计算...\n");

    cv::Mat camera_matrix, distort_coeffs;
    std::vector<cv::Mat> rvecs, tvecs;
    auto criteria =
        cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, DBL_EPSILON);
    cv::calibrateCamera(
        obj_points, img_points, img_size, camera_matrix, distort_coeffs, rvecs, tvecs,
        cv::CALIB_FIX_K3, criteria);

    fmt::print("标定计算完成！\n");

    double error_sum = 0;
    size_t total_points = 0;
    for (size_t i = 0; i < obj_points.size(); i++) {
        std::vector<cv::Point2f> reprojected_points;
        cv::projectPoints(
        obj_points[i], rvecs[i], tvecs[i], camera_matrix, distort_coeffs, reprojected_points);

        total_points += reprojected_points.size();
        for (size_t j = 0; j < reprojected_points.size(); j++)
            error_sum += cv::norm(img_points[i][j] - reprojected_points[j]);
    }
    auto error = error_sum / total_points;

    print_result(camera_matrix, distort_coeffs, error);
    fmt::print("提示: 重投影误差 < 0.5px 为优秀，< 1.0px 为良好\n");
    fmt::print("      如果误差较大，建议重新采集数据或增加图像数量\n");

    return 0;
}