/*
 * ============================================================================
 * 文件名: Demo.cs
 * 描述:   喷印瑕疵检测系统 C# Demo - 仿照C++ demo.cpp实现
 * 版本:   1.0
 *
 * 功能说明:
 *   本程序完全仿照C++ demo.cpp的功能和参数实现，包括：
 *   - 完整的命令行参数解析（与demo.cpp保持一致）
 *   - 二值图像优化模式配置
 *   - ROI设置（随机生成/手动选择/从文件加载）
 *   - 瑕疵检测（基于面积阈值）
 *   - 详细的统计报告
 *
 * 使用方法:
 *   Demo.exe [选项] [数据目录]
 *
 * 选项:
 *   --help              显示帮助信息
 *   --output <dir>      设置输出目录 (默认: output)
 *   --roi-count <num>   设置ROI数量 (默认: 4)
 *   --seed <num>        设置随机种子 (默认: 时间种子)
 *   --no-align          禁用自动对齐
 *   --align-full        使用整图对齐模式
 *   --align-roi         使用ROI区域对齐模式 (默认)
 *   --verbose           显示详细信息
 *
 * 二值图像优化参数:
 *   --no-binary-opt     禁用二值图像优化
 *   --edge-tol <px>     边缘容错像素数 (默认: 4)
 *   --noise-size <px>   噪声过滤尺寸 (默认: 10)
 *   --min-area <px>     最小显著面积 (默认: 20)
 *   --sim-thresh <f>    总体相似度阈值 (默认: 0.90)
 *
 * ROI配置:
 *   --manual-roi        启用手动ROI选择模式
 *   --load-roi <file>   从JSON文件加载ROI配置
 *   --save-roi <file>   保存ROI配置到JSON文件
 *
 * 示例:
 *   Demo.exe data/demo
 *   Demo.exe --verbose --roi-count 6 data/demo
 *   Demo.exe --edge-tol 3 --noise-size 15 data/demo
 * ============================================================================
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace DefectDetection
{
    /// <summary>
    /// ROI配置结构 - 用于JSON序列化
    /// </summary>
    public class ROIConfig
    {
        [JsonPropertyName("roi_count")]
        public int ROICount { get; set; }

        [JsonPropertyName("rois")]
        public List<ROIItem> ROIs { get; set; } = new List<ROIItem>();
    }

    /// <summary>
    /// ROI项 - 用于JSON序列化
    /// </summary>
    public class ROIItem
    {
        [JsonPropertyName("x")]
        public int X { get; set; }

        [JsonPropertyName("y")]
        public int Y { get; set; }

        [JsonPropertyName("width")]
        public int Width { get; set; }

        [JsonPropertyName("height")]
        public int Height { get; set; }

        [JsonPropertyName("threshold")]
        public float Threshold { get; set; }
    }

    /// <summary>
    /// Demo程序 - 完全仿照C++ demo.cpp实现
    /// </summary>
    public class Demo
    {
        // ========== 默认配置常量 ==========
        private const string DEFAULT_DATA_DIR = "data/demo";
        private const string DEFAULT_OUTPUT_DIR = "output";
        private const int DEFAULT_ROI_COUNT = 4;
        private const int DEFAULT_BINARY_THRESHOLD = 128;
        private const int DEFAULT_MIN_DEFECT_SIZE = 100;

        // ========== 程序配置结构 ==========
        private class ProgramConfig
        {
            // 路径配置
            public string DataDir { get; set; } = DEFAULT_DATA_DIR;
            public string OutputDir { get; set; } = DEFAULT_OUTPUT_DIR;
            public string LoadROIFile { get; set; } = "";
            public string SaveROIFile { get; set; } = "";

            // 检测参数
            public int ROICount { get; set; } = DEFAULT_ROI_COUNT;
            public int RandomSeed { get; set; } = new Random().Next();
            public bool SeedSpecified { get; set; } = false;

            // 对齐配置
            public bool AutoAlignEnabled { get; set; } = true;
            public DefectDetectorAPI.AlignmentMode AlignMode { get; set; } = DefectDetectorAPI.AlignmentMode.RoiOnly;

            // 二值优化配置
            public bool BinaryOptimizationEnabled { get; set; } = true;
            public DefectDetectorAPI.BinaryDetectionParams BinaryParams { get; set; } = new DefectDetectorAPI.BinaryDetectionParams
            {
                Enabled = 1,
                AutoDetectBinary = 1,
                NoiseFilterSize = 10,
                EdgeTolerancePixels = 4,
                EdgeDiffIgnoreRatio = 0.05f,
                MinSignificantArea = 20,
                AreaDiffThreshold = 0.001f,
                OverallSimilarityThreshold = 0.90f
            };

            // ROI选择模式
            public bool ManualROIMode { get; set; } = false;

            // 输出控制
            public bool VerboseMode { get; set; } = false;
        }

        /// <summary>
        /// 程序入口
        /// </summary>
        public static void Main(string[] args)
        {
            try
            {
                // 解析命令行参数
                var config = ParseArguments(args);
                if (config == null)
                    return;

                // 运行检测演示
                RunDemo(config);
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"\n[错误] {ex.Message}");
                Console.ResetColor();
                Environment.ExitCode = 1;
            }

            Console.WriteLine("\n按任意键退出...");
            Console.ReadKey();
        }

        #region 命令行解析

        /// <summary>
        /// 显示使用帮助 - 与C++ demo保持一致
        /// </summary>
        private static void PrintUsage()
        {
            Console.WriteLine(@"
用法: Demo.exe [选项] [数据目录]

默认数据目录: data/demo

选项:
  --output <dir>      设置输出目录 (默认: output)
  --roi-count <num>   设置ROI数量 (1-20, 默认: 4)
  --seed <num>        设置随机种子 (默认: 时间种子)
  --verbose, -v       显示详细信息

对齐选项:
  --no-align          禁用自动对齐 (不推荐)
  --align-full        使用整图对齐模式 (较慢)
  --align-roi         使用ROI区域对齐模式 (较快, 默认)

二值图像优化参数:
  --no-binary-opt     禁用二值图像优化
  --edge-tol <px>     边缘容错像素数 (默认: 4)
  --noise-size <px>   噪声过滤尺寸 (默认: 10)
  --min-area <px>     最小显著面积 (默认: 20)
  --sim-thresh <f>    总体相似度阈值 (默认: 0.90)

ROI配置:
  --manual-roi        启用手动ROI选择模式 (交互式)
  --load-roi <file>   从JSON文件加载ROI配置
  --save-roi <file>   保存ROI配置到JSON文件

  --help              显示此帮助信息

示例:
  Demo.exe data/demo
  Demo.exe --verbose --roi-count 6 data/demo
  Demo.exe --edge-tol 3 --noise-size 15 --sim-thresh 0.92 data/demo
  Demo.exe --load-roi roi_config.json --save-roi roi_config.json data/demo
");
        }

        /// <summary>
        /// 解析命令行参数 - 与C++ demo保持一致
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

                    case "--output":
                        if (i + 1 < args.Length)
                            config.OutputDir = args[++i];
                        break;

                    case "--roi-count":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int roiCount))
                            config.ROICount = Math.Max(1, Math.Min(20, roiCount));
                        break;

                    case "--seed":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int seed))
                        {
                            config.RandomSeed = seed;
                            config.SeedSpecified = true;
                        }
                        break;

                    case "--verbose":
                    case "-v":
                        config.VerboseMode = true;
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
                        config.BinaryParams.Enabled = 0;
                        break;

                    case "--edge-tol":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int edgeTol))
                            config.BinaryParams.EdgeTolerancePixels = edgeTol;
                        break;

                    case "--noise-size":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int noiseSize))
                            config.BinaryParams.NoiseFilterSize = noiseSize;
                        break;

                    case "--min-area":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int minArea))
                            config.BinaryParams.MinSignificantArea = minArea;
                        break;

                    case "--sim-thresh":
                        if (i + 1 < args.Length && float.TryParse(args[++i], out float simThresh))
                            config.BinaryParams.OverallSimilarityThreshold = Math.Max(0.0f, Math.Min(1.0f, simThresh));
                        break;

                    case "--manual-roi":
                        config.ManualROIMode = true;
                        break;

                    case "--load-roi":
                        if (i + 1 < args.Length)
                            config.LoadROIFile = args[++i];
                        break;

                    case "--save-roi":
                        if (i + 1 < args.Length)
                            config.SaveROIFile = args[++i];
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
        /// 运行检测演示 - 仿照C++ demo.cpp实现
        /// </summary>
        private static void RunDemo(ProgramConfig config)
        {
            // 打印程序标题
            PrintHeader();

            // 验证数据目录
            if (!Directory.Exists(config.DataDir))
            {
                throw new DirectoryNotFoundException($"数据目录不存在: {config.DataDir}");
            }

            // 获取图像文件列表
            var (templateFile, imageFiles) = GetImageFiles(config);

            Console.WriteLine($"模板: {templateFile}");
            Console.WriteLine($"找到 {imageFiles.Count} 张测试图像\n");

            // 创建输出目录
            if (!Directory.Exists(config.OutputDir))
            {
                Directory.CreateDirectory(config.OutputDir);
            }

            // 使用检测器
            using (var detector = new DefectDetectorAPI())
            {
                // Step 1: 导入模板
                Console.WriteLine("Step 1: Creating detector...");
                Console.WriteLine("Step 2: Importing template image...");

                if (!detector.ImportTemplateFromFile(templateFile))
                {
                    throw new InvalidOperationException($"无法导入模板: {templateFile}");
                }

                var (templWidth, templHeight, templChannels) = detector.GetTemplateSize();
                Console.WriteLine($"  Template loaded: {templWidth}x{templHeight} pixels");
                Console.WriteLine($"  File: {templateFile}\n");

                // Step 2.5: 配置检测参数
                ConfigureDetector(detector, config);

                // Step 3: 设置ROI区域
                SetupROIs(detector, config, templWidth, templHeight);

                // Step 3.5: 配置自动对齐
                ConfigureAlignment(detector, config);

                // Step 4: 执行批量检测
                ProcessImages(detector, imageFiles, config);
            }
        }

        /// <summary>
        /// 打印程序标题
        /// </summary>
        private static void PrintHeader()
        {
            Console.WriteLine("========================================");
            Console.WriteLine("   Print Defect Detection System Demo");
            Console.WriteLine("========================================\n");
        }

        #endregion

        #region 图像文件处理

        /// <summary>
        /// 获取图像文件列表 - 仿照C++ demo实现
        /// </summary>
        private static (string templateFile, List<string> imageFiles) GetImageFiles(ProgramConfig config)
        {
            // 获取所有BMP文件（与C++ demo保持一致，只处理BMP）
            var allFiles = Directory.GetFiles(config.DataDir, "*.bmp")
                .OrderBy(f => f)
                .ToList();

            if (allFiles.Count == 0)
            {
                throw new FileNotFoundException($"在目录 {config.DataDir} 中未找到BMP图像文件");
            }

            string templateFile;
            List<string> imageFiles;

            // 查找 template.bmp 或使用第一张图片
            var defaultTemplate = Path.Combine(config.DataDir, "template.bmp");
            if (File.Exists(defaultTemplate))
            {
                templateFile = defaultTemplate;
                imageFiles = allFiles
                    .Where(f => Path.GetFileName(f).ToLower() != "template.bmp")
                    .ToList();
                Console.WriteLine("Template: " + templateFile + " (explicit template file)");
            }
            else
            {
                // 没有 template.bmp，使用第一张图片作为模板
                templateFile = allFiles[0];
                imageFiles = allFiles.Skip(1).ToList();
                Console.WriteLine("Template: " + templateFile + " (first image as template)");
            }

            if (imageFiles.Count == 0)
            {
                throw new InvalidOperationException("需要至少2张图像（1张模板 + 1张测试图）");
            }

            return (templateFile, imageFiles);
        }

        #endregion

        #region 检测器配置

        /// <summary>
        /// 配置检测器参数 - 仿照C++ demo实现
        /// </summary>
        private static void ConfigureDetector(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 2.5: Configuring detection parameters...");

            // 配置二值化阈值和匹配方法
            int binaryThresholdValue = DEFAULT_BINARY_THRESHOLD;
            detector.SetMatchMethod(DefectDetectorAPI.MatchMethod.Binary);
            detector.SetBinaryThreshold(binaryThresholdValue);
            detector.SetParameter("binary_threshold", binaryThresholdValue);

            Console.WriteLine($"  Comparison method: BINARY (for B&W images)");
            Console.WriteLine($"  binary_threshold = {binaryThresholdValue}");

            // 配置二值图像优化参数
            if (config.BinaryOptimizationEnabled)
            {
                Console.WriteLine("\n  *** Binary Image Optimization Mode ***");
                Console.WriteLine("  Binary Image Optimization: ENABLED");

                // 应用二值检测参数
                detector.SetBinaryDetectionParams(config.BinaryParams);

                // 打印配置的二值优化参数
                Console.WriteLine("  Parameters:");
                Console.WriteLine($"    - auto_detect_binary = {(config.BinaryParams.AutoDetectBinary != 0 ? "true" : "false")}");
                Console.WriteLine($"    - noise_filter_size = {config.BinaryParams.NoiseFilterSize} px");
                Console.WriteLine($"    - edge_tolerance_pixels = {config.BinaryParams.EdgeTolerancePixels} px");
                Console.WriteLine($"    - edge_diff_ignore_ratio = {config.BinaryParams.EdgeDiffIgnoreRatio * 100:F1}%");
                Console.WriteLine($"    - min_significant_area = {config.BinaryParams.MinSignificantArea} px");
                Console.WriteLine($"    - area_diff_threshold = {config.BinaryParams.AreaDiffThreshold * 100:F2}%");
                Console.WriteLine($"    - overall_similarity_threshold = {config.BinaryParams.OverallSimilarityThreshold * 100:F0}%");
            }
            else
            {
                Console.WriteLine("\n  Binary Image Optimization: DISABLED");
                Console.WriteLine("  (Use default algorithm, may have higher false positive rate)");
            }

            // 其他通用检测参数 - 与C++ demo保持一致
            detector.SetParameter("min_defect_size", DEFAULT_MIN_DEFECT_SIZE);
            detector.SetParameter("blur_kernel_size", 3);
            detector.SetParameter("detect_black_on_white", 1.0f);
            detector.SetParameter("detect_white_on_black", 1.0f);

            Console.WriteLine($"\n  min_defect_size = {DEFAULT_MIN_DEFECT_SIZE} (pixels)");
            Console.WriteLine($"  blur_kernel_size = 3");
            Console.WriteLine();
        }

        #endregion

        #region ROI设置

        /// <summary>
        /// 设置ROI区域 - 仿照C++ demo实现三种模式
        /// </summary>
        private static void SetupROIs(DefectDetectorAPI detector, ProgramConfig config, int templWidth, int templHeight)
        {
            List<System.Drawing.Rectangle> selectedROIs = new List<System.Drawing.Rectangle>();
            List<float> selectedThresholds = new List<float>();

            // 决定ROI选择模式（优先级：加载文件 > 手动选择 > 随机生成）
            if (!string.IsNullOrEmpty(config.LoadROIFile))
            {
                // 模式1: 从文件加载ROI配置
                LoadROIFromFile(config.LoadROIFile, selectedROIs, selectedThresholds);

                // 验证ROI是否在图像边界内
                for (int i = 0; i < selectedROIs.Count; i++)
                {
                    var roi = selectedROIs[i];
                    if (roi.X < 0 || roi.Y < 0 ||
                        roi.X + roi.Width > templWidth ||
                        roi.Y + roi.Height > templHeight)
                    {
                        Console.WriteLine($"Warning: ROI {i + 1} is outside template boundaries, adjusting...");
                        int newX = Math.Max(0, roi.X);
                        int newY = Math.Max(0, roi.Y);
                        int newW = Math.Min(roi.Width, templWidth - newX);
                        int newH = Math.Min(roi.Height, templHeight - newY);
                        selectedROIs[i] = new System.Drawing.Rectangle(newX, newY, newW, newH);
                    }
                }

                // 添加ROI到检测器
                Console.WriteLine($"Step 3: Loading ROI regions from file...");
                for (int i = 0; i < selectedROIs.Count; i++)
                {
                    var roi = selectedROIs[i];
                    int roiId = detector.AddROI(roi.X, roi.Y, roi.Width, roi.Height, selectedThresholds[i]);
                    Console.WriteLine($"  ROI {roiId}: ({roi.X}, {roi.Y}) {roi.Width}x{roi.Height} threshold={selectedThresholds[i]:F2}");
                }
            }
            else if (config.ManualROIMode)
            {
                // 模式2: 手动交互式ROI选择
                // 注意：C#控制台版本简化为提示用户输入坐标
                Console.WriteLine($"Step 3: Manual ROI selection mode...");
                Console.WriteLine($"  Target ROI count: {config.ROICount}");
                Console.WriteLine("\n  [注意] C#控制台版本不支持图形界面选择");
                Console.WriteLine("  请使用 --load-roi 从文件加载或使用随机生成模式\n");

                // 回退到随机生成
                Console.WriteLine("  Falling back to random ROI generation...\n");
                GenerateRandomROIs(detector, config, templWidth, templHeight, selectedROIs, selectedThresholds);
            }
            else
            {
                // 模式3: 随机ROI生成（默认模式）
                GenerateRandomROIs(detector, config, templWidth, templHeight, selectedROIs, selectedThresholds);
            }

            // 保存ROI配置（如果指定了保存文件）
            if (!string.IsNullOrEmpty(config.SaveROIFile) && selectedROIs.Count > 0)
            {
                SaveROIToFile(config.SaveROIFile, selectedROIs, selectedThresholds);
            }

            Console.WriteLine($"  Total ROIs: {detector.GetROICount()}\n");
        }

        /// <summary>
        /// 从JSON文件加载ROI配置
        /// </summary>
        private static void LoadROIFromFile(string filename, List<System.Drawing.Rectangle> rois, List<float> thresholds)
        {
            Console.WriteLine($"Step 3: Loading ROI regions from file...");

            if (!File.Exists(filename))
            {
                throw new FileNotFoundException($"ROI配置文件不存在: {filename}");
            }

            string json = File.ReadAllText(filename);
            var config = JsonSerializer.Deserialize<ROIConfig>(json);

            if (config?.ROIs == null || config.ROIs.Count == 0)
            {
                throw new InvalidOperationException($"ROI配置文件无效或为空: {filename}");
            }

            rois.Clear();
            thresholds.Clear();

            foreach (var item in config.ROIs)
            {
                rois.Add(new System.Drawing.Rectangle(item.X, item.Y, item.Width, item.Height));
                thresholds.Add(item.Threshold);
            }

            Console.WriteLine($"Loaded {rois.Count} ROIs from: {filename}");
        }

        /// <summary>
        /// 保存ROI配置到JSON文件
        /// </summary>
        private static void SaveROIToFile(string filename, List<System.Drawing.Rectangle> rois, List<float> thresholds)
        {
            var config = new ROIConfig
            {
                ROICount = rois.Count,
                ROIs = new List<ROIItem>()
            };

            for (int i = 0; i < rois.Count; i++)
            {
                config.ROIs.Add(new ROIItem
                {
                    X = rois[i].X,
                    Y = rois[i].Y,
                    Width = rois[i].Width,
                    Height = rois[i].Height,
                    Threshold = thresholds[i]
                });
            }

            var options = new JsonSerializerOptions { WriteIndented = true };
            string json = JsonSerializer.Serialize(config, options);
            File.WriteAllText(filename, json);

            Console.WriteLine($"ROI configuration saved to: {filename}");
        }

        /// <summary>
        /// 生成随机ROI区域 - 仿照C++ demo实现
        /// </summary>
        private static void GenerateRandomROIs(DefectDetectorAPI detector, ProgramConfig config,
            int templWidth, int templHeight,
            List<System.Drawing.Rectangle> selectedROIs, List<float> selectedThresholds)
        {
            Console.WriteLine($"Step 3: Setting up ROI regions (random placement)...");
            Console.WriteLine($"  Random seed: {config.RandomSeed}" +
                (config.SeedSpecified ? " (user-specified)" : " (time-based)"));
            Console.WriteLine($"  Generating {config.ROICount} random ROI regions...");

            // 使用随机种子初始化随机数生成器
            var rng = new Random(config.RandomSeed);

            // ROI尺寸基于模板图像尺寸计算
            int roiW = templWidth / 25;
            int roiH = templHeight / 25;

            // 边缘边距，避免ROI太靠近图像边缘
            int marginX = templWidth / 5;  // 20% 水平边距
            int marginY = templHeight / 5;  // 20% 垂直边距
            int maxX = templWidth - roiW - marginX;
            int maxY = templHeight - roiH - marginY;

            // ROI阈值列表
            float[] thresholds = { 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f };

            // ROI名称
            string[] roiNames = { "Logo", "Text", "Barcode", "Date", "Batch", "Symbol", "Border", "Extra" };

            // 生成随机ROI区域
            for (int r = 0; r < config.ROICount; r++)
            {
                int x = rng.Next(marginX, Math.Max(marginX + 1, maxX));
                int y = rng.Next(marginY, Math.Max(marginY + 1, maxY));
                float threshold = thresholds[r % thresholds.Length];
                string name = roiNames[r % roiNames.Length];

                int roiId = detector.AddROI(x, y, roiW, roiH, threshold);
                selectedROIs.Add(new System.Drawing.Rectangle(x, y, roiW, roiH));
                selectedThresholds.Add(threshold);

                Console.WriteLine($"  ROI {roiId}: {name} region at ({x},{y}) threshold={threshold:F2}");
            }
        }

        #endregion

        #region 对齐配置

        /// <summary>
        /// 配置自动对齐 - 仿照C++ demo实现
        /// </summary>
        private static void ConfigureAlignment(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 3.5: Auto alignment configuration...");

            detector.EnableAutoLocalization(config.AutoAlignEnabled);
            detector.SetAlignmentMode(config.AlignMode);

            Console.WriteLine($"  Auto localization: {(config.AutoAlignEnabled ? "ENABLED" : "DISABLED")}");

            if (config.AutoAlignEnabled)
            {
                string modeStr = config.AlignMode switch
                {
                    DefectDetectorAPI.AlignmentMode.None => "NONE",
                    DefectDetectorAPI.AlignmentMode.FullImage => "FULL_IMAGE (slower, whole image transform)",
                    DefectDetectorAPI.AlignmentMode.RoiOnly => "ROI_ONLY (faster, recommended)",
                    _ => "Unknown"
                };

                Console.WriteLine($"  Alignment mode: {modeStr}");
                Console.WriteLine("  This corrects position offset, rotation and scale differences");

                Console.WriteLine("\n  [Alignment Tuning Tips]");
                Console.WriteLine("    - Use --verbose to see alignment details");
                Console.WriteLine("    - If alignment fails often, check image quality");
                Console.WriteLine("    - ROI_ONLY mode is 30-50% faster than FULL_IMAGE");
                Console.WriteLine("    - For large misalignment (>50px), use FULL_IMAGE mode");
            }
            else
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("  WARNING: May produce false positives if images are not aligned!");
                Console.ResetColor();
            }

            Console.WriteLine();
        }

        #endregion

        #region 图像处理和结果输出

        /// <summary>
        /// 处理所有测试图像 - 仿照C++ demo实现
        /// </summary>
        private static void ProcessImages(DefectDetectorAPI detector, List<string> imageFiles, ProgramConfig config)
        {
            Console.WriteLine("Step 4: Processing test images...");
            Console.WriteLine("----------------------------------------");

            // 限制处理数量
            int processCount = Math.Min(20, imageFiles.Count);

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

            // 预获取ROI列表
            var rois = new List<DefectDetectorAPI.ROIInfo>();
            for (int i = 0; i < detector.GetROICount(); i++)
            {
                rois.Add(detector.GetROIInfo(i));
            }

            // 处理循环
            bool userQuit = false;
            for (int i = 0; i < processCount && !userQuit; i++)
            {
                string testFile = imageFiles[i];
                string filename = Path.GetFileName(testFile);

                try
                {
                    // 执行检测
                    var result = detector.DetectFromFileWithFullResult(testFile);

                    totalTime += result.ProcessingTimeMs;
                    totalLocTime += result.LocalizationTimeMs;
                    totalRoiTime += result.ROIComparisonTimeMs;

                    // 获取ROI结果
                    var roiResults = detector.GetLastROIResults();

                    // 统计通过和未通过的ROI
                    int passedROIs = 0;
                    float minSimilarity = 1.0f;
                    int zeroSimilarityCount = 0;
                    foreach (var roiResult in roiResults)
                    {
                        if (roiResult.Passed != 0) passedROIs++;
                        if (roiResult.Similarity < minSimilarity)
                            minSimilarity = roiResult.Similarity;
                        if (roiResult.Similarity == 0.0f)
                            zeroSimilarityCount++;
                    }

                    // 瑕疵检测判定 - 与C++ demo一致
                    const int DEFECT_SIZE_THRESHOLD = 100;
                    bool hasSignificantDefects = false;
                    List<string> defectInfoList = new List<string>();
                    List<int> defectROIIds = new List<int>();

                    // 获取瑕疵详情
                    var defectResult = detector.DetectFromFileEx(testFile);
                    foreach (var defect in defectResult.Defects)
                    {
                        if (defect.Area >= DEFECT_SIZE_THRESHOLD)
                        {
                            hasSignificantDefects = true;
                            defectInfoList.Add($"ROI{defect.RoiId}[{(int)defect.Area}px@({(int)defect.X},{(int)defect.Y})]");
                            if (!defectROIIds.Contains(defect.RoiId))
                                defectROIIds.Add(defect.RoiId);
                        }
                    }

                    // 综合判定
                    bool shouldPass = !hasSignificantDefects && result.OverallPassed != 0;
                    bool finalPass = !hasSignificantDefects;

                    // 详细模式输出
                    if (config.VerboseMode)
                    {
                        PrintVerboseInfo(detector, config, result, roiResults, defectInfoList);
                    }

                    // 输出ROI瑕疵详情（非详细模式下也输出关键信息）
                    if (hasSignificantDefects)
                    {
                        PrintROIDefectDetails(roiResults, defectResult.Defects, DEFECT_SIZE_THRESHOLD, rois);
                    }

                    // 主输出行
                    PrintResultLine(i + 1, processCount, filename, finalPass, passedROIs,
                        roiResults.Length, minSimilarity, zeroSimilarityCount,
                        hasSignificantDefects ? defectInfoList.Count : 0,
                        result.ProcessingTimeMs,
                        hasSignificantDefects ? defectROIIds : null);

                    // 统计更新
                    if (finalPass)
                        passCount++;
                    else
                        failCount++;

                    // 收集对齐统计
                    if (config.AutoAlignEnabled && result.Localization.Success != 0)
                    {
                        alignSuccessCount++;
                        var loc = result.Localization;
                        float offset = (float)Math.Sqrt(loc.OffsetX * loc.OffsetX + loc.OffsetY * loc.OffsetY);

                        maxOffset = Math.Max(maxOffset, offset);
                        minOffset = Math.Min(minOffset, offset);
                        totalOffset += offset;

                        maxRotation = Math.Max(maxRotation, Math.Abs(loc.RotationAngle));
                        minRotation = Math.Min(minRotation, Math.Abs(loc.RotationAngle));
                        totalRotation += Math.Abs(loc.RotationAngle);

                        maxScale = Math.Max(maxScale, loc.Scale);
                        minScale = Math.Min(minScale, loc.Scale);
                        totalScale += loc.Scale;
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
            int passedROIs, int totalROIs, float minSimilarity, int zeroCount, int defectCount,
            double processingTime, List<int> defectROIIds = null)
        {
            // 截断过长的文件名
            if (filename.Length > 35)
                filename = filename.Substring(0, 32) + "...";

            // 设置颜色
            Console.ForegroundColor = passed ? ConsoleColor.Green : ConsoleColor.Red;
            string status = passed ? "PASS" : "FAIL";

            Console.Write($"[{index,3}/{total}] {filename,-35} | {status}");
            Console.ResetColor();

            Console.Write($" | {passedROIs}/{totalROIs} ROIs");
            Console.Write($" | min:{minSimilarity * 100,5:F1}%");

            if (zeroCount > 0)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.Write($" ({zeroCount} null)");
                Console.ResetColor();
            }

            if (defectCount > 0)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.Write($" [DEFECTS:{defectCount}]");
                // 显示有瑕疵的ROI编号
                if (defectROIIds != null && defectROIIds.Count > 0)
                {
                    Console.Write($" ROI[{string.Join(",", defectROIIds)}]");
                }
                Console.ResetColor();
            }

            Console.WriteLine($" | {processingTime,6:F1} ms");
        }

        /// <summary>
        /// 打印ROI瑕疵详情 - 输出哪个ROI框出了瑕疵
        /// </summary>
        private static void PrintROIDefectDetails(DefectDetectorAPI.ROIResult[] roiResults,
            DefectDetectorAPI.DefectInfo[] defects, int defectSizeThreshold,
            List<DefectDetectorAPI.ROIInfo> rois)
        {
            // 按ROI分组统计瑕疵
            var defectsByROI = defects
                .Where(d => d.Area >= defectSizeThreshold)
                .GroupBy(d => d.RoiId)
                .OrderBy(g => g.Key);

            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("    |- [瑕疵详情 - ROI定位]:");

            foreach (var group in defectsByROI)
            {
                int roiId = group.Key;
                var roiInfo = rois.FirstOrDefault(r => r.Id == roiId);
                var roiResult = roiResults.FirstOrDefault(r => r.RoiId == roiId);

                // 输出ROI基本信息
                if (roiInfo.Id != 0)
                {
                    Console.WriteLine($"       ROI {roiId}: 位置({roiInfo.X},{roiInfo.Y}) " +
                        $"大小{roiInfo.Width}x{roiInfo.Height} " +
                        $"相似度:{roiResult.Similarity * 100:F1}%");
                }
                else
                {
                    Console.WriteLine($"       ROI {roiId}: 信息未获取");
                }

                // 输出该ROI下的所有瑕疵
                foreach (var defect in group)
                {
                    // 计算瑕疵在ROI中的相对位置
                    float relX = defect.X - roiInfo.X;
                    float relY = defect.Y - roiInfo.Y;

                    Console.WriteLine($"         └─ 瑕疵: 绝对位置({defect.X:F0},{defect.Y:F0}) " +
                        $"相对ROI({relX:F0},{relY:F0}) " +
                        $"面积{defect.Area:F0}px " +
                        $"大小{defect.Width:F0}x{defect.Height:F0} " +
                        $"严重度:{defect.Severity:F2}");
                }
            }

            Console.ResetColor();
        }

        /// <summary>
        /// 打印详细信息（verbose模式）
        /// </summary>
        private static void PrintVerboseInfo(DefectDetectorAPI detector, ProgramConfig config,
            DefectDetectorAPI.DetectionResult result,
            DefectDetectorAPI.ROIResult[] roiResults,
            List<string> defectInfoList)
        {
            // 显示对齐信息
            if (config.AutoAlignEnabled)
            {
                var loc = result.Localization;
                Console.Write("    |- Align: ");
                Console.ForegroundColor = loc.Success != 0 ? ConsoleColor.Green : ConsoleColor.Red;
                Console.Write(loc.Success != 0 ? "OK" : "FAIL");
                Console.ResetColor();
                Console.WriteLine($" | offset=({loc.OffsetX:F2}, {loc.OffsetY:F2})px" +
                    $" | rot={loc.RotationAngle:F3}deg" +
                    $" | scale={loc.Scale:F4}");
            }

            // 显示每个ROI的相似度
            Console.Write("    |- ROIs: ");
            for (int r = 0; r < roiResults.Length; r++)
            {
                var roi = roiResults[r];
                Console.ForegroundColor = roi.Passed != 0 ? ConsoleColor.Green : ConsoleColor.Red;
                Console.Write((roi.Passed != 0 ? "[P]" : "[F]") + $"{roi.Similarity * 100:F0}%");
                Console.ResetColor();
                if (r < roiResults.Length - 1) Console.Write(" ");
            }
            Console.WriteLine();

            // 显示瑕疵信息
            if (defectInfoList.Count > 0)
            {
                Console.Write("    |- DEFECTS: ");
                for (int d = 0; d < defectInfoList.Count; d++)
                {
                    Console.Write(defectInfoList[d]);
                    if (d < defectInfoList.Count - 1) Console.Write(", ");
                }
                Console.WriteLine();
            }
        }

        #endregion

        #region 统计报告

        /// <summary>
        /// 打印统计汇总报告 - 仿照C++ demo实现
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
            Console.WriteLine("           Summary Report");
            Console.WriteLine("========================================");
            Console.WriteLine("Comparison method: BINARY");
            Console.WriteLine($"Binary Optimization: {(config.BinaryOptimizationEnabled ? "ENABLED" : "DISABLED")}");

            if (config.BinaryOptimizationEnabled)
            {
                Console.WriteLine($"  - edge_tolerance = {config.BinaryParams.EdgeTolerancePixels} px");
                Console.WriteLine($"  - noise_filter = {config.BinaryParams.NoiseFilterSize} px");
                Console.WriteLine($"  - similarity_threshold = {config.BinaryParams.OverallSimilarityThreshold * 100:F0}%");
            }

            string alignModeStr = config.AutoAlignEnabled
                ? (config.AlignMode == DefectDetectorAPI.AlignmentMode.FullImage ? "FULL_IMAGE" : "ROI_ONLY")
                : "DISABLED";
            Console.WriteLine($"Alignment mode: {alignModeStr}");

            Console.WriteLine("----------------------------------------");
            Console.WriteLine($"Images processed: {processCount}");
            Console.WriteLine($"Passed: {passCount}");
            Console.WriteLine($"Failed: {failCount}");

            double passRate = 100.0 * passCount / processCount;
            Console.WriteLine($"Pass rate: {passRate:F1}%");

            Console.WriteLine("----------------------------------------");
            Console.WriteLine($"Average total time: {avgTotal:F2} ms");

            if (config.AutoAlignEnabled)
            {
                Console.WriteLine($"  - Localization time: {avgLoc:F2} ms");
                Console.WriteLine($"  - ROI comparison time: {avgRoi:F2} ms");
            }

            Console.WriteLine("----------------------------------------");

            // 对齐统计
            if (config.AutoAlignEnabled && alignSuccessCount > 0)
            {
                Console.WriteLine("Alignment Statistics:");
                Console.WriteLine($"  Success rate: {alignSuccessCount}/{processCount} ({100.0 * alignSuccessCount / processCount:F1}%)");
                Console.WriteLine($"  Offset (px):  min={minOffset:F2}, max={maxOffset:F2}, avg={totalOffset / alignSuccessCount:F2}");
                Console.WriteLine($"  Rotation (deg): min={minRotation:F3}, max={maxRotation:F3}, avg={totalRotation / alignSuccessCount:F3}");
                Console.WriteLine($"  Scale:        min={minScale:F4}, max={maxScale:F4}, avg={totalScale / alignSuccessCount:F4}");
                Console.WriteLine("----------------------------------------");

                // 对齐调参建议
                Console.WriteLine("[Alignment Tuning Suggestions]");
                if (maxOffset > 20.0f)
                {
                    Console.WriteLine("  - Large offset detected (>20px), alignment is working well");
                    Console.WriteLine("  - If results are poor, increase edge_tolerance_pixels");
                }
                if (maxRotation > 2.0f)
                {
                    Console.WriteLine("  - Large rotation detected (>2deg), ensure images are captured consistently");
                }
            }
            else if (!config.AutoAlignEnabled)
            {
                Console.WriteLine("NOTE: Alignment disabled. Use --align-roi or --align-full to enable.");
                Console.WriteLine("      Compare results with alignment to diagnose accuracy issues.");
                Console.WriteLine("----------------------------------------");
            }

            // 性能评估
            bool perfPass = avgRoi <= 20.0;
            Console.Write("ROI Comparison Performance: ");
            Console.ForegroundColor = perfPass ? ConsoleColor.Green : ConsoleColor.Red;
            Console.Write(perfPass ? "PASS" : "FAIL");
            Console.ResetColor();
            Console.WriteLine(" (target: <= 20ms per readme.md)");

            // 最终调参建议
            Console.WriteLine("----------------------------------------");
            Console.WriteLine("[Final Tuning Recommendations]");

            if (passCount == 0)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("  WARNING: All images failed!");
                Console.ResetColor();
                Console.WriteLine("  - Try increasing --sim-thresh (e.g., 0.95)");
                Console.WriteLine("  - Try increasing --edge-tol (e.g., 5-6)");
                Console.WriteLine("  - Check if template is representative");
                Console.WriteLine("  - Verify alignment is working with --verbose");
            }
            else if (failCount == 0)
            {
                Console.WriteLine("  NOTE: All images passed!");
                Console.WriteLine("  - If this is expected (all good samples), parameters are good");
                Console.WriteLine("  - Test with known defective samples to verify detection");
                Console.WriteLine("  - Consider lowering --sim-thresh for stricter detection");
            }
            else
            {
                Console.WriteLine("  Mixed results - review failed images manually");
                Console.WriteLine("  - Use --verbose to see detailed info");
                Console.WriteLine("  - Adjust --sim-thresh based on false positive/negative rate");
            }

            Console.WriteLine("========================================");
        }

        #endregion
    }
}
