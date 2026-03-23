/**
 * @file demo_dll.cpp
 * @brief DLL API 测试演示程序 - 动态加载DLL版本
 *
 * 此程序通过动态加载 defect_detection.dll 来调用 API，
 * 而不是直接链接静态库。演示了如何使用 C API 进行：
 * - 从CSV文件读取定位点数据
 * - 使用外部变换进行图像对齐
 * - 批量图像检测
 * - 详细结果输出
 *
 * 使用方法:
 *   demo_dll.exe --template <模板文件> --roi <x,y,width,height,threshold> [选项] [数据目录]
 *
 * 示例:
 *   demo_dll.exe --template data/template.bmp --roi 200,420,820,320,0.8
 */

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
#include <limits>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <locale>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

// ==================== DLL 函数指针类型定义 ====================

// 检测器接口
typedef void* (*CreateDetectorFunc)();
typedef void (*DestroyDetectorFunc)(void* detector);
typedef int (*ImportTemplateFromFileFunc)(void* detector, const char* filepath);
typedef int (*AddROIFunc)(void* detector, int x, int y, int width, int height, float threshold);

// DetectionParamsC 结构体定义（与 c_api.h 一致）
typedef struct {
    int blur_kernel_size;
    int binary_threshold;
    int min_defect_size;
    int detect_black_on_white;
    int detect_white_on_black;
    float similarity_threshold;
    int morphology_kernel_size;
} DetectionParamsC;

typedef int (*AddROIWithParamsFunc)(void* detector, int x, int y, int width, int height, float threshold, const DetectionParamsC* params);
typedef int (*RemoveROIFunc)(void* detector, int roi_id);
typedef int (*SetROIThresholdFunc)(void* detector, int roi_id, float threshold);
typedef void (*ClearROIsFunc)(void* detector);
typedef int (*GetROICountFunc)(void* detector);
typedef int (*SetLocalizationPointsFunc)(void* detector, float* points, int point_count);
typedef void (*EnableAutoLocalizationFunc)(void* detector, int enable);
typedef int (*SetExternalTransformFunc)(void* detector, float offset_x, float offset_y, float rotation);
typedef void (*ClearExternalTransformFunc)(void* detector);
typedef int (*DetectDefectsFromFileFunc)(void* detector, const char* filepath, float* similarity_results, int* defect_count);
// DetectDefectsFromFileExFunc 定义在后面与 DefectInfo 一起
typedef int (*LearnTemplateFromFileFunc)(void* detector, const char* filepath);
typedef void (*ResetTemplateFunc)(void* detector);
typedef int (*SetParameterFunc)(void* detector, const char* param_name, float value);
typedef float (*GetParameterFunc)(void* detector, const char* param_name);
typedef double (*GetLastProcessingTimeFunc)(void* detector);
typedef int (*SetAlignmentModeFunc)(void* detector, int mode);
typedef int (*GetAlignmentModeFunc)(void* detector);
typedef int (*GetExternalTransformFunc)(void* detector, float* offset_x, float* offset_y, float* rotation, int* is_set);

// 二值图像检测参数接口
typedef struct {
    int enabled;
    int auto_detect_binary;
    int noise_filter_size;
    int edge_tolerance_pixels;
    float edge_diff_ignore_ratio;
    int min_significant_area;
    float area_diff_threshold;
    float overall_similarity_threshold;
    int edge_defect_size_threshold;
    float edge_distance_multiplier;
    int binary_threshold;
} BinaryDetectionParamsC;

typedef int (*SetBinaryDetectionParamsFunc)(void* detector, const BinaryDetectionParamsC* params);
typedef int (*GetBinaryDetectionParamsFunc)(void* detector, BinaryDetectionParamsC* params);
typedef void (*GetDefaultBinaryDetectionParamsFunc)(BinaryDetectionParamsC* params);

// 匹配方法接口
typedef int (*SetMatchMethodFunc)(void* detector, int method);
typedef int (*GetMatchMethodFunc)(void* detector);
typedef int (*SetBinaryThresholdFunc)(void* detector, int threshold);
typedef int (*GetBinaryThresholdFunc)(void* detector);

// ROI信息接口
typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    float similarity_threshold;
    int enabled;
} ROIInfoC;

typedef int (*GetROIInfoFunc)(void* detector, int index, ROIInfoC* roi_info);
typedef int (*ExtractROITemplatesFunc)(void* detector);

// 模板信息接口
typedef int (*GetTemplateSizeFunc)(void* detector, int* width, int* height, int* channels);
typedef int (*IsTemplateLoadedFunc)(void* detector);

// 瑕疵信息结构
typedef struct {
    float x;
    float y;
    float width;
    float height;
    float area;
    int defect_type;
    float severity;
    int roi_id;
} DefectInfo;

