#include "display.h"
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_io_spi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
static const char* TAG = "lcd";
static uint8_t* buffer1;
static uint8_t* buffer2;
static bool spi_initialized = false;
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
static esp_lcd_panel_handle_t lcd_handle = NULL;
static void spi_init(void) {
    if(spi_initialized) {
        return;
    }
    spi_bus_config_t spi_cfg;
    uint32_t spi_sz;

    memset(&spi_cfg,0,sizeof(spi_cfg));
    spi_sz = LCD_TRANSFER_SIZE+8;

    if(spi_sz>32*1024) {
        ESP_LOGW(TAG,"SPI transfer size is limited to 32KB, but a SPI host device demands more. Increase LCD_DIVISOR");
        spi_sz = 32*1024;
    }
    spi_cfg.max_transfer_sz = spi_sz;
    spi_cfg.sclk_io_num = LCD_PIN_NUM_CLK;
    spi_cfg.mosi_io_num = LCD_PIN_NUM_MOSI;
    spi_cfg.miso_io_num = -1;
    
    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)LCD_SPI_HOST,&spi_cfg,SPI_DMA_CH_AUTO));    
    spi_initialized = true;
}
static IRAM_ATTR bool on_flush_complete(esp_lcd_panel_io_handle_t lcd_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
    lcd_flush_complete();
    return true;
}
void lcd_init(void) {
    if(lcd_handle!=NULL) {
        ESP_LOGW(TAG,"lcd_init() was already called");
        return; // already initialized
    }
    spi_init();
    buffer1 = (uint8_t*)heap_caps_malloc(LCD_TRANSFER_SIZE,MALLOC_CAP_DMA);
    if(!buffer1) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    buffer2 = (uint8_t*)heap_caps_malloc(LCD_TRANSFER_SIZE,MALLOC_CAP_DMA);
    if(!buffer2) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
#ifdef LCD_PIN_NUM_BCKL
#if LCD_PIN_NUM_BCKL >= 0
    gpio_config_t bk_gpio_config;
    memset(&bk_gpio_config,0,sizeof(gpio_config_t));
    bk_gpio_config.mode = GPIO_MODE_OUTPUT;
    bk_gpio_config.pin_bit_mask = 1ULL << LCD_PIN_NUM_BCKL;
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level((gpio_num_t)LCD_PIN_NUM_BCKL, LCD_BCKL_OFF_LEVEL);
#endif
#endif
#ifdef LCD_RESET
    LCD_RESET;
#endif
    esp_lcd_panel_io_spi_config_t lcd_spi_cfg;
    memset(&lcd_spi_cfg,0,sizeof(lcd_spi_cfg));
    lcd_spi_cfg.cs_gpio_num = LCD_PIN_NUM_CS;
    lcd_spi_cfg.dc_gpio_num = LCD_PIN_NUM_DC;
    lcd_spi_cfg.lcd_cmd_bits = 8;
    lcd_spi_cfg.lcd_param_bits = 8;        
#ifdef LCD_CLOCK_HZ
    lcd_spi_cfg.pclk_hz = LCD_CLOCK_HZ;
#else
    lcd_spi_cfg.pclk_hz = 20 * 1000 * 1000;
#endif
    lcd_spi_cfg.trans_queue_depth = 10;
    lcd_spi_cfg.on_color_trans_done = on_flush_complete;
#ifdef LCD_SPI_MODE
    lcd_spi_cfg.spi_mode = LCD_SPI_MODE;
#else
    lcd_spi_cfg.spi_mode = 0;
#endif
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &lcd_spi_cfg, &lcd_io_handle));
    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config,0,sizeof(panel_config));
#ifdef LCD_PIN_NUM_RST
    panel_config.reset_gpio_num = LCD_PIN_NUM_RST;
#else
    panel_config.reset_gpio_num = -1;
#endif
#if defined(LCD_RST_ON_LEVEL)
    panel_config.flags.reset_active_high = LCD_RST_ON_LEVEL;
#endif
    
#if LCD_COLOR_SPACE == LCD_COLOR_RGB
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
#elif LCD_COLOR_SPACE == LCD_COLOR_BGR
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
#elif LCD_COLOR_SPACE == LCD_COLOR_GSC
    // seems to work
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
#endif
#if defined(LCD_DATA_ENDIAN_LITTLE) && LCD_DATA_ENDIAN_LITTLE > 0
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
#else
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
#endif
    panel_config.bits_per_pixel = LCD_BIT_DEPTH;
    ESP_ERROR_CHECK(LCD_INIT(lcd_io_handle, &panel_config, &lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_handle));
    int gap_x = 0, gap_y = 0;
#ifdef LCD_GAP_X
    gap_x = LCD_GAP_X;
#endif
#ifdef LCD_GAP_Y
    gap_y = LCD_GAP_Y;
#endif
    esp_lcd_panel_set_gap(lcd_handle,gap_x,gap_y);
    
#ifdef LCD_SWAP_XY
#if LCD_SWAP_XY
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_handle,true));
#endif
#endif
    bool mirror_x = false, mirror_y = false;
#ifdef LCD_MIRROR_X
#if LCD_MIRROR_X
    mirror_x = LCD_MIRROR_X;
#endif
#endif
#ifdef LCD_MIRROR_Y
#if LCD_MIRROR_Y
    mirror_y = LCD_MIRROR_Y;
#endif
#endif
    esp_lcd_panel_mirror(lcd_handle,mirror_x,mirror_y);
    
#ifdef LCD_INVERT_COLOR
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_handle,LCD_INVERT_COLOR));
#endif
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#ifdef LCD_PIN_NUM_BCKL
#if LCD_PIN_NUM_BCKL >= 0
    gpio_set_level((gpio_num_t)LCD_PIN_NUM_BCKL, LCD_BCKL_ON_LEVEL);
#endif
#endif
}
uint8_t* lcd_buffer1(void) { return buffer1; }
uint8_t* lcd_buffer2(void) { return buffer2; }

void lcd_flush(int x1, int y1, int x2, int y2, void* bitmap) {
    if(lcd_handle==NULL) {
        ESP_LOGE(TAG,"panel_lcd_flush() was invoked but panel_lcd_init() was never called.");
        return;
    }
#ifdef LCD_TRANSLATE
    LCD_TRANSLATE;
#endif    
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2 + 1, y2 + 1, bitmap));
}