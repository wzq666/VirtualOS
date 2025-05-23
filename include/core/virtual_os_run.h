/**
 * @file virtual_os_run.h
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief 系统宏定义
 * @version 1.0
 * @date 2024-08-21
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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#ifndef __VIRTUAL_OS_RUN_H__
#define __VIRTUAL_OS_RUN_H__

#include <stddef.h>

#include "core/virtual_os_config.h"
#include "utils/stimer.h"

#if VIRTUALOS_ENABLE_BGET

/**
 * @brief 框架调度初始化 如果启用VirtualOS的内存管理则需要提供一个内存池大小
 * 
 * @param port 时钟配置 详细参考`utils/stimer.h`
 * @param poll_size 内存池大小 通常是RAM剩余的大小范围内
 */
void virtual_os_init(struct timer_port *port, size_t poll_size);

#else

/**
 * @brief 框架调度初始化
 * 
 * @param port 时钟配置 详细参考`utils/stimer.h`
 */
void virtual_os_init(struct timer_port *port);

#endif

#endif /* __VIRTUAL_OS_RUN_H__ */