// 完整检测结果接口
typedef struct {
    int success;
    float offset_x;
    float offset_y;
    float rotation_angle;
    float scale;
    float match_quality;
    int inlier_count;
    int from_external;
} LocalizationInfoC;

typedef struct {
    int overall_passed;
    int total_defect_count;
    double processing_time_ms;
    double localization_time_ms;
    double roi_comparison_time_ms;
    LocalizationInfoC localization;
    int roi_count;
} DetectionResultC;

typedef struct {
    int roi_id;
    float similarity;
    float threshold;
    int passed;
    int defect_count;
} ROIResultC;

typedef int (*DetectFromFileWithFullResultFunc)(void* detector, const char* filepath, DetectionResultC* result);
typedef int (*GetLastROIResultsFunc)(void* detector, ROIResultC* roi_results, int max_count, int* actual_count);
typedef int (*GetLastLocalizationInfoFunc)(void* detector, LocalizationInfoC* loc_info);
typedef double (*GetLastLocalizationTimeFunc)(void* detector);
typedef double (*GetLastROIComparisonTimeFunc)(void* detector);

// 版本信息接口
typedef const char* (*GetLibraryVersionFunc)(void);
typedef const char* (*GetBuildInfoFunc)(void);

// 日志控制接口
typedef void (*SetAPILogFileFunc)(const char* filepath);
typedef void (*EnableAPILogFunc)(int enabled);
typedef void (*ClearAPILogFunc)(void);

// ==================== DLL 加载管理类 ====================

class DLLManager {
private:
#ifdef _WIN32
    HMODULE dll_handle_ = nullptr;
#else
    void* dll_handle_ = nullptr;
#endif

public:
    // 检测器函数
    CreateDetectorFunc CreateDetector = nullptr;
    DestroyDetectorFunc DestroyDetector = nullptr;
    ImportTemplateFromFileFunc ImportTemplateFromFile = nullptr;
    AddROIFunc AddROI = nullptr;
    AddROIWithParamsFunc AddROIWithParams = nullptr;
    RemoveROIFunc RemoveROI = nullptr;
    SetROIThresholdFunc SetROIThreshold = nullptr;
    ClearROIsFunc ClearROIs = nullptr;
    GetROICountFunc GetROICount = nullptr;
    SetLocalizationPointsFunc SetLocalizationPoints = nullptr;
    EnableAutoLocalizationFunc EnableAutoLocalization = nullptr;
    SetExternalTransformFunc SetExternalTransform = nullptr;
    ClearExternalTransformFunc ClearExternalTransform = nullptr;
    DetectDefectsFromFileFunc DetectDefectsFromFile = nullptr;
    typedef int (*DetectDefectsFromFileExFunc)(void* detector, const char* filepath, DefectInfo* defect_infos, int max_defects, int* actual_defect_count);
    DetectDefectsFromFileExFunc DetectDefectsFromFileEx = nullptr;
    LearnTemplateFromFileFunc LearnTemplateFromFile = nullptr;
    ResetTemplateFunc ResetTemplate = nullptr;
    SetParameterFunc SetParameter = nullptr;
    GetParameterFunc GetParameter = nullptr;
    GetLastProcessingTimeFunc GetLastProcessingTime = nullptr;
    SetAlignmentModeFunc SetAlignmentMode = nullptr;
    GetAlignmentModeFunc GetAlignmentMode = nullptr;
    GetExternalTransformFunc GetExternalTransform = nullptr;
    SetBinaryDetectionParamsFunc SetBinaryDetectionParams = nullptr;
    GetBinaryDetectionParamsFunc GetBinaryDetectionParams = nullptr;
    GetDefaultBinaryDetectionParamsFunc GetDefaultBinaryDetectionParams = nullptr;
    SetMatchMethodFunc SetMatchMethod = nullptr;
    GetMatchMethodFunc GetMatchMethod = nullptr;
    SetBinaryThresholdFunc SetBinaryThreshold = nullptr;
    GetBinaryThresholdFunc GetBinaryThreshold = nullptr;
    GetROIInfoFunc GetROIInfo = nullptr;
    ExtractROITemplatesFunc ExtractROITemplates = nullptr;
    GetTemplateSizeFunc GetTemplateSize = nullptr;
    IsTemplateLoadedFunc IsTemplateLoaded = nullptr;
    DetectFromFileWithFullResultFunc DetectFromFileWithFullResult = nullptr;
    GetLastROIResultsFunc GetLastROIResults = nullptr;
    GetLastLocalizationInfoFunc GetLastLocalizationInfo = nullptr;
    GetLastLocalizationTimeFunc GetLastLocalizationTime = nullptr;
    GetLastROIComparisonTimeFunc GetLastROIComparisonTime = nullptr;
    GetLibraryVersionFunc GetLibraryVersion = nullptr;
    GetBuildInfoFunc GetBuildInfo = nullptr;
    SetAPILogFileFunc SetAPILogFile = nullptr;
    EnableAPILogFunc EnableAPILog = nullptr;
    ClearAPILogFunc ClearAPILog = nullptr;

