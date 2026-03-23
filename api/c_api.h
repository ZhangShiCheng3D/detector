/**
 * @file c_api.h
 * @brief 喷印瑕疵检测系统 - C语言接口
 *
 * 提供给外部系统调用的C接口
 *
 * @note 线程安全性:
 *   - 不同detector实例可以在不同线程中并发使用
 *   - 同一detector实例的并发访问需要外部同步
 *   - GetLastXxx系列函数使用全局状态，非线程安全，建议在单线程环境使用
 *     或在调用DetectXxx后立即调用GetLastXxx
 */

#ifndef DEFECT_DETECTION_C_API_H
#define DEFECT_DETECTION_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #ifdef DEFECT_DETECTION_EXPORTS
        #define EXPORT_API __declspec(dllexport)
    #else
        #define EXPORT_API __declspec(dllimport)
    #endif
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

// ==================== 枚举类型定义 ====================

/**
 * @brief 图像对齐模式
 */
typedef enum {
    ALIGNMENT_NONE = 0,         ///< 不进行对齐
    ALIGNMENT_FULL_IMAGE = 1,   ///< 整图校正模式（对整张图像应用变换）
    ALIGNMENT_ROI_ONLY = 2      ///< ROI区域校正模式（仅对ROI区域应用变换，性能优化）
} AlignmentModeC;

/**
 * @brief 匹配方法枚举
 */
typedef enum {
    MATCH_METHOD_NCC = 0,       ///< 归一化互相关
    MATCH_METHOD_SSIM = 1,      ///< 结构相似性
    MATCH_METHOD_NCC_SSIM = 2,  ///< NCC + SSIM组合
    MATCH_METHOD_BINARY = 3     ///< 二值化对比 (适用于黑白图像)
} MatchMethodC;

// ==================== 结构体定义 ====================

/**
 * @brief 二值图像检测参数结构（C接口）
 * @note 版本2.0新增字段：edge_defect_size_threshold, edge_distance_multiplier, binary_threshold
 */
typedef struct {
    int enabled;                        ///< 是否启用二值图像优化模式 (0=禁用, 1=启用)
    int auto_detect_binary;             ///< 自动检测输入是否为二值图像 (0=禁用, 1=启用)
    int noise_filter_size;              ///< 噪声过滤核大小，默认2（去除2像素以下噪点）
    int edge_tolerance_pixels;          ///< 边缘容错像素数，默认2（允许2像素边缘偏移）
    float edge_diff_ignore_ratio;       ///< 忽略的边缘差异比例，默认0.05（5%）
    int min_significant_area;           ///< 最小显著差异面积，默认20像素
    float area_diff_threshold;          ///< 区域差异阈值，默认0.001（0.1%）
    float overall_similarity_threshold; ///< 总体相似度阈值，默认0.95（95%）
    // 版本2.0新增字段
    int edge_defect_size_threshold;     ///< 边缘区域大瑕疵保留阈值，默认500像素
    float edge_distance_multiplier;     ///< 边缘安全距离倍数，默认2.0
    int binary_threshold;               ///< 二值化阈值，用于提取模板边缘，默认128
} BinaryDetectionParamsC;

/**
 * @brief 定位信息结构（C接口）
 */
typedef struct {
    int success;                ///< 定位是否成功 (0=失败, 1=成功)
    float offset_x;             ///< X方向偏移
    float offset_y;             ///< Y方向偏移
    float rotation_angle;       ///< 旋转角度（度）
    float scale;                ///< 缩放比例
    float match_quality;        ///< 匹配质量 (0.0-1.0)
    int inlier_count;           ///< 内点数量
    int from_external;          ///< 是否来自外部传入的变换 (0=自动计算, 1=外部传入)
} LocalizationInfoC;

/**
 * @brief ROI检测参数结构（C接口）
 */
typedef struct {
    int blur_kernel_size;       ///< 高斯模糊核大小
    int binary_threshold;       ///< 二值化阈值
    int min_defect_size;        ///< 最小瑕疵尺寸
    int detect_black_on_white;  ///< 检测白底黑点 (0=禁用, 1=启用)
    int detect_white_on_black;  ///< 检测黑底白点 (0=禁用, 1=启用)
    float similarity_threshold; ///< 相似度阈值
    int morphology_kernel_size; ///< 形态学核大小
} DetectionParamsC;

