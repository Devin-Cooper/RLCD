#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>

#define LCD_WIDTH  400
#define LCD_HEIGHT 300

enum ColorSelection {
    ColorBlack = 0,
    ColorWhite = 0xff
};

class DisplayPort {
private:
    esp_lcd_panel_io_handle_t io_handle = NULL;
    int mosi_;
    int scl_;
    int dc_;
    int cs_;
    int rst_;
    int width_;
    int height_;
    uint8_t *DispBuffer = NULL;
    int DisplayLen;

    uint16_t (*PixelIndexLUT)[LCD_HEIGHT];
    uint8_t (*PixelBitLUT)[LCD_HEIGHT];

    void InitLandscapeLUT();
    void Set_ResetIOLevel(uint8_t level);
    void RLCD_SendCommand(uint8_t Reg);
    void RLCD_SendData(uint8_t Data);
    void RLCD_Sendbuffera(uint8_t *Data, int len);
    void RLCD_Reset(void);

public:
    DisplayPort(int mosi, int scl, int dc, int cs, int rst, int width, int height, spi_host_device_t spihost = SPI3_HOST);
    ~DisplayPort();

    void RLCD_Init();
    void RLCD_ColorClear(uint8_t color);
    void RLCD_Display();
    void RLCD_SetPixel(uint16_t x, uint16_t y, uint8_t color);
    void RLCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
    void RLCD_DrawChar(uint16_t x, uint16_t y, char c, uint8_t color);
    void RLCD_DrawString(uint16_t x, uint16_t y, const char *str, uint8_t color);
};