    ~DLLManager() {
        unload();
    }

    bool load(const char* dll_path) {
#ifdef _WIN32
        dll_handle_ = LoadLibraryA(dll_path);
        if (!dll_handle_) {
            std::cerr << "无法加载DLL: " << dll_path << " (错误码: " << GetLastError() << ")" << std::endl;
            return false;
        }

        // 加载所有函数
        #define LOAD_FUNC(name) \
            name = (name##Func)GetProcAddress(dll_handle_, #name); \
            if (!name) { \
                std::cerr << "无法加载函数: " << #name << std::endl; \
                unload(); \
                return false; \
            }
#else
        dll_handle_ = dlopen(dll_path, RTLD_LAZY);
        if (!dll_handle_) {
            std::cerr << "无法加载DLL: " << dll_path << " (错误: " << dlerror() << ")" << std::endl;
            return false;
        }

        #define LOAD_FUNC(name) \
            name = (name##Func)dlsym(dll_handle_, #name); \
            if (!name) { \
                std::cerr << "无法加载函数: " << #name << std::endl; \
                unload(); \
                return false; \
            }
#endif

        LOAD_FUNC(CreateDetector);
        LOAD_FUNC(DestroyDetector);
        LOAD_FUNC(ImportTemplateFromFile);
        LOAD_FUNC(AddROI);
        LOAD_FUNC(AddROIWithParams);
        LOAD_FUNC(RemoveROI);
        LOAD_FUNC(SetROIThreshold);
        LOAD_FUNC(ClearROIs);
        LOAD_FUNC(GetROICount);
        LOAD_FUNC(SetLocalizationPoints);
        LOAD_FUNC(EnableAutoLocalization);
        LOAD_FUNC(SetExternalTransform);
        LOAD_FUNC(ClearExternalTransform);
        LOAD_FUNC(DetectDefectsFromFile);
        LOAD_FUNC(DetectDefectsFromFileEx);
        LOAD_FUNC(LearnTemplateFromFile);
        LOAD_FUNC(ResetTemplate);
        LOAD_FUNC(SetParameter);
        LOAD_FUNC(GetParameter);
        LOAD_FUNC(GetLastProcessingTime);
        LOAD_FUNC(SetAlignmentMode);
        LOAD_FUNC(GetAlignmentMode);
        LOAD_FUNC(GetExternalTransform);
        LOAD_FUNC(SetBinaryDetectionParams);
        LOAD_FUNC(GetBinaryDetectionParams);
        LOAD_FUNC(GetDefaultBinaryDetectionParams);
        LOAD_FUNC(SetMatchMethod);
        LOAD_FUNC(GetMatchMethod);
        LOAD_FUNC(SetBinaryThreshold);
        LOAD_FUNC(GetBinaryThreshold);
        LOAD_FUNC(GetROIInfo);
        LOAD_FUNC(ExtractROITemplates);
        LOAD_FUNC(GetTemplateSize);
        LOAD_FUNC(IsTemplateLoaded);
        LOAD_FUNC(DetectFromFileWithFullResult);
        LOAD_FUNC(GetLastROIResults);
        LOAD_FUNC(GetLastLocalizationInfo);
        LOAD_FUNC(GetLastLocalizationTime);
        LOAD_FUNC(GetLastROIComparisonTime);
        LOAD_FUNC(GetLibraryVersion);
        LOAD_FUNC(GetBuildInfo);
        LOAD_FUNC(SetAPILogFile);
        LOAD_FUNC(EnableAPILog);
        LOAD_FUNC(ClearAPILog);

#undef LOAD_FUNC

        return true;
    }

    void unload() {
        if (dll_handle_) {
#ifdef _WIN32
            FreeLibrary(dll_handle_);
#else
            dlclose(dll_handle_);
#endif
            dll_handle_ = nullptr;
        }
    }

    bool isLoaded() const {
        return dll_handle_ != nullptr;
    }
};

// ==================== 常量定义 ====================

#ifdef _WIN32
const std::wstring CSV_FILENAME_W = L"匹配信息_116_20260304161622333.csv";
const std::string CSV_FILENAME = "匹配信息_116_20260304161622333.csv";
#else
const std::string CSV_FILENAME = "匹配信息_116_20260304161622333.csv";
#endif
const int BINARY_THRESHOLD = 30;
const int ALIGN_DIFF_THRESHOLD = 128;
const int DEFECT_SIZE_THRESHOLD = 100;

// 匹配方法枚举
enum MatchMethodC {
    MATCH_METHOD_NCC = 0,
    MATCH_METHOD_SSIM = 1,
    MATCH_METHOD_NCC_SSIM = 2,
    MATCH_METHOD_BINARY = 3
};

// 对齐模式枚举
enum AlignmentModeC {
    ALIGNMENT_NONE = 0,
    ALIGNMENT_FULL_IMAGE = 1,
    ALIGNMENT_ROI_ONLY = 2
};

// ==================== 数据结构 ====================

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
    std::string templateFile;
    ROIParams roi;
    bool hasROI = false;
    std::string dataDir;
    std::string outputDir = "output";
    int maxImages = INT_MAX;
    bool verbose = false;
    bool vizEnabled = false;  // 禁用可视化，因为DLL API中可视化器接口可能不完整
    int displayDelayMs = 1500;
    bool showAlignmentOverlay = false;  // 禁用对齐可视化
    int overlayMode = 0;
    bool autoAlignEnabled = true;
    int alignMode = ALIGNMENT_ROI_ONLY;
    bool useCsvAlignment = true;
    int roiShrinkPixels = 5;
    bool binaryOptimizationEnabled = true;
    int binaryThreshold = 30;
    int noiseFilterSize = 3;
    int edgeTolerancePixels = 2;
    bool edgeFilterEnabled = true;
    float edgeDiffIgnoreRatio = 0.05f;
    int minSignificantArea = 20;
    float areaDiffThreshold = 0.01f;
    float overallSimilarityThreshold = 0.90f;
    int minDefectSize = 100;
    int blurKernelSize = 3;
    bool detectBlackOnWhite = true;
    bool detectWhiteOnBlack = true;
    int edgeDefectSizeThreshold = 500;
    float edgeDistanceMultiplier = 2.0f;
};

// ==================== CSV文件读取 ====================

std::map<std::string, LocationData> loadLocationData(const std::string& dataDir) {
    std::map<std::string, LocationData> locationDict;
    
#ifdef _WIN32
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
    
    std::wifstream file(wCsvPath);
    if (!file.is_open()) {
        std::cerr << "[警告] 无法打开CSV文件: " << csvPath << "\n";
        return locationDict;
    }
    
    try {
        file.imbue(std::locale(""));
    } catch (...) {
        try {
            file.imbue(std::locale::classic());
        } catch (...) {
            std::cerr << "[警告] 无法设置文件locale\n";
        }
    }
#else
    std::string csvPath = dataDir + "/" + CSV_FILENAME;
    
    if (!fs::exists(csvPath)) {
        std::cout << "[警告] CSV文件不存在: " << csvPath << "\n";
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
    std::wstring wline;
    std::getline(file, wline);
    
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
    std::getline(file, line);
    
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

// ==================== 命令行解析 ====================

void printUsage(const char* prog) {
    std::cout << "\n用法: " << prog << " --template <模板文件> --roi <x,y,w,h,t> [选项] [数据目录]\n\n";
    std::cout << "必需参数:\n";
    std::cout << "  --template <file>   指定模板文件路径\n";
    std::cout << "  --roi <x,y,w,h,t>   指定ROI参数 (例如: --roi 200,420,820,320,0.8)\n\n";
    std::cout << "基础选项:\n";
    std::cout << "  --help              显示此帮助信息\n";
    std::cout << "  --dll <path>        指定DLL路径 (默认: defect_detection.dll)\n";
    std::cout << "  --output <dir>      指定输出目录 (默认: output)\n";
    std::cout << "  --max-images <n>    最大处理图像数量 (默认: 全部)\n";
    std::cout << "  --verbose, -v       显示详细信息\n";
    std::cout << "  --save              保存检测结果图像\n\n";
    
    std::cout << "对齐选项:\n";
    std::cout << "  --no-align          禁用自动对齐\n";
    std::cout << "  --align-full        使用全图对齐模式\n";
    std::cout << "  --align-roi         使用ROI对齐模式（默认）\n";
    std::cout << "  --no-csv            禁用CSV对齐，使用自动对齐\n\n";
    
    std::cout << "ROI选项:\n";
    std::cout << "  --roi-shrink <px>   ROI内缩像素数（避免检测边缘，默认: 5）\n\n";
    
    std::cout << "二值图像优化参数:\n";
    std::cout << "  --no-binary-opt     禁用二值图像优化\n";
    std::cout << "  --binary-thresh <n> 二值化阈值 (默认: 30)\n";
    std::cout << "  --edge-tol <px>     边缘容错像素数 (默认: 2)\n";
    std::cout << "  --noise-size <px>   噪声过滤尺寸 (默认: 3)\n\n";
    
    std::cout << "通用检测参数:\n";
    std::cout << "  --min-defect <n>    最小瑕疵尺寸 (默认: 100)\n";
    std::cout << "  --blur-size <n>     高斯模糊核大小 (默认: 3)\n\n";
    
    std::cout << "示例:\n";
    std::cout << "  " << prog << " --template template.bmp --roi 200,420,820,320,0.8 data/20260204\n";
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
        
        if (roi.width <= 0 || roi.height <= 0) {
            throw std::invalid_argument("ROI宽度和高度必须大于0");
        }
        if (roi.threshold < 0.0f || roi.threshold > 1.0f) {
            throw std::invalid_argument("ROI阈值必须在0.0到1.0之间");
        }
        
        return roi;
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("ROI参数解析失败: ") + e.what());
    }
}

ProgramConfig parseArguments(int argc, char** argv, std::string& dllPath) {
    ProgramConfig config;
    dllPath = "defect_detection.dll";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        } else if (arg == "--dll" && i + 1 < argc) {
            dllPath = argv[++i];
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
            // config.saveResults = true;  // 保存功能在DLL模式下暂不支持
        } else if (arg == "--no-align") {
            config.autoAlignEnabled = false;
            config.alignMode = ALIGNMENT_NONE;
        } else if (arg == "--align-full") {
            config.alignMode = ALIGNMENT_FULL_IMAGE;
        } else if (arg == "--align-roi") {
            config.alignMode = ALIGNMENT_ROI_ONLY;
        } else if (arg == "--no-csv") {
            config.useCsvAlignment = false;
        } else if (arg == "--roi-shrink" && i + 1 < argc) {
            config.roiShrinkPixels = std::stoi(argv[++i]);
            if (config.roiShrinkPixels < 0) config.roiShrinkPixels = 0;
            if (config.roiShrinkPixels > 50) config.roiShrinkPixels = 50;
        } else if (arg == "--no-binary-opt") {
            config.binaryOptimizationEnabled = false;
        } else if (arg == "--binary-thresh" && i + 1 < argc) {
            config.binaryThreshold = std::stoi(argv[++i]);
        } else if (arg == "--edge-tol" && i + 1 < argc) {
            config.edgeTolerancePixels = std::stoi(argv[++i]);
        } else if (arg == "--noise-size" && i + 1 < argc) {
            config.noiseFilterSize = std::stoi(argv[++i]);
        } else if (arg == "--min-defect" && i + 1 < argc) {
            config.minDefectSize = std::stoi(argv[++i]);
        } else if (arg == "--blur-size" && i + 1 < argc) {
            config.blurKernelSize = std::stoi(argv[++i]);
        } else if (arg == "--sim-thresh" && i + 1 < argc) {
            config.overallSimilarityThreshold = std::stof(argv[++i]);
        } else if (arg == "--min-area" && i + 1 < argc) {
            config.minSignificantArea = std::stoi(argv[++i]);
        } else if (arg[0] != '-') {
            config.dataDir = arg;
        }
    }
    
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
    
    if (config.dataDir.empty()) {
        config.dataDir = fs::path(config.templateFile).parent_path().string();
        if (config.dataDir.empty()) config.dataDir = ".";
    }
    
    return config;
}

