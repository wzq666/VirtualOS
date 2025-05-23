/**
 * @file modbus_master.c
 * @author wenshuyu (wsy2161826815@163.com)
 * @brief modbus主机协议
 * @version 1.0
 * @date 2024-12-18
 * 
 * @copyright Copyright (c) 2024-2025
 * @see repository: https://github.com/i-tesetd-it-no-problem/VirtualOS.git
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, virtual_os_free of charge, to any person obtaining a copy
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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/crc.h"
#include "utils/queue.h"

#include "core/virtual_os_mm.h"

#include "protocol/modbus/modbus_master.h"

#define NO_RETRIES (1) // 如果是1表示禁用重发，0表示启用重发

#define REPEAT_IDX (0)	// 重发
#define TIMEOUT_IDX (1) // 超时
#define DOING_IDX (2)	// 正在处理
#define REG_LEN_IDX (3) // 写寄存器长度

// 接收状态
enum rx_state {
	RX_STATE_ADDR = 0, // 从机地址
	RX_STATE_FUNC,	   // 功能码

	RX_STATE_ERR, // 异常响应

	RX_STATE_DATA_LEN, // 数据长度      (仅对 读 功能玛有效)
	RX_STATE_DATA,	   // 数据内容      (仅对 读 功能玛有效)

	RX_STATE_REG,	  // 寄存器地址    (仅对 写 功能玛有效)
	RX_STATE_REG_LEN, // 寄存器长度    (仅对 写 功能玛有效)

	RX_STATE_CRC, // CRC校验
};
// 两倍最大帧长度 接收缓冲
#define RX_BUFF_SIZE (MODBUS_FRAME_BYTES_MAX * 2)

// 最大请求处理个数
#define MAX_REQUEST (32)
#define REQUEST_BUFFER (MAX_REQUEST * sizeof(struct mb_mst_request *))

// 请求信息
struct req_info {
	struct mb_mst_request request; // 请求信息

	uint32_t to_timeout;  // 超时时间
	uint32_t cur_ctr;	  // 当前计数器
	uint8_t repeat_times; // 重发次数
	uint8_t reg_len;	  // 写功能码时的寄存器长度

	bool valid; // 是否正在使用
};

// 接收数据信息
struct msg_info {
	uint8_t r_data[MAX_READ_REG_NUM * 2]; // 读功能码接收的有效数据

	uint8_t rx_queue_buff[RX_BUFF_SIZE]; // 接收队列缓冲
	struct queue_info rx_q;				 // 接收队列

	uint16_t wr_reg_data[MAX_WRITE_REG_NUM]; // 写功能码发送的有效数据
	struct queue_info wr_q;					 // 写数据队列 临时存储写功能的数据

	struct req_info req_infos[MAX_REQUEST];		   // 请求信息缓冲
	struct req_info *req_infos_q_buf[MAX_REQUEST]; // 请求信息队列指针缓冲
	struct queue_info req_info_q;				   // 请求信息队列

	size_t anchor;	// 滑动左窗口
	size_t forward; // 滑动右窗口

	uint16_t cal_crc;						// 计算的CRC
	uint8_t recv_crc[MODBUS_CRC_BYTES_NUM]; // 接收的CRC

	uint8_t pdu_in;		// 接收索引
	uint8_t pdu_len;	// 接收长度
	uint8_t err_code;	// 异常响应码
	uint8_t r_data_len; // 有效数据长度

	enum rx_state state; // 当前接收状态
};

// 主机句柄
struct mb_mst {
	struct serial_opts *opts;  // 用户回调指针
	struct msg_info msg_state; // 接收信息
	size_t period_ms;		   // 任务周期
	uint8_t sem; // 信号量(模拟) 必须确保一收一发或者接收超时后才能继续发送 因为需要切换收发引脚 防止一直在发
};

static struct req_info *allow_req_info(mb_mst_handle handle)
{
	if (!handle)
		return NULL;

	for (size_t i = 0; i < MAX_REQUEST; ++i) {
		if (!handle->msg_state.req_infos[i].valid) {
			handle->msg_state.req_infos[i].valid = true;
			return &handle->msg_state.req_infos[i];
		}
	}
	return NULL;
}

static bool _recv_parser(mb_mst_handle handle);		 // 解析数据
static void _dispatch_rtu_msg(mb_mst_handle handle); // 处理数据

// 剩余数据
static inline size_t check_rx_queue_remain_data(const struct msg_info *p_msg)
{
	return (p_msg->rx_q.wr - p_msg->forward);
}

// 获取队首数据
static inline uint8_t get_rx_queue_remain_data(const struct msg_info *p_msg)
{
	uint8_t *p = p_msg->rx_q.buf;
	return p[p_msg->forward & (p_msg->rx_q.buf_size - 1)];
}

/**
 * @brief 右移左滑动窗口
 * 
 * @param p_msg 
 */
