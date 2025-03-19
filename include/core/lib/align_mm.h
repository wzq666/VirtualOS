/**
 * @file align_mm.h
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief 对齐内存接口
 * @version 1.0
 * @date 2025-03-18
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

#ifndef __VIRTUAL_OS_ALIGH_MM_H__
#define __VIRTUAL_OS_ALIGH_MM_H__

#include <stddef.h>

/**
 * @brief 内存对齐分配
 * 
 * @param size 申请大小
 * @param align 对齐大小
 * @return void* 对齐后的指针，失败返回NULL
 */
void *aligned_malloc(size_t size, size_t align);

/**
 * @brief 内存对齐释放
 * 
 * @param ptr 通过aligned_malloc申请的指针
 */
void aligned_free(void *ptr);

#endif /* __VIRTUAL_OS_ALIGH_MM_H__ */
