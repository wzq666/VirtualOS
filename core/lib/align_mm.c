/**
 * @file align_mm.c
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

#include <stdlib.h>
#include "core/lib/align_mm.h"

#define IS_POWER_OF_TWO(align) ((align) != 0 && ((align) & ((align) - 1)) == 0)
#define EXTRA_MEMORY(size, align) ((size) + (align) - 1 + sizeof(void *))
#define ALIGN_UP(ptr, align) ((void *)(((size_t)(ptr) + (align) - 1 + sizeof(void *)) & ~((align) - 1)))

/**
 * @brief 内存对齐分配
 * 
 * @param size 申请大小
 * @param align 对齐大小
 * @return void* 对齐后的指针，失败返回NULL
 */
void *aligned_malloc(size_t size, size_t align)
{
	if (!IS_POWER_OF_TWO(align) || align < sizeof(void *))
		return NULL;

	if (size == 0)
		return NULL;

	size_t alloc_size = EXTRA_MEMORY(size, align);
	void *ptr = malloc(alloc_size);
	if (ptr == NULL)
		return NULL;

	void *aligned_ptr = ALIGN_UP(ptr, align);
	((void **)aligned_ptr)[-1] = ptr;

	return aligned_ptr;
}

/**
 * @brief 内存对齐释放
 * 
 * @param ptr 通过aligned_malloc申请的指针
 */
void aligned_free(void *ptr)
{
	if (ptr == NULL)
		return;

	if (((size_t)ptr & (sizeof(void *) - 1)) != 0)
		return;

	void *real_ptr = ((void **)ptr)[-1];
	free(real_ptr);
}