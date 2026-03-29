using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.IO;

namespace DetectionUITest
{
    /// <summary>
    /// 喷印瑕疵检测系统 C# 封装
    /// 版本: 3.6 (与demo_dll.cpp保持一致)
    /// </summary>
    public class DefectDetectorAPI : IDisposable
    {
        // ==================== 常量定义（与demo_dll.cpp一致） ====================
        public const int DEFAULT_BINARY_THRESHOLD = 30;
        public const int DEFAULT_DEFECT_SIZE_THRESHOLD = 100;
        public const int DEFAULT_ALIGN_DIFF_THRESHOLD = 128;
        public const int DEFAULT_MIN_SIGNIFICANT_AREA = 20;
        public const float DEFAULT_OVERALL_SIMILARITY_THRESHOLD = 0.90f;
        public const int DEFAULT_EDGE_DEFECT_SIZE_THRESHOLD = 500;
        public const float DEFAULT_EDGE_DISTANCE_MULTIPLIER = 2.0f;
        #region DLL导入声明

        private const string DllName = "defect_detection.dll";

        // ==================== 基础接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr CreateDetector();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void DestroyDetector(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ImportTemplate(IntPtr detector,
            byte[] bitmap_data,
            int width, int height, int channels);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int ImportTemplateFromFile(IntPtr detector, string filepath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int AddROI(IntPtr detector, int x, int y, int width, int height, float threshold);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int AddROIWithParams(IntPtr detector, int x, int y, int width, int height, float threshold, ref DetectionParamsC params_);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int RemoveROI(IntPtr detector, int roi_id);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetROIThreshold(IntPtr detector, int roi_id, float threshold);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ClearROIs(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetROICount(IntPtr detector);

        // ==================== 定位与变换接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetLocalizationPoints(IntPtr detector, float[] points, int point_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void EnableAutoLocalization(IntPtr detector, int enable);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetExternalTransform(IntPtr detector, float offset_x, float offset_y, float rotation);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ClearExternalTransform(IntPtr detector);

        // ==================== 对齐模式接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetAlignmentMode(IntPtr detector, int mode);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetAlignmentMode(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetExternalTransform(IntPtr detector,
            out float offset_x, out float offset_y, out float rotation, out int is_set);

        // ==================== 二值图像检测参数接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetBinaryDetectionParams(IntPtr detector, ref BinaryDetectionParamsC params_);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetBinaryDetectionParams(IntPtr detector, out BinaryDetectionParamsC params_);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void GetDefaultBinaryDetectionParams(out BinaryDetectionParamsC params_);

        // ==================== 模板匹配器配置接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetMatchMethod(IntPtr detector, int method);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetMatchMethod(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetBinaryThreshold(IntPtr detector, int threshold);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetBinaryThreshold(IntPtr detector);

        // ==================== 基础检测接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectDefects(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            [Out] float[] similarity_results,
            out int defect_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectDefectsFromFile(IntPtr detector,
            string filepath,
            [Out] float[] similarity_results,
            out int defect_count);

        // ==================== 瑕疵详情接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectDefectsEx(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            [Out] DefectInfoC[] defect_infos,
            int max_defects,
            out int actual_defect_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectDefectsFromFileEx(IntPtr detector,
            string filepath,
            [Out] DefectInfoC[] defect_infos,
            int max_defects,
            out int actual_defect_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetDefectThreshold(IntPtr detector, int threshold);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int SetMinDefectSize(IntPtr detector, int min_size);

        // ==================== ROI信息获取接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetROIInfo(IntPtr detector, int index, out ROIInfoC roi_info);

        // ==================== 模板信息接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetTemplateSize(IntPtr detector, out int width, out int height, out int channels);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int IsTemplateLoaded(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ExtractROITemplates(IntPtr detector);

        // ==================== 完整检测结果接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int DetectWithFullResult(IntPtr detector,
            byte[] test_image,
            int width, int height, int channels,
            out DetectionResultC result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int DetectFromFileWithFullResult(IntPtr detector,
            string filepath,
            out DetectionResultC result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetLastROIResults(IntPtr detector, [Out] ROIResultC[] roi_results, int max_count, out int actual_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetLastLocalizationInfo(IntPtr detector, out LocalizationInfoC loc_info);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastLocalizationTime(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastROIComparisonTime(IntPtr detector);

        // ==================== 学习功能接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int LearnTemplate(IntPtr detector,
            byte[] new_template,
            int width, int height, int channels);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ResetTemplate(IntPtr detector);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern int LearnTemplateEx(IntPtr detector,
            byte[] new_template,
            int width, int height, int channels,
            out float quality_score,
            [MarshalAs(UnmanagedType.LPStr)] StringBuilder message,
            int message_size);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int LearnTemplateFromFile(IntPtr detector, string filepath);

        // ==================== 参数配置接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern int SetParameter(IntPtr detector, string param_name, float value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern float GetParameter(IntPtr detector, string param_name);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetLastProcessingTime(IntPtr detector);

        // ==================== 版本信息接口 ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr GetLibraryVersion();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr GetBuildInfo();

        // ==================== 日志控制接口 (新增) ====================

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "SetAPILogFile")]
        private static extern void SetApiLogFileNative(string filepath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "EnableAPILog")]
        private static extern void EnableApiLogNative(int enabled);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ClearAPILog")]
        private static extern void ClearApiLogNative();

        #endregion

        #region 枚举和结构体定义

        /// <summary>
        /// 对齐模式枚举
        /// </summary>
        public enum AlignmentMode
        {
            None = 0,           // 不进行对齐
            FullImage = 1,      // 整图校正模式
            RoiOnly = 2         // ROI区域校正模式
        }

        /// <summary>
        /// 匹配方法枚举
        /// </summary>
        public enum MatchMethod
        {
            NCC = 0,            // 归一化互相关
            SSIM = 1,           // 结构相似性
            NCC_SSIM = 2,       // NCC + SSIM组合
            Binary = 3          // 二值化对比
        }

        /// <summary>
        /// 瑕疵类型枚举
        /// </summary>
        public enum DefectType
        {
            Unknown = 0,        // 未知
            MissInk = 1,        // 漏墨
            MissPrint = 2,      // 漏喷
            Mixed = 3           // 混合瑕疵
        }

        #region C#友好的结构体

        /// <summary>
        /// 二值图像检测参数结构
        /// </summary>
        public class BinaryDetectionParams
        {
            public int Enabled { get; set; }                       // 是否启用二值图像优化模式
            public int AutoDetectBinary { get; set; }              // 自动检测输入是否为二值图像
            public int NoiseFilterSize { get; set; }                // 噪声过滤核大小
            public int EdgeTolerancePixels { get; set; }            // 边缘容错像素数
            public float EdgeDiffIgnoreRatio { get; set; }          // 忽略的边缘差异比例
            public int MinSignificantArea { get; set; }             // 最小显著差异面积
            public float AreaDiffThreshold { get; set; }            // 区域差异阈值
            public float OverallSimilarityThreshold { get; set; }   // 总体相似度阈值
            public int EdgeDefectSizeThreshold { get; set; }        // 边缘区域大瑕疵保留阈值
            public float EdgeDistanceMultiplier { get; set; }       // 边缘安全距离倍数
            public int BinaryThreshold { get; set; }                // 二值化阈值

            public override string ToString()
            {
                return $"二值检测参数: 启用={Enabled}, 自动检测={AutoDetectBinary}, 噪声过滤={NoiseFilterSize}像素";
            }
        }

        /// <summary>
        /// 定位信息结构
        /// </summary>
        public struct LocalizationInfo
        {
            public int Success;                 // 定位是否成功
            public float OffsetX;               // X方向偏移
            public float OffsetY;               // Y方向偏移
            public float RotationAngle;         // 旋转角度（度）
            public float Scale;                 // 缩放比例
            public float MatchQuality;          // 匹配质量 (0.0-1.0)
            public int InlierCount;             // 内点数量
            public int FromExternal;            // 是否来自外部传入的变换

            public override string ToString()
            {
                if (Success == 0)
                    return "定位失败";
                return $"定位成功: 偏移({OffsetX:F2},{OffsetY:F2}), 旋转{RotationAngle:F2}°, 缩放{Scale:F3}, 质量{MatchQuality:F3}";
            }
        }

        /// <summary>
        /// ROI区域信息结构
        /// </summary>
        public struct ROIInfo
        {
            public int Id;                      // ROI的ID
            public int X;                       // 左上角X坐标
            public int Y;                       // 左上角Y坐标
            public int Width;                   // 宽度
            public int Height;                   // 高度
            public float SimilarityThreshold;   // 相似度阈值
            public int Enabled;                  // 是否启用

            public Rectangle GetRectangle()
            {
                return new Rectangle(X, Y, Width, Height);
            }

            public override string ToString()
            {
                return $"ROI{Id}: ({X},{Y}) {Width}x{Height} 阈值:{SimilarityThreshold:F3} 启用:{Enabled}";
            }
        }

        /// <summary>
        /// ROI检测结果结构
        /// </summary>
        public struct ROIResult
        {
            public int RoiId;                   // ROI的ID
            public float Similarity;            // 相似度 (0.0-1.0)
            public float Threshold;             // 阈值
            public int Passed;                   // 是否通过
            public int DefectCount;              // 该ROI的瑕疵数量

            public override string ToString()
            {
                string status = Passed == 1 ? "通过" : "失败";
                return $"ROI{RoiId}: 相似度={Similarity:F3}, 阈值={Threshold:F3}, {status}, 瑕疵数={DefectCount}";
            }
        }

        /// <summary>
        /// 完整检测结果结构
        /// </summary>
        public struct DetectionResult
        {
            /// <summary>
            /// 原始返回码: 0=通过(无瑕疵), 1=未通过(有瑕疵), -1=检测执行失败
            /// </summary>
            public int ResultCode;

            /// <summary>
            /// 总体是否通过 (0=未通过, 1=通过)
            /// </summary>
            public int OverallPassed;

            /// <summary>
            /// 总瑕疵数量
            /// </summary>
            public int TotalDefectCount;

            /// <summary>
            /// 总处理时间（毫秒）
            /// </summary>
            public double ProcessingTimeMs;

            /// <summary>
            /// 定位校正时间（毫秒）
            /// </summary>
            public double LocalizationTimeMs;

            /// <summary>
            /// ROI比对时间（毫秒）
            /// </summary>
            public double ROIComparisonTimeMs;

            /// <summary>
            /// 定位信息
            /// </summary>
            public LocalizationInfo Localization;

            /// <summary>
            /// ROI结果数量
            /// </summary>
            public int ROICount;

            /// <summary>
            /// 检测是否通过（无瑕疵）
            /// <para>等价于 ResultCode == 0</para>
            /// </summary>
            public bool IsPassed => ResultCode == 0;

            /// <summary>
            /// 检测是否成功执行（无系统错误）
            /// <para>等价于 ResultCode != -1</para>
            /// </summary>
            public bool IsSuccess => ResultCode != -1;

            /// <summary>
            /// 是否有瑕疵（检测成功但未通过）
            /// <para>等价于 ResultCode == 1</para>
            /// </summary>
            public bool HasDefects => ResultCode == 1;

            public override string ToString()
            {
                switch (ResultCode)
                {
                    case -1:
                        return "检测执行失败";
                    case 0:
                        return string.Format("检测通过, 无瑕疵, 总耗时={0:F1}ms, ROI数量={1}", ProcessingTimeMs, ROICount);
                    case 1:
                        return string.Format("检测未通过, 发现{0}个瑕疵, 总耗时={1:F1}ms, ROI数量={2}", TotalDefectCount, ProcessingTimeMs, ROICount);
                    default:
                        return string.Format("未知状态(ResultCode={0})", ResultCode);
                }
            }
        }

        /// <summary>
        /// 瑕疵信息结构体
        /// </summary>
        public struct DefectInfo
        {
            public float X;                     // 瑕疵中心点X坐标
            public float Y;                     // 瑕疵中心点Y坐标
            public float Width;                  // 瑕疵宽度
            public float Height;                 // 瑕疵高度
            public float Area;                    // 瑕疵面积
            public DefectType Type;               // 瑕疵类型
            public float Severity;                // 严重程度 (0.0-1.0)
            public int RoiId;                     // 所属ROI的ID

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

        /// <summary>
        /// ROI检测参数结构（C接口）- 与demo_dll.cpp一致
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        internal struct DetectionParamsC
        {
            public int blur_kernel_size;       // 高斯模糊核大小
            public int binary_threshold;       // 二值化阈值
            public int min_defect_size;        // 最小瑕疵尺寸
            public int detect_black_on_white;  // 检测白底黑点 (0=禁用, 1=启用)
            public int detect_white_on_black;  // 检测黑底白点 (0=禁用, 1=启用)
            public float similarity_threshold; // 相似度阈值
            public int morphology_kernel_size; // 形态学核大小
        }

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

        private volatile IntPtr _detectorHandle;
        private int _isDisposedFlag = 0;

        #endregion

        #region 属性

        /// <summary>
        /// 检测器是否已初始化
        /// </summary>
        public bool IsInitialized => _detectorHandle != IntPtr.Zero && _isDisposedFlag == 0;

        /// <summary>
        /// 获取库版本信息
        /// </summary>
        public string Version
        {
            get
            {
                CheckDisposed();
                IntPtr ptr = GetLibraryVersion();
                return ptr == IntPtr.Zero ? "未知" : Marshal.PtrToStringAnsi(ptr) ?? "未知";
            }
        }

        /// <summary>
        /// 获取构建信息
        /// </summary>
        public string BuildInfo
        {
            get
            {
                CheckDisposed();
                IntPtr ptr = GetBuildInfo();
                return ptr == IntPtr.Zero ? "未知" : Marshal.PtrToStringAnsi(ptr) ?? "未知";
            }
        }

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
                throw new InvalidOperationException("无法创建检测器实例，请检查defect_detection.dll是否存在且兼容");
            }

            Console.WriteLine($"缺陷检测库版本: {Version}");
            Console.WriteLine($"构建信息: {BuildInfo}");
        }