/**
 * @brief ROI区域信息结构（C接口）
 */
typedef struct {
    int id;                     ///< ROI的ID
    int x;                      ///< 左上角X坐标
    int y;                      ///< 左上角Y坐标
    int width;                  ///< 宽度
    int height;                 ///< 高度
    float similarity_threshold; ///< 相似度阈值
    int enabled;                ///< 是否启用 (0=禁用, 1=启用)
} ROIInfoC;

/**
 * @brief 单个ROI检测结果（C接口）
 */
typedef struct {
    int roi_id;                 ///< ROI的ID
    float similarity;           ///< 相似度 (0.0-1.0)
    float threshold;            ///< 阈值
    int passed;                 ///< 是否通过 (0=未通过, 1=通过)
    int defect_count;           ///< 该ROI的瑕疵数量
} ROIResultC;

/**
 * @brief 完整检测结果（C接口）
 */
typedef struct {
    int overall_passed;         ///< 总体是否通过 (0=未通过, 1=通过)
    int total_defect_count;     ///< 总瑕疵数量
    double processing_time_ms;  ///< 总处理时间（毫秒）
    double localization_time_ms;    ///< 定位校正时间（毫秒）
    double roi_comparison_time_ms;  ///< ROI比对时间（毫秒）
    LocalizationInfoC localization; ///< 定位信息
    int roi_count;              ///< ROI结果数量
} DetectionResultC;

/**
 * @brief 创建检测器实例
 * @return 检测器句柄，失败返回NULL
 */
EXPORT_API void* CreateDetector();

/**
 * @brief 销毁检测器实例
 * @param detector 检测器句柄
 */
EXPORT_API void DestroyDetector(void* detector);

/**
 * @brief 导入模板图像
 * @param detector 检测器句柄
 * @param bitmap_data 图像数据 (BGR格式)
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数 (1或3)
 * @return 0成功，非0失败
 */
EXPORT_API int ImportTemplate(void* detector, 
                               unsigned char* bitmap_data,
                               int width, int height, int channels);

/**
 * @brief 从文件导入模板图像
 * @param detector 检测器句柄
 * @param filepath 文件路径
 * @return 0成功，非0失败
 */
EXPORT_API int ImportTemplateFromFile(void* detector, const char* filepath);

/**
 * @brief 添加ROI区域
 * @param detector 检测器句柄
 * @param x 左上角X坐标
 * @param y 左上角Y坐标
 * @param width 宽度
 * @param height 高度
 * @param threshold 相似度阈值 (0.0-1.0)
 * @return ROI的ID，失败返回-1
 */
EXPORT_API int AddROI(void* detector, int x, int y, int width, int height, float threshold);

/**
 * @brief 添加ROI区域（带详细参数）
 * @param detector 检测器句柄
 * @param x 左上角X坐标
 * @param y 左上角Y坐标
 * @param width 宽度
 * @param height 高度
 * @param threshold 相似度阈值 (0.0-1.0)
 * @param params ROI检测参数
 * @return ROI的ID，失败返回-1
 * @note 此函数允许为每个ROI设置独立的检测参数，与demo_cpp的ROIManager::addROI行为一致
 */
EXPORT_API int AddROIWithParams(void* detector, int x, int y, int width, int height, float threshold, const DetectionParamsC* params);

/**
 * @brief 移除ROI区域
 * @param detector 检测器句柄
 * @param roi_id ROI的ID
 * @return 0成功，非0失败
 */
EXPORT_API int RemoveROI(void* detector, int roi_id);

/**
 * @brief 设置ROI的相似度阈值
 * @param detector 检测器句柄
 * @param roi_id ROI的ID
 * @param threshold 阈值 (0.0-1.0)
 * @return 0成功，非0失败
 */
EXPORT_API int SetROIThreshold(void* detector, int roi_id, float threshold);

/**
 * @brief 清除所有ROI
 * @param detector 检测器句柄
 */
