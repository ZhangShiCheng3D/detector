using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.IO;

namespace DetectionUITest
{
    public class SocketClient : IDisposable
    {
        private TcpClient _client;
        private NetworkStream _stream;
        private readonly object _lock = new object();
        private CancellationTokenSource _cts;
        private Task _receiveTask;
        private bool _isConnected = false;

        public event EventHandler<bool> ConnectionStatusChanged;
        public event EventHandler<string> LogMessage;
        public event EventHandler<byte[]> MessageReceived;

        public string ServerIp { get; set; }
        public int ServerPort { get; set; }
        public bool IsConnected => _isConnected;
        public bool AutoReconnect { get; set; } = true;
        public bool IsConnecting { get; private set; } = false;
        public SocketClient()
        {
            _cts = new CancellationTokenSource();
        }

        public async Task StartAsync(string ip, int port)
        {
            if (IsConnecting)
            {
                LogMessage?.Invoke(this, "正在连接中，请稍候...");
                return;
            }

            IsConnecting = true;
            ServerIp = ip;
            ServerPort = port;

            Stop();

            _cts = new CancellationTokenSource();
            _receiveTask = Task.Run(() => ReceiveLoopAsync(_cts.Token));

            // 等待一小段时间，看是否连接成功
            await Task.Delay(100);
            IsConnecting = false;
        }

        public void Stop()
        {
            try
            {
                _cts?.Cancel();
                lock (_lock)
                {
                    _stream?.Close();
                    _client?.Close();
                    _stream = null;
                    _client = null;
                }

                if (_isConnected)
                {
                    _isConnected = false;
                    ConnectionStatusChanged?.Invoke(this, false);
                }
            }
            catch (Exception ex)
            {
                LogMessage?.Invoke(this, $"停止Socket错误: {ex.Message}");
            }
        }

        private async Task ReceiveLoopAsync(CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    if (!_isConnected)
                    {
                        await ConnectAsync(cancellationToken);
                    }

                    if (_isConnected && _stream != null && _stream.CanRead)
                    {
                        await ReceiveMessageAsync(cancellationToken);
                    }
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    LogMessage?.Invoke(this, $"接收循环错误: {ex.Message}");

                    if (_isConnected)
                    {
                        _isConnected = false;
                        ConnectionStatusChanged?.Invoke(this, false);
                    }

                    if (AutoReconnect && !cancellationToken.IsCancellationRequested)
                    {
                        LogMessage?.Invoke(this, "尝试重新连接...");
                        await Task.Delay(3000, cancellationToken);
                    }
                    else
                    {
                        await Task.Delay(1000, cancellationToken);
                    }
                }
            }

            IsConnecting = false;
        }
        private async Task ConnectAsync(CancellationToken cancellationToken)
        {
            try
            {
                var client = new TcpClient();
                await client.ConnectAsync(ServerIp, ServerPort).ConfigureAwait(false);

                lock (_lock)
                {
                    _client = client;
                    _stream = client.GetStream();
                }

                _isConnected = true;
                ConnectionStatusChanged?.Invoke(this, true);
                LogMessage?.Invoke(this, $"已连接到服务器 {ServerIp}:{ServerPort}");

                // 发送就绪状态
                await SendStatusAsync("ready");
            }
            catch (Exception ex)
            {
                LogMessage?.Invoke(this, $"连接失败: {ex.Message}");
                throw;
            }
        }

        private async Task ReceiveMessageAsync(CancellationToken cancellationToken)
        {
            byte[] headerBuffer = new byte[SocketProtocol.HEADER_SIZE];
            int bytesRead = 0;

            // 读取消息头
            while (bytesRead < SocketProtocol.HEADER_SIZE)
            {
                int read = await _stream.ReadAsync(headerBuffer, bytesRead,
                    SocketProtocol.HEADER_SIZE - bytesRead, cancellationToken);
                if (read == 0) throw new Exception("连接已关闭");
                bytesRead += read;
            }

            // 解析消息头
            if (!SocketProtocol.DecodeHeader(headerBuffer, out byte command, out int dataLength))
            {
                throw new Exception("无效的消息头");
            }

            // 读取消息体
            byte[] dataBuffer = new byte[dataLength];
            bytesRead = 0;
            while (bytesRead < dataLength)
            {
                int read = await _stream.ReadAsync(dataBuffer, bytesRead, dataLength - bytesRead, cancellationToken);
                if (read == 0) throw new Exception("连接已关闭");
                bytesRead += read;
            }

            // 组合完整消息
            byte[] fullMessage = new byte[SocketProtocol.HEADER_SIZE + dataLength];
            Array.Copy(headerBuffer, 0, fullMessage, 0, SocketProtocol.HEADER_SIZE);
            Array.Copy(dataBuffer, 0, fullMessage, SocketProtocol.HEADER_SIZE, dataLength);

            LogMessage?.Invoke(this, $"收到命令: 0x{command:X2}, 数据长度: {dataLength}");

            // 触发消息接收事件
            MessageReceived?.Invoke(this, fullMessage);
        }

        public async Task SendAsync(byte[] data)
        {
            if (!_isConnected || _stream == null)
                throw new InvalidOperationException("未连接到服务器");

            lock (_lock)
            {
                _stream.Write(data, 0, data.Length);
            }
            await Task.CompletedTask;
        }

        public async Task SendTextAsync(byte command, string text)
        {
            await SendAsync(SocketProtocol.CreateTextMessage(command, text));
        }

        public async Task SendResultAsync(bool passed, int defectCount, string details)
        {
            await SendAsync(SocketProtocol.CreateResultMessage(passed, defectCount, details));
            LogMessage?.Invoke(this, $"发送比对结果: 通过={passed}, 瑕疵数={defectCount}");
        }

        public async Task SendStatusAsync(string status)
        {
            await SendAsync(SocketProtocol.CreateStatusMessage(_isConnected, status));
        }

        public void Dispose()
        {
            Stop();
            _cts?.Dispose();
        }
    }
}