/*
 * ============================================================================
 * 文件名: Program.cs
 * 描述:   喷印瑕疵检测系统 C# 演示程序
 * 版本:   2.0
 *
 * 功能说明:
 *   本程序演示如何使用 DefectDetectorAPI 进行瑕疵检测，包括：
 *   - 命令行参数解析
 *   - 模板导入和配置
 *   - ROI区域设置
 *   - 批量图像检测
 *   - 详细结果输出
 *
 * 使用方法:
 *   DefectDetection.exe [选项] [数据目录]
 *
 *   选项:
 *     --help              显示帮助信息
 *     --template <file>   指定模板文件路径
 *     --output <dir>      指定输出目录
 *     --roi-count <n>     设置ROI数量 (默认: 4)
 *     --threshold <f>     设置相似度阈值 (默认: 0.50)
 *     --no-align          禁用自动对齐
 *     --align-full        使用整图对齐模式
 *     --align-roi         使用ROI区域对齐模式 (默认)
 *     --verbose           显示详细信息
 *     --max-images <n>    最大处理图像数量 (默认: 20)
 *
 * 示例:
 *   DefectDetection.exe data/demo
 *   DefectDetection.exe --template template.bmp --verbose data/test
 *   DefectDetection.exe --roi-count 6 --threshold 0.6 data/demo
 * ============================================================================
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Drawing;
using System.Diagnostics;

namespace DefectDetection
{
    /// <summary>
    /// 喷印瑕疵检测系统演示程序。
    /// 参考 C++ demo.cpp 实现，提供完整的命令行界面和批量处理功能。
    /// </summary>
    public class Program
    {
        #region 配置常量

        // ========== 默认配置 ==========
        /// <summary>默认数据目录</summary>
        private const string DEFAULT_DATA_DIR = "data/demo";

        /// <summary>默认输出目录</summary>
        private const string DEFAULT_OUTPUT_DIR = "output";

        /// <summary>默认ROI数量</summary>
        private const int DEFAULT_ROI_COUNT = 4;

        /// <summary>默认相似度阈值</summary>
        private const float DEFAULT_THRESHOLD = 0.50f;

        /// <summary>默认最大处理图像数</summary>
        private const int DEFAULT_MAX_IMAGES = 20;

        /// <summary>二值化阈值</summary>
        private const int BINARY_THRESHOLD = 128;

        // ========== 二值图像优化参数 ==========
        /// <summary>噪声过滤尺寸（像素）</summary>
        private const int NOISE_FILTER_SIZE = 10;

        /// <summary>边缘容差（像素）</summary>
        private const int EDGE_TOLERANCE_PIXELS = 4;

        /// <summary>边缘差异忽略比例</summary>
        private const float EDGE_DIFF_IGNORE_RATIO = 0.05f;

        /// <summary>最小显著区域面积</summary>
        private const int MIN_SIGNIFICANT_AREA = 20;

        /// <summary>面积差异阈值</summary>
        private const float AREA_DIFF_THRESHOLD = 0.001f;

        /// <summary>整体相似度阈值</summary>
        private const float OVERALL_SIMILARITY_THRESHOLD = 0.95f;

        #endregion

        #region 程序配置结构

        /// <summary>
        /// 程序运行配置
        /// </summary>
        private class ProgramConfig
        {
            // 路径配置
            public string DataDir { get; set; } = DEFAULT_DATA_DIR;
            public string OutputDir { get; set; } = DEFAULT_OUTPUT_DIR;
            public string TemplateFile { get; set; } = null;

            // 检测参数
            public int RoiCount { get; set; } = DEFAULT_ROI_COUNT;
            public float Threshold { get; set; } = DEFAULT_THRESHOLD;
            public int MaxImages { get; set; } = DEFAULT_MAX_IMAGES;

            // 对齐配置
            public bool AutoAlignEnabled { get; set; } = true;
            public DefectDetectorAPI.AlignmentMode AlignMode { get; set; } = DefectDetectorAPI.AlignmentMode.RoiOnly;

            // 二值优化
            public bool BinaryOptimizationEnabled { get; set; } = true;

            // 输出控制
            public bool Verbose { get; set; } = false;
            public bool SaveResults { get; set; } = false;
        }

        #endregion

        #region 主程序入口

        /// <summary>
        /// 程序主入口
        /// </summary>
        public static void Main(string[] args)
        {
            try
            {
                // 解析命令行参数
                var config = ParseArguments(args);
                if (config == null)
                    return; // 显示帮助后退出

                // 运行检测演示
                RunDemo(config);
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"\n[错误] {ex.Message}");
                Console.ResetColor();

                if (ex.InnerException != null)
                {
                    Console.WriteLine($"  详情: {ex.InnerException.Message}");
                }

                Environment.ExitCode = 1;
            }

            Console.WriteLine("\n按任意键退出...");
            Console.ReadKey();
        }

        #endregion


        #region 命令行解析

        /// <summary>
        /// 显示使用帮助
        /// </summary>
        private static void PrintUsage()
        {
            Console.WriteLine(@"
用法: DefectDetection.exe [选项] [数据目录]

默认数据目录: data/demo

选项:
  --help              显示此帮助信息
  --template <file>   指定模板文件路径
  --output <dir>      指定输出目录
  --roi-count <n>     设置ROI数量 (1-20, 默认: 4)
  --threshold <f>     设置相似度阈值 (0.0-1.0, 默认: 0.50)
  --max-images <n>    最大处理图像数量 (默认: 20)

对齐选项:
  --no-align          禁用自动对齐 (不推荐)
  --align-full        使用整图对齐模式 (较慢)
  --align-roi         使用ROI区域对齐模式 (较快, 默认)

二值优化:
  --no-binary-opt     禁用二值图像优化

输出控制:
  --verbose, -v       显示详细信息
  --save              保存检测结果图像

示例:
  DefectDetection.exe data/demo
  DefectDetection.exe --template template.bmp --verbose data/test
  DefectDetection.exe --roi-count 6 --threshold 0.6 --save data/demo
");
        }

        /// <summary>
        /// 解析命令行参数
        /// </summary>
        private static ProgramConfig ParseArguments(string[] args)
        {
            var config = new ProgramConfig();

            for (int i = 0; i < args.Length; i++)
            {
                string arg = args[i];

                switch (arg.ToLower())
                {
                    case "--help":
                    case "-h":
                    case "/?":
                        PrintUsage();
                        return null;

                    case "--template":
                        if (i + 1 < args.Length)
                            config.TemplateFile = args[++i];
                        break;

                    case "--output":
                        if (i + 1 < args.Length)
                            config.OutputDir = args[++i];
                        break;

                    case "--roi-count":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int roiCount))
                            config.RoiCount = Math.Max(1, Math.Min(20, roiCount));
                        break;

                    case "--threshold":
                        if (i + 1 < args.Length && float.TryParse(args[++i], out float threshold))
                            config.Threshold = Math.Max(0.0f, Math.Min(1.0f, threshold));
                        break;

                    case "--max-images":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int maxImages))
                            config.MaxImages = Math.Max(1, maxImages);
                        break;

                    case "--no-align":
                        config.AutoAlignEnabled = false;
                        config.AlignMode = DefectDetectorAPI.AlignmentMode.None;
                        break;

                    case "--align-full":
                        config.AlignMode = DefectDetectorAPI.AlignmentMode.FullImage;
                        break;

                    case "--align-roi":
                        config.AlignMode = DefectDetectorAPI.AlignmentMode.RoiOnly;
                        break;

                    case "--no-binary-opt":
                        config.BinaryOptimizationEnabled = false;
                        break;

                    case "--verbose":
                    case "-v":
                        config.Verbose = true;
                        break;

                    case "--save":
                        config.SaveResults = true;
                        break;

                    default:
                        // 非选项参数视为数据目录
                        if (!arg.StartsWith("-"))
                            config.DataDir = arg;
                        break;
                }
            }

            return config;
        }

        #endregion

        #region 核心检测流程

        /// <summary>
        /// 运行检测演示
        /// </summary>
        private static void RunDemo(ProgramConfig config)
        {
            // ========== 打印程序标题 ==========
            PrintHeader();

            // ========== 验证数据目录 ==========
            if (!Directory.Exists(config.DataDir))
            {
                throw new DirectoryNotFoundException($"数据目录不存在: {config.DataDir}");
            }

            // ========== 获取图像文件列表 ==========
            var (templateFile, imageFiles) = GetImageFiles(config);

            Console.WriteLine($"模板文件: {templateFile}");
            Console.WriteLine($"测试图像: {imageFiles.Count} 张");
            Console.WriteLine();

            // ========== 创建输出目录 ==========
            if (config.SaveResults && !Directory.Exists(config.OutputDir))
            {
                Directory.CreateDirectory(config.OutputDir);
                Console.WriteLine($"创建输出目录: {config.OutputDir}");
            }

            // ========== 使用检测器 ==========
            using (var detector = new DefectDetectorAPI())
            {
                // Step 1: 导入模板
                Console.WriteLine("Step 1: 导入模板图像...");
                if (!detector.ImportTemplateFromFile(templateFile))
                {
                    throw new InvalidOperationException($"无法导入模板: {templateFile}");
                }

                var templateSize = detector.GetTemplateSize();
                Console.WriteLine($"  模板尺寸: {templateSize.Width}x{templateSize.Height} 像素");
                Console.WriteLine();

                // Step 2: 配置检测参数
                ConfigureDetector(detector, config);

                // Step 3: 设置ROI区域
                SetupROIs(detector, config, templateSize);

                // Step 4: 配置对齐模式
                ConfigureAlignment(detector, config);

                // Step 5: 执行批量检测
                ProcessImages(detector, imageFiles, config);
            }
        }

        #endregion


        #region 辅助方法

        /// <summary>
        /// 打印程序标题
        /// </summary>
        private static void PrintHeader()
        {
            Console.WriteLine("========================================");
            Console.WriteLine("   喷印瑕疵检测系统 - C# 演示程序");
            Console.WriteLine("   Print Defect Detection System Demo");
            Console.WriteLine("========================================");
            Console.WriteLine();
        }

        /// <summary>
        /// 获取图像文件列表
        /// </summary>
        private static (string templateFile, List<string> imageFiles) GetImageFiles(ProgramConfig config)
        {
            // 获取所有支持的图像文件
            var supportedExtensions = new[] { ".bmp", ".jpg", ".jpeg", ".png" };
            var allFiles = Directory.GetFiles(config.DataDir)
                .Where(f => supportedExtensions.Contains(Path.GetExtension(f).ToLower()))
                .OrderBy(f => f)
                .ToList();

            if (allFiles.Count == 0)
            {
                throw new FileNotFoundException($"在目录 {config.DataDir} 中未找到图像文件");
            }

            string templateFile;
            List<string> imageFiles;

            // 确定模板文件
            if (!string.IsNullOrEmpty(config.TemplateFile))
            {
                // 使用指定的模板文件
                templateFile = config.TemplateFile;
                if (!File.Exists(templateFile))
                {
                    throw new FileNotFoundException($"模板文件不存在: {templateFile}");
                }
                imageFiles = allFiles.Where(f => f != templateFile).ToList();
            }
            else
            {
                // 查找 template.bmp 或使用第一张图片
                var defaultTemplate = Path.Combine(config.DataDir, "template.bmp");
                if (File.Exists(defaultTemplate))
                {
                    templateFile = defaultTemplate;
                    imageFiles = allFiles.Where(f =>
                        Path.GetFileName(f).ToLower() != "template.bmp").ToList();
                }
                else
                {
                    // 使用第一张图片作为模板
                    templateFile = allFiles[0];
                    imageFiles = allFiles.Skip(1).ToList();
                }
            }

            if (imageFiles.Count == 0)
            {
                throw new InvalidOperationException("需要至少2张图像（1张模板 + 1张测试图）");
            }

            return (templateFile, imageFiles);
        }

        /// <summary>
        /// 配置检测器参数
        /// </summary>
        private static void ConfigureDetector(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 2: 配置检测参数...");

            // 设置匹配方法为二值模式（适用于黑白图像）
            detector.SetMatchMethod(DefectDetectorAPI.MatchMethod.Binary);
            detector.SetBinaryThreshold(BINARY_THRESHOLD);
            Console.WriteLine($"  匹配方法: BINARY (二值图像)");
            Console.WriteLine($"  二值化阈值: {BINARY_THRESHOLD}");

            // 配置二值图像优化参数
            if (config.BinaryOptimizationEnabled)
            {
                Console.WriteLine();
                Console.WriteLine("  *** 二值图像优化模式: 已启用 ***");

                var binaryParams = new DefectDetectorAPI.BinaryDetectionParams
                {
                    Enabled = 1,
                    AutoDetectBinary = 1,
                    NoiseFilterSize = NOISE_FILTER_SIZE,
                    EdgeTolerancePixels = EDGE_TOLERANCE_PIXELS,
                    EdgeDiffIgnoreRatio = EDGE_DIFF_IGNORE_RATIO,
                    MinSignificantArea = MIN_SIGNIFICANT_AREA,
                    AreaDiffThreshold = AREA_DIFF_THRESHOLD,
                    OverallSimilarityThreshold = OVERALL_SIMILARITY_THRESHOLD
                };
                detector.SetBinaryDetectionParams(binaryParams);

                Console.WriteLine($"    - noise_filter_size = {NOISE_FILTER_SIZE} px");
                Console.WriteLine($"    - edge_tolerance_pixels = {EDGE_TOLERANCE_PIXELS} px");
                Console.WriteLine($"    - edge_diff_ignore_ratio = {EDGE_DIFF_IGNORE_RATIO * 100:F1}%");
                Console.WriteLine($"    - min_significant_area = {MIN_SIGNIFICANT_AREA} px");
                Console.WriteLine($"    - overall_similarity_threshold = {OVERALL_SIMILARITY_THRESHOLD * 100:F0}%");
            }
            else
            {
                Console.WriteLine();
                Console.WriteLine("  二值图像优化: 已禁用");
                var binaryParams = new DefectDetectorAPI.BinaryDetectionParams { Enabled = 0 };
                detector.SetBinaryDetectionParams(binaryParams);
            }

            // 其他检测参数
            detector.SetParameter("min_defect_size", 1000);  // 禁用瑕疵检测（仅使用相似度判断）
            detector.SetParameter("blur_kernel_size", 3);    // 轻微平滑
            detector.SetParameter("detect_black_on_white", 0.0f);
            detector.SetParameter("detect_white_on_black", 0.0f);

            Console.WriteLine();
            Console.WriteLine($"  min_defect_size = 1000 (瑕疵检测已禁用)");
            Console.WriteLine($"  blur_kernel_size = 3");
            Console.WriteLine();
        }

        /// <summary>
        /// 设置ROI区域（随机生成）
        /// </summary>
        private static void SetupROIs(DefectDetectorAPI detector, ProgramConfig config, Size templateSize)
        {
            Console.WriteLine($"Step 3: 设置ROI区域 (随机生成 {config.RoiCount} 个)...");

            // 使用时间种子的随机数生成器
            var random = new Random();

            // ROI尺寸基于模板尺寸
            int roiWidth = templateSize.Width / 25;
            int roiHeight = templateSize.Height / 25;

            // 边距（20%）
            int marginX = templateSize.Width / 5;
            int marginY = templateSize.Height / 5;
            int maxX = templateSize.Width - roiWidth - marginX;
            int maxY = templateSize.Height - roiHeight - marginY;

            // ROI名称列表
            string[] roiNames = { "Logo", "Text", "Barcode", "Date", "Batch", "Symbol", "Border", "Extra" };

            for (int i = 0; i < config.RoiCount; i++)
            {
                int x = random.Next(marginX, maxX);
                int y = random.Next(marginY, maxY);
                string name = roiNames[i % roiNames.Length];

                int roiId = detector.AddROI(x, y, roiWidth, roiHeight, config.Threshold);
                Console.WriteLine($"  ROI {roiId}: {name} 区域 ({x},{y}) {roiWidth}x{roiHeight} 阈值={config.Threshold:F2}");
            }

            Console.WriteLine($"  总计: {detector.GetROICount()} 个ROI");
            Console.WriteLine();
        }

        /// <summary>
        /// 配置对齐模式
        /// </summary>
        private static void ConfigureAlignment(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 4: 配置自动对齐...");

            detector.EnableAutoLocalization(config.AutoAlignEnabled);
            detector.SetAlignmentMode(config.AlignMode);

            Console.WriteLine($"  自动定位: {(config.AutoAlignEnabled ? "已启用" : "已禁用")}");

            if (config.AutoAlignEnabled)
            {
                string modeStr = config.AlignMode switch
                {
                    DefectDetectorAPI.AlignmentMode.None => "NONE",
                    DefectDetectorAPI.AlignmentMode.FullImage => "FULL_IMAGE (整图变换，较慢)",
                    DefectDetectorAPI.AlignmentMode.RoiOnly => "ROI_ONLY (ROI区域变换，推荐)",
                    _ => "未知"
                };
                Console.WriteLine($"  对齐模式: {modeStr}");
                Console.WriteLine("  功能: 自动校正位置偏移、旋转和缩放差异");
            }
            else
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("  警告: 禁用对齐可能导致误报！");
                Console.ResetColor();
            }

            Console.WriteLine();
        }

        #endregion


        #region 批量处理

        /// <summary>
        /// 处理所有测试图像
        /// </summary>
        private static void ProcessImages(DefectDetectorAPI detector, List<string> imageFiles, ProgramConfig config)
        {
            Console.WriteLine("Step 5: 处理测试图像...");
            Console.WriteLine("----------------------------------------");

            // 限制处理数量
            int processCount = Math.Min(config.MaxImages, imageFiles.Count);

            // 统计变量
            int passCount = 0;
            int failCount = 0;
            double totalTime = 0;
            double totalLocTime = 0;
            double totalRoiTime = 0;

            // 对齐统计
            int alignSuccessCount = 0;
            float maxOffset = 0, minOffset = float.MaxValue, totalOffset = 0;
            float maxRotation = 0, minRotation = float.MaxValue, totalRotation = 0;
            float maxScale = 0, minScale = float.MaxValue, totalScale = 0;

            // 处理每张图像
            for (int i = 0; i < processCount; i++)
            {
                string testFile = imageFiles[i];
                string filename = Path.GetFileName(testFile);

                try
                {
                    // 执行检测
                    var result = detector.DetectFromFile(testFile);

                    // 累计时间
                    totalTime += result.ProcessingTime;
                    totalLocTime += result.LocalizationTime;
                    totalRoiTime += result.ROIComparisonTime;

                    // 获取ROI结果
                    var roiResults = detector.GetLastROIResults();
                    int passedRois = roiResults.Count(r => r.Passed);
                    float minSimilarity = roiResults.Length > 0 ? roiResults.Min(r => r.Similarity) : 0;
                    int zeroSimilarityCount = roiResults.Count(r => r.Similarity == 0);

                    // 判断通过/失败
                    bool passed = result.ResultCode == 0;
                    if (passed) passCount++; else failCount++;

                    // 输出结果行
                    PrintResultLine(i + 1, processCount, filename, passed, passedRois,
                        roiResults.Length, minSimilarity, zeroSimilarityCount, result.ProcessingTime);

                    // 详细模式：显示对齐信息和ROI相似度
                    if (config.Verbose)
                    {
                        PrintVerboseInfo(detector, config, roiResults);
                    }

                    // 收集对齐统计
                    if (config.AutoAlignEnabled)
                    {
                        try
                        {
                            var locInfo = detector.GetLastLocalizationInfo();
                            if (locInfo.Success)
                            {
                                alignSuccessCount++;
                                float offset = (float)Math.Sqrt(locInfo.OffsetX * locInfo.OffsetX +
                                    locInfo.OffsetY * locInfo.OffsetY);
                                maxOffset = Math.Max(maxOffset, offset);
                                minOffset = Math.Min(minOffset, offset);
                                totalOffset += offset;
                                maxRotation = Math.Max(maxRotation, Math.Abs(locInfo.RotationAngle));
                                minRotation = Math.Min(minRotation, Math.Abs(locInfo.RotationAngle));
                                totalRotation += Math.Abs(locInfo.RotationAngle);
                                maxScale = Math.Max(maxScale, locInfo.Scale);
                                minScale = Math.Min(minScale, locInfo.Scale);
                                totalScale += locInfo.Scale;
                            }
                        }
                        catch
                        {
                            // 忽略获取定位信息失败
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"[{i + 1,3}/{processCount}] {filename,-35} | 错误: {ex.Message}");
                    Console.ResetColor();
                    failCount++;
                }
            }

            // 打印统计报告
            PrintSummaryReport(config, processCount, passCount, failCount,
                totalTime, totalLocTime, totalRoiTime,
                alignSuccessCount, minOffset, maxOffset, totalOffset,
                minRotation, maxRotation, totalRotation,
                minScale, maxScale, totalScale);
        }

        /// <summary>
        /// 打印单个结果行
        /// </summary>
        private static void PrintResultLine(int index, int total, string filename, bool passed,
            int passedRois, int totalRois, float minSimilarity, int zeroCount, double processingTime)
        {
            // 设置颜色
            Console.ForegroundColor = passed ? ConsoleColor.Green : ConsoleColor.Red;
            string status = passed ? "PASS" : "FAIL";

            // 截断过长的文件名
            if (filename.Length > 35)
                filename = filename.Substring(0, 32) + "...";

            Console.Write($"[{index,3}/{total}] {filename,-35} | {status}");
            Console.ResetColor();

            Console.Write($" | {passedRois}/{totalRois} ROIs");
            Console.Write($" | min:{minSimilarity * 100,5:F1}%");

            if (zeroCount > 0)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.Write($" ({zeroCount} null)");
                Console.ResetColor();
            }

            Console.WriteLine($" | {processingTime,6:F1} ms");
        }

        /// <summary>
        /// 打印详细信息（verbose模式）
        /// </summary>
        private static void PrintVerboseInfo(DefectDetectorAPI detector, ProgramConfig config,
            DefectDetectorAPI.ROIResult[] roiResults)
        {
            // 显示对齐信息
            if (config.AutoAlignEnabled)
            {
                try
                {
                    var loc = detector.GetLastLocalizationInfo();
                    Console.Write("    └─ 对齐: ");
                    Console.ForegroundColor = loc.Success ? ConsoleColor.Green : ConsoleColor.Red;
                    Console.Write(loc.Success ? "OK" : "FAIL");
                    Console.ResetColor();
                    Console.WriteLine($" | 偏移=({loc.OffsetX:F2}, {loc.OffsetY:F2})px" +
                        $" | 旋转={loc.RotationAngle:F3}°" +
                        $" | 缩放={loc.Scale:F4}");
                }
                catch
                {
                    Console.WriteLine("    └─ 对齐: 无法获取信息");
                }
            }

            // 显示各ROI相似度
            Console.Write("    └─ ROIs: ");
            for (int r = 0; r < roiResults.Length; r++)
            {
                var roi = roiResults[r];
                Console.ForegroundColor = roi.Passed ? ConsoleColor.Green : ConsoleColor.Red;
                Console.Write(roi.Passed ? "✓" : "✗");
                Console.ResetColor();
                Console.Write($"{roi.Similarity * 100:F0}%");
                if (r < roiResults.Length - 1) Console.Write(" ");
            }
            Console.WriteLine();
        }

        #endregion


        #region 统计报告

        /// <summary>
        /// 打印统计汇总报告
        /// </summary>
        private static void PrintSummaryReport(ProgramConfig config, int processCount,
            int passCount, int failCount, double totalTime, double totalLocTime, double totalRoiTime,
            int alignSuccessCount, float minOffset, float maxOffset, float totalOffset,
            float minRotation, float maxRotation, float totalRotation,
            float minScale, float maxScale, float totalScale)
        {
            double avgTotal = totalTime / processCount;
            double avgLoc = totalLocTime / processCount;
            double avgRoi = totalRoiTime / processCount;

            Console.WriteLine();
            Console.WriteLine("========================================");
            Console.WriteLine("           统计报告 Summary");
            Console.WriteLine("========================================");

            // 配置信息
            Console.WriteLine($"匹配方法: BINARY");
            Console.WriteLine($"二值优化: {(config.BinaryOptimizationEnabled ? "已启用" : "已禁用")}");
            if (config.BinaryOptimizationEnabled)
            {
                Console.WriteLine($"  - edge_tolerance = {EDGE_TOLERANCE_PIXELS} px");
                Console.WriteLine($"  - noise_filter = {NOISE_FILTER_SIZE} px");
                Console.WriteLine($"  - similarity_threshold = {OVERALL_SIMILARITY_THRESHOLD * 100:F0}%");
            }

            string alignModeStr = config.AutoAlignEnabled
                ? (config.AlignMode == DefectDetectorAPI.AlignmentMode.FullImage ? "FULL_IMAGE" : "ROI_ONLY")
                : "已禁用";
            Console.WriteLine($"对齐模式: {alignModeStr}");

            Console.WriteLine("----------------------------------------");

            // 检测结果统计
            Console.WriteLine($"处理图像数: {processCount}");
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"通过: {passCount}");
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"失败: {failCount}");
            Console.ResetColor();

            double passRate = 100.0 * passCount / processCount;
            Console.ForegroundColor = passRate >= 80 ? ConsoleColor.Green :
                (passRate >= 50 ? ConsoleColor.Yellow : ConsoleColor.Red);
            Console.WriteLine($"通过率: {passRate:F1}%");
            Console.ResetColor();

            Console.WriteLine("----------------------------------------");

            // 性能统计
            Console.WriteLine($"平均总耗时: {avgTotal:F2} ms");
            if (config.AutoAlignEnabled)
            {
                Console.WriteLine($"  - 定位耗时: {avgLoc:F2} ms");
                Console.WriteLine($"  - ROI比对耗时: {avgRoi:F2} ms");
            }

            Console.WriteLine("----------------------------------------");

            // 对齐统计
            if (config.AutoAlignEnabled && alignSuccessCount > 0)
            {
                Console.WriteLine("对齐统计:");
                Console.WriteLine($"  成功率: {alignSuccessCount}/{processCount} ({100.0 * alignSuccessCount / processCount:F1}%)");
                Console.WriteLine($"  偏移 (px):  min={minOffset:F2}, max={maxOffset:F2}, avg={totalOffset / alignSuccessCount:F2}");
                Console.WriteLine($"  旋转 (°):   min={minRotation:F3}, max={maxRotation:F3}, avg={totalRotation / alignSuccessCount:F3}");
                Console.WriteLine($"  缩放:       min={minScale:F4}, max={maxScale:F4}, avg={totalScale / alignSuccessCount:F4}");
                Console.WriteLine("----------------------------------------");
            }
            else if (!config.AutoAlignEnabled)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("注意: 对齐已禁用。使用 --align-roi 或 --align-full 启用。");
                Console.WriteLine("      对比启用对齐后的结果以诊断准确性问题。");
                Console.ResetColor();
                Console.WriteLine("----------------------------------------");
            }

            // 性能评估
            bool perfPass = avgRoi <= 20.0;
            Console.Write("ROI比对性能: ");
            Console.ForegroundColor = perfPass ? ConsoleColor.Green : ConsoleColor.Red;
            Console.Write(perfPass ? "PASS" : "FAIL");
            Console.ResetColor();
            Console.WriteLine(" (目标: <= 20ms)");

            Console.WriteLine("========================================");
        }

        #endregion
    }
}