// ==================== 辅助方法 ====================

void printHeader(const ProgramConfig& config, DLLManager& dll) {
    std::cout << "============================================================\n";
    std::cout << "   喷印瑕疵检测系统 - DLL API 版本\n";
    std::cout << "   Print Defect Detection System - DLL API Version\n";
    std::cout << "============================================================\n";
    std::cout << "库版本: " << dll.GetLibraryVersion() << "\n";
    std::cout << "构建信息: " << dll.GetBuildInfo() << "\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "模板文件: " << config.templateFile << "\n";
    std::cout << "ROI参数: " << config.roi.toString() << "\n";
    std::cout << "数据目录: " << config.dataDir << "\n";
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
    
    if (config.maxImages < (int)imageFiles.size()) {
        std::cout << "限制处理数量: " << config.maxImages << " 张 (共 " << imageFiles.size() << " 张)\n";
        imageFiles.resize(config.maxImages);
    }
    
    return imageFiles;
}

void printResultLine(int index, int total, const std::string& filename, bool passed,
                     int passedRois, int totalRois, float minSimilarity, double processingTime,
                     float offsetX, float offsetY, float rotationDiff) {
    std::string displayName = filename;
    if (displayName.length() > 30) {
        displayName = displayName.substr(0, 27) + "...";
    }
    
    if (passed) {
        std::cout << "\033[32m";
    } else {
        std::cout << "\033[31m";
    }
    
    std::cout << "[" << std::setw(3) << index << "/" << total << "] " 
              << std::left << std::setw(30) << displayName
              << " | " << (passed ? "PASS" : "FAIL");
    std::cout << "\033[0m";
    
    std::cout << " | " << passedRois << "/" << totalRois
              << " | sim:" << std::fixed << std::setprecision(1) << (minSimilarity * 100) << "%"
              << " | Δ(" << std::setw(6) << std::setprecision(1) << offsetX << ","
              << std::setw(6) << offsetY << ")"
              << " | Δθ" << std::setw(6) << std::setprecision(3) << rotationDiff << "°"
              << " | " << std::setw(6) << std::setprecision(1) << processingTime << "ms\n";
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

// ==================== 核心检测流程 ====================

void configureDetector(void* detector, DLLManager& dll, const ProgramConfig& config) {
    std::cout << "Step 2: 配置检测参数...\n";
    
    // 设置匹配方法为二值模式
    dll.SetMatchMethod(detector, MATCH_METHOD_BINARY);
    dll.SetBinaryThreshold(detector, config.binaryThreshold);
    
    // 配置二值检测参数（与 demo_cpp 完全一致）
    BinaryDetectionParamsC binaryParams;
    dll.GetDefaultBinaryDetectionParams(&binaryParams);
    
    binaryParams.enabled = config.binaryOptimizationEnabled ? 1 : 0;
    binaryParams.auto_detect_binary = 1;  // demo_cpp 中显式设置为 true
    binaryParams.noise_filter_size = config.noiseFilterSize;
    binaryParams.edge_tolerance_pixels = config.edgeFilterEnabled ? config.edgeTolerancePixels : 0;
    binaryParams.edge_diff_ignore_ratio = config.edgeDiffIgnoreRatio;
    binaryParams.min_significant_area = config.minSignificantArea;
    binaryParams.area_diff_threshold = config.areaDiffThreshold;  // 确保传递此参数
    binaryParams.overall_similarity_threshold = config.overallSimilarityThreshold;
    binaryParams.edge_defect_size_threshold = config.edgeDefectSizeThreshold;
    binaryParams.edge_distance_multiplier = config.edgeDistanceMultiplier;
    binaryParams.binary_threshold = config.binaryThreshold;
    
    dll.SetBinaryDetectionParams(detector, &binaryParams);
    
    // 设置其他参数
    dll.SetParameter(detector, "min_defect_size", static_cast<float>(config.minDefectSize));
    dll.SetParameter(detector, "blur_kernel_size", static_cast<float>(config.blurKernelSize));
    dll.SetParameter(detector, "detect_black_on_white", config.detectBlackOnWhite ? 1.0f : 0.0f);
    dll.SetParameter(detector, "detect_white_on_black", config.detectWhiteOnBlack ? 1.0f : 0.0f);
    
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
    } else {
        std::cout << "  二值图像优化: 已禁用\n";
    }
    
    std::cout << "  min_defect_size = " << config.minDefectSize << "\n";
    std::cout << "  blur_kernel_size = " << config.blurKernelSize << "\n\n";
}

