/**
 * @file virtual_os_mm.h
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief VirtualOS的内存管理(使用BGET组件)
 * @version 1.0
 * @date 2025-03-19
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

#ifndef __VIRTUAL_OS_MM_H
#define __VIRTUAL_OS_MM_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 此接口为VirtualOS的内存管理接口，使用BGET组件
 * 如果未使能宏`VIRTUALOS_ENABLE_BGET`则其中所有的接口实现都
 * 与标准库如malloc一一对应，仅仅是为了兼容接口名
 * 
 * 如果使能了该宏，则VirtualOS会使用BGET组件来管理内存，
 */

/**
 * @brief 初始化内存管理
 * 对给定的堆空间大小，进行一次malloc申请，后续这部分空间都由VirtualOS来管理
 * 如果未使能宏`VIRTUALOS_ENABLE_BGET`则不需要这一步
 * 
 * @param pool_size 内存池大小
 * @return true 成功
 * @return false 失败
 */
bool virtual_os_mm_init(size_t pool_size);

/**
 * @brief 申请内存
 * 
 * @param size 内存大小
 * @return void* 失败返回NULL
 */
void *virtual_os_malloc(size_t size);

/**
 * @brief 申请内存并初始化为0
 * 
 * @param num 元素个数
 * @param per_size 每个元素的大小
 * @return void* 失败返回NULL
 */
void *virtual_os_calloc(size_t num, size_t per_size);

/**
 * @brief 重新分配内存
 * 
 * @param old_ptr 原内存地址
 * @param size 新内存大小
 * @return void* 失败返回NULL
 */
void *virtual_os_realloc(void *old_ptr, size_t size);

/**
 * @brief 释放内存
 * 
 * @param ptr 内存地址
 */
void virtual_os_free(void *ptr);

#endif /* __VIRTUAL_OS_MM_H */