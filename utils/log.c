/**
 * @file log.c
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "utils/log.h"
#include "utils/queue.h"

#if USE_TIME_STAMP
#include <time.h>
#endif

#define LOG_LEVEL_STR(level)                                                                                           \
	((level) == LOG_LEVEL_DEBUG			 ? "DEBUG"                                                                     \
			: (level) == LOG_LEVEL_INFO	 ? "INFO"                                                                      \
			: (level) == LOG_LEVEL_WARN	 ? "WARN"                                                                      \
			: (level) == LOG_LEVEL_ERROR ? "ERROR"                                                                     \
										 : "XXXXX")

static uint8_t log_buffer[LOG_BUFFER_SIZE]; /* 日志缓冲区 */

struct syslog_instance {
	log_write f_write;				  // 输出接口
	struct queue_info log_queue;	  // 日志队列
	uint32_t timestamp;				  // 时间戳
	uint32_t pre_time;				  // 时间戳计数
	uint32_t period_md;				  // 任务周期
	bool initialized;				  // 是否初始化
	enum log_level current_log_level; // 当前日志等级
	uint32_t module_mask;			  // 模块掩码 用于过滤日志
	uint8_t module_cnt;				  // 最多支持32个模块
};

static const char *module_info[32] = { 0 }; // 所有模块信息

static inline uint8_t mask_idx(uint32_t mask)
{
	uint8_t idx = 0;
	while (mask) {
		if (mask & 1)
			return idx;
		mask >>= 1;
		idx++;
	}
}

static struct syslog_instance syslog = { 0 };

/**
 * @brief 检查日志实例是否有效
 * 
 * @param instance 日志实例指针
 * @return true 有效
 * @return false 无效
 */
static bool check_instance(struct syslog_instance *instance)
{
	return instance && instance->initialized && instance->f_write;
}

/**
 * @brief 向日志队列中写入日志
 * 
 * @param instance 日志实例
 * @param buf 日志内容缓冲区
 * @param len 日志内容长度
 * @param mask 模块掩码
 * @return size_t 实际写入的字节数
 */
static size_t syslog_write(struct syslog_instance *instance, uint8_t *buf, size_t len, uint32_t mask)
{
	if (!check_instance(instance))
		return 0;

#if USE_TIME_STAMP
	char time_buffer[64] = "NO_TIME";

	time_t raw_time = (time_t)instance->timestamp;
	struct tm time_info;

	if (localtime_r(&raw_time, &time_info) != NULL) {
		strftime(time_buffer, sizeof(time_buffer), "[%Y-%m-%d %H:%M:%S]", &time_info);
	} else {
		snprintf(time_buffer, sizeof(time_buffer), "[NO_TIME]");
	}

	char new_buf[MAX_LOG_LENGTH];
	size_t new_len = snprintf(new_buf, sizeof(new_buf), "%s %.*s", time_buffer, (int)len, buf);

	if (new_len >= MAX_LOG_LENGTH) {
		// 如果日志长度超过最大长度，截断
		new_len = MAX_LOG_LENGTH - 1;
		// 移除手动添加 '\0'
	}

	// 使用 new_buf 作为要发送的日志内容
	buf = (uint8_t *)new_buf;
	len = new_len;
#endif

	size_t total_len = len + sizeof(size_t) + sizeof(uint32_t); // 日志长度信息占用 4 字节 掩码信息占用 4 字节
	size_t remain_space = queue_remain_space(&instance->log_queue);

	if (remain_space < total_len)
		return 0;

	// 长度信息
	if (queue_add(&instance->log_queue, (uint8_t *)&len, sizeof(size_t)) != sizeof(size_t))
		return 0;

	// 掩码信息
	if (queue_add(&instance->log_queue, (uint8_t *)&mask, sizeof(uint32_t)) != sizeof(uint32_t))
		return 0;

	// 日志内容
	if (queue_add(&instance->log_queue, buf, len) != len)
		return 0;

	return len;
}

/**
 * @brief 日志显示
 * 
 * @param instance 任务实例
 */
static void syslog_show(struct syslog_instance *instance)
{
	if (!check_instance(instance))
		return;

#if USE_TIME_STAMP
	instance->pre_time += instance->period_md;

	if (instance->pre_time >= 1000) {
		instance->timestamp++;
		instance->pre_time = 0;
	}
#endif

	while (!is_queue_empty(&instance->log_queue)) {
		// 日志长度信息
		size_t flush_len = 0;
		queue_get(&instance->log_queue, (uint8_t *)&flush_len, sizeof(size_t));

		if (flush_len == 0 || flush_len > MAX_LOG_LENGTH)
			return;

		// 掩码信息
		uint32_t mask = 0;
		queue_get(&instance->log_queue, (uint8_t *)&mask, sizeof(uint32_t));

		// 取出日志
		uint8_t tmp_buf[MAX_LOG_LENGTH];
		queue_get(&instance->log_queue, tmp_buf, flush_len);
		if (syslog.module_mask & mask)
			instance->f_write(tmp_buf, flush_len); // 过滤掩码
	}
}