void setupSingleROI(void* detector, DLLManager& dll, const ProgramConfig& config) {
    std::cout << "Step 3: 设置ROI区域...\n";
    
    // ROI内缩（与 demo_cpp 完全一致）
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
    
    // 创建DetectionParams并设置参数（与 demo_cpp 完全一致）
    DetectionParamsC params;
    params.binary_threshold = config.binaryThreshold;
    params.blur_kernel_size = config.blurKernelSize;
    params.min_defect_size = config.minDefectSize;
    params.detect_black_on_white = config.detectBlackOnWhite ? 1 : 0;
    params.detect_white_on_black = config.detectWhiteOnBlack ? 1 : 0;
    params.similarity_threshold = config.roi.threshold;
    params.morphology_kernel_size = 3;  // 默认值
    
    // 使用AddROIWithParams添加ROI（与 demo_cpp 的ROIManager::addROI行为一致）
    int roiId = -1;
    if (dll.AddROIWithParams) {
        roiId = dll.AddROIWithParams(detector, x, y, width, height, config.roi.threshold, &params);
    } else {
        // 回退到普通AddROI
        roiId = dll.AddROI(detector, x, y, width, height, config.roi.threshold);
    }
    
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
    std::cout << "  总计: " << dll.GetROICount(detector) << " 个ROI\n";
    
    // 提取ROI模板图像（关键！否则模板图像为空，无法检测）
    if (dll.ExtractROITemplates) {
        dll.ExtractROITemplates(detector);
        std::cout << "  已提取ROI模板图像\n\n";
    } else {
        std::cout << "  [警告] DLL 不支持 ExtractROITemplates\n\n";
    }
}

