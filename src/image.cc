//
// Created by fish on 2026/3/27.
//

#include "robomaster/robomaster.h"
#include "bsp/time.h"
#include "bsp/sys.h"
#include "utils/crc.h"
#include "utils/logger.h"

#include <cstring>

using namespace robomaster;

namespace robomaster::image {
    bsp_uart_e port;
    frame_header_t header;
    uint8_t rx_stream[BSP_UART_BUFFER_SIZE * 2];
    size_t rx_size;
    namespace rc {
        raw_frame_t raw;
        data_t rc_data_;
    }
    namespace custom {
        uint8_t* ptr; size_t size;
        uint32_t timestamp;
    };
    void callback(bsp_uart_e device, const uint8_t *data, size_t size);
}

const image::rc::data_t *image::rc::data() {
    return &rc_data_;
}

image::rc::data_t image::rc::state() {
    const unsigned long state = bsp_sys_enter_critical();
    const data_t copy = rc_data_;
    bsp_sys_exit_critical(state);
    return copy;
}

void image::init(bsp_uart_e uart) {
    static_assert(sizeof(rc::raw_frame_t) == 21);
    port = uart;
    BSP_ASSERT(bsp_uart_set_baudrate(uart, 921600) == BSP_STATUS_OK);
    BSP_ASSERT(bsp_uart_set_callback(uart, callback) == BSP_STATUS_OK);
}

void image::custom::bind(uint8_t* _ptr, size_t _size) {
    const unsigned long state = bsp_sys_enter_critical();
    ptr = _ptr;
    size = _size;
    bsp_sys_exit_critical(state);
}

uint32_t image::custom::get_timestamp() {
    const unsigned long state = bsp_sys_enter_critical();
    const uint32_t copy = timestamp;
    bsp_sys_exit_critical(state);
    return copy;
}

void image::callback(bsp_uart_e device, const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) return;
    if (size > sizeof(rx_stream)) {
        data += size - sizeof(rx_stream);
        size = sizeof(rx_stream);
        rx_size = 0;
    } else if (size > sizeof(rx_stream) - rx_size) {
        rx_size = 0;
    }
    memcpy(rx_stream + rx_size, data, size);
    rx_size += size;
    data = rx_stream;
    size = rx_size;

    size_t p = 0;
    while (p < size) {
        const size_t remaining = size - p;
        if (remaining == 1 && data[p] == 0xa9) break;
        if (remaining >= 2 && data[p] == 0xa9 && data[p + 1] == 0x53) {
            if (remaining < sizeof(rc::raw_frame_t)) break;
            memcpy(&rc::raw, data+p, sizeof(rc::raw_frame_t));
            if (!crc16::verify(rc::raw)) { p++; continue; }

            rc::rc_data_.l[0] = static_cast<int16_t>(static_cast<int16_t>(rc::raw.ch3) - 1024);
            rc::rc_data_.l[1] = static_cast<int16_t>(static_cast<int16_t>(rc::raw.ch2) - 1024);
            rc::rc_data_.r[0] = static_cast<int16_t>(static_cast<int16_t>(rc::raw.ch0) - 1024);
            rc::rc_data_.r[1] = static_cast<int16_t>(static_cast<int16_t>(rc::raw.ch1) - 1024);
            rc::rc_data_.dial = static_cast<int16_t>(static_cast<int16_t>(rc::raw.dial) - 1024);
            rc::rc_data_.sw = static_cast<int8_t>(rc::raw.sw - 1);

            rc::rc_data_.key_suspend = rc::raw.key_suspend;
            rc::rc_data_.key_l = rc::raw.key_l;
            rc::rc_data_.key_r = rc::raw.key_r;
            rc::rc_data_.key_shoot = rc::raw.key_shoot;

            rc::rc_data_.mouse_x = rc::raw.mouse_x;
            rc::rc_data_.mouse_y = rc::raw.mouse_y;
            rc::rc_data_.mouse_z = rc::raw.mouse_z;
            rc::rc_data_.mouse_l = rc::raw.mouse_l;
            rc::rc_data_.mouse_r = rc::raw.mouse_r;
            rc::rc_data_.mouse_m = rc::raw.mouse_m;
            rc::rc_data_.keyboard = rc::raw.keyboard;

            rc::rc_data_.timestamp = bsp_time_get_ms();

            p += sizeof(rc::raw_frame_t);
        } else if (data[p] == 0xa5) {
            if (remaining < sizeof(frame_header_t)) break;
            memcpy(&header, data + p, sizeof(frame_header_t));
            if (header.sof != 0xa5 or !crc8::verify(header)) { p ++; continue; }
            const size_t frame_size = sizeof(frame_header_t) + 2 + header.data_length + 2;
            if (frame_size > BSP_UART_BUFFER_SIZE) { p++; continue; }
            if (frame_size > remaining) break;

            uint16_t expected_crc;
            memcpy(&expected_crc, data + p + frame_size - sizeof(expected_crc), sizeof(expected_crc));
            if (crc16::calc(data + p, frame_size - sizeof(expected_crc)) != expected_crc)
            {
                p ++;
                continue;
            }

            uint16_t cmd_id;
            memcpy(&cmd_id, data + p + sizeof(frame_header_t), sizeof(cmd_id));
            p += sizeof(frame_header_t) + 2;

            switch(cmd_id) {
            case 0x0302:
                if (custom::ptr != nullptr and custom::size == header.data_length)
                    memcpy(custom::ptr, data + p, custom::size), custom::timestamp = bsp_time_get_ms();
                break;
            default:
                logger::warn("[referee/image] 0x%04X, %d, %d", cmd_id, p, bsp_time_get_ms());
                break;
            }

            p += header.data_length + 2; // data + crc16
        } else {
            p ++;
        }
    }

    if (p > 0) {
        memmove(rx_stream, rx_stream + p, rx_size - p);
        rx_size -= p;
    }
}
