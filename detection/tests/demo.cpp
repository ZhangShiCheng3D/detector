/**
 * @file demo.cpp
 * @brief 瑕疵检测系统演示程序 - 二值图像优化版
 *
 * 使用二值图像优化检测模式，对测试图像进行瑕疵检测。
 * 专门针对黑白二值图像优化，显著降低误报率。
 *
 * 功能特点：
 * - 启用二值图像优化模式（边缘容错、形态学噪声过滤）
 * - 支持配置文件和命令行参数
 * - 支持手动ROI选择模式
 * - 实时可视化输出
 */

#include "defect_detector.h"
#include "visualizer.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <random>
#include <chrono>
#include <cmath>
#include <fstream>

namespace fs = std::filesystem;

// ========== 手动ROI选择相关结构和全局变量 ==========
struct ROISelectionState {
    cv::Mat template_img;           // 原始模板图像
    cv::Mat display_img;            // 用于显示的图像副本
    std::vector<cv::Rect> rois;     // 已选择的ROI列表
    std::vector<float> thresholds;  // 每个ROI的阈值
    cv::Point start_point;          // 当前矩形起始点
    cv::Point current_point;        // 当前鼠标位置
    bool is_drawing;                // 是否正在绘制
    int target_count;               // 需要选择的ROI数量
    bool selection_done;            // 选择是否完成
    std::vector<cv::Scalar> colors; // ROI颜色列表
};

// 鼠标回调函数
void onMouseROISelection(int event, int x, int y, int flags, void* userdata) {
    ROISelectionState* state = static_cast<ROISelectionState*>(userdata);

    if (state->selection_done || (int)state->rois.size() >= state->target_count) {
        return;
    }

    switch (event) {
        case cv::EVENT_LBUTTONDOWN:
            state->start_point = cv::Point(x, y);
            state->current_point = cv::Point(x, y);
            state->is_drawing = true;
            break;

        case cv::EVENT_MOUSEMOVE:
            if (state->is_drawing) {
                state->current_point = cv::Point(x, y);
            }
            break;

        case cv::EVENT_LBUTTONUP:
            if (state->is_drawing) {
                state->current_point = cv::Point(x, y);
                state->is_drawing = false;

                // 计算矩形区域
                int rx = std::min(state->start_point.x, state->current_point.x);
                int ry = std::min(state->start_point.y, state->current_point.y);
                int rw = std::abs(state->current_point.x - state->start_point.x);
                int rh = std::abs(state->current_point.y - state->start_point.y);

                // 检查有效性（最小尺寸10x10）
                if (rw >= 10 && rh >= 10) {
                    // 确保在图像边界内
                    rx = std::max(0, rx);
                    ry = std::max(0, ry);
                    rw = std::min(rw, state->template_img.cols - rx);
                    rh = std::min(rh, state->template_img.rows - ry);

                    state->rois.push_back(cv::Rect(rx, ry, rw, rh));
                    state->thresholds.push_back(0.50f);  // 默认阈值

                    std::cout << "  ROI " << state->rois.size() << " selected: ("
                              << rx << ", " << ry << ") " << rw << "x" << rh << "\n";
                }
            }
            break;
    }
}