static void rebase_parser(struct msg_info *p_msg)
{
	p_msg->state = RX_STATE_ADDR;
	p_msg->err_code = MODBUS_RESP_ERR_NONE;
	p_msg->rx_q.rd = p_msg->anchor + 1;

	p_msg->anchor = p_msg->rx_q.rd;
	p_msg->forward = p_msg->rx_q.rd;
}

/**
 * @brief 刷新解析状态
 * 
 * @param p_msg 
 */
static void flush_parser(struct msg_info *p_msg)
{
	p_msg->state = RX_STATE_ADDR;
	p_msg->rx_q.rd = p_msg->forward;
	p_msg->anchor = p_msg->rx_q.rd;
}

/**
 * @brief 检查请求包是否合法
 * 
 * @param request 
 * @return true 
 * @return false 
 */
static bool check_request_valid(struct mb_mst_request *request)
{
	// 空指针 寄存器范围过大 未设置超时时间
	if (!request || !CHECK_REG_NUM_VALID(request->reg_len, request->func) || !request->timeout_ms)
		return false;

	return true;
}

/**
 * @brief 解析协议数据帧, 支持粘包断包处理
 * 
 * @param handle 主机句柄
 * @return true 解析成功
 * @return false 解析失败
 */
static bool _recv_parser(mb_mst_handle handle)
{
	// 无请求不接收
	if (!handle)
		return false;

	uint8_t c;

	struct msg_info *p_msg = &handle->msg_state;
	struct req_info *req_info = NULL;
	queue_peek(&handle->msg_state.req_info_q, (uint8_t *)&req_info, 1); // 不出队 只查询

	while (check_rx_queue_remain_data(p_msg)) {
		c = get_rx_queue_remain_data(p_msg);
		++p_msg->forward;
		switch (p_msg->state) {
		case RX_STATE_ADDR:
			if (req_info->request.slave_addr == c) {
				p_msg->state = RX_STATE_FUNC;
				p_msg->cal_crc = crc16_update(0xffff, c);
			} else
				rebase_parser(p_msg);
			break;
		case RX_STATE_FUNC:
			if (c == MODBUS_FUN_RD_REG_MUL) {
				p_msg->state = RX_STATE_DATA_LEN;
				p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);

			} else if (c == MODBUS_FUN_WR_REG_MUL) {
				p_msg->pdu_in = 0;
				p_msg->pdu_len = MODBUS_REG_BYTES_NUM;
				p_msg->state = RX_STATE_REG;
				p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			} else if (c & 0x80) {
				// 异常响应
				p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
				p_msg->state = RX_STATE_ERR;

			} else
				rebase_parser(p_msg);
			break;

		case RX_STATE_ERR:
			p_msg->err_code = c; // 异常响应码
			p_msg->pdu_in = 0;
			p_msg->pdu_len = MODBUS_CRC_BYTES_NUM;
			p_msg->state = RX_STATE_CRC;
			p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			break;

		case RX_STATE_DATA_LEN:
			if (c > MAX_READ_REG_NUM * 2) {
				rebase_parser(p_msg);
				break;
			}
			p_msg->pdu_in = 0;
			p_msg->pdu_len = c;
			p_msg->state = RX_STATE_DATA;
			p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			memset(p_msg->r_data, 0, sizeof(p_msg->r_data));
			break;

		case RX_STATE_DATA:
			p_msg->r_data[p_msg->pdu_in++] = c;
			p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			if (p_msg->pdu_in >= p_msg->pdu_len) {
				p_msg->r_data_len = p_msg->pdu_in;
				p_msg->pdu_in = 0;
				p_msg->pdu_len = MODBUS_CRC_BYTES_NUM;
				p_msg->state = RX_STATE_CRC;
			}

			break;

		case RX_STATE_REG:
			p_msg->pdu_in++;
			p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			if (p_msg->pdu_in >= p_msg->pdu_len) {
				p_msg->pdu_in = 0;
				p_msg->pdu_len = MODBUS_REG_LEN_BYTES_NUM;
				p_msg->state = RX_STATE_REG_LEN;
			}

			break;

		case RX_STATE_REG_LEN:
			p_msg->pdu_in++;
			p_msg->cal_crc = crc16_update(p_msg->cal_crc, c);
			if (p_msg->pdu_in >= p_msg->pdu_len) {
				p_msg->pdu_in = 0;
				p_msg->pdu_len = MODBUS_CRC_BYTES_NUM;
				p_msg->state = RX_STATE_CRC;
			}
			break;

		case RX_STATE_CRC:
			p_msg->recv_crc[p_msg->pdu_in++] = c;
			if (p_msg->pdu_in >= p_msg->pdu_len) {
				uint16_t recv_crc = COMBINE_U8_TO_U16(p_msg->recv_crc[1], p_msg->recv_crc[0]);
				if (p_msg->cal_crc == recv_crc) {
					flush_parser(p_msg);
					return true;
				} else
					rebase_parser(p_msg);
			}
			break;
		default:
			break;
		}
	}
	return false;
}

