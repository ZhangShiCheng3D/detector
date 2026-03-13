/*
 * ============================================================================
 * 文件名: Program.cs
 * 描述:   喷印瑕疵检测系统 C# 演示程序 - 带CSV定位数据版本
 * 版本:   2.2 (API v2.0 + demo_cpp 特性)
 *
 * 功能说明:
 *   本程序演示如何使用 DefectDetectorAPI 进行瑕疵检测，包括：
 *   - 从CSV文件读取定位点数据
 *   - 使用外部变换进行图像对齐
 *   - 批量图像检测
 *   - 详细结果输出
 *   - API v2.0 新增参数支持
 *   - 完整的命令行参数（与demo_cpp一致）
 *
 * 使用方法:
 *   DefectDetection.exe --template <模板文件> --roi <x,y,width,height,threshold> [选项]
 *
 *   必需参数:
 *     --template <file>   指定模板文件路径
 *     --roi <x,y,w,h,t>   指定ROI参数 (例如: --roi 400,50,950,750,0.9)
 *
 *   对齐选项:
 *     --align-roi         使用ROI对齐模式 (默认)
 *     --align-full        使用全图对齐模式
 *     --no-align          禁用对齐
 *
 *   二值图像优化参数:
 *     --binary-thresh <n> 二值化阈值 (默认: 128)
 *     --edge-tol <px>     边缘容错像素数 (默认: 10)
 *     --noise-size <px>   噪声过滤尺寸 (默认: 3)
 *     --min-area <px>     最小显著面积 (默认: 20)
 *     --min-defect <n>    最小瑕疵尺寸 (默认: 100)
 *     --sim-thresh <f>    总体相似度阈值 (默认: 0.90)
 *
 *   API v2.0 新增参数:
 *     --edge-defect-size <n> 边缘大瑕疵保留阈值 (默认: 500)
 *     --edge-dist-mult <f>   边缘安全距离倍数 (默认: 2.0)
 *
 *   其他选项:
 *     --help              显示帮助信息
 *     --output <dir>      指定输出目录
 *     --max-images <n>    最大处理图像数量 (默认: 全部)
 *     --verbose, -v       显示详细信息
 *     --save              保存检测结果图像
 *
 * 示例:
 *   DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 data/test
 *   DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 --edge-tol 6 data/demo
 *   DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 --binary-thresh 128 --edge-defect-size 500 data/test
 * ============================================================================
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Drawing;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace DefectDetection
{
    /// <summary>
    /// 喷印瑕疵检测系统演示程序 - 带CSV定位数据版本
    /// </summary>
    public class Program
    {
        #region 配置常量

        /// <summary>默认输出目录</summary>
        private const string DEFAULT_OUTPUT_DIR = "output";

        /// <summary>CSV文件名</summary>
        private const string CSV_FILENAME = "匹配信息_116_20260304161622333.csv";

        /// <summary>默认二值化阈值</summary>
        private const int DEFAULT_BINARY_THRESHOLD = 128;

        /// <summary>默认边缘容错像素数</summary>
        private const int DEFAULT_EDGE_TOLERANCE = 10;

        /// <summary>默认噪声过滤尺寸</summary>
        private const int DEFAULT_NOISE_SIZE = 3;

        /// <summary>默认最小显著面积</summary>
        private const int DEFAULT_MIN_AREA = 20;

        /// <summary>默认最小瑕疵尺寸</summary>
        private const int DEFAULT_MIN_DEFECT_SIZE = 100;

        /// <summary>默认边缘大瑕疵保留阈值 (API v2.0)</summary>
        private const int DEFAULT_EDGE_DEFECT_SIZE = 500;

        /// <summary>默认边缘安全距离倍数 (API v2.0)</summary>
        private const float DEFAULT_EDGE_DIST_MULT = 2.0f;

        /// <summary>默认总体相似度阈值</summary>
        private const float DEFAULT_SIM_THRESHOLD = 0.90f;

        #endregion

        #region 程序配置结构

        /// <summary>
        /// ROI参数结构
        /// </summary>
        private class ROIParams
        {
            public int X { get; set; }
            public int Y { get; set; }
            public int Width { get; set; }
            public int Height { get; set; }
            public float Threshold { get; set; }

            public override string ToString()
            {
                return $"({X},{Y}) {Width}x{Height} 阈值={Threshold:F2}";
            }
        }

        /// <summary>
        /// 定位点数据
        /// </summary>
        private class LocationData
        {
            public string Filename { get; set; }
            public float X { get; set; }
            public float Y { get; set; }
            public float Rotation { get; set; }
            public bool IsValid { get; set; }

            public override string ToString()
            {
                return IsValid ?
                    $"({X:F2}, {Y:F2}) 旋转={Rotation:F3}°" :
                    "无效数据";
            }
        }

        /// <summary>
        /// 程序运行配置
        /// </summary>
        private class ProgramConfig
        {
            // 必需参数
            public string TemplateFile { get; set; } = null;
            public ROIParams ROI { get; set; } = null;

            // 路径配置
            public string DataDir { get; set; } = null;
            public string OutputDir { get; set; } = DEFAULT_OUTPUT_DIR;

            // 检测参数
            public int MaxImages { get; set; } = int.MaxValue;

            // 输出控制
            public bool Verbose { get; set; } = false;
            public bool SaveResults { get; set; } = false;

            // 二值图像优化参数 (from demo_cpp)
            public int BinaryThreshold { get; set; } = DEFAULT_BINARY_THRESHOLD;
            public int EdgeTolerancePixels { get; set; } = DEFAULT_EDGE_TOLERANCE;
            public int NoiseFilterSize { get; set; } = DEFAULT_NOISE_SIZE;
            public int MinSignificantArea { get; set; } = DEFAULT_MIN_AREA;
            public int MinDefectSize { get; set; } = DEFAULT_MIN_DEFECT_SIZE;
            public float SimilarityThreshold { get; set; } = DEFAULT_SIM_THRESHOLD;

            // API v2.0 新增参数
            public int EdgeDefectSizeThreshold { get; set; } = DEFAULT_EDGE_DEFECT_SIZE;
            public float EdgeDistanceMultiplier { get; set; } = DEFAULT_EDGE_DIST_MULT;

            // 对齐模式
            public DefectDetectorAPI.AlignmentMode AlignmentMode { get; set; } = DefectDetectorAPI.AlignmentMode.RoiOnly;
            public bool AutoAlignEnabled { get; set; } = true;
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
                // 如果没有提供参数，使用默认测试参数（仅用于调试）
                if (args == null || args.Length == 0)
                {
                    args = new string[]
                    {
                        "--template",
                        @"data\demo\template.bmp",
                        "--roi",
                        "200,420,820,320,0.8"
                    };
                }

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
用法: DefectDetection.exe --template <模板文件> --roi <x,y,w,h,t> [选项] [数据目录]

必需参数:
  --template <file>   指定模板文件路径
  --roi <x,y,w,h,t>   指定ROI参数，格式: x坐标,y坐标,宽度,高度,阈值
                      例如: --roi 400,50,950,750,0.9

选项:
  --help              显示此帮助信息
  --output <dir>      指定输出目录
  --max-images <n>    最大处理图像数量 (默认: 全部)

对齐选项:
  --align-roi         使用ROI对齐模式 (默认)
  --align-full        使用全图对齐模式
  --no-align          禁用对齐

二值图像优化参数:
  --binary-thresh <n> 二值化阈值 (默认: 128)
  --edge-tol <px>     边缘容错像素数 (默认: 10)
  --noise-size <px>   噪声过滤尺寸 (默认: 3)
  --min-area <px>     最小显著面积 (默认: 20)
  --min-defect <n>    最小瑕疵尺寸 (默认: 100)
  --sim-thresh <f>    总体相似度阈值 (默认: 0.90)

API v2.0 新增参数:
  --edge-defect-size <n> 边缘大瑕疵保留阈值 (默认: 500)
  --edge-dist-mult <f>   边缘安全距离倍数 (默认: 2.0)

输出控制:
  --verbose, -v       显示详细信息
  --save              保存检测结果图像

示例:
  DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 data/test
  DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 --edge-tol 6 data/demo
  DefectDetection.exe --template template.bmp --roi 400,50,950,750,0.9 --binary-thresh 128 --edge-defect-size 500 data/test
");
        }

        /// <summary>
        /// 解析ROI参数字符串
        /// </summary>
        private static ROIParams ParseROI(string roiStr)
        {
            var parts = roiStr.Split(',');
            if (parts.Length != 5)
                throw new ArgumentException("ROI参数格式错误，应为: x,y,width,height,threshold");

            try
            {
                return new ROIParams
                {
                    X = int.Parse(parts[0]),
                    Y = int.Parse(parts[1]),
                    Width = int.Parse(parts[2]),
                    Height = int.Parse(parts[3]),
                    Threshold = float.Parse(parts[4])
                };
            }
            catch (Exception ex)
            {
                throw new ArgumentException($"ROI参数解析失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 解析命令行参数
        /// </summary>
        private static ProgramConfig ParseArguments(string[] args)
        {
            var config = new ProgramConfig();

            for (int i = 0; i < args.Length; i++)
            {
                string arg = args[i].ToLower();

                switch (arg)
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

                    case "--roi":
                        if (i + 1 < args.Length)
                            config.ROI = ParseROI(args[++i]);
                        break;

                    case "--output":
                        if (i + 1 < args.Length)
                            config.OutputDir = args[++i];
                        break;

                    case "--max-images":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int maxImages))
                            config.MaxImages = maxImages;
                        break;

                    case "--verbose":
                    case "-v":
                        config.Verbose = true;
                        break;

                    case "--save":
                        config.SaveResults = true;
                        break;

                    // 对齐选项
                    case "--align-roi":
                        config.AlignmentMode = DefectDetectorAPI.AlignmentMode.RoiOnly;
                        config.AutoAlignEnabled = true;
                        break;

                    case "--align-full":
                        config.AlignmentMode = DefectDetectorAPI.AlignmentMode.FullImage;
                        config.AutoAlignEnabled = true;
                        break;

                    case "--no-align":
                        config.AlignmentMode = DefectDetectorAPI.AlignmentMode.None;
                        config.AutoAlignEnabled = false;
                        break;

                    // 二值图像优化参数
                    case "--binary-thresh":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int binThresh))
                            config.BinaryThreshold = Math.Max(0, Math.Min(255, binThresh));
                        break;

                    case "--edge-tol":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int edgeTol))
                            config.EdgeTolerancePixels = Math.Max(0, edgeTol);
                        break;

                    case "--noise-size":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int noiseSize))
                            config.NoiseFilterSize = Math.Max(1, noiseSize);
                        break;

                    case "--min-area":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int minArea))
                            config.MinSignificantArea = Math.Max(1, minArea);
                        break;

                    case "--min-defect":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int minDefect))
                            config.MinDefectSize = Math.Max(1, minDefect);
                        break;

                    case "--sim-thresh":
                        if (i + 1 < args.Length && float.TryParse(args[++i], out float simThresh))
                            config.SimilarityThreshold = Math.Max(0.0f, Math.Min(1.0f, simThresh));
                        break;

                    // API v2.0 新增参数
                    case "--edge-defect-size":
                        if (i + 1 < args.Length && int.TryParse(args[++i], out int edgeDefectSize))
                            config.EdgeDefectSizeThreshold = Math.Max(0, edgeDefectSize);
                        break;

                    case "--edge-dist-mult":
                        if (i + 1 < args.Length && float.TryParse(args[++i], out float edgeDistMult))
                            config.EdgeDistanceMultiplier = Math.Max(0.0f, edgeDistMult);
                        break;

                    default:
                        // 非选项参数视为数据目录
                        if (!arg.StartsWith("-"))
                            config.DataDir = args[i];
                        break;
                }
            }

            // 验证必需参数
            if (string.IsNullOrEmpty(config.TemplateFile))
            {
                Console.WriteLine("[错误] 必须指定模板文件 (--template)");
                PrintUsage();
                return null;
            }

            if (config.ROI == null)
            {
                Console.WriteLine("[错误] 必须指定ROI参数 (--roi)");
                PrintUsage();
                return null;
            }

            if (!File.Exists(config.TemplateFile))
            {
                throw new FileNotFoundException($"模板文件不存在: {config.TemplateFile}");
            }

            // 如果没有指定数据目录，使用模板文件所在目录
            if (string.IsNullOrEmpty(config.DataDir))
            {
                config.DataDir = Path.GetDirectoryName(config.TemplateFile);
                if (string.IsNullOrEmpty(config.DataDir))
                    config.DataDir = ".";
            }

            return config;
        }

        #endregion

        #region CSV文件读取

        /// <summary>
        /// 从CSV文件读取定位数据
        /// </summary>
        private static Dictionary<string, LocationData> LoadLocationData(string dataDir)
        {
            var locationDict = new Dictionary<string, LocationData>();
            string csvPath = Path.Combine(dataDir, CSV_FILENAME);

            if (!File.Exists(csvPath))
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"警告: CSV文件不存在: {csvPath}");
                Console.WriteLine("将使用默认值 (x=0, y=0, r=0)");
                Console.ResetColor();
                return locationDict;
            }

            Console.WriteLine($"读取CSV文件: {csvPath}");
            int lineCount = 0;
            int validCount = 0;

            try
            {
                var lines = File.ReadAllLines(csvPath);
                for (int i = 1; i < lines.Length; i++) // 跳过标题行
                {
                    lineCount++;
                    var line = lines[i].Trim();
                    if (string.IsNullOrEmpty(line))
                        continue;

                    var parts = line.Split(',');
                    if (parts.Length >= 4)
                    {
                        var data = new LocationData
                        {
                            Filename = parts[0].Trim(),
                            IsValid = !string.IsNullOrEmpty(parts[1]) &&
                                     !string.IsNullOrEmpty(parts[2]) &&
                                     !string.IsNullOrEmpty(parts[3])
                        };

                        if (data.IsValid)
                        {
                            if (float.TryParse(parts[1], out float x) &&
                                float.TryParse(parts[2], out float y) &&
                                float.TryParse(parts[3], out float r))
                            {
                                data.X = x;
                                data.Y = y;
                                data.Rotation = r;
                                validCount++;
                            }
                            else
                            {
                                data.IsValid = false;
                            }
                        }

                        locationDict[data.Filename] = data;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"警告: 读取CSV文件时出错: {ex.Message}");
                Console.ResetColor();
            }

            Console.WriteLine($"  - 总记录数: {lineCount}");
            Console.WriteLine($"  - 有效记录: {validCount}");
            Console.WriteLine($"  - 无效记录: {lineCount - validCount}");
            Console.WriteLine();

            return locationDict;
        }

        /// <summary>
        /// 获取图片的定位数据
        /// </summary>
        private static LocationData GetLocationData(Dictionary<string, LocationData> locationDict,
            string filename, bool verbose)
        {
            string shortFilename = Path.GetFileName(filename);

            if (locationDict.TryGetValue(shortFilename, out var data))
            {
                if (verbose)
                {
                    Console.WriteLine($"    定位数据: {data}");
                }
                return data;
            }

            // 没找到则返回默认值
            if (verbose)
            {
                Console.WriteLine($"    定位数据: 未找到，使用默认值 (0,0,0)");
            }

            return new LocationData
            {
                Filename = shortFilename,
                X = 0,
                Y = 0,
                Rotation = 0,
                IsValid = false
            };
        }

        #endregion

        #region 核心检测流程

        /// <summary>
        /// 运行检测演示
        /// </summary>
        private static void RunDemo(ProgramConfig config)
        {
            // ========== 打印程序标题 ==========
            PrintHeader(config);

            // ========== 验证数据目录 ==========
            if (!Directory.Exists(config.DataDir))
            {
                throw new DirectoryNotFoundException($"数据目录不存在: {config.DataDir}");
            }

            // ========== 读取CSV定位数据 ==========
            var locationDict = LoadLocationData(config.DataDir);

            // ========== 获取模板的定位数据 ==========
            string templateFilename = Path.GetFileName(config.TemplateFile);
            var templateLocation = GetLocationData(locationDict, templateFilename, config.Verbose);
            Console.WriteLine($"模板定位点: {templateLocation}");
            Console.WriteLine();

            // ========== 获取图像文件列表 ==========
            var imageFiles = GetImageFiles(config, templateFilename);
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
                if (!detector.ImportTemplateFromFile(config.TemplateFile))
                {
                    throw new InvalidOperationException($"无法导入模板: {config.TemplateFile}");
                }

                int width, height, channel;
                detector.GetTemplateSize(out width, out height, out channel);
                Console.WriteLine($"  模板尺寸: {width}x{height} 像素");
                Console.WriteLine();

                // Step 2: 配置检测参数
                ConfigureDetector(detector, config);

                // Step 3: 设置ROI区域（只设置一个ROI）
                SetupSingleROI(detector, config);

                // Step 4: 配置对齐模式
                ConfigureAlignment(detector, config);

                // Step 5: 执行批量检测
                ProcessImages(detector, imageFiles, locationDict, templateLocation, config);
            }
        }

        #endregion

        #region 辅助方法

        /// <summary>
        /// 打印程序标题
        /// </summary>
        private static void PrintHeader(ProgramConfig config)
        {
            Console.WriteLine("============================================================");
            Console.WriteLine("   喷印瑕疵检测系统 - 带CSV定位数据版本");
            Console.WriteLine("   Print Defect Detection System - CSV Location Version");
            Console.WriteLine("============================================================");
            Console.WriteLine($"模板文件: {config.TemplateFile}");
            Console.WriteLine($"ROI参数: {config.ROI}");
            Console.WriteLine($"数据目录: {config.DataDir}");
            Console.WriteLine($"输出目录: {config.OutputDir}");
            Console.WriteLine($"对齐模式: {(config.AutoAlignEnabled ? (config.AlignmentMode == DefectDetectorAPI.AlignmentMode.RoiOnly ? "ROI对齐" : "全图对齐") : "禁用")}");
            Console.WriteLine($"二值化阈值: {config.BinaryThreshold}");
            Console.WriteLine($"边缘容错: {config.EdgeTolerancePixels}px, 噪声过滤: {config.NoiseFilterSize}px");
            Console.WriteLine($"最小面积: {config.MinSignificantArea}px, 最小瑕疵: {config.MinDefectSize}px");
            Console.WriteLine($"相似度阈值: {config.SimilarityThreshold:F2}");
            Console.WriteLine("============================================================");
            Console.WriteLine();
        }

        /// <summary>
        /// 获取图像文件列表（排除模板文件）
        /// </summary>
        private static List<string> GetImageFiles(ProgramConfig config, string templateFilename)
        {
            var supportedExtensions = new[] { ".bmp", ".jpg", ".jpeg", ".png" };
            var allFiles = Directory.GetFiles(config.DataDir)
                .Where(f => supportedExtensions.Contains(Path.GetExtension(f).ToLower()))
                .OrderBy(f => f)
                .ToList();

            // 排除模板文件
            var imageFiles = allFiles.Where(f =>
                Path.GetFileName(f).ToLower() != templateFilename.ToLower()).ToList();

            if (imageFiles.Count == 0)
            {
                throw new InvalidOperationException("数据目录中除了模板外没有其他图像文件");
            }

            // 限制处理数量
            if (config.MaxImages < imageFiles.Count)
            {
                Console.WriteLine($"限制处理数量: {config.MaxImages} 张 (共 {imageFiles.Count} 张)");
                return imageFiles.Take(config.MaxImages).ToList();
            }

            return imageFiles;
        }

        /// <summary>
        /// 配置检测器参数
        /// </summary>
        private static void ConfigureDetector(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 2: 配置检测参数...");

            // 设置匹配方法为二值模式（适用于黑白图像）
            detector.SetMatchMethod(DefectDetectorAPI.MatchMethod.Binary);
            detector.SetBinaryThreshold(config.BinaryThreshold);

            // API v2.0: 配置二值检测参数，包含新增字段
            var binaryParams = new DefectDetectorAPI.BinaryDetectionParams
            {
                Enabled = 1,
                AutoDetectBinary = 1,
                NoiseFilterSize = config.NoiseFilterSize,
                EdgeTolerancePixels = config.EdgeTolerancePixels,
                EdgeDiffIgnoreRatio = 0.05f,
                MinSignificantArea = config.MinSignificantArea,
                AreaDiffThreshold = 0.001f,
                OverallSimilarityThreshold = config.SimilarityThreshold,
                // API v2.0 新增字段
                EdgeDefectSizeThreshold = config.EdgeDefectSizeThreshold,
                EdgeDistanceMultiplier = config.EdgeDistanceMultiplier,
                BinaryThreshold = config.BinaryThreshold
            };

            detector.SetBinaryDetectionParams(binaryParams);
            detector.SetParameter("min_defect_size", config.MinDefectSize);
            detector.SetParameter("blur_kernel_size", 3);
            detector.SetParameter("detect_black_on_white", 1.0f);
            detector.SetParameter("detect_white_on_black", 1.0f);

            Console.WriteLine($"  匹配方法: BINARY (二值图像)");
            Console.WriteLine($"  二值化阈值: {config.BinaryThreshold}");
            Console.WriteLine($"  边缘容错像素: {config.EdgeTolerancePixels}");
            Console.WriteLine($"  噪声过滤尺寸: {config.NoiseFilterSize}");
            Console.WriteLine($"  最小显著面积: {config.MinSignificantArea}");
            Console.WriteLine($"  最小瑕疵尺寸: {config.MinDefectSize}");
            Console.WriteLine($"  相似度阈值: {config.SimilarityThreshold:F2}");
            Console.WriteLine($"  边缘大瑕疵阈值: {config.EdgeDefectSizeThreshold} (API v2.0)");
            Console.WriteLine($"  边缘距离倍数: {config.EdgeDistanceMultiplier:F1} (API v2.0)");
            Console.WriteLine($"  二值图像优化: 已启用");
            Console.WriteLine();
        }

        /// <summary>
        /// 设置单个ROI区域
        /// </summary>
        private static void SetupSingleROI(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 3: 设置ROI区域...");

            var roi = config.ROI;
            int roiId = detector.AddROI(roi.X, roi.Y, roi.Width, roi.Height, roi.Threshold);

            Console.WriteLine($"  ROI {roiId}: {roi}");
            Console.WriteLine($"  总计: {detector.GetROICount()} 个ROI");

            // Fix: 添加ROI后需要重新导入模板，以确保ROI模板数据正确提取
            // 因为AddROI之后，需要从模板中提取对应ROI区域的模板数据用于比对
            Console.WriteLine($"  重新导入模板以提取ROI模板数据...");
            if (!detector.ImportTemplateFromFile(config.TemplateFile))
            {
                throw new InvalidOperationException($"重新导入模板失败: {config.TemplateFile}");
            }

            Console.WriteLine();
        }

        /// <summary>
        /// 配置对齐模式
        /// </summary>
        private static void ConfigureAlignment(DefectDetectorAPI detector, ProgramConfig config)
        {
            Console.WriteLine("Step 4: 配置自动对齐...");

            // 配置自动对齐模式和外部变换支持
            detector.EnableAutoLocalization(config.AutoAlignEnabled);
            detector.SetAlignmentMode(config.AlignmentMode);

            string modeStr = config.AlignmentMode switch
            {
                DefectDetectorAPI.AlignmentMode.None => "禁用对齐",
                DefectDetectorAPI.AlignmentMode.FullImage => "全图对齐",
                DefectDetectorAPI.AlignmentMode.RoiOnly => "ROI对齐",
                _ => "未知"
            };

            Console.WriteLine($"  自动对齐: {(config.AutoAlignEnabled ? "已启用" : "已禁用")}");
            Console.WriteLine($"  对齐模式: {modeStr}");
            Console.WriteLine($"  外部变换: 已启用 (使用CSV定位数据)");
            Console.WriteLine();
        }

        #endregion

        #region 批量处理

        /// <summary>
        /// 处理所有测试图像
        /// </summary>
        private static void ProcessImages(DefectDetectorAPI detector,
            List<string> imageFiles,
            Dictionary<string, LocationData> locationDict,
            LocationData templateLocation,
            ProgramConfig config)
        {
            Console.WriteLine("Step 5: 处理测试图像...");
            Console.WriteLine("----------------------------------------");

            // 统计变量
            int passCount = 0;
            int failCount = 0;
            double totalTime = 0;

            // 偏移统计
            float maxOffset = 0, minOffset = float.MaxValue, totalOffset = 0;
            float maxRotationDiff = 0, minRotationDiff = float.MaxValue, totalRotationDiff = 0;
            int validLocationCount = 0;

            // 处理每张图像
            for (int i = 0; i < imageFiles.Count; i++)
            {
                string testFile = imageFiles[i];
                string filename = Path.GetFileName(testFile);

                try
                {
                    // 获取当前图片的定位数据
                    var testLocation = GetLocationData(locationDict, filename, config.Verbose);

                    // 计算与模板的差值
                    float offsetX = testLocation.IsValid ? testLocation.X - templateLocation.X : 0;
                    float offsetY = testLocation.IsValid ? testLocation.Y - templateLocation.Y : 0;
                    float rotationDiff = testLocation.IsValid ? testLocation.Rotation - templateLocation.Rotation : 0;

                    if (testLocation.IsValid)
                    {
                        validLocationCount++;

                        float offset = (float)Math.Sqrt(offsetX * offsetX + offsetY * offsetY);
                        maxOffset = Math.Max(maxOffset, offset);
                        minOffset = Math.Min(minOffset, offset);
                        totalOffset += offset;

                        float absRotation = Math.Abs(rotationDiff);
                        maxRotationDiff = Math.Max(maxRotationDiff, absRotation);
                        minRotationDiff = Math.Min(minRotationDiff, absRotation);
                        totalRotationDiff += absRotation;
                    }

                    // 设置外部变换（使用CSV定位数据）
                    if (testLocation.IsValid)
                    {
                        detector.SetExternalTransform(offsetX, offsetY, rotationDiff);

                        if (config.Verbose)
                        {
                            Console.WriteLine($"  设置外部变换: 偏移({offsetX:F2}, {offsetY:F2})px, 旋转{rotationDiff:F3}°");
                        }
                    }

                    // 执行检测
                    //var result = detector.DetectFromFile(testFile);
                    var result = detector.DetectFromFileWithFullResult(testFile);

                    // 累计时间
                    totalTime += result.ProcessingTimeMs;
                    // 获取ROI结果
                    var roiResults = detector.GetLastROIResults();

                    // 获取ROI结果
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

                    // 输出结果行
                    PrintResultLine(i + 1, imageFiles.Count, filename, finalPass,
                        passedROIs, roiResults.Length, minSimilarity, result.ProcessingTimeMs,
                        offsetX, offsetY, rotationDiff);

                    // 详细模式：显示更多信息
                    if (config.Verbose)
                    {
                        PrintVerboseInfo(roiResults);
                    }

                    // 更新统计计数
                    if (finalPass)
                        passCount++;
                    else
                        failCount++;

                    // 清除外部变换，避免影响下一张图片
                    detector.ClearExternalTransform();
                }
                catch (Exception ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"[{i + 1,3}/{imageFiles.Count}] {Path.GetFileName(filename),-35} | 错误: {ex.Message}");
                    Console.ResetColor();
                    failCount++;

                    // 确保清除外部变换
                    detector.ClearExternalTransform();
                }
            }

            // 打印统计报告
            PrintSummaryReport(config, imageFiles.Count, passCount, failCount, totalTime,
                validLocationCount, minOffset, maxOffset, totalOffset,
                minRotationDiff, maxRotationDiff, totalRotationDiff);
        }

        /// <summary>
        /// 打印单个结果行
        /// </summary>
        private static void PrintResultLine(int index, int total, string filename, bool passed,
            int passedRois, int totalRois, float minSimilarity, double processingTime,
            float offsetX, float offsetY, float rotationDiff)
        {
            // 设置颜色
            Console.ForegroundColor = passed ? ConsoleColor.Green : ConsoleColor.Red;
            string status = passed ? "PASS" : "FAIL";

            // 截断过长的文件名
            if (filename.Length > 30)
                filename = filename.Substring(0, 27) + "...";

            Console.Write($"[{index,3}/{total}] {filename,-30} | {status}");
            Console.ResetColor();

            Console.Write($" | {passedRois}/{totalRois}");
            Console.Write($" | sim:{minSimilarity * 100,5:F1}%");
            Console.Write($" | Δ({offsetX,6:F1},{offsetY,6:F1})");
            Console.Write($" | Δθ{rotationDiff,6:F3}°");
            Console.WriteLine($" | {processingTime,6:F1}ms");
        }

        /// <summary>
        /// 打印详细信息
        /// </summary>
        private static void PrintVerboseInfo(DefectDetectorAPI.ROIResult[] roiResults)
        {
            Console.Write("    └─ ROIs: ");
            for (int r = 0; r < roiResults.Length; r++)
            {
                var roi = roiResults[r];
                Console.ForegroundColor = roi.Passed == 1 ? ConsoleColor.Green : ConsoleColor.Red;
                Console.Write(roi.Passed == 1 ? "✓" : "✗");
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
        private static void PrintSummaryReport(ProgramConfig config, int totalImages,
            int passCount, int failCount, double totalTime,
            int validLocationCount, float minOffset, float maxOffset, float totalOffset,
            float minRotationDiff, float maxRotationDiff, float totalRotationDiff)
        {
            double avgTime = totalTime / totalImages;

            Console.WriteLine();
            Console.WriteLine("============================================================");
            Console.WriteLine("                   统计报告 Summary");
            Console.WriteLine("============================================================");

            // 配置信息
            Console.WriteLine($"模板文件: {Path.GetFileName(config.TemplateFile)}");
            Console.WriteLine($"ROI区域: {config.ROI}");
            Console.WriteLine("----------------------------------------");

            // 检测结果统计
            Console.WriteLine($"处理图像数: {totalImages}");
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"通过: {passCount}");
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"失败: {failCount}");
            Console.ResetColor();

            double passRate = 100.0 * passCount / totalImages;
            Console.ForegroundColor = passRate >= 80 ? ConsoleColor.Green :
                (passRate >= 50 ? ConsoleColor.Yellow : ConsoleColor.Red);
            Console.WriteLine($"通过率: {passRate:F1}%");
            Console.ResetColor();

            Console.WriteLine($"平均耗时: {avgTime:F2} ms");
            Console.WriteLine("----------------------------------------");

            // 定位数据统计
            Console.WriteLine($"定位数据:");
            Console.WriteLine($"  有效记录: {validLocationCount}/{totalImages} ({100.0 * validLocationCount / totalImages:F1}%)");

            if (validLocationCount > 0)
            {
                Console.WriteLine($"  偏移 (px):  min={minOffset:F2}, max={maxOffset:F2}, avg={totalOffset / validLocationCount:F2}");
                Console.WriteLine($"  旋转差 (°): min={minRotationDiff:F3}, max={maxRotationDiff:F3}, avg={totalRotationDiff / validLocationCount:F3}");
            }
            else
            {
                Console.WriteLine($"  所有图片使用默认值 (0,0,0)");
            }

            Console.WriteLine("============================================================");
        }

        #endregion
    }
}