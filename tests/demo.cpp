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
 * 
 * =================================================================================
 * 【调参指南】
 * 
 * 本系统包含多个关键参数，合理调参可以显著改善检测效果。
 * 以下是主要参数的详细说明和调参技巧：
 * 
 * -------------------------------------------------------------------------------
 * 一、二值图像优化参数 (BinaryDetectionParams) - 针对黑白图像的核心优化
 * -------------------------------------------------------------------------------
 * 
 * 1. enabled (bool)
 *    - 作用: 是否启用二值图像优化模式
 *    - 默认值: true
 *    - 调参技巧: 黑白图像强烈建议开启，彩色图像建议关闭使用普通NCC匹配
 *    - 命令行: --no-binary-opt 可禁用
 * 
 * 2. auto_detect_binary (bool)
 *    - 作用: 自动检测输入图像是否为二值图像
 *    - 默认值: true
 *    - 调参技巧: 通常保持开启，系统会自动选择合适的算法
 * 
 * 3. edge_tolerance_pixels (int)
 *    - 作用: 边缘容错像素数，忽略边缘附近指定像素范围内的差异
 *    - 默认值: 4 (像素)
 *    - 取值范围: 0-10
 *    - 调参技巧:
 *      * 值越大，对边缘位置偏差越宽容，误报越少
 *      * 值越小，检测越严格，可能检测到更多真实瑕疵但误报增加
 *      * 如果图像对齐有轻微偏差（1-3像素），建议设为2-4
 *      * 如果图像对齐很好，可以设为0-2提高检测精度
 *      * 对于边缘模糊或打印偏移的图像，建议设为3-5
 *    - 命令行: --edge-tol <px>
 * 
 * 4. noise_filter_size (int)
 *    - 作用: 噪声过滤尺寸，小于此面积的噪点将被忽略
 *    - 默认值: 10 (像素)
 *    - 取值范围: 1-50
 *    - 调参技巧:
 *      * 值越大，过滤的噪声越多，但可能漏检小瑕疵
 *      * 值越小，检测越敏感，可能产生更多误报
 *      * 建议根据最小关注瑕疵尺寸设置：设为最小瑕疵面积的1/2
 *      * 例如：最小关注瑕疵是20像素，则设为10
 *      * 如果图像噪声较多（扫描图像），可以适当增大到15-20
 *    - 命令行: --noise-size <px>
 * 
 * 5. edge_diff_ignore_ratio (float)
 *    - 作用: 边缘差异忽略比例，差异小于此比例视为正常
 *    - 默认值: 0.05 (5%)
 *    - 取值范围: 0.0-0.2
 *    - 调参技巧:
 *      * 对于打印质量稳定的图像，可以设为0.02-0.03（更严格）
 *      * 对于打印质量波动大的图像，可以设为0.08-0.10（更宽松）
 *      * 通常不需要调整，保持默认值即可
 * 
 * 6. min_significant_area (int)
 *    - 作用: 最小显著面积，小于此面积的差异视为噪声
 *    - 默认值: 20 (像素)
 *    - 取值范围: 5-100
 *    - 调参技巧:
 *      * 这是瑕疵检测的关键参数！
 *      * 根据实际要检测的最小瑕疵尺寸设置
 *      * 例如：要检测直径4像素的圆点瑕疵，面积约12-13，设为10
 *      * 如果只想检测较大的瑕疵（>100像素），设为50-80
 *      * 过小（<10）会导致噪声误报，过大（>50）会漏检小瑕疵
 *    - 命令行: --min-area <px>
 * 
 * 7. area_diff_threshold (float)
 *    - 作用: 面积差异阈值，面积差异超过此比例视为瑕疵
 *    - 默认值: 0.001 (0.1%)
 *    - 取值范围: 0.0001-0.01
 *    - 调参技巧:
 *      * 通常保持默认值，这是敏感度的微调参数
 *      * 增大此值会降低对大面积差异的敏感度
 * 
 * 8. overall_similarity_threshold (float)
 *    - 作用: 总体相似度阈值，低于此值判定为不合格
 *    - 默认值: 0.90 (90%)
 *    - 取值范围: 0.80-0.99
 *    - 调参技巧:
 *      * 这是最常用的调参参数！
 *      * 值越高（如0.95），检测越严格，更多图像被判为不合格
 *      * 值越低（如0.85），检测越宽松，减少误报但可能漏检
 *      * 建议根据实际良品率调整：如果良品判为不良太多，降低此值
 *      * 高精度要求场景：0.95-0.98
 *      * 普通检测场景：0.90-0.93
 *      * 快速筛选场景：0.85-0.88
 *    - 命令行: --sim-thresh <f>
 * 
 * -------------------------------------------------------------------------------
 * 二、通用检测参数
 * -------------------------------------------------------------------------------
 * 
 * 1. binary_threshold (int)
 *    - 作用: 二值化阈值，将灰度图像转为二值图像的阈值
 *    - 默认值: 128
 *    - 取值范围: 0-255
 *    - 调参技巧:
 *      * 对于标准黑白图像（0和255），保持128即可
 *      * 如果图像偏暗，可以适当降低（100-120）
 *      * 如果图像偏亮，可以适当提高（140-160）
 *      * 使用Otsu自适应阈值可以自动确定最佳值
 * 
 * 2. min_defect_size (int)
 *    - 作用: 最小瑕疵像素数，小于此值的瑕疵被忽略
 *    - 默认值: 100 (像素)
 *    - 取值范围: 10-500
 *    - 调参技巧:
 *      * 与 min_significant_area 类似，但更侧重于最终输出
 *      * 建议设为 min_significant_area 的 2-5 倍
 *      * 快速筛选大瑕疵：设为200-300
 *      * 精细检测小瑕疵：设为20-50
 *      * 注意：过小会导致噪声误判，过大会漏检真实小瑕疵
 * 
 * 3. blur_kernel_size (int)
 *    - 作用: 高斯模糊核大小，用于平滑图像减少噪声
 *    - 默认值: 3
 *    - 取值范围: 1-15 (奇数)
 *    - 调参技巧:
 *      * 值越大，平滑效果越强，噪声越少，但边缘越模糊
 *      * 值越小，保留更多细节，但噪声可能增加
 *      * 图像噪声大：设为5-7
 *      * 图像清晰：设为1-3（甚至设为1禁用模糊）
 *      * 注意：必须是奇数，偶数会自动+1
 * 
 * 4. detect_black_on_white / detect_white_on_black (bool)
 *    - 作用: 分别启用白底黑点检测和黑底白点检测
 *    - 默认值: 都启用 (1.0f)
 *    - 调参技巧:
 *      * 根据实际瑕疵类型选择
 *      * 只有黑瑕疵（如黑点、黑线）：只启用 detect_black_on_white
 *      * 只有白瑕疵（如白点、缺墨）：只启用 detect_white_on_black
 *      * 同时启用会增加计算量，但更全面
 * 
 * -------------------------------------------------------------------------------
 * 三、ROI（感兴趣区域）参数
 * -------------------------------------------------------------------------------
 * 
 * 1. roi_count (int)
 *    - 作用: ROI区域数量
 *    - 默认值: 4
 *    - 取值范围: 1-20
 *    - 调参技巧:
 *      * 更多ROI可以覆盖更多检测区域，但增加计算时间
 *      * 建议根据产品特征设置：Logo区、文字区、条码区、空白区等
 *      * 每个ROI最好对应一个相对均匀的区域
 *      * 时间敏感场景：减少ROI数量
 *      * 全覆盖检测：增加ROI数量
 *    - 命令行: --roi-count <num>
 * 
 * 2. similarity_threshold (每个ROI可单独设置)
 *    - 作用: 单个ROI的相似度阈值
 *    - 默认值: 0.50 (50%)
 *    - 取值范围: 0.0-1.0
 *    - 调参技巧:
 *      * 不同区域可以设置不同阈值
 *      * 关键区域（如条码）：设为0.90-0.95
 *      * 普通区域（如背景）：设为0.70-0.80
 *      * 变化大的区域（如文字）：设为0.50-0.60
 * 
 * -------------------------------------------------------------------------------
 * 四、对齐（Alignment）参数
 * -------------------------------------------------------------------------------
 * 
 * 1. auto_align_enabled (bool)
 *    - 作用: 是否启用自动对齐
 *    - 默认值: true
 *    - 调参技巧:
 *      * 除非模板和测试图像已完全对齐，否则建议开启
 *      * 关闭会显著降低检测准确度
 *      * 如果图像对齐很好，可以关闭以提高速度
 *    - 命令行: --no-align 禁用, --align-roi 启用ROI对齐, --align-full 启用全图对齐
 * 
 * 2. align_mode (AlignmentMode)
 *    - 作用: 对齐模式选择
 *    - 默认值: ROI_ONLY
 *    - 选项:
 *      * NONE: 不对齐（仅当图像已对齐时使用）
 *      * ROI_ONLY: 仅对齐ROI区域（更快，推荐）
 *      * FULL_IMAGE: 对齐整张图像（更精确但较慢）
 *    - 调参技巧:
 *      * 通常使用 ROI_ONLY 即可，速度更快
 *      * 如果图像整体变形严重，使用 FULL_IMAGE
 *      * 对齐速度：ROI_ONLY 比 FULL_IMAGE 快 30-50%
 * 
 * -------------------------------------------------------------------------------
 * 五、可视化参数
 * -------------------------------------------------------------------------------
 * 
 * 1. viz_enabled (bool)
 *    - 作用: 是否启用可视化输出
 *    - 默认值: true
 *    - 调参技巧:
 *      * 调试阶段建议开启，可以直观看到检测结果
 *      * 生产环境批量检测建议关闭以提高速度
 *    - 命令行: --no-viz 禁用, --viz-file 只输出文件, --viz-display 只显示
 * 
 * 2. display_delay_ms (int)
 *    - 作用: 显示延迟（毫秒）
 *    - 默认值: 1500 (1.5秒)
 *    - 调参技巧:
 *      * 调参时设为1000-2000，方便观察
 *      * 快速查看时可以设为100-300
 *      * 按空格或任意键可立即进入下一张
 *    - 命令行: --delay <ms>
 * 
 * -------------------------------------------------------------------------------
 * 六、调参工作流程建议
 * -------------------------------------------------------------------------------
 * 
 * 1. 初步调试阶段：
 *    - 使用少量测试图像（5-10张，包含良品和已知不良品）
 *    - 开启可视化 (--viz-display)
 *    - 使用手动ROI选择 (--manual-roi)
 *    - 从默认参数开始
 * 
 * 2. 参数微调阶段：
 *    - 如果良品被判为不良：
 *      * 增大 overall_similarity_threshold
 *      * 增大 edge_tolerance_pixels
 *      * 增大 noise_filter_size
 *    - 如果不良品被判为良品：
 *      * 降低 overall_similarity_threshold
 *      * 降低 min_significant_area
 *      * 检查对齐是否正常
 * 
 * 3. 验证阶段：
 *    - 使用大量测试图像（>100张）验证参数稳定性
 *    - 关注误报率（False Positive）和漏检率（False Negative）
 *    - 调整参数使两者达到平衡
 * 
 * 4. 部署阶段：
 *    - 保存ROI配置 (--save-roi)
 *    - 关闭可视化 (--no-viz)
 *    - 记录最终参数配置
 * 
 * =================================================================================
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
#include <sstream>

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
    std::cout << "  --sim-thresh <f>   Overall similarity threshold (default: 0.90)\n";
    std::cout << "\nManual ROI Selection:\n";
    std::cout << "  --manual-roi       Enable interactive manual ROI selection mode\n";
    std::cout << "  --load-roi <file>  Load ROI configuration from JSON file\n";
    std::cout << "  --save-roi <file>  Save selected ROI configuration to JSON file\n";
    std::cout << "\n  --help             Show this help message\n";
}


