/**
 * @file demo_cpp.cpp
 * @brief C#风格C++演示程序 - 带CSV定位数据版本
 *
 * 仿照C# demo的功能，使用C++实现：
 * - 从CSV文件读取定位点数据
 * - 使用外部变换进行图像对齐
 * - 批量图像检测
 * - 详细结果输出和可视化
 *
 * 使用方法:
 *   demo_cpp.exe --template <模板文件> --roi <x,y,width,height,threshold> [选项] [数据目录]
 *
 * 示例:
 *   demo_cpp.exe --template data/20260204/20260204/20260204_1348395638_Bar1_QR1_OCR0_Blob1_Loc1_Dis1_Com0.bmp --roi 200,420,820,320,0.8
 */

#include "defect_detector.h"
#include "visualizer.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <limits>  // for INT_MAX

#ifdef _WIN32
#include <windows.h>
#include <locale>
#endif

namespace fs = std::filesystem;
using namespace defect_detection;

// ========== 配置常量 ==========
// 支持中文文件名 - 使用宽字符版本
#ifdef _WIN32
const std::wstring CSV_FILENAME_W = L"匹配信息_116_20260304161622333.csv";
const std::string CSV_FILENAME = "匹配信息_116_20260304161622333.csv";
#else
const std::string CSV_FILENAME = "匹配信息_116_20260304161622333.csv";
#endif
const int BINARY_THRESHOLD = 30;           // 二值化阈值（用于图像二值化转换）
const int ALIGN_DIFF_THRESHOLD = 128;        // 对齐差异阈值（用于差异检测，越大越宽松）
const int DEFECT_SIZE_THRESHOLD = 100;      // 瑕疵尺寸阈值

// ========== 数据结构 ==========

struct ROIParams {
    int x, y, width, height;
    float threshold;
    
    std::string toString() const {
        std::stringstream ss;
        ss << "(" << x << "," << y << ") " << width << "x" << height << " threshold=" << std::fixed << std::setprecision(2) << threshold;
        return ss.str();
    }
};

struct LocationData {
    std::string filename;
    float x, y, rotation;
    bool isValid;
    
    LocationData() : x(0), y(0), rotation(0), isValid(false) {}
    
    std::string toString() const {
        if (!isValid) return "无效数据";
        std::stringstream ss;
        ss << "(" << std::fixed << std::setprecision(2) << x << ", " << y << ") 旋转=" << std::setprecision(3) << rotation << "°";
        return ss.str();
    }
};

struct ProgramConfig {
    // 必需参数
    std::string templateFile;
    ROIParams roi;
    bool hasROI = false;
    
    // 路径配置
    std::string dataDir;
    std::string outputDir = "output";
    std::string configFile = "config/visualization_config.json";  // 可视化配置文件
    std::string loadRoiFile = "";   // ROI加载文件
    std::string saveRoiFile = "";   // ROI保存文件
    
    // 检测参数
    int maxImages = INT_MAX;
    
    // 输出控制
    bool verbose = false;
    bool saveResults = false;
    bool vizEnabled = true;
    int displayDelayMs = 1500;
    OutputMode vizMode = OutputMode::BOTH;  // 可视化输出模式
    
    // 对齐可视化选项
    bool showAlignmentOverlay = true;  // 显示对齐叠加图
    int overlayMode = 0;               // 0=彩色叠加, 1=棋盘格, 2=半透明混合
    
    // 对齐参数
    bool autoAlignEnabled = true;      // 自动对齐使能
    AlignmentMode alignMode = AlignmentMode::ROI_ONLY;  // 对齐模式
    bool useCsvAlignment = true;       // 使用CSV数据进行对齐（禁用则使用自动对齐）
    
    // ROI参数
    int roiCount = 4;                  // ROI数量（随机模式）
    bool manualRoiMode = false;        // 手动ROI选择模式
    unsigned int randomSeed = 0;       // 随机种子（0表示时间种子）
    bool seedSpecified = false;        // 是否指定了种子
    int roiShrinkPixels = 5;           // ROI内缩像素数（避免边缘检测）
    
    // 二值图像优化参数
    bool binaryOptimizationEnabled = true;  // 二值优化总开关
    int binaryThreshold = 30;          // 二值化阈值（瑕疵检测用，建议30-50）
    int noiseFilterSize = 3;           // 噪声过滤尺寸（减小以避免过滤小瑕疵）
    int edgeTolerancePixels = 2;       // 边缘容错像素数（减小以避免过度过滤）
    bool edgeFilterEnabled = true;     // 边缘容错过滤开关（可禁用调试）
    float edgeDiffIgnoreRatio = 0.05f; // 边缘差异忽略比例
    int minSignificantArea = 20;       // 最小显著面积
    float areaDiffThreshold = 0.01f;  // 面积差异阈值
    float overallSimilarityThreshold = 0.90f;  // 总体相似度阈值
    
    // 通用检测参数
    int minDefectSize = 100;           // 最小瑕疵尺寸
    int blurKernelSize = 3;            // 高斯模糊核大小
    bool detectBlackOnWhite = true;    // 检测白底黑点
    bool detectWhiteOnBlack = true;    // 检测黑底白点
    
    // 边缘容错增强参数
    int edgeDefectSizeThreshold = 500; // 边缘区域大瑕疵保留阈值（px）
    float edgeDistanceMultiplier = 2.0f; // 边缘安全距离倍数（相对于edge_tol）
};

// ========== CSV文件读取 ==========

std::map<std::string, LocationData> loadLocationData(const std::string& dataDir) {
    std::map<std::string, LocationData> locationDict;
    
#ifdef _WIN32
    // Windows: 使用宽字符路径处理中文文件名
    std::wstring wDataDir(dataDir.begin(), dataDir.end());
    std::wstring wCsvPath = wDataDir + L"/" + CSV_FILENAME_W;
    std::string csvPath = dataDir + "/" + CSV_FILENAME;
    
    if (!fs::exists(wCsvPath)) {
        std::cout << "[警告] CSV文件不存在: " << csvPath << "\n";
        std::cout << "将使用默认值 (x=0, y=0, r=0)\n";
        return locationDict;
    }
    
    std::cout << "读取CSV文件: " << csvPath << "\n";
    int lineCount = 0;
    int validCount = 0;
    
    // 使用宽字符打开文件
    std::wifstream file(wCsvPath);
    if (!file.is_open()) {
        std::cerr << "[警告] 无法打开CSV文件: " << csvPath << "\n";
        return locationDict;
    }
    
    // 设置区域设置以正确处理中文（带异常保护）
    try {
        file.imbue(std::locale(""));
    } catch (const std::runtime_error&) {
        // 如果系统locale不可用，尝试使用经典locale
        try {
            file.imbue(std::locale::classic());
        } catch (...) {
            std::cerr << "[警告] 无法设置文件locale，CSV解析可能不正确\n";
        }
    } catch (...) {
        std::cerr << "[警告] 无法设置文件locale，CSV解析可能不正确\n";
    }
#else
    std::string csvPath = dataDir + "/" + CSV_FILENAME;
    
    if (!fs::exists(csvPath)) {
        std::cout << "[警告] CSV文件不存在: " << csvPath << "\n";
        std::cout << "将使用默认值 (x=0, y=0, r=0)\n";
        return locationDict;
    }
    
    std::cout << "读取CSV文件: " << csvPath << "\n";
    int lineCount = 0;
    int validCount = 0;
    
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        std::cerr << "[警告] 无法打开CSV文件: " << csvPath << "\n";
        return locationDict;
    }
