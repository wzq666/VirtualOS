/**
 * @file virtual_os_mm.c
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

#include <stdlib.h>

#include "core/virtual_os_config.h"
#include "core/virtual_os_mm.h"

#include "core/lib/bget.h"

#if VIRTUALOS_ENABLE_BGET

#endif

/**
 * @brief 初始化内存管理
 * 对给定的堆空间大小，进行一次malloc申请，后续这部分空间都由VirtualOS来管理
 * 
 * @param pool_size 内存池大小
 * @return true 成功
 * @return false 失败
 */
bool virtual_os_mm_init(size_t pool_size)
{
#if VIRTUALOS_ENABLE_BGET
	void *p = malloc(pool_size);
	if (!p)
		return false;

	bpool(p, pool_size);

	return true;
#else
	return true;
#endif
}

/**
 * @brief 申请内存
 * 
 * @param size 内存大小
 * @return void* 失败返回NULL
 */
void *virtual_os_malloc(size_t size)
{
#if VIRTUALOS_ENABLE_BGET
	return bget(size);
#else
	return malloc(size);
#endif
}

/**
 * @brief 申请内存并初始化为0
 * 
 * @param num 元素个数
 * @param per_size 每个元素的大小
 * @return void* 失败返回NULL
 */
void *virtual_os_calloc(size_t num, size_t per_size)
{
#if VIRTUALOS_ENABLE_BGET
	return bgetz(num * per_size);
#else
	return calloc(num, per_size);
#endif
}

/**
 * @brief 重新分配内存
 * 
 * @param old_ptr 原内存地址
 * @param size 新内存大小
 * @return void* 失败返回NULL
 */
void *virtual_os_realloc(void *old_ptr, size_t size)
{
#if VIRTUALOS_ENABLE_BGET
	return bgetr(old_ptr, size);
#else
	return realloc(old_ptr, size);
#endif
}

/**
 * @brief 释放内存
 * 
 * @param ptr 内存地址
 */
void virtual_os_free(void *ptr)
{
#if VIRTUALOS_ENABLE_BGET
	brel(ptr);
#else
	free(ptr);
#endif
}