        ~DefectDetectorAPI()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (Interlocked.CompareExchange(ref _isDisposedFlag, 1, 0) == 0)
            {
                IntPtr handle = Interlocked.Exchange(ref _detectorHandle, IntPtr.Zero);
                if (handle != IntPtr.Zero)
                {
                    try { DestroyDetector(handle); }
                    catch (Exception ex) { System.Diagnostics.Debug.WriteLine($"销毁检测器时出错: {ex.Message}"); }
                }
            }
        }

        #endregion

        #region 日志控制 (新增)

        /// <summary>
        /// 设置API日志文件路径
        /// </summary>
        /// <param name="filepath">日志文件路径（UTF-8编码），如果为NULL则使用默认路径 "api_debug.log"</param>
        public void SetAPILogFile(string filepath = null)
        {
            CheckDisposed();
            SetApiLogFileNative(filepath);
        }

        /// <summary>
        /// 启用或禁用API日志
        /// </summary>
        /// <param name="enabled">true启用日志，false禁用日志</param>
        public void EnableAPILog(bool enabled)
        {
            CheckDisposed();
            EnableApiLogNative(enabled ? 1 : 0);
        }

        /// <summary>
        /// 清除当前日志文件内容
        /// </summary>
        public void ClearAPILog()
        {
            CheckDisposed();
            ClearApiLogNative();
        }


        #endregion

        #region 模板管理

        public bool ImportTemplateFromFile(string filepath)
        {
            CheckDisposed();
            if (filepath == null) throw new ArgumentNullException(nameof(filepath));
            if (string.IsNullOrWhiteSpace(filepath)) throw new ArgumentException("文件路径不能为空", nameof(filepath));
            if (!File.Exists(filepath)) throw new FileNotFoundException("模板文件不存在", filepath);

            return ImportTemplateFromFile(_detectorHandle, filepath) == 0;
        }

        public bool ImportTemplate(Bitmap bitmap)
        {
            CheckDisposed();
            if (bitmap == null) throw new ArgumentNullException(nameof(bitmap));

            using (var imageData = BitmapToBgrData(bitmap))
            {
                return ImportTemplate(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels) == 0;
            }
        }

        public bool IsTemplateLoaded()
        {
            CheckDisposed();
            return IsTemplateLoaded(_detectorHandle) == 1;
        }

        /// <summary>
        /// 提取ROI模板图像
        /// </summary>
        /// <returns>是否成功</returns>
        /// <remarks>
        /// 在添加ROI后调用此函数，从已加载的模板中提取各ROI区域的模板图像。
        /// 这是与demo_cpp行为一致的关键步骤。
        /// </remarks>
        public bool ExtractROITemplates()
        {
            CheckDisposed();
            if (!IsTemplateLoaded())
                throw new InvalidOperationException("模板未加载，无法提取ROI模板");
            
            return ExtractROITemplates(_detectorHandle) == 0;
        }

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
            if (width <= 0 || height <= 0)
                throw new ArgumentException("ROI宽度和高度必须大于0", nameof(width));
            if (x < 0 || y < 0)
                throw new ArgumentException("ROI坐标不能为负数", nameof(x));

            int roiId = AddROI(_detectorHandle, x, y, width, height, threshold);
            if (roiId < 0)
                throw new InvalidOperationException($"添加ROI失败，错误码:{roiId}");
            return roiId;
        }

        /// <summary>
        /// 添加ROI区域（带详细参数）- 与demo_dll.cpp的ROIManager::addROI行为一致
        /// </summary>
        /// <param name="x">左上角X坐标</param>
        /// <param name="y">左上角Y坐标</param>
        /// <param name="width">宽度</param>
        /// <param name="height">高度</param>
        /// <param name="threshold">相似度阈值</param>
        /// <param name="params">ROI检测参数</param>
        /// <returns>ROI的ID</returns>
        public int AddROIWithParams(int x, int y, int width, int height, float threshold, DetectionParams parameters)
        {
            CheckDisposed();

            if (threshold < 0 || threshold > 1.0f)
                throw new ArgumentException("阈值必须在0.0到1.0之间", nameof(threshold));
            if (width <= 0 || height <= 0)
                throw new ArgumentException("ROI宽度和高度必须大于0", nameof(width));
            if (x < 0 || y < 0)
                throw new ArgumentException("ROI坐标不能为负数", nameof(x));
            if (parameters == null)
                throw new ArgumentNullException(nameof(parameters));

            var paramsC = new DetectionParamsC
            {
                blur_kernel_size = parameters.BlurKernelSize,
                binary_threshold = parameters.BinaryThreshold,
                min_defect_size = parameters.MinDefectSize,
                detect_black_on_white = parameters.DetectBlackOnWhite ? 1 : 0,
                detect_white_on_black = parameters.DetectWhiteOnBlack ? 1 : 0,
                similarity_threshold = parameters.SimilarityThreshold,
                morphology_kernel_size = parameters.MorphologyKernelSize
            };

            int roiId = AddROIWithParams(_detectorHandle, x, y, width, height, threshold, ref paramsC);
            if (roiId < 0)
                throw new InvalidOperationException($"添加ROI失败，错误码:{roiId}");
            return roiId;
        }

        /// <summary>
        /// 添加ROI区域（使用默认参数）- 与demo_dll.cpp行为一致
        /// </summary>
        /// <param name="x">左上角X坐标</param>
        /// <param name="y">左上角Y坐标</param>
        /// <param name="width">宽度</param>
        /// <param name="height">高度</param>
        /// <param name="threshold">相似度阈值</param>
        /// <param name="binaryThreshold">二值化阈值（默认30）</param>
        /// <param name="blurKernelSize">高斯模糊核大小（默认3）</param>
        /// <param name="minDefectSize">最小瑕疵尺寸（默认100）</param>
        /// <returns>ROI的ID</returns>
        public int AddROIWithDefaultParams(int x, int y, int width, int height, float threshold,
            int binaryThreshold = DEFAULT_BINARY_THRESHOLD,
            int blurKernelSize = 3,
            int minDefectSize = DEFAULT_DEFECT_SIZE_THRESHOLD)
        {
            var defaultParams = new DetectionParams
            {
                BinaryThreshold = binaryThreshold,
                BlurKernelSize = blurKernelSize,
                MinDefectSize = minDefectSize,
                DetectBlackOnWhite = true,
                DetectWhiteOnBlack = true,
                SimilarityThreshold = threshold,
                MorphologyKernelSize = 3
            };
            return AddROIWithParams(x, y, width, height, threshold, defaultParams);
        }

        public int AddROI(Rectangle rect, float threshold = 0.85f)
        {
            return AddROI(rect.X, rect.Y, rect.Width, rect.Height, threshold);
        }

        /// <summary>
        /// ROI检测参数类 - 与demo_dll.cpp的DetectionParamsC对应
        /// </summary>
        public class DetectionParams
        {
            public int BlurKernelSize { get; set; } = 3;           // 高斯模糊核大小
            public int BinaryThreshold { get; set; } = 30;          // 二值化阈值
            public int MinDefectSize { get; set; } = 100;           // 最小瑕疵尺寸
            public bool DetectBlackOnWhite { get; set; } = true;    // 检测白底黑点
            public bool DetectWhiteOnBlack { get; set; } = true;    // 检测黑底白点
            public float SimilarityThreshold { get; set; } = 0.85f; // 相似度阈值
            public int MorphologyKernelSize { get; set; } = 3;      // 形态学核大小
        }

        public bool RemoveROI(int roiId)
        {
            CheckDisposed();
            if (roiId < 0) throw new ArgumentException("ROI ID不能为负数", nameof(roiId));
            return RemoveROI(_detectorHandle, roiId) == 0;
        }

        public bool SetROIThreshold(int roiId, float threshold)
        {
            CheckDisposed();
            if (roiId < 0) throw new ArgumentException("ROI ID不能为负数", nameof(roiId));
            if (threshold < 0 || threshold > 1.0f)
                throw new ArgumentException("阈值必须在0.0到1.0之间", nameof(threshold));

            return SetROIThreshold(_detectorHandle, roiId, threshold) == 0;
        }

        public void ClearROIs()
        {
            CheckDisposed();
            ClearROIs(_detectorHandle);
        }

        public int GetROICount()
        {
            CheckDisposed();
            return GetROICount(_detectorHandle);
        }

        public ROIInfo GetROIInfo(int index)
        {
            CheckDisposed();
            if (index < 0) throw new ArgumentOutOfRangeException(nameof(index), "索引不能为负数");

            int result = GetROIInfo(_detectorHandle, index, out ROIInfoC roiInfoC);
            if (result != 0)
                throw new InvalidOperationException($"获取ROI信息失败，索引:{index}，错误码:{result}");

            return ConvertROIInfo(roiInfoC);
        }

        public ROIInfo[] GetAllROIInfo()
        {
            int count = GetROICount();
            ROIInfo[] roiInfos = new ROIInfo[count];
            for (int i = 0; i < count; i++)
                roiInfos[i] = GetROIInfo(i);
            return roiInfos;
        }

        #endregion

        #region 定位与变换设置

        /// <summary>
        /// 设置定位点
        /// </summary>
        /// <param name="points">定位点数组，至少需要3个点</param>
        /// <returns>是否成功</returns>
        public bool SetLocalizationPoints(PointF[] points)
        {
            CheckDisposed();
            if (points == null || points.Length == 0)
                throw new ArgumentException("定位点数组不能为空", nameof(points));
            if (points.Length < 3)
                throw new ArgumentException("至少需要3个定位点", nameof(points));

            float[] pointArray = new float[points.Length * 2];
            for (int i = 0; i < points.Length; i++)
            {
                pointArray[i * 2] = points[i].X;
                pointArray[i * 2 + 1] = points[i].Y;
            }

            return SetLocalizationPoints(_detectorHandle, pointArray, points.Length) == 0;
        }

        public void EnableAutoLocalization(bool enable)
        {
            CheckDisposed();
            EnableAutoLocalization(_detectorHandle, enable ? 1 : 0);
        }

        public bool SetExternalTransform(float offsetX, float offsetY, float rotation)
        {
            CheckDisposed();
            return SetExternalTransform(_detectorHandle, offsetX, offsetY, rotation) == 0;
        }

        public void ClearExternalTransform()
        {
            CheckDisposed();
            ClearExternalTransform(_detectorHandle);
        }

        public ExternalTransform GetExternalTransform()
        {
            CheckDisposed();

            int result = GetExternalTransform(_detectorHandle, out float offsetX, out float offsetY, out float rotation, out int isSet);
            if (result != 0)
                throw new InvalidOperationException($"获取外部变换参数失败，错误码:{result}");

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

        public bool SetAlignmentMode(AlignmentMode mode)
        {
            CheckDisposed();
            return SetAlignmentMode(_detectorHandle, (int)mode) == 0;
        }

        public AlignmentMode GetAlignmentMode()
        {
            CheckDisposed();
            int mode = GetAlignmentMode(_detectorHandle);
            if (mode < 0 || mode > 2)
                throw new InvalidOperationException($"DLL返回了无效的对齐模式值: {mode}");
            return (AlignmentMode)mode;
        }

        #endregion

        #region 二值图像检测参数配置

        public bool SetBinaryDetectionParams(BinaryDetectionParams parameters)
        {
            CheckDisposed();
            if (parameters == null) throw new ArgumentNullException(nameof(parameters));

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

        public BinaryDetectionParams GetBinaryDetectionParams()
        {
            CheckDisposed();

            int result = GetBinaryDetectionParams(_detectorHandle, out BinaryDetectionParamsC paramsC);
            if (result != 0)
                throw new InvalidOperationException($"获取二值检测参数失败，错误码:{result}");

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

        public bool SetMatchMethod(MatchMethod method)
        {
            CheckDisposed();
            if (method < MatchMethod.NCC || method > MatchMethod.Binary)
                throw new ArgumentOutOfRangeException(nameof(method));
            return SetMatchMethod(_detectorHandle, (int)method) == 0;
        }

        public MatchMethod GetMatchMethod()
        {
            CheckDisposed();
            int method = GetMatchMethod(_detectorHandle);
            if (method < 0 || method > 3)
                throw new InvalidOperationException($"DLL返回了无效的匹配方法值: {method}");
            return (MatchMethod)method;
        }

        public bool SetBinaryThreshold(int threshold)
        {
            CheckDisposed();
            return SetBinaryThreshold(_detectorHandle, threshold) == 0;
        }

        public int GetBinaryThreshold()
        {
            CheckDisposed();
            return GetBinaryThreshold(_detectorHandle);
        }

        #endregion

        #region 检测功能

        public DetectionResultBasic Detect(Bitmap testImage)
        {
            CheckDisposed();
            if (testImage == null) throw new ArgumentNullException(nameof(testImage));

            using (var imageData = BitmapToBgrData(testImage))
            {
                int roiCount = GetROICount();
                if (roiCount == 0)
                    throw new InvalidOperationException("未设置ROI区域，请先调用AddROI添加ROI");

                float[] similarities = new float[roiCount];
                int defectCount = 0;

                int resultCode = DetectDefects(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels, similarities, out defectCount);

                return new DetectionResultBasic
                {
                    ResultCode = resultCode,
                    Similarities = similarities,
                    DefectCount = defectCount,
                    ProcessingTime = GetLastProcessingTimeInternal(),
                    LocalizationTime = GetLastLocalizationTimeInternal(),
                    ROIComparisonTime = GetLastROIComparisonTimeInternal()
                };
            }
        }

        public DetectionResultBasic DetectFromFile(string filepath)
        {
            CheckDisposed();
            if (filepath == null) throw new ArgumentNullException(nameof(filepath));
            if (string.IsNullOrWhiteSpace(filepath)) throw new ArgumentException("文件路径不能为空", nameof(filepath));
            if (!File.Exists(filepath)) throw new FileNotFoundException("测试图像文件不存在", filepath);

            int roiCount = GetROICount();
            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先调用AddROI添加ROI");

            float[] similarities = new float[roiCount];
            int defectCount = 0;

            int resultCode = DetectDefectsFromFile(_detectorHandle, filepath, similarities, out defectCount);

            return new DetectionResultBasic
            {
                ResultCode = resultCode,
                Similarities = similarities,
                DefectCount = defectCount,
                ProcessingTime = GetLastProcessingTimeInternal(),
                LocalizationTime = GetLastLocalizationTimeInternal(),
                ROIComparisonTime = GetLastROIComparisonTimeInternal()
            };
        }

        public DetectionResultDetailed DetectEx(Bitmap testImage, int maxDefects = 1000)
        {
            CheckDisposed();
            if (testImage == null) throw new ArgumentNullException(nameof(testImage));
            if (maxDefects <= 0) throw new ArgumentException("最大瑕疵数必须大于0", nameof(maxDefects));
            if (maxDefects > 10000) throw new ArgumentException("最大瑕疵数不能超过10000", nameof(maxDefects));

            using (var imageData = BitmapToBgrData(testImage))
            {
                int roiCount = GetROICount();
                if (roiCount == 0)
                    throw new InvalidOperationException("未设置ROI区域，请先调用AddROI添加ROI");

                DefectInfoC[] defectArrayC = new DefectInfoC[maxDefects];
                int actualDefectCount = 0;

                int resultCode = DetectDefectsEx(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels, defectArrayC, maxDefects, out actualDefectCount);

                // 防御性编程：确保不越界
                if (actualDefectCount > maxDefects)
                {
                    System.Diagnostics.Debug.WriteLine($"警告：C++返回的瑕疵数({actualDefectCount})超过最大值({maxDefects})，已截断");
                    actualDefectCount = maxDefects;
                }

                DefectInfo[] defects = new DefectInfo[actualDefectCount];
                for (int i = 0; i < actualDefectCount; i++)
                    defects[i] = ConvertDefectInfo(defectArrayC[i]);

                return new DetectionResultDetailed
                {
                    ResultCode = resultCode,
                    Defects = defects,
                    ActualDefectCount = actualDefectCount,
                    ProcessingTime = GetLastProcessingTimeInternal(),
                    LocalizationTime = GetLastLocalizationTimeInternal(),
                    ROIComparisonTime = GetLastROIComparisonTimeInternal()
                };
            }
        }

        public DetectionResultDetailed DetectFromFileEx(string filepath, int maxDefects = 1000)
        {
            CheckDisposed();
            if (filepath == null) throw new ArgumentNullException(nameof(filepath));
            if (maxDefects <= 0) throw new ArgumentException("最大瑕疵数必须大于0", nameof(maxDefects));
            if (maxDefects > 10000) throw new ArgumentException("最大瑕疵数不能超过10000", nameof(maxDefects));
            if (!File.Exists(filepath)) throw new FileNotFoundException("测试图像文件不存在", filepath);

            int roiCount = GetROICount();
            if (roiCount == 0)
                throw new InvalidOperationException("未设置ROI区域，请先调用AddROI添加ROI");

            DefectInfoC[] defectArrayC = new DefectInfoC[maxDefects];
            int actualDefectCount = 0;

            int resultCode = DetectDefectsFromFileEx(_detectorHandle, filepath, defectArrayC, maxDefects, out actualDefectCount);

            if (actualDefectCount > maxDefects)
            {
                System.Diagnostics.Debug.WriteLine($"警告：C++返回的瑕疵数({actualDefectCount})超过最大值({maxDefects})，已截断");
                actualDefectCount = maxDefects;
            }

            DefectInfo[] defects = new DefectInfo[actualDefectCount];
            for (int i = 0; i < actualDefectCount; i++)
                defects[i] = ConvertDefectInfo(defectArrayC[i]);

            return new DetectionResultDetailed
            {
                ResultCode = resultCode,
                Defects = defects,
                ActualDefectCount = actualDefectCount,
                ProcessingTime = GetLastProcessingTimeInternal(),
                LocalizationTime = GetLastLocalizationTimeInternal(),
                ROIComparisonTime = GetLastROIComparisonTimeInternal()
            };
        }

        /// <summary>
        /// 检测并获取完整结果
        /// </summary>
        /// <param name="testImage">测试图像</param>
        /// <returns>检测结果（ResultCode: 0=通过, 1=未通过, -1=执行失败）</returns>
        /// <exception cref="InvalidOperationException">检测执行失败时抛出（ResultCode=-1）</exception>
        public DetectionResult DetectWithFullResult(Bitmap testImage)
        {
            CheckDisposed();
            if (testImage == null) throw new ArgumentNullException(nameof(testImage));

            using (var imageData = BitmapToBgrData(testImage))
            {
                int result = DetectWithFullResult(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels, out DetectionResultC resultC);

                // 只有-1表示执行失败，0和1都是正常业务结果
                if (result == -1)
                {
                    throw new InvalidOperationException("检测执行失败。可能原因：1) 模板未加载 2) 图像格式不支持 3) 内部处理错误");
                }

                return ConvertDetectionResult(resultC, result);
            }
        }

        /// <summary>
        /// 从文件检测并获取完整结果
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>检测结果（ResultCode: 0=通过, 1=未通过, -1=执行失败）</returns>
        /// <exception cref="InvalidOperationException">检测执行失败时抛出（ResultCode=-1）</exception>
        public DetectionResult DetectFromFileWithFullResult(string filepath)
        {
            CheckDisposed();
            if (filepath == null) throw new ArgumentNullException(nameof(filepath));
            if (string.IsNullOrWhiteSpace(filepath)) throw new ArgumentException("文件路径不能为空", nameof(filepath));
            if (!File.Exists(filepath)) throw new FileNotFoundException("测试图像文件不存在", filepath);

            int result = DetectFromFileWithFullResult(_detectorHandle, filepath, out DetectionResultC resultC);

            // 只有-1表示执行失败
            if (result == -1)
            {
                throw new InvalidOperationException("检测执行失败。可能原因：1) 模板未加载 2) 文件无法读取 3) 图像格式不支持 4) 内部处理错误");
            }

            return ConvertDetectionResult(resultC, result);
        }

        /// <summary>
        /// 尝试检测并获取完整结果（不抛出异常）
        /// </summary>
        /// <param name="testImage">测试图像</param>
        /// <param name="result">检测结果</param>
        /// <returns>是否成功执行检测（true=执行成功，无论是否通过；false=执行失败）</returns>
        public bool TryDetectWithFullResult(Bitmap testImage, out DetectionResult result)
        {
            result = default;
            try
            {
                result = DetectWithFullResult(testImage);
                return result.IsSuccess; // ResultCode != -1
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"检测失败: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 尝试从文件检测并获取完整结果（不抛出异常）
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <param name="result">检测结果</param>
        /// <returns>是否成功执行检测（true=执行成功，无论是否通过；false=执行失败）</returns>
        public bool TryDetectFromFileWithFullResult(string filepath, out DetectionResult result)
        {
            result = default;
            try
            {
                result = DetectFromFileWithFullResult(filepath);
                return result.IsSuccess; // ResultCode != -1
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"检测失败: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 获取上次检测的每个ROI结果
        /// </summary>
        /// <remarks>
        /// ⚠️ 线程安全警告：此函数依赖全局状态。如果在多线程环境中使用，
        /// 确保在调用DetectXxx后立即调用此方法，且避免多线程并发检测。
        /// </remarks>
        public ROIResult[] GetLastROIResults()
        {
            CheckDisposed();

            int roiCount = GetROICount();
            if (roiCount == 0) return Array.Empty<ROIResult>();

            ROIResultC[] roiResultsC = new ROIResultC[roiCount];
            int actualCount = 0;

            int result = GetLastROIResults(_detectorHandle, roiResultsC, roiCount, out actualCount);
            if (result != 0)
                throw new InvalidOperationException($"获取ROI结果失败，错误码:{result}");

            if (actualCount > roiCount)
            {
                System.Diagnostics.Debug.WriteLine($"警告：C++返回的ROI结果数({actualCount})超过ROI总数({roiCount})，已截断");
                actualCount = roiCount;
            }

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
        /// 获取上次检测的定位信息
        /// </summary>
        /// <remarks>
        /// ⚠️ 线程安全警告：同GetLastROIResults
        /// </remarks>
        public LocalizationInfo GetLastLocalizationInfo()
        {
            CheckDisposed();

            int result = GetLastLocalizationInfo(_detectorHandle, out LocalizationInfoC locInfoC);
            if (result != 0)
                throw new InvalidOperationException($"获取定位信息失败，错误码:{result}");

            return ConvertLocalizationInfo(locInfoC);
        }

        /// <summary>
        /// 尝试获取上次检测的定位信息（不抛出异常）
        /// </summary>
        /// <param name="locInfo">定位信息</param>
        /// <returns>是否成功获取</returns>
        public bool TryGetLastLocalizationInfo(out LocalizationInfo locInfo)
        {
            locInfo = default;
            try
            {
                locInfo = GetLastLocalizationInfo();
                return true;
            }
            catch
            {
                return false;
            }
        }

        public bool SetDefectThreshold(int threshold)
        {
            CheckDisposed();
            if (threshold < 0 || threshold > 255)
                throw new ArgumentException("阈值必须在0到255之间", nameof(threshold));
            return SetDefectThreshold(_detectorHandle, threshold) == 0;
        }

        public bool SetMinDefectSize(int minSize)
        {
            CheckDisposed();
            if (minSize < 0) throw new ArgumentException("最小尺寸不能为负数", nameof(minSize));
            return SetMinDefectSize(_detectorHandle, minSize) == 0;
        }

        #endregion

        #region 学习功能

        public bool LearnTemplate(Bitmap newTemplate)
        {
            CheckDisposed();
            if (newTemplate == null) throw new ArgumentNullException(nameof(newTemplate));

            using (var imageData = BitmapToBgrData(newTemplate))
            {
                return LearnTemplate(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels) == 0;
            }
        }

        public LearningResult LearnTemplateEx(Bitmap newTemplate)
        {
            CheckDisposed();
            if (newTemplate == null) throw new ArgumentNullException(nameof(newTemplate));

            using (var imageData = BitmapToBgrData(newTemplate))
            {
                float qualityScore = 0;
                StringBuilder message = new StringBuilder(256);

                int result = LearnTemplateEx(_detectorHandle, imageData.Bytes, imageData.Width, imageData.Height, imageData.Channels, out qualityScore, message, message.Capacity);

                return new LearningResult
                {
                    Success = result == 0,
                    QualityScore = qualityScore,
                    Message = message.ToString()
                };
            }
        }

        public bool LearnTemplateFromFile(string filepath)
        {
            CheckDisposed();
            if (filepath == null) throw new ArgumentNullException(nameof(filepath));
            if (string.IsNullOrWhiteSpace(filepath)) throw new ArgumentException("文件路径不能为空", nameof(filepath));
            if (!File.Exists(filepath)) throw new FileNotFoundException("模板文件不存在", filepath);

            return LearnTemplateFromFile(_detectorHandle, filepath) == 0;
        }

        public void ResetTemplate()
        {
            CheckDisposed();
            ResetTemplate(_detectorHandle);
        }

        #endregion

        #region 参数配置

        public bool SetParameter(string paramName, float value)
        {
            CheckDisposed();
            if (paramName == null) throw new ArgumentNullException(nameof(paramName));
            if (string.IsNullOrWhiteSpace(paramName)) throw new ArgumentException("参数名称不能为空", nameof(paramName));

            return SetParameter(_detectorHandle, paramName, value) == 0;
        }

        /// <summary>
        /// 获取参数值
        /// </summary>
        /// <param name="paramName">参数名称</param>
        /// <returns>参数值</returns>
        /// <exception cref="InvalidOperationException">获取失败时抛出</exception>
        /// <remarks>
        /// 注意：C++ API在错误时返回0.0f，无法区分"参数值为0"和"参数不存在"。
        /// 如果需要精确判断，建议先确认参数存在或使用TryGetParameter。
        /// </remarks>
        public float GetParameter(string paramName)
        {
            if (!TryGetParameter(paramName, out float value))
                throw new InvalidOperationException($"获取参数'{paramName}'失败，参数可能不存在");
            return value;
        }

        /// <summary>
        /// 尝试获取参数（不抛出异常）
        /// </summary>
        /// <param name="paramName">参数名称</param>
        /// <param name="value">参数值</param>
        /// <returns>是否成功获取</returns>
        /// <remarks>
        /// 注意：由于C++ API限制，无法可靠区分"参数值为0"和"获取失败"。
        /// 此方法仅在内部错误时返回false，参数值为0时也会返回true。
        /// </remarks>
        public bool TryGetParameter(string paramName, out float value)
        {
            value = 0.0f;
            try
            {
                CheckDisposed();
                if (string.IsNullOrWhiteSpace(paramName)) return false;

                value = GetParameter(_detectorHandle, paramName);
                return true;
            }
            catch
            {
                return false;
            }
        }

        #endregion

        #region 性能监控

        public double GetLastProcessingTime()
        {
            CheckDisposed();
            return GetLastProcessingTimeInternal();
        }

        public double GetLastLocalizationTime()
        {
            CheckDisposed();
            return GetLastLocalizationTimeInternal();
        }

        public double GetLastROIComparisonTime()
        {
            CheckDisposed();
            return GetLastROIComparisonTimeInternal();
        }

        private double GetLastProcessingTimeInternal()
        {
            IntPtr handle = _detectorHandle;
            return handle != IntPtr.Zero ? GetLastProcessingTime(handle) : 0;
        }

        private double GetLastLocalizationTimeInternal()
        {
            IntPtr handle = _detectorHandle;
            return handle != IntPtr.Zero ? GetLastLocalizationTime(handle) : 0;
        }

        private double GetLastROIComparisonTimeInternal()
        {
            IntPtr handle = _detectorHandle;
            return handle != IntPtr.Zero ? GetLastROIComparisonTime(handle) : 0;
        }

        #endregion

        #region 工具方法

        private void CheckDisposed()
        {
            if (_isDisposedFlag != 0)
                throw new ObjectDisposedException(nameof(DefectDetectorAPI), "检测器已被释放");
        }

        private DisposableImageData BitmapToBgrData(Bitmap bitmap)
        {
            if (bitmap == null) throw new ArgumentNullException(nameof(bitmap));

            Bitmap bitmapToProcess = bitmap;
            Bitmap convertedBitmap = null;
            BitmapData bitmapData = null;

            try
            {
                if (bitmap.PixelFormat != PixelFormat.Format24bppRgb)
                {
                    convertedBitmap = new Bitmap(bitmap.Width, bitmap.Height, PixelFormat.Format24bppRgb);
                    using (Graphics g = Graphics.FromImage(convertedBitmap))
                    {
                        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.HighQuality;
                        g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
                        g.DrawImage(bitmap, 0, 0, bitmap.Width, bitmap.Height);
                    }
                    bitmapToProcess = convertedBitmap;
                }

                Rectangle rect = new Rectangle(0, 0, bitmapToProcess.Width, bitmapToProcess.Height);
                bitmapData = bitmapToProcess.LockBits(rect, ImageLockMode.ReadOnly, bitmapToProcess.PixelFormat);

                if (bitmapData == null)
                    throw new InvalidOperationException("无法锁定图像数据");

                int byteCount = bitmapData.Stride * bitmapData.Height;
                if (byteCount <= 0)
                    throw new InvalidOperationException($"无效的图像数据大小: {byteCount}");

                byte[] bytes = new byte[byteCount];
                Marshal.Copy(bitmapData.Scan0, bytes, 0, byteCount);

                // RGB -> BGR 转换
                for (int y = 0; y < bitmapData.Height; y++)
                {
                    int rowStart = y * bitmapData.Stride;
                    for (int x = 0; x < bitmapData.Width; x++)
                    {
                        int idx = rowStart + x * 3;
                        byte temp = bytes[idx];
                        bytes[idx] = bytes[idx + 2];
                        bytes[idx + 2] = temp;
                    }
                }

                return new DisposableImageData
                {
                    Bytes = bytes,
                    Width = bitmapToProcess.Width,
                    Height = bitmapToProcess.Height,
                    Channels = 3,
                    Stride = bitmapData.Stride,
                    LockedBitmap = bitmapToProcess,
                    BitmapData = bitmapData,
                    ConvertedBitmap = convertedBitmap
                };
            }
            catch
            {
                CleanupBitmapResources(bitmapData, bitmapToProcess, convertedBitmap);
                throw;
            }
        }

        private void CleanupBitmapResources(BitmapData bitmapData, Bitmap lockedBitmap, Bitmap convertedBitmap)
        {
            if (bitmapData != null && lockedBitmap != null)
            {
                try { lockedBitmap.UnlockBits(bitmapData); }
                catch { }
            }
            convertedBitmap?.Dispose();
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

        private DetectionResult ConvertDetectionResult(DetectionResultC resultC, int resultCode)
        {
            return new DetectionResult
            {
                ResultCode = resultCode,
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

        private class DisposableImageData : IDisposable
        {
            public byte[] Bytes;
            public int Width;
            public int Height;
            public int Channels;
            public int Stride;
            public Bitmap LockedBitmap;
            public BitmapData BitmapData;
            public Bitmap ConvertedBitmap;
            private int _isDisposed = 0;

            public void Dispose()
            {
                if (Interlocked.CompareExchange(ref _isDisposed, 1, 0) == 0)
                {
                    if (BitmapData != null && LockedBitmap != null)
                    {
                        try { LockedBitmap.UnlockBits(BitmapData); }
                        catch { }
                        BitmapData = null;
                    }
                    ConvertedBitmap?.Dispose();
                    ConvertedBitmap = null;
                    Bytes = null;
                    LockedBitmap = null;
                }
            }
        }

        /// <summary>
        /// 外部变换参数
        /// </summary>
        public class ExternalTransform
        {
            public float OffsetX { get; set; }
            public float OffsetY { get; set; }
            public float Rotation { get; set; }
            public bool IsSet { get; set; }

            public override string ToString()
            {
                return IsSet ? $"外部变换: 偏移({OffsetX:F2},{OffsetY:F2}), 旋转{Rotation:F2}°" : "外部变换: 未设置";
            }
        }

        /// <summary>
        /// 基础检测结果
        /// </summary>
        public class DetectionResultBasic
        {
            /// <summary>
            /// 结果代码: 0=通过(无瑕疵), 1=未通过(有瑕疵), -1=检测执行失败
            /// </summary>
            public int ResultCode { get; set; }

            public float[] Similarities { get; set; }
            public int DefectCount { get; set; }
            public double ProcessingTime { get; set; }
            public double LocalizationTime { get; set; }
            public double ROIComparisonTime { get; set; }

            public bool IsPassed => ResultCode == 0;
            public bool IsSuccess => ResultCode != -1;
            public bool HasDefects => ResultCode == 1;

            public float GetAverageSimilarity()
            {
                if (Similarities == null || Similarities.Length == 0) return 0.0f;
                return Similarities.Average();
            }

            public override string ToString()
            {
                switch (ResultCode)
                {
                    case -1:
                        return "检测执行失败";
                    case 0:
                        return string.Format("检测通过, 无瑕疵, 总耗时:{0:F1}ms", ProcessingTime);
                    case 1:
                        return string.Format("检测未通过, 发现{0}个瑕疵, 总耗时:{1:F1}ms", DefectCount, ProcessingTime);
                    default:
                        return string.Format("未知状态({0})", ResultCode);
                }
            }
        }

        /// <summary>
        /// 详细检测结果（含瑕疵信息）- 与demo_dll.cpp检测逻辑一致
        /// </summary>
        public class DetectionResultDetailed : DetectionResultBasic
        {
            public DefectInfo[] Defects { get; set; }
            public int ActualDefectCount { get; set; }

            /// <summary>
            /// 重大瑕疵判定阈值（与demo_dll.cpp的DEFECT_SIZE_THRESHOLD一致）
            /// </summary>
            public int SignificantDefectThreshold { get; set; } = DEFAULT_DEFECT_SIZE_THRESHOLD;

            /// <summary>
            /// 是否有重大瑕疵（面积 >= SignificantDefectThreshold）
            /// 与demo_dll.cpp的判定逻辑一致：只看瑕疵面积，不看similarity
            /// </summary>
            public bool HasSignificantDefects
            {
                get
                {
                    if (Defects == null) return false;
                    return Defects.Any(d => d.Area >= SignificantDefectThreshold);
                }
            }

            /// <summary>
            /// 获取重大瑕疵列表（与demo_dll.cpp一致）
            /// </summary>
            public DefectInfo[] GetSignificantDefects()
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.Area >= SignificantDefectThreshold).ToArray();
            }

            /// <summary>
            /// 最终判定结果（与demo_dll.cpp一致）
            /// </summary>
            public bool FinalPassed => !HasSignificantDefects;

            public DefectInfo[] GetDefectsByROI(int roiId)
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.RoiId == roiId).ToArray();
            }

            public DefectInfo[] GetDefectsByType(DefectType type)
            {
                if (Defects == null) return Array.Empty<DefectInfo>();
                return Defects.Where(d => d.Type == type).ToArray();
            }

            public override string ToString()
            {
                switch (ResultCode)
                {
                    case -1:
                        return "检测执行失败";
                    case 0:
                        return string.Format("检测通过, 无瑕疵, 总耗时:{0:F1}ms", ProcessingTime);
                    case 1:
                        var sigDefects = GetSignificantDefects();
                        return string.Format("检测未通过, 发现{0}个瑕疵({1}个重大), 总耗时:{2:F1}ms",
                            ActualDefectCount, sigDefects.Length, ProcessingTime);
                    default:
                        return string.Format("未知状态({0})", ResultCode);
                }
            }
        }

        /// <summary>
        /// 学习结果
        /// </summary>
        public class LearningResult
        {
            public bool Success { get; set; }
            public float QualityScore { get; set; }
            public string Message { get; set; }

            public override string ToString()
            {
                return Success ? $"学习成功, 质量评分:{QualityScore:F3}, {Message}" : $"学习失败: {Message}";
            }
        }

        #endregion
    }

    #region 高级辅助类（与demo_dll.cpp使用方式一致）

    /// <summary>
    /// ROI参数 - 与demo_dll.cpp的ROIParams对应
    /// </summary>
    public class ROIParams
    {
        public int X { get; set; }
        public int Y { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
        public float Threshold { get; set; }

        public ROIParams() { }

        public ROIParams(int x, int y, int width, int height, float threshold)
        {
            X = x;
            Y = y;
            Width = width;
            Height = height;
            Threshold = threshold;
        }

        public Rectangle ToRectangle()
        {
            return new Rectangle(X, Y, Width, Height);
        }

        public override string ToString()
        {
            return $"({X},{Y}) {Width}x{Height} threshold={Threshold:F2}";
        }
    }

    /// <summary>
    /// 检测配置 - 与demo_dll.cpp的ProgramConfig对应
    /// </summary>
    public class DetectionConfig
    {
        // 基本配置
        public string TemplateFile { get; set; }
        public ROIParams ROI { get; set; } = new ROIParams();
        public string DataDir { get; set; }
        public string OutputDir { get; set; } = "output";

        // 对齐配置
        public bool AutoAlignEnabled { get; set; } = true;
        public DefectDetectorAPI.AlignmentMode AlignMode { get; set; } = DefectDetectorAPI.AlignmentMode.RoiOnly;
        public bool UseCsvAlignment { get; set; } = true;

        // ROI内缩配置
        public int ROIShrinkPixels { get; set; } = 5;

        // 二值图像优化配置（与demo_dll.cpp一致）
        public bool BinaryOptimizationEnabled { get; set; } = true;
        public int BinaryThreshold { get; set; } = DefectDetectorAPI.DEFAULT_BINARY_THRESHOLD;
        public int NoiseFilterSize { get; set; } = 3;
        public int EdgeTolerancePixels { get; set; } = 2;
        public bool EdgeFilterEnabled { get; set; } = true;
        public float EdgeDiffIgnoreRatio { get; set; } = 0.05f;
        public int MinSignificantArea { get; set; } = DefectDetectorAPI.DEFAULT_MIN_SIGNIFICANT_AREA;
        public float AreaDiffThreshold { get; set; } = 0.01f;
        public float OverallSimilarityThreshold { get; set; } = DefectDetectorAPI.DEFAULT_OVERALL_SIMILARITY_THRESHOLD;
        public int EdgeDefectSizeThreshold { get; set; } = DefectDetectorAPI.DEFAULT_EDGE_DEFECT_SIZE_THRESHOLD;
        public float EdgeDistanceMultiplier { get; set; } = DefectDetectorAPI.DEFAULT_EDGE_DISTANCE_MULTIPLIER;

        // 通用检测参数
        public int MinDefectSize { get; set; } = DefectDetectorAPI.DEFAULT_DEFECT_SIZE_THRESHOLD;
        public int BlurKernelSize { get; set; } = 3;
        public bool DetectBlackOnWhite { get; set; } = true;
        public bool DetectWhiteOnBlack { get; set; } = true;

        // 瑕疵判定阈值
        public int DefectSizeThreshold { get; set; } = DefectDetectorAPI.DEFAULT_DEFECT_SIZE_THRESHOLD;

        public override string ToString()
        {
            return $"Template={TemplateFile}, ROI={ROI}, BinaryOpt={BinaryOptimizationEnabled}, BinaryThreshold={BinaryThreshold}";
        }
    }

    /// <summary>
    /// 定位数据结构 - 与demo_dll.cpp的LocationData对应
    /// </summary>
    public class LocationData
    {
        public string Filename { get; set; }
        public float X { get; set; }
        public float Y { get; set; }
        public float Rotation { get; set; }
        public bool IsValid { get; set; }

        public override string ToString()
        {
            if (!IsValid) return "无效数据";
            return $"({X:F2}, {Y:F2}) 旋转={Rotation:F3}°";
        }
    }

    /// <summary>
    /// 单张图像检测结果 - 与demo_dll.cpp的处理结果对应
    /// </summary>
    public class ImageDetectionResult
    {
        public string Filename { get; set; }
        public bool Passed { get; set; }
        public int PassedROIs { get; set; }
        public int TotalROIs { get; set; }
        public float MinSimilarity { get; set; }
        public double ProcessingTime { get; set; }
        public float OffsetX { get; set; }
        public float OffsetY { get; set; }
        public float RotationDiff { get; set; }
        public DefectDetectorAPI.DefectInfo[] Defects { get; set; }
        public DefectDetectorAPI.DefectInfo[] SignificantDefects { get; set; }
        public string ErrorMessage { get; set; }
        public bool HasError => !string.IsNullOrEmpty(ErrorMessage);
    }

    /// <summary>
    /// 批次检测统计 - 与demo_dll.cpp的统计报告对应
    /// </summary>
    public class BatchStatistics
    {
        public int TotalImages { get; set; }
        public int PassCount { get; set; }
        public int FailCount { get; set; }
        public double TotalTime { get; set; }
        public double AverageTime => TotalImages > 0 ? TotalTime / TotalImages : 0;
        public double PassRate => TotalImages > 0 ? 100.0 * PassCount / TotalImages : 0;

        // 定位数据统计
        public int ValidLocationCount { get; set; }
        public float MinOffset { get; set; }
        public float MaxOffset { get; set; }
        public float AverageOffset { get; set; }
        public float MinRotationDiff { get; set; }
        public float MaxRotationDiff { get; set; }
        public float AverageRotationDiff { get; set; }

        public override string ToString()
        {
            return $"总计:{TotalImages}, 通过:{PassCount}, 失败:{FailCount}, 通过率:{PassRate:F1}%, 平均耗时:{AverageTime:F2}ms";
        }
    }

    /// <summary>
    /// 瑕疵检测辅助类 - 提供与demo_dll.cpp一致的高级API
    /// </summary>
    public class DefectDetectionHelper : IDisposable
    {
        private readonly DefectDetectorAPI _api;
        private bool _isDisposed;

        public DefectDetectorAPI API => _api;

        public DefectDetectionHelper()
        {
            _api = new DefectDetectorAPI();
        }

        /// <summary>
        /// 初始化检测器（与demo_dll.cpp的configureDetector + setupSingleROI + configureAlignment一致）
        /// </summary>
        public void Initialize(DetectionConfig config)
        {
            if (config == null)
                throw new ArgumentNullException(nameof(config));
            if (string.IsNullOrEmpty(config.TemplateFile))
                throw new ArgumentException("模板文件路径不能为空", nameof(config));
            if (!File.Exists(config.TemplateFile))
                throw new FileNotFoundException("模板文件不存在", config.TemplateFile);

            // Step 1: 导入模板
            if (!_api.ImportTemplateFromFile(config.TemplateFile))
                throw new InvalidOperationException("无法导入模板: " + config.TemplateFile);

            // 提取ROI模板（关键步骤！）
            _api.ExtractROITemplates();

            // Step 2: 配置检测参数（与demo_dll.cpp的configureDetector一致）
            ConfigureDetector(config);

            // Step 3: 设置ROI区域（与demo_dll.cpp的setupSingleROI一致）
            SetupSingleROI(config);

            // Step 4: 配置对齐（与demo_dll.cpp的configureAlignment一致）
            ConfigureAlignment(config);
        }

        /// <summary>
        /// 配置检测参数 - 与demo_dll.cpp的configureDetector函数一致
        /// </summary>
        private void ConfigureDetector(DetectionConfig config)
        {
            // 设置匹配方法为二值模式
            _api.SetMatchMethod(DefectDetectorAPI.MatchMethod.Binary);
            _api.SetBinaryThreshold(config.BinaryThreshold);

            // 配置二值检测参数（与demo_dll.cpp完全一致）
            var binaryParams = DefectDetectorAPI.GetDefaultBinaryDetectionParams();
            binaryParams.Enabled = config.BinaryOptimizationEnabled ? 1 : 0;
            binaryParams.AutoDetectBinary = 1;  // demo_cpp中显式设置为true
            binaryParams.NoiseFilterSize = config.NoiseFilterSize;
            binaryParams.EdgeTolerancePixels = config.EdgeFilterEnabled ? config.EdgeTolerancePixels : 0;
            binaryParams.EdgeDiffIgnoreRatio = config.EdgeDiffIgnoreRatio;
            binaryParams.MinSignificantArea = config.MinSignificantArea;
            binaryParams.AreaDiffThreshold = config.AreaDiffThreshold;
            binaryParams.OverallSimilarityThreshold = config.OverallSimilarityThreshold;
            binaryParams.EdgeDefectSizeThreshold = config.EdgeDefectSizeThreshold;
            binaryParams.EdgeDistanceMultiplier = config.EdgeDistanceMultiplier;
            binaryParams.BinaryThreshold = config.BinaryThreshold;

            _api.SetBinaryDetectionParams(binaryParams);

            // 设置其他参数
            _api.SetParameter("min_defect_size", config.MinDefectSize);
            _api.SetParameter("blur_kernel_size", config.BlurKernelSize);
            _api.SetParameter("detect_black_on_white", config.DetectBlackOnWhite ? 1.0f : 0.0f);
            _api.SetParameter("detect_white_on_black", config.DetectWhiteOnBlack ? 1.0f : 0.0f);
        }

        /// <summary>
        /// 设置单个ROI - 与demo_dll.cpp的setupSingleROI函数一致
        /// </summary>
        private void SetupSingleROI(DetectionConfig config)
        {
            // ROI内缩（与demo_dll.cpp完全一致）
            int shrinkPixels = config.ROIShrinkPixels;
            int x = config.ROI.X + shrinkPixels;
            int y = config.ROI.Y + shrinkPixels;
            int width = config.ROI.Width - 2 * shrinkPixels;
            int height = config.ROI.Height - 2 * shrinkPixels;

            // 确保ROI尺寸有效
            if (width <= 0 || height <= 0)
            {
                x = config.ROI.X;
                y = config.ROI.Y;
                width = config.ROI.Width;
                height = config.ROI.Height;
                shrinkPixels = 0;
            }

            // 创建DetectionParams并设置参数（与demo_dll.cpp完全一致）
            var params_ = new DefectDetectorAPI.DetectionParams
            {
                BinaryThreshold = config.BinaryThreshold,
                BlurKernelSize = config.BlurKernelSize,
                MinDefectSize = config.MinDefectSize,
                DetectBlackOnWhite = config.DetectBlackOnWhite,
                DetectWhiteOnBlack = config.DetectWhiteOnBlack,
                SimilarityThreshold = config.ROI.Threshold,
                MorphologyKernelSize = 3
            };

            // 使用AddROIWithParams添加ROI
            _api.AddROIWithParams(x, y, width, height, config.ROI.Threshold, params_);

            // 提取ROI模板图像（关键！否则模板图像为空，无法检测）
            _api.ExtractROITemplates();
        }

        /// <summary>
        /// 配置对齐 - 与demo_dll.cpp的configureAlignment函数一致
        /// </summary>
        private void ConfigureAlignment(DetectionConfig config)
        {
            _api.EnableAutoLocalization(config.AutoAlignEnabled);
            _api.SetAlignmentMode(config.AlignMode);
        }

        /// <summary>
        /// 检测单张图像 - 与demo_dll.cpp的processImages中的处理逻辑一致
        /// </summary>
        public ImageDetectionResult DetectImage(string imagePath, LocationData locationData = null, DetectionConfig config = null)
        {
            if (string.IsNullOrEmpty(imagePath))
                throw new ArgumentException("图像路径不能为空", nameof(imagePath));
            if (!File.Exists(imagePath))
                throw new FileNotFoundException("图像文件不存在", imagePath);

            var result = new ImageDetectionResult { Filename = Path.GetFileName(imagePath) };

            try
            {
                float offsetX = 0, offsetY = 0, rotationDiff = 0;

                // 设置外部变换（如果提供了定位数据）
                if (locationData != null && locationData.IsValid)
                {
                    offsetX = locationData.X;
                    offsetY = locationData.Y;
                    rotationDiff = locationData.Rotation;
                    _api.SetExternalTransform(offsetX, offsetY, rotationDiff);
                }
                else
                {
                    _api.ClearExternalTransform();
                }

                // 执行检测（使用DetectDefectsFromFileEx获取完整信息和瑕疵详情）
                var detectionResult = _api.DetectFromFileEx(imagePath);

                // 瑕疵检测判定（与demo_dll.cpp完全一致：面积>=DefectSizeThreshold视为重大缺陷）
                var significantDefectThreshold = config?.DefectSizeThreshold ?? DefectDetectorAPI.DEFAULT_DEFECT_SIZE_THRESHOLD;
                var significantDefects = detectionResult.Defects?.Where(d => d.Area >= significantDefectThreshold).ToArray() ?? new DefectDetectorAPI.DefectInfo[0];
                bool finalPass = significantDefects.Length == 0;

                // 获取ROI结果
                var roiResults = _api.GetLastROIResults();
                int passedROIs = roiResults.Count(r => r.Passed == 1);
                float minSimilarity = roiResults.Length > 0 ? roiResults.Min(r => r.Similarity) : 1.0f;

                result.Passed = finalPass;
                result.PassedROIs = passedROIs;
                result.TotalROIs = roiResults.Length;
                result.MinSimilarity = minSimilarity;
                result.ProcessingTime = detectionResult.ProcessingTime;
                result.OffsetX = offsetX;
                result.OffsetY = offsetY;
                result.RotationDiff = rotationDiff;
                result.Defects = detectionResult.Defects;
                result.SignificantDefects = significantDefects;

                // 清除外部变换
                _api.ClearExternalTransform();

                return result;
            }
            catch (Exception ex)
            {
                result.Passed = false;
                result.ErrorMessage = ex.Message;
                _api.ClearExternalTransform();
                return result;
            }
        }

        /// <summary>
        /// 批量检测图像
        /// </summary>
        public List<ImageDetectionResult> DetectImages(List<string> imagePaths, Dictionary<string, LocationData> locationDict = null, DetectionConfig config = null)
        {
            var results = new List<ImageDetectionResult>();

            foreach (var path in imagePaths)
            {
                string filename = Path.GetFileName(path);
                LocationData locData = null;
                locationDict?.TryGetValue(filename, out locData);

                var result = DetectImage(path, locData, config);
                results.Add(result);
            }

            return results;
        }

        /// <summary>
        /// 计算批次统计信息 - 与demo_dll.cpp的printSummaryReport一致
        /// </summary>
        public BatchStatistics CalculateStatistics(List<ImageDetectionResult> results)
        {
            if (results == null || results.Count == 0)
                return new BatchStatistics();

            var stats = new BatchStatistics
            {
                TotalImages = results.Count,
                PassCount = results.Count(r => r.Passed && !r.HasError),
                FailCount = results.Count(r => !r.Passed || r.HasError),
                TotalTime = results.Sum(r => r.ProcessingTime),
                ValidLocationCount = results.Count(r => Math.Abs(r.OffsetX) > 0.001f || Math.Abs(r.OffsetY) > 0.001f || Math.Abs(r.RotationDiff) > 0.001f)
            };

            // 计算偏移统计
            var offsets = results
                .Where(r => Math.Abs(r.OffsetX) > 0.001f || Math.Abs(r.OffsetY) > 0.001f)
                .Select(r => (float)Math.Sqrt(r.OffsetX * r.OffsetX + r.OffsetY * r.OffsetY))
                .ToList();

            if (offsets.Count > 0)
            {
                stats.MinOffset = offsets.Min();
                stats.MaxOffset = offsets.Max();
                stats.AverageOffset = offsets.Average();
            }

            // 计算旋转差统计
            var rotDiffs = results
                .Where(r => Math.Abs(r.RotationDiff) > 0.001f)
                .Select(r => Math.Abs(r.RotationDiff))
                .ToList();

            if (rotDiffs.Count > 0)
            {
                stats.MinRotationDiff = rotDiffs.Min();
                stats.MaxRotationDiff = rotDiffs.Max();
                stats.AverageRotationDiff = rotDiffs.Average();
            }

            return stats;
        }

        /// <summary>
        /// 从CSV文件读取定位数据 - 与demo_dll.cpp的loadLocationData对应
        /// </summary>
        public static Dictionary<string, LocationData> LoadLocationDataFromCsv(string dataDir, string csvFilename = "匹配信息_116_20260304161622333.csv")
        {
            var dict = new Dictionary<string, LocationData>(StringComparer.OrdinalIgnoreCase);
            string csvPath = Path.Combine(dataDir, csvFilename);

            if (!File.Exists(csvPath))
            {
                Console.WriteLine($"[警告] CSV文件不存在: {csvPath}");
                return dict;
            }

            try
            {
                var lines = File.ReadAllLines(csvPath, Encoding.UTF8);
                if (lines.Length < 2) return dict;

                // 跳过标题行
                for (int i = 1; i < lines.Length; i++)
                {
                    var line = lines[i].Trim();
                    if (string.IsNullOrEmpty(line)) continue;

                    var parts = line.Split(',');
                    if (parts.Length >= 4)
                    {
                        var data = new LocationData
                        {
                            Filename = parts[0].Trim(),
                            IsValid = !string.IsNullOrWhiteSpace(parts[1]) &&
                                     !string.IsNullOrWhiteSpace(parts[2]) &&
                                     !string.IsNullOrWhiteSpace(parts[3])
                        };

                        if (data.IsValid)
                        {
                            if (float.TryParse(parts[1].Trim(), out float x))
                                data.X = x;
                            if (float.TryParse(parts[2].Trim(), out float y))
                                data.Y = y;
                            if (float.TryParse(parts[3].Trim(), out float r))
                                data.Rotation = r;
                        }

                        dict[data.Filename] = data;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[警告] 读取CSV文件失败: {ex.Message}");
            }

            return dict;
        }

        /// <summary>
        /// 获取支持的图像文件列表
        /// </summary>
        public static List<string> GetImageFiles(string dataDir, string excludeFilename = null)
        {
            var extensions = new[] { ".bmp", ".jpg", ".jpeg", ".png" };
            var files = new List<string>();

            if (!Directory.Exists(dataDir))
                return files;

            foreach (var ext in extensions)
            {
                files.AddRange(Directory.GetFiles(dataDir, "*" + ext, SearchOption.TopDirectoryOnly));
            }

            // 排除模板文件
            if (!string.IsNullOrEmpty(excludeFilename))
            {
                string excludeLower = excludeFilename.ToLowerInvariant();
                files = files.Where(f => !Path.GetFileName(f).Equals(excludeLower, StringComparison.OrdinalIgnoreCase)).ToList();
            }

            return files.OrderBy(f => f).ToList();
        }

        public void Dispose()
        {
            if (!_isDisposed)
            {
                _api?.Dispose();
                _isDisposed = true;
            }
        }
    }

    #endregion
}