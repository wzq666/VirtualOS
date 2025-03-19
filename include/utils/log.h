/**
 * @file log.h
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief 日志组件
 * @version 0.1
 * @date 2024-12-27
 * 
 * @copyright Copyright (c) 2024-2025
 * @see repository: https://github.com/i-tesetd-it-no-problem/VirtualOS.git
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#ifndef __VIRTUAL_OS_LOG_H__
#define __VIRTUAL_OS_LOG_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define USE_TIME_STAMP 0 /* 日志启用时显示时间,0为关闭,1为启用 开启会编译time.h头文件，将占用大量FLASH空间 */
#define MAX_LOG_LENGTH 256									 /* 每条日志的最大长度 */
#define TOTAL_FRAME_COUNT (8)								 // 缓冲8条
#define LOG_BUFFER_SIZE (MAX_LOG_LENGTH * TOTAL_FRAME_COUNT) /* 日志缓冲区总大小 2K */

typedef size_t (*log_write)(uint8_t *buf, size_t len); // 发送接口

enum log_level {
	LOG_LEVEL_ALL = 0,	 /* 所有日志 */
	LOG_LEVEL_DEBUG = 1, /* 调试日志 */
	LOG_LEVEL_INFO = 2,	 /* 信息日志 */
	LOG_LEVEL_WARN = 3,	 /* 警告日志 */
	LOG_LEVEL_ERROR = 4, /* 错误日志 */
	LOG_LEVEL_NONE = 5	 /* 关闭日志 */
};

/**
 * @brief 日志发送 可以通过掩码过滤日志 建议使用宏定义
 * 
 * @param mask 模块掩码
 * @param level 日志等级
 * @param line 行号
 * @param format 日志格式
 * @param ... 可变参数
 */
void origin_log(uint32_t mask, enum log_level level, int line, const char *format, ...);

/**
 * @brief 修改输出接口
 * 
 * @param interface 新的读写接口
 */
void modify_output(log_write f_write);

/**
 * @brief 填充模块名称数组 按照掩码从小到大排序 模块名的索引就是实际的掩码位
 * 		  结束后 module_buf_size 指向实际填充的模块个数
 * @param module_buf 
 * @param module_buf_size 
 */
void fill_module_names(char *module_buf[], uint8_t *module_buf_size);

/* 设置系统时间戳 */
void syslog_set_time(uint32_t timestamp);

/* 获取系统当前时间戳 */
uint32_t syslog_get_time(void);

/* 设置日志等级 */
void syslog_set_level(enum log_level level);

// 设置日志模块掩码
void set_log_module_mask(uint32_t mask);

// 获取日志模块掩码
uint32_t get_log_module_mask(void);

// 启用全部模块日志
void enable_all_mask(void);

// 日志回调接口

/****************************************关键接口****************************************/

/**
 * @brief 日志初始化
 * 
 * @param f_write 日志输出接口
 * @param period_ms 任务周期（毫秒）
 */
void syslog_init(log_write f_write, uint32_t period_ms);

/**
 * @brief 日志任务
 * 
 */
void syslog_task(void);

/**
 * @brief 申请日志模块掩码
 * 
 * @param module_name 模块名(必须是全局变量)
 * @return uint32_t 
 */
uint32_t allocate_log_mask(const char * const module_name);

/* 日志宏定义 */
// 注意提前通过`allocate_log_mask`获取一个模块掩码
#define log_d(_mask, format, ...) origin_log(_mask, LOG_LEVEL_DEBUG, __LINE__, format, ##__VA_ARGS__) /* 调试日志 */
#define log_i(_mask, format, ...) origin_log(_mask, LOG_LEVEL_INFO, __LINE__, format, ##__VA_ARGS__)  /* 信息日志 */
#define log_w(_mask, format, ...) origin_log(_mask, LOG_LEVEL_WARN, __LINE__, format, ##__VA_ARGS__)  /* 警告日志 */
#define log_e(_mask, format, ...) origin_log(_mask, LOG_LEVEL_ERROR, __LINE__, format, ##__VA_ARGS__) /* 错误日志 */

#endif /* __VIRTUAL_OS_LOG_H__ */
