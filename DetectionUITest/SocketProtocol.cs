using System;
using System.Text;

namespace DetectionUITest
{
    /// <summary>
    /// Socket通信协议定义
    /// 消息格式： [命令类型(1字节)][数据长度(4字节)][数据内容]
    /// 命令类型：
    /// 0x01 - 设置模板图片
    /// 0x02 - 比对图片
    /// 0x03 - 配置ROI
    /// 0x04 - 配置参数
    /// 0x05 - 比对结果响应
    /// 0x06 - 状态响应
    /// 0xFF - 心跳/Ping
    /// </summary>
    public static class SocketProtocol
    {
        // 命令类型
        public const byte CMD_SET_TEMPLATE = 0x01;      // 设置模板图片
        public const byte CMD_COMPARE_IMAGE = 0x02;      // 比对图片
        public const byte CMD_CONFIGURE_ROI = 0x03;      // 配置ROI
        public const byte CMD_CONFIGURE_PARAM = 0x04;    // 配置参数
        public const byte CMD_COMPARE_RESULT = 0x05;     // 比对结果响应
        public const byte CMD_STATUS = 0x06;              // 状态响应
        public const byte CMD_PING = 0xFF;                // 心跳

        // 消息头大小: 命令(1) + 长度(4) = 5字节
        public const int HEADER_SIZE = 5;

        /// <summary>
        /// 编码消息
        /// </summary>
        public static byte[] EncodeMessage(byte command, byte[] data)
        {
            byte[] message = new byte[HEADER_SIZE + (data?.Length ?? 0)];
            message[0] = command;

            // 写入数据长度（大端序）
            int dataLen = data?.Length ?? 0;
            message[1] = (byte)((dataLen >> 24) & 0xFF);
            message[2] = (byte)((dataLen >> 16) & 0xFF);
            message[3] = (byte)((dataLen >> 8) & 0xFF);
            message[4] = (byte)(dataLen & 0xFF);

            // 写入数据
            if (data != null && data.Length > 0)
            {
                Array.Copy(data, 0, message, HEADER_SIZE, data.Length);
            }

            return message;
        }

        /// <summary>
        /// 解码消息头
        /// </summary>
        public static bool DecodeHeader(byte[] header, out byte command, out int dataLength)
        {
            command = 0;
            dataLength = 0;

            if (header == null || header.Length < HEADER_SIZE)
                return false;

            command = header[0];
            dataLength = (header[1] << 24) | (header[2] << 16) | (header[3] << 8) | header[4];

            return true;
        }

        /// <summary>
        /// 创建文本数据包
        /// </summary>
        public static byte[] CreateTextMessage(byte command, string text)
        {
            byte[] data = text != null ? Encoding.UTF8.GetBytes(text) : new byte[0];
            return EncodeMessage(command, data);
        }

        /// <summary>
        /// 从数据包中提取文本
        /// </summary>
        public static string ExtractText(byte[] data)
        {
            return data != null ? Encoding.UTF8.GetString(data) : "";
        }

        /// <summary>
        /// 创建图片数据包
        /// </summary>
        public static byte[] CreateImageMessage(byte command, byte[] imageData)
        {
            return EncodeMessage(command, imageData ?? new byte[0]);
        }

        /// <summary>
        /// 创建比对结果响应
        /// </summary>
        public static byte[] CreateResultMessage(bool passed, int defectCount, string details)
        {
            string result = $"{{" +
                $"\"passed\":{passed.ToString().ToLower()}," +
                $"\"defectCount\":{defectCount}," +
                $"\"details\":\"{EscapeJson(details)}\"" +
                $"}}";
            return CreateTextMessage(CMD_COMPARE_RESULT, result);
        }

        /// <summary>
        /// 创建状态响应
        /// </summary>
        public static byte[] CreateStatusMessage(bool isReady, string status)
        {
            string result = $"{{" +
                $"\"ready\":{isReady.ToString().ToLower()}," +
                $"\"status\":\"{EscapeJson(status)}\"" +
                $"}}";
            return CreateTextMessage(CMD_STATUS, result);
        }

        private static string EscapeJson(string s)
        {
            if (string.IsNullOrEmpty(s)) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        /// <summary>
        /// 解析ROI配置
        /// 格式: ROI_ID,X,Y,Width,Height,Threshold;ROI_ID,X,Y,Width,Height,Threshold
        /// </summary>
        public static bool ParseROIConfig(string config, out ROIConfigItem[] rois)
        {
            var list = new System.Collections.Generic.List<ROIConfigItem>();
            rois = null;

            try
            {
                var parts = config.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var part in parts)
                {
                    var values = part.Split(',');
                    if (values.Length >= 6)
                    {
                        var roi = new ROIConfigItem
                        {
                            Id = int.Parse(values[0]),
                            X = int.Parse(values[1]),
                            Y = int.Parse(values[2]),
                            Width = int.Parse(values[3]),
                            Height = int.Parse(values[4]),
                            Threshold = float.Parse(values[5])
                        };
                        list.Add(roi);
                    }
                }

                rois = list.ToArray();
                return rois.Length > 0;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 解析参数配置
        /// 格式: param1=value1,param2=value2,...
        /// </summary>
        public static bool ParseParamConfig(string config, out System.Collections.Generic.Dictionary<string, string> parameters)
        {
            parameters = new System.Collections.Generic.Dictionary<string, string>();

            try
            {
                var parts = config.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var part in parts)
                {
                    var kv = part.Split('=');
                    if (kv.Length == 2)
                    {
                        parameters[kv[0].Trim()] = kv[1].Trim();
                    }
                }
                return parameters.Count > 0;
            }
            catch
            {
                return false;
            }
        }
    }

    /// <summary>
    /// ROI配置项
    /// </summary>
    public class ROIConfigItem
    {
        public int Id { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
        public float Threshold { get; set; }
    }
}