// 绘制ROI选择界面
void drawROISelectionUI(ROISelectionState& state) {
    state.display_img = state.template_img.clone();

    // 绘制已选择的ROI
    for (size_t i = 0; i < state.rois.size(); ++i) {
        cv::Scalar color = state.colors[i % state.colors.size()];
        cv::rectangle(state.display_img, state.rois[i], color, 2);

        // 绘制ROI编号标签
        std::string label = "ROI " + std::to_string(i + 1);
        int baseline;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::Point label_pos(state.rois[i].x, state.rois[i].y - 5);
        if (label_pos.y < text_size.height + 5) {
            label_pos.y = state.rois[i].y + state.rois[i].height + text_size.height + 5;
        }
        cv::putText(state.display_img, label, label_pos,
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
    }

    // 绘制正在绘制的矩形
    if (state.is_drawing) {
        cv::Rect current_rect(
            std::min(state.start_point.x, state.current_point.x),
            std::min(state.start_point.y, state.current_point.y),
            std::abs(state.current_point.x - state.start_point.x),
            std::abs(state.current_point.y - state.start_point.y)
        );
        cv::rectangle(state.display_img, current_rect, cv::Scalar(0, 255, 255), 2);

        // 显示尺寸信息
        std::string size_info = std::to_string(current_rect.width) + "x" + std::to_string(current_rect.height);
        cv::putText(state.display_img, size_info,
                    cv::Point(current_rect.x + 5, current_rect.y + current_rect.height - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
    }

    // 绘制顶部指示栏
    cv::rectangle(state.display_img, cv::Rect(0, 0, state.display_img.cols, 40),
                  cv::Scalar(50, 50, 50), -1);

    std::string instruction;
    if ((int)state.rois.size() < state.target_count) {
        instruction = "Select ROI " + std::to_string(state.rois.size() + 1) + "/" +
                      std::to_string(state.target_count) +
                      " - Click and drag | ENTER: finish | ESC: cancel | R: reset";
    } else {
        instruction = "All ROIs selected! Press ENTER to continue or R to reset";
    }
    cv::putText(state.display_img, instruction, cv::Point(10, 28),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
}

// 保存ROI配置到JSON文件
bool saveROIConfig(const std::string& filename, const std::vector<cv::Rect>& rois,
                   const std::vector<float>& thresholds) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << filename << "\n";
        return false;
    }

    file << "{\n";
    file << "  \"roi_count\": " << rois.size() << ",\n";
    file << "  \"rois\": [\n";
    for (size_t i = 0; i < rois.size(); ++i) {
        file << "    {\n";
        file << "      \"x\": " << rois[i].x << ",\n";
        file << "      \"y\": " << rois[i].y << ",\n";
        file << "      \"width\": " << rois[i].width << ",\n";
        file << "      \"height\": " << rois[i].height << ",\n";
        file << "      \"threshold\": " << std::fixed << std::setprecision(2) << thresholds[i] << "\n";
        file << "    }" << (i < rois.size() - 1 ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    file.close();
    std::cout << "ROI configuration saved to: " << filename << "\n";
    return true;
}

// 从JSON文件加载ROI配置（简单解析）
bool loadROIConfig(const std::string& filename, std::vector<cv::Rect>& rois,
                   std::vector<float>& thresholds) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file for reading: " << filename << "\n";
        return false;
    }

    rois.clear();
    thresholds.clear();

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // 简单的JSON解析（查找数值）
    size_t pos = 0;
    while ((pos = content.find("\"x\":", pos)) != std::string::npos) {
        int x = 0, y = 0, w = 0, h = 0;
        float threshold = 0.5f;

        // 解析 x
        pos = content.find(":", pos) + 1;
        x = std::stoi(content.substr(pos));

        // 解析 y
        pos = content.find("\"y\":", pos);
        pos = content.find(":", pos) + 1;
        y = std::stoi(content.substr(pos));

        // 解析 width
        pos = content.find("\"width\":", pos);
        pos = content.find(":", pos) + 1;
        w = std::stoi(content.substr(pos));

        // 解析 height
        pos = content.find("\"height\":", pos);
        pos = content.find(":", pos) + 1;
        h = std::stoi(content.substr(pos));

        // 解析 threshold
        pos = content.find("\"threshold\":", pos);
        pos = content.find(":", pos) + 1;
        threshold = std::stof(content.substr(pos));

        rois.push_back(cv::Rect(x, y, w, h));
        thresholds.push_back(threshold);
    }

    std::cout << "Loaded " << rois.size() << " ROIs from: " << filename << "\n";
    return !rois.empty();
}

// 执行手动ROI选择
bool performManualROISelection(const cv::Mat& templ, int target_count,
                               std::vector<cv::Rect>& out_rois,
                               std::vector<float>& out_thresholds) {
    ROISelectionState state;
    state.template_img = templ.clone();
    state.target_count = target_count;
    state.is_drawing = false;
    state.selection_done = false;

    // 初始化颜色列表
    state.colors = {
        cv::Scalar(0, 255, 0),    // 绿色
        cv::Scalar(255, 0, 0),    // 蓝色
        cv::Scalar(0, 0, 255),    // 红色
        cv::Scalar(255, 255, 0),  // 青色
        cv::Scalar(255, 0, 255),  // 品红
        cv::Scalar(0, 255, 255),  // 黄色
        cv::Scalar(128, 0, 255),  // 紫色
        cv::Scalar(0, 128, 255)   // 橙色
    };

    // 创建窗口
    std::string window_name = "Manual ROI Selection - Template Image";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL);
    cv::resizeWindow(window_name, 1200, 900);
    cv::setMouseCallback(window_name, onMouseROISelection, &state);

    std::cout << "\n========================================\n";
    std::cout << "       Manual ROI Selection Mode\n";
    std::cout << "========================================\n";
    std::cout << "Instructions:\n";
    std::cout << "  - Click and drag to draw ROI rectangle\n";
    std::cout << "  - Press R to reset all selections\n";
    std::cout << "  - Press ENTER to finish and continue\n";
    std::cout << "  - Press ESC to cancel\n";
    std::cout << "----------------------------------------\n\n";

    while (!state.selection_done) {
        drawROISelectionUI(state);
        cv::imshow(window_name, state.display_img);

        int key = cv::waitKey(30);

        if (key == 27) {  // ESC - 取消
            std::cout << "\nROI selection cancelled.\n";
            cv::destroyWindow(window_name);
            return false;
        } else if (key == 13 || key == 10) {  // Enter - 完成
            if (!state.rois.empty()) {
                state.selection_done = true;
            } else {
                std::cout << "  [Warning] Please select at least one ROI before continuing.\n";
            }
        } else if (key == 'r' || key == 'R') {  // R - 重置
            state.rois.clear();
            state.thresholds.clear();
            std::cout << "  [Reset] All ROI selections cleared.\n";
        }
    }

    cv::destroyWindow(window_name);

    out_rois = state.rois;
    out_thresholds = state.thresholds;

    std::cout << "\n  Selection complete! " << out_rois.size() << " ROIs selected.\n\n";
    return true;
}

using namespace defect_detection;

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] [data_dir]\n";
    std::cout << "\nDefault data directory: data/demo\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --config <file>    Load visualization config from file\n";
    std::cout << "  --no-viz           Disable visualization\n";
    std::cout << "  --viz-file         Output visualization to files only\n";
    std::cout << "  --viz-display      Output visualization to display only\n";
    std::cout << "  --output <dir>     Set output directory\n";
    std::cout << "  --delay <ms>       Set display delay between images (default: 1500ms)\n";
    std::cout << "  --seed <num>       Set random seed for ROI placement (default: time-based)\n";
    std::cout << "  --roi-count <num>  Set number of ROI regions (default: 4)\n";
    std::cout << "  --no-align         Disable auto alignment (not recommended)\n";
    std::cout << "  --align-full       Use full image alignment mode (slower)\n";
    std::cout << "  --align-roi        Use ROI-only alignment mode (faster, default)\n";
    std::cout << "  --verbose          Show detailed alignment info for each image\n";
    std::cout << "\nBinary Image Optimization:\n";
    std::cout << "  --no-binary-opt    Disable binary image optimization (not recommended for B&W)\n";
    std::cout << "  --edge-tol <px>    Edge tolerance pixels (default: 2)\n";
    std::cout << "  --noise-size <px>  Noise filter size (default: 2)\n";
    std::cout << "  --min-area <px>    Minimum significant area (default: 20)\n";
    std::cout << "  --sim-thresh <f>   Overall similarity threshold (default: 0.95)\n";
    std::cout << "\nManual ROI Selection:\n";
    std::cout << "  --manual-roi       Enable interactive manual ROI selection mode\n";
    std::cout << "  --load-roi <file>  Load ROI configuration from JSON file\n";
    std::cout << "  --save-roi <file>  Save selected ROI configuration to JSON file\n";
    std::cout << "\n  --help             Show this help message\n";
}

