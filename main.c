#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define TFT_SPI spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_RST 20
#define PIN_DC 21
#define PIN_CS 17
#define PIN_BL 16

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

static inline void cs_low(void){ gpio_put(PIN_CS,0); }
static inline void cs_high(void){ gpio_put(PIN_CS,1); }

static void write_cmd(uint8_t v){
    gpio_put(PIN_DC,0); cs_low(); spi_write_blocking(TFT_SPI,&v,1); cs_high();
}

static void write_data(const uint8_t *d,size_t n){
    gpio_put(PIN_DC,1); cs_low(); spi_write_blocking(TFT_SPI,d,n); cs_high();
}

static void cmd_data(uint8_t c,const uint8_t *d,size_t n){ write_cmd(c); if(n) write_data(d,n); }

static void st7789_init(void){
    gpio_put(PIN_RST,1); sleep_ms(10);
    gpio_put(PIN_RST,0); sleep_ms(20);
    gpio_put(PIN_RST,1); sleep_ms(120);

    write_cmd(0x01); sleep_ms(150); // SWRESET
    write_cmd(0x11); sleep_ms(120); // SLPOUT

    const uint8_t colmod[]={0x55};
    cmd_data(0x3A,colmod,sizeof colmod); // RGB565

    const uint8_t madctl[]={0x00};
    cmd_data(0x36,madctl,sizeof madctl);

    write_cmd(0x21); // INVON
    write_cmd(0x13); // NORON
    sleep_ms(10);
    write_cmd(0x29); // DISPON
    sleep_ms(120);
}

static void set_window(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){
    const uint8_t cols[]={x0>>8,x0&0xff,x1>>8,x1&0xff};
    const uint8_t rows[]={y0>>8,y0&0xff,y1>>8,y1&0xff};
    cmd_data(0x2A,cols,sizeof cols);
    cmd_data(0x2B,rows,sizeof rows);
    write_cmd(0x2C);
}

static void fill(uint16_t color){
    set_window(0,0,TFT_WIDTH-1,TFT_HEIGHT-1);
    enum { N=256 };
    uint8_t buf[N*2];
    uint8_t hi=color>>8, lo=color&0xff;
    for(size_t i=0;i<N;i++){ buf[i*2]=hi; buf[i*2+1]=lo; }

    gpio_put(PIN_DC,1); cs_low();
    uint32_t remaining=TFT_WIDTH*TFT_HEIGHT;
    while(remaining){
        uint32_t count=remaining>N?N:remaining;
        spi_write_blocking(TFT_SPI,buf,count*2);
        remaining-=count;
    }
    cs_high();
}

int main(void){
    stdio_init_all();
    spi_init(TFT_SPI,40*1000*1000);
    gpio_set_function(PIN_SCK,GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI,GPIO_FUNC_SPI);

    const uint pins[]={PIN_RST,PIN_DC,PIN_CS,PIN_BL};
    for(size_t i=0;i<4;i++){ gpio_init(pins[i]); gpio_set_dir(pins[i],GPIO_OUT); }
    gpio_put(PIN_CS,1);
    gpio_put(PIN_RST,1);
    gpio_put(PIN_BL,1);

    st7789_init();

    while(true){
        fill(0xF800); sleep_ms(1000);
        fill(0x07E0); sleep_ms(1000);
        fill(0x001F); sleep_ms(1000);
        fill(0xFFFF); sleep_ms(500);
        fill(0x0000); sleep_ms(500);
    }
}