void configureAlignment(void* detector, DLLManager& dll, const ProgramConfig& config) {
    std::cout << "Step 4: 配置对齐...\n";
    
    // 配置自动对齐
    dll.EnableAutoLocalization(detector, config.autoAlignEnabled ? 1 : 0);
    dll.SetAlignmentMode(detector, config.alignMode);
    
    if (config.useCsvAlignment) {
        std::cout << "  对齐方式: CSV外部数据（优先）+ 自动对齐（后备）\n";
    } else {
        std::cout << "  对齐方式: 自动对齐（CSV已禁用）\n";
    }
    
    if (config.autoAlignEnabled) {
        std::string modeStr;
        switch (config.alignMode) {
            case ALIGNMENT_NONE: modeStr = "NONE"; break;
            case ALIGNMENT_FULL_IMAGE: modeStr = "FULL_IMAGE (全图对齐)"; break;
            case ALIGNMENT_ROI_ONLY: modeStr = "ROI_ONLY (ROI区域对齐)"; break;
        }
        std::cout << "  自动对齐模式: " << modeStr << "\n";
        if (config.useCsvAlignment) {
            std::cout << "  注意: 当CSV数据可用时将优先使用CSV数据对齐\n\n";
        }
    } else {
        std::cout << "  自动对齐: 已禁用\n\n";
    }
}