/**
 * @brief 填充模块名称数组 按照掩码从小到大排序 模块名的索引就是实际的掩码位
 * 		  结束后 module_buf_size 指向实际填充的模块个数
 * @param module_buf 
 * @param module_buf_size 
 */
void fill_module_names(char *module_buf[], uint8_t *module_buf_size)
{
	size_t less = *module_buf_size < syslog.module_cnt ? *module_buf_size : syslog.module_cnt;
	*module_buf_size = less;
	for (size_t i = 0; i < less; i++)
		module_buf[i] = (char *)module_info[i];
}

/**
  * @brief 日志发送 可以通过掩码过滤日志
  * 
  * @param mask 模块掩码
  * @param level 日志等级
  * @param line 行号
  * @param format 日志格式
  * @param ... 可变参数
  */
void origin_log(uint32_t mask, enum log_level level, int line, const char *format, ...)
{
	if (!check_instance(&syslog))
		return;

	if (level < syslog.current_log_level)
		return;

	char buffer[MAX_LOG_LENGTH] = { 0 };
	int len;
	va_list args;

	va_start(args, format);

	uint8_t module_idx = mask_idx(mask);
	if (module_idx >= syslog.module_cnt)
		return;

	len = snprintf(
		buffer, sizeof(buffer), "[%-5s] [%-10s] [%-4d] : ", LOG_LEVEL_STR(level), module_info[module_idx], line);

	if (len < 0) {
		// snprintf 出错
		va_end(args);
		return;
	}

	int remaining_space = MAX_LOG_LENGTH - len;

	if (remaining_space > 1) {
		int formatted_len = vsnprintf(buffer + len, remaining_space, format, args);
		if (formatted_len < 0)
			len = MAX_LOG_LENGTH - 1;
		else if (formatted_len >= remaining_space)
			len = MAX_LOG_LENGTH - 1;
		else
			len += formatted_len;
	} else
		len = MAX_LOG_LENGTH - 1;

	va_end(args);

	syslog_write(&syslog, (uint8_t *)buffer, len, mask);
}

// 设置日志模块掩码
void set_log_module_mask(uint32_t mask)
{
	syslog.module_mask = mask;
}

// 获取日志模块掩码
uint32_t get_log_module_mask(void)
{
	return syslog.module_mask;
}

// 启用全部模块日志
void enable_all_mask(void)
{
	for (size_t i = 0; i < syslog.module_cnt; i++) {
		uint32_t mask = 1 << i;
		syslog.module_mask |= mask;
	}
}

/**
  * @brief 设置日志等级
  * 
  * @param level 日志等级
  */
void syslog_set_level(enum log_level level)
{
	if (check_instance(&syslog))
		syslog.current_log_level = level;
}

/* 设置系统时间戳 */
void syslog_set_time(uint32_t timestamp)
{
	if (check_instance(&syslog))
		syslog.timestamp = timestamp;
}

/* 获取系统当前时间戳 */
uint32_t syslog_get_time(void)
{
	if (check_instance(&syslog))
		return syslog.timestamp;
	return 0;
}

void modify_output(log_write f_write)
{
	syslog.f_write = f_write;
}
/**************************API**************************/

/**
 * @brief 日志初始化
 * 
 * @param f_write 日志输出接口
 * @param period_ms 任务周期（毫秒）
 */
void syslog_init(log_write f_write, uint32_t period_ms)
{
	if (!f_write)
		return;

	syslog.f_write = f_write;
	syslog.current_log_level = LOG_LEVEL_INFO; // 默认日志等级为INFO

	queue_init(&syslog.log_queue, sizeof(uint8_t), log_buffer, LOG_BUFFER_SIZE);

	syslog.initialized = true;
}

/**
  * @brief 日志任务
  * 
  */
void syslog_task(void)
{
	syslog_show(&syslog);
}

/**
 * @brief 申请日志模块掩码
 * 
 * @param module_name 模块名(必须是全局变量)
 * @return uint32_t 
 */
uint32_t allocate_log_mask(const char * const module_name)
{
	if (syslog.module_cnt >= 32)
		syslog.module_cnt = 31; // 最多支持32个模块 覆盖最后一个

	uint32_t mask = 1 << syslog.module_cnt;
	syslog.module_mask |= mask;
	module_info[syslog.module_cnt] = module_name;
	syslog.module_cnt++;
	return mask;
}
