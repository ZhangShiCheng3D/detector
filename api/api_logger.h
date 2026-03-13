/**
 * @file api_logger.h
 * @brief API调用日志记录器
 * @description 记录所有C API接口的调用情况，便于调试和排查问题
 */

#ifndef API_LOGGER_H
#define API_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace defect_detection {

/**
 * @brief API调用日志记录器类
 * 
 * 单例模式，线程安全，自动记录时间戳
 */
class APILogger {
public:
    /**
     * @brief 获取单例实例
     */
    static APILogger& getInstance() {
        static APILogger instance;
        return instance;
    }

    /**
     * @brief 设置日志文件路径
     * @param filepath 日志文件路径
     */
    void setLogFile(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
        log_filepath_ = filepath;
        log_file_.open(filepath, std::ios::out | std::ios::app);
    }

    /**
     * @brief 启用/禁用日志
     * @param enabled 是否启用
     */
    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }

    /**
     * @brief 记录API调用（带参数）
     * @param func_name 函数名
     * @param params 参数描述
     */
    void logCall(const std::string& func_name, const std::string& params = "") {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        ensureFileOpen();
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[CALL] " << func_name;
        
        if (!params.empty()) {
            ss << " | " << params;
        }
        
        writeLine(ss.str());
    }

    /**
     * @brief 记录API调用结果
     * @param func_name 函数名
     * @param result 结果描述
     */
    void logResult(const std::string& func_name, const std::string& result) {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        ensureFileOpen();
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[RET]  " << func_name << " | " << result;
        
        writeLine(ss.str());
    }

    /**
     * @brief 记录配置信息
     * @param category 配置类别
     * @param config 配置详情
     */
    void logConfig(const std::string& category, const std::string& config) {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        ensureFileOpen();
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[CFG]  [" << category << "] " << config;
        
        writeLine(ss.str());
    }

    /**
     * @brief 记录检测信息
     * @param info 检测信息
     */
    void logDetection(const std::string& info) {
        if (!enabled_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        ensureFileOpen();
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << "[DET]  " << info;
        
        writeLine(ss.str());
    }

private:
    APILogger() : enabled_(true), log_filepath_("api_debug.log") {
        log_file_.open(log_filepath_, std::ios::out | std::ios::app);
    }
    
    ~APILogger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    // 禁止拷贝和移动
    APILogger(const APILogger&) = delete;
    APILogger& operator=(const APILogger&) = delete;

    void ensureFileOpen() {
        if (!log_file_.is_open()) {
            log_file_.open(log_filepath_, std::ios::out | std::ios::app);
        }
    }

    void writeLine(const std::string& line) {
        if (log_file_.is_open()) {
            log_file_ << line << std::endl;
            log_file_.flush();
        }
    }

    bool enabled_;
    std::string log_filepath_;
    std::ofstream log_file_;
    std::mutex mutex_;
};

// 便捷宏定义
#define API_LOG_CALL(func, params) \
    defect_detection::APILogger::getInstance().logCall(func, params)

#define API_LOG_RESULT(func, result) \
    defect_detection::APILogger::getInstance().logResult(func, result)

#define API_LOG_CONFIG(category, config) \
    defect_detection::APILogger::getInstance().logConfig(category, config)

#define API_LOG_DETECTION(info) \
    defect_detection::APILogger::getInstance().logDetection(info)

} // namespace defect_detection

#endif // API_LOGGER_H
