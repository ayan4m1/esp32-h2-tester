#include "display.h"
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
static const char* TAG = "lcd";
static uint8_t* buffer1;
static uint8_t* buffer2;
static bool i2c_initialized = false;
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
static esp_lcd_panel_handle_t lcd_handle = NULL;
static void i2c_init() {
    if(i2c_initialized) {
        return;
    }
    i2c_config_t i2c_cfg;
    memset(&i2c_cfg,0,sizeof(i2c_cfg));
    i2c_cfg.master.clk_speed = LCD_CLOCK_HZ;
    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_io_num = LCD_PIN_NUM_SDA;
    i2c_cfg.sda_pullup_en = 1;
    i2c_cfg.scl_io_num = LCD_PIN_NUM_SCL;
    i2c_cfg.scl_pullup_en = 1;
    ESP_ERROR_CHECK(i2c_driver_install((i2c_port_t)LCD_I2C_HOST,I2C_MODE_MASTER,0,0,0));
    ESP_ERROR_CHECK(i2c_param_config((i2c_port_t)LCD_I2C_HOST,&i2c_cfg));
    i2c_initialized=true;
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
    i2c_init();
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
    esp_lcd_panel_io_i2c_config_t lcd_i2c_cfg;
    memset(&lcd_i2c_cfg,0,sizeof(lcd_i2c_cfg));
    lcd_i2c_cfg.control_phase_bytes = LCD_CONTROL_PHASE_BYTES;
    lcd_i2c_cfg.dc_bit_offset = LCD_DC_BIT_OFFSET;
    lcd_i2c_cfg.dev_addr = LCD_I2C_ADDRESS;
#ifdef LCD_DISABLE_CONTROL_PHASE
#if LCD_DISABLE_CONTROL_PHASE > 0
    lcd_i2c_cfg.flags.disable_control_phase = 1;
#endif
#endif
    lcd_i2c_cfg.lcd_cmd_bits = 8;
    lcd_i2c_cfg.lcd_param_bits = 8;
    lcd_i2c_cfg.on_color_trans_done = on_flush_complete;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)LCD_I2C_HOST, &lcd_i2c_cfg, &lcd_io_handle));
    
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
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
#elif LCD_COLOR_SPACE == LCD_COLOR_BGR
    panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#elif LCD_COLOR_SPACE == LCD_COLOR_GSC
    // seems to work
    panel_config.color_space = ESP_LCD_COLOR_SPACE_MONOCHROME;
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