#endif
    
#ifdef _WIN32
    // Windows: 从 wifstream 读取宽字符行并转换为窄字符
    std::wstring wline;
    std::getline(file, wline); // 跳过标题行
    
    // 宽字符转窄字符辅助函数
    auto wstringToString = [](const std::wstring& ws) -> std::string {
        if (ws.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &str[0], size_needed, NULL, NULL);
        return str;
    };
    
    while (std::getline(file, wline)) {
        lineCount++;
        std::string line = wstringToString(wline);
#else
    std::string line;
    std::getline(file, line); // 跳过标题行
    
    while (std::getline(file, line)) {
        lineCount++;
#endif
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) s = "";
            else s = s.substr(start, end - start + 1);
        };
        trim(line);
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(ss, part, ',')) {
            trim(part);
            parts.push_back(part);
        }
        
        if (parts.size() >= 4) {
            LocationData data;
            data.filename = parts[0];
            data.isValid = !parts[1].empty() && !parts[2].empty() && !parts[3].empty();
            
            if (data.isValid) {
                try {
                    data.x = std::stof(parts[1]);
                    data.y = std::stof(parts[2]);
                    data.rotation = std::stof(parts[3]);
                    validCount++;
                } catch (...) {
                    data.isValid = false;
                }
            }
            locationDict[data.filename] = data;
        }
    }
    
    file.close();
    std::cout << "  - 总记录数: " << lineCount << "\n";
    std::cout << "  - 有效记录: " << validCount << "\n";
    std::cout << "  - 无效记录: " << (lineCount - validCount) << "\n\n";
    
    return locationDict;
}

LocationData getLocationData(const std::map<std::string, LocationData>& locationDict,
                             const std::string& filename, bool verbose) {
    std::string shortFilename = fs::path(filename).filename().string();
    
    auto it = locationDict.find(shortFilename);
    if (it != locationDict.end()) {
        if (verbose) {
            std::cout << "    定位数据: " << it->second.toString() << "\n";
        }
        return it->second;
    }
    
    if (verbose) {
        std::cout << "    定位数据: 未找到，使用默认值 (0,0,0)\n";
    }
    
    LocationData defaultData;
    defaultData.filename = shortFilename;
    return defaultData;
}

// ========== 命令行解析 ==========

void printUsage(const char* prog) {
    std::cout << "\n用法: " << prog << " --template <模板文件> --roi <x,y,w,h,t> [选项] [数据目录]\n\n";
    std::cout << "必需参数:\n";
    std::cout << "  --template <file>   指定模板文件路径\n";
    std::cout << "  --roi <x,y,w,h,t>   指定ROI参数 (例如: --roi 200,420,820,320,0.8)\n\n";
    std::cout << "基础选项:\n";
    std::cout << "  --help              显示此帮助信息\n";
    std::cout << "  --output <dir>      指定输出目录 (默认: output)\n";
    std::cout << "  --max-images <n>    最大处理图像数量 (默认: 全部)\n";
    std::cout << "  --verbose, -v       显示详细信息\n";
    std::cout << "  --save              保存检测结果图像\n\n";
    
    std::cout << "可视化选项:\n";
    std::cout << "  --no-viz            禁用可视化\n";
    std::cout << "  --viz-file          只输出到文件\n";
    std::cout << "  --viz-display       只显示窗口\n";
    std::cout << "  --delay <ms>        显示延迟 (默认: 1500ms)\n";
    std::cout << "  --config <file>     加载可视化配置文件\n";
    std::cout << "  --no-align-viz      禁用对齐叠加可视化\n";
    std::cout << "  --overlay-mode <n>  叠加模式: 0=彩色叠加(默认), 1=棋盘格, 2=半透明混合\n\n";
    
    std::cout << "对齐选项:\n";
    std::cout << "  --no-align          禁用自动对齐\n";
    std::cout << "  --align-full        使用全图对齐模式（更精确但更慢）\n";
    std::cout << "  --align-roi         使用ROI对齐模式（更快，默认）\n";
    std::cout << "  --no-csv            禁用CSV对齐，使用自动对齐\n\n";
    
    std::cout << "ROI选项:\n";
    std::cout << "  --manual-roi        启用手动ROI选择模式\n";
    std::cout << "  --load-roi <file>   从JSON文件加载ROI配置\n";
    std::cout << "  --save-roi <file>   保存ROI配置到JSON文件\n";
    std::cout << "  --roi-count <num>   设置ROI数量（随机模式，默认: 4）\n";
    std::cout << "  --roi-shrink <px>   ROI内缩像素数（避免检测边缘，默认: 5）\n";
    std::cout << "  --seed <num>        设置随机种子（用于ROI位置）\n\n";
    
    std::cout << "二值图像优化参数:\n";
    std::cout << "  --no-binary-opt     禁用二值图像优化\n";
    std::cout << "  --binary-thresh <n> 二值化阈值 (默认: 30)\n";
    std::cout << "  --edge-tol <px>     边缘容错像素数 (默认: 2)\n";
    std::cout << "  --no-edge-filter    禁用边缘容错过滤（调试用）\n";
    std::cout << "  --noise-size <px>   噪声过滤尺寸 (默认: 3)\n";
    std::cout << "  --min-area <px>     最小显著面积 (默认: 20)\n";
    std::cout << "  --sim-thresh <f>    总体相似度阈值 (默认: 0.90)\n\n";
    
    std::cout << "通用检测参数:\n";
    std::cout << "  --min-defect <n>    最小瑕疵尺寸 (默认: 100)\n";
    std::cout << "  --blur-size <n>     高斯模糊核大小 (默认: 3)\n";
    std::cout << "  --no-black-detect   禁用白底黑点检测\n";
    std::cout << "  --no-white-detect   禁用黑底白点检测\n";
    std::cout << "  --edge-defect-size <n> 边缘区域大瑕疵保留阈值 (默认: 500)\n";
    std::cout << "  --edge-dist-mult <f>   边缘安全距离倍数 (默认: 2.0)\n\n";
    
    std::cout << "示例:\n";
    std::cout << "  " << prog << " --template template.bmp --roi 200,420,820,320,0.8 data/20260204/20260204\n";
    std::cout << "  " << prog << " --template template.bmp --roi 200,420,820,320,0.8 --edge-tol 6 --noise-size 15\n";
}

