/*
 * ============================================================================
 * 文件名: DefectDetectionAPI.cs
 * 描述:   喷印瑕疵检测系统 C# 封装
 * 版本:   3.2 (优化版)
 *
 * 功能说明:
 *   本类是对 defect_detection.dll 的 C# 封装，提供了完整的瑕疵检测功能接口，
 *   包括模板管理、ROI配置、检测执行、结果获取等。
 *
 * 线程安全性:
 *   - 不同 DefectDetectorAPI 实例可以在不同线程中并发使用
 *   - 同一实例的方法调用需要外部同步（加锁）
 *   - GetLastXxx 系列方法使用 C API 的全局状态，非线程安全
 *     建议在调用 Detect/DetectEx 后立即调用 GetLastXxx 获取结果
 *
 * 内存管理:
 *   - 本类实现 IDisposable 接口，使用完毕后请调用 Dispose() 或使用 using 语句
 *   - Bitmap 参数不会被本类修改或释放，调用方负责管理其生命周期
 *
 * 使用示例:
 *   using (var detector = new DefectDetectorAPI())
 *   {
 *       detector.ImportTemplateFromFile("template.png");
 *       detector.AddROI(100, 100, 200, 200, 0.85f);
 *       var result = detector.Detect(testBitmap);
 *       Console.WriteLine(result);
 *   }
 * ============================================================================
 */