EXPORT_API void ClearROIs(void* detector);

/**
 * @brief 获取ROI数量
 * @param detector 检测器句柄
 * @return ROI数量
 */
EXPORT_API int GetROICount(void* detector);

/**
 * @brief 设置定位点（保留兼容性）
 * @param detector 检测器句柄
 * @param points 点坐标数组 [x1,y1,x2,y2,...]
 * @param point_count 点的数量
 * @return 0成功，非0失败
 */
EXPORT_API int SetLocalizationPoints(void* detector, float* points, int point_count);

/**
 * @brief 启用/禁用自动定位（保留兼容性）
 * @param detector 检测器句柄
 * @param enable 1启用，0禁用
 */
EXPORT_API void EnableAutoLocalization(void* detector, int enable);

/**
 * @brief 设置外部变换矩阵（X+Y+R）
 * @param detector 检测器句柄
 * @param offset_x X方向偏移
 * @param offset_y Y方向偏移
 * @param rotation 旋转角度（度）
 * @return 0成功，非0失败
 * @note 此接口用于外部系统传入已计算好的变换参数，替代自动定位
 */
EXPORT_API int SetExternalTransform(void* detector, float offset_x, float offset_y, float rotation);

/**
 * @brief 清除外部变换设置
 * @param detector 检测器句柄
 */
EXPORT_API void ClearExternalTransform(void* detector);

/**
 * @brief 执行瑕疵检测
 * @param detector 检测器句柄
 * @param test_image 测试图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数 (1或3)
 * @param similarity_results 输出相似度数组（需预分配，大小为GetROICount()）
 * @param defect_count 输出总瑕疵数量
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectDefects(void* detector,
                              unsigned char* test_image,
                              int width, int height, int channels,
                              float* similarity_results,
                              int* defect_count);

/**
 * @brief 从文件执行瑕疵检测
 * @param detector 检测器句柄
 * @param filepath 图像文件路径
 * @param similarity_results 输出相似度数组（需预分配，大小为GetROICount()）
 * @param defect_count 输出总瑕疵数量
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectDefectsFromFile(void* detector,
                                      const char* filepath,
                                      float* similarity_results,
                                      int* defect_count);

/**
 * @brief 一键学习新模板
 * @param detector 检测器句柄
 * @param new_template 新模板图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数
 * @return 0成功，非0失败
 */
EXPORT_API int LearnTemplate(void* detector,
                              unsigned char* new_template,
                              int width, int height, int channels);

/**
 * @brief 重置模板到原始状态
 * @param detector 检测器句柄
 */
EXPORT_API void ResetTemplate(void* detector);

/**
 * @brief 设置参数
 * @param detector 检测器句柄
 * @param param_name 参数名称
 * @param value 参数值
 * @return 0成功，非0失败
 */
EXPORT_API int SetParameter(void* detector, const char* param_name, float value);

/**
 * @brief 获取参数
 * @param detector 检测器句柄
 * @param param_name 参数名称
 * @return 参数值
 */
EXPORT_API float GetParameter(void* detector, const char* param_name);

/**
 * @brief 获取最后一次检测的处理时间
 * @param detector 检测器句柄
 * @return 处理时间（毫秒）
 */
EXPORT_API double GetLastProcessingTime(void* detector);

// ==================== 瑕疵详情接口 ====================

/**
 * @brief 瑕疵信息结构（C接口）
 */
typedef struct {
    float x;                ///< 瑕疵中心点X坐标
    float y;                ///< 瑕疵中心点Y坐标
    float width;            ///< 瑕疵宽度
    float height;           ///< 瑕疵高度
    float area;             ///< 瑕疵面积
    int defect_type;        ///< 瑕疵类型: 0=未知, 1=漏墨, 2=漏喷, 3=混合瑕疵
    float severity;         ///< 严重程度 (0.0-1.0)
    int roi_id;             ///< 所属ROI的ID
} DefectInfo;