ROIParams parseROI(const std::string& roiStr) {
    std::stringstream ss(roiStr);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(ss, part, ',')) {
        parts.push_back(part);
    }
    
    if (parts.size() != 5) {
        throw std::invalid_argument("ROI参数格式错误，应为: x,y,width,height,threshold");
    }
    
    try {
        ROIParams roi;
        roi.x = std::stoi(parts[0]);
        roi.y = std::stoi(parts[1]);
        roi.width = std::stoi(parts[2]);
        roi.height = std::stoi(parts[3]);
        roi.threshold = std::stof(parts[4]);
        
        // 验证ROI参数有效性
        if (roi.width <= 0 || roi.height <= 0) {
            throw std::invalid_argument("ROI宽度和高度必须大于0");
        }
        if (roi.threshold < 0.0f || roi.threshold > 1.0f) {
            throw std::invalid_argument("ROI阈值必须在0.0到1.0之间");
        }
        
        return roi;
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("ROI参数解析失败: ") + e.what());
    } catch (const std::exception& e) {
        throw std::invalid_argument(std::string("ROI参数解析失败: ") + e.what());
    }
}

ProgramConfig parseArguments(int argc, char** argv) {
    ProgramConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        } else if (arg == "--template" && i + 1 < argc) {
            config.templateFile = argv[++i];
        } else if (arg == "--roi" && i + 1 < argc) {
            config.roi = parseROI(argv[++i]);
            config.hasROI = true;
        } else if (arg == "--output" && i + 1 < argc) {
            config.outputDir = argv[++i];
        } else if (arg == "--max-images" && i + 1 < argc) {
            config.maxImages = std::stoi(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg == "--save") {
            config.saveResults = true;
        } else if (arg == "--no-viz") {
            config.vizEnabled = false;
        } else if (arg == "--delay" && i + 1 < argc) {
            config.displayDelayMs = std::stoi(argv[++i]);
        } else if (arg == "--no-align-viz") {
            config.showAlignmentOverlay = false;
        } else if (arg == "--overlay-mode" && i + 1 < argc) {
            config.overlayMode = std::stoi(argv[++i]);
        }
        // 可视化选项
        else if (arg == "--viz-file") {
            config.vizMode = OutputMode::FILE_ONLY;
        } else if (arg == "--viz-display") {
            config.vizMode = OutputMode::DISPLAY_ONLY;
        } else if (arg == "--config" && i + 1 < argc) {
            config.configFile = argv[++i];
        }
        // 对齐选项
        else if (arg == "--no-align") {
            config.autoAlignEnabled = false;
            config.alignMode = AlignmentMode::NONE;
        } else if (arg == "--align-full") {
            config.alignMode = AlignmentMode::FULL_IMAGE;
        } else if (arg == "--align-roi") {
            config.alignMode = AlignmentMode::ROI_ONLY;
        } else if (arg == "--no-csv") {
            config.useCsvAlignment = false;
        }
        // ROI选项
        else if (arg == "--manual-roi") {
            config.manualRoiMode = true;
        } else if (arg == "--load-roi" && i + 1 < argc) {
            config.loadRoiFile = argv[++i];
        } else if (arg == "--save-roi" && i + 1 < argc) {
            config.saveRoiFile = argv[++i];
        } else if (arg == "--roi-count" && i + 1 < argc) {
            config.roiCount = std::stoi(argv[++i]);
            if (config.roiCount < 1) config.roiCount = 1;
            if (config.roiCount > 20) config.roiCount = 20;
        } else if (arg == "--roi-shrink" && i + 1 < argc) {
            config.roiShrinkPixels = std::stoi(argv[++i]);
            if (config.roiShrinkPixels < 0) config.roiShrinkPixels = 0;
            if (config.roiShrinkPixels > 50) config.roiShrinkPixels = 50;
        } else if (arg == "--seed" && i + 1 < argc) {
            config.randomSeed = static_cast<unsigned int>(std::stoul(argv[++i]));
            config.seedSpecified = true;
        }
        // 二值图像优化参数
        else if (arg == "--no-binary-opt") {
            config.binaryOptimizationEnabled = false;
        } else if (arg == "--binary-thresh" && i + 1 < argc) {
            config.binaryThreshold = std::stoi(argv[++i]);
        } else if (arg == "--edge-tol" && i + 1 < argc) {
            config.edgeTolerancePixels = std::stoi(argv[++i]);
        } else if (arg == "--no-edge-filter") {
            config.edgeFilterEnabled = false;
        } else if (arg == "--noise-size" && i + 1 < argc) {
            config.noiseFilterSize = std::stoi(argv[++i]);
        } else if (arg == "--min-area" && i + 1 < argc) {
            config.minSignificantArea = std::stoi(argv[++i]);
        } else if (arg == "--sim-thresh" && i + 1 < argc) {
            config.overallSimilarityThreshold = std::stof(argv[++i]);
        }
        // 通用检测参数
        else if (arg == "--min-defect" && i + 1 < argc) {
            config.minDefectSize = std::stoi(argv[++i]);
        } else if (arg == "--blur-size" && i + 1 < argc) {
            config.blurKernelSize = std::stoi(argv[++i]);
        } else if (arg == "--no-black-detect") {
            config.detectBlackOnWhite = false;
        } else if (arg == "--no-white-detect") {
            config.detectWhiteOnBlack = false;
        } else if (arg == "--edge-defect-size" && i + 1 < argc) {
            config.edgeDefectSizeThreshold = std::stoi(argv[++i]);
        } else if (arg == "--edge-dist-mult" && i + 1 < argc) {
            config.edgeDistanceMultiplier = std::stof(argv[++i]);
        }
        // 数据目录
        else if (arg[0] != '-') {
            config.dataDir = arg;
        }
    }
    
    // 验证必需参数
    if (config.templateFile.empty()) {
        std::cerr << "[错误] 必须指定模板文件 (--template)\n";
        printUsage(argv[0]);
        exit(1);
    }
    
    if (!config.hasROI) {
        std::cerr << "[错误] 必须指定ROI参数 (--roi)\n";
        printUsage(argv[0]);
        exit(1);
    }
    
    if (!fs::exists(config.templateFile)) {
        throw std::runtime_error("模板文件不存在: " + config.templateFile);
    }
    
    // 如果没有指定数据目录，使用模板文件所在目录
    if (config.dataDir.empty()) {
        config.dataDir = fs::path(config.templateFile).parent_path().string();
        if (config.dataDir.empty()) config.dataDir = ".";
    }
    
    return config;
}

// ========== 对齐叠加可视化 ==========

/**
 * @brief 将图像转换为三通道二值图像（用于可视化）
 * @param src 输入图像（灰度或彩色）
 * @param threshold 二值化阈值
 * @return 三通道二值图像
 */
cv::Mat toBinary3Channel(const cv::Mat& src, int threshold = 128) {
    cv::Mat gray, binary;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src.clone();
    }
    cv::threshold(gray, binary, threshold, 255, cv::THRESH_BINARY);
    cv::Mat binary3C;
    cv::cvtColor(binary, binary3C, cv::COLOR_GRAY2BGR);
    return binary3C;
}

/**
 * @brief 创建对齐后的叠加图像（使用二值化图像），用于检查对齐准确性
 * @param templateImg 模板图像
 * @param testImg 测试图像
 * @param offsetX X方向偏移
 * @param offsetY Y方向偏移
 * @param rotation 旋转角度（度）
 * @param binaryThreshold 二值化阈值
 * @return 叠加后的可视化图像
 */