void processImages(void* detector, DLLManager& dll,
                   const std::vector<std::string>& imageFiles,
                   const std::map<std::string, LocationData>& locationDict,
                   const LocationData& templateLocation,
                   const ProgramConfig& config) {
    std::cout << "Step 5: 处理测试图像...\n";
    std::cout << "----------------------------------------\n";
    
    int passCount = 0;
    int failCount = 0;
    double totalTime = 0;
    
    float maxOffset = 0, minOffset = 999999.0f, totalOffset = 0;
    float maxRotationDiff = 0, minRotationDiff = 999999.0f, totalRotationDiff = 0;
    int validLocationCount = 0;
    
    // 分配相似度结果缓冲区
    int roiCount = dll.GetROICount(detector);
    std::vector<float> similarityResults(roiCount);
    
    for (size_t i = 0; i < imageFiles.size(); ++i) {
        const std::string& testFile = imageFiles[i];
        std::string filename = fs::path(testFile).filename().string();
        
        try {
            float offsetX = 0, offsetY = 0, rotationDiff = 0;
            
            if (config.useCsvAlignment) {
                auto testLocation = getLocationData(locationDict, filename, config.verbose);
                
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
                
                // 设置外部变换
                if (testLocation.isValid) {
                    dll.SetExternalTransform(detector, offsetX, offsetY, rotationDiff);
                    if (config.verbose) {
                        std::cout << "  使用CSV数据对齐: 偏移(" << std::fixed << std::setprecision(2) 
                                  << offsetX << ", " << offsetY << ")px, 旋转" << std::setprecision(3) 
                                  << rotationDiff << "°\n";
                    }
                } else {
                    dll.ClearExternalTransform(detector);
                    if (config.verbose) {
                        std::cout << "  无CSV定位数据，使用自动对齐\n";
                    }
                }
            } else {
                dll.ClearExternalTransform(detector);
                if (config.verbose) {
                    std::cout << "  使用自动对齐（CSV已禁用）\n";
                }
            }
            
            // 执行检测（只调用一次 DetectDefectsFromFileEx 获取完整信息和瑕疵详情）
            const int MAX_DEFECTS = 1000;
            std::vector<DefectInfo> defectInfos(MAX_DEFECTS);
            int actualDefectCount = 0;
            
            int ret = dll.DetectDefectsFromFileEx(detector, testFile.c_str(), defectInfos.data(), MAX_DEFECTS, &actualDefectCount);
            
            // 获取时间和ROI结果（从上次的检测中）
            double processingTime = dll.GetLastProcessingTime(detector);
            totalTime += processingTime;
            
            // 获取ROI结果
            int passedROIs = 0;
            float minSimilarity = 1.0f;
            int zeroSimilarityCount = 0;
            std::vector<ROIResultC> roiResults(roiCount);
            int actualCount = 0;
            dll.GetLastROIResults(detector, roiResults.data(), roiCount, &actualCount);
            
            for (int r = 0; r < actualCount; ++r) {
                if (roiResults[r].passed) passedROIs++;
                if (roiResults[r].similarity < minSimilarity) minSimilarity = roiResults[r].similarity;
                if (roiResults[r].similarity < 1e-6f) zeroSimilarityCount++;
            }
            
            // 瑕疵检测判定（与demo_cpp完全一致：面积>=100视为重大缺陷）
            bool hasSignificantDefects = false;
            std::vector<std::string> defectInfoList;
            
            for (int d = 0; d < actualDefectCount; ++d) {
                if (defectInfos[d].area >= DEFECT_SIZE_THRESHOLD) {
                    hasSignificantDefects = true;
                    std::stringstream ss;
                    ss << "ROI" << defectInfos[d].roi_id << "[" << (int)defectInfos[d].area 
                       << "px@(" << (int)defectInfos[d].x << "," << (int)defectInfos[d].y << ")]";
                    defectInfoList.push_back(ss.str());
                }
            }
            
            // 调试输出：显示检测到的瑕疵数量（与demo_cpp一致）
            if (config.verbose) {
                std::cout << "  检测到瑕疵: " << actualDefectCount << " 个，重大瑕疵: " << defectInfoList.size() << " 个\n";
            }
            
            // 综合判定（与demo_cpp完全一致：只看瑕疵面积，不看similarity）
            bool finalPass = !hasSignificantDefects;
            
            // 输出结果行
            printResultLine(static_cast<int>(i + 1), static_cast<int>(imageFiles.size()), 
                           filename, finalPass, passedROIs, actualCount, 
                           minSimilarity, processingTime,
                           offsetX, offsetY, rotationDiff);
            
            // 详细模式输出瑕疵信息
            if (config.verbose && !defectInfoList.empty()) {
                std::cout << "    重大瑕疵: ";
                for (size_t d = 0; d < defectInfoList.size() && d < 5; ++d) {
                    if (d > 0) std::cout << ", ";
                    std::cout << defectInfoList[d];
                }
                if (defectInfoList.size() > 5) {
                    std::cout << " ... 等" << defectInfoList.size() << "个";
                }
                std::cout << "\n";
            }
            
            // 统计
            if (finalPass) passCount++;
            else failCount++;
            
            // 清除外部变换
            dll.ClearExternalTransform(detector);
        }
        catch (const std::exception& ex) {
            std::cerr << "\033[31m[" << std::setw(3) << (i + 1) << "/" << imageFiles.size() 
                      << "] " << std::left << std::setw(35) << filename 
                      << " | 错误: " << ex.what() << "\033[0m\n";
            failCount++;
            dll.ClearExternalTransform(detector);
        }
    }
    
    // 打印统计报告
    printSummaryReport(config, static_cast<int>(imageFiles.size()), passCount, failCount, totalTime,
                      validLocationCount, minOffset, maxOffset, totalOffset,
                      minRotationDiff, maxRotationDiff, totalRotationDiff);
}