/**
 * @brief 执行瑕疵检测并返回详细信息
 * @param detector 检测器句柄
 * @param test_image 测试图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数
 * @param defect_infos 输出瑕疵详情数组（需预分配）
 * @param max_defects 预分配数组的最大容量
 * @param actual_defect_count 输出实际检测到的瑕疵数量
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectDefectsEx(void* detector,
                                unsigned char* test_image,
                                int width, int height, int channels,
                                DefectInfo* defect_infos,
                                int max_defects,
                                int* actual_defect_count);

/**
 * @brief 从文件执行瑕疵检测并返回详细信息
 * @param detector 检测器句柄
 * @param filepath 图像文件路径
 * @param defect_infos 输出瑕疵详情数组（需预分配）
 * @param max_defects 预分配数组的最大容量
 * @param actual_defect_count 输出实际检测到的瑕疵数量
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectDefectsFromFileEx(void* detector,
                                        const char* filepath,
                                        DefectInfo* defect_infos,
                                        int max_defects,
                                        int* actual_defect_count);

/**
 * @brief 设置瑕疵检测阈值
 * @param detector 检测器句柄
 * @param threshold 瑕疵检测二值化阈值 (0-255)
 * @return 0成功，非0失败
 */
EXPORT_API int SetDefectThreshold(void* detector, int threshold);

/**
 * @brief 设置最小瑕疵尺寸
 * @param detector 检测器句柄
 * @param min_size 最小瑕疵面积（像素）
 * @return 0成功，非0失败
 */
EXPORT_API int SetMinDefectSize(void* detector, int min_size);

// ==================== 对齐模式接口 ====================

/**
 * @brief 设置对齐模式
 * @param detector 检测器句柄
 * @param mode 对齐模式 (ALIGNMENT_NONE=0, ALIGNMENT_FULL_IMAGE=1, ALIGNMENT_ROI_ONLY=2)
 * @return 0成功，非0失败
 */
EXPORT_API int SetAlignmentMode(void* detector, int mode);

/**
 * @brief 获取当前对齐模式
 * @param detector 检测器句柄
 * @return 对齐模式 (ALIGNMENT_NONE=0, ALIGNMENT_FULL_IMAGE=1, ALIGNMENT_ROI_ONLY=2)
 */
EXPORT_API int GetAlignmentMode(void* detector);

/**
 * @brief 获取外部变换参数
 * @param detector 检测器句柄
 * @param offset_x 输出X方向偏移
 * @param offset_y 输出Y方向偏移
 * @param rotation 输出旋转角度（度）
 * @param is_set 输出是否已设置 (0=未设置, 1=已设置)
 * @return 0成功，非0失败
 */
EXPORT_API int GetExternalTransform(void* detector, float* offset_x, float* offset_y, float* rotation, int* is_set);

// ==================== 二值图像检测参数接口 ====================

/**
 * @brief 设置二值图像检测参数
 * @param detector 检测器句柄
 * @param params 二值检测参数
 * @return 0成功，非0失败
 */
EXPORT_API int SetBinaryDetectionParams(void* detector, const BinaryDetectionParamsC* params);

/**
 * @brief 获取二值图像检测参数
 * @param detector 检测器句柄
 * @param params 输出参数结构（需预分配）
 * @return 0成功，非0失败
 */
EXPORT_API int GetBinaryDetectionParams(void* detector, BinaryDetectionParamsC* params);

/**
 * @brief 获取默认二值检测参数
 * @param params 输出参数结构（需预分配）
 */
EXPORT_API void GetDefaultBinaryDetectionParams(BinaryDetectionParamsC* params);

// ==================== 模板匹配器配置接口 ====================

/**
 * @brief 设置匹配方法
 * @param detector 检测器句柄
 * @param method 匹配方法 (MATCH_METHOD_NCC=0, MATCH_METHOD_SSIM=1, MATCH_METHOD_NCC_SSIM=2, MATCH_METHOD_BINARY=3)
 * @return 0成功，非0失败
 */
EXPORT_API int SetMatchMethod(void* detector, int method);

/**
 * @brief 获取当前匹配方法
 * @param detector 检测器句柄
 * @return 匹配方法 (MATCH_METHOD_NCC=0, MATCH_METHOD_SSIM=1, MATCH_METHOD_NCC_SSIM=2, MATCH_METHOD_BINARY=3)
 */
