#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "bsp/board_api.h"
#include "tusb.h"

#define TFT_SPI spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_RST 20
#define PIN_DC 21
#define PIN_CS 17
#define PIN_BL 16

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_GREEN 0x07E0
#define COLOR_YELLOW 0xFFE0
#define COLOR_CYAN 0x07FF
#define COLOR_RED 0xF800

static uint8_t midi_dev_idx = TUSB_INDEX_INVALID_8;
static bool screen_dirty = true;
static absolute_time_t last_render;

static char line_type[24] = "WAITING FOR MIDI";
static char line_one[24] = "";
static char line_two[24] = "";
static char line_three[24] = "";

static inline void cs_low(void) { gpio_put(PIN_CS, 0); }
static inline void cs_high(void) { gpio_put(PIN_CS, 1); }

static void write_cmd(uint8_t value) {
    gpio_put(PIN_DC, 0);
    cs_low();
    spi_write_blocking(TFT_SPI, &value, 1);
    cs_high();
}

static void write_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_DC, 1);
    cs_low();
    spi_write_blocking(TFT_SPI, data, len);
    cs_high();
}

static void cmd_data(uint8_t cmd, const uint8_t *data, size_t len) {
    write_cmd(cmd);
    if (len) write_data(data, len);
}

static void st7789_init(void) {
    gpio_put(PIN_RST, 1);
    sleep_ms(10);
    gpio_put(PIN_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);

    write_cmd(0x01);
    sleep_ms(150);
    write_cmd(0x11);
    sleep_ms(120);

    const uint8_t colmod[] = {0x55};
    cmd_data(0x3A, colmod, sizeof colmod);
    const uint8_t madctl[] = {0x00};
    cmd_data(0x36, madctl, sizeof madctl);

    write_cmd(0x21);
    write_cmd(0x13);
    sleep_ms(10);
    write_cmd(0x29);
    sleep_ms(120);
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t cols[] = {x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff};
    const uint8_t rows[] = {y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff};
    cmd_data(0x2A, cols, sizeof cols);
    cmd_data(0x2B, rows, sizeof rows);
    write_cmd(0x2C);
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!w || !h || x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    if (x + w > TFT_WIDTH) w = TFT_WIDTH - x;
    if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;

    set_window(x, y, x + w - 1, y + h - 1);

    enum { PIXELS_PER_CHUNK = 128 };
    uint8_t buf[PIXELS_PER_CHUNK * 2];
    const uint8_t hi = color >> 8;
    const uint8_t lo = color & 0xff;
    for (size_t i = 0; i < PIXELS_PER_CHUNK; ++i) {
        buf[i * 2] = hi;
        buf[i * 2 + 1] = lo;
    }

    uint32_t remaining = (uint32_t)w * h;
    gpio_put(PIN_DC, 1);
    cs_low();
    while (remaining) {
        const uint32_t count = remaining > PIXELS_PER_CHUNK ? PIXELS_PER_CHUNK : remaining;
        spi_write_blocking(TFT_SPI, buf, count * 2);
        remaining -= count;
    }
    cs_high();
}

static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t font_letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static void get_glyph(char c, uint8_t glyph[5]) {
    memset(glyph, 0, 5);
    if (c >= '0' && c <= '9') {
        memcpy(glyph, font_digits[c - '0'], 5);
    } else if (c >= 'A' && c <= 'Z') {
        memcpy(glyph, font_letters[c - 'A'], 5);
    } else if (c == ':') {
        glyph[1] = 0x36;
        glyph[2] = 0x36;
    } else if (c == '-') {
        glyph[0] = glyph[1] = glyph[2] = glyph[3] = glyph[4] = 0x08;
    } else if (c == '+') {
        glyph[1] = 0x08; glyph[2] = 0x3E; glyph[3] = 0x08;
    }
}