using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace DefectDetection
{
    /// <summary>
    /// 喷印瑕疵检测系统 C# 封装类。
    /// <para>
    /// 提供完整的瑕疵检测功能，包括模板管理、ROI配置、检测执行和结果分析。
    /// 实现了 <see cref="IDisposable"/> 接口，使用完毕后请释放资源。
    /// </para>
    /// </summary>
    /// <remarks>
    /// <para><b>线程安全性：</b></para>
    /// <list type="bullet">
    ///   <item>不同实例可在不同线程中并发使用</item>
    ///   <item>同一实例需要外部同步</item>
    ///   <item>GetLastXxx 方法使用全局状态，非线程安全</item>
    /// </list>
    /// </remarks>
    /// <example>
    /// <code>
    /// using (var detector = new DefectDetectorAPI())
    /// {
    ///     detector.ImportTemplateFromFile("template.png");
    ///     detector.AddROI(100, 100, 200, 200, 0.85f);
    ///     var result = detector.Detect(testBitmap);
    ///     if (result.HasDefects)
    ///         Console.WriteLine($"发现 {result.DefectCount} 个瑕疵");
    /// }
    /// </code>
    /// </example>
    public class DefectDetectorAPI : IDisposable
    {
        #region DLL导入声明

        /// <summary>
        /// DLL文件名。
        /// 注意：DLL应放置在应用程序目录或系统PATH中。
        /// </summary>
        private const string DllName = "defect_detection.dll";

        // ==================== 基础接口 ====================
        // 用于创建、销毁检测器实例及模板/ROI管理

        /// <summary>创建检测器实例，返回句柄指针</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr CreateDetector();

        /// <summary>销毁检测器实例，释放非托管资源</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void DestroyDetector(IntPtr detector);

        /// <summary>从内存数据导入模板图像（BGR格式）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ImportTemplate(IntPtr detector,
            byte[] bitmap_data,
            int width, int height, int channels);

        /// <summary>从文件路径导入模板图像</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int ImportTemplateFromFile(IntPtr detector, string filepath);

        /// <summary>添加ROI区域，返回ROI ID</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int AddROI(IntPtr detector, int x, int y, int width, int height, float threshold);

        /// <summary>移除指定ID的ROI</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RemoveROI(IntPtr detector, int roi_id);

        /// <summary>设置指定ROI的相似度阈值</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetROIThreshold(IntPtr detector, int roi_id, float threshold);

        /// <summary>清除所有ROI区域</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ClearROIs(IntPtr detector);

        /// <summary>获取当前ROI数量</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetROICount(IntPtr detector);

        // ==================== 定位与变换接口 ====================
        // 用于图像对齐和位置校正

        /// <summary>设置用于定位的特征点坐标（x1,y1,x2,y2...格式）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetLocalizationPoints(IntPtr detector, float[] points, int point_count);

        /// <summary>启用/禁用自动定位功能</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void EnableAutoLocalization(IntPtr detector, int enable);

        /// <summary>设置外部提供的变换参数（偏移和旋转）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetExternalTransform(IntPtr detector, float offset_x, float offset_y, float rotation);

        /// <summary>清除外部变换设置，恢复自动计算</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ClearExternalTransform(IntPtr detector);

        // ==================== 对齐模式接口 ====================
        // 控制图像对齐的方式

        /// <summary>设置对齐模式（0=不对齐, 1=整图校正, 2=仅ROI校正）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetAlignmentMode(IntPtr detector, int mode);

        /// <summary>获取当前对齐模式</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetAlignmentMode(IntPtr detector);

        /// <summary>获取当前外部变换参数</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetExternalTransform(IntPtr detector,
            out float offset_x, out float offset_y, out float rotation, out int is_set);

        // ==================== 二值图像检测参数接口 ====================
        // 针对黑白/二值图像的优化检测参数

        /// <summary>设置二值图像检测参数</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetBinaryDetectionParams(IntPtr detector, ref BinaryDetectionParamsC params_);

        /// <summary>获取当前二值图像检测参数</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetBinaryDetectionParams(IntPtr detector, out BinaryDetectionParamsC params_);

        /// <summary>获取默认二值图像检测参数</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void GetDefaultBinaryDetectionParams(out BinaryDetectionParamsC params_);

        // ==================== 模板匹配器配置接口 ====================
        // 配置匹配算法和阈值

        /// <summary>设置匹配方法（0=NCC, 1=SSIM, 2=NCC+SSIM, 3=二值化）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetMatchMethod(IntPtr detector, int method);

        /// <summary>获取当前匹配方法</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetMatchMethod(IntPtr detector);

        /// <summary>设置二值化阈值（0-255）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetBinaryThreshold(IntPtr detector, int threshold);

        /// <summary>获取当前二值化阈值</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetBinaryThreshold(IntPtr detector);

        // ==================== 基础检测接口 ====================
        // 执行瑕疵检测，返回相似度和瑕疵数量
        // 返回值: 0=通过(无瑕疵), 1=未通过(有瑕疵), -1=检测失败

        /// <summary>
        /// 从内存图像执行检测。
        /// 返回值: 0=通过, 1=有瑕疵, -1=失败
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectDefects(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            [Out] float[] similarity_results,
            out int defect_count);

        /// <summary>
        /// 从文件执行检测。
        /// 返回值: 0=通过, 1=有瑕疵, -1=失败
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectDefectsFromFile(IntPtr detector,
            string filepath,
            [Out] float[] similarity_results,
            out int defect_count);

        // ==================== 瑕疵详情接口 ====================
        // 获取瑕疵的详细信息（位置、大小、类型等）

        /// <summary>从内存图像执行检测并返回瑕疵详情</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectDefectsEx(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            [Out] DefectInfoC[] defect_infos,
            int max_defects,
            out int actual_defect_count);

        /// <summary>从文件执行检测并返回瑕疵详情</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectDefectsFromFileEx(IntPtr detector,
            string filepath,
            [Out] DefectInfoC[] defect_infos,
            int max_defects,
            out int actual_defect_count);

        /// <summary>设置瑕疵检测阈值（灰度差值，0-255）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetDefectThreshold(IntPtr detector, int threshold);

        /// <summary>设置最小瑕疵尺寸（像素，小于此值的差异将被忽略）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetMinDefectSize(IntPtr detector, int min_size);

        // ==================== ROI信息获取接口 ====================

        /// <summary>获取指定索引的ROI信息</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetROIInfo(IntPtr detector, int index, out ROIInfoC roi_info);

        // ==================== 模板信息接口 ====================

        /// <summary>获取模板图像尺寸</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetTemplateSize(IntPtr detector, out int width, out int height, out int channels);

        /// <summary>检查是否已加载模板（返回1=已加载，0=未加载）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int IsTemplateLoaded(IntPtr detector);

        // ==================== 完整检测结果接口 ====================
        // 获取包含定位信息、时间统计的完整检测结果

        /// <summary>从内存图像检测并返回完整结果</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectWithFullResult(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            out DetectionResultC result);

        /// <summary>从文件检测并返回完整结果</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectFromFileWithFullResult(IntPtr detector,
            string filepath,
            out DetectionResultC result);

        /// <summary>
        /// 获取上次检测的每个ROI结果。
        /// 注意：使用全局状态，非线程安全，应在检测后立即调用。
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetLastROIResults(IntPtr detector, [Out] ROIResultC[] roi_results, int max_count, out int actual_count);

        /// <summary>
        /// 获取上次检测的定位信息。
        /// 注意：使用全局状态，非线程安全。
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetLastLocalizationInfo(IntPtr detector, out LocalizationInfoC loc_info);

        /// <summary>获取上次检测的定位耗时（毫秒）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastLocalizationTime(IntPtr detector);

        /// <summary>获取上次检测的ROI比对耗时（毫秒）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastROIComparisonTime(IntPtr detector);

        // ==================== 学习功能接口 ====================
        // 动态更新模板，适应产品变化

        /// <summary>从内存图像学习新模板（融合现有模板）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int LearnTemplate(IntPtr detector,
            byte[] new_template,
            int width, int height, int channels);

        /// <summary>重置模板到初始状态</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ResetTemplate(IntPtr detector);

        /// <summary>学习新模板并返回详细结果（质量评分和消息）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int LearnTemplateEx(IntPtr detector,
            byte[] new_template,
            int width, int height, int channels,
            out float quality_score,
            StringBuilder message,
            int message_size);

        /// <summary>从文件学习新模板</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int LearnTemplateFromFile(IntPtr detector, string filepath);

        // ==================== 参数配置接口 ====================
        // 通用参数设置，支持动态配置

        /// <summary>设置命名参数值</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int SetParameter(IntPtr detector, string param_name, float value);

        /// <summary>获取命名参数值</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern float GetParameter(IntPtr detector, string param_name);

        /// <summary>获取上次检测的总处理时间（毫秒）</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastProcessingTime(IntPtr detector);

        // ==================== 版本信息接口 ====================

        /// <summary>获取库版本字符串指针</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr GetLibraryVersion();

        /// <summary>获取构建信息字符串指针</summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr GetBuildInfo();

        #endregion

        #region 枚举和结构体定义

        /// <summary>
        /// 图像对齐模式枚举。
        /// 控制检测时图像位置校正的方式。
        /// </summary>
        public enum AlignmentMode
        {
            /// <summary>不进行对齐，直接使用原始坐标</summary>
            None = 0,
            /// <summary>整图校正模式，对整张图像应用变换后再检测</summary>
            FullImage = 1,
            /// <summary>ROI区域校正模式，仅对ROI区域应用变换（性能更优）</summary>
            RoiOnly = 2
        }

        /// <summary>
        /// 模板匹配方法枚举。
        /// 不同方法适用于不同场景，影响检测精度和速度。
        /// </summary>
        public enum MatchMethod
        {
            /// <summary>归一化互相关（NCC），适用于一般图像，速度快</summary>
            NCC = 0,
            /// <summary>结构相似性（SSIM），适用于需要考虑亮度/对比度变化的场景</summary>
            SSIM = 1,
            /// <summary>NCC + SSIM 组合，精度最高但速度较慢</summary>
            NCC_SSIM = 2,
            /// <summary>二值化对比，适用于黑白/印刷图像，速度最快</summary>
            Binary = 3
        }

        /// <summary>
        /// 瑕疵类型枚举。
        /// 描述检测到的瑕疵的具体类型。
        /// </summary>
        public enum DefectType
        {
            /// <summary>未知类型</summary>
            Unknown = 0,
            /// <summary>漏墨（墨水缺失）</summary>
            MissInk = 1,
            /// <summary>漏喷（应有内容未打印）</summary>
            MissPrint = 2,
            /// <summary>混合瑕疵（多种问题）</summary>
            Mixed = 3
        }

        #region C#友好的结构体

        /// <summary>
        /// 二值图像检测参数结构。
        /// 用于优化黑白/二值图像的检测效果。
        /// </summary>
        public struct BinaryDetectionParams
        {
            /// <summary>是否启用二值图像优化模式（0=禁用, 1=启用）</summary>
            public int Enabled;
            /// <summary>自动检测输入是否为二值图像（0=禁用, 1=启用）</summary>
            public int AutoDetectBinary;
            /// <summary>噪声过滤核大小（像素），去除小于此值的噪点</summary>
            public int NoiseFilterSize;
            /// <summary>边缘容错像素数，允许的边缘位置偏移</summary>
            public int EdgeTolerancePixels;
            /// <summary>忽略的边缘差异比例（0.0-1.0）</summary>
            public float EdgeDiffIgnoreRatio;
            /// <summary>最小显著差异面积（像素），小于此值的差异将被忽略</summary>
            public int MinSignificantArea;
            /// <summary>区域差异阈值（0.0-1.0）</summary>
            public float AreaDiffThreshold;
            /// <summary>总体相似度阈值（0.0-1.0），低于此值判定为不合格</summary>
            public float OverallSimilarityThreshold;
            /// <summary>边缘区域大瑕疵保留阈值（像素），默认500</summary>
            public int EdgeDefectSizeThreshold;
            /// <summary>边缘安全距离倍数，默认2.0</summary>
            public float EdgeDistanceMultiplier;
            /// <summary>二值化阈值，用于提取模板边缘，默认128</summary>
            public int BinaryThreshold;

            /// <summary>返回参数的描述字符串</summary>
            public override string ToString()
            {
                return $"二值检测参数: 启用={Enabled}, 自动检测={AutoDetectBinary}, 噪声过滤={NoiseFilterSize}像素";
            }
        }

        /// <summary>
        /// 定位信息结构。
        /// 描述测试图像相对于模板的位置变换信息。
        /// </summary>
        public struct LocalizationInfo
        {
            /// <summary>定位是否成功（0=失败, 1=成功）</summary>
            public int Success;
            /// <summary>X方向偏移（像素）</summary>
            public float OffsetX;
            /// <summary>Y方向偏移（像素）</summary>
            public float OffsetY;
            /// <summary>旋转角度（度，顺时针为正）</summary>
            public float RotationAngle;
            /// <summary>缩放比例（1.0=原始大小）</summary>
            public float Scale;
            /// <summary>匹配质量（0.0-1.0，越高越好）</summary>
            public float MatchQuality;
            /// <summary>特征匹配内点数量，数量越多定位越可靠</summary>
            public int InlierCount;
            /// <summary>是否来自外部传入的变换（0=自动计算, 1=外部传入）</summary>
            public int FromExternal;

            /// <summary>判断定位是否成功</summary>
            public bool IsSuccess => Success != 0;

            /// <summary>返回定位信息的描述字符串</summary>
            public override string ToString()
            {
                if (Success == 0)
                    return "定位失败";

                return $"定位成功: 偏移({OffsetX:F2},{OffsetY:F2}), 旋转{RotationAngle:F2}°, 缩放{Scale:F3}, 质量{MatchQuality:F3}";
            }
        }

        /// <summary>
        /// ROI（感兴趣区域）信息结构。
        /// 描述检测区域的位置、大小和检测参数。
        /// </summary>
        public struct ROIInfo
        {
            /// <summary>ROI的唯一标识符</summary>
            public int Id;
            /// <summary>左上角X坐标（像素）</summary>
            public int X;
            /// <summary>左上角Y坐标（像素）</summary>
            public int Y;
            /// <summary>区域宽度（像素）</summary>
            public int Width;
            /// <summary>区域高度（像素）</summary>
            public int Height;
            /// <summary>相似度阈值（0.0-1.0），低于此值判定为有瑕疵</summary>
            public float SimilarityThreshold;
            /// <summary>是否启用此ROI（0=禁用, 1=启用）</summary>
            public int Enabled;

            /// <summary>判断ROI是否启用</summary>
            public bool IsEnabled => Enabled != 0;

            /// <summary>获取ROI的矩形区域</summary>
            /// <returns>表示ROI位置和大小的Rectangle</returns>
            public Rectangle GetRectangle()
            {
                return new Rectangle(X, Y, Width, Height);
            }

            /// <summary>返回ROI信息的描述字符串</summary>
            public override string ToString()
            {
                return $"ROI{Id}: ({X},{Y}) {Width}x{Height} 阈值:{SimilarityThreshold:F3} 启用:{Enabled}";
            }
        }

        /// <summary>
        /// 单个ROI的检测结果结构。
        /// 描述该ROI区域的检测状态和瑕疵情况。
        /// </summary>
        public struct ROIResult
        {
            /// <summary>ROI的唯一标识符</summary>
            public int RoiId;
            /// <summary>实际计算的相似度（0.0-1.0）</summary>
            public float Similarity;
            /// <summary>判定阈值（0.0-1.0）</summary>
            public float Threshold;
            /// <summary>是否通过检测（0=未通过/有瑕疵, 1=通过）</summary>
            public int Passed;
            /// <summary>该ROI内检测到的瑕疵数量</summary>
            public int DefectCount;

            /// <summary>判断是否通过检测</summary>
            public bool IsPassed => Passed != 0;

            /// <summary>返回检测结果的描述字符串</summary>
            public override string ToString()
            {
                string status = Passed == 1 ? "通过" : "失败";
                return $"ROI{RoiId}: 相似度={Similarity:F3}, 阈值={Threshold:F3}, {status}, 瑕疵数={DefectCount}";
            }
        }

        /// <summary>
        /// 完整检测结果结构。
        /// 包含整体检测状态、时间统计和定位信息。
        /// </summary>
        public struct DetectionResult
        {
            /// <summary>总体是否通过（0=未通过/有瑕疵, 1=通过）</summary>
            public int OverallPassed;
            /// <summary>所有ROI的总瑕疵数量</summary>
            public int TotalDefectCount;
            /// <summary>总处理时间（毫秒）</summary>
            public double ProcessingTimeMs;
            /// <summary>定位校正时间（毫秒）</summary>
            public double LocalizationTimeMs;
            /// <summary>ROI比对时间（毫秒）</summary>
            public double ROIComparisonTimeMs;
            /// <summary>定位信息</summary>
            public LocalizationInfo Localization;
            /// <summary>ROI结果数量</summary>
            public int ROICount;

            /// <summary>判断是否整体通过</summary>
            public bool IsPassed => OverallPassed != 0;

            /// <summary>返回检测结果的描述字符串</summary>
            public override string ToString()
            {
                string status = OverallPassed == 1 ? "通过" : "失败";
                return $"检测{status}: 总瑕疵={TotalDefectCount}, 总耗时={ProcessingTimeMs:F1}ms, ROI数量={ROICount}";
            }
        }

        /// <summary>
        /// 单个瑕疵的详细信息结构。
        /// 描述瑕疵的位置、大小、类型和严重程度。
        /// </summary>
        public struct DefectInfo
        {
            /// <summary>瑕疵中心点X坐标（像素）</summary>
            public float X;
            /// <summary>瑕疵中心点Y坐标（像素）</summary>
            public float Y;
            /// <summary>瑕疵宽度（像素）</summary>
            public float Width;
            /// <summary>瑕疵高度（像素）</summary>
            public float Height;
            /// <summary>瑕疵面积（平方像素）</summary>
            public float Area;
            /// <summary>瑕疵类型</summary>
            public DefectType Type;
            /// <summary>严重程度（0.0-1.0，越高越严重）</summary>
            public float Severity;
            /// <summary>所属ROI的ID</summary>
            public int RoiId;

            public RectangleF GetBoundingBox()
            {
                return new RectangleF(X - Width / 2, Y - Height / 2, Width, Height);
            }

            public override string ToString()
            {
                return $"[ROI{RoiId}] ({X:F1},{Y:F1}) {Width:F1}x{Height:F1} {Type} 严重度:{Severity:F2} 面积:{Area:F1}";
            }
        }

        #endregion

        #region P/Invoke原生结构体

        [StructLayout(LayoutKind.Sequential)]
        internal struct BinaryDetectionParamsC
        {
            public int enabled;
            public int auto_detect_binary;
            public int noise_filter_size;
            public int edge_tolerance_pixels;
            public float edge_diff_ignore_ratio;
            public int min_significant_area;
            public float area_diff_threshold;
            public float overall_similarity_threshold;
            public int edge_defect_size_threshold;
            public float edge_distance_multiplier;
            public int binary_threshold;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct LocalizationInfoC
        {
            public int success;
            public float offset_x;
            public float offset_y;
            public float rotation_angle;
            public float scale;
            public float match_quality;
            public int inlier_count;
            public int from_external;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ROIInfoC
        {
            public int id;
            public int x;
            public int y;
            public int width;
            public int height;
            public float similarity_threshold;
            public int enabled;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ROIResultC
        {
            public int roi_id;
            public float similarity;
            public float threshold;
            public int passed;
            public int defect_count;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct DetectionResultC
        {
            public int overall_passed;
            public int total_defect_count;
            public double processing_time_ms;
            public double localization_time_ms;
            public double roi_comparison_time_ms;
            public LocalizationInfoC localization;
            public int roi_count;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct DefectInfoC
        {
            public float x;
            public float y;
            public float width;
            public float height;
            public float area;
            public int defect_type;
            public float severity;
            public int roi_id;
        }

        #endregion

        #endregion

        #region 成员变量

        private IntPtr _detectorHandle;
        private bool _disposed = false;

        #endregion

        #region 属性

        /// <summary>
        /// 检测器是否已初始化
        /// </summary>
        public bool IsInitialized => _detectorHandle != IntPtr.Zero && !_disposed;

        /// <summary>
        /// 获取库版本信息
        /// </summary>
        public string Version => Marshal.PtrToStringAnsi(GetLibraryVersion()) ?? "未知";

        /// <summary>
        /// 获取构建信息
        /// </summary>
        public string BuildInfo => Marshal.PtrToStringAnsi(GetBuildInfo()) ?? "未知";

        #endregion

        #region 构造函数和析构函数

        /// <summary>
        /// 创建检测器实例
        /// </summary>
        public DefectDetectorAPI()
        {
            _detectorHandle = CreateDetector();
            if (_detectorHandle == IntPtr.Zero)
            {
                throw new InvalidOperationException("无法创建检测器实例");
            }

            Console.WriteLine($"缺陷检测库版本: {Version}");
            Console.WriteLine($"构建信息: {BuildInfo}");
        }

        /// <summary>
        /// 析构函数
        /// </summary>
        ~DefectDetectorAPI()
        {
            Dispose(false);
        }

        /// <summary>
        /// 释放资源
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (_detectorHandle != IntPtr.Zero)
                {
                    DestroyDetector(_detectorHandle);
                    _detectorHandle = IntPtr.Zero;
                }
                _disposed = true;
            }
        }

        #endregion

        #region 模板管理

        /// <summary>
        /// 从文件导入模板图像
        /// </summary>
        public bool ImportTemplateFromFile(string filepath)
        {
            CheckDisposed();
            return ImportTemplateFromFile(_detectorHandle, filepath) == 0;
        }

        /// <summary>
        /// 从Bitmap导入模板图像
        /// </summary>
        public bool ImportTemplate(Bitmap bitmap)
        {
            CheckDisposed();

            var data = BitmapToBgrData(bitmap);
            return ImportTemplate(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels) == 0;
        }

        /// <summary>
        /// 检查模板是否已加载
        /// </summary>
        public bool IsTemplateLoaded()
        {
            CheckDisposed();
            return IsTemplateLoaded(_detectorHandle) == 1;
        }

        /// <summary>
        /// 获取模板图像尺寸
        /// </summary>
        public bool GetTemplateSize(out int width, out int height, out int channels)
        {
            CheckDisposed();
            return GetTemplateSize(_detectorHandle, out width, out height, out channels) == 0;
        }

        #endregion

        #region ROI管理

        /// <summary>
        /// 添加ROI区域
        /// </summary>
        public int AddROI(int x, int y, int width, int height, float threshold = 0.85f)
        {
            CheckDisposed();

            if (threshold < 0 || threshold > 1.0f)
                throw new ArgumentException("阈值必须在0.0到1.0之间", nameof(threshold));

            return AddROI(_detectorHandle, x, y, width, height, threshold);
        }

        /// <summary>
        /// 添加ROI区域（使用Rectangle）
        /// </summary>
        public int AddROI(Rectangle rect, float threshold = 0.85f)
        {
            return AddROI(rect.X, rect.Y, rect.Width, rect.Height, threshold);
        }

        /// <summary>
        /// 移除ROI区域
        /// </summary>
        public bool RemoveROI(int roiId)
        {
            CheckDisposed();
            return RemoveROI(_detectorHandle, roiId) == 0;
        }

        /// <summary>
        /// 设置ROI阈值
        /// </summary>
        public bool SetROIThreshold(int roiId, float threshold)
        {
            CheckDisposed();

            if (threshold < 0 || threshold > 1.0f)
                throw new ArgumentException("阈值必须在0.0到1.0之间", nameof(threshold));

            return SetROIThreshold(_detectorHandle, roiId, threshold) == 0;
        }

        /// <summary>
        /// 清除所有ROI
        /// </summary>
        public void ClearROIs()
        {
            CheckDisposed();
            ClearROIs(_detectorHandle);
        }

        /// <summary>
        /// 获取ROI数量
        /// </summary>
        public int GetROICount()
        {
            CheckDisposed();
            return GetROICount(_detectorHandle);
        }

        /// <summary>
        /// 获取指定索引的ROI信息
        /// </summary>
        public ROIInfo GetROIInfo(int index)
        {
            CheckDisposed();

            if (GetROIInfo(_detectorHandle, index, out ROIInfoC roiInfoC) != 0)
                throw new ArgumentException($"获取ROI信息失败，索引:{index}");

            return ConvertROIInfo(roiInfoC);
        }

        /// <summary>
        /// 获取所有ROI信息
        /// </summary>
        public ROIInfo[] GetAllROIInfo()
        {
            int count = GetROICount();
            ROIInfo[] roiInfos = new ROIInfo[count];

            for (int i = 0; i < count; i++)
            {
                roiInfos[i] = GetROIInfo(i);
            }

            return roiInfos;
        }

        #endregion

        #region 定位与变换设置

        /// <summary>
        /// 设置定位点
        /// </summary>
        public bool SetLocalizationPoints(PointF[] points)
        {
            CheckDisposed();

            if (points == null || points.Length == 0)
                return false;

            float[] pointArray = new float[points.Length * 2];
            for (int i = 0; i < points.Length; i++)
            {
                pointArray[i * 2] = points[i].X;
                pointArray[i * 2 + 1] = points[i].Y;
            }

            return SetLocalizationPoints(_detectorHandle, pointArray, points.Length) == 0;
        }

        /// <summary>
        /// 启用/禁用自动定位
        /// </summary>
        public void EnableAutoLocalization(bool enable)
        {
            CheckDisposed();
            EnableAutoLocalization(_detectorHandle, enable ? 1 : 0);
        }

        /// <summary>
        /// 设置外部变换矩阵
        /// </summary>
        public bool SetExternalTransform(float offsetX, float offsetY, float rotation)
        {
            CheckDisposed();
            return SetExternalTransform(_detectorHandle, offsetX, offsetY, rotation) == 0;
        }

        /// <summary>
        /// 清除外部变换设置
        /// </summary>
        public void ClearExternalTransform()
        {
            CheckDisposed();
            ClearExternalTransform(_detectorHandle);
        }

        /// <summary>
        /// 获取外部变换参数
        /// </summary>
        public ExternalTransform GetExternalTransform()
        {
            CheckDisposed();

            if (GetExternalTransform(_detectorHandle, out float offsetX, out float offsetY,
                out float rotation, out int isSet) != 0)
            {
                throw new InvalidOperationException("获取外部变换参数失败");
            }

            return new ExternalTransform
            {
                OffsetX = offsetX,
                OffsetY = offsetY,
                Rotation = rotation,
                IsSet = isSet == 1
            };
        }

        #endregion

        #region 对齐模式配置

        /// <summary>
        /// 设置对齐模式
        /// </summary>
        public bool SetAlignmentMode(AlignmentMode mode)
        {
            CheckDisposed();
            return SetAlignmentMode(_detectorHandle, (int)mode) == 0;
        }

        /// <summary>
        /// 获取当前对齐模式
        /// </summary>
        public AlignmentMode GetAlignmentMode()
        {
            CheckDisposed();
            int mode = GetAlignmentMode(_detectorHandle);
            return (AlignmentMode)mode;
        }

        #endregion

        #region 二值图像检测参数配置

        /// <summary>
        /// 设置二值图像检测参数
        /// </summary>
        public bool SetBinaryDetectionParams(BinaryDetectionParams parameters)
        {
            CheckDisposed();

            var paramsC = new BinaryDetectionParamsC
            {
                enabled = parameters.Enabled,
                auto_detect_binary = parameters.AutoDetectBinary,
                noise_filter_size = parameters.NoiseFilterSize,
                edge_tolerance_pixels = parameters.EdgeTolerancePixels,
                edge_diff_ignore_ratio = parameters.EdgeDiffIgnoreRatio,
                min_significant_area = parameters.MinSignificantArea,
                area_diff_threshold = parameters.AreaDiffThreshold,
                overall_similarity_threshold = parameters.OverallSimilarityThreshold,
                edge_defect_size_threshold = parameters.EdgeDefectSizeThreshold,
                edge_distance_multiplier = parameters.EdgeDistanceMultiplier,
                binary_threshold = parameters.BinaryThreshold
            };

            return SetBinaryDetectionParams(_detectorHandle, ref paramsC) == 0;
        }

        /// <summary>
        /// 获取二值图像检测参数
        /// </summary>
        public BinaryDetectionParams GetBinaryDetectionParams()
        {
            CheckDisposed();

            if (GetBinaryDetectionParams(_detectorHandle, out BinaryDetectionParamsC paramsC) != 0)
                throw new InvalidOperationException("获取二值检测参数失败");

            return new BinaryDetectionParams
            {
                Enabled = paramsC.enabled,
                AutoDetectBinary = paramsC.auto_detect_binary,
                NoiseFilterSize = paramsC.noise_filter_size,
                EdgeTolerancePixels = paramsC.edge_tolerance_pixels,
                EdgeDiffIgnoreRatio = paramsC.edge_diff_ignore_ratio,
                MinSignificantArea = paramsC.min_significant_area,
                AreaDiffThreshold = paramsC.area_diff_threshold,
                OverallSimilarityThreshold = paramsC.overall_similarity_threshold,
                EdgeDefectSizeThreshold = paramsC.edge_defect_size_threshold,
                EdgeDistanceMultiplier = paramsC.edge_distance_multiplier,
                BinaryThreshold = paramsC.binary_threshold
            };
        }

        /// <summary>
        /// 获取默认二值检测参数
        /// </summary>
        public static BinaryDetectionParams GetDefaultBinaryDetectionParams()
        {
            GetDefaultBinaryDetectionParams(out BinaryDetectionParamsC paramsC);

            return new BinaryDetectionParams
            {
                Enabled = paramsC.enabled,
                AutoDetectBinary = paramsC.auto_detect_binary,
                NoiseFilterSize = paramsC.noise_filter_size,
                EdgeTolerancePixels = paramsC.edge_tolerance_pixels,
                EdgeDiffIgnoreRatio = paramsC.edge_diff_ignore_ratio,
                MinSignificantArea = paramsC.min_significant_area,
                AreaDiffThreshold = paramsC.area_diff_threshold,
                OverallSimilarityThreshold = paramsC.overall_similarity_threshold,
                EdgeDefectSizeThreshold = paramsC.edge_defect_size_threshold,
                EdgeDistanceMultiplier = paramsC.edge_distance_multiplier,
                BinaryThreshold = paramsC.binary_threshold
            };
        }

        #endregion

        #region 模板匹配器配置

        /// <summary>
        /// 设置匹配方法
        /// </summary>
        public bool SetMatchMethod(MatchMethod method)
        {
            CheckDisposed();
            return SetMatchMethod(_detectorHandle, (int)method) == 0;
        }

        /// <summary>
        /// 获取当前匹配方法
        /// </summary>
        public MatchMethod GetMatchMethod()
        {
            CheckDisposed();
            int method = GetMatchMethod(_detectorHandle);
            return (MatchMethod)method;
        }

        /// <summary>
        /// 设置二值化阈值
        /// </summary>
        public bool SetBinaryThreshold(int threshold)
        {
            CheckDisposed();

            if (threshold < 0 || threshold > 255)
                throw new ArgumentException("阈值必须在0到255之间", nameof(threshold));

            return SetBinaryThreshold(_detectorHandle, threshold) == 0;
        }

        /// <summary>
        /// 获取二值化阈值
        /// </summary>
        public int GetBinaryThreshold()
        {
            CheckDisposed();
            return GetBinaryThreshold(_detectorHandle);
        }

        #endregion

        #region 检测功能

        /// <summary>
        /// 执行基础瑕疵检测。
        /// </summary>
        /// <param name="testImage">待检测的图像（不会被修改）</param>
        /// <returns>包含相似度和瑕疵数量的基础检测结果</returns>
        /// <exception cref="ObjectDisposedException">检测器已释放</exception>
        /// <exception cref="InvalidOperationException">未设置ROI区域</exception>
        /// <exception cref="ArgumentNullException">testImage为null</exception>
        /// <remarks>
        /// <para><b>线程安全：</b>返回的时间信息来自全局状态，多线程环境下可能不准确。</para>
        /// </remarks>
        /// <example>
        /// <code>
        /// var result = detector.Detect(bitmap);
        /// if (result.HasDefects)
        ///     Console.WriteLine($"发现 {result.DefectCount} 个瑕疵");
        /// </code>
        /// </example>
        public DetectionResultBasic Detect(Bitmap testImage)
        {
            CheckDisposed();

            var data = BitmapToBgrData(testImage);
            int roiCount = GetROICount();

            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先添加ROI");

            float[] similarities = new float[roiCount];
            int defectCount = 0;

            int resultCode = DetectDefects(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels,
                similarities,
                out defectCount);

            return new DetectionResultBasic
            {
                ResultCode = resultCode,
                Similarities = similarities,
                DefectCount = defectCount,
                ProcessingTime = GetLastProcessingTime(),
                LocalizationTime = GetLastLocalizationTime(),
                ROIComparisonTime = GetLastROIComparisonTime()
            };
        }

        /// <summary>
        /// 从图像文件执行基础瑕疵检测。
        /// </summary>
        /// <param name="filepath">图像文件路径（支持常见图像格式）</param>
        /// <returns>包含相似度和瑕疵数量的基础检测结果</returns>
        /// <exception cref="ObjectDisposedException">检测器已释放</exception>
        /// <exception cref="InvalidOperationException">未设置ROI区域</exception>
        /// <remarks>
        /// 直接从文件读取比通过 Bitmap 加载更高效，推荐用于批量处理。
        /// </remarks>
        public DetectionResultBasic DetectFromFile(string filepath)
        {
            CheckDisposed();

            int roiCount = GetROICount();

            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先添加ROI");

            float[] similarities = new float[roiCount];
            int defectCount = 0;

            int resultCode = DetectDefectsFromFile(_detectorHandle,
                filepath,
                similarities,
                out defectCount);

            return new DetectionResultBasic
            {
                ResultCode = resultCode,
                Similarities = similarities,
                DefectCount = defectCount,
                ProcessingTime = GetLastProcessingTime(),
                LocalizationTime = GetLastLocalizationTime(),
                ROIComparisonTime = GetLastROIComparisonTime()
            };
        }

        /// <summary>
        /// 执行详细瑕疵检测，获取每个瑕疵的具体信息。
        /// </summary>
        /// <param name="testImage">待检测的图像</param>
        /// <param name="maxDefects">最大返回瑕疵数量（默认1000）</param>
        /// <returns>包含瑕疵详细信息的检测结果</returns>
        /// <exception cref="ObjectDisposedException">检测器已释放</exception>
        /// <exception cref="InvalidOperationException">未设置ROI区域</exception>
        /// <remarks>
        /// 相比 <see cref="Detect"/> 方法，此方法返回每个瑕疵的位置、大小、类型等详细信息。
        /// </remarks>
        public DetectionResultDetailed DetectEx(Bitmap testImage, int maxDefects = 1000)
        {
            CheckDisposed();

            var data = BitmapToBgrData(testImage);
            int roiCount = GetROICount();

            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先添加ROI");

            DefectInfoC[] defectArrayC = new DefectInfoC[maxDefects];
            int actualDefectCount = 0;

            int resultCode = DetectDefectsEx(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels,
                defectArrayC,
                maxDefects,
                out actualDefectCount);

            // 转换到C#结构体
            DefectInfo[] defects = new DefectInfo[actualDefectCount];
            for (int i = 0; i < actualDefectCount; i++)
            {
                defects[i] = ConvertDefectInfo(defectArrayC[i]);
            }

            return new DetectionResultDetailed
            {
                ResultCode = resultCode,
                Defects = defects,
                ActualDefectCount = actualDefectCount,
                ProcessingTime = GetLastProcessingTime(),
                LocalizationTime = GetLastLocalizationTime(),
                ROIComparisonTime = GetLastROIComparisonTime()
            };
        }

        /// <summary>
        /// 从文件检测并返回详细信息
        /// </summary>
        public DetectionResultDetailed DetectFromFileEx(string filepath, int maxDefects = 1000)
        {
            CheckDisposed();

            int roiCount = GetROICount();

            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先添加ROI");

            DefectInfoC[] defectArrayC = new DefectInfoC[maxDefects];
            int actualDefectCount = 0;

            int resultCode = DetectDefectsFromFileEx(_detectorHandle,
                filepath,
                defectArrayC,
                maxDefects,
                out actualDefectCount);

            // 转换到C#结构体
            DefectInfo[] defects = new DefectInfo[actualDefectCount];
            for (int i = 0; i < actualDefectCount; i++)
            {
                defects[i] = ConvertDefectInfo(defectArrayC[i]);
            }

            return new DetectionResultDetailed
            {
                ResultCode = resultCode,
                Defects = defects,
                ActualDefectCount = actualDefectCount,
                ProcessingTime = GetLastProcessingTime(),
                LocalizationTime = GetLastLocalizationTime(),
                ROIComparisonTime = GetLastROIComparisonTime()
            };
        }

        /// <summary>
        /// 执行检测并获取完整结果（包含定位信息和时间统计）。
        /// </summary>
        /// <param name="testImage">待检测的图像</param>
        /// <returns>包含定位信息、时间统计的完整检测结果</returns>
        /// <exception cref="InvalidOperationException">检测失败</exception>
        /// <remarks>
        /// 此方法返回最全面的检测信息，适用于需要分析检测过程的场景。
        /// </remarks>
        public DetectionResult DetectWithFullResult(Bitmap testImage)
        {
            CheckDisposed();

            var data = BitmapToBgrData(testImage);

            if (DetectWithFullResult(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels,
                out DetectionResultC resultC) != 0)
            {
                throw new InvalidOperationException("获取完整检测结果失败");
            }

            return ConvertDetectionResult(resultC);
        }

        /// <summary>
        /// 从文件执行检测并获取完整结果。
        /// </summary>
        /// <param name="filepath">图像文件路径</param>
        /// <returns>包含定位信息、时间统计的完整检测结果</returns>
        /// <exception cref="InvalidOperationException">检测失败</exception>
        public DetectionResult DetectFromFileWithFullResult(string filepath)
        {
            CheckDisposed();

            if (DetectFromFileWithFullResult(_detectorHandle,
                filepath,
                out DetectionResultC resultC) != 0)
            {
                throw new InvalidOperationException("获取完整检测结果失败");
            }

            return ConvertDetectionResult(resultC);
        }

        /// <summary>
        /// 获取上次检测的每个ROI的详细结果。
        /// </summary>
        /// <returns>ROI检测结果数组</returns>
        /// <exception cref="InvalidOperationException">获取失败</exception>
        /// <remarks>
        /// <para><b>⚠️ 线程安全警告：</b></para>
        /// <para>此方法使用 C API 的全局状态存储结果，在多线程环境下可能返回其他线程的检测结果。</para>
        /// <para>建议在调用 Detect/DetectEx 后立即调用此方法。</para>
        /// </remarks>
        public ROIResult[] GetLastROIResults()
        {
            CheckDisposed();

            int roiCount = GetROICount();
            ROIResultC[] roiResultsC = new ROIResultC[roiCount];
            int actualCount = 0;

            if (GetLastROIResults(_detectorHandle, roiResultsC, roiCount, out actualCount) != 0)
                throw new InvalidOperationException("获取ROI结果失败");

            ROIResult[] roiResults = new ROIResult[actualCount];
            for (int i = 0; i < actualCount; i++)
            {
                roiResults[i] = new ROIResult
                {
                    RoiId = roiResultsC[i].roi_id,
                    Similarity = roiResultsC[i].similarity,
                    Threshold = roiResultsC[i].threshold,
                    Passed = roiResultsC[i].passed,
                    DefectCount = roiResultsC[i].defect_count
                };
            }

            return roiResults;
        }

        /// <summary>
        /// 获取上次检测的定位/对齐信息。
        /// </summary>
        /// <returns>定位信息结构</returns>
        /// <exception cref="InvalidOperationException">获取失败</exception>
        /// <remarks>
        /// <para><b>⚠️ 线程安全警告：</b>此方法使用全局状态，多线程环境下需谨慎使用。</para>
        /// </remarks>
        public LocalizationInfo GetLastLocalizationInfo()
        {
            CheckDisposed();

            if (GetLastLocalizationInfo(_detectorHandle, out LocalizationInfoC locInfoC) != 0)
                throw new InvalidOperationException("获取定位信息失败");

            return ConvertLocalizationInfo(locInfoC);
        }

        /// <summary>
        /// 设置瑕疵检测的灰度差异阈值。
        /// </summary>
        /// <param name="threshold">阈值（0-255），灰度差异超过此值的像素被视为瑕疵</param>
        /// <returns>设置是否成功</returns>
        /// <exception cref="ArgumentException">阈值超出有效范围</exception>
        public bool SetDefectThreshold(int threshold)
        {
            CheckDisposed();

            if (threshold < 0 || threshold > 255)
                throw new ArgumentException("阈值必须在0到255之间", nameof(threshold));

            return SetDefectThreshold(_detectorHandle, threshold) == 0;
        }

        /// <summary>
        /// 设置最小瑕疵尺寸，小于此值的差异将被忽略。
        /// </summary>
        /// <param name="minSize">最小尺寸（像素），建议值: 3-10</param>
        /// <returns>设置是否成功</returns>
        /// <exception cref="ArgumentException">minSize为负数</exception>
        public bool SetMinDefectSize(int minSize)
        {
            CheckDisposed();

            if (minSize < 0)
                throw new ArgumentException("最小尺寸不能为负数", nameof(minSize));

            return SetMinDefectSize(_detectorHandle, minSize) == 0;
        }

        #endregion

        #region 学习功能

        /// <summary>
        /// 一键学习新模板
        /// </summary>
        public bool LearnTemplate(Bitmap newTemplate)
        {
            CheckDisposed();

            var data = BitmapToBgrData(newTemplate);
            return LearnTemplate(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels) == 0;
        }

        /// <summary>
        /// 一键学习新模板并返回详细结果
        /// </summary>
        public LearningResult LearnTemplateEx(Bitmap newTemplate)
        {
            CheckDisposed();

            var data = BitmapToBgrData(newTemplate);
            float qualityScore = 0;
            StringBuilder message = new StringBuilder(256);

            int result = LearnTemplateEx(_detectorHandle,
                data.Bytes,
                data.Width,
                data.Height,
                data.Channels,
                out qualityScore,
                message,
                message.Capacity);

            return new LearningResult
            {
                Success = result == 0,
                QualityScore = qualityScore,
                Message = message.ToString()
            };
        }

        /// <summary>
        /// 从文件学习新模板
        /// </summary>
        public bool LearnTemplateFromFile(string filepath)
        {
            CheckDisposed();
            return LearnTemplateFromFile(_detectorHandle, filepath) == 0;
        }

        /// <summary>
        /// 重置模板到原始状态
        /// </summary>
        public void ResetTemplate()
        {
            CheckDisposed();
            ResetTemplate(_detectorHandle);
        }

        #endregion

        #region 参数配置

        /// <summary>
        /// 设置命名参数的值。
        /// </summary>
        /// <param name="paramName">参数名称</param>
        /// <param name="value">参数值</param>
        /// <returns>设置是否成功</returns>
        /// <remarks>
        /// 支持的参数名称请参考 C API 文档。
        /// </remarks>
        public bool SetParameter(string paramName, float value)
        {
            CheckDisposed();
            return SetParameter(_detectorHandle, paramName, value) == 0;
        }

        /// <summary>
        /// 获取命名参数的当前值。
        /// </summary>
        /// <param name="paramName">参数名称</param>
        /// <returns>参数值，若参数不存在返回默认值</returns>
        public float GetParameter(string paramName)
        {
            CheckDisposed();
            return GetParameter(_detectorHandle, paramName);
        }

        #endregion

        #region 性能监控
        // 注意：以下方法使用 C API 的全局状态，在多线程环境下可能返回其他线程的结果

        /// <summary>
        /// 获取上次检测的总处理时间。
        /// </summary>
        /// <returns>处理时间（毫秒）</returns>
        /// <remarks>
        /// <para><b>⚠️ 线程安全警告：</b>此值来自全局状态，多线程环境下需谨慎使用。</para>
        /// </remarks>
        public double GetLastProcessingTime()
        {
            CheckDisposed();
            return GetLastProcessingTime(_detectorHandle);
        }

        /// <summary>
        /// 获取上次检测的定位/对齐耗时。
        /// </summary>
        /// <returns>定位时间（毫秒）</returns>
        /// <remarks>
        /// <para><b>⚠️ 线程安全警告：</b>此值来自全局状态。</para>
        /// </remarks>
        public double GetLastLocalizationTime()
        {
            CheckDisposed();
            return GetLastLocalizationTime(_detectorHandle);
        }

        /// <summary>
        /// 获取上次检测的ROI比对耗时。
        /// </summary>
        /// <returns>ROI比对时间（毫秒）</returns>
        /// <remarks>
        /// <para><b>⚠️ 线程安全警告：</b>此值来自全局状态。</para>
        /// </remarks>
        public double GetLastROIComparisonTime()
        {
            CheckDisposed();
            return GetLastROIComparisonTime(_detectorHandle);
        }

        #endregion

        #region 工具方法

        /// <summary>
        /// 检查对象是否已释放，若已释放则抛出异常。
        /// </summary>
        /// <exception cref="ObjectDisposedException">对象已被释放时抛出</exception>
        private void CheckDisposed()
        {
            if (_disposed || _detectorHandle == IntPtr.Zero)
                throw new ObjectDisposedException("DefectDetector");
        }

        /// <summary>
        /// 将 Bitmap 转换为 BGR 格式的字节数组。
        /// </summary>
        /// <param name="bitmap">输入的位图（不会被修改或释放）</param>
        /// <returns>包含 BGR 数据的 ImageData 结构</returns>
        /// <remarks>
        /// <para>此方法执行以下操作：</para>
        /// <list type="number">
        ///   <item>若非24bpp格式，转换为24bpp RGB格式</item>
        ///   <item>正确处理Stride与图像宽度不等的情况（移除行填充字节）</item>
        ///   <item>将RGB通道顺序转换为BGR（OpenCV所需格式）</item>
        /// </list>
        /// </remarks>
        /// <exception cref="ArgumentNullException">bitmap为null时抛出</exception>
        private ImageData BitmapToBgrData(Bitmap bitmap)
        {
            if (bitmap == null)
                throw new ArgumentNullException(nameof(bitmap));

            // 是否需要释放临时创建的位图
            Bitmap workingBitmap = bitmap;
            bool needDispose = false;

            try
            {
                // 转换为24bpp RGB格式（如果需要）
                if (bitmap.PixelFormat != PixelFormat.Format24bppRgb)
                {
                    workingBitmap = new Bitmap(bitmap.Width, bitmap.Height, PixelFormat.Format24bppRgb);
                    needDispose = true;
                    using (Graphics g = Graphics.FromImage(workingBitmap))
                    {
                        g.DrawImage(bitmap, 0, 0, bitmap.Width, bitmap.Height);
                    }
                }

                BitmapData bitmapData = workingBitmap.LockBits(
                    new Rectangle(0, 0, workingBitmap.Width, workingBitmap.Height),
                    ImageLockMode.ReadOnly,
                    workingBitmap.PixelFormat);

                try
                {
                    int width = workingBitmap.Width;
                    int height = workingBitmap.Height;
                    int stride = bitmapData.Stride;
                    int bytesPerPixel = 3;
                    int rowBytes = width * bytesPerPixel;

                    // 创建紧凑的输出数组（不含行填充）
                    byte[] outputBytes = new byte[width * height * bytesPerPixel];

                    // 处理每一行，正确处理Stride
                    unsafe
                    {
                        byte* srcPtr = (byte*)bitmapData.Scan0;
                        for (int y = 0; y < height; y++)
                        {
                            int srcOffset = y * stride;
                            int dstOffset = y * rowBytes;

                            for (int x = 0; x < width; x++)
                            {
                                int srcPixel = srcOffset + x * bytesPerPixel;
                                int dstPixel = dstOffset + x * bytesPerPixel;

                                // RGB -> BGR 转换
                                // 源数据是RGB格式（GDI+ Format24bppRgb实际上是BGR）
                                // 但Marshal.Copy读取的顺序需要确认
                                outputBytes[dstPixel] = srcPtr[srcPixel + 2];     // B <- R
                                outputBytes[dstPixel + 1] = srcPtr[srcPixel + 1]; // G <- G
                                outputBytes[dstPixel + 2] = srcPtr[srcPixel];     // R <- B
                            }
                        }
                    }

                    return new ImageData
                    {
                        Bytes = outputBytes,
                        Width = width,
                        Height = height,
                        Channels = bytesPerPixel,
                        Stride = rowBytes  // 返回紧凑的stride
                    };
                }
                finally
                {
                    workingBitmap.UnlockBits(bitmapData);
                }
            }
            finally
            {
                // 释放临时创建的位图
                if (needDispose && workingBitmap != null)
                {
                    workingBitmap.Dispose();
                }
            }
        }

        private ROIInfo ConvertROIInfo(ROIInfoC roiInfoC)
        {
            return new ROIInfo
            {
                Id = roiInfoC.id,
                X = roiInfoC.x,
                Y = roiInfoC.y,
                Width = roiInfoC.width,
                Height = roiInfoC.height,
                SimilarityThreshold = roiInfoC.similarity_threshold,
                Enabled = roiInfoC.enabled
            };
        }

        private LocalizationInfo ConvertLocalizationInfo(LocalizationInfoC locInfoC)
        {
            return new LocalizationInfo
            {
                Success = locInfoC.success,
                OffsetX = locInfoC.offset_x,
                OffsetY = locInfoC.offset_y,
                RotationAngle = locInfoC.rotation_angle,
                Scale = locInfoC.scale,
                MatchQuality = locInfoC.match_quality,
                InlierCount = locInfoC.inlier_count,
                FromExternal = locInfoC.from_external
            };
        }

        private DefectInfo ConvertDefectInfo(DefectInfoC defectInfoC)
        {
            return new DefectInfo
            {
                X = defectInfoC.x,
                Y = defectInfoC.y,
                Width = defectInfoC.width,
                Height = defectInfoC.height,
                Area = defectInfoC.area,
                Type = (DefectType)defectInfoC.defect_type,
                Severity = defectInfoC.severity,
                RoiId = defectInfoC.roi_id
            };
        }

        private DetectionResult ConvertDetectionResult(DetectionResultC resultC)
        {
            return new DetectionResult
            {
                OverallPassed = resultC.overall_passed,
                TotalDefectCount = resultC.total_defect_count,
                ProcessingTimeMs = resultC.processing_time_ms,
                LocalizationTimeMs = resultC.localization_time_ms,
                ROIComparisonTimeMs = resultC.roi_comparison_time_ms,
                Localization = ConvertLocalizationInfo(resultC.localization),
                ROICount = resultC.roi_count
            };
        }

        #endregion

        #region 辅助类

        /// <summary>
        /// 内部使用的图像数据结构。
        /// 存储从Bitmap转换得到的BGR格式原始数据。
        /// </summary>
        private struct ImageData
        {
            /// <summary>BGR格式的像素数据</summary>
            public byte[] Bytes;
            /// <summary>图像宽度（像素）</summary>
            public int Width;
            /// <summary>图像高度（像素）</summary>
            public int Height;
            /// <summary>通道数（通常为3）</summary>
            public int Channels;
            /// <summary>每行字节数</summary>
            public int Stride;
        }

        /// <summary>
        /// 外部变换参数类。
        /// 用于传入外部计算的位置校正参数。
        /// </summary>
        public class ExternalTransform
        {
            /// <summary>X方向偏移（像素）</summary>
            public float OffsetX { get; set; }
            /// <summary>Y方向偏移（像素）</summary>
            public float OffsetY { get; set; }
            /// <summary>旋转角度（度，顺时针为正）</summary>
            public float Rotation { get; set; }
            /// <summary>是否已设置外部变换</summary>
            public bool IsSet { get; set; }

            /// <summary>返回变换参数的描述字符串</summary>
            public override string ToString()
            {
                return IsSet ?
                    $"外部变换: 偏移({OffsetX:F2},{OffsetY:F2}), 旋转{Rotation:F2}°" :
                    "外部变换: 未设置";
            }
        }

        /// <summary>
        /// 基础检测结果类。
        /// 包含检测状态、相似度和时间统计信息。
        /// </summary>
        public class DetectionResultBasic
        {
            /// <summary>
            /// 结果代码。
            /// <para>0 = 通过（无瑕疵）</para>
            /// <para>1 = 未通过（有瑕疵）</para>
            /// <para>-1 = 检测失败</para>
            /// </summary>
            public int ResultCode { get; set; }

            /// <summary>
            /// 各ROI相似度数组
            /// </summary>
            public float[] Similarities { get; set; }

            /// <summary>
            /// 总瑕疵数量
            /// </summary>
            public int DefectCount { get; set; }

            /// <summary>
            /// 总处理时间(毫秒)
            /// </summary>
            public double ProcessingTime { get; set; }

            /// <summary>
            /// 定位校正时间(毫秒)
            /// </summary>
            public double LocalizationTime { get; set; }

            /// <summary>
            /// ROI比对时间(毫秒)
            /// </summary>
            public double ROIComparisonTime { get; set; }

            /// <summary>
            /// 是否通过检测（无瑕疵）
            /// </summary>
            public bool IsPassed => ResultCode == 0;

            /// <summary>
            /// 检测是否成功（无系统错误）
            /// </summary>
            public bool IsSuccess => ResultCode != -1;

            /// <summary>
            /// 是否有瑕疵
            /// </summary>
            public bool HasDefects => ResultCode == 1 && DefectCount > 0;

            /// <summary>
            /// 获取平均相似度
            /// </summary>
            public float GetAverageSimilarity()
            {
                if (Similarities == null || Similarities.Length == 0)
                    return 0.0f;

                return Similarities.Average();
            }

            public override string ToString()
            {
                if (ResultCode == -1)
                    return "检测失败";
                else if (ResultCode == 0)
                    return $"检测通过, 无瑕疵, 总耗时:{ProcessingTime:F1}ms";
                else
                    return $"检测未通过, 发现{DefectCount}个瑕疵, 总耗时:{ProcessingTime:F1}ms";
            }
        }

        /// <summary>
        /// 详细检测结果类。
        /// 继承自 <see cref="DetectionResultBasic"/>，额外包含每个瑕疵的详细信息。
        /// </summary>
        public class DetectionResultDetailed : DetectionResultBasic
        {
            /// <summary>
            /// 检测到的瑕疵详细信息数组。
            /// </summary>
            public DefectInfo[] Defects { get; set; }

            /// <summary>
            /// 实际检测到的瑕疵数量。
            /// </summary>
            public int ActualDefectCount { get; set; }

            /// <summary>
            /// 获取指定ROI中的所有瑕疵。
            /// </summary>
            /// <param name="roiId">ROI的ID</param>
            /// <returns>该ROI中的瑕疵数组，若无瑕疵返回空数组</returns>
            public DefectInfo[] GetDefectsByROI(int roiId)
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.RoiId == roiId).ToArray();
            }

            /// <summary>
            /// 获取指定类型的所有瑕疵。
            /// </summary>
            /// <param name="type">瑕疵类型</param>
            /// <returns>该类型的瑕疵数组，若无瑕疵返回空数组</returns>
            public DefectInfo[] GetDefectsByType(DefectType type)
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.Type == type).ToArray();
            }

            /// <summary>
            /// 获取严重程度高于指定阈值的瑕疵。
            /// </summary>
            /// <param name="minSeverity">最小严重程度（0.0-1.0）</param>
            /// <returns>满足条件的瑕疵数组</returns>
            public DefectInfo[] GetDefectsBySeverity(float minSeverity)
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.Severity >= minSeverity).ToArray();
            }

            /// <summary>返回检测结果的描述字符串</summary>
            public override string ToString()
            {
                if (ResultCode == -1)
                    return "检测失败";
                else if (ResultCode == 0)
                    return $"检测通过, 无瑕疵, 总耗时:{ProcessingTime:F1}ms";
                else
                    return $"检测未通过, 发现{ActualDefectCount}个瑕疵, 总耗时:{ProcessingTime:F1}ms";
            }
        }

        /// <summary>
        /// 模板学习结果类。
        /// 描述学习操作的成功状态、质量评估和消息。
        /// </summary>
        public class LearningResult
        {
            /// <summary>学习是否成功</summary>
            public bool Success { get; set; }
            /// <summary>学习质量评分（0.0-1.0），越高表示学习效果越好</summary>
            public float QualityScore { get; set; }
            /// <summary>学习过程消息（成功时为统计信息，失败时为错误原因）</summary>
            public string Message { get; set; }

            /// <summary>返回学习结果的描述字符串</summary>
            public override string ToString()
            {
                return Success ?
                    $"学习成功, 质量评分:{QualityScore:F3}, {Message}" :
                    $"学习失败: {Message}";
            }
        }

        #endregion
    }
}