EXPORT_API int GetMatchMethod(void* detector);

/**
 * @brief 设置二值化阈值（用于BINARY匹配方法）
 * @param detector 检测器句柄
 * @param threshold 阈值 (0-255)
 * @return 0成功，非0失败
 */
EXPORT_API int SetBinaryThreshold(void* detector, int threshold);

/**
 * @brief 获取二值化阈值
 * @param detector 检测器句柄
 * @return 阈值 (0-255)
 */
EXPORT_API int GetBinaryThreshold(void* detector);

// ==================== ROI信息获取接口 ====================

/**
 * @brief 获取指定索引的ROI信息
 * @param detector 检测器句柄
 * @param index ROI索引 (0 到 GetROICount()-1)
 * @param roi_info 输出ROI信息结构（需预分配）
 * @return 0成功，非0失败
 */
EXPORT_API int GetROIInfo(void* detector, int index, ROIInfoC* roi_info);

// ==================== 模板信息接口 ====================

/**
 * @brief 获取模板图像尺寸
 * @param detector 检测器句柄
 * @param width 输出宽度
 * @param height 输出高度
 * @param channels 输出通道数
 * @return 0成功，非0失败（模板未加载）
 */
EXPORT_API int GetTemplateSize(void* detector, int* width, int* height, int* channels);

/**
 * @brief 检查模板是否已加载
 * @param detector 检测器句柄
 * @return 1=已加载，0=未加载
 */
EXPORT_API int IsTemplateLoaded(void* detector);

/**
 * @brief 提取ROI模板图像
 * @param detector 检测器句柄
 * @return 0成功，非0失败
 * @note 在添加ROI后调用此函数，从已加载的模板中提取各ROI区域的模板图像
 */
EXPORT_API int ExtractROITemplates(void* detector);

// ==================== 完整检测结果接口 ====================

/**
 * @brief 执行检测并获取完整结果
 * @param detector 检测器句柄
 * @param test_image 测试图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数
 * @param result 输出完整检测结果（需预分配）
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectWithFullResult(void* detector,
                                     unsigned char* test_image,
                                     int width, int height, int channels,
                                     DetectionResultC* result);

/**
 * @brief 从文件执行检测并获取完整结果
 * @param detector 检测器句柄
 * @param filepath 图像文件路径
 * @param result 输出完整检测结果（需预分配）
 * @return 0=通过(无瑕疵)，1=未通过(有瑕疵)，-1=检测失败
 */
EXPORT_API int DetectFromFileWithFullResult(void* detector,
                                             const char* filepath,
                                             DetectionResultC* result);

/**
 * @brief 获取上次检测的每个ROI结果
 * @param detector 检测器句柄
 * @param roi_results 输出ROI结果数组（需预分配）
 * @param max_count 数组最大容量
 * @param actual_count 输出实际ROI数量
 * @return 0成功，非0失败
 */
EXPORT_API int GetLastROIResults(void* detector, ROIResultC* roi_results, int max_count, int* actual_count);

/**
 * @brief 获取上次检测的定位信息
 * @param detector 检测器句柄
 * @param loc_info 输出定位信息（需预分配）
 * @return 0成功，非0失败
 */
EXPORT_API int GetLastLocalizationInfo(void* detector, LocalizationInfoC* loc_info);

/**
 * @brief 获取上次检测的定位时间
 * @param detector 检测器句柄
 * @return 定位时间（毫秒）
 */
EXPORT_API double GetLastLocalizationTime(void* detector);

/**
 * @brief 获取上次检测的ROI比对时间
 * @param detector 检测器句柄
 * @return ROI比对时间（毫秒）
 */
EXPORT_API double GetLastROIComparisonTime(void* detector);

// ==================== 一键学习扩展接口 ====================

/**
 * @brief 一键学习新模板并返回详细结果
 * @param detector 检测器句柄
 * @param new_template 新模板图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数
 * @param quality_score 输出图像质量评分 (0.0-1.0)
 * @param message 输出消息缓冲区（需预分配，建议256字节）
 * @param message_size 消息缓冲区大小
 * @return 0成功，非0失败
 */
