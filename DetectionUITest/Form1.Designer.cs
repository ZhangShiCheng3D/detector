namespace DetectionUITest
{
    partial class Form1
    {
        private System.ComponentModel.IContainer components = null;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.PictureBox pictureBoxTemplate;
        private System.Windows.Forms.PictureBox pictureBoxTest;
        private System.Windows.Forms.GroupBox groupBoxControl;
        private System.Windows.Forms.GroupBox groupBoxConfig;
        private System.Windows.Forms.GroupBox groupBoxLog;
        private System.Windows.Forms.GroupBox groupBoxDefects;
        private System.Windows.Forms.GroupBox groupBoxStats;

        // 控制按钮
        private System.Windows.Forms.Button btnSelectTemplate;
        private System.Windows.Forms.Button btnSelectFolder;
        private System.Windows.Forms.Button btnBrowseCSV;
        private System.Windows.Forms.Button btnPrev;
        private System.Windows.Forms.Button btnNext;
        private System.Windows.Forms.Button btnSingleCompare;
        private System.Windows.Forms.Button btnContinuousCompare;
        private System.Windows.Forms.Button btnStop;

        // ROI配置按钮
        private System.Windows.Forms.Button btnAddROI;
        private System.Windows.Forms.Button btnRemoveROI;
        private System.Windows.Forms.Button btnClearROI;

        // 配置控件
        private System.Windows.Forms.TabControl tabConfig;
        private System.Windows.Forms.TabPage tabPageROI;
        private System.Windows.Forms.TabPage tabPageBinary;
        private System.Windows.Forms.TabPage tabPageAlign;

        // ROI配置表格
        private System.Windows.Forms.DataGridView dgvROI;

        // 二值化参数
        private System.Windows.Forms.Label lblBinaryThreshold;
        private System.Windows.Forms.NumericUpDown nudBinaryThreshold;
        private System.Windows.Forms.Label lblEdgeTol;
        private System.Windows.Forms.NumericUpDown nudEdgeTol;
        private System.Windows.Forms.Label lblNoiseSize;
        private System.Windows.Forms.NumericUpDown nudNoiseSize;
        private System.Windows.Forms.Label lblMinArea;
        private System.Windows.Forms.NumericUpDown nudMinArea;
        private System.Windows.Forms.Label lblSimThresh;
        private System.Windows.Forms.NumericUpDown nudSimThresh;
        private System.Windows.Forms.CheckBox chkBinaryOpt;

        // 对齐配置
        private System.Windows.Forms.Label lblAlignMode;
        private System.Windows.Forms.ComboBox cmbAlignMode;
        private System.Windows.Forms.CheckBox chkAutoAlign;
        private System.Windows.Forms.Label lblCSVPath;
        private System.Windows.Forms.TextBox txtCSVPath;

        // Socket配置
        private System.Windows.Forms.GroupBox groupBoxSocket;
        private System.Windows.Forms.TextBox txtServerIP;
        private System.Windows.Forms.TextBox txtServerPort;
        private System.Windows.Forms.Button btnSocketConnect;
        private System.Windows.Forms.Button btnSocketDisconnect;
        private System.Windows.Forms.Label lblSocketStatus;
        private System.Windows.Forms.CheckBox chkAutoReconnect;

        // 数据显示
        private System.Windows.Forms.DataGridView dgvDefects;
        private System.Windows.Forms.ListView listViewLog;
        private System.Windows.Forms.Label lblPassCount;
        private System.Windows.Forms.Label lblFailCount;
        private System.Windows.Forms.Label lblTotalCount;
        private System.Windows.Forms.Label lblCurrentImage;
        private System.Windows.Forms.ProgressBar progressBar;

        // 状态栏
        private System.Windows.Forms.StatusStrip statusStrip;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusLabel;

        // 面板
        private System.Windows.Forms.Panel panel1;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.pictureBoxTemplate = new System.Windows.Forms.PictureBox();
            this.pictureBoxTest = new System.Windows.Forms.PictureBox();
            this.groupBoxLog = new System.Windows.Forms.GroupBox();
            this.listViewLog = new System.Windows.Forms.ListView();
            this.panel1 = new System.Windows.Forms.Panel();
            this.groupBoxDefects = new System.Windows.Forms.GroupBox();
            this.dgvDefects = new System.Windows.Forms.DataGridView();
            this.groupBoxStats = new System.Windows.Forms.GroupBox();
            this.lblPassCount = new System.Windows.Forms.Label();
            this.lblFailCount = new System.Windows.Forms.Label();
            this.lblTotalCount = new System.Windows.Forms.Label();
            this.lblCurrentImage = new System.Windows.Forms.Label();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.groupBoxConfig = new System.Windows.Forms.GroupBox();
            this.tabConfig = new System.Windows.Forms.TabControl();
            this.tabPageROI = new System.Windows.Forms.TabPage();
            this.dgvROI = new System.Windows.Forms.DataGridView();
            this.btnAddROI = new System.Windows.Forms.Button();
            this.btnRemoveROI = new System.Windows.Forms.Button();
            this.btnClearROI = new System.Windows.Forms.Button();
            this.tabPageBinary = new System.Windows.Forms.TabPage();
            this.chkBinaryOpt = new System.Windows.Forms.CheckBox();
            this.lblBinaryThreshold = new System.Windows.Forms.Label();
            this.nudBinaryThreshold = new System.Windows.Forms.NumericUpDown();
            this.lblEdgeTol = new System.Windows.Forms.Label();
            this.nudEdgeTol = new System.Windows.Forms.NumericUpDown();
            this.lblNoiseSize = new System.Windows.Forms.Label();
            this.nudNoiseSize = new System.Windows.Forms.NumericUpDown();
            this.lblMinArea = new System.Windows.Forms.Label();
            this.nudMinArea = new System.Windows.Forms.NumericUpDown();
            this.lblSimThresh = new System.Windows.Forms.Label();
            this.nudSimThresh = new System.Windows.Forms.NumericUpDown();
            this.tabPageAlign = new System.Windows.Forms.TabPage();
            this.chkAutoAlign = new System.Windows.Forms.CheckBox();
            this.lblAlignMode = new System.Windows.Forms.Label();
            this.cmbAlignMode = new System.Windows.Forms.ComboBox();
            this.groupBoxControl = new System.Windows.Forms.GroupBox();
            this.groupBoxSocket = new System.Windows.Forms.GroupBox();
            this.txtServerIP = new System.Windows.Forms.TextBox();
            this.txtServerPort = new System.Windows.Forms.TextBox();
            this.btnSocketConnect = new System.Windows.Forms.Button();
            this.btnSocketDisconnect = new System.Windows.Forms.Button();
            this.lblSocketStatus = new System.Windows.Forms.Label();
            this.chkAutoReconnect = new System.Windows.Forms.CheckBox();
            this.btnSelectTemplate = new System.Windows.Forms.Button();
            this.btnSelectFolder = new System.Windows.Forms.Button();
            this.btnBrowseCSV = new System.Windows.Forms.Button();
            this.txtCSVPath = new System.Windows.Forms.TextBox();
            this.btnStop = new System.Windows.Forms.Button();
            this.btnPrev = new System.Windows.Forms.Button();
            this.btnNext = new System.Windows.Forms.Button();
            this.btnSingleCompare = new System.Windows.Forms.Button();
            this.btnContinuousCompare = new System.Windows.Forms.Button();
            this.lblCSVPath = new System.Windows.Forms.Label();
            this.statusStrip = new System.Windows.Forms.StatusStrip();
            this.toolStripStatusLabel = new System.Windows.Forms.ToolStripStatusLabel();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTemplate)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTest)).BeginInit();
            this.groupBoxLog.SuspendLayout();
            this.panel1.SuspendLayout();
            this.groupBoxDefects.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dgvDefects)).BeginInit();
            this.groupBoxStats.SuspendLayout();
            this.groupBoxConfig.SuspendLayout();
            this.tabConfig.SuspendLayout();
            this.tabPageROI.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dgvROI)).BeginInit();
            this.tabPageBinary.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.nudBinaryThreshold)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudEdgeTol)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudNoiseSize)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudMinArea)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudSimThresh)).BeginInit();
            this.tabPageAlign.SuspendLayout();
            this.groupBoxControl.SuspendLayout();
            this.groupBoxSocket.SuspendLayout();
            this.statusStrip.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.splitContainer2);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.groupBoxLog);
            this.splitContainer1.Panel2.Controls.Add(this.panel1);
            this.splitContainer1.Size = new System.Drawing.Size(1130, 608);
            this.splitContainer1.SplitterDistance = 230;
            this.splitContainer1.SplitterWidth = 5;
            this.splitContainer1.TabIndex = 0;
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.splitContainer2.Name = "splitContainer2";
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.pictureBoxTemplate);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.pictureBoxTest);
            this.splitContainer2.Size = new System.Drawing.Size(1130, 230);
            this.splitContainer2.SplitterDistance = 517;
            this.splitContainer2.SplitterWidth = 5;
            this.splitContainer2.TabIndex = 0;
            // 
            // pictureBoxTemplate
            // 
            this.pictureBoxTemplate.BackColor = System.Drawing.Color.White;
            this.pictureBoxTemplate.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pictureBoxTemplate.Dock = System.Windows.Forms.DockStyle.Fill;
            this.pictureBoxTemplate.Location = new System.Drawing.Point(0, 0);
            this.pictureBoxTemplate.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.pictureBoxTemplate.Name = "pictureBoxTemplate";
            this.pictureBoxTemplate.Size = new System.Drawing.Size(517, 230);
            this.pictureBoxTemplate.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.pictureBoxTemplate.TabIndex = 0;
            this.pictureBoxTemplate.TabStop = false;
            // 
            // pictureBoxTest
            // 
            this.pictureBoxTest.BackColor = System.Drawing.Color.White;
            this.pictureBoxTest.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.pictureBoxTest.Dock = System.Windows.Forms.DockStyle.Fill;
            this.pictureBoxTest.Location = new System.Drawing.Point(0, 0);
            this.pictureBoxTest.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.pictureBoxTest.Name = "pictureBoxTest";
            this.pictureBoxTest.Size = new System.Drawing.Size(608, 230);
            this.pictureBoxTest.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.pictureBoxTest.TabIndex = 1;
            this.pictureBoxTest.TabStop = false;
            // 
            // groupBoxLog
            // 
            this.groupBoxLog.Controls.Add(this.listViewLog);
            this.groupBoxLog.Dock = System.Windows.Forms.DockStyle.Fill;
            this.groupBoxLog.Location = new System.Drawing.Point(0, 247);
            this.groupBoxLog.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxLog.Name = "groupBoxLog";
            this.groupBoxLog.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxLog.Size = new System.Drawing.Size(1130, 126);
            this.groupBoxLog.TabIndex = 4;
            this.groupBoxLog.TabStop = false;
            this.groupBoxLog.Text = "日志信息";
            // 
            // listViewLog
            // 
            this.listViewLog.Dock = System.Windows.Forms.DockStyle.Fill;
            this.listViewLog.FullRowSelect = true;
            this.listViewLog.GridLines = true;
            this.listViewLog.HideSelection = false;
            this.listViewLog.Location = new System.Drawing.Point(4, 18);
            this.listViewLog.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.listViewLog.Name = "listViewLog";
            this.listViewLog.Size = new System.Drawing.Size(1122, 104);
            this.listViewLog.TabIndex = 0;
            this.listViewLog.UseCompatibleStateImageBehavior = false;
            this.listViewLog.View = System.Windows.Forms.View.Details;
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.groupBoxDefects);
            this.panel1.Controls.Add(this.groupBoxStats);
            this.panel1.Controls.Add(this.groupBoxConfig);
            this.panel1.Controls.Add(this.groupBoxControl);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Top;
            this.panel1.Location = new System.Drawing.Point(0, 0);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(1130, 247);
            this.panel1.TabIndex = 5;
            // 
            // groupBoxDefects
            // 
            this.groupBoxDefects.Controls.Add(this.dgvDefects);
            this.groupBoxDefects.Dock = System.Windows.Forms.DockStyle.Fill;
            this.groupBoxDefects.Location = new System.Drawing.Point(726, 95);
            this.groupBoxDefects.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxDefects.Name = "groupBoxDefects";
            this.groupBoxDefects.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxDefects.Size = new System.Drawing.Size(404, 152);
            this.groupBoxDefects.TabIndex = 3;
            this.groupBoxDefects.TabStop = false;
            this.groupBoxDefects.Text = "瑕疵信息";
            // 
            // dgvDefects
            // 
            this.dgvDefects.AllowUserToAddRows = false;
            this.dgvDefects.AllowUserToDeleteRows = false;
            this.dgvDefects.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.Fill;
            this.dgvDefects.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dgvDefects.Dock = System.Windows.Forms.DockStyle.Fill;
            this.dgvDefects.Location = new System.Drawing.Point(4, 18);
            this.dgvDefects.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.dgvDefects.Name = "dgvDefects";
            this.dgvDefects.ReadOnly = true;
            this.dgvDefects.RowHeadersWidth = 62;
            this.dgvDefects.Size = new System.Drawing.Size(396, 130);
            this.dgvDefects.TabIndex = 0;
            // 
            // groupBoxStats
            // 
            this.groupBoxStats.Controls.Add(this.lblPassCount);
            this.groupBoxStats.Controls.Add(this.lblFailCount);
            this.groupBoxStats.Controls.Add(this.lblTotalCount);
            this.groupBoxStats.Controls.Add(this.lblCurrentImage);
            this.groupBoxStats.Controls.Add(this.progressBar);
            this.groupBoxStats.Dock = System.Windows.Forms.DockStyle.Top;
            this.groupBoxStats.Location = new System.Drawing.Point(726, 0);
            this.groupBoxStats.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxStats.Name = "groupBoxStats";
            this.groupBoxStats.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxStats.Size = new System.Drawing.Size(404, 95);
            this.groupBoxStats.TabIndex = 2;
            this.groupBoxStats.TabStop = false;
            this.groupBoxStats.Text = "统计信息";
            // 
            // lblPassCount
            // 
            this.lblPassCount.AutoSize = true;
            this.lblPassCount.Font = new System.Drawing.Font("宋体", 10F, System.Drawing.FontStyle.Bold);
            this.lblPassCount.ForeColor = System.Drawing.Color.Green;
            this.lblPassCount.Location = new System.Drawing.Point(13, 25);
            this.lblPassCount.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblPassCount.Name = "lblPassCount";
            this.lblPassCount.Size = new System.Drawing.Size(61, 14);
            this.lblPassCount.TabIndex = 0;
            this.lblPassCount.Text = "通过: 0";
            // 
            // lblFailCount
            // 
            this.lblFailCount.AutoSize = true;
            this.lblFailCount.Font = new System.Drawing.Font("宋体", 10F, System.Drawing.FontStyle.Bold);
            this.lblFailCount.ForeColor = System.Drawing.Color.Red;
            this.lblFailCount.Location = new System.Drawing.Point(154, 25);
            this.lblFailCount.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblFailCount.Name = "lblFailCount";
            this.lblFailCount.Size = new System.Drawing.Size(61, 14);
            this.lblFailCount.TabIndex = 1;
            this.lblFailCount.Text = "失败: 0";
            // 
            // lblTotalCount
            // 
            this.lblTotalCount.AutoSize = true;
            this.lblTotalCount.Font = new System.Drawing.Font("宋体", 10F);
            this.lblTotalCount.Location = new System.Drawing.Point(296, 25);
            this.lblTotalCount.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblTotalCount.Name = "lblTotalCount";
            this.lblTotalCount.Size = new System.Drawing.Size(56, 14);
            this.lblTotalCount.TabIndex = 2;
            this.lblTotalCount.Text = "总计: 0";
            // 
            // lblCurrentImage
            // 
            this.lblCurrentImage.AutoSize = true;
            this.lblCurrentImage.Location = new System.Drawing.Point(437, 25);
            this.lblCurrentImage.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblCurrentImage.Name = "lblCurrentImage";
            this.lblCurrentImage.Size = new System.Drawing.Size(77, 12);
            this.lblCurrentImage.TabIndex = 3;
            this.lblCurrentImage.Text = "当前: 未选择";
            // 
            // progressBar
            // 
            this.progressBar.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.progressBar.Location = new System.Drawing.Point(4, 65);
            this.progressBar.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(396, 26);
            this.progressBar.TabIndex = 4;
            // 
            // groupBoxConfig
            // 
            this.groupBoxConfig.Controls.Add(this.tabConfig);
            this.groupBoxConfig.Dock = System.Windows.Forms.DockStyle.Left;
            this.groupBoxConfig.Location = new System.Drawing.Point(383, 0);
            this.groupBoxConfig.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxConfig.Name = "groupBoxConfig";
            this.groupBoxConfig.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxConfig.Size = new System.Drawing.Size(343, 247);
            this.groupBoxConfig.TabIndex = 1;
            this.groupBoxConfig.TabStop = false;
            this.groupBoxConfig.Text = "参数配置";
            // 
            // tabConfig
            // 
            this.tabConfig.Controls.Add(this.tabPageROI);
            this.tabConfig.Controls.Add(this.tabPageBinary);
            this.tabConfig.Controls.Add(this.tabPageAlign);
            this.tabConfig.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tabConfig.Location = new System.Drawing.Point(4, 18);
            this.tabConfig.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabConfig.Name = "tabConfig";
            this.tabConfig.SelectedIndex = 0;
            this.tabConfig.Size = new System.Drawing.Size(335, 225);
            this.tabConfig.TabIndex = 0;
            // 
            // tabPageROI
            // 
            this.tabPageROI.Controls.Add(this.dgvROI);
            this.tabPageROI.Controls.Add(this.btnAddROI);
            this.tabPageROI.Controls.Add(this.btnRemoveROI);
            this.tabPageROI.Controls.Add(this.btnClearROI);
            this.tabPageROI.Location = new System.Drawing.Point(4, 22);
            this.tabPageROI.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageROI.Name = "tabPageROI";
            this.tabPageROI.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageROI.Size = new System.Drawing.Size(327, 199);
            this.tabPageROI.TabIndex = 0;
            this.tabPageROI.Text = "ROI配置";
            this.tabPageROI.UseVisualStyleBackColor = true;
            // 
            // dgvROI
            // 
            this.dgvROI.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dgvROI.Dock = System.Windows.Forms.DockStyle.Top;
            this.dgvROI.Location = new System.Drawing.Point(4, 4);
            this.dgvROI.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.dgvROI.Name = "dgvROI";
            this.dgvROI.RowHeadersWidth = 62;
            this.dgvROI.Size = new System.Drawing.Size(319, 154);
            this.dgvROI.TabIndex = 0;
            // 
            // btnAddROI
            // 
            this.btnAddROI.Location = new System.Drawing.Point(21, 168);
            this.btnAddROI.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnAddROI.Name = "btnAddROI";
            this.btnAddROI.Size = new System.Drawing.Size(85, 21);
            this.btnAddROI.TabIndex = 1;
            this.btnAddROI.Text = "添加ROI";
            this.btnAddROI.UseVisualStyleBackColor = true;
            // 
            // btnRemoveROI
            // 
            this.btnRemoveROI.Location = new System.Drawing.Point(122, 168);
            this.btnRemoveROI.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnRemoveROI.Name = "btnRemoveROI";
            this.btnRemoveROI.Size = new System.Drawing.Size(85, 21);
            this.btnRemoveROI.TabIndex = 2;
            this.btnRemoveROI.Text = "删除ROI";
            this.btnRemoveROI.UseVisualStyleBackColor = true;
            // 
            // btnClearROI
            // 
            this.btnClearROI.Location = new System.Drawing.Point(224, 168);
            this.btnClearROI.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnClearROI.Name = "btnClearROI";
            this.btnClearROI.Size = new System.Drawing.Size(85, 21);
            this.btnClearROI.TabIndex = 3;
            this.btnClearROI.Text = "清除所有ROI";
            this.btnClearROI.UseVisualStyleBackColor = true;
            // 
            // tabPageBinary
            // 
            this.tabPageBinary.Controls.Add(this.chkBinaryOpt);
            this.tabPageBinary.Controls.Add(this.lblBinaryThreshold);
            this.tabPageBinary.Controls.Add(this.nudBinaryThreshold);
            this.tabPageBinary.Controls.Add(this.lblEdgeTol);
            this.tabPageBinary.Controls.Add(this.nudEdgeTol);
            this.tabPageBinary.Controls.Add(this.lblNoiseSize);
            this.tabPageBinary.Controls.Add(this.nudNoiseSize);
            this.tabPageBinary.Controls.Add(this.lblMinArea);
            this.tabPageBinary.Controls.Add(this.nudMinArea);
            this.tabPageBinary.Controls.Add(this.lblSimThresh);
            this.tabPageBinary.Controls.Add(this.nudSimThresh);
            this.tabPageBinary.Location = new System.Drawing.Point(4, 22);
            this.tabPageBinary.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageBinary.Name = "tabPageBinary";
            this.tabPageBinary.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageBinary.Size = new System.Drawing.Size(511, 199);
            this.tabPageBinary.TabIndex = 1;
            this.tabPageBinary.Text = "二值化参数";
            this.tabPageBinary.UseVisualStyleBackColor = true;
            // 
            // chkBinaryOpt
            // 
            this.chkBinaryOpt.AutoSize = true;
            this.chkBinaryOpt.Location = new System.Drawing.Point(13, 13);
            this.chkBinaryOpt.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.chkBinaryOpt.Name = "chkBinaryOpt";
            this.chkBinaryOpt.Size = new System.Drawing.Size(96, 16);
            this.chkBinaryOpt.TabIndex = 0;
            this.chkBinaryOpt.Text = "启用二值优化";
            this.chkBinaryOpt.UseVisualStyleBackColor = true;
            // 
            // lblBinaryThreshold
            // 
            this.lblBinaryThreshold.AutoSize = true;
            this.lblBinaryThreshold.Location = new System.Drawing.Point(13, 51);
            this.lblBinaryThreshold.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblBinaryThreshold.Name = "lblBinaryThreshold";
            this.lblBinaryThreshold.Size = new System.Drawing.Size(71, 12);
            this.lblBinaryThreshold.TabIndex = 1;
            this.lblBinaryThreshold.Text = "二值化阈值:";
            // 
            // nudBinaryThreshold
            // 
            this.nudBinaryThreshold.Location = new System.Drawing.Point(129, 49);
            this.nudBinaryThreshold.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.nudBinaryThreshold.Maximum = new decimal(new int[] {
            255,
            0,
            0,
            0});
            this.nudBinaryThreshold.Name = "nudBinaryThreshold";
            this.nudBinaryThreshold.Size = new System.Drawing.Size(103, 21);
            this.nudBinaryThreshold.TabIndex = 2;
            this.nudBinaryThreshold.Value = new decimal(new int[] {
            160,
            0,
            0,
            0});
            // 
            // lblEdgeTol
            // 
            this.lblEdgeTol.AutoSize = true;
            this.lblEdgeTol.Location = new System.Drawing.Point(257, 51);
            this.lblEdgeTol.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblEdgeTol.Name = "lblEdgeTol";
            this.lblEdgeTol.Size = new System.Drawing.Size(59, 12);
            this.lblEdgeTol.TabIndex = 3;
            this.lblEdgeTol.Text = "边缘容错:";
            // 
            // nudEdgeTol
            // 
            this.nudEdgeTol.Location = new System.Drawing.Point(360, 49);
            this.nudEdgeTol.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.nudEdgeTol.Maximum = new decimal(new int[] {
            20,
            0,
            0,
            0});
            this.nudEdgeTol.Name = "nudEdgeTol";
            this.nudEdgeTol.Size = new System.Drawing.Size(77, 21);
            this.nudEdgeTol.TabIndex = 4;
            this.nudEdgeTol.Value = new decimal(new int[] {
            4,
            0,
            0,
            0});
            // 
            // lblNoiseSize
            // 
            this.lblNoiseSize.AutoSize = true;
            this.lblNoiseSize.Location = new System.Drawing.Point(13, 90);
            this.lblNoiseSize.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblNoiseSize.Name = "lblNoiseSize";
            this.lblNoiseSize.Size = new System.Drawing.Size(59, 12);
            this.lblNoiseSize.TabIndex = 5;
            this.lblNoiseSize.Text = "噪声过滤:";
            // 
            // nudNoiseSize
            // 
            this.nudNoiseSize.Location = new System.Drawing.Point(129, 87);
            this.nudNoiseSize.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.nudNoiseSize.Maximum = new decimal(new int[] {
            50,
            0,
            0,
            0});
            this.nudNoiseSize.Name = "nudNoiseSize";
            this.nudNoiseSize.Size = new System.Drawing.Size(103, 21);
            this.nudNoiseSize.TabIndex = 6;
            this.nudNoiseSize.Value = new decimal(new int[] {
            10,
            0,
            0,
            0});
            // 
            // lblMinArea
            // 
            this.lblMinArea.AutoSize = true;
            this.lblMinArea.Location = new System.Drawing.Point(257, 90);
            this.lblMinArea.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblMinArea.Name = "lblMinArea";
            this.lblMinArea.Size = new System.Drawing.Size(59, 12);
            this.lblMinArea.TabIndex = 7;
            this.lblMinArea.Text = "最小面积:";
            // 
            // nudMinArea
            // 
            this.nudMinArea.Location = new System.Drawing.Point(360, 87);
            this.nudMinArea.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.nudMinArea.Maximum = new decimal(new int[] {
            500,
            0,
            0,
            0});
            this.nudMinArea.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.nudMinArea.Name = "nudMinArea";
            this.nudMinArea.Size = new System.Drawing.Size(77, 21);
            this.nudMinArea.TabIndex = 8;
            this.nudMinArea.Value = new decimal(new int[] {
            20,
            0,
            0,
            0});
            // 
            // lblSimThresh
            // 
            this.lblSimThresh.AutoSize = true;
            this.lblSimThresh.Location = new System.Drawing.Point(13, 129);
            this.lblSimThresh.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblSimThresh.Name = "lblSimThresh";
            this.lblSimThresh.Size = new System.Drawing.Size(65, 12);
            this.lblSimThresh.TabIndex = 9;
            this.lblSimThresh.Text = "相似度(%):";
            // 
            // nudSimThresh
            // 
            this.nudSimThresh.DecimalPlaces = 1;
            this.nudSimThresh.Location = new System.Drawing.Point(129, 126);
            this.nudSimThresh.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.nudSimThresh.Name = "nudSimThresh";
            this.nudSimThresh.Size = new System.Drawing.Size(103, 21);
            this.nudSimThresh.TabIndex = 10;
            this.nudSimThresh.Value = new decimal(new int[] {
            90,
            0,
            0,
            0});
            // 
            // tabPageAlign
            // 
            this.tabPageAlign.Controls.Add(this.chkAutoAlign);
            this.tabPageAlign.Controls.Add(this.lblAlignMode);
            this.tabPageAlign.Controls.Add(this.cmbAlignMode);
            this.tabPageAlign.Location = new System.Drawing.Point(4, 22);
            this.tabPageAlign.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageAlign.Name = "tabPageAlign";
            this.tabPageAlign.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.tabPageAlign.Size = new System.Drawing.Size(511, 199);
            this.tabPageAlign.TabIndex = 2;
            this.tabPageAlign.Text = "对齐配置";
            this.tabPageAlign.UseVisualStyleBackColor = true;
            // 
            // chkAutoAlign
            // 
            this.chkAutoAlign.AutoSize = true;
            this.chkAutoAlign.Checked = true;
            this.chkAutoAlign.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkAutoAlign.Location = new System.Drawing.Point(13, 13);
            this.chkAutoAlign.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.chkAutoAlign.Name = "chkAutoAlign";
            this.chkAutoAlign.Size = new System.Drawing.Size(72, 16);
            this.chkAutoAlign.TabIndex = 0;
            this.chkAutoAlign.Text = "自动对齐";
            this.chkAutoAlign.UseVisualStyleBackColor = true;
            // 
            // lblAlignMode
            // 
            this.lblAlignMode.AutoSize = true;
            this.lblAlignMode.Location = new System.Drawing.Point(13, 51);
            this.lblAlignMode.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblAlignMode.Name = "lblAlignMode";
            this.lblAlignMode.Size = new System.Drawing.Size(59, 12);
            this.lblAlignMode.TabIndex = 1;
            this.lblAlignMode.Text = "对齐模式:";
            // 
            // cmbAlignMode
            // 
            this.cmbAlignMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cmbAlignMode.FormattingEnabled = true;
            this.cmbAlignMode.Location = new System.Drawing.Point(103, 49);
            this.cmbAlignMode.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.cmbAlignMode.Name = "cmbAlignMode";
            this.cmbAlignMode.Size = new System.Drawing.Size(192, 20);
            this.cmbAlignMode.TabIndex = 2;
            // 
            // groupBoxControl
            // 
            this.groupBoxControl.Controls.Add(this.groupBoxSocket);
            this.groupBoxControl.Controls.Add(this.btnSelectTemplate);
            this.groupBoxControl.Controls.Add(this.btnSelectFolder);
            this.groupBoxControl.Controls.Add(this.btnBrowseCSV);
            this.groupBoxControl.Controls.Add(this.txtCSVPath);
            this.groupBoxControl.Controls.Add(this.btnStop);
            this.groupBoxControl.Controls.Add(this.btnPrev);
            this.groupBoxControl.Controls.Add(this.btnNext);
            this.groupBoxControl.Controls.Add(this.btnSingleCompare);
            this.groupBoxControl.Controls.Add(this.btnContinuousCompare);
            this.groupBoxControl.Dock = System.Windows.Forms.DockStyle.Left;
            this.groupBoxControl.Location = new System.Drawing.Point(0, 0);
            this.groupBoxControl.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxControl.Name = "groupBoxControl";
            this.groupBoxControl.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxControl.Size = new System.Drawing.Size(383, 247);
            this.groupBoxControl.TabIndex = 0;
            this.groupBoxControl.TabStop = false;
            this.groupBoxControl.Text = "控制面板";
            // 
            // groupBoxSocket
            // 
            this.groupBoxSocket.Controls.Add(this.txtServerIP);
            this.groupBoxSocket.Controls.Add(this.txtServerPort);
            this.groupBoxSocket.Controls.Add(this.btnSocketConnect);
            this.groupBoxSocket.Controls.Add(this.btnSocketDisconnect);
            this.groupBoxSocket.Controls.Add(this.lblSocketStatus);
            this.groupBoxSocket.Controls.Add(this.chkAutoReconnect);
            this.groupBoxSocket.Location = new System.Drawing.Point(20, 149);
            this.groupBoxSocket.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxSocket.Name = "groupBoxSocket";
            this.groupBoxSocket.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.groupBoxSocket.Size = new System.Drawing.Size(356, 83);
            this.groupBoxSocket.TabIndex = 10;
            this.groupBoxSocket.TabStop = false;
            this.groupBoxSocket.Text = "Socket服务";
            // 
            // txtServerIP
            // 
            this.txtServerIP.Location = new System.Drawing.Point(10, 22);
            this.txtServerIP.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.txtServerIP.Name = "txtServerIP";
            this.txtServerIP.Size = new System.Drawing.Size(120, 21);
            this.txtServerIP.TabIndex = 0;
            this.txtServerIP.Text = "127.0.0.1";
            // 
            // txtServerPort
            // 
            this.txtServerPort.Location = new System.Drawing.Point(140, 22);
            this.txtServerPort.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.txtServerPort.Name = "txtServerPort";
            this.txtServerPort.Size = new System.Drawing.Size(60, 21);
            this.txtServerPort.TabIndex = 1;
            this.txtServerPort.Text = "8888";
            // 
            // btnSocketConnect
            // 
            this.btnSocketConnect.Location = new System.Drawing.Point(210, 20);
            this.btnSocketConnect.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnSocketConnect.Name = "btnSocketConnect";
            this.btnSocketConnect.Size = new System.Drawing.Size(60, 25);
            this.btnSocketConnect.TabIndex = 2;
            this.btnSocketConnect.Text = "连接";
            this.btnSocketConnect.UseVisualStyleBackColor = true;
            // 
            // btnSocketDisconnect
            // 
            this.btnSocketDisconnect.Enabled = false;
            this.btnSocketDisconnect.Location = new System.Drawing.Point(280, 20);
            this.btnSocketDisconnect.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnSocketDisconnect.Name = "btnSocketDisconnect";
            this.btnSocketDisconnect.Size = new System.Drawing.Size(60, 25);
            this.btnSocketDisconnect.TabIndex = 3;
            this.btnSocketDisconnect.Text = "断开";
            this.btnSocketDisconnect.UseVisualStyleBackColor = true;
            // 
            // lblSocketStatus
            // 
            this.lblSocketStatus.AutoSize = true;
            this.lblSocketStatus.ForeColor = System.Drawing.Color.Red;
            this.lblSocketStatus.Location = new System.Drawing.Point(120, 58);
            this.lblSocketStatus.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lblSocketStatus.Name = "lblSocketStatus";
            this.lblSocketStatus.Size = new System.Drawing.Size(41, 12);
            this.lblSocketStatus.TabIndex = 5;
            this.lblSocketStatus.Text = "未连接";
            // 
            // chkAutoReconnect
            // 
            this.chkAutoReconnect.AutoSize = true;
            this.chkAutoReconnect.Checked = true;
            this.chkAutoReconnect.CheckState = System.Windows.Forms.CheckState.Checked;
            this.chkAutoReconnect.Location = new System.Drawing.Point(10, 56);
            this.chkAutoReconnect.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.chkAutoReconnect.Name = "chkAutoReconnect";
            this.chkAutoReconnect.Size = new System.Drawing.Size(72, 16);
            this.chkAutoReconnect.TabIndex = 4;
            this.chkAutoReconnect.Text = "自动重连";
            this.chkAutoReconnect.UseVisualStyleBackColor = true;
            // 
            // btnSelectTemplate
            // 
            this.btnSelectTemplate.Location = new System.Drawing.Point(19, 21);
            this.btnSelectTemplate.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnSelectTemplate.Name = "btnSelectTemplate";
            this.btnSelectTemplate.Size = new System.Drawing.Size(120, 25);
            this.btnSelectTemplate.TabIndex = 0;
            this.btnSelectTemplate.Text = "选择模板";
            this.btnSelectTemplate.UseVisualStyleBackColor = true;
            // 
            // btnSelectFolder
            // 
            this.btnSelectFolder.Location = new System.Drawing.Point(138, 21);
            this.btnSelectFolder.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnSelectFolder.Name = "btnSelectFolder";
            this.btnSelectFolder.Size = new System.Drawing.Size(120, 25);
            this.btnSelectFolder.TabIndex = 1;
            this.btnSelectFolder.Text = "比对文件";
            this.btnSelectFolder.UseVisualStyleBackColor = true;
            // 
            // btnBrowseCSV
            // 
            this.btnBrowseCSV.Location = new System.Drawing.Point(256, 21);
            this.btnBrowseCSV.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnBrowseCSV.Name = "btnBrowseCSV";
            this.btnBrowseCSV.Size = new System.Drawing.Size(120, 25);
            this.btnBrowseCSV.TabIndex = 2;
            this.btnBrowseCSV.Text = "偏移文件";
            this.btnBrowseCSV.UseVisualStyleBackColor = true;
            // 
            // txtCSVPath
            // 
            this.txtCSVPath.Location = new System.Drawing.Point(19, 56);
            this.txtCSVPath.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.txtCSVPath.Name = "txtCSVPath";
            this.txtCSVPath.ReadOnly = true;
            this.txtCSVPath.Size = new System.Drawing.Size(356, 21);
            this.txtCSVPath.TabIndex = 3;
            // 
            // btnStop
            // 
            this.btnStop.Enabled = false;
            this.btnStop.Location = new System.Drawing.Point(19, 111);
            this.btnStop.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnStop.Name = "btnStop";
            this.btnStop.Size = new System.Drawing.Size(356, 28);
            this.btnStop.TabIndex = 9;
            this.btnStop.Text = "停止";
            this.btnStop.UseVisualStyleBackColor = true;
            // 
            // btnPrev
            // 
            this.btnPrev.Location = new System.Drawing.Point(19, 81);
            this.btnPrev.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnPrev.Name = "btnPrev";
            this.btnPrev.Size = new System.Drawing.Size(90, 25);
            this.btnPrev.TabIndex = 5;
            this.btnPrev.Text = "上一张";
            this.btnPrev.UseVisualStyleBackColor = true;
            // 
            // btnNext
            // 
            this.btnNext.Location = new System.Drawing.Point(108, 81);
            this.btnNext.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnNext.Name = "btnNext";
            this.btnNext.Size = new System.Drawing.Size(90, 25);
            this.btnNext.TabIndex = 6;
            this.btnNext.Text = "下一张";
            this.btnNext.UseVisualStyleBackColor = true;
            // 
            // btnSingleCompare
            // 
            this.btnSingleCompare.Location = new System.Drawing.Point(196, 81);
            this.btnSingleCompare.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnSingleCompare.Name = "btnSingleCompare";
            this.btnSingleCompare.Size = new System.Drawing.Size(90, 25);
            this.btnSingleCompare.TabIndex = 7;
            this.btnSingleCompare.Text = "单步";
            this.btnSingleCompare.UseVisualStyleBackColor = true;
            // 
            // btnContinuousCompare
            // 
            this.btnContinuousCompare.Location = new System.Drawing.Point(285, 81);
            this.btnContinuousCompare.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.btnContinuousCompare.Name = "btnContinuousCompare";
            this.btnContinuousCompare.Size = new System.Drawing.Size(90, 25);
            this.btnContinuousCompare.TabIndex = 8;
            this.btnContinuousCompare.Text = "连续";
            this.btnContinuousCompare.UseVisualStyleBackColor = true;
            // 
            // lblCSVPath
            // 
            this.lblCSVPath.Location = new System.Drawing.Point(0, 0);
            this.lblCSVPath.Name = "lblCSVPath";
            this.lblCSVPath.Size = new System.Drawing.Size(100, 23);
            this.lblCSVPath.TabIndex = 0;
            // 
            // statusStrip
            // 
            this.statusStrip.ImageScalingSize = new System.Drawing.Size(24, 24);
            this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripStatusLabel});
            this.statusStrip.Location = new System.Drawing.Point(0, 608);
            this.statusStrip.Name = "statusStrip";
            this.statusStrip.Padding = new System.Windows.Forms.Padding(1, 0, 18, 0);
            this.statusStrip.Size = new System.Drawing.Size(1130, 22);
            this.statusStrip.TabIndex = 1;
            this.statusStrip.Text = "statusStrip";
            // 
            // toolStripStatusLabel
            // 
            this.toolStripStatusLabel.Name = "toolStripStatusLabel";
            this.toolStripStatusLabel.Size = new System.Drawing.Size(32, 17);
            this.toolStripStatusLabel.Text = "就绪";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
            this.ClientSize = new System.Drawing.Size(1130, 630);
            this.Controls.Add(this.splitContainer1);
            this.Controls.Add(this.statusStrip);
            this.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.Name = "Form1";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "喷印瑕疵检测系统";
            this.Resize += new System.EventHandler(this.Form1_Resize);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTemplate)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTest)).EndInit();
            this.groupBoxLog.ResumeLayout(false);
            this.panel1.ResumeLayout(false);
            this.groupBoxDefects.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dgvDefects)).EndInit();
            this.groupBoxStats.ResumeLayout(false);
            this.groupBoxStats.PerformLayout();
            this.groupBoxConfig.ResumeLayout(false);
            this.tabConfig.ResumeLayout(false);
            this.tabPageROI.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.dgvROI)).EndInit();
            this.tabPageBinary.ResumeLayout(false);
            this.tabPageBinary.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.nudBinaryThreshold)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudEdgeTol)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudNoiseSize)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudMinArea)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudSimThresh)).EndInit();
            this.tabPageAlign.ResumeLayout(false);
            this.tabPageAlign.PerformLayout();
            this.groupBoxControl.ResumeLayout(false);
            this.groupBoxControl.PerformLayout();
            this.groupBoxSocket.ResumeLayout(false);
            this.groupBoxSocket.PerformLayout();
            this.statusStrip.ResumeLayout(false);
            this.statusStrip.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }
    }
}