static void draw_char(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t color) {
    uint8_t glyph[5];
    get_glyph(c, glyph);
    for (uint8_t col = 0; col < 5; ++col) {
        for (uint8_t row = 0; row < 7; ++row) {
            if (glyph[col] & (1u << row)) {
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text(uint16_t x, uint16_t y, const char *text, uint8_t scale, uint16_t color) {
    while (*text && x + 6u * scale <= TFT_WIDTH) {
        draw_char(x, y, *text++, scale, color);
        x += 6u * scale;
    }
}

static void clear_text_row(uint16_t y, uint16_t h) {
    fill_rect(0, y, TFT_WIDTH, h, COLOR_BLACK);
}

static void render_screen(void) {
    const bool connected = midi_dev_idx != TUSB_INDEX_INVALID_8 && tuh_midi_mounted(midi_dev_idx);

    clear_text_row(8, 22);
    draw_text(8, 8, "USB MIDI MONITOR", 2, COLOR_CYAN);

    clear_text_row(42, 18);
    draw_text(8, 42, connected ? "CONNECTED" : "NO MIDI DEVICE", 2,
              connected ? COLOR_GREEN : COLOR_YELLOW);

    clear_text_row(82, 20);
    draw_text(8, 82, line_type, 2, COLOR_WHITE);

    clear_text_row(122, 18);
    draw_text(8, 122, line_one, 2, COLOR_WHITE);

    clear_text_row(154, 18);
    draw_text(8, 154, line_two, 2, COLOR_WHITE);

    clear_text_row(186, 18);
    draw_text(8, 186, line_three, 2, COLOR_WHITE);

    screen_dirty = false;
    last_render = get_absolute_time();
}

static void set_event_lines(const char *type, uint8_t channel, int value1, int value2,
                            const char *label1, const char *label2) {
    snprintf(line_type, sizeof line_type, "%s", type);
    snprintf(line_one, sizeof line_one, "CH: %u", channel + 1u);
    snprintf(line_two, sizeof line_two, "%s: %d", label1, value1);
    if (label2 && label2[0]) {
        snprintf(line_three, sizeof line_three, "%s: %d", label2, value2);
    } else {
        line_three[0] = '\0';
    }
    screen_dirty = true;
}

static uint8_t running_status = 0;
static uint8_t midi_data[2];
static uint8_t midi_data_count = 0;
static uint8_t midi_data_needed = 0;

static void handle_channel_message(uint8_t status, const uint8_t *data) {
    const uint8_t type = status & 0xF0u;
    const uint8_t channel = status & 0x0Fu;

    switch (type) {
        case 0x80:
            set_event_lines("NOTE OFF", channel, data[0], data[1], "NOTE", "VELOCITY");
            break;
        case 0x90:
            if (data[1] == 0) {
                set_event_lines("NOTE OFF", channel, data[0], 0, "NOTE", "VELOCITY");
            } else {
                set_event_lines("NOTE ON", channel, data[0], data[1], "NOTE", "VELOCITY");
            }
            break;
        case 0xA0:
            set_event_lines("POLY PRESSURE", channel, data[0], data[1], "NOTE", "VALUE");
            break;
        case 0xB0:
            set_event_lines("CONTROL CHANGE", channel, data[0], data[1], "CC", "VALUE");
            break;
        case 0xC0:
            set_event_lines("PROGRAM CHANGE", channel, data[0], 0, "PROGRAM", "");
            break;
        case 0xD0:
            set_event_lines("CHANNEL PRESSURE", channel, data[0], 0, "VALUE", "");
            break;
        case 0xE0: {
            const int bend = ((int)data[1] << 7) | data[0];
            set_event_lines("PITCH BEND", channel, bend - 8192, 0, "VALUE", "");
            break;
        }
        default:
            break;
    }
}

static void parse_midi_byte(uint8_t byte) {
    if (byte >= 0xF8) return;

    if (byte & 0x80u) {
        if (byte >= 0xF0u) {
            running_status = 0;
            midi_data_count = 0;
            midi_data_needed = 0;
            return;
        }

        running_status = byte;
        midi_data_count = 0;
        const uint8_t type = byte & 0xF0u;
        midi_data_needed = (type == 0xC0u || type == 0xD0u) ? 1 : 2;
        return;
    }

    if (!running_status || !midi_data_needed) return;

    midi_data[midi_data_count++] = byte;
    if (midi_data_count >= midi_data_needed) {
        handle_channel_message(running_status, midi_data);
        midi_data_count = 0;
    }
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_data) {
    (void)mount_data;
    if (midi_dev_idx == TUSB_INDEX_INVALID_8) {
        midi_dev_idx = idx;
        snprintf(line_type, sizeof line_type, "READY");
        line_one[0] = line_two[0] = line_three[0] = '\0';
        screen_dirty = true;
    }
}

void tuh_midi_umount_cb(uint8_t idx) {
    if (idx == midi_dev_idx) {
        midi_dev_idx = TUSB_INDEX_INVALID_8;
        running_status = 0;
        midi_data_count = 0;
        snprintf(line_type, sizeof line_type, "WAITING FOR MIDI");
        line_one[0] = line_two[0] = line_three[0] = '\0';
        screen_dirty = true;
    }
}

void tuh_midi_rx_cb(uint8_t idx, uint32_t num_bytes) {
    if (idx != midi_dev_idx || num_bytes == 0) return;

    uint8_t cable = 0;
    uint8_t buffer[64];
    while (true) {
        const uint32_t bytes_read = tuh_midi_stream_read(idx, &cable, buffer, sizeof buffer);
        (void)cable;
        if (!bytes_read) break;
        for (uint32_t i = 0; i < bytes_read; ++i) parse_midi_byte(buffer[i]);
    }
}

void tuh_midi_tx_cb(uint8_t idx, uint32_t num_bytes) {
    (void)idx;
    (void)num_bytes;
}

static void init_display(void) {
    spi_init(TFT_SPI, 40 * 1000 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    const uint pins[] = {PIN_RST, PIN_DC, PIN_CS, PIN_BL};
    for (size_t i = 0; i < sizeof pins / sizeof pins[0]; ++i) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
    }

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_RST, 1);
    gpio_put(PIN_BL, 1);
    st7789_init();
    fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK);
}

int main(void) {
    board_init();
    init_display();
    render_screen();

    if (!tusb_init()) {
        snprintf(line_type, sizeof line_type, "USB INIT FAILED");
        screen_dirty = true;
    }

    while (true) {
        tuh_task();

        if (screen_dirty && absolute_time_diff_us(last_render, get_absolute_time()) >= 33000) {
            render_screen();
        }
    }
}