EXPORT_API int LearnTemplateEx(void* detector,
                                unsigned char* new_template,
                                int width, int height, int channels,
                                float* quality_score,
                                char* message,
                                int message_size);

/**
 * @brief 从文件学习新模板
 * @param detector 检测器句柄
 * @param filepath 图像文件路径
 * @return 0成功，非0失败
 */
EXPORT_API int LearnTemplateFromFile(void* detector, const char* filepath);

// ==================== 版本信息接口 ====================

/**
 * @brief 获取库版本号
 * @return 版本字符串 (如 "1.0.0")
 */
EXPORT_API const char* GetLibraryVersion(void);

/**
 * @brief 获取构建信息
 * @return 构建信息字符串
 */
EXPORT_API const char* GetBuildInfo(void);

// ==================== 可视化器接口 ====================

/**
 * @brief 创建可视化器实例
 * @return 可视化器句柄，失败返回NULL
 */
EXPORT_API void* CreateVisualizer();

/**
 * @brief 销毁可视化器实例
 * @param visualizer 可视化器句柄
 */
EXPORT_API void DestroyVisualizer(void* visualizer);

/**
 * @brief 加载可视化器配置文件
 * @param visualizer 可视化器句柄
 * @param filepath 配置文件路径
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerLoadConfig(void* visualizer, const char* filepath);

/**
 * @brief 设置可视化器输出目录
 * @param visualizer 可视化器句柄
 * @param output_dir 输出目录路径
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerSetOutputDir(void* visualizer, const char* output_dir);

/**
 * @brief 设置可视化器输出模式
 * @param visualizer 可视化器句柄
 * @param mode 输出模式 (0=仅文件, 1=仅显示, 2=文件+显示)
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerSetOutputMode(void* visualizer, int mode);

/**
 * @brief 设置可视化器显示延迟
 * @param visualizer 可视化器句柄
 * @param delay_ms 延迟毫秒数
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerSetDisplayDelay(void* visualizer, int delay_ms);

/**
 * @brief 设置可视化器窗口名称
 * @param visualizer 可视化器句柄
 * @param window_name 窗口名称
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerSetWindowName(void* visualizer, const char* window_name);

/**
 * @brief 执行可视化并输出
 * @param visualizer 可视化器句柄
 * @param test_image 测试图像数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 通道数
 * @param result 检测结果（DetectionResultC结构）
 * @param base_filename 基础文件名
 * @return 0成功，非0失败
 */
EXPORT_API int VisualizerProcessAndOutput(void* visualizer,
                                           unsigned char* test_image,
                                           int width, int height, int channels,
                                           DetectionResultC* result,
                                           const char* base_filename);

/**
 * @brief 销毁所有OpenCV窗口
 * @param visualizer 可视化器句柄
 */
EXPORT_API void VisualizerDestroyAllWindows(void* visualizer);

// ==================== 模板图像接口 ====================

/**
 * @brief 获取模板图像数据
 * @param detector 检测器句柄
 * @param buffer 输出缓冲区（需预分配，大小为 width * height * channels）
 * @param buffer_size 缓冲区大小
 * @return 0成功，非0失败
 */
EXPORT_API int GetTemplateImageData(void* detector, unsigned char* buffer, int buffer_size);

// ==================== 日志控制接口 ====================

/**
 * @brief 设置API日志文件路径
 * @param filepath 日志文件路径（UTF-8编码），如果为NULL则使用默认路径 "api_debug.log"
 * @note 默认日志文件位于工作目录下的 api_debug.log
 * @note 日志会自动追加到已有文件
 */
EXPORT_API void SetAPILogFile(const char* filepath);

/**
 * @brief 启用或禁用API日志
 * @param enabled 1启用日志，0禁用日志
 * @note 默认启用日志
 */
EXPORT_API void EnableAPILog(int enabled);

/**
 * @brief 清除当前日志文件内容
 * @note 删除并重新创建日志文件
 */
EXPORT_API void ClearAPILog(void);

#ifdef __cplusplus
}
#endif

#endif // DEFECT_DETECTION_C_API_H