/**
 * @brief 处理对应功能码
 *
 * @param handle 主机句柄
 */
static void _dispatch_rtu_msg(mb_mst_handle handle)
{
	if (!handle || is_queue_empty(&handle->msg_state.req_info_q))
		return;

	struct req_info *req_info_ptr = NULL;
	queue_get(&handle->msg_state.req_info_q, &req_info_ptr, 1); // 出队

	uint16_t wr_tmp_buf[MAX_WRITE_REG_NUM] = { 0 };
	queue_get(&handle->msg_state.wr_q, wr_tmp_buf, req_info_ptr->reg_len); // 出队

	// 正常用户回调(未超时)
	if (req_info_ptr->request.resp) {
		req_info_ptr->request.resp(
			handle->msg_state.r_data, handle->msg_state.r_data_len, handle->msg_state.err_code, false);
	}

	// 清空请求信息
	if (req_info_ptr)
		memset(req_info_ptr, 0, sizeof(struct req_info));

	handle->msg_state.err_code = MODBUS_RESP_ERR_NONE; // 异常响应码清零
}

/**
 * @brief 检查当前请求回复是否超时
 * 
 * @param handle 主机句柄
 * @param request 请求包
 * @return true 超时
 * @return false 未超时
 */
static bool check_timeout(size_t period, struct req_info *req_info_ptr)
{
	if (!req_info_ptr)
		return true;

	req_info_ptr->cur_ctr += period; // 累加超时时间
	if (req_info_ptr->cur_ctr > req_info_ptr->to_timeout) {
		req_info_ptr->cur_ctr = 0;	  // 超时后重置计时器
		req_info_ptr->repeat_times++; // 增加重发次数标志
		return true;				  // 超时
	}

	return false; // 未超时
}

/**
 * @brief 发送请求
 * 
 * @param handle 主机句柄
 * @param request 请求包
 */
static void _request_pdu(mb_mst_handle handle, struct req_info *req_info_ptr)
{
	if (!handle || !check_request_valid(&req_info_ptr->request))
		return;

	uint8_t temp_buf[256] = { 0 };
	uint16_t idx = 0;
	temp_buf[idx++] = req_info_ptr->request.slave_addr;
	temp_buf[idx++] = req_info_ptr->request.func;

	temp_buf[idx++] = GET_U8_HIGH_FROM_U16(req_info_ptr->request.reg_addr);
	temp_buf[idx++] = GET_U8_LOW_FROM_U16(req_info_ptr->request.reg_addr);
	temp_buf[idx++] = GET_U8_HIGH_FROM_U16(req_info_ptr->request.reg_len);
	temp_buf[idx++] = GET_U8_LOW_FROM_U16(req_info_ptr->request.reg_len);

	// 写功能玛
	if (req_info_ptr->request.func == MODBUS_FUN_WR_REG_MUL) {
		temp_buf[idx++] = req_info_ptr->request.reg_len << 1;

		uint16_t wr_tmp_buf[MAX_WRITE_REG_NUM] = { 0 };
		queue_peek(&handle->msg_state.wr_q, wr_tmp_buf, req_info_ptr->reg_len); // 拷贝写数据内容 这里不一定会出队

		for (uint8_t i = 0; i < req_info_ptr->request.reg_len; i++) {
			temp_buf[idx++] = GET_U8_HIGH_FROM_U16(wr_tmp_buf[i]);
			temp_buf[idx++] = GET_U8_LOW_FROM_U16(wr_tmp_buf[i]);
		}
	}

	uint16_t crc = crc16_update_bytes(0xFFFF, temp_buf, idx);

	temp_buf[idx++] = GET_U8_LOW_FROM_U16(crc);
	temp_buf[idx++] = GET_U8_HIGH_FROM_U16(crc);

	handle->opts->f_write(temp_buf, idx);

	// 用户自行在应用中切换为接收模式，如在发送完成中断中切换
}