int main(int argc, char** argv) {
    // =================================================================================
    // 【第一节：基础配置参数】
    // 这些参数控制程序的基本运行行为
    // =================================================================================
    
    // data_dir: 数据目录路径
    // - 作用: 指定包含BMP图像文件的目录
    // - 默认值: "data/demo"
    // - 调参技巧: 
    //   * 相对路径基于程序工作目录
    //   * 目录中应包含 template.bmp（模板）和测试图像
    //   * 如果没有 template.bmp，第一张图像会被用作模板
    std::string data_dir = "data/demo";
    
    // output_dir: 输出目录路径
    // - 作用: 可视化结果和检测报告保存位置
    // - 默认值: "output"
    // - 调参技巧:
    //   * 确保程序有写入权限
    //   * 建议使用独立目录避免污染源代码目录
    std::string output_dir = "output";
    
    // config_file: 可视化配置文件路径
    // - 作用: JSON格式的可视化配置，控制输出样式
    // - 默认值: "config/visualization_config.json"
    // - 调参技巧:
    //   * 可通过配置文件精细控制颜色、字体、输出格式等
    //   * 命令行 --config 可指定其他配置文件
    std::string config_file = "config/visualization_config.json";
    
    // viz_enabled: 可视化使能开关
    // - 作用: 是否启用结果可视化（显示窗口和/或保存文件）
    // - 默认值: true
    // - 调参技巧:
    //   * 调试阶段: 开启，直观查看检测结果
    //   * 生产环境: 关闭 (--no-viz)，提高处理速度约20-30%
    bool viz_enabled = true;
    
    // auto_align_enabled: 自动对齐使能开关
    // - 作用: 是否启用模板与测试图像的自动对齐
    // - 默认值: true
    // - 调参技巧:
    //   * 强烈建议保持开启！除非图像已机械对齐
    //   * 关闭后误报率可能增加50%以上
    //   * 如果检测速度是瓶颈且对齐很好，可以关闭
    bool auto_align_enabled = true;
    
    // align_mode: 对齐模式
    // - 作用: 选择图像对齐的算法模式
    // - 选项:
    //   * NONE: 不对齐（仅图像已对齐时使用）
    //   * ROI_ONLY: 只校正ROI区域（推荐，速度更快）
    //   * FULL_IMAGE: 校正整张图像（更精确但慢）
    // - 默认值: ROI_ONLY
    // - 调参技巧:
    //   * 通常 ROI_ONLY 足够，速度比 FULL_IMAGE 快30-50%
    //   * 如果图像有整体变形（透视、缩放），使用 FULL_IMAGE
    //   * 对齐时间对比：ROI_ONLY (~2ms) < FULL_IMAGE (~5ms)
    AlignmentMode align_mode = AlignmentMode::ROI_ONLY;
    
    // viz_mode: 可视化输出模式
    // - 作用: 控制可视化结果输出到哪里
    // - 选项:
    //   * FILE_ONLY: 只保存到文件
    //   * DISPLAY_ONLY: 只显示窗口
    //   * BOTH: 同时保存和显示（默认）
    // - 调参技巧:
    //   * 自动化测试: FILE_ONLY
    //   * 人工检查: DISPLAY_ONLY 或 BOTH
    OutputMode viz_mode = OutputMode::BOTH;
    
    // display_delay_ms: 显示延迟（毫秒）
    // - 作用: 每张图像显示后的等待时间
    // - 默认值: 1500 (1.5秒)
    // - 取值范围: 0 - 10000
    // - 调参技巧:
    //   * 仔细观察: 2000-3000ms
    //   * 快速浏览: 500-1000ms
    //   * 自动运行: 1-100ms
    //   * 按任意键可跳过等待
    int display_delay_ms = 1500;
    
    // random_seed: 随机种子
    // - 作用: 控制随机ROI位置的可重复性
    // - 默认值: 基于当前时间（每次运行不同）
    // - 调参技巧:
    //   * 固定种子可复现相同的ROI位置，方便对比测试
    //   * 多轮测试对比时建议固定种子
    //   * 生产环境建议时间种子以获得更好覆盖率
    unsigned int random_seed = static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    bool seed_specified = false;  // 标记用户是否指定了种子
    
    // roi_count: ROI区域数量
    // - 作用: 随机模式或手动模式下生成的ROI数量
    // - 默认值: 4
    // - 取值范围: 1-20
    // - 调参技巧:
    //   * 更多ROI = 更全面覆盖，但更慢
    //   * 建议根据产品特征区域设置（Logo、文字、条码等）
    //   * 每张ROI增加约3-5ms处理时间
    //   * 时间敏感: 1-2个ROI
    //   * 全面检测: 6-10个ROI
    int roi_count = 4;
    
    // verbose_mode: 详细输出模式
    // - 作用: 是否显示每幅图像的详细对齐和ROI信息
    // - 默认值: false
    // - 调参技巧:
    //   * 调试对齐问题时开启，可看到偏移量、旋转角度等
    //   * 分析误报原因时很有用
    //   * 生产环境建议关闭减少日志量
    bool verbose_mode = false;

    // =================================================================================
    // 【第二节：二值图像优化参数】
    // 这些参数专门针对黑白二值图像优化，是系统的核心特性
    // 详细说明见文件顶部的【调参指南】
    // =================================================================================
    
    // binary_optimization_enabled: 二值优化总开关
    // - 作用: 是否启用二值图像优化模式
    // - 默认值: true
    // - 调参技巧:
    //   * 黑白图像必须开启！这是系统的核心优势
    //   * 彩色图像应关闭，使用普通NCC匹配
    //   * 开启后可显著降低边缘误报
    bool binary_optimization_enabled = true;
    
    // BinaryDetectionParams: 二值检测参数结构体
    // 包含以下字段（详细说明见文件顶部调参指南）：
    BinaryDetectionParams binary_params;
    
    // enabled: 二值优化内部使能
    binary_params.enabled = true;
    
    // auto_detect_binary: 自动检测是否为二值图像
    // - 如果为true，系统会自动判断输入图像类型
    // - 建议保持true，让系统自动选择
    binary_params.auto_detect_binary = true;
    
    // noise_filter_size: 噪声过滤尺寸（像素）
    // - 作用: 小于此面积的连通区域被视为噪声忽略
    // - 默认值: 10
    // - 调参技巧:
    //   * 设为最小关注瑕疵面积的1/2
    //   * 扫描图像噪点多：增大到15-20
    //   * 高质量图像：减小到5-8
    binary_params.noise_filter_size = 10;
    
    // edge_tolerance_pixels: 边缘容错像素数
    // - 作用: 边缘附近此范围内的差异被忽略
    // - 默认值: 4
    // - 调参技巧:
    //   * 对齐偏差1-3像素：设为2-4
    //   * 对齐很好：设为0-2
    //   * 打印偏移大：设为3-5
    binary_params.edge_tolerance_pixels = 4;
    
    // edge_diff_ignore_ratio: 边缘差异忽略比例
    // - 作用: 边缘处小于此比例的差异被视为正常
    // - 默认值: 0.05 (5%)
    // - 通常不需要调整
    binary_params.edge_diff_ignore_ratio = 0.05f;
    
    // min_significant_area: 最小显著面积（像素）
    // - 作用: 小于此面积的差异被视为非瑕疵
    // - 默认值: 20
    // - 调参技巧:
    //   * 这是瑕疵检测的核心参数！
    //   * 根据实际要检测的最小瑕疵设置
    //   * 过大(>50)会漏检，过小(<10)会误报
    binary_params.min_significant_area = 20;
    
    // area_diff_threshold: 面积差异阈值
    // - 作用: 面积差异超过此比例视为瑕疵
    // - 默认值: 0.001 (0.1%)
    // - 通常保持默认值
    binary_params.area_diff_threshold = 0.001f;
    
    // overall_similarity_threshold: 总体相似度阈值
    // - 作用: 整体相似度低于此值判定为不合格
    // - 默认值: 0.90 (90%)
    // - 调参技巧:
    //   * 最常用的调参参数！
    //   * 良品被判不良 -> 降低此值
    //   * 不良品被判良品 -> 提高此值
    //   * 高精度: 0.95-0.98
    //   * 普通: 0.90-0.93
    //   * 快速筛选: 0.85-0.88
    binary_params.overall_similarity_threshold = 0.90f;

    // =================================================================================
    // 【第三节：手动ROI配置】
    // =================================================================================
    
    // manual_roi_mode: 手动ROI选择模式
    // - 作用: 启用交互式ROI框选界面
    // - 默认值: false
    // - 调参技巧:
    //   * 首次设置ROI时建议开启
    //   * 设置完成后保存配置 (--save-roi)
    //   * 后续运行加载配置 (--load-roi)
    bool manual_roi_mode = false;
    
    // load_roi_file: ROI配置文件加载路径
    // - 作用: 从JSON文件加载预设的ROI配置
    // - 默认值: "" (空，不加载)
    // - 调参技巧:
    //   * 可重复使用相同的ROI配置
    //   * 适合批量检测相同类型产品
    std::string load_roi_file = "";
    
    // save_roi_file: ROI配置文件保存路径
    // - 作用: 将手动或随机生成的ROI保存到JSON文件
    // - 默认值: "" (空，不保存)
    // - 调参技巧:
    //   * 手动设置后保存，供后续使用
    //   * 随机生成满意后保存，固定ROI位置
    std::string save_roi_file = "";

    // =================================================================================
    // 【第四节：命令行参数解析】
    // 以下代码解析命令行参数，覆盖上述默认值
    // =================================================================================
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
    
    // =================================================================================
    // 【第五节：程序启动和初始化】
    // =================================================================================
    std::cout << "========================================\n";
    std::cout << "   Print Defect Detection System Demo\n";
    std::cout << "========================================\n\n";

    // 初始化可视化器
    Visualizer visualizer;
    if (fs::exists(config_file)) {
        if (visualizer.loadConfig(config_file)) {
            std::cout << "Loaded visualization config from: " << config_file << "\n";
        } else {
            std::cout << "Warning: Failed to load config, using defaults\n";
        }
    }

    // 应用命令行覆盖的可视化配置
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


    // =================================================================================
    // 【第六节：检测器初始化和参数配置】
    // 这是核心配置部分，所有检测参数都在这里设置
    // =================================================================================
    
    // Step 1: 创建检测器实例
    std::cout << "Step 1: Creating detector...\n";
    DefectDetector detector;

    // Step 2: 导入模板图像
    // 模板是检测的基准，所有测试图像都会与模板比较
    std::cout << "Step 2: Importing template image...\n";

    if (!detector.importTemplateFromFile(template_file)) {
        std::cerr << "Error: Failed to import template: " << template_file << "\n";
        return 1;
    }

    const cv::Mat& templ = detector.getTemplate();
    std::cout << "  Template loaded: " << templ.cols << "x" << templ.rows << " pixels\n";
    std::cout << "  File: " << template_file << "\n\n";

    // Step 2.5: 在添加ROI之前配置检测参数
    // 【重要】这些参数必须在添加ROI之前设置，否则可能不生效
    std::cout << "Step 2.5: Configuring detection parameters...\n";

    // =================================================================================
    // 【6.1 二值化阈值参数】
    // =================================================================================
    // binary_threshold: 二值化阈值
    // - 作用: 将灰度图像转换为二值图像的分界值
    // - 默认值: 128 (标准中值)
    // - 取值范围: 0-255
    // - 调参技巧:
    //   * 标准黑白图像（0和255）保持128
    //   * 图像整体偏暗（灰底黑字）：设为100-120
    //   * 图像整体偏亮（白底灰字）：设为140-160
    //   * 可先用图像查看工具确定最佳阈值
    int binary_threshold_value = 128;
    detector.setParameter("binary_threshold", static_cast<float>(binary_threshold_value));

    // 配置模板匹配器使用BINARY模式
    // BINARY模式专门针对二值图像优化，比NCC更快更准
    detector.getTemplateMatcher().setMethod(MatchMethod::BINARY);
    detector.getTemplateMatcher().setBinaryThreshold(binary_threshold_value);
    std::cout << "  Comparison method: BINARY (for B&W images)\n";
    std::cout << "  binary_threshold = " << binary_threshold_value << "\n";

    // =================================================================================
    // 【6.2 二值图像优化模式配置】
    // 这是系统的核心特性，专门针对黑白二值图像优化
    // =================================================================================
    if (binary_optimization_enabled) {
        std::cout << "\n  *** Binary Image Optimization Mode ***\n";
        std::cout << "  Binary Image Optimization: ENABLED\n";

        // 应用二值检测参数到检测器
        detector.setBinaryDetectionParams(binary_params);

        // 打印当前配置的二值优化参数
        std::cout << "  Parameters:\n";
        std::cout << "    - auto_detect_binary = " << (binary_params.auto_detect_binary ? "true" : "false") << "\n";
        std::cout << "    - noise_filter_size = " << binary_params.noise_filter_size << " px\n";
        std::cout << "    - edge_tolerance_pixels = " << binary_params.edge_tolerance_pixels << " px\n";
        std::cout << "    - edge_diff_ignore_ratio = " << (binary_params.edge_diff_ignore_ratio * 100) << "%\n";
        std::cout << "    - min_significant_area = " << binary_params.min_significant_area << " px\n";
        std::cout << "    - area_diff_threshold = " << (binary_params.area_diff_threshold * 100) << "%\n";
        std::cout << "    - overall_similarity_threshold = " << (binary_params.overall_similarity_threshold * 100) << "%\n";
        
        // 【调参提示】
        std::cout << "\n  [Tuning Tips for Binary Optimization]\n";
        std::cout << "    1. If good products marked as defective:\n";
        std::cout << "       - Increase --edge-tol (try 3-5)\n";
        std::cout << "       - Increase --noise-size (try 15-20)\n";
        std::cout << "       - Increase --sim-thresh (try 0.92-0.95)\n";
        std::cout << "    2. If defective products marked as good:\n";
        std::cout << "       - Decrease --sim-thresh (try 0.85-0.88)\n";
        std::cout << "       - Decrease --min-area (try 10-15)\n";
        std::cout << "    3. If too many edge false positives:\n";
        std::cout << "       - Increase --edge-tol to 4-6\n";
        std::cout << "       - Check alignment with --verbose\n";
    } else {
        std::cout << "\n  Binary Image Optimization: DISABLED\n";
        std::cout << "  (Use default algorithm, may have higher false positive rate)\n";
    }

    // =================================================================================
    // 【6.3 其他通用检测参数】
    // =================================================================================
    
    // MIN_DEFECT_SIZE: 最小瑕疵尺寸（像素）
    // - 作用: 小于此面积的瑕疵在最终判定中被忽略
    // - 默认值: 100 (像素)
    // - 取值范围: 10-500
    // - 调参技巧:
    //   * 设为 min_significant_area 的 2-5 倍
    //   * 快速筛选大瑕疵：200-300
    //   * 精细检测：20-50
    //   * 过小会噪声误报，过大漏检真实瑕疵
    const int MIN_DEFECT_SIZE = 100;
    detector.setParameter("min_defect_size", MIN_DEFECT_SIZE);
    
    // blur_kernel_size: 高斯模糊核大小
    // - 作用: 平滑图像减少噪声，但会略微模糊边缘
    // - 默认值: 3
    // - 取值范围: 1-15（奇数，偶数自动+1）
    // - 调参技巧:
    //   * 图像噪声多（扫描件）：设为5-7
    //   * 图像清晰（相机直出）：设为1-3
    //   * 设为1相当于禁用模糊
    detector.setParameter("blur_kernel_size", 3);
    
    // detect_black_on_white: 白底黑点检测使能
    // - 作用: 检测白色背景上的黑色瑕疵（黑点、黑线、缺墨等）
    // - 默认值: 1.0f (启用)
    // - 调参技巧:
    //   * 如果只关心白瑕疵（白点、划痕），可设为0禁用
    //   * 同时启用两项会增加计算量
    detector.setParameter("detect_black_on_white", 1.0f);
    
    // detect_white_on_black: 黑底白点检测使能
    // - 作用: 检测黑色背景上的白色瑕疵（白点、墨点、污渍等）
    // - 默认值: 1.0f (启用)
    // - 调参技巧:
    //   * 如果只关心黑瑕疵，可设为0禁用
    //   * 根据实际瑕疵类型选择性启用
    detector.setParameter("detect_white_on_black", 1.0f);
    
    std::cout << "\n  min_defect_size = " << detector.getParameter("min_defect_size") 
              << " (defect detection enabled, threshold: " << MIN_DEFECT_SIZE << " pixels)\n";
    std::cout << "  blur_kernel_size = " << detector.getParameter("blur_kernel_size") << "\n\n";

    // =================================================================================
    // 【第七节：ROI（感兴趣区域）设置】
    // ROI定义了检测的具体区域，合理的ROI设置可以显著提高检测效果
    // =================================================================================
    std::vector<cv::Rect> selected_rois;
    std::vector<float> selected_thresholds;

    // 决定ROI选择模式（优先级：加载文件 > 手动选择 > 随机生成）
    if (!load_roi_file.empty()) {
        // -------------------------------------------------------------------------
        // 模式1: 从文件加载ROI配置
        // 适合批量检测相同类型产品，可重复性强
        // -------------------------------------------------------------------------
        std::cout << "Step 3: Loading ROI regions from file...\n";
        if (!loadROIConfig(load_roi_file, selected_rois, selected_thresholds)) {
            std::cerr << "Error: Failed to load ROI configuration from: " << load_roi_file << "\n";
            return 1;
        }

        // 验证ROI是否在图像边界内，超出则自动裁剪
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

        // 添加ROI到检测器
        for (size_t i = 0; i < selected_rois.size(); ++i) {
            const cv::Rect& roi = selected_rois[i];
            int roi_id = detector.addROI(roi.x, roi.y, roi.width, roi.height, selected_thresholds[i]);
            std::cout << "  ROI " << roi_id << ": (" << roi.x << ", " << roi.y << ") "
                      << roi.width << "x" << roi.height
                      << " threshold=" << std::fixed << std::setprecision(2) << selected_thresholds[i] << "\n";
        }

    } else if (manual_roi_mode) {
        // -------------------------------------------------------------------------
        // 模式2: 手动交互式ROI选择
        // 适合首次设置或精细调整ROI位置
        // -------------------------------------------------------------------------
        std::cout << "Step 3: Manual ROI selection mode...\n";
        std::cout << "  Target ROI count: " << roi_count << "\n";

        if (!performManualROISelection(templ, roi_count, selected_rois, selected_thresholds)) {
            std::cerr << "ROI selection was cancelled. Exiting.\n";
            return 1;
        }

        // 添加ROI到检测器
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
        // -------------------------------------------------------------------------
        // 模式3: 随机ROI生成（默认模式）
        // 适合快速测试，ROI位置随机分布
        // -------------------------------------------------------------------------
        std::cout << "Step 3: Setting up ROI regions (random placement)...\n";
        std::cout << "  Random seed: " << random_seed
                  << (seed_specified ? " (user-specified)" : " (time-based)") << "\n";
        std::cout << "  Generating " << roi_count << " random ROI regions...\n";

        // 使用随机种子初始化随机数生成器
        std::mt19937 rng(random_seed);

        // ROI尺寸基于模板图像尺寸计算
        // 这里设为模板尺寸的1/25，确保ROI不会太大
        int roi_w = templ.cols / 25;
        int roi_h = templ.rows / 25;

        // 边缘边距，避免ROI太靠近图像边缘
        // 设为图像尺寸的20%，留出对齐偏差的空间
        int margin_x = templ.cols / 5;  // 20% 水平边距
        int margin_y = templ.rows / 5;  // 20% 垂直边距
        int max_x = templ.cols - roi_w - margin_x;
        int max_y = templ.rows - roi_h - margin_y;

        // ROI阈值列表，可根据ROI重要性设置不同阈值
        std::vector<float> thresholds = {0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f};
        
        // ROI名称，用于标识不同类型的检测区域
        std::vector<std::string> roi_names = {"Logo", "Text", "Barcode", "Date", "Batch", "Symbol", "Border", "Extra"};

        // 创建均匀分布用于随机位置生成
        std::uniform_int_distribution<int> dist_x(margin_x, max_x);
        std::uniform_int_distribution<int> dist_y(margin_y, max_y);

        // 生成随机ROI区域
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

    // =================================================================================
    // 【第八节：自动对齐配置】
    // 自动对齐校正模板与测试图像之间的位置偏差、旋转和缩放差异
    // =================================================================================
    std::cout << "Step 3.5: Auto alignment configuration...\n";
    
    // 启用/禁用自动定位
    detector.enableAutoLocalization(auto_align_enabled);
    
    // 设置对齐模式
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
        
        // 【对齐调参提示】
        std::cout << "\n  [Alignment Tuning Tips]\n";
        std::cout << "    - Use --verbose to see alignment details\n";
        std::cout << "    - If alignment fails often, check image quality\n";
        std::cout << "    - ROI_ONLY mode is 30-50% faster than FULL_IMAGE\n";
        std::cout << "    - For large misalignment (>50px), use FULL_IMAGE mode\n";
    } else {
        std::cout << "  WARNING: May produce false positives if images are not aligned!\n";
    }
    std::cout << "\n";


    // =================================================================================
    // 【第九节：测试图像处理循环】
    // 逐张处理测试图像，执行检测并输出结果
    // =================================================================================
    std::cout << "Step 4: Processing test images...\n";
    std::cout << "----------------------------------------\n";
    
    int pass_count = 0;
    int fail_count = 0;
    double total_time = 0;

    // 限制处理数量，避免测试时间过长
    // 可通过修改此值处理更多图像
    int process_count = std::min(20, (int)image_files.size());
    double total_loc_time = 0.0;  // 总定位时间
    double total_roi_time = 0.0;  // 总ROI比对时间

    // 对齐统计信息
    int align_success_count = 0;
    float max_offset = 0.0f, min_offset = 999.0f, total_offset = 0.0f;
    float max_rotation = 0.0f, min_rotation = 999.0f, total_rotation = 0.0f;
    float max_scale = 0.0f, min_scale = 999.0f, total_scale = 0.0f;

    // 预获取ROI列表用于可视化（只需获取一次）
    std::vector<ROIRegion> rois;
    for (int r = 0; r < detector.getROICount(); ++r) {
        rois.push_back(detector.getROI(r));
    }

    // 处理循环
    bool user_quit = false;
    for (int i = 0; i < process_count && !user_quit; ++i) {
        const std::string& test_file = image_files[i];
        std::string filename = fs::path(test_file).filename().string();

        // 执行检测
        DetectionResult result = detector.detectFromFile(test_file);
        total_time += result.processing_time_ms;
        total_loc_time += result.localization_time_ms;
        total_roi_time += result.roi_comparison_time_ms;

        // 统计通过和未通过的ROI，收集相似度信息
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

        // =============================================================================
        // 【9.1 瑕疵检测判定】
        // 根据瑕疵大小和数量判定是否通过
        // =============================================================================
        
        // DEFECT_SIZE_THRESHOLD: 显著瑕疵尺寸阈值
        // - 作用: 只有面积大于等于此值的瑕疵才会影响最终判定
        // - 默认值: 100 (像素)
        // - 调参技巧:
        //   * 应与 min_significant_area 保持一致或稍大
        //   * 过大(>200)会漏检小瑕疵
        //   * 过小(<50)会噪声误报
        const int DEFECT_SIZE_THRESHOLD = 100;
        
        bool has_significant_defects = false;
        std::vector<std::string> defect_info_list;
        
        // 遍历所有ROI的瑕疵，筛选出显著瑕疵
        for (const auto& roi_result : result.roi_results) {
            for (const auto& defect : roi_result.defects) {
                if (defect.area >= DEFECT_SIZE_THRESHOLD) {
                    has_significant_defects = true;
                    std::stringstream defect_info;
                    defect_info << "ROI" << roi_result.roi_id 
                                << "[" << (int)defect.area << "px@(" 
                                << (int)defect.x << "," << (int)defect.y << ")]";
                    defect_info_list.push_back(defect_info.str());
                }
            }
        }
        
        // 综合判定：没有显著瑕疵且整体通过才算合格
        bool should_pass = !has_significant_defects && result.overall_passed;
        
        // =============================================================================
        // 【9.2 详细输出模式（verbose）】
        // 显示每张图像的详细对齐信息和ROI结果
        // =============================================================================
        if (verbose_mode) {
            // 显示对齐信息
            if (auto_align_enabled) {
                const auto& loc = result.localization;
                std::cout << "    |- Align: "
                          << (loc.success ? "OK" : "FAIL")
                          << " | offset=(" << std::fixed << std::setprecision(2)
                          << loc.offset_x << ", " << loc.offset_y << ")px"
                          << " | rot=" << std::setprecision(3) << loc.rotation_angle << "deg"
                          << " | scale=" << std::setprecision(4) << loc.scale << "\n";
            }
            
            // 显示每个ROI的相似度
            std::cout << "    |- ROIs: ";
            for (size_t r = 0; r < result.roi_results.size(); ++r) {
                const auto& roi = result.roi_results[r];
                std::cout << (roi.passed ? "[P]" : "[F]") << std::fixed << std::setprecision(0) << (roi.similarity * 100) << "%";
                if (r < result.roi_results.size() - 1) std::cout << " ";
            }
            std::cout << "\n";
            
            // 显示瑕疵信息（如果有）
            if (!defect_info_list.empty()) {
                std::cout << "    |- DEFECTS: ";
                for (size_t d = 0; d < defect_info_list.size(); ++d) {
                    std::cout << defect_info_list[d];
                    if (d < defect_info_list.size() - 1) std::cout << ", ";
                }
                std::cout << "\n";
            }
        }
        
        // =============================================================================
        // 【9.3 最终判定和输出】
        // 根据瑕疵检测结果确定最终通过/失败状态
        // =============================================================================
        bool final_pass = !has_significant_defects;
        
        // 主输出行：文件名、状态、ROI统计、相似度、耗时
        std::cout << "[" << std::setw(3) << (i + 1) << "/" << process_count << "] "
                  << std::left << std::setw(35) << filename
                  << " | " << (final_pass ? "PASS" : "FAIL")
                  << " | " << passed_rois << "/" << result.roi_results.size() << " ROIs"
                  << " | min:" << std::fixed << std::setprecision(2) << (min_similarity * 100) << "%";
        
        // 显示零相似度警告（通常表示对齐失败）
        if (zero_similarity_count > 0) {
            std::cout << " (" << zero_similarity_count << " null)";
        }
        
        // 显示瑕疵信息
        if (has_significant_defects) {
            std::cout << " [DEFECTS:" << defect_info_list.size() << "]";
        }
        std::cout << " | " << std::setw(6) << result.processing_time_ms << " ms\n";
        
        // 详细模式：显示覆盖说明
        if (has_significant_defects && verbose_mode) {
            if (result.overall_passed) {
                std::cout << "    |- NOTE: Overridden to FAIL (similarity pass but defects >= " << DEFECT_SIZE_THRESHOLD << "px)\n";
            }
        }

        // 统计更新
        if (final_pass) {
            pass_count++;
        } else {
            fail_count++;
        }

        // =============================================================================
        // 【9.4 对齐统计信息收集】
        // 用于后续输出对齐统计报告
        // =============================================================================
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

        // =============================================================================
        // 【9.5 实时可视化】
        // 显示检测结果图像并保存到文件
        // =============================================================================
        if (viz_enabled) {
            cv::Mat test_img = cv::imread(test_file);
            std::string base_name = fs::path(test_file).stem().string();

            // 使用Visualizer进行可视化输出（显示窗口 + 保存文件）
            visualizer.processAndOutput(test_img, result, rois, base_name);

            // 检查用户是否按下ESC键退出
            int key = cv::waitKey(1);
            if (key == 27) {
                std::cout << "\n[INFO] User pressed ESC - stopping early...\n";
                user_quit = true;
            }
        }
    }

    // 关闭可视化窗口
    if (viz_enabled && viz_mode != OutputMode::FILE_ONLY) {
        cv::destroyAllWindows();
    }
    
    // =================================================================================
    // 【第十节：统计报告】
    // 汇总所有检测结果，输出统计信息
    // =================================================================================
    
    // 计算平均处理时间
    double avg_total = total_time / process_count;
    double avg_loc = total_loc_time / process_count;
    double avg_roi = total_roi_time / process_count;

    std::cout << "\n========================================\n";
    std::cout << "           Summary Report\n";
    std::cout << "========================================\n";
    std::cout << "Comparison method: BINARY\n";
    std::cout << "Binary Optimization: " << (binary_optimization_enabled ? "ENABLED" : "DISABLED") << "\n";
    
    // 显示二值优化参数配置
    if (binary_optimization_enabled) {
        std::cout << "  - edge_tolerance = " << binary_params.edge_tolerance_pixels << " px\n";
        std::cout << "  - noise_filter = " << binary_params.noise_filter_size << " px\n";
        std::cout << "  - similarity_threshold = " << (binary_params.overall_similarity_threshold * 100) << "%\n";
    }
    
    // 显示对齐模式
    std::cout << "Alignment mode: " << (auto_align_enabled ?
        (align_mode == AlignmentMode::FULL_IMAGE ? "FULL_IMAGE" : "ROI_ONLY") : "DISABLED") << "\n";
    
    std::cout << "----------------------------------------\n";
    std::cout << "Images processed: " << process_count << "\n";
    std::cout << "Passed: " << pass_count << "\n";
    std::cout << "Failed: " << fail_count << "\n";
    std::cout << "Pass rate: " << std::fixed << std::setprecision(1)
              << (100.0 * pass_count / process_count) << "%\n";
    std::cout << "----------------------------------------\n";
    
    // 处理时间统计
    std::cout << "Average total time: " << std::fixed << std::setprecision(2)
              << avg_total << " ms\n";
    if (auto_align_enabled) {
        std::cout << "  - Localization time: " << avg_loc << " ms\n";
        std::cout << "  - ROI comparison time: " << avg_roi << " ms\n";
    }
    std::cout << "----------------------------------------\n";

    // =================================================================================
    // 【第十一节：对齐统计报告】
    // 显示对齐偏移、旋转、缩放的统计信息
    // =================================================================================
    if (auto_align_enabled && align_success_count > 0) {
        std::cout << "Alignment Statistics:\n";
        std::cout << "  Success rate: " << align_success_count << "/" << process_count
                  << " (" << std::setprecision(1) << (100.0 * align_success_count / process_count) << "%)\n";
        std::cout << "  Offset (px):  min=" << std::setprecision(2) << min_offset
                  << ", max=" << max_offset
                  << ", avg=" << (total_offset / align_success_count) << "\n";
        std::cout << "  Rotation (deg): min=" << std::setprecision(3) << min_rotation
                  << ", max=" << max_rotation
                  << ", avg=" << (total_rotation / align_success_count) << "\n";
        std::cout << "  Scale:        min=" << std::setprecision(4) << min_scale
                  << ", max=" << max_scale
                  << ", avg=" << (total_scale / align_success_count) << "\n";
        std::cout << "----------------------------------------\n";
        
        // 【对齐调参建议】
        std::cout << "[Alignment Tuning Suggestions]\n";
        if (max_offset > 20.0f) {
            std::cout << "  - Large offset detected (>20px), alignment is working well\n";
            std::cout << "  - If results are poor, increase edge_tolerance_pixels\n";
        }
        if (max_rotation > 2.0f) {
            std::cout << "  - Large rotation detected (>2deg), ensure images are captured consistently\n";
        }
    } else if (!auto_align_enabled) {
        std::cout << "NOTE: Alignment disabled. Use --align-roi or --align-full to enable.\n";
        std::cout << "      Compare results with alignment to diagnose accuracy issues.\n";
        std::cout << "----------------------------------------\n";
    }

    // =================================================================================
    // 【第十二节：性能评估】
    // =================================================================================
    std::cout << "ROI Comparison Performance: " << (avg_roi <= 20.0 ? "PASS" : "FAIL")
              << " (target: <= 20ms per readme.md)\n";
    
    // 最终调参建议
    std::cout << "----------------------------------------\n";
    std::cout << "[Final Tuning Recommendations]\n";
    if (pass_count == 0) {
        std::cout << "  WARNING: All images failed!\n";
        std::cout << "  - Try increasing --sim-thresh (e.g., 0.95)\n";
        std::cout << "  - Try increasing --edge-tol (e.g., 5-6)\n";
        std::cout << "  - Check if template is representative\n";
        std::cout << "  - Verify alignment is working with --verbose\n";
    } else if (fail_count == 0) {
        std::cout << "  NOTE: All images passed!\n";
        std::cout << "  - If this is expected (all good samples), parameters are good\n";
        std::cout << "  - Test with known defective samples to verify detection\n";
        std::cout << "  - Consider lowering --sim-thresh for stricter detection\n";
    } else {
        std::cout << "  Mixed results - review failed images manually\n";
        std::cout << "  - Use --verbose to see detailed info\n";
        std::cout << "  - Adjust --sim-thresh based on false positive/negative rate\n";
    }
    std::cout << "========================================\n";

    return 0;
}