cv::Mat createAlignedOverlay(const cv::Mat& templateImg, const cv::Mat& testImg,
                             float offsetX, float offsetY, float rotation, 
                             int binaryThreshold = 128) {
    // 确保测试图像尺寸一致
    cv::Mat testResized;
    if (testImg.size() != templateImg.size()) {
        cv::resize(testImg, testResized, templateImg.size());
    } else {
        testResized = testImg.clone();
    }
    
    // 创建变换矩阵（反向变换，将测试图像对齐到模板）
    cv::Mat transform = cv::getRotationMatrix2D(
        cv::Point2f(templateImg.cols / 2.0f, templateImg.rows / 2.0f),
        -rotation,
        1.0
    );
    
    // 添加平移（反向）
    transform.at<double>(0, 2) -= offsetX;
    transform.at<double>(1, 2) -= offsetY;
    
    // 应用变换到测试图像
    cv::Mat alignedTest;
    cv::warpAffine(testResized, alignedTest, transform, templateImg.size(),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    
    // 转换为灰度图用于比较
    cv::Mat templateGray, alignedGray;
    if (templateImg.channels() == 3) {
        cv::cvtColor(templateImg, templateGray, cv::COLOR_BGR2GRAY);
    } else {
        templateGray = templateImg.clone();
    }
    if (alignedTest.channels() == 3) {
        cv::cvtColor(alignedTest, alignedGray, cv::COLOR_BGR2GRAY);
    } else {
        alignedGray = alignedTest.clone();
    }
    
    // 计算差异图（绝对差值）
    cv::Mat diff;
    cv::absdiff(templateGray, alignedGray, diff);
    
    // 创建二值掩码：差异大于阈值的区域为未对齐区域
    cv::Mat misalignedMask;
    cv::threshold(diff, misalignedMask, binaryThreshold, 255, cv::THRESH_BINARY);
    
    // 形态学操作：膨胀掩码以突出显示未对齐区域
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::dilate(misalignedMask, misalignedMask, kernel);
    
    // 创建结果图像：基础是原始灰度图像
    cv::Mat result;
    cv::cvtColor(templateGray, result, cv::COLOR_GRAY2BGR);
    
    // 纯深红色（BGR格式）：B=0, G=0, R=220（完全不透明的深红色）
    cv::Scalar pureDarkRed(0, 0, 220);
    
    // 确保掩码是单通道二值图像（0或255）
    // 使用copyTo实现完全不透明的覆盖效果
    cv::Mat redOverlay(result.size(), result.type(), pureDarkRed);
    redOverlay.copyTo(result, misalignedMask);
    
    return result;
}

/**
 * @brief 创建棋盘格叠加模式，交替显示模板和对齐后的测试图像
 */
cv::Mat createCheckerboardOverlay(const cv::Mat& templateImg, const cv::Mat& testImg,
                                  float offsetX, float offsetY, float rotation,
                                  int checkerSize = 32, int binaryThreshold = 128) {
    // 转换为二值图像
    cv::Mat templateBinary = toBinary3Channel(templateImg, binaryThreshold);
    
    // 对齐测试图像
    cv::Mat testResized;
    if (testImg.size() != templateImg.size()) {
        cv::resize(testImg, testResized, templateImg.size());
    } else {
        testResized = testImg.clone();
    }
    
    // 创建变换矩阵（反向变换）
    cv::Mat transform = cv::getRotationMatrix2D(
        cv::Point2f(templateImg.cols / 2.0f, templateImg.rows / 2.0f),
        -rotation,
        1.0
    );
    transform.at<double>(0, 2) -= offsetX;
    transform.at<double>(1, 2) -= offsetY;
    
    cv::Mat alignedTest;
    cv::warpAffine(testResized, alignedTest, transform, templateImg.size(),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    
    // 将变换后的测试图像转换为二值图像
    cv::Mat alignedBinary = toBinary3Channel(alignedTest, binaryThreshold);
    
    // 创建棋盘格掩码
    cv::Mat checkerboard(templateImg.size(), CV_8UC3);
    
    for (int y = 0; y < templateImg.rows; y++) {
        for (int x = 0; x < templateImg.cols; x++) {
            int checkerX = x / checkerSize;
            int checkerY = y / checkerSize;
            if ((checkerX + checkerY) % 2 == 0) {
                checkerboard.at<cv::Vec3b>(y, x) = templateBinary.at<cv::Vec3b>(y, x);
            } else {
                checkerboard.at<cv::Vec3b>(y, x) = alignedBinary.at<cv::Vec3b>(y, x);
            }
        }
    }
    
    return checkerboard;
}

/**
 * @brief 创建半透明混合叠加
 */
cv::Mat createBlendedOverlay(const cv::Mat& templateImg, const cv::Mat& testImg,
                             float offsetX, float offsetY, float rotation,
                             float alpha = 0.5, int binaryThreshold = 128) {
    // 转换为二值图像
    cv::Mat templateBinary = toBinary3Channel(templateImg, binaryThreshold);
    
    // 对齐测试图像
    cv::Mat testResized;
    if (testImg.size() != templateImg.size()) {
        cv::resize(testImg, testResized, templateImg.size());
    } else {
        testResized = testImg.clone();
    }
    
    // 创建变换矩阵（反向变换）
    cv::Mat transform = cv::getRotationMatrix2D(
        cv::Point2f(templateImg.cols / 2.0f, templateImg.rows / 2.0f),
        -rotation,
        1.0
    );
    transform.at<double>(0, 2) -= offsetX;
    transform.at<double>(1, 2) -= offsetY;
    
    cv::Mat alignedTest;
    cv::warpAffine(testResized, alignedTest, transform, templateImg.size(),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    
    // 将变换后的测试图像转换为二值图像
    cv::Mat alignedBinary = toBinary3Channel(alignedTest, binaryThreshold);
    
    // 创建半透明混合
    cv::Mat blended;
    cv::addWeighted(templateBinary, alpha, alignedBinary, 1.0 - alpha, 0, blended);
    
    return blended;
}

// ========== 辅助方法 ==========

void printHeader(const ProgramConfig& config) {
    std::cout << "============================================================\n";
    std::cout << "   喷印瑕疵检测系统 - 带CSV定位数据版本\n";
    std::cout << "   Print Defect Detection System - CSV Location Version\n";
    std::cout << "============================================================\n";
    std::cout << "模板文件: " << config.templateFile << "\n";
    std::cout << "ROI参数: " << config.roi.toString() << "\n";
    std::cout << "数据目录: " << config.dataDir << "\n";
    std::cout << "输出目录: " << config.outputDir << "\n";
    std::cout << "============================================================\n\n";
}

std::vector<std::string> getImageFiles(const ProgramConfig& config, const std::string& templateFilename) {
    std::vector<std::string> supportedExts = {".bmp", ".jpg", ".jpeg", ".png"};
    std::vector<std::string> allFiles;
    
    for (const auto& entry : fs::directory_iterator(config.dataDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (std::find(supportedExts.begin(), supportedExts.end(), ext) != supportedExts.end()) {
                allFiles.push_back(entry.path().string());
            }
        }
    }
    std::sort(allFiles.begin(), allFiles.end());
    
    // 排除模板文件
    std::string templateLower = templateFilename;
    std::transform(templateLower.begin(), templateLower.end(), templateLower.begin(), ::tolower);
    
    std::vector<std::string> imageFiles;
    for (const auto& f : allFiles) {
        std::string filename = fs::path(f).filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        if (filename != templateLower) {
            imageFiles.push_back(f);
        }
    }
    
    if (imageFiles.empty()) {
        throw std::runtime_error("数据目录中除了模板外没有其他图像文件");
    }
    
    // 限制处理数量
    if (config.maxImages < (int)imageFiles.size()) {
        std::cout << "限制处理数量: " << config.maxImages << " 张 (共 " << imageFiles.size() << " 张)\n";
        imageFiles.resize(config.maxImages);
    }
    
    return imageFiles;
}

void printResultLine(int index, int total, const std::string& filename, bool passed,
                     int passedRois, int totalRois, float minSimilarity, double processingTime,
                     float offsetX, float offsetY, float rotationDiff) {
    // 截断过长的文件名
    std::string displayName = filename;
    if (displayName.length() > 30) {
        displayName = displayName.substr(0, 27) + "...";
    }
    
    // 设置颜色
    if (passed) {
        std::cout << "\033[32m";  // 绿色
    } else {
        std::cout << "\033[31m";  // 红色
    }
    
    std::cout << "[" << std::setw(3) << index << "/" << total << "] " 
              << std::left << std::setw(30) << displayName
              << " | " << (passed ? "PASS" : "FAIL");
    std::cout << "\033[0m";  // 重置颜色
    
    std::cout << " | " << passedRois << "/" << totalRois
              << " | sim:" << std::fixed << std::setprecision(1) << (minSimilarity * 100) << "%"
              << " | Δ(" << std::setw(6) << std::setprecision(1) << offsetX << ","
              << std::setw(6) << offsetY << ")"
              << " | Δθ" << std::setw(6) << std::setprecision(3) << rotationDiff << "°"
              << " | " << std::setw(6) << std::setprecision(1) << processingTime << "ms\n";
}

void printVerboseInfo(const std::vector<ROIResult>& roiResults) {
    std::cout << "    └─ ROIs: ";
    for (size_t r = 0; r < roiResults.size(); ++r) {
        const auto& roi = roiResults[r];
        if (roi.passed) {
            std::cout << "\033[32m✓\033[0m";
        } else {
            std::cout << "\033[31m✗\033[0m";
        }
        std::cout << std::fixed << std::setprecision(0) << (roi.similarity * 100) << "%";
        if (r < roiResults.size() - 1) std::cout << " ";
    }
    std::cout << "\n";
}

void printSummaryReport(const ProgramConfig& config, int totalImages,
                        int passCount, int failCount, double totalTime,
                        int validLocationCount, float minOffset, float maxOffset, float totalOffset,
                        float minRotationDiff, float maxRotationDiff, float totalRotationDiff) {
    double avgTime = totalTime / totalImages;
    
    std::cout << "\n============================================================\n";
    std::cout << "                   统计报告 Summary\n";
    std::cout << "============================================================\n";
    std::cout << "模板文件: " << fs::path(config.templateFile).filename().string() << "\n";
    std::cout << "ROI区域: " << config.roi.toString() << "\n";
    std::cout << "----------------------------------------\n";
    std::cout << "处理图像数: " << totalImages << "\n";
    std::cout << "\033[32m通过: " << passCount << "\033[0m\n";
    std::cout << "\033[31m失败: " << failCount << "\033[0m\n";
    
    double passRate = 100.0 * passCount / totalImages;
    std::cout << "通过率: " << std::fixed << std::setprecision(1) << passRate << "%\n";
    std::cout << "平均耗时: " << std::fixed << std::setprecision(2) << avgTime << " ms\n";
    std::cout << "----------------------------------------\n";
    
    std::cout << "定位数据:\n";
    std::cout << "  有效记录: " << validLocationCount << "/" << totalImages 
              << " (" << std::fixed << std::setprecision(1) << (100.0 * validLocationCount / totalImages) << "%)\n";
    
    if (validLocationCount > 0) {
        std::cout << "  偏移 (px):  min=" << std::fixed << std::setprecision(2) << minOffset
                  << ", max=" << maxOffset
                  << ", avg=" << (totalOffset / validLocationCount) << "\n";
        std::cout << "  旋转差 (°): min=" << std::fixed << std::setprecision(3) << minRotationDiff
                  << ", max=" << maxRotationDiff
                  << ", avg=" << (totalRotationDiff / validLocationCount) << "\n";
    } else {
        std::cout << "  所有图片使用默认值 (0,0,0)\n";
    }
    
    std::cout << "============================================================\n";
}

// ========== 核心检测流程 ==========

void configureDetector(DefectDetector& detector, const ProgramConfig& config) {
    std::cout << "Step 2: 配置检测参数...\n";
    
    // 设置匹配方法为二值模式
    detector.getTemplateMatcher().setMethod(MatchMethod::BINARY);
    detector.getTemplateMatcher().setBinaryThreshold(config.binaryThreshold);
    
    // 配置二值检测参数
    BinaryDetectionParams binaryParams;
    binaryParams.enabled = config.binaryOptimizationEnabled;
    binaryParams.auto_detect_binary = true;
    binaryParams.noise_filter_size = config.noiseFilterSize;
    // 如果禁用边缘过滤，将容错像素设为0
    binaryParams.edge_tolerance_pixels = config.edgeFilterEnabled ? config.edgeTolerancePixels : 0;
    binaryParams.edge_diff_ignore_ratio = config.edgeDiffIgnoreRatio;
    binaryParams.min_significant_area = config.minSignificantArea;
    binaryParams.area_diff_threshold = config.areaDiffThreshold;
    binaryParams.overall_similarity_threshold = config.overallSimilarityThreshold;
    binaryParams.edge_defect_size_threshold = config.edgeDefectSizeThreshold;
    binaryParams.edge_distance_multiplier = config.edgeDistanceMultiplier;
    binaryParams.binary_threshold = config.binaryThreshold;
    detector.setBinaryDetectionParams(binaryParams);
    
    // 设置其他参数
    detector.setParameter("min_defect_size", static_cast<float>(config.minDefectSize));
    detector.setParameter("blur_kernel_size", static_cast<float>(config.blurKernelSize));
    detector.setParameter("detect_black_on_white", config.detectBlackOnWhite ? 1.0f : 0.0f);
    detector.setParameter("detect_white_on_black", config.detectWhiteOnBlack ? 1.0f : 0.0f);
    
    std::cout << "  匹配方法: BINARY (二值图像)\n";
    std::cout << "  二值化阈值: " << config.binaryThreshold << "\n";
    
    if (config.binaryOptimizationEnabled) {
        std::cout << "  *** 二值图像优化模式 ***\n";
        std::cout << "  二值图像优化: 已启用\n";
        std::cout << "    - noise_filter_size = " << config.noiseFilterSize << " px\n";
        std::cout << "    - edge_tolerance_pixels = " << config.edgeTolerancePixels << " px\n";
        std::cout << "    - min_significant_area = " << config.minSignificantArea << " px\n";
        std::cout << "    - overall_similarity_threshold = " << std::fixed << std::setprecision(0) 
                  << (config.overallSimilarityThreshold * 100) << "%\n";
        std::cout << "    - edge_defect_size_threshold = " << config.edgeDefectSizeThreshold << " px\n";
        std::cout << "    - edge_distance_multiplier = " << config.edgeDistanceMultiplier << "\n";
        
        // 验证参数是否正确传递
        const auto& verifyParams = detector.getTemplateMatcher().getBinaryDetectionParams();
        std::cout << "  [调试] 模板匹配器参数验证:\n";
        std::cout << "    - enabled = " << (verifyParams.enabled ? "true" : "false") << "\n";
        std::cout << "    - edge_tolerance_pixels = " << verifyParams.edge_tolerance_pixels << " px\n";
    } else {
        std::cout << "  二值图像优化: 已禁用\n";
    }
    
    std::cout << "  min_defect_size = " << config.minDefectSize << "\n";
    std::cout << "  blur_kernel_size = " << config.blurKernelSize << "\n";
    std::cout << "  detect_black_on_white = " << (config.detectBlackOnWhite ? "true" : "false") << "\n";
    std::cout << "  detect_white_on_black = " << (config.detectWhiteOnBlack ? "true" : "false") << "\n\n";
}

void setupSingleROI(DefectDetector& detector, const ProgramConfig& config) {
    std::cout << "Step 3: 设置ROI区域...\n";
    
    // ROI内缩（Shrink）- 避免检测边缘区域，减少对齐误差导致的边缘误报
    int shrinkPixels = config.roiShrinkPixels;
    int x = config.roi.x + shrinkPixels;
    int y = config.roi.y + shrinkPixels;
    int width = config.roi.width - 2 * shrinkPixels;
    int height = config.roi.height - 2 * shrinkPixels;
    
    // 确保ROI尺寸有效
    if (width <= 0 || height <= 0) {
        std::cout << "  [警告] ROI内缩后尺寸无效，使用原始ROI\n";
        x = config.roi.x;
        y = config.roi.y;
        width = config.roi.width;
        height = config.roi.height;
        shrinkPixels = 0;
    }
    
    // 创建DetectionParams并设置二值化阈值
    DetectionParams params;
    params.binary_threshold = config.binaryThreshold;
    params.blur_kernel_size = config.blurKernelSize;
    params.min_defect_size = config.minDefectSize;
    params.detect_black_on_white = config.detectBlackOnWhite;
    params.detect_white_on_black = config.detectWhiteOnBlack;
    
    // 使用ROIManager添加ROI并传递参数
    int roiId = detector.getROIManager().addROI(
        defect_detection::Rect(x, y, width, height), 
        config.roi.threshold, 
        params);
    
    if (shrinkPixels > 0) {
        std::cout << "  原始ROI: (" << config.roi.x << "," << config.roi.y << ") " 
                  << config.roi.width << "x" << config.roi.height << "\n";
        std::cout << "  内缩后ROI: (" << x << "," << y << ") " 
                  << width << "x" << height << " (内缩 " << shrinkPixels << "px)\n";
    } else {
        std::cout << "  ROI: (" << x << "," << y << ") " 
                  << width << "x" << height << "\n";
    }
    std::cout << "  ROI " << roiId << ": 阈值=" << config.roi.threshold << "\n";
    std::cout << "  总计: " << detector.getROICount() << " 个ROI\n";
    
    // 提取ROI模板图像（关键！否则模板图像为空，无法检测）
    detector.getROIManager().extractTemplates(detector.getTemplate());
    std::cout << "  已提取ROI模板图像\n\n";
}

void configureAlignment(DefectDetector& detector, const ProgramConfig& config) {
    std::cout << "Step 4: 配置对齐...\n";
    
    // 配置自动对齐
    detector.enableAutoLocalization(config.autoAlignEnabled);
    detector.setAlignmentMode(config.alignMode);
    
    if (config.useCsvAlignment) {
        std::cout << "  对齐方式: CSV外部数据（优先）+ 自动对齐（后备）\n";
    } else {
        std::cout << "  对齐方式: 自动对齐（CSV已禁用）\n";
    }
    
    if (config.autoAlignEnabled) {
        std::string modeStr;
        switch (config.alignMode) {
            case AlignmentMode::NONE: modeStr = "NONE"; break;
            case AlignmentMode::FULL_IMAGE: modeStr = "FULL_IMAGE (全图对齐)"; break;
            case AlignmentMode::ROI_ONLY: modeStr = "ROI_ONLY (ROI区域对齐)"; break;
        }
        std::cout << "  自动对齐模式: " << modeStr << "\n";
        if (config.useCsvAlignment) {
            std::cout << "  注意: 当CSV数据可用时将优先使用CSV数据对齐\n\n";
        } else {
            std::cout << "  注意: 纯自动对齐模式，CSV数据将被忽略\n\n";
        }
    } else {
        if (config.useCsvAlignment) {
            std::cout << "  自动对齐: 已禁用（完全依赖CSV数据）\n";
            std::cout << "  警告: 无CSV数据时将不进行对齐！\n\n";
        } else {
            std::cout << "  自动对齐: 已禁用\n";
            std::cout << "  警告: 无对齐数据，检测可能不准确！\n\n";
        }
    }
}

void processImages(DefectDetector& detector, Visualizer& visualizer,
                   const std::vector<std::string>& imageFiles,
                   const std::map<std::string, LocationData>& locationDict,
                   const LocationData& templateLocation,
                   const ProgramConfig& config) {
    std::cout << "Step 5: 处理测试图像...\n";
    std::cout << "----------------------------------------\n";
    
    // 统计变量
    int passCount = 0;
    int failCount = 0;
    double totalTime = 0;
    
    // 偏移统计
    float maxOffset = 0, minOffset = 999999.0f, totalOffset = 0;
    float maxRotationDiff = 0, minRotationDiff = 999999.0f, totalRotationDiff = 0;
    int validLocationCount = 0;
    
    // 获取ROI列表用于可视化
    std::vector<ROIRegion> rois;
    for (int r = 0; r < detector.getROICount(); ++r) {
        rois.push_back(detector.getROI(r));
    }
    
    // 处理每张图像
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        const std::string& testFile = imageFiles[i];
        std::string filename = fs::path(testFile).filename().string();
        
        try {
            float offsetX = 0, offsetY = 0, rotationDiff = 0;
            
            if (config.useCsvAlignment) {
                // 获取当前图片的定位数据
                auto testLocation = getLocationData(locationDict, filename, config.verbose);
                
                // 计算与模板的差值
                offsetX = testLocation.isValid ? testLocation.x - templateLocation.x : 0;
                offsetY = testLocation.isValid ? testLocation.y - templateLocation.y : 0;
                rotationDiff = testLocation.isValid ? testLocation.rotation - templateLocation.rotation : 0;
                
                if (testLocation.isValid) {
                    validLocationCount++;
                    
                    float offset = std::sqrt(offsetX * offsetX + offsetY * offsetY);
                    maxOffset = (std::max)(maxOffset, offset);
                    minOffset = (std::min)(minOffset, offset);
                    totalOffset += offset;
                    
                    float absRotation = std::abs(rotationDiff);
                    maxRotationDiff = (std::max)(maxRotationDiff, absRotation);
                    minRotationDiff = (std::min)(minRotationDiff, absRotation);
                    totalRotationDiff += absRotation;
                }
                
                // 设置外部变换（使用CSV数据进行对齐）
                if (testLocation.isValid) {
                    detector.setExternalTransform(offsetX, offsetY, rotationDiff);
                    if (config.verbose) {
                        std::cout << "  使用CSV数据对齐: 偏移(" << std::fixed << std::setprecision(2) 
                                  << offsetX << ", " << offsetY << ")px, 旋转" << std::setprecision(3) 
                                  << rotationDiff << "°\n";
                    }
                } else {
                    // 无CSV数据时清除外部变换，使用自动对齐
                    detector.clearExternalTransform();
                    if (config.verbose) {
                        std::cout << "  无CSV定位数据，使用自动对齐\n";
                    }
                }
            } else {
                // CSV对齐已禁用，使用自动对齐
                detector.clearExternalTransform();
                if (config.verbose) {
                    std::cout << "  使用自动对齐（CSV已禁用）\n";
                }
            }
            
            // 执行检测
            DetectionResult result = detector.detectFromFile(testFile);
            totalTime += result.processing_time_ms;
            
            // 调试输出：显示检测到的瑕疵数量
            if (config.verbose) {
                int totalDefects = 0;
                for (const auto& roiResult : result.roi_results) {
                    totalDefects += static_cast<int>(roiResult.defects.size());
                }
                std::cout << "  检测到瑕疵: " << totalDefects << " 个\n";
            }
            
            // 获取ROI结果
            int passedROIs = 0;
            float minSimilarity = 1.0f;
            int zeroSimilarityCount = 0;
            for (const auto& roiResult : result.roi_results) {
                if (roiResult.passed) passedROIs++;
                if (roiResult.similarity < minSimilarity) minSimilarity = roiResult.similarity;
                if (roiResult.similarity < 1e-6f) zeroSimilarityCount++;
            }
            
            // 瑕疵检测判定
            bool hasSignificantDefects = false;
            std::vector<std::string> defectInfoList;
            
            for (const auto& roiResult : result.roi_results) {
                for (const auto& defect : roiResult.defects) {
                    if (defect.area >= DEFECT_SIZE_THRESHOLD) {
                        hasSignificantDefects = true;
                        std::stringstream ss;
                        ss << "ROI" << roiResult.roi_id << "[" << (int)defect.area 
                           << "px@(" << (int)defect.x << "," << (int)defect.y << ")]";
                        defectInfoList.push_back(ss.str());
                    }
                }
            }
            
            // 综合判定
            bool finalPass = !hasSignificantDefects;
            
            // 输出结果行
            printResultLine(static_cast<int>(i + 1), static_cast<int>(imageFiles.size()), 
                           filename, finalPass, passedROIs, static_cast<int>(result.roi_results.size()), 
                           minSimilarity, result.processing_time_ms,
                           offsetX, offsetY, rotationDiff);
            
            // 详细模式
            if (config.verbose) {
                printVerboseInfo(result.roi_results);
            }
            
            // 统计
            if (finalPass) passCount++;
            else failCount++;
            
            // 可视化
            if (config.vizEnabled) {
                cv::Mat testImg = cv::imread(testFile);
                if (testImg.empty()) {
                    std::cerr << "\033[33m[警告] 无法加载图像: " << filename << "\033[0m\n";
                    continue;
                }
                std::string baseName = fs::path(testFile).stem().string();
                
                // 将测试图像转换为二值图像用于可视化
                cv::Mat testImgBinary = toBinary3Channel(testImg, BINARY_THRESHOLD);
                visualizer.processAndOutput(testImgBinary, result, rois, baseName + "_binary");
                
                // 对齐叠加可视化
                if (config.showAlignmentOverlay) {
                    // 使用计算得到的偏移量进行可视化（CSV或自动对齐都会提供）
                    if (config.useCsvAlignment || config.autoAlignEnabled) {
                        const cv::Mat& templ = detector.getTemplate();
                        if (templ.empty()) {
                            std::cout << "    [警告] 模板图像为空，无法显示对齐叠加\n";
                        } else if (!testImg.empty()) {
                            cv::Mat overlay;
                            std::string overlayTitle;
                            
                            switch (config.overlayMode) {
                                case 1:  // 棋盘格模式
                                    overlay = createCheckerboardOverlay(templ, testImg, offsetX, offsetY, rotationDiff, 32, BINARY_THRESHOLD);
                                    (void)ALIGN_DIFF_THRESHOLD;  // 避免未使用警告
                                    overlayTitle = "Alignment Check - Checkerboard";
                                    break;
                                case 2:  // 半透明混合
                                    overlay = createBlendedOverlay(templ, testImg, offsetX, offsetY, rotationDiff, 0.5, BINARY_THRESHOLD);
                                    overlayTitle = "Alignment Check - Blended";
                                    break;
                                default:  // 彩色叠加模式（默认）
                                    overlay = createAlignedOverlay(templ, testImg, offsetX, offsetY, rotationDiff, ALIGN_DIFF_THRESHOLD);
                                    overlayTitle = "Alignment Check - Color Overlay";
                                    break;
                            }
                            
                            // 添加信息文字
                            std::stringstream info;
                            info << "Offset: (" << std::fixed << std::setprecision(2) << offsetX << ", " << offsetY 
                                 << ")px  Rotation: " << std::setprecision(3) << rotationDiff << "deg";
                            cv::putText(overlay, info.str(), cv::Point(10, 30), 
                                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
                            cv::putText(overlay, info.str(), cv::Point(10, 30), 
                                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 1);
                            
                            // 添加图例说明
                            std::string legend;
                            if (config.overlayMode == 0) {
                                legend = "Original=Aligned, DarkRed=Misaligned";
                            } else if (config.overlayMode == 1) {
                                legend = "Checkerboard: alternating Template/Aligned";
                            } else {
                                legend = "Blended: 50% Template + 50% Aligned";
                            }
                            cv::putText(overlay, legend, cv::Point(10, 60), 
                                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
                            cv::putText(overlay, legend, cv::Point(10, 60), 
                                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 1);
                            
                            // 调整大小以适应屏幕
                            cv::Mat displayOverlay;
                            double scale = 0.6;  // 缩放比例
                            cv::resize(overlay, displayOverlay, cv::Size(), scale, scale);
                            
                            // 创建窗口并设置位置（避免被遮挡）
                            cv::namedWindow(overlayTitle, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
                            cv::moveWindow(overlayTitle, 50, 50);  // 设置窗口位置在左上角
                            
                            // Windows: 设置窗口置顶（带重试机制）
                            #ifdef _WIN32
                            HWND hwnd = NULL;
                            // 重试最多10次，每次间隔20ms，确保窗口已创建
                            for (int retry = 0; retry < 10 && !hwnd; ++retry) {
                                hwnd = FindWindowA(NULL, overlayTitle.c_str());
                                if (!hwnd) {
                                    Sleep(20);
                                }
                            }
                            if (hwnd) {
                                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                                            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                            } else {
                                std::cout << "    [警告] 无法找到对齐可视化窗口\n";
                            }
                            #endif
                            
                            // 显示叠加图像
                            cv::imshow(overlayTitle, displayOverlay);
                            cv::resizeWindow(overlayTitle, displayOverlay.cols, displayOverlay.rows);
                            
                            std::cout << "    [对齐可视化] 已显示窗口: \"" << overlayTitle << "\"\n";
                            std::cout << "                 窗口尺寸: " << displayOverlay.cols << "x" << displayOverlay.rows << "\n";
                            if (config.overlayMode == 0) {
                                std::cout << "                 提示: 原始图像=对齐良好，深红色=未对齐区域\n";
                            }
                        } else {
                            std::cout << "    [警告] 模板或测试图像为空，无法显示对齐叠加\n";
                        }
                    }
                }
                
                int key = cv::waitKey(config.displayDelayMs);
                if (key == 27) {  // ESC
                    std::cout << "\n[INFO] 用户按ESC退出...\n";
                    break;
                }
            }
            
            // 清除外部变换，为下一张图像做准备
            detector.clearExternalTransform();
        }
        catch (const std::exception& ex) {
            std::cerr << "\033[31m[" << std::setw(3) << (i + 1) << "/" << imageFiles.size() 
                      << "] " << std::left << std::setw(35) << filename 
                      << " | 错误: " << ex.what() << "\033[0m\n";
            failCount++;
            // 错误时也要清除外部变换
            detector.clearExternalTransform();
        }
    }
    
    // 关闭可视化窗口
    if (config.vizEnabled) {
        cv::destroyAllWindows();
    }
    
    // 打印统计报告
    printSummaryReport(config, static_cast<int>(imageFiles.size()), passCount, failCount, totalTime,
                      validLocationCount, minOffset, maxOffset, totalOffset,
                      minRotationDiff, maxRotationDiff, totalRotationDiff);
}

void runDemo(const ProgramConfig& config) {
    // 初始化随机种子（用于ROI生成等）
    unsigned int seed = config.seedSpecified ? config.randomSeed : 
        static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::srand(seed);
    
    // 打印程序标题
    printHeader(config);
    
    // 显示随机种子信息（如果是随机模式且用户未指定种子）
    if (!config.seedSpecified) {
        std::cout << "随机种子: " << seed << " (基于时间)\n";
    } else {
        std::cout << "随机种子: " << seed << " (用户指定)\n";
    }
    
    // 验证数据目录
    if (!fs::exists(config.dataDir)) {
        throw std::runtime_error("数据目录不存在: " + config.dataDir);
    }
    
    // 读取CSV定位数据（如果启用CSV对齐）
    std::map<std::string, LocationData> locationDict;
    LocationData templateLocation;
    std::string templateFilename = fs::path(config.templateFile).filename().string();
    
    if (config.useCsvAlignment) {
        locationDict = loadLocationData(config.dataDir);
        templateLocation = getLocationData(locationDict, templateFilename, config.verbose);
        std::cout << "模板定位点: " << templateLocation.toString() << "\n\n";
    } else {
        std::cout << "CSV对齐: 已禁用，使用自动对齐模式\n";
        std::cout << "模板定位点: 使用默认值 (0, 0, 0)\n\n";
    }
    
    // 获取图像文件列表
    auto imageFiles = getImageFiles(config, templateFilename);
    std::cout << "测试图像: " << imageFiles.size() << " 张\n\n";
    
    // 创建输出目录
    if (config.saveResults && !fs::exists(config.outputDir)) {
        fs::create_directory(config.outputDir);
        std::cout << "创建输出目录: " << config.outputDir << "\n";
    }
    
    // 初始化可视化器
    Visualizer visualizer;
    
    // 加载可视化配置文件（如果存在）
    if (fs::exists(config.configFile)) {
        if (visualizer.loadConfig(config.configFile)) {
            std::cout << "已加载可视化配置: " << config.configFile << "\n";
        } else {
            std::cout << "警告: 无法加载可视化配置，使用默认值\n";
        }
    }
    
    // 应用命令行配置
    VisualizationConfig vizConfig = visualizer.getConfig();
    vizConfig.enabled = config.vizEnabled;
    vizConfig.output_mode = config.vizMode;
    vizConfig.output_directory = config.outputDir;
    vizConfig.display_options.wait_key_ms = config.displayDelayMs;
    vizConfig.display_options.window_name = "Print Defect Detection - Real-time Results";
    visualizer.setConfig(vizConfig);
    
    // 输出可视化模式
    std::cout << "可视化: " << (config.vizEnabled ? "已启用" : "已禁用") << "\n";
    if (config.vizEnabled) {
        std::string modeStr;
        switch (config.vizMode) {
            case OutputMode::FILE_ONLY: modeStr = "仅文件输出"; break;
            case OutputMode::DISPLAY_ONLY: modeStr = "仅显示"; break;
            case OutputMode::BOTH: modeStr = "文件+显示"; break;
        }
        std::cout << "输出模式: " << modeStr << "\n";
    }
    
    // 使用检测器
    DefectDetector detector;
    
    // Step 1: 导入模板
    std::cout << "Step 1: 导入模板图像...\n";
    if (!detector.importTemplateFromFile(config.templateFile)) {
        throw std::runtime_error("无法导入模板: " + config.templateFile);
    }
    
    const cv::Mat& templ = detector.getTemplate();
    std::cout << "  模板尺寸: " << templ.cols << "x" << templ.rows << " 像素\n\n";
    
    // Step 2: 配置检测参数
    configureDetector(detector, config);
    
    // Step 3: 设置ROI区域
    setupSingleROI(detector, config);
    
    // Step 4: 配置对齐模式
    configureAlignment(detector, config);
    
    // Step 5: 执行批量检测
    processImages(detector, visualizer, imageFiles, locationDict, templateLocation, config);
}

// ========== 主入口 ==========

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    // 设置 Windows 控制台为 UTF-8 编码，解决中文乱码
    SetConsoleOutputCP(CP_UTF8);
    // 同时设置输入代码页
    SetConsoleCP(CP_UTF8);
#endif
    
    try {
        // 解析命令行参数
        auto config = parseArguments(argc, argv);
        
        // 运行检测演示
        runDemo(config);
    }
    catch (const std::exception& ex) {
        std::cerr << "\n\033[31m[错误] " << ex.what() << "\033[0m\n";
        return 1;
    }
    
    std::cout << "\n按任意键退出...\n";
    std::cin.get();
    return 0;
}