int main(int argc, char** argv) {
    // ========== 默认配置 ==========
    std::string data_dir = "data/demo";  // 默认数据目录改为 data/demo
    std::string output_dir = "output";
    std::string config_file = "config/visualization_config.json";
    bool viz_enabled = true;
    bool auto_align_enabled = true;  // 默认启用自动对齐
    AlignmentMode align_mode = AlignmentMode::ROI_ONLY;  // 默认ROI区域校正模式
    OutputMode viz_mode = OutputMode::BOTH;  // Default to both display and file
    int display_delay_ms = 1500;  // Delay between images for real-time viewing (1.5 seconds)
    // Use time-based seed by default for truly random ROI placement each run
    unsigned int random_seed = static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    bool seed_specified = false;  // Track if user specified a seed
    int roi_count = 4;  // Number of ROI regions
    bool verbose_mode = false;  // Show detailed alignment info

    // ========== 二值图像优化参数（默认启用）==========
    bool binary_optimization_enabled = true;  // 默认启用二值图像优化
    BinaryDetectionParams binary_params;
    binary_params.enabled = true;
    binary_params.auto_detect_binary = true;
    binary_params.noise_filter_size = 10;
    binary_params.edge_tolerance_pixels = 4;
    binary_params.edge_diff_ignore_ratio = 0.05f;
    binary_params.min_significant_area = 20;
    binary_params.area_diff_threshold = 0.001f;
    binary_params.overall_similarity_threshold = 0.95f;

    // Manual ROI selection options
    bool manual_roi_mode = false;
    std::string load_roi_file = "";
    std::string save_roi_file = "";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--no-viz") {
            viz_enabled = false;
        } else if (arg == "--viz-file") {
            viz_mode = OutputMode::FILE_ONLY;
        } else if (arg == "--viz-display") {
            viz_mode = OutputMode::DISPLAY_ONLY;
        } else if (arg == "--output" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--delay" && i + 1 < argc) {
            display_delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            random_seed = static_cast<unsigned int>(std::stoul(argv[++i]));
            seed_specified = true;
        } else if (arg == "--roi-count" && i + 1 < argc) {
            roi_count = std::stoi(argv[++i]);
            if (roi_count < 1) roi_count = 1;
            if (roi_count > 20) roi_count = 20;
        } else if (arg == "--no-align") {
            auto_align_enabled = false;
            align_mode = AlignmentMode::NONE;
        } else if (arg == "--align-full") {
            align_mode = AlignmentMode::FULL_IMAGE;
        } else if (arg == "--align-roi") {
            align_mode = AlignmentMode::ROI_ONLY;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose_mode = true;
        // 二值图像优化参数
        } else if (arg == "--no-binary-opt") {
            binary_optimization_enabled = false;
            binary_params.enabled = false;
        } else if (arg == "--edge-tol" && i + 1 < argc) {
            binary_params.edge_tolerance_pixels = std::stoi(argv[++i]);
        } else if (arg == "--noise-size" && i + 1 < argc) {
            binary_params.noise_filter_size = std::stoi(argv[++i]);
        } else if (arg == "--min-area" && i + 1 < argc) {
            binary_params.min_significant_area = std::stoi(argv[++i]);
        } else if (arg == "--sim-thresh" && i + 1 < argc) {
            binary_params.overall_similarity_threshold = std::stof(argv[++i]);
        } else if (arg == "--manual-roi") {
            manual_roi_mode = true;
        } else if (arg == "--load-roi" && i + 1 < argc) {
            load_roi_file = argv[++i];
        } else if (arg == "--save-roi" && i + 1 < argc) {
            save_roi_file = argv[++i];
        } else if (arg[0] != '-') {
            data_dir = arg;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "   喷印瑕疵检测系统 - 演示程序\n";
    std::cout << "   Print Defect Detection System Demo\n";
    std::cout << "========================================\n\n";

    // Initialize visualizer with configuration
    Visualizer visualizer;
    if (fs::exists(config_file)) {
        if (visualizer.loadConfig(config_file)) {
            std::cout << "Loaded visualization config from: " << config_file << "\n";
        } else {
            std::cout << "Warning: Failed to load config, using defaults\n";
        }
    }

    // Apply command-line overrides
    VisualizationConfig viz_config = visualizer.getConfig();
    viz_config.enabled = viz_enabled;
    viz_config.output_mode = viz_mode;
    viz_config.output_directory = output_dir;
    viz_config.display_options.wait_key_ms = display_delay_ms;
    viz_config.display_options.window_name = "Print Defect Detection - Real-time Results";
    visualizer.setConfig(viz_config);

    std::cout << "Visualization: " << (viz_enabled ? "enabled" : "disabled") << "\n";
    std::cout << "Output mode: " << (viz_mode == OutputMode::FILE_ONLY ? "file" :
                                     viz_mode == OutputMode::DISPLAY_ONLY ? "display" : "both") << "\n";
    if (viz_mode != OutputMode::FILE_ONLY) {
        std::cout << "Display delay: " << display_delay_ms << " ms (press any key to advance, ESC to quit)\n";
    }
    std::cout << "\n";

    // 创建输出目录
    if (!fs::exists(output_dir)) {
        fs::create_directory(output_dir);
    }

    // 获取所有BMP文件
    std::vector<std::string> all_bmp_files;
    for (const auto& entry : fs::directory_iterator(data_dir)) {
        if (entry.path().extension() == ".bmp") {
            all_bmp_files.push_back(entry.path().string());
        }
    }
    std::sort(all_bmp_files.begin(), all_bmp_files.end());

    if (all_bmp_files.empty()) {
        std::cerr << "Error: No BMP files found in " << data_dir << "\n";
        return 1;
    }

    // 查找模板文件：优先使用 template.bmp，否则使用第一张图片
    std::string template_file = data_dir + "/template.bmp";
    std::vector<std::string> image_files;

    if (fs::exists(template_file)) {
        // 使用 template.bmp 作为模板
        for (const auto& file : all_bmp_files) {
            std::string filename = fs::path(file).filename().string();
            if (filename != "template.bmp") {
                image_files.push_back(file);
            }
        }
        std::cout << "Template: " << template_file << " (explicit template file)\n";
    } else {
        // 没有 template.bmp，使用第一张图片作为模板
        template_file = all_bmp_files[0];
        for (size_t i = 1; i < all_bmp_files.size(); ++i) {
            image_files.push_back(all_bmp_files[i]);
        }
        std::cout << "Template: " << template_file << " (first image as template)\n";
    }

    if (image_files.empty()) {
        std::cerr << "Error: No test images found (need at least 2 BMP files)\n";
        return 1;
    }

    std::cout << "Found " << image_files.size() << " test images\n\n";

    // ========== 按照运行流程实例操作 ==========

    // Step 1: 创建检测器
    std::cout << "Step 1: Creating detector...\n";
    DefectDetector detector;

    // Step 2: 调用图片导入接口，输入一张bitmap格式的图像作为原始图
    std::cout << "Step 2: Importing template image...\n";

    if (!detector.importTemplateFromFile(template_file)) {
        std::cerr << "Error: Failed to import template: " << template_file << "\n";
        return 1;
    }

    const cv::Mat& templ = detector.getTemplate();
    std::cout << "  Template loaded: " << templ.cols << "x" << templ.rows << " pixels\n";
    std::cout << "  File: " << template_file << "\n\n";

    // Step 2.5: Configure detection parameters BEFORE adding ROIs
    std::cout << "Step 2.5: Configuring detection parameters...\n";

    // 配置二值化阈值
    int binary_threshold_value = 128;  // 二值图像标准阈值
    detector.setParameter("binary_threshold", static_cast<float>(binary_threshold_value));

    // 配置匹配方法为 BINARY
    detector.getTemplateMatcher().setMethod(MatchMethod::BINARY);
    detector.getTemplateMatcher().setBinaryThreshold(binary_threshold_value);
    std::cout << "  Comparison method: BINARY (for B&W images)\n";
    std::cout << "  binary_threshold = " << binary_threshold_value << "\n";

    // ========== 二值图像优化模式配置 ==========
    if (binary_optimization_enabled) {
        std::cout << "\n  *** 使用二值图像优化模式 ***\n";
        std::cout << "  Binary Image Optimization: ENABLED\n";

        // 应用二值检测参数
        detector.setBinaryDetectionParams(binary_params);

        std::cout << "  Parameters:\n";
        std::cout << "    - auto_detect_binary = " << (binary_params.auto_detect_binary ? "true" : "false") << "\n";
        std::cout << "    - noise_filter_size = " << binary_params.noise_filter_size << " px\n";
        std::cout << "    - edge_tolerance_pixels = " << binary_params.edge_tolerance_pixels << " px\n";
        std::cout << "    - edge_diff_ignore_ratio = " << (binary_params.edge_diff_ignore_ratio * 100) << "%\n";
        std::cout << "    - min_significant_area = " << binary_params.min_significant_area << " px\n";
        std::cout << "    - area_diff_threshold = " << (binary_params.area_diff_threshold * 100) << "%\n";
        std::cout << "    - overall_similarity_threshold = " << (binary_params.overall_similarity_threshold * 100) << "%\n";
    } else {
        std::cout << "\n  Binary Image Optimization: DISABLED\n";
        std::cout << "  (Use default algorithm, may have higher false positive rate)\n";
    }

    // 其他检测参数 - 启用瑕疵检测
    detector.setParameter("min_defect_size", 10);   // 检测大于10像素的瑕疵（根据需求文档）
    detector.setParameter("blur_kernel_size", 3);   // 轻微平滑
    detector.setParameter("detect_black_on_white", 1.0f);  // 启用白底黑色脏污检测
    detector.setParameter("detect_white_on_black", 1.0f);  // 启用黑底白色瑕疵检测
    std::cout << "\n  min_defect_size = " << detector.getParameter("min_defect_size") << " (defect detection enabled)\n";
    std::cout << "  blur_kernel_size = " << detector.getParameter("blur_kernel_size") << "\n";
    std::cout << "  detect_black_on_white = enabled (white background + black defect)\n";
    std::cout << "  detect_white_on_black = enabled (black background + white defect)\n\n";

    // Step 3: 调用ROI设置接口
    std::vector<cv::Rect> selected_rois;
    std::vector<float> selected_thresholds;

    // 决定ROI选择模式
    if (!load_roi_file.empty()) {
        // 从文件加载ROI配置
        std::cout << "Step 3: Loading ROI regions from file...\n";
        if (!loadROIConfig(load_roi_file, selected_rois, selected_thresholds)) {
            std::cerr << "Error: Failed to load ROI configuration from: " << load_roi_file << "\n";
            return 1;
        }

        // 验证ROI是否在图像边界内
        for (size_t i = 0; i < selected_rois.size(); ++i) {
            const cv::Rect& roi = selected_rois[i];
            if (roi.x < 0 || roi.y < 0 ||
                roi.x + roi.width > templ.cols ||
                roi.y + roi.height > templ.rows) {
                std::cerr << "Warning: ROI " << (i+1) << " is outside template boundaries, adjusting...\n";
                selected_rois[i].x = std::max(0, roi.x);
                selected_rois[i].y = std::max(0, roi.y);
                selected_rois[i].width = std::min(roi.width, templ.cols - selected_rois[i].x);
                selected_rois[i].height = std::min(roi.height, templ.rows - selected_rois[i].y);
            }
        }

        // 添加到检测器
        for (size_t i = 0; i < selected_rois.size(); ++i) {
            const cv::Rect& roi = selected_rois[i];
            int roi_id = detector.addROI(roi.x, roi.y, roi.width, roi.height, selected_thresholds[i]);
            std::cout << "  ROI " << roi_id << ": (" << roi.x << ", " << roi.y << ") "
                      << roi.width << "x" << roi.height
                      << " threshold=" << std::fixed << std::setprecision(2) << selected_thresholds[i] << "\n";
        }

    } else if (manual_roi_mode) {
        // 手动交互式ROI选择
        std::cout << "Step 3: Manual ROI selection mode...\n";
        std::cout << "  Target ROI count: " << roi_count << "\n";

        if (!performManualROISelection(templ, roi_count, selected_rois, selected_thresholds)) {
            std::cerr << "ROI selection was cancelled. Exiting.\n";
            return 1;
        }

        // 添加到检测器
        for (size_t i = 0; i < selected_rois.size(); ++i) {
            const cv::Rect& roi = selected_rois[i];
            int roi_id = detector.addROI(roi.x, roi.y, roi.width, roi.height, selected_thresholds[i]);
            std::cout << "  ROI " << roi_id << ": (" << roi.x << ", " << roi.y << ") "
                      << roi.width << "x" << roi.height
                      << " threshold=" << std::fixed << std::setprecision(2) << selected_thresholds[i] << "\n";
        }

        // 保存ROI配置（如果指定了保存文件）
        if (!save_roi_file.empty()) {
            saveROIConfig(save_roi_file, selected_rois, selected_thresholds);
        }

    } else {
        // 随机ROI生成模式（默认）
        std::cout << "Step 3: Setting up ROI regions (random placement)...\n";
        std::cout << "  Random seed: " << random_seed
                  << (seed_specified ? " (user-specified)" : " (time-based)") << "\n";
        std::cout << "  Generating " << roi_count << " random ROI regions...\n";

        // Initialize random number generator with seed
        std::mt19937 rng(random_seed);

        // ROI size based on template dimensions
        int roi_w = templ.cols / 25;  // Slightly larger ROIs
        int roi_h = templ.rows / 25;

        // Use a very large margin from image edges
        int margin_x = templ.cols / 5;  // 20% margin from each edge
        int margin_y = templ.rows / 5;
        int max_x = templ.cols - roi_w - margin_x;
        int max_y = templ.rows - roi_h - margin_y;

        // Threshold values
        std::vector<float> thresholds = {0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f};
        std::vector<std::string> roi_names = {"Logo", "Text", "Barcode", "Date", "Batch", "Symbol", "Border", "Extra"};

        // Create uniform distributions for x and y positions
        std::uniform_int_distribution<int> dist_x(margin_x, max_x);
        std::uniform_int_distribution<int> dist_y(margin_y, max_y);

        // Generate random ROI regions
        for (int r = 0; r < roi_count; ++r) {
            int x = dist_x(rng);
            int y = dist_y(rng);
            float threshold = thresholds[r % thresholds.size()];
            std::string name = roi_names[r % roi_names.size()];

            int roi_id = detector.addROI(x, y, roi_w, roi_h, threshold);
            selected_rois.push_back(cv::Rect(x, y, roi_w, roi_h));
            selected_thresholds.push_back(threshold);
            std::cout << "  ROI " << roi_id << ": " << name << " region at ("
                      << x << "," << y << ") threshold=" << std::fixed << std::setprecision(2)
                      << threshold << "\n";
        }

        // 保存随机生成的ROI配置（如果指定了保存文件）
        if (!save_roi_file.empty()) {
            saveROIConfig(save_roi_file, selected_rois, selected_thresholds);
        }
    }

    std::cout << "  Total ROIs: " << detector.getROICount() << "\n\n";

    // 启用自动定位校正 - 解决测试图与模板图位置偏差问题
    std::cout << "Step 3.5: Auto alignment configuration...\n";
    detector.enableAutoLocalization(auto_align_enabled);
    detector.setAlignmentMode(align_mode);

    std::cout << "  Auto localization: " << (auto_align_enabled ? "ENABLED" : "DISABLED") << "\n";
    if (auto_align_enabled) {
        std::string mode_str;
        switch (align_mode) {
            case AlignmentMode::NONE: mode_str = "NONE"; break;
            case AlignmentMode::FULL_IMAGE: mode_str = "FULL_IMAGE (slower, whole image transform)"; break;
            case AlignmentMode::ROI_ONLY: mode_str = "ROI_ONLY (faster, recommended)"; break;
        }
        std::cout << "  Alignment mode: " << mode_str << "\n";
        std::cout << "  This corrects position offset, rotation and scale differences\n";
    } else {
        std::cout << "  WARNING: May produce false positives if images are not aligned!\n";
    }
    std::cout << "\n";

    // Step 4: 外部相机触发，输入新图片并进行检测
    std::cout << "Step 4: Processing test images...\n";
    std::cout << "----------------------------------------\n";
    
    int pass_count = 0;
    int fail_count = 0;
    double total_time = 0;

    // 处理所有图片（模拟相机连续触发）
    int process_count = std::min(20, (int)image_files.size());
    double total_loc_time = 0.0;
    double total_roi_time = 0.0;

    // Alignment statistics
    int align_success_count = 0;
    float max_offset = 0.0f, min_offset = 999.0f, total_offset = 0.0f;
    float max_rotation = 0.0f, min_rotation = 999.0f, total_rotation = 0.0f;
    float max_scale = 0.0f, min_scale = 999.0f, total_scale = 0.0f;

    // Pre-fetch ROI list for visualization (only need to do this once)
    std::vector<ROIRegion> rois;
    for (int r = 0; r < detector.getROICount(); ++r) {
        rois.push_back(detector.getROI(r));
    }

    bool user_quit = false;
    for (int i = 0; i < process_count && !user_quit; ++i) {
        const std::string& test_file = image_files[i];
        std::string filename = fs::path(test_file).filename().string();

        // 执行检测
        DetectionResult result = detector.detectFromFile(test_file);
        total_time += result.processing_time_ms;
        total_loc_time += result.localization_time_ms;
        total_roi_time += result.roi_comparison_time_ms;

        // Count passing ROIs and collect similarity info
        int passed_rois = 0;
        float min_similarity = 1.0f;
        int zero_similarity_count = 0;
        for (const auto& roi_result : result.roi_results) {
            if (roi_result.passed) passed_rois++;
            if (roi_result.similarity < min_similarity) {
                min_similarity = roi_result.similarity;
            }
            if (roi_result.similarity == 0.0f) {
                zero_similarity_count++;
            }
        }

        // 简化的控制台输出：文件名、状态、通过的ROI数、最低相似度、时间
        std::cout << "[" << std::setw(3) << (i + 1) << "/" << process_count << "] "
                  << std::left << std::setw(35) << filename
                  << " | " << (result.overall_passed ? "PASS" : "FAIL")
                  << " | " << passed_rois << "/" << result.roi_results.size() << " ROIs"
                  << " | min:" << std::fixed << std::setprecision(2) << (min_similarity * 100) << "%";
        if (zero_similarity_count > 0) {
            std::cout << " (" << zero_similarity_count << " null)";
        }
        std::cout << " | " << std::setw(6) << result.processing_time_ms << " ms\n";

        // Verbose mode: show detailed alignment info and ROI similarities
        if (verbose_mode) {
            if (auto_align_enabled) {
                const auto& loc = result.localization;
                std::cout << "    └─ Align: "
                          << (loc.success ? "OK" : "FAIL")
                          << " | offset=(" << std::fixed << std::setprecision(2)
                          << loc.offset_x << ", " << loc.offset_y << ")px"
                          << " | rot=" << std::setprecision(3) << loc.rotation_angle << "°"
                          << " | scale=" << std::setprecision(4) << loc.scale << "\n";
            }
            // Show individual ROI similarities
            std::cout << "    └─ ROIs: ";
            for (size_t r = 0; r < result.roi_results.size(); ++r) {
                const auto& roi = result.roi_results[r];
                std::cout << (roi.passed ? "✓" : "✗") << std::fixed << std::setprecision(0) << (roi.similarity * 100) << "%";
                if (r < result.roi_results.size() - 1) std::cout << " ";
            }
            std::cout << "\n";
        }

        if (result.overall_passed) {
            pass_count++;
        } else {
            fail_count++;
        }

        // Collect alignment statistics
        if (auto_align_enabled) {
            const auto& loc = result.localization;
            if (loc.success) {
                align_success_count++;
                float offset = std::sqrt(loc.offset_x * loc.offset_x + loc.offset_y * loc.offset_y);
                max_offset = std::max(max_offset, offset);
                min_offset = std::min(min_offset, offset);
                total_offset += offset;
                max_rotation = std::max(max_rotation, std::abs(loc.rotation_angle));
                min_rotation = std::min(min_rotation, std::abs(loc.rotation_angle));
                total_rotation += std::abs(loc.rotation_angle);
                max_scale = std::max(max_scale, loc.scale);
                min_scale = std::min(min_scale, loc.scale);
                total_scale += loc.scale;
            }
        }

        // 实时可视化系统
        if (viz_enabled) {
            cv::Mat test_img = cv::imread(test_file);
            std::string base_name = fs::path(test_file).stem().string();

            // 使用Visualizer进行可视化输出（显示窗口 + 保存文件）
            visualizer.processAndOutput(test_img, result, rois, base_name);

            // 检查用户是否按下ESC键退出
            // Note: waitKey is called inside displayVisualization, but we check return value
            // For ESC key (27), user wants to quit early
            int key = cv::waitKey(1);  // Quick check for ESC
            if (key == 27) {
                std::cout << "\n[INFO] User pressed ESC - stopping early...\n";
                user_quit = true;
            }
        }
    }

    // Close visualization window
    if (viz_enabled && viz_mode != OutputMode::FILE_ONLY) {
        cv::destroyAllWindows();
    }
    
    // 统计汇总
    double avg_total = total_time / process_count;
    double avg_loc = total_loc_time / process_count;
    double avg_roi = total_roi_time / process_count;

    std::cout << "\n========================================\n";
    std::cout << "           Summary Report\n";
    std::cout << "========================================\n";
    std::cout << "Comparison method: BINARY\n";
    std::cout << "Binary Optimization: " << (binary_optimization_enabled ? "ENABLED" : "DISABLED") << "\n";
    if (binary_optimization_enabled) {
        std::cout << "  - edge_tolerance = " << binary_params.edge_tolerance_pixels << " px\n";
        std::cout << "  - noise_filter = " << binary_params.noise_filter_size << " px\n";
        std::cout << "  - similarity_threshold = " << (binary_params.overall_similarity_threshold * 100) << "%\n";
    }
    std::cout << "Alignment mode: " << (auto_align_enabled ?
        (align_mode == AlignmentMode::FULL_IMAGE ? "FULL_IMAGE" : "ROI_ONLY") : "DISABLED") << "\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Images processed: " << process_count << "\n";
    std::cout << "Passed: " << pass_count << "\n";
    std::cout << "Failed: " << fail_count << "\n";
    std::cout << "Pass rate: " << std::fixed << std::setprecision(1)
              << (100.0 * pass_count / process_count) << "%\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Average total time: " << std::fixed << std::setprecision(2)
              << avg_total << " ms\n";
    if (auto_align_enabled) {
        std::cout << "  - Localization time: " << avg_loc << " ms\n";
        std::cout << "  - ROI comparison time: " << avg_roi << " ms\n";
    }
    std::cout << "----------------------------------------\n";

    // Alignment statistics summary
    if (auto_align_enabled && align_success_count > 0) {
        std::cout << "Alignment Statistics:\n";
        std::cout << "  Success rate: " << align_success_count << "/" << process_count
                  << " (" << std::setprecision(1) << (100.0 * align_success_count / process_count) << "%)\n";
        std::cout << "  Offset (px):  min=" << std::setprecision(2) << min_offset
                  << ", max=" << max_offset
                  << ", avg=" << (total_offset / align_success_count) << "\n";
        std::cout << "  Rotation (°): min=" << std::setprecision(3) << min_rotation
                  << ", max=" << max_rotation
                  << ", avg=" << (total_rotation / align_success_count) << "\n";
        std::cout << "  Scale:        min=" << std::setprecision(4) << min_scale
                  << ", max=" << max_scale
                  << ", avg=" << (total_scale / align_success_count) << "\n";
        std::cout << "----------------------------------------\n";
    } else if (!auto_align_enabled) {
        std::cout << "NOTE: Alignment disabled. Use --align-roi or --align-full to enable.\n";
        std::cout << "      Compare results with alignment to diagnose accuracy issues.\n";
        std::cout << "----------------------------------------\n";
    }

    std::cout << "ROI Comparison Performance: " << (avg_roi <= 20.0 ? "PASS" : "FAIL")
              << " (target: <= 20ms per readme.md)\n";
    std::cout << "========================================\n";

    return 0;
}