/**
 * @brief 从发送队列中取出数据, 并发送
 * 
 * @param handle 
 */
static void master_write(mb_mst_handle handle)
{
	if (!handle || is_queue_empty(&handle->msg_state.req_info_q))
		return;

	struct req_info *req_info_ptr = NULL;
	queue_peek(&handle->msg_state.req_info_q, &req_info_ptr, 1); // 不出队，只查看

	uint16_t wr_tmp_buf[MAX_WRITE_REG_NUM] = { 0 };
	queue_peek(&handle->msg_state.wr_q, wr_tmp_buf, req_info_ptr->reg_len); // 拷贝写数据内容 这里不一定会出队

	// 不重发
	if (NO_RETRIES) {
		// 初始包直接发
		if (req_info_ptr->cur_ctr == 0 && handle->sem) {
			handle->sem--;								// 获取信号量
			req_info_ptr->cur_ctr += handle->period_ms; // 累加超时时间
			req_info_ptr->repeat_times++;				// 增加重发次数标志
			_request_pdu(handle, req_info_ptr);			// 发送第一包请求
			return;
		} else if (check_timeout(handle->period_ms, req_info_ptr)) {
			// 超时后从队列中移除

			struct req_info *req_info_ptr = NULL;
			queue_get(&handle->msg_state.req_info_q, &req_info_ptr, 1);

			uint16_t wr_tmp_buf[MAX_WRITE_REG_NUM] = { 0 };
			queue_get(&handle->msg_state.wr_q, wr_tmp_buf, req_info_ptr->reg_len);

			// 用户回调（超时）
			if (req_info_ptr->request.resp)
				req_info_ptr->request.resp(
					handle->msg_state.r_data, handle->msg_state.r_data_len, MODBUS_RESP_ERR_NONE, true);

			if (req_info_ptr)
				memset(req_info_ptr, 0, sizeof(struct req_info));

			handle->sem++; // 释放信号量
		}
	} else {
		if (req_info_ptr->repeat_times < MASTER_REPEATS) {
			// 未重发完

			// 初始包直接发
			if (req_info_ptr->cur_ctr == 0 && !handle->sem) {
				handle->sem++;
				req_info_ptr->cur_ctr += handle->period_ms; // 累加超时时间
				req_info_ptr->repeat_times++;				// 增加重发次数标志
				_request_pdu(handle, req_info_ptr);			// 发送第一包请求
				return;
			} else {
				check_timeout(handle->period_ms, req_info_ptr); // 检查超时
			}
		} else {
			// 重发完后 从队列中移除
			struct req_info *req_info_ptr = NULL;
			queue_get(&handle->msg_state.req_info_q, &req_info_ptr, 1);

			uint16_t wr_tmp_buf[MAX_WRITE_REG_NUM] = { 0 };
			queue_get(&handle->msg_state.wr_q, wr_tmp_buf, req_info_ptr->reg_len);

			// 用户回调（超时）
			if (req_info_ptr->request.resp)
				req_info_ptr->request.resp(
					handle->msg_state.r_data, handle->msg_state.r_data_len, MODBUS_RESP_ERR_NONE, true);

			if (req_info_ptr)
				memset(req_info_ptr, 0, sizeof(struct req_info));

			handle->sem--;
		}
	}
}

