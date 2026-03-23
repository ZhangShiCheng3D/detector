using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using System.Drawing.Imaging;
using System.Threading.Tasks;
using System.Threading;
using Microsoft.Win32;
using System.Net.Sockets;
using System.Net;
using static DetectionUITest.DefectDetectorAPI;

namespace DetectionUITest
{
    public partial class Form1 : Form
    {
        #region 私有字段

        private DefectDetectorAPI detector;
        private string templateFile;
        private List<string> testFiles = new List<string>();
        private int currentIndex = -1;
        private Dictionary<string, LocationData> locationDict = new Dictionary<string, LocationData>();
        private LocationData templateLocation;
        private bool isComparing = false;
        private CancellationTokenSource cancellationTokenSource;

        // 统计信息
        private int passCount = 0;
        private int failCount = 0;
        private Dictionary<string, ImageResult> imageResults = new Dictionary<string, ImageResult>();

        // ROI列表 - 使用本地存储的ROI信息
        private List<ROIItem> roiList = new List<ROIItem>();

        // 映射表：界面显示索引 -> API返回的ROI ID
        private Dictionary<int, int> uiIndexToApiId = new Dictionary<int, int>();

        // 映射表：API返回的ROI ID -> 界面显示索引
        private Dictionary<int, int> apiIdToUiIndex = new Dictionary<int, int>();

        // 鼠标绘制ROI相关
        private bool isDragging = false;
        private Point startPoint;
        private Rectangle currentROI;

        // 保存最后一次打开的路径
        private string lastOpenPath = "";

        // Socket客户端
        private SocketClient _socketClient;
        private bool _isProcessingImage = false;

        // 自动连接控制
        private bool _isAutoConnecting = false;
        private CancellationTokenSource _autoConnectCts;

        // Socket接收图片计数器
        private int socketImageCounter = 0;

        // ROI内缩像素数（与demo_cpp一致）
        private const int SHRINK_PIXELS = 5;

        // 瑕疵尺寸阈值（与demo_cpp一致）
        private const int DEFECT_SIZE_THRESHOLD = DefectDetectorAPI.DEFAULT_DEFECT_SIZE_THRESHOLD;

        #endregion

        #region 构造函数

        public Form1()
        {
            InitializeComponent();
            InitializeCustomComponents();
            SetupEventHandlers();
            detector = new DefectDetectorAPI();
            detector.EnableAPILog(true);
            detector.ClearAPILog();

            // 初始化检测器参数（确保与demo_cpp一致）
            InitializeDetectorParams();

            // 初始化Socket客户端
            InitializeSocketClient();

            // 从注册表加载上次打开的路径
            LoadLastOpenPath();

            // 绑定窗体Shown事件
            this.Shown += Form1_Shown;
        }

        private void Form1_Shown(object sender, EventArgs e)
        {
            // 窗体显示后开始自动连接
            StartAutoConnect();
        }

        private void InitializeDetectorParams()
        {
            if (detector == null) return;

            try
            {
                // 启用自动对齐（与demo_cpp默认一致）
                detector.EnableAutoLocalization(chkAutoAlign.Checked);

                // 设置对齐模式（与demo_cpp默认一致：ROI_ONLY）
                switch (cmbAlignMode.SelectedIndex)
                {
                    case 0:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.None);
                        break;
                    case 1:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.FullImage);
                        break;
                    case 2:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.RoiOnly);
                        break;
                }