void runDemo(const ProgramConfig& config, DLLManager& dll) {
    // 初始化随机种子
    unsigned int seed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::srand(seed);
    
    // 打印程序标题
    printHeader(config, dll);
    
    // 验证数据目录
    if (!fs::exists(config.dataDir)) {
        throw std::runtime_error("数据目录不存在: " + config.dataDir);
    }
    
    // 读取CSV定位数据
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
    if (!fs::exists(config.outputDir)) {
        fs::create_directory(config.outputDir);
    }
    
    // 使用检测器
    void* detector = dll.CreateDetector();
    if (!detector) {
        throw std::runtime_error("无法创建检测器实例");
    }
    
    // Step 1: 导入模板
    std::cout << "Step 1: 导入模板图像...\n";
    if (dll.ImportTemplateFromFile(detector, config.templateFile.c_str()) != 0) {
        dll.DestroyDetector(detector);
        throw std::runtime_error("无法导入模板: " + config.templateFile);
    }
    
    int width, height, channels;
    dll.GetTemplateSize(detector, &width, &height, &channels);
    std::cout << "  模板尺寸: " << width << "x" << height << " 像素\n\n";
    
    // Step 2: 配置检测参数
    configureDetector(detector, dll, config);
    
    // Step 3: 设置ROI区域
    setupSingleROI(detector, dll, config);
    
    // Step 4: 配置对齐模式
    configureAlignment(detector, dll, config);
    
    // Step 5: 执行批量检测
    processImages(detector, dll, imageFiles, locationDict, templateLocation, config);
    
    // 销毁检测器
    dll.DestroyDetector(detector);
}

// ==================== 主入口 ====================

#ifdef _WIN32
int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
int main(int argc, char** argv) {
#endif
    
    try {
        std::string dllPath;
        auto config = parseArguments(argc, argv, dllPath);
        
        // 加载DLL
        std::cout << "正在加载DLL: " << dllPath << "...\n";
        DLLManager dll;
        if (!dll.load(dllPath.c_str())) {
            std::cerr << "错误: 无法加载DLL库\n";
            std::cerr << "请确保 " << dllPath << " 在当前目录或PATH中\n";
            return 1;
        }
        std::cout << "DLL加载成功!\n\n";
        
        // 运行检测演示
        runDemo(config, dll);
    }
    catch (const std::exception& ex) {
        std::cerr << "\n\033[31m[错误] " << ex.what() << "\033[0m\n";
        return 1;
    }
    
    std::cout << "\n按任意键退出...\n";
    std::cin.get();
    return 0;
}