static void master_read(mb_mst_handle handle)
{
	if (!handle || is_queue_empty(&handle->msg_state.req_info_q))
		return;

	size_t ptk_len; // 响应长度

	uint8_t temp_buf[MODBUS_FRAME_BYTES_MAX] = { 0 };

	ptk_len = handle->opts->f_read(temp_buf, MODBUS_FRAME_BYTES_MAX);
	if (!ptk_len) // 无数据
		return;

	size_t ret_q = queue_add(&(handle->msg_state.rx_q), temp_buf, ptk_len);
	if (ret_q != ptk_len)
		return; // 空间不足

	bool ret_parser = _recv_parser(handle); // 解析数据帧

	if (!ret_parser)
		return;

	_dispatch_rtu_msg(handle); // 处理数据帧, 调用用户回调注册表

	handle->sem++; // 释放信号量
}

/***************************API***************************/

/**
 * @brief 主机初始化并申请句柄
 *
 * @param opts 读写等回调函数指针
 * @param period_ms 轮训周期
 * @return mb_mst_handle 成功返回句柄,失败返回NULL
 */
mb_mst_handle mb_mst_init(struct serial_opts *opts, size_t period_ms)
{
	bool ret = false;

	if (!opts || !opts->f_init || !opts->f_read || !opts->f_write)
		return NULL;

	struct mb_mst *handle = virtual_os_calloc(1, sizeof(struct mb_mst));
	if (!handle)
		return NULL;

	handle->opts = opts;
	handle->period_ms = period_ms;
	handle->sem = 1;

	// 接收队列
	ret = queue_init(&handle->msg_state.rx_q, sizeof(uint8_t), handle->msg_state.rx_queue_buff, RX_BUFF_SIZE);
	if (!ret) {
		virtual_os_free(handle);
		return NULL;
	}

	// 写功能码缓冲区
	ret = queue_init(&handle->msg_state.wr_q, sizeof(uint16_t), handle->msg_state.wr_reg_data, MAX_WRITE_REG_NUM);
	if (!ret) {
		virtual_os_free(handle);
		return NULL;
	}

	// 请求信息缓冲区
	ret = queue_init(
		&handle->msg_state.req_info_q, sizeof(struct req_info *), handle->msg_state.req_infos_q_buf, MAX_REQUEST);
	if (!ret) {
		virtual_os_free(handle);
		return NULL;
	}

	// 用户串口初始化
	ret = opts->f_init();
	if (!ret) {
		virtual_os_free(handle);
		return NULL;
	}

	return handle;
}

/**
 * @brief 释放主机句柄
 *
 * @param handle
 */
void mb_mst_destroy(mb_mst_handle handle)
{
	if (!handle)
		return;

	virtual_os_free(handle);
}

/**
 * @brief modbus主机轮询函数
 *
 * @param handle 主机句柄
 */
void mb_mst_poll(mb_mst_handle handle)
{
	if (!handle)
		return;

	master_read(handle); // 接收处理

	master_write(handle); // 发送处理
}

/**
 * @brief 
 * 
 * @param handle 主机句柄
 * @param request 请求包
 * @param reg_data 寄存器数据 仅在request的功能码为写请求时有效
 * @param reg_len 寄存器长度 仅在request的功能码为写请求时有效
 */
void mb_mst_pdu_request(mb_mst_handle handle, struct mb_mst_request *request, uint16_t *reg_data, uint8_t reg_len)
{
	if (!handle || !check_request_valid(request))
		return;

	// 写请求时 写的寄存器长度要等于请求的寄存器长度
	if (request->func == MODBUS_FUN_WR_REG_MUL && reg_len != request->reg_len)
		return;

	// 写请求时 寄存器数据有效
	if (request->func == MODBUS_FUN_WR_REG_MUL && (reg_len > MAX_WRITE_REG_NUM || reg_len == 0 || !reg_data))
		return;

	// 申请一个空闲的请求信息
	struct req_info *new_req_info = allow_req_info(handle);
	if (!new_req_info)
		return;

	memset(new_req_info, 0, sizeof(struct req_info));
	memcpy(&new_req_info->request, request, sizeof(struct mb_mst_request));
	new_req_info->cur_ctr = 0;
	new_req_info->repeat_times = 0;
	new_req_info->to_timeout = request->timeout_ms;
	new_req_info->reg_len = reg_len;
	new_req_info->valid = true;

	queue_add(&handle->msg_state.req_info_q, &new_req_info, 1); // 保存请求信息
	queue_add(&handle->msg_state.wr_q, reg_data, reg_len);		// 拷贝写数据内容
}