                // 更新其他检测参数
                UpdateBinaryParams();
            }
            catch (Exception ex)
            {
                AddLog("错误", $"初始化检测器参数失败: {ex.Message}");
            }
        }

        private void InitializeSocketClient()
        {
            try
            {
                _socketClient = new SocketClient();
                _socketClient.ConnectionStatusChanged += SocketClient_ConnectionStatusChanged;
                _socketClient.LogMessage += SocketClient_LogMessage;
                _socketClient.MessageReceived += SocketClient_MessageReceived;
                _autoConnectCts = new CancellationTokenSource();
            }
            catch (Exception ex)
            {
                AddLog("错误", $"初始化Socket客户端失败: {ex.Message}");
            }
        }

        private void StartAutoConnect()
        {
            // 延迟一下再开始连接，确保界面完全加载
            Task.Delay(500).ContinueWith(_ =>
            {
                if (!this.IsDisposed && !_socketClient.IsConnected && !_isAutoConnecting)
                {
                    _ = AutoConnectLoop(_autoConnectCts.Token);
                }
            }, TaskScheduler.FromCurrentSynchronizationContext());
        }

        private async Task AutoConnectLoop(CancellationToken cancellationToken)
        {
            if (_isAutoConnecting || _socketClient.IsConnected) return;

            _isAutoConnecting = true;

            try
            {
                // 从注册表加载上次连接的IP和端口
                LoadLastSocketConfig();

                int maxRetryCount = 5;
                int retryCount = 0;
                int baseWaitTime = 2000;

                while (!_socketClient.IsConnected && retryCount < maxRetryCount && !cancellationToken.IsCancellationRequested && !this.IsDisposed)
                {
                    try
                    {
                        retryCount++;

                        string ip = txtServerIP.Text.Trim();
                        if (!int.TryParse(txtServerPort.Text, out int port) || port <= 0 || port > 65535)
                        {
                            AddLog("Socket", $"自动连接失败: 无效的端口号 {txtServerPort.Text}");
                            await Task.Delay(baseWaitTime, cancellationToken);
                            continue;
                        }

                        AddLog("Socket", $"自动连接尝试 {retryCount}/{maxRetryCount} 到 {ip}:{port}...");
                        await Task.Delay(baseWaitTime, cancellationToken);

                        this.Invoke(new Action(() => { btnSocketConnect.Enabled = false; }));

                        await _socketClient.StartAsync(ip, port);

                        if (_socketClient.IsConnected)
                        {
                            AddLog("Socket", $"自动连接成功到 {ip}:{port}");
                            SaveSocketConfig();
                            break;
                        }
                    }
                    catch (OperationCanceledException)
                    {
                        AddLog("Socket", "自动连接已取消");
                        break;
                    }
                    catch (Exception ex)
                    {
                        AddLog("Socket", $"自动连接失败: {ex.Message}");

                        if (retryCount < maxRetryCount && !cancellationToken.IsCancellationRequested)
                        {
                            int waitTime = Math.Min(baseWaitTime * retryCount, 10000);
                            await Task.Delay(waitTime, cancellationToken);
                        }
                    }
                    finally
                    {
                        if (!_socketClient.IsConnected && !cancellationToken.IsCancellationRequested)
                        {
                            this.Invoke(new Action(() => { btnSocketConnect.Enabled = true; }));
                        }
                    }
                }

                if (!_socketClient.IsConnected && retryCount >= maxRetryCount)
                {
                    AddLog("Socket", $"自动连接已达到最大重试次数 {maxRetryCount}，停止自动连接");
                }
            }
            finally
            {
                _isAutoConnecting = false;
            }
        }

        private void LoadLastSocketConfig()
        {
            try
            {
                RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\DefectDetection\Socket");
                if (key != null)
                {
                    string lastIP = key.GetValue("LastIP", "127.0.0.1") as string;
                    string lastPort = key.GetValue("LastPort", "8888") as string;

                    if (!string.IsNullOrEmpty(lastIP))
                        txtServerIP.Text = lastIP;
                    if (!string.IsNullOrEmpty(lastPort))
                        txtServerPort.Text = lastPort;

                    key.Close();
                }
            }
            catch { }
        }

        private void SaveSocketConfig()
        {
            try
            {
                RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\DefectDetection\Socket");
                if (key != null)
                {
                    key.SetValue("LastIP", txtServerIP.Text.Trim());
                    key.SetValue("LastPort", txtServerPort.Text.Trim());
                    key.Close();
                }
            }
            catch { }
        }

        private void LoadLastOpenPath()
        {
            try
            {
                RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\DefectDetection");
                if (key != null)
                {
                    lastOpenPath = key.GetValue("LastOpenPath", "") as string;
                    key.Close();
                }
            }
            catch { }
        }

        private void SaveLastOpenPath(string path)
        {
            try
            {
                RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\DefectDetection");
                if (key != null)
                {
                    key.SetValue("LastOpenPath", path);
                    key.Close();
                }
            }
            catch { }
        }

        private void InitializeCustomComponents()
        {
            try
            {
                // 初始化ROI表格
                InitializeROIGrid();

                // 初始化瑕疵表格
                InitializeDefectGrid();

                // 初始化所有图片结果表格
                InitializeAllResultsGrid();

                // 初始化日志列表
                InitializeLogView();

                // 初始化下拉框
                cmbAlignMode.Items.Clear();
                cmbAlignMode.Items.AddRange(new object[] {
                    "禁用对齐",
                    "整图对齐",
                    "ROI区域对齐"
                });
                cmbAlignMode.SelectedIndex = 2; // 默认ROI区域对齐

                // 设置数值控件的默认值（与demo_cpp一致）
                InitializeNumericControls();

                // 设置复选框默认值
                chkBinaryOpt.Checked = true;
                chkAutoAlign.Checked = true;

                // 设置PictureBox属性
                InitializePictureBoxes();

                // 初始化状态标签
                InitializeStatusLabels();

                // 初始化按钮状态
                InitializeButtonStates();

                // 添加默认ROI
                AddDefaultROI();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"初始化组件失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void InitializeROIGrid()
        {
            // 清空现有列
            dgvROI.Columns.Clear();

            // 添加列
            dgvROI.Columns.Add("Index", "序号");
            dgvROI.Columns.Add("ApiId", "API ID");
            dgvROI.Columns.Add("X", "X");
            dgvROI.Columns.Add("Y", "Y");
            dgvROI.Columns.Add("Width", "宽度");
            dgvROI.Columns.Add("Height", "高度");
            dgvROI.Columns.Add("Threshold", "阈值");

            // 设置列属性
            dgvROI.Columns["Index"].ReadOnly = true;
            dgvROI.Columns["ApiId"].ReadOnly = true;
            dgvROI.Columns["X"].ReadOnly = false;
            dgvROI.Columns["Y"].ReadOnly = false;
            dgvROI.Columns["Width"].ReadOnly = false;
            dgvROI.Columns["Height"].ReadOnly = false;
            dgvROI.Columns["Threshold"].ReadOnly = false;

            // 设置列宽
            dgvROI.Columns["Index"].Width = 50;
            dgvROI.Columns["ApiId"].Width = 60;
            dgvROI.Columns["X"].Width = 50;
            dgvROI.Columns["Y"].Width = 50;
            dgvROI.Columns["Width"].Width = 60;
            dgvROI.Columns["Height"].Width = 60;
            dgvROI.Columns["Threshold"].Width = 80;

            // 设置列对齐方式
            dgvROI.Columns["Index"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            dgvROI.Columns["ApiId"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;
            dgvROI.Columns["X"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvROI.Columns["Y"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvROI.Columns["Width"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvROI.Columns["Height"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvROI.Columns["Threshold"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;

            // 设置列格式
            dgvROI.Columns["Threshold"].DefaultCellStyle.Format = "F2";

            // 设置表格属性
            dgvROI.AllowUserToAddRows = false;
            dgvROI.AllowUserToDeleteRows = true;
            dgvROI.RowHeadersVisible = false;
            dgvROI.EditMode = DataGridViewEditMode.EditOnEnter;
            dgvROI.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
            dgvROI.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            dgvROI.MultiSelect = false;
            dgvROI.VirtualMode = false;
            dgvROI.DataSource = null;

            // 添加事件
            dgvROI.CellEndEdit += DgvROI_CellEndEdit;
            dgvROI.UserDeletingRow += DgvROI_UserDeletingRow;
        }

        private void InitializeDefectGrid()
        {
            dgvDefects.Columns.Clear();
            dgvDefects.Columns.Add("ROI序号", "ROI序号");
            dgvDefects.Columns.Add("X", "X(绝对)");
            dgvDefects.Columns.Add("Y", "Y(绝对)");
            dgvDefects.Columns.Add("Width", "宽度");
            dgvDefects.Columns.Add("Height", "高度");
            dgvDefects.Columns.Add("Area", "面积");
            dgvDefects.Columns.Add("Type", "类型");
            dgvDefects.Columns.Add("Severity", "严重度");

            // 设置列宽
            dgvDefects.Columns["ROI序号"].Width = 70;
            dgvDefects.Columns["X"].Width = 80;
            dgvDefects.Columns["Y"].Width = 80;
            dgvDefects.Columns["Width"].Width = 70;
            dgvDefects.Columns["Height"].Width = 70;
            dgvDefects.Columns["Area"].Width = 70;
            dgvDefects.Columns["Type"].Width = 100;
            dgvDefects.Columns["Severity"].Width = 80;

            dgvDefects.ReadOnly = true;
            dgvDefects.RowHeadersVisible = false;
            dgvDefects.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
            dgvDefects.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            dgvDefects.MultiSelect = false;

            dgvDefects.CellDoubleClick += DgvDefects_CellDoubleClick;
        }

        private void InitializeAllResultsGrid()
        {
            dgvAllResults.Columns.Clear();

            dgvAllResults.Columns.Add("ImageName", "图片名称");
            dgvAllResults.Columns.Add("Similarity", "相似度(%)");
            dgvAllResults.Columns.Add("DefectCount", "瑕疵数量");
            dgvAllResults.Columns.Add("Result", "结果");

            dgvAllResults.Columns["ImageName"].ReadOnly = true;
            dgvAllResults.Columns["Similarity"].ReadOnly = true;
            dgvAllResults.Columns["DefectCount"].ReadOnly = true;
            dgvAllResults.Columns["Result"].ReadOnly = true;

            dgvAllResults.Columns["ImageName"].Width = 180;
            dgvAllResults.Columns["Similarity"].Width = 80;
            dgvAllResults.Columns["DefectCount"].Width = 80;
            dgvAllResults.Columns["Result"].Width = 60;

            dgvAllResults.Columns["Similarity"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvAllResults.Columns["DefectCount"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleRight;
            dgvAllResults.Columns["Result"].DefaultCellStyle.Alignment = DataGridViewContentAlignment.MiddleCenter;

            dgvAllResults.Columns["Similarity"].DefaultCellStyle.Format = "F1";
            dgvAllResults.Columns["DefectCount"].DefaultCellStyle.Format = "F0";

            dgvAllResults.AllowUserToAddRows = false;
            dgvAllResults.AllowUserToDeleteRows = false;
            dgvAllResults.ReadOnly = true;
            dgvAllResults.RowHeadersVisible = false;
            dgvAllResults.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            dgvAllResults.MultiSelect = false;
            dgvAllResults.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;

            dgvAllResults.CellDoubleClick += DgvAllResults_CellDoubleClick;
        }

        private void InitializeLogView()
        {
            listViewLog.View = View.Details;
            listViewLog.FullRowSelect = true;
            listViewLog.GridLines = true;
            listViewLog.Columns.Clear();
            listViewLog.Columns.Add("时间", 100);
            listViewLog.Columns.Add("类型", 80);
            listViewLog.Columns.Add("信息", 1000);
        }

        private void InitializeNumericControls()
        {
            nudBinaryThreshold.Minimum = 0;
            nudBinaryThreshold.Maximum = 255;
            nudBinaryThreshold.Value = 30;
            nudBinaryThreshold.Increment = 1;

            nudEdgeTol.Minimum = 0;
            nudEdgeTol.Maximum = 20;
            nudEdgeTol.Value = 2;
            nudEdgeTol.Increment = 1;

            nudNoiseSize.Minimum = 0;
            nudNoiseSize.Maximum = 50;
            nudNoiseSize.Value = 3;
            nudNoiseSize.Increment = 1;

            nudMinArea.Minimum = 1;
            nudMinArea.Maximum = 500;
            nudMinArea.Value = 20;
            nudMinArea.Increment = 1;

            nudSimThresh.Minimum = 0;
            nudSimThresh.Maximum = 100;
            nudSimThresh.Value = 90;
            nudSimThresh.DecimalPlaces = 1;
            nudSimThresh.Increment = 1;
        }

        private void InitializePictureBoxes()
        {
            pictureBoxTemplate.SizeMode = PictureBoxSizeMode.Zoom;
            pictureBoxTest.SizeMode = PictureBoxSizeMode.Zoom;
            pictureBoxTemplate.BackColor = Color.White;
            pictureBoxTest.BackColor = Color.White;
            pictureBoxTemplate.BorderStyle = BorderStyle.FixedSingle;
            pictureBoxTest.BorderStyle = BorderStyle.FixedSingle;
        }

        private void InitializeStatusLabels()
        {
            lblPassCount.Text = "通过: 0";
            lblFailCount.Text = "失败: 0";
            lblTotalCount.Text = "总计: 0";
            lblCurrentImage.Text = "当前: 未选择";

            progressBar.Minimum = 0;
            progressBar.Maximum = 100;
            progressBar.Value = 0;
        }

        private void InitializeButtonStates()
        {
            btnPrev.Enabled = false;
            btnNext.Enabled = false;
            btnSingleCompare.Enabled = false;
            btnContinuousCompare.Enabled = false;
            btnStop.Enabled = false;
            btnSocketDisconnect.Enabled = false;
        }

        private void AddDefaultROI()
        {
            try
            {
                var roi = new ROIItem
                {
                    UiIndex = 1,
                    X = 200,
                    Y = 420,
                    Width = 820,
                    Height = 320,
                    Threshold = 0.8f
                };

                roiList.Add(roi);

                if (detector != null && detector.IsInitialized)
                {
                    int x = roi.X + SHRINK_PIXELS;
                    int y = roi.Y + SHRINK_PIXELS;
                    int width = roi.Width - 2 * SHRINK_PIXELS;
                    int height = roi.Height - 2 * SHRINK_PIXELS;

                    if (width <= 0 || height <= 0)
                    {
                        x = roi.X;
                        y = roi.Y;
                        width = roi.Width;
                        height = roi.Height;
                    }

                    int apiId = detector.AddROI(x, y, width, height, roi.Threshold);
                    roi.ApiId = apiId;
                    uiIndexToApiId[roi.UiIndex] = apiId;
                    apiIdToUiIndex[apiId] = roi.UiIndex;

                    if (detector.IsTemplateLoaded())
                    {
                        detector.ExtractROITemplates();
                    }
                }

                UpdateROIGrid();
                AddLog("ROI", $"添加默认ROI: ({roi.X},{roi.Y}) {roi.Width}x{roi.Height}");
            }
            catch (Exception ex)
            {
                AddLog("错误", $"添加默认ROI失败: {ex.Message}");
            }
        }

        private void SetupEventHandlers()
        {
            this.btnSelectTemplate.Click += BtnSelectTemplate_Click;
            this.btnSelectFolder.Click += BtnSelectFolder_Click;
            this.btnBrowseCSV.Click += BtnBrowseCSV_Click;
            this.btnPrev.Click += BtnPrev_Click;
            this.btnNext.Click += BtnNext_Click;
            this.btnSingleCompare.Click += BtnSingleCompare_Click;
            this.btnContinuousCompare.Click += BtnContinuousCompare_Click;
            this.btnStop.Click += BtnStop_Click;
            this.btnAddROI.Click += BtnAddROI_Click;
            this.btnRemoveROI.Click += BtnRemoveROI_Click;
            this.btnClearROI.Click += BtnClearROI_Click;
            this.btnClearAll.Click += BtnClearAll_Click;
            this.btnSocketConnect.Click += BtnSocketConnect_Click;
            this.btnSocketDisconnect.Click += BtnSocketDisconnect_Click;

            this.chkAutoAlign.CheckedChanged += ChkAutoAlign_CheckedChanged;
            this.cmbAlignMode.SelectedIndexChanged += CmbAlignMode_SelectedIndexChanged;
            this.chkBinaryOpt.CheckedChanged += ChkBinaryOpt_CheckedChanged;
            this.chkAutoReconnect.CheckedChanged += ChkAutoReconnect_CheckedChanged;

            this.nudBinaryThreshold.ValueChanged += Parameter_ValueChanged;
            this.nudEdgeTol.ValueChanged += Parameter_ValueChanged;
            this.nudNoiseSize.ValueChanged += Parameter_ValueChanged;
            this.nudMinArea.ValueChanged += Parameter_ValueChanged;
            this.nudSimThresh.ValueChanged += Parameter_ValueChanged;

            this.pictureBoxTest.MouseDown += PictureBoxTest_MouseDown;
            this.pictureBoxTest.MouseMove += PictureBoxTest_MouseMove;
            this.pictureBoxTest.MouseUp += PictureBoxTest_MouseUp;
            this.pictureBoxTest.Paint += PictureBoxTest_Paint;
            this.pictureBoxTest.Resize += (s, e) => pictureBoxTest.Invalidate();

            this.Resize += Form1_Resize;
        }

        #endregion

        #region Socket事件处理方法

        private void SocketClient_ConnectionStatusChanged(object sender, bool connected)
        {
            if (this.InvokeRequired)
            {
                this.Invoke(new Action(() => SocketClient_ConnectionStatusChanged(sender, connected)));
                return;
            }

            try
            {
                btnSocketConnect.Enabled = !connected;
                btnSocketDisconnect.Enabled = connected;
                lblSocketStatus.Text = connected ? "已连接" : "未连接";
                lblSocketStatus.ForeColor = connected ? Color.Green : Color.Red;

                if (connected)
                {
                    AddLog("Socket", $"已连接到服务器 {_socketClient.ServerIp}:{_socketClient.ServerPort}");
                }
                else
                {
                    AddLog("Socket", "与服务器的连接已断开");
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"更新连接状态失败: {ex.Message}");
            }
        }

        private void SocketClient_LogMessage(object sender, string message)
        {
            AddLog("Socket", message);
        }

        private async void SocketClient_MessageReceived(object sender, byte[] message)
        {
            if (message == null || message.Length < SocketProtocol.HEADER_SIZE)
                return;

            if (!SocketProtocol.DecodeHeader(message, out byte command, out int dataLength))
                return;

            byte[] data = new byte[dataLength];
            if (dataLength > 0)
            {
                Array.Copy(message, SocketProtocol.HEADER_SIZE, data, 0, dataLength);
            }

            try
            {
                switch (command)
                {
                    case SocketProtocol.CMD_SET_TEMPLATE:
                        await HandleSetTemplate(data);
                        break;
                    case SocketProtocol.CMD_COMPARE_IMAGE:
                        await HandleCompareImage(data);
                        break;
                    case SocketProtocol.CMD_CONFIGURE_ROI:
                        await HandleConfigureROI(data);
                        break;
                    case SocketProtocol.CMD_CONFIGURE_PARAM:
                        await HandleConfigureParam(data);
                        break;
                    case SocketProtocol.CMD_PING:
                        await HandlePing();
                        break;
                    default:
                        AddLog("Socket", $"未知命令: 0x{command:X2}");
                        break;
                }
            }
            catch (Exception ex)
            {
                AddLog("Socket错误", $"处理消息失败: {ex.Message}");
                await _socketClient.SendStatusAsync($"error: {ex.Message}");
            }
        }

        private async Task HandleSetTemplate(byte[] imageData)
        {
            AddLog("Socket", "收到模板图片，开始加载...");

            if (imageData == null || imageData.Length == 0)
            {
                await _socketClient.SendStatusAsync("error: 图片数据为空");
                return;
            }

            try
            {
                string tempFile = Path.GetTempFileName() + ".png";
                File.WriteAllBytes(tempFile, imageData);

                await Task.Run(() =>
                {
                    this.Invoke(new Action(() =>
                    {
                        detector.ClearROIs();
                        roiList.Clear();
                        uiIndexToApiId.Clear();
                        apiIdToUiIndex.Clear();

                        if (!detector.ImportTemplateFromFile(tempFile))
                        {
                            throw new Exception("无法导入模板文件");
                        }

                        if (pictureBoxTemplate.Image != null)
                        {
                            pictureBoxTemplate.Image.Dispose();
                            pictureBoxTemplate.Image = null;
                        }

                        var img = Image.FromFile(tempFile);
                        pictureBoxTemplate.Image = new Bitmap(img);
                        img.Dispose();

                        templateFile = tempFile;
                        AddLog("Socket", $"模板加载成功: {Path.GetFileName(tempFile)}");

                        int width, height, channels;
                        detector.GetTemplateSize(out width, out height, out channels);
                        AddLog("Socket", $"模板尺寸: {width}x{height}, 通道数:{channels}");

                        if (roiList.Count > 0)
                        {
                            ReapplyROIsToDetector();
                        }

                        pictureBoxTemplate.Invalidate();
                        pictureBoxTest.Invalidate();
                        UpdateROIGrid();

                        AddLog("Socket", "模板加载完成，UI已更新");
                    }));
                });

                await _socketClient.SendStatusAsync("template loaded");
            }
            catch (Exception ex)
            {
                AddLog("Socket错误", $"加载模板失败: {ex.Message}");
                await _socketClient.SendStatusAsync($"error: {ex.Message}");
            }
        }

        private async Task HandleCompareImage(byte[] imageData)
        {
            if (_isProcessingImage)
            {
                await _socketClient.SendStatusAsync("busy");
                return;
            }

            if (string.IsNullOrEmpty(templateFile))
            {
                await _socketClient.SendStatusAsync("error: 未加载模板");
                return;
            }

            _isProcessingImage = true;

            try
            {
                AddLog("Socket", "收到比对图片，开始检测...");

                string tempFile = Path.GetTempFileName() + ".png";
                File.WriteAllBytes(tempFile, imageData);

                string displayName = $"Socket_{DateTime.Now:yyyyMMddHHmmssfff}_{socketImageCounter + 1}.png";

                this.Invoke(new Action(() =>
                {
                    socketImageCounter++;
                    testFiles.Add(tempFile);
                    lblTotalCount.Text = $"总计: {testFiles.Count}";
                    AddLog("Socket", $"图片已加入测试列表: {displayName}");
                }));

                DefectDetectorAPI.DetectionResultDetailed detailedResult = default;
                DefectDetectorAPI.ROIResult[] roiResults = null;
                DefectDetectorAPI.DefectInfo[] defects = null;

                await Task.Run(() =>
                {
                    this.Invoke(new Action(UpdateBinaryParams));
                    detailedResult = detector.DetectFromFileEx(tempFile);
                    roiResults = detector.GetLastROIResults();
                    defects = detailedResult.Defects;
                });

                int significantDefectCount = defects?.Count(d => d.Area >= DEFECT_SIZE_THRESHOLD) ?? 0;
                bool passed = significantDefectCount == 0;
                int defectCount = significantDefectCount;

                string details = $"相似度:{GetMinSimilarity(roiResults) * 100:F1}%, " +
                                $"耗时:{detailedResult.ProcessingTime:F1}ms";

                var localization = detector.GetLastLocalizationInfo();
                if (localization.Success != 0)
                {
                    details += $", 偏移({localization.OffsetX:F1},{localization.OffsetY:F1})";
                }

                await _socketClient.SendResultAsync(passed, defectCount, details);

                this.Invoke(new Action(() =>
                {
                    bool hadPreviousResult = imageResults.ContainsKey(tempFile);
                    bool wasPreviousPassed = false;

                    if (hadPreviousResult)
                    {
                        wasPreviousPassed = imageResults[tempFile].Passed;
                    }

                    var tempDetailedResult = new DefectDetectorAPI.DetectionResultDetailed
                    {
                        ResultCode = detailedResult.ResultCode,
                        Defects = defects ?? new DefectDetectorAPI.DefectInfo[0],
                        ActualDefectCount = defects?.Length ?? 0,
                        ProcessingTime = detailedResult.ProcessingTime,
                        LocalizationTime = detailedResult.LocalizationTime,
                        ROIComparisonTime = detailedResult.ROIComparisonTime
                    };

                    ProcessResult(tempFile, detailedResult, roiResults, hadPreviousResult, wasPreviousPassed, displayName);

                    if (testFiles.Count == 1 || currentIndex == -1)
                    {
                        currentIndex = testFiles.Count - 1;
                        LoadTestImage(currentIndex);
                        LoadCurrentImageResults();
                    }

                    UpdateStats();
                    UpdateAllResultsGrid();
                    pictureBoxTest.Invalidate();
                    dgvDefects.Refresh();

                    AddLog("Socket", $"UI更新完成，当前共 {testFiles.Count} 张图片");
                }));

                AddLog("Socket", $"检测完成: {(passed ? "通过" : "失败")}, 瑕疵数:{defectCount}");
            }
            catch (Exception ex)
            {
                AddLog("Socket错误", $"检测失败: {ex.Message}");
                await _socketClient.SendStatusAsync($"error: {ex.Message}");
            }
            finally
            {
                _isProcessingImage = false;
            }
        }

        private async Task HandleConfigureROI(byte[] data)
        {
            string config = SocketProtocol.ExtractText(data);
            AddLog("Socket", $"收到ROI配置: {config}");

            if (SocketProtocol.ParseROIConfig(config, out ROIConfigItem[] rois))
            {
                await Task.Run(() =>
                {
                    this.Invoke(new Action(() =>
                    {
                        detector.ClearROIs();
                        roiList.Clear();
                        uiIndexToApiId.Clear();
                        apiIdToUiIndex.Clear();

                        AddLog("Socket", $"已清除现有ROI");

                        for (int i = 0; i < rois.Length; i++)
                        {
                            var roi = rois[i];

                            if (roi.Width <= 0 || roi.Height <= 0)
                            {
                                AddLog("Socket警告", $"ROI {roi.Id} 尺寸无效，跳过");
                                continue;
                            }

                            int x = roi.X + SHRINK_PIXELS;
                            int y = roi.Y + SHRINK_PIXELS;
                            int width = roi.Width - 2 * SHRINK_PIXELS;
                            int height = roi.Height - 2 * SHRINK_PIXELS;

                            if (width <= 0 || height <= 0)
                            {
                                x = roi.X;
                                y = roi.Y;
                                width = roi.Width;
                                height = roi.Height;
                            }

                            int apiId = detector.AddROI(x, y, width, height, roi.Threshold);

                            var newRoi = new ROIItem
                            {
                                UiIndex = i + 1,
                                ApiId = apiId,
                                X = roi.X,
                                Y = roi.Y,
                                Width = roi.Width,
                                Height = roi.Height,
                                Threshold = roi.Threshold
                            };

                            roiList.Add(newRoi);
                            uiIndexToApiId[i + 1] = apiId;
                            apiIdToUiIndex[apiId] = i + 1;

                            AddLog("Socket", $"添加ROI {newRoi.UiIndex}: ({x},{y}) {width}x{height} (原始: {roi.X},{roi.Y} {roi.Width}x{roi.Height}, 内缩{SHRINK_PIXELS}px) 阈值:{newRoi.Threshold}");
                        }

                        if (detector.IsTemplateLoaded())
                        {
                            detector.ExtractROITemplates();
                            AddLog("Socket", $"ROI配置完成，共 {roiList.Count} 个ROI（内缩{SHRINK_PIXELS}px），已提取ROI模板，UI已更新");
                        }
                        else
                        {
                            AddLog("Socket", $"ROI配置完成，共 {roiList.Count} 个ROI（内缩{SHRINK_PIXELS}px），模板未加载跳过提取，UI已更新");
                        }

                        UpdateROIGrid();
                        pictureBoxTest.Invalidate();
                        pictureBoxTemplate.Invalidate();
                        dgvROI.Refresh();
                    }));
                });

                await _socketClient.SendStatusAsync($"roi configured: {roiList.Count} items");
            }
            else
            {
                AddLog("Socket错误", "无效的ROI配置格式");
                await _socketClient.SendStatusAsync("error: 无效的ROI配置");
            }
        }

        private async Task HandleConfigureParam(byte[] data)
        {
            string config = SocketProtocol.ExtractText(data);
            AddLog("Socket", $"收到参数配置: {config}");

            if (SocketProtocol.ParseParamConfig(config, out var parameters))
            {
                await Task.Run(() =>
                {
                    this.Invoke(new Action(() =>
                    {
                        int paramCount = 0;

                        foreach (var kv in parameters)
                        {
                            switch (kv.Key.ToLower())
                            {
                                case "binary_threshold":
                                    if (int.TryParse(kv.Value, out int bt))
                                    {
                                        nudBinaryThreshold.Value = Math.Max(0, Math.Min(255, bt));
                                        paramCount++;
                                    }
                                    break;
                                case "edge_tol":
                                    if (int.TryParse(kv.Value, out int et))
                                    {
                                        nudEdgeTol.Value = Math.Max(0, Math.Min(20, et));
                                        paramCount++;
                                    }
                                    break;
                                case "noise_size":
                                    if (int.TryParse(kv.Value, out int ns))
                                    {
                                        nudNoiseSize.Value = Math.Max(0, Math.Min(50, ns));
                                        paramCount++;
                                    }
                                    break;
                                case "min_area":
                                    if (int.TryParse(kv.Value, out int ma))
                                    {
                                        nudMinArea.Value = Math.Max(1, Math.Min(500, ma));
                                        paramCount++;
                                    }
                                    break;
                                case "sim_thresh":
                                    if (float.TryParse(kv.Value, out float st))
                                    {
                                        nudSimThresh.Value = Math.Max(0, Math.Min(100, (decimal)st));
                                        paramCount++;
                                    }
                                    break;
                                case "auto_align":
                                    chkAutoAlign.Checked = kv.Value.ToLower() == "true" || kv.Value == "1";
                                    paramCount++;
                                    break;
                                case "align_mode":
                                    if (int.TryParse(kv.Value, out int am) && am >= 0 && am <= 2)
                                    {
                                        cmbAlignMode.SelectedIndex = am;
                                        paramCount++;
                                    }
                                    break;
                                case "binary_opt":
                                    chkBinaryOpt.Checked = kv.Value.ToLower() == "true" || kv.Value == "1";
                                    paramCount++;
                                    break;
                            }
                        }

                        if (paramCount > 0)
                        {
                            UpdateBinaryParams();
                            dgvROI.Refresh();
                            pictureBoxTest.Invalidate();
                            AddLog("Socket", $"已更新 {paramCount} 个参数");
                        }
                    }));
                });

                await _socketClient.SendStatusAsync("parameters configured");
            }
            else
            {
                await _socketClient.SendStatusAsync("error: 无效的参数配置");
            }
        }

        private async Task HandlePing()
        {
            await _socketClient.SendStatusAsync("pong");
        }

        #endregion

        #region 按钮事件处理方法

        private void BtnSelectTemplate_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog dlg = new OpenFileDialog())
            {
                dlg.Filter = "图像文件|*.bmp;*.jpg;*.jpeg;*.png|所有文件|*.*";
                dlg.Title = "选择模板图像";
                dlg.InitialDirectory = string.IsNullOrEmpty(lastOpenPath) ?
                    Environment.GetFolderPath(Environment.SpecialFolder.MyPictures) : lastOpenPath;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    templateFile = dlg.FileName;
                    lastOpenPath = Path.GetDirectoryName(templateFile);
                    SaveLastOpenPath(lastOpenPath);

                    try
                    {
                        if (!detector.ImportTemplateFromFile(templateFile))
                        {
                            MessageBox.Show("无法导入模板文件", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return;
                        }

                        if (pictureBoxTemplate.Image != null)
                        {
                            pictureBoxTemplate.Image.Dispose();
                        }

                        var img = Image.FromFile(templateFile);
                        pictureBoxTemplate.Image = new Bitmap(img);
                        img.Dispose();

                        AddLog("系统", $"模板加载成功: {Path.GetFileName(templateFile)}");

                        int width, height, channels;
                        detector.GetTemplateSize(out width, out height, out channels);
                        AddLog("系统", $"模板尺寸: {width}x{height}, 通道数:{channels}");

                        ReapplyROIsToDetector();

                        string templateFilename = Path.GetFileName(templateFile);
                        if (locationDict.ContainsKey(templateFilename))
                        {
                            templateLocation = locationDict[templateFilename];
                            AddLog("系统", $"模板定位点: {templateLocation}");
                        }

                        if (testFiles.Count > 0)
                        {
                            btnSingleCompare.Enabled = true;
                            btnContinuousCompare.Enabled = true;
                        }
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"加载模板失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        AddLog("错误", $"加载模板失败: {ex.Message}");
                    }
                }
            }
        }

        private void BtnSelectFolder_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog dlg = new OpenFileDialog())
            {
                dlg.Filter = "图像文件|*.bmp;*.jpg;*.jpeg;*.png|所有文件|*.*";
                dlg.Title = "选择测试图像文件 (可多选)";
                dlg.Multiselect = true;
                dlg.InitialDirectory = string.IsNullOrEmpty(lastOpenPath) ?
                    Environment.GetFolderPath(Environment.SpecialFolder.MyPictures) : lastOpenPath;
                dlg.RestoreDirectory = true;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    lastOpenPath = Path.GetDirectoryName(dlg.FileNames[0]);
                    SaveLastOpenPath(lastOpenPath);

                    var supportedExtensions = new[] { ".bmp", ".jpg", ".jpeg", ".png" };
                    var selectedFiles = dlg.FileNames
                        .Where(f => supportedExtensions.Contains(Path.GetExtension(f).ToLower()))
                        .OrderBy(f => f)
                        .ToList();

                    if (!string.IsNullOrEmpty(templateFile))
                    {
                        string templateName = Path.GetFileName(templateFile).ToLower();
                        selectedFiles = selectedFiles.Where(f =>
                            Path.GetFileName(f).ToLower() != templateName).ToList();
                    }

                    testFiles.AddRange(selectedFiles);

                    lblTotalCount.Text = $"总计: {testFiles.Count}";
                    AddLog("系统", $"添加了 {selectedFiles.Count} 张测试图像，总计 {testFiles.Count} 张");

                    if (testFiles.Count > 0)
                    {
                        if (currentIndex == -1)
                        {
                            currentIndex = 0;
                            LoadTestImage(currentIndex);
                            LoadCurrentImageResults();
                        }

                        btnPrev.Enabled = currentIndex > 0;
                        btnNext.Enabled = currentIndex < testFiles.Count - 1;

                        if (!string.IsNullOrEmpty(templateFile))
                        {
                            btnSingleCompare.Enabled = true;
                            btnContinuousCompare.Enabled = true;
                        }
                    }

                    UpdateAllResultsGrid();
                }
            }
        }

        private void BtnBrowseCSV_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog dlg = new OpenFileDialog())
            {
                dlg.Filter = "CSV文件|*.csv|所有文件|*.*";
                dlg.Title = "选择定位数据CSV文件";
                dlg.InitialDirectory = string.IsNullOrEmpty(lastOpenPath) ?
                    Environment.GetFolderPath(Environment.SpecialFolder.MyPictures) : lastOpenPath;

                if (dlg.ShowDialog() == DialogResult.OK)
                {
                    lastOpenPath = Path.GetDirectoryName(dlg.FileName);
                    SaveLastOpenPath(lastOpenPath);
                    LoadLocationData(dlg.FileName);
                }
            }
        }

        private void BtnPrev_Click(object sender, EventArgs e)
        {
            if (testFiles.Count > 0 && currentIndex > 0)
            {
                currentIndex--;
                LoadTestImage(currentIndex);
                LoadCurrentImageResults();
                btnNext.Enabled = true;
                btnPrev.Enabled = currentIndex > 0;
            }
        }

        private void BtnNext_Click(object sender, EventArgs e)
        {
            if (testFiles.Count > 0 && currentIndex < testFiles.Count - 1)
            {
                currentIndex++;
                LoadTestImage(currentIndex);
                LoadCurrentImageResults();
                btnPrev.Enabled = true;
                btnNext.Enabled = currentIndex < testFiles.Count - 1;
            }
        }

        private async void BtnSingleCompare_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(templateFile))
            {
                MessageBox.Show("请先选择模板图像", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (testFiles.Count == 0 || currentIndex < 0)
            {
                MessageBox.Show("请先选择测试图像", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            await CompareSingleImage(testFiles[currentIndex]);
        }

        private async void BtnContinuousCompare_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(templateFile))
            {
                MessageBox.Show("请先选择模板图像", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (testFiles.Count == 0)
            {
                MessageBox.Show("请先选择测试图像", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            isComparing = true;
            cancellationTokenSource = new CancellationTokenSource();
            btnContinuousCompare.Enabled = false;
            btnStop.Enabled = true;
            btnSingleCompare.Enabled = false;
            btnPrev.Enabled = false;
            btnNext.Enabled = false;
            btnAddROI.Enabled = false;
            btnRemoveROI.Enabled = false;
            btnClearROI.Enabled = false;
            btnClearAll.Enabled = false;

            progressBar.Maximum = testFiles.Count;
            progressBar.Value = 0;

            try
            {
                await CompareAllImages(cancellationTokenSource.Token);
            }
            catch (OperationCanceledException)
            {
                AddLog("系统", "比较已取消");
            }
            finally
            {
                isComparing = false;
                btnContinuousCompare.Enabled = true;
                btnStop.Enabled = false;
                btnSingleCompare.Enabled = true;
                btnPrev.Enabled = currentIndex > 0;
                btnNext.Enabled = currentIndex < testFiles.Count - 1;
                btnAddROI.Enabled = true;
                btnRemoveROI.Enabled = true;
                btnClearROI.Enabled = true;
                btnClearAll.Enabled = true;
            }
        }

        private void BtnStop_Click(object sender, EventArgs e)
        {
            if (cancellationTokenSource != null)
            {
                cancellationTokenSource.Cancel();
            }
        }

        private void BtnAddROI_Click(object sender, EventArgs e)
        {
            if (detector == null || !detector.IsInitialized)
            {
                MessageBox.Show("请先加载模板", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                int newUiIndex = roiList.Count + 1;
                var roi = new ROIItem
                {
                    UiIndex = newUiIndex,
                    X = 100,
                    Y = 100,
                    Width = 200,
                    Height = 150,
                    Threshold = 0.8f
                };

                int x = roi.X + SHRINK_PIXELS;
                int y = roi.Y + SHRINK_PIXELS;
                int width = roi.Width - 2 * SHRINK_PIXELS;
                int height = roi.Height - 2 * SHRINK_PIXELS;

                if (width <= 0 || height <= 0)
                {
                    x = roi.X;
                    y = roi.Y;
                    width = roi.Width;
                    height = roi.Height;
                }

                int apiId = detector.AddROI(x, y, width, height, roi.Threshold);
                roi.ApiId = apiId;
                roiList.Add(roi);

                uiIndexToApiId[roi.UiIndex] = apiId;
                apiIdToUiIndex[apiId] = roi.UiIndex;

                if (detector.IsTemplateLoaded())
                {
                    detector.ExtractROITemplates();
                }

                UpdateROIGrid();
                pictureBoxTest.Invalidate();

                AddLog("ROI", $"添加ROI {roi.UiIndex} (API ID:{apiId}): ({x},{y}) {width}x{height} (原始: {roi.X},{roi.Y} {roi.Width}x{roi.Height}, 内缩{SHRINK_PIXELS}px)");
            }
            catch (Exception ex)
            {
                AddLog("错误", $"添加ROI失败: {ex.Message}");
                MessageBox.Show($"添加ROI失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnRemoveROI_Click(object sender, EventArgs e)
        {
            try
            {
                if (dgvROI.SelectedRows.Count > 0)
                {
                    int index = dgvROI.SelectedRows[0].Index;
                    if (index >= 0 && index < roiList.Count)
                    {
                        var roi = roiList[index];

                        // 确认删除
                        var result = MessageBox.Show($"确定要删除ROI {roi.UiIndex} 吗？",
                            "确认删除", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                        if (result != DialogResult.Yes)
                            return;

                        // 结束编辑状态
                        dgvROI.EndEdit();
                        dgvROI.CurrentCell = null;

                        // 从检测器中移除ROI
                        detector.RemoveROI(roi.ApiId);

                        // 清理映射表
                        uiIndexToApiId.Remove(roi.UiIndex);
                        apiIdToUiIndex.Remove(roi.ApiId);

                        // 从列表中移除
                        roiList.RemoveAt(index);

                        // 重新排序UI索引
                        ReorderROIIndices();

                        // 更新DataGridView
                        UpdateROIGrid();

                        // 强制刷新
                        dgvROI.Refresh();
                        Application.DoEvents();

                        // 刷新图片显示
                        pictureBoxTest.Invalidate();
                        pictureBoxTemplate.Invalidate();

                        // 如果模板已加载，重新提取ROI模板
                        if (detector.IsTemplateLoaded() && roiList.Count > 0)
                        {
                            detector.ExtractROITemplates();
                        }

                        AddLog("ROI", $"删除ROI {roi.UiIndex} (API ID:{roi.ApiId})，剩余 {roiList.Count} 个ROI");
                    }
                }
                else
                {
                    MessageBox.Show("请先选择要删除的ROI", "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"删除ROI失败: {ex.Message}");
                MessageBox.Show($"删除ROI失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BtnClearROI_Click(object sender, EventArgs e)
        {
            if (roiList.Count > 0)
            {
                var result = MessageBox.Show("确定要清除所有ROI吗？", "确认",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                if (result == DialogResult.Yes)
                {
                    try
                    {
                        detector.ClearROIs();
                        uiIndexToApiId.Clear();
                        apiIdToUiIndex.Clear();
                        roiList.Clear();

                        UpdateROIGrid();
                        pictureBoxTest.Invalidate();
                        pictureBoxTemplate.Invalidate();

                        AddLog("ROI", "清除所有ROI");
                    }
                    catch (Exception ex)
                    {
                        AddLog("错误", $"清除ROI失败: {ex.Message}");
                    }
                }
            }
        }

        private void BtnClearAll_Click(object sender, EventArgs e)
        {
            if (testFiles.Count > 0 || imageResults.Count > 0)
            {
                var result = MessageBox.Show("确定要清除所有图片和检测结果吗？\n这将清空图片列表、检测结果和统计数据。",
                    "确认清除", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

                if (result == DialogResult.Yes)
                {
                    ClearAllData();
                }
            }
        }

        private async void BtnSocketConnect_Click(object sender, EventArgs e)
        {
            string ip = txtServerIP.Text.Trim();
            if (!int.TryParse(txtServerPort.Text, out int port) || port <= 0 || port > 65535)
            {
                MessageBox.Show("请输入有效的端口号(1-65535)", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            btnSocketConnect.Enabled = false;
            try
            {
                _socketClient.AutoReconnect = chkAutoReconnect.Checked;
                await _socketClient.StartAsync(ip, port);

                if (_socketClient.IsConnected)
                {
                    SaveSocketConfig();
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"连接失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                btnSocketConnect.Enabled = true;
                AddLog("Socket错误", $"连接失败: {ex.Message}");
            }
        }

        private void BtnSocketDisconnect_Click(object sender, EventArgs e)
        {
            _socketClient.Stop();
        }

        #endregion

        #region DataGridView事件处理方法

        private void DgvROI_CellEndEdit(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0 && e.RowIndex < roiList.Count && e.ColumnIndex >= 2)
            {
                var row = dgvROI.Rows[e.RowIndex];
                try
                {
                    var roi = roiList[e.RowIndex];

                    int oldX = roi.X;
                    int oldY = roi.Y;
                    int oldW = roi.Width;
                    int oldH = roi.Height;
                    float oldT = roi.Threshold;

                    // 获取新值
                    if (row.Cells[2].Value != null)
                        roi.X = Convert.ToInt32(row.Cells[2].Value);
                    if (row.Cells[3].Value != null)
                        roi.Y = Convert.ToInt32(row.Cells[3].Value);
                    if (row.Cells[4].Value != null)
                        roi.Width = Convert.ToInt32(row.Cells[4].Value);
                    if (row.Cells[5].Value != null)
                        roi.Height = Convert.ToInt32(row.Cells[5].Value);
                    if (row.Cells[6].Value != null)
                        roi.Threshold = Convert.ToSingle(row.Cells[6].Value);

                    // 更新检测器中的ROI阈值
                    detector.SetROIThreshold(roi.ApiId, roi.Threshold);

                    // 更新检测器中的ROI位置（需要先移除再添加）
                    // 注意：这里可能需要重新设置ROI位置，如果API支持的话
                    // 如果不支持，可能需要重新创建ROI

                    pictureBoxTest.Invalidate();

                    AddLog("ROI", $"编辑ROI {roi.UiIndex}: ({oldX},{oldY}){oldW}x{oldH} T:{oldT} -> ({roi.X},{roi.Y}){roi.Width}x{roi.Height} T:{roi.Threshold}");
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"数据格式错误: {ex.Message}", "错误",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    UpdateROIGrid();
                }
            }
        }

        private void DgvROI_UserDeletingRow(object sender, DataGridViewRowCancelEventArgs e)
        {
            // 阻止直接通过Delete键删除行，强制使用删除按钮
            e.Cancel = true;
            MessageBox.Show("请使用'删除ROI'按钮删除选中的ROI", "提示",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void DgvDefects_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0 && currentIndex >= 0 && currentIndex < testFiles.Count)
            {
                var currentFile = testFiles[currentIndex];
                if (imageResults.ContainsKey(currentFile))
                {
                    var defects = imageResults[currentFile].Defects;
                    if (e.RowIndex < defects.Count)
                    {
                        var defect = defects[e.RowIndex];

                        Rectangle imageRect = GetImageDisplayRect(pictureBoxTest);
                        if (imageRect.Width <= 0 || imageRect.Height <= 0 || pictureBoxTest.Image == null)
                            return;

                        float scaleX = (float)imageRect.Width / pictureBoxTest.Image.Width;
                        float scaleY = (float)imageRect.Height / pictureBoxTest.Image.Height;

                        int picX = (int)(defect.X * scaleX) + imageRect.X;
                        int picY = (int)(defect.Y * scaleY) + imageRect.Y;

                        // 闪烁效果
                        FlashDefectLocation(picX, picY);

                        AddLog("定位", $"跳转到瑕疵: ROI{defect.UiIndex}(ID:{defect.RoiId}) 位置({defect.X:F1},{defect.Y:F1})");
                    }
                }
            }
        }

        private async void FlashDefectLocation(int x, int y)
        {
            try
            {
                for (int i = 0; i < 3; i++)
                {
                    pictureBoxTest.Invalidate();
                    await Task.Delay(100);

                    using (Graphics g = pictureBoxTest.CreateGraphics())
                    {
                        using (Pen flashPen = new Pen(Color.Yellow, 3))
                        {
                            g.DrawEllipse(flashPen, x - 10, y - 10, 20, 20);
                        }
                    }
                    await Task.Delay(100);
                }
                pictureBoxTest.Invalidate();
            }
            catch (Exception ex)
            {
                AddLog("错误", $"闪烁效果失败: {ex.Message}");
            }
        }

        private void DgvAllResults_CellDoubleClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0 && e.RowIndex < dgvAllResults.Rows.Count)
            {
                string filePath = dgvAllResults.Rows[e.RowIndex].Tag as string;
                if (!string.IsNullOrEmpty(filePath))
                {
                    int index = testFiles.IndexOf(filePath);
                    if (index >= 0 && index != currentIndex)
                    {
                        currentIndex = index;
                        LoadTestImage(currentIndex);
                        LoadCurrentImageResults();

                        btnPrev.Enabled = currentIndex > 0;
                        btnNext.Enabled = currentIndex < testFiles.Count - 1;

                        AddLog("系统", $"跳转到图片: {Path.GetFileName(filePath)}");
                    }
                }
            }
        }

        #endregion

        #region 控件事件处理方法

        private void ChkAutoAlign_CheckedChanged(object sender, EventArgs e)
        {
            if (detector != null)
            {
                detector.EnableAutoLocalization(chkAutoAlign.Checked);
            }
            cmbAlignMode.Enabled = chkAutoAlign.Checked;
            AddLog("配置", $"自动对齐: {(chkAutoAlign.Checked ? "启用" : "禁用")}");
        }

        private void CmbAlignMode_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (detector == null) return;

            try
            {
                switch (cmbAlignMode.SelectedIndex)
                {
                    case 0:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.None);
                        AddLog("配置", "对齐模式: 禁用");
                        break;
                    case 1:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.FullImage);
                        AddLog("配置", "对齐模式: 整图对齐");
                        break;
                    case 2:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.RoiOnly);
                        AddLog("配置", "对齐模式: ROI区域对齐");
                        break;
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"设置对齐模式失败: {ex.Message}");
            }
        }

        private void ChkBinaryOpt_CheckedChanged(object sender, EventArgs e)
        {
            UpdateBinaryParams();
            AddLog("配置", $"二值化优化: {(chkBinaryOpt.Checked ? "启用" : "禁用")}");
        }

        private void ChkAutoReconnect_CheckedChanged(object sender, EventArgs e)
        {
            if (_socketClient != null)
            {
                _socketClient.AutoReconnect = chkAutoReconnect.Checked;
            }
        }

        private void Parameter_ValueChanged(object sender, EventArgs e)
        {
            UpdateBinaryParams();
        }

        private void Form1_Resize(object sender, EventArgs e)
        {
            if (splitContainer2 != null && splitContainer2.Width > 0)
            {
                splitContainer2.SplitterDistance = splitContainer2.Width / 2;
            }
        }

        #endregion

        #region 鼠标事件

        private void PictureBoxTest_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left && pictureBoxTest.Image != null && !isComparing)
            {
                isDragging = true;
                startPoint = GetImageCoordinates(e.Location);
            }
        }

        private void PictureBoxTest_MouseMove(object sender, MouseEventArgs e)
        {
            if (isDragging)
            {
                Point endPoint = GetImageCoordinates(e.Location);
                if (endPoint != Point.Empty && startPoint != Point.Empty)
                {
                    currentROI = new Rectangle(
                        Math.Min(startPoint.X, endPoint.X),
                        Math.Min(startPoint.Y, endPoint.Y),
                        Math.Abs(endPoint.X - startPoint.X),
                        Math.Abs(endPoint.Y - startPoint.Y)
                    );

                    pictureBoxTest.Invalidate();
                }
            }
            else
            {
                Point imagePoint = GetImageCoordinates(e.Location);
                if (imagePoint != Point.Empty)
                {
                    toolStripStatusLabel.Text = $"图像坐标: ({imagePoint.X}, {imagePoint.Y})";
                }
            }
        }

        private void PictureBoxTest_MouseUp(object sender, MouseEventArgs e)
        {
            if (isDragging && currentROI.Width > 10 && currentROI.Height > 10 && detector != null && detector.IsInitialized)
            {
                try
                {
                    int newUiIndex = roiList.Count + 1;

                    int x = currentROI.X + SHRINK_PIXELS;
                    int y = currentROI.Y + SHRINK_PIXELS;
                    int width = currentROI.Width - 2 * SHRINK_PIXELS;
                    int height = currentROI.Height - 2 * SHRINK_PIXELS;

                    if (width <= 0 || height <= 0)
                    {
                        x = currentROI.X;
                        y = currentROI.Y;
                        width = currentROI.Width;
                        height = currentROI.Height;
                    }

                    int apiId = detector.AddROI(x, y, width, height, 0.8f);

                    var roi = new ROIItem
                    {
                        UiIndex = newUiIndex,
                        ApiId = apiId,
                        X = currentROI.X,
                        Y = currentROI.Y,
                        Width = currentROI.Width,
                        Height = currentROI.Height,
                        Threshold = 0.8f
                    };

                    roiList.Add(roi);
                    uiIndexToApiId[roi.UiIndex] = apiId;
                    apiIdToUiIndex[apiId] = roi.UiIndex;

                    if (detector.IsTemplateLoaded())
                    {
                        detector.ExtractROITemplates();
                    }

                    UpdateROIGrid();
                    AddLog("ROI", $"手动添加ROI {roi.UiIndex} (API ID:{apiId}): ({x},{y}) {width}x{height} (原始: {roi.X},{roi.Y} {roi.Width}x{roi.Height}, 内缩{SHRINK_PIXELS}px)");
                }
                catch (Exception ex)
                {
                    AddLog("错误", $"手动添加ROI失败: {ex.Message}");
                }
            }

            isDragging = false;
            currentROI = Rectangle.Empty;
            pictureBoxTest.Invalidate();
        }

        private void PictureBoxTest_Paint(object sender, PaintEventArgs e)
        {
            if (pictureBoxTest.Image == null) return;

            Rectangle imageRect = GetImageDisplayRect(pictureBoxTest);

            if (imageRect.Width <= 0 || imageRect.Height <= 0) return;

            var oldTransform = e.Graphics.Transform;

            try
            {
                float scaleX = (float)imageRect.Width / pictureBoxTest.Image.Width;
                float scaleY = (float)imageRect.Height / pictureBoxTest.Image.Height;

                e.Graphics.TranslateTransform(imageRect.X, imageRect.Y);
                e.Graphics.ScaleTransform(scaleX, scaleY);

                // 绘制ROI
                using (Pen roiPen = new Pen(Color.Blue, 2 / scaleX))
                {
                    foreach (var roi in roiList)
                    {
                        e.Graphics.DrawRectangle(roiPen, roi.X, roi.Y, roi.Width, roi.Height);

                        string label = $"ROI {roi.UiIndex} (ID:{roi.ApiId},阈:{roi.Threshold:F2})";
                        using (Font font = new Font("Arial", 10 / scaleX))
                        using (Brush bgBrush = new SolidBrush(Color.FromArgb(128, Color.Blue)))
                        {
                            SizeF textSize = e.Graphics.MeasureString(label, font);
                            float textX = roi.X;
                            float textY = roi.Y - textSize.Height - 2;

                            if (textY < 0) textY = roi.Y + roi.Height + 2;

                            e.Graphics.FillRectangle(bgBrush, textX, textY,
                                textSize.Width + 4, textSize.Height + 2);
                            e.Graphics.DrawString(label, font, Brushes.White, textX + 2, textY + 2);
                        }
                    }
                }

                // 绘制拖拽中的ROI
                if (isDragging && currentROI != Rectangle.Empty)
                {
                    using (Pen dashPen = new Pen(Color.Blue, 2 / scaleX)
                    { DashStyle = DashStyle.Dash })
                    {
                        e.Graphics.DrawRectangle(dashPen, currentROI.X, currentROI.Y,
                            currentROI.Width, currentROI.Height);
                    }
                }

                // 绘制当前图片的瑕疵
                if (currentIndex >= 0 && currentIndex < testFiles.Count)
                {
                    string currentFile = testFiles[currentIndex];
                    if (imageResults.ContainsKey(currentFile))
                    {
                        var result = imageResults[currentFile];
                        using (Pen defectPen = new Pen(Color.Red, 3 / scaleX))
                        {
                            foreach (var defect in result.Defects)
                            {
                                float x = defect.X - defect.Width / 2;
                                float y = defect.Y - defect.Height / 2;

                                e.Graphics.DrawRectangle(defectPen, x, y, defect.Width, defect.Height);

                                string label = $"{defect.Area:F0}px";
                                if (isComparing)
                                {
                                    label = $"R{defect.UiIndex}:{defect.Area:F0}px";
                                }

                                using (Font font = new Font("Arial", 9 / scaleX))
                                using (Brush bgBrush = new SolidBrush(Color.FromArgb(128, Color.Red)))
                                {
                                    SizeF textSize = e.Graphics.MeasureString(label, font);
                                    float textX = x;
                                    float textY = y - textSize.Height - 2;

                                    if (textY < 0) textY = y + defect.Height + 2;

                                    e.Graphics.FillRectangle(bgBrush, textX, textY,
                                        textSize.Width + 4, textSize.Height + 2);
                                    e.Graphics.DrawString(label, font, Brushes.White,
                                        textX + 2, textY + 2);
                                }
                            }
                        }
                    }
                }
            }
            finally
            {
                e.Graphics.Transform = oldTransform;
            }
        }

        #endregion

        #region 核心功能方法

        private void ReapplyROIsToDetector()
        {
            if (detector == null || !detector.IsInitialized) return;

            try
            {
                detector.ClearROIs();
                uiIndexToApiId.Clear();
                apiIdToUiIndex.Clear();

                foreach (var roi in roiList)
                {
                    int x = roi.X + SHRINK_PIXELS;
                    int y = roi.Y + SHRINK_PIXELS;
                    int width = roi.Width - 2 * SHRINK_PIXELS;
                    int height = roi.Height - 2 * SHRINK_PIXELS;

                    if (width <= 0 || height <= 0)
                    {
                        x = roi.X;
                        y = roi.Y;
                        width = roi.Width;
                        height = roi.Height;
                    }

                    int apiId = detector.AddROI(x, y, width, height, roi.Threshold);
                    roi.ApiId = apiId;
                    uiIndexToApiId[roi.UiIndex] = apiId;
                    apiIdToUiIndex[apiId] = roi.UiIndex;
                }

                if (detector.IsTemplateLoaded())
                {
                    detector.ExtractROITemplates();
                    AddLog("系统", $"重新应用 {roiList.Count} 个ROI到检测器（内缩{SHRINK_PIXELS}px），已提取ROI模板");
                }
                else
                {
                    AddLog("系统", $"重新应用 {roiList.Count} 个ROI到检测器（内缩{SHRINK_PIXELS}px），模板未加载跳过提取");
                }

                UpdateROIGrid();
            }
            catch (Exception ex)
            {
                AddLog("错误", $"重新应用ROI失败: {ex.Message}");
            }
        }

        private void ReorderROIIndices()
        {
            try
            {
                for (int i = 0; i < roiList.Count; i++)
                {
                    int oldUiIndex = roiList[i].UiIndex;
                    int newUiIndex = i + 1;

                    if (oldUiIndex != newUiIndex)
                    {
                        if (uiIndexToApiId.ContainsKey(oldUiIndex))
                        {
                            uiIndexToApiId.Remove(oldUiIndex);
                        }
                        uiIndexToApiId[newUiIndex] = roiList[i].ApiId;

                        if (apiIdToUiIndex.ContainsKey(roiList[i].ApiId))
                        {
                            apiIdToUiIndex[roiList[i].ApiId] = newUiIndex;
                        }

                        roiList[i].UiIndex = newUiIndex;
                    }
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"重新排序ROI索引失败: {ex.Message}");
            }
        }

        private void LoadLocationData(string csvPath)
        {
            locationDict.Clear();

            try
            {
                var lines = File.ReadAllLines(csvPath);
                int validCount = 0;

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

                AddLog("系统", $"加载定位数据: {lines.Length - 1} 条记录, 有效: {validCount} 条");

                if (!string.IsNullOrEmpty(templateFile))
                {
                    string templateFilename = Path.GetFileName(templateFile);
                    if (locationDict.ContainsKey(templateFilename))
                    {
                        templateLocation = locationDict[templateFilename];
                        AddLog("系统", $"模板定位点更新: {templateLocation}");
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"加载CSV文件失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                AddLog("错误", $"加载CSV文件失败: {ex.Message}");
            }
        }

        private void LoadTestImage(int index)
        {
            if (index >= 0 && index < testFiles.Count)
            {
                string file = testFiles[index];
                try
                {
                    if (pictureBoxTest.Image != null)
                    {
                        pictureBoxTest.Image.Dispose();
                    }

                    var img = Image.FromFile(file);
                    pictureBoxTest.Image = new Bitmap(img);
                    img.Dispose();

                    lblCurrentImage.Text = $"当前: {Path.GetFileName(file)} ({index + 1}/{testFiles.Count})";
                    pictureBoxTest.Invalidate();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"加载图像失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    AddLog("错误", $"加载图像失败: {ex.Message}");
                }
            }
        }

        private void LoadCurrentImageResults()
        {
            if (currentIndex >= 0 && currentIndex < testFiles.Count)
            {
                string currentFile = testFiles[currentIndex];
                if (imageResults.ContainsKey(currentFile))
                {
                    var result = imageResults[currentFile];
                    UpdateDefectGrid(result.Defects);

                    AddLog("系统", $"加载图片 {Path.GetFileName(currentFile)} 的检测结果，瑕疵数: {result.Defects.Count}");
                }
                else
                {
                    dgvDefects.Rows.Clear();
                }
            }
        }

        private void UpdateBinaryParams()
        {
            if (detector == null) return;

            try
            {
                detector.SetMatchMethod(DefectDetectorAPI.MatchMethod.Binary);
                detector.SetBinaryThreshold((int)nudBinaryThreshold.Value);

                var binaryParams = new DefectDetectorAPI.BinaryDetectionParams
                {
                    Enabled = chkBinaryOpt.Checked ? 1 : 0,
                    AutoDetectBinary = 1,
                    NoiseFilterSize = (int)nudNoiseSize.Value,
                    EdgeTolerancePixels = (int)nudEdgeTol.Value,
                    EdgeDiffIgnoreRatio = 0.05f,
                    MinSignificantArea = (int)nudMinArea.Value,
                    AreaDiffThreshold = 0.001f,
                    OverallSimilarityThreshold = (float)nudSimThresh.Value / 100f,
                    EdgeDefectSizeThreshold = 500,
                    EdgeDistanceMultiplier = 2
                };
                detector.SetBinaryDetectionParams(binaryParams);

                detector.SetParameter("min_defect_size", (int)nudMinArea.Value);
                detector.SetParameter("blur_kernel_size", 3);
                detector.SetParameter("detect_black_on_white", 1);
                detector.SetParameter("detect_white_on_black", 1);

                switch (cmbAlignMode.SelectedIndex)
                {
                    case 0:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.None);
                        break;
                    case 1:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.FullImage);
                        break;
                    case 2:
                        detector.SetAlignmentMode(DefectDetectorAPI.AlignmentMode.RoiOnly);
                        break;
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"更新参数失败: {ex.Message}");
            }
        }

        private void SetExternalTransform(string file)
        {
            if (detector == null) return;

            try
            {
                string filename = Path.GetFileName(file);

                if (locationDict.TryGetValue(filename, out LocationData testLocation) &&
                    testLocation.IsValid &&
                    templateLocation != null &&
                    templateLocation.IsValid)
                {
                    float offsetX = testLocation.X - templateLocation.X;
                    float offsetY = testLocation.Y - templateLocation.Y;
                    float rotationDiff = testLocation.Rotation - templateLocation.Rotation;

                    detector.SetExternalTransform(offsetX, offsetY, rotationDiff);

                    if (isComparing)
                    {
                        AddLog("定位", $"{filename}: 偏移({offsetX:F1},{offsetY:F1}) 旋转{rotationDiff:F2}°");
                    }
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"设置外部变换失败: {ex.Message}");
            }
        }

        private async Task CompareSingleImage(string file)
        {
            try
            {
                UpdateBinaryParams();
                SetExternalTransform(file);

                bool hadPreviousResult = imageResults.ContainsKey(file);
                bool wasPreviousPassed = false;

                if (hadPreviousResult)
                {
                    wasPreviousPassed = imageResults[file].Passed;
                    AddLog("检测", $"图片 {Path.GetFileName(file)} 已有检测结果，将覆盖更新");
                }

                var detailedResult = await Task.Run(() => detector.DetectFromFileEx(file));
                var roiResults = detector.GetLastROIResults();

                ProcessResult(file, detailedResult, roiResults, hadPreviousResult, wasPreviousPassed);

                detector.ClearExternalTransform();
                pictureBoxTest.Invalidate();

                LoadCurrentImageResults();
                UpdateStats();
                UpdateAllResultsGrid();
            }
            catch (Exception ex)
            {
                AddLog("错误", $"比较失败: {ex.Message}");
            }
        }

        private async Task CompareAllImages(CancellationToken cancellationToken)
        {
            for (int i = 0; i < testFiles.Count && !cancellationToken.IsCancellationRequested; i++)
            {
                string file = testFiles[i];

                try
                {
                    UpdateBinaryParams();
                    SetExternalTransform(file);

                    bool hadPreviousResult = imageResults.ContainsKey(file);
                    bool wasPreviousPassed = false;

                    if (hadPreviousResult)
                    {
                        wasPreviousPassed = imageResults[file].Passed;
                    }

                    var detailedResult = await Task.Run(() => detector.DetectFromFileEx(file));
                    var roiResults = detector.GetLastROIResults();

                    ProcessResult(file, detailedResult, roiResults, hadPreviousResult, wasPreviousPassed);

                    detector.ClearExternalTransform();

                    progressBar.Value = i + 1;

                    if (i == currentIndex)
                    {
                        pictureBoxTest.Invalidate();
                        LoadCurrentImageResults();
                    }

                    if (i % 10 == 0 || i == testFiles.Count - 1)
                    {
                        UpdateAllResultsGrid();
                    }
                }
                catch (Exception ex)
                {
                    AddLog("错误", $"{Path.GetFileName(file)} 比较失败: {ex.Message}");
                    if (!imageResults.ContainsKey(file))
                    {
                        failCount++;
                    }
                }

                UpdateStats();
            }

            UpdateAllResultsGrid();
            AddLog("系统", "批量比较完成");
        }

        private void ProcessResult(string file,
            DefectDetectorAPI.DetectionResultDetailed detailedResult,
            DefectDetectorAPI.ROIResult[] roiResults,
            bool hadPreviousResult = false,
            bool wasPreviousPassed = false,
            string customDisplayName = null)
        {
            try
            {
                int passedRois = roiResults.Count(r => r.Passed != 0);
                float minSimilarity = roiResults.Length > 0 ? roiResults.Min(r => r.Similarity) : 0;

                var allDefects = detailedResult?.Defects ?? new DefectDetectorAPI.DefectInfo[0];

                var convertedDefects = new List<UIDefectInfo>();

                foreach (var defect in allDefects.Where(d => d.Area >= DEFECT_SIZE_THRESHOLD))
                {
                    if (apiIdToUiIndex.TryGetValue(defect.RoiId, out int uiIndex))
                    {
                        var roi = roiList.FirstOrDefault(r => r.UiIndex == uiIndex);

                        if (roi != null)
                        {
                            float absX = roi.X + defect.X;
                            float absY = roi.Y + defect.Y;

                            convertedDefects.Add(new UIDefectInfo
                            {
                                RoiId = defect.RoiId,
                                UiIndex = uiIndex,
                                X = absX,
                                Y = absY,
                                Width = defect.Width,
                                Height = defect.Height,
                                Area = defect.Area,
                                Type = defect.Type.ToString(),
                                Severity = defect.Severity,
                                RelX = defect.X,
                                RelY = defect.Y
                            });

                            if (isComparing)
                            {
                                AddLog("坐标转换", $"ROI{uiIndex}(ID:{defect.RoiId}): ROI位置({roi.X},{roi.Y}), " +
                                       $"瑕疵相对({defect.X:F1},{defect.Y:F1}) -> 绝对({absX:F1},{absY:F1})");
                            }
                        }
                        else
                        {
                            AddLog("警告", $"找不到UI索引 {uiIndex} 对应的ROI信息");
                        }
                    }
                    else
                    {
                        AddLog("警告", $"找不到API ID {defect.RoiId} 对应的UI索引");
                    }
                }

                bool hasSignificantDefects = convertedDefects.Any();
                bool finalPass = !hasSignificantDefects;

                if (hadPreviousResult)
                {
                    if (wasPreviousPassed)
                        passCount--;
                    else
                        failCount--;

                    AddLog("检测", $"调整计数: 移除之前的结果 ({(wasPreviousPassed ? "通过" : "失败")})");
                }

                if (finalPass)
                    passCount++;
                else
                    failCount++;

                var localization = detector.GetLastLocalizationInfo();

                var imageResult = new ImageResult
                {
                    FileName = file,
                    DisplayName = customDisplayName,
                    Passed = finalPass,
                    PassedROIs = passedRois,
                    TotalROIs = roiResults.Length,
                    MinSimilarity = minSimilarity,
                    Defects = convertedDefects,
                    ProcessingTime = detailedResult.ProcessingTime,
                    Localization = localization,
                    ResultCode = detailedResult.ResultCode
                };

                imageResults[file] = imageResult;

                string status = finalPass ? "通过" : "失败";
                string defectInfo = convertedDefects.Any() ? $", 瑕疵:{convertedDefects.Count}" : "";
                string alignInfo = "";
                string updateInfo = hadPreviousResult ? " (更新)" : "";

                if (localization.Success != 0)
                {
                    alignInfo = $", 偏移({localization.OffsetX:F1},{localization.OffsetY:F1}) " +
                               $"旋转:{localization.RotationAngle:F2}°";
                }

                string displayFileName = customDisplayName ?? Path.GetFileName(file);
                AddLog("检测", $"{displayFileName} - {status}{updateInfo}, 相似度:{minSimilarity * 100:F1}%, " +
                       $"耗时:{detailedResult.ProcessingTime:F1}ms{defectInfo}{alignInfo}");

                if (currentIndex >= 0 && currentIndex < testFiles.Count && testFiles[currentIndex] == file)
                {
                    UpdateDefectGrid(convertedDefects);
                }
            }
            catch (Exception ex)
            {
                AddLog("错误", $"处理结果失败: {ex.Message}");
            }
        }

        private float GetMinSimilarity(ROIResult[] results)
        {
            if (results == null || results.Length == 0) return 1.0f;
            return results.Min(r => r.Similarity);
        }

        private void ClearAllData()
        {
            try
            {
                testFiles.Clear();
                imageResults.Clear();

                passCount = 0;
                failCount = 0;
                socketImageCounter = 0;

                currentIndex = -1;

                if (pictureBoxTest.Image != null)
                {
                    pictureBoxTest.Image.Dispose();
                    pictureBoxTest.Image = null;
                }

                dgvDefects.Rows.Clear();

                UpdateAllResultsGrid();
                UpdateStats();

                lblTotalCount.Text = "总计: 0";
                lblCurrentImage.Text = "当前: 未选择";

                btnPrev.Enabled = false;
                btnNext.Enabled = false;
                btnSingleCompare.Enabled = !string.IsNullOrEmpty(templateFile) && testFiles.Count > 0;
                btnContinuousCompare.Enabled = !string.IsNullOrEmpty(templateFile) && testFiles.Count > 0;

                pictureBoxTest.Invalidate();

                AddLog("系统", "已清除所有图片和检测结果");
            }
            catch (Exception ex)
            {
                AddLog("错误", $"清除数据失败: {ex.Message}");
            }
        }

        #endregion

        #region UI更新方法

        private void UpdateROIGrid()
        {
            if (dgvROI.InvokeRequired)
            {
                dgvROI.Invoke(new Action(UpdateROIGrid));
                return;
            }

            try
            {
                dgvROI.SuspendLayout();
                dgvROI.Rows.Clear();

                foreach (var roi in roiList)
                {
                    int rowIndex = dgvROI.Rows.Add(
                        roi.UiIndex,
                        roi.ApiId,
                        roi.X,
                        roi.Y,
                        roi.Width,
                        roi.Height,
                        roi.Threshold
                    );
                    dgvROI.Rows[rowIndex].Tag = roi;
                }

                int totalTestFiles = testFiles.Count;
                lblTotalCount.Text = $"总计: {totalTestFiles} | ROI: {roiList.Count}";

                AddLog("调试", $"更新ROI表格: 当前 {roiList.Count} 行");
            }
            catch (Exception ex)
            {
                AddLog("错误", $"更新ROI表格失败: {ex.Message}");
            }
            finally
            {
                dgvROI.ResumeLayout();
                dgvROI.Refresh();
            }
        }

        private void UpdateDefectGrid(List<UIDefectInfo> defects)
        {
            if (dgvDefects.InvokeRequired)
            {
                dgvDefects.Invoke(new Action<List<UIDefectInfo>>(UpdateDefectGrid), defects);
                return;
            }

            try
            {
                dgvDefects.SuspendLayout();
                dgvDefects.Rows.Clear();

                foreach (var defect in defects)
                {
                    dgvDefects.Rows.Add(
                        defect.UiIndex,
                        defect.X.ToString("F1"),
                        defect.Y.ToString("F1"),
                        defect.Width.ToString("F1"),
                        defect.Height.ToString("F1"),
                        defect.Area.ToString("F0"),
                        defect.Type,
                        defect.Severity.ToString("F2")
                    );
                }
            }
            finally
            {
                dgvDefects.ResumeLayout();
                dgvDefects.Refresh();
            }
        }

        private void UpdateAllResultsGrid()
        {
            if (dgvAllResults.InvokeRequired)
            {
                dgvAllResults.Invoke(new Action(UpdateAllResultsGrid));
                return;
            }

            try
            {
                dgvAllResults.SuspendLayout();
                dgvAllResults.Rows.Clear();

                var sortedResults = imageResults.OrderBy(kv => kv.Value.DisplayName ?? Path.GetFileName(kv.Key));

                foreach (var kv in sortedResults)
                {
                    var result = kv.Value;
                    string fileName = result.DisplayName ?? Path.GetFileName(kv.Key);
                    string similarity = result.MinSimilarity > 0 ? (result.MinSimilarity * 100).ToString("F1") : "-";
                    string defectCount = result.Defects.Count > 0 ? result.Defects.Count.ToString() : "0";
                    string resultText = result.Passed ? "通过" : "失败";
                    Color resultColor = result.Passed ? Color.Green : Color.Red;

                    int rowIndex = dgvAllResults.Rows.Add(fileName, similarity, defectCount, resultText);
                    dgvAllResults.Rows[rowIndex].Cells["Result"].Style.ForeColor = resultColor;
                    dgvAllResults.Rows[rowIndex].Cells["Result"].Style.Font = new Font(dgvAllResults.Font, FontStyle.Bold);

                    dgvAllResults.Rows[rowIndex].Tag = kv.Key;
                }

                lblTotalCount.Text = $"总计: {testFiles.Count} | 已检: {imageResults.Count}";
            }
            finally
            {
                dgvAllResults.ResumeLayout();
                dgvAllResults.Refresh();
            }
        }

        private void UpdateStats()
        {
            if (this.InvokeRequired)
            {
                this.Invoke(new Action(UpdateStats));
                return;
            }

            lblPassCount.Text = $"通过: {passCount}";
            lblFailCount.Text = $"失败: {failCount}";

            double passRate = testFiles.Count > 0 ? (double)passCount / testFiles.Count * 100 : 0;
            toolStripStatusLabel.Text = $"通过率: {passRate:F1}%";
        }

        private void AddLog(string type, string message)
        {
            if (listViewLog.InvokeRequired)
            {
                listViewLog.Invoke(new Action<string, string>(AddLog), type, message);
                return;
            }

            try
            {
                ListViewItem item = new ListViewItem(DateTime.Now.ToString("HH:mm:ss"));
                item.SubItems.Add(type);
                item.SubItems.Add(message);

                // 设置颜色
                if (type == "错误")
                    item.ForeColor = Color.Red;
                else if (type == "检测" && message.Contains("失败"))
                    item.ForeColor = Color.Red;
                else if (type == "检测" && message.Contains("通过"))
                    item.ForeColor = Color.Green;
                else if (type == "系统")
                    item.ForeColor = Color.Blue;
                else if (type == "坐标转换")
                    item.ForeColor = Color.Orange;
                else if (type == "警告")
                    item.ForeColor = Color.OrangeRed;
                else if (type == "ROI")
                    item.ForeColor = Color.Purple;
                else if (type == "定位")
                    item.ForeColor = Color.Teal;
                else if (type == "Socket" && message.Contains("成功"))
                    item.ForeColor = Color.Green;
                else if (type == "Socket" && message.Contains("失败"))
                    item.ForeColor = Color.Red;
                else if (type == "Socket")
                    item.ForeColor = Color.DarkCyan;
                else if (type == "调试")
                    item.ForeColor = Color.Gray;

                listViewLog.Items.Insert(0, item);

                // 限制日志数量
                while (listViewLog.Items.Count > 1000)
                {
                    listViewLog.Items.RemoveAt(listViewLog.Items.Count - 1);
                }

                toolStripStatusLabel.Text = message;
            }
            catch (Exception ex)
            {
                // 避免递归
                Console.WriteLine($"添加日志失败: {ex.Message}");
            }
        }

        private Rectangle GetImageDisplayRect(PictureBox pictureBox)
        {
            if (pictureBox.Image == null)
                return Rectangle.Empty;

            Rectangle clientRect = pictureBox.ClientRectangle;
            float imageAspect = (float)pictureBox.Image.Width / pictureBox.Image.Height;
            float clientAspect = (float)clientRect.Width / clientRect.Height;

            int displayWidth, displayHeight, displayX, displayY;

            if (imageAspect > clientAspect)
            {
                displayWidth = clientRect.Width;
                displayHeight = (int)(clientRect.Width / imageAspect);
                displayX = 0;
                displayY = (clientRect.Height - displayHeight) / 2;
            }
            else
            {
                displayHeight = clientRect.Height;
                displayWidth = (int)(clientRect.Height * imageAspect);
                displayY = 0;
                displayX = (clientRect.Width - displayWidth) / 2;
            }

            return new Rectangle(displayX, displayY, displayWidth, displayHeight);
        }

        private Point GetImageCoordinates(Point mousePoint)
        {
            if (pictureBoxTest.Image == null)
                return Point.Empty;

            Rectangle imageRect = GetImageDisplayRect(pictureBoxTest);

            if (!imageRect.Contains(mousePoint))
                return Point.Empty;

            float relativeX = mousePoint.X - imageRect.X;
            float relativeY = mousePoint.Y - imageRect.Y;

            float scaleX = (float)pictureBoxTest.Image.Width / imageRect.Width;
            float scaleY = (float)pictureBoxTest.Image.Height / imageRect.Height;

            return new Point(
                (int)(relativeX * scaleX),
                (int)(relativeY * scaleY)
            );
        }

        #endregion

        #region 辅助类

        // LocationData类已移至DefectDetectionAPI.cs，使用API中的定义

        public class ROIItem
        {
            public int UiIndex { get; set; }
            public int ApiId { get; set; }
            public int X { get; set; }
            public int Y { get; set; }
            public int Width { get; set; }
            public int Height { get; set; }
            public float Threshold { get; set; }
        }

        public class UIDefectInfo
        {
            public int RoiId { get; set; }
            public int UiIndex { get; set; }
            public float X { get; set; }
            public float Y { get; set; }
            public float Width { get; set; }
            public float Height { get; set; }
            public float Area { get; set; }
            public string Type { get; set; }
            public float Severity { get; set; }
            public float? RelX { get; set; }
            public float? RelY { get; set; }
        }

        public class ImageResult
        {
            public string FileName { get; set; }
            public string DisplayName { get; set; }
            public bool Passed { get; set; }
            public int PassedROIs { get; set; }
            public int TotalROIs { get; set; }
            public float MinSimilarity { get; set; }
            public List<UIDefectInfo> Defects { get; set; } = new List<UIDefectInfo>();
            public double ProcessingTime { get; set; }
            public DefectDetectorAPI.LocalizationInfo Localization { get; set; }
            public int ResultCode { get; set; }
        }

        #endregion

        #region 窗体关闭

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            try
            {
                _autoConnectCts?.Cancel();
                _autoConnectCts?.Dispose();

                if (isComparing)
                {
                    cancellationTokenSource?.Cancel();
                    System.Threading.Thread.Sleep(500);
                }

                _socketClient?.Dispose();

                if (pictureBoxTemplate.Image != null)
                    pictureBoxTemplate.Image.Dispose();

                if (pictureBoxTest.Image != null)
                    pictureBoxTest.Image.Dispose();

                detector?.Dispose();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"关闭窗体时出错: {ex.Message}");
            }

            base.OnFormClosing(e);
        }

        #endregion
    }
}