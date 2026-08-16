#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "Bt.h"
#include "Menu.h"
#include "FanControl.h"
#include "Icons.h"
#include "Fonts.cpp"
#include "Display.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool needDisplay = true;
unsigned long lastDisplayTime = 0;

void Display_Init()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    LOG_ERR("SSD1306 init failed");
    for (;;);
  }
  delay(50); // 等待 SSD1306 初始化完成，避免首帧花屏
  Display_SetBrightness(brightness);
}

void Display_SetBrightness(int level)
{
  int contrast = map(constrain(level, BRIGHTNESS_MIN, BRIGHTNESS_MAX),
                     BRIGHTNESS_MIN, BRIGHTNESS_MAX, 8, 255);
  display.ssd1306_command(0x81);
  display.ssd1306_command(contrast);
}

// 开机画面（对齐原固件：LAPTOP / FAN 居中）
void Display_Startup()
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  int16_t x, y;
  uint16_t w, h;

  display.getTextBounds("LAPTOP", 0, 0, &x, &y, &w, &h);
  int xPos = (OLED_WIDTH - w) / 2;
  int yPos = (OLED_HEIGHT - h * 2) / 2;
  display.setCursor(xPos, yPos);
  display.print(F("LAPTOP"));

  display.getTextBounds("FAN", 0, 0, &x, &y, &w, &h);
  xPos = (OLED_WIDTH - w) / 2;
  yPos = yPos + h + 2;
  display.setCursor(xPos, yPos);
  display.print(F("FAN"));

  display.display();
  delay(STARTUP_MS);
  LOG_INFO("Startup screen shown");
}

void Display_ShowRestarting()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 28);
  display.print(F("Restarting..."));
  display.display();
}

void Display_Clear()
{
  display.clearDisplay();
  display.display();
}

// OLED 面板开关：休眠时关断面板省电，唤醒时重新点亮
void Display_SetPanelOn(bool on)
{
  display.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void Display_RequestUpdate()
{
  needDisplay = true;
}

// 行1 右侧蓝牙状态图标（16x16）：BLE 已连接 / BLE 连接中 / COM 模式
static void Display_DrawBtIcon(int16_t x, int16_t y)
{
  const unsigned char* icon = ICON_BT_COM;
  if (btMode == BT_MODE_BLE)
  {
    icon = Bt_IsConnected() ? ICON_BT_BLE : ICON_BT_CONNECTING;
  }

  for (int page = 0; page < 2; page++)
  {
    for (int col = 0; col < 16; col++)
    {
      uint8_t bits = pgm_read_byte(&icon[page * 16 + col]);
      for (int row = 0; row < 8; row++)
      {
        if (bits & (1 << row))
        {
          display.drawPixel(x + col, y + page * 8 + row, SSD1306_WHITE);
        }
      }
    }
  }
}

// 绘制 22×22 图标（66 字节：3 页 × 22 列）
static void Display_DrawIcon22(const unsigned char* icon, int16_t x, int16_t y)
{
  for (int page = 0; page < 3; page++)
  {
    for (int col = 0; col < 22; col++)
    {
      uint8_t bits = pgm_read_byte(&icon[page * 22 + col]);
      for (int row = 0; row < 8; row++)
      {
        if (bits & (1 << row))
        {
          display.drawPixel(x + col, y + page * 8 + row, SSD1306_WHITE);
        }
      }
    }
  }
}

// ASCII 字符 -> 字模索引（Fonts.cpp）
static uint8_t Display_CharToFontIdx(char c)
{
  if (c >= '1' && c <= '9')
  {
    return (uint8_t)(c - '1'); // FONT_IDX_1..FONT_IDX_9
  }
  switch (c)
  {
    case '0': return FONT_IDX_0;
    case '.': return FONT_IDX_DOT;
    case '%': return FONT_IDX_PERC;
    case 'A': return FONT_IDX_A;
    case 'U': return FONT_IDX_U;
    case 'T': return FONT_IDX_T;
    case 'O': return FONT_IDX_O;
    case 'M': return FONT_IDX_M;
    case 'N': return FONT_IDX_N;
    case 'L': return FONT_IDX_L;
    case '-': return FONT_IDX_MINUS;
  }
  return FONT_IDX_0;
}

// 绘制文字（普通字符 7×14、小数点 5×14、℃ 14×14），从 (x, y) 起始逐字符推进
static void Display_DrawText14(int16_t x, int16_t y, const uint8_t* idx, int len)
{
  for (int i = 0; i < len; i++)
  {
    int width = 7;
    if (idx[i] == FONT_IDX_DOT)
    {
      width = 5;
    }
    else if (idx[i] == FONT_IDX_DEG)
    {
      width = 14;
    }
    else if (idx[i] == FONT_IDX_O)
    {
      width = 8;
    }
    const unsigned char* glyph =
        (const unsigned char*)pgm_read_ptr(&fontData[idx[i]]);
    for (int page = 0; page < 2; page++)
    {
      for (int col = 0; col < width; col++)
      {
        uint8_t bits = pgm_read_byte(&glyph[page * width + col]);
        for (int row = 0; row < 8; row++)
        {
          if (bits & (1 << row))
          {
            display.drawPixel(x + col, y + page * 8 + row, SSD1306_WHITE);
          }
        }
      }
    }
    x += width;
  }
}

void Display_Loop(unsigned long now)
{
  if (isSleeping)
  {
    return;
  }

  if (now - lastDisplayTime < DISPLAY_REFRESH_MS)
  {
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (menuOpen)
  {
    display.setCursor(0, 0);
    display.println(F("Menu"));

    for (int i = 0; i < MENU_ITEM_COUNT; i++)
    {
      int y = 12 + i * 13;
      display.setCursor(4, y);
      int selected = Menu_Selection();
      display.print(i == selected ? ">" : " ");
      display.print(" ");
      display.print(i == 0 ? "BT Visible" : i == 1 ? "BT Mode" : i == 2 ? "Brightness" : "Fan Freq");

      String value;
      switch (i)
      {
        case 0: value = btVisible ? "On" : "Off"; break;
        case 1: value = btMode == BT_MODE_BLE ? "BLE" : "COM"; break;
        case 2: value = String(brightness); break;
        case 3: value = String(fanFrequency) + "Hz"; break;
      }

      int vx = OLED_WIDTH - (value.length() * 6) - 2;
      display.setCursor(vx, y);
      display.print(value);
    }
  }
  else
  {
    // 行1：年月日 + 蓝牙状态图标
    display.setCursor(0, 0);
    if (timeSet)
    {
      currentTime = Time_Now();
      char dateBuf[20];
      snprintf(dateBuf, sizeof(dateBuf), "%04d/%02d/%02d",
               currentTime.year, currentTime.month, currentTime.day);
      display.print(dateBuf);
    }

    Display_DrawBtIcon(OLED_WIDTH - 18, 0);

    // 行2：时分秒（8 像素字高，紧接日期行）
    if (timeSet)
    {
      char timeBuf[20];
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
               currentTime.hour, currentTime.minute, currentTime.second);
      display.setCursor(0, 9);
      display.print(timeBuf);
    }

    // 下半部分：2×2 四区域（CPU / GPU / MODE / FAN）
    // 左上：CPU 温度
    uint8_t idx[16];
    char buf[12];
    int n;

    // 断连/无数据：温度显示 "--"；自动模式下实际转速归 0，手动模式由按键控制不受影响
    bool dataActive = hasSensorData && Bt_IsDataActive();
    if (!dataActive && autoMode && dutyCycle != 0)
    {
      Fan_SetDuty(0);
    }
    uint8_t dash[2] = {FONT_IDX_MINUS, FONT_IDX_MINUS};

    // 图标 (17,0)，文字起点 (21,23)
    Display_DrawIcon22(ICON_CPU, 0, 17);
    if (dataActive)
    {
      n = snprintf(buf, sizeof(buf), "%.1f", cpuValue);
      for (int i = 0; i < n; i++)
      {
        idx[i] = Display_CharToFontIdx(buf[i]);
      }
      idx[n++] = FONT_IDX_DEG;
      Display_DrawText14(23, 21, idx, n);
    }
    else
    {
      Display_DrawText14(23, 21, dash, 2);
    }

    // 右上：GPU 温度
    // 图标 (17,65)，文字起点 (21,87)
    Display_DrawIcon22(ICON_GPU, 65, 17);
    if (dataActive)
    {
      n = snprintf(buf, sizeof(buf), "%.1f", gpuValue);
      for (int i = 0; i < n; i++)
      {
        idx[i] = Display_CharToFontIdx(buf[i]);
      }
      idx[n++] = FONT_IDX_DEG;
      Display_DrawText14(87, 21, idx, n);
    }
    else
    {
      Display_DrawText14(87, 21, dash, 2);
    }

    // 左下：模式
    // 图标 (41,0)，文字起点 (45,23)
    Display_DrawIcon22(ICON_MODE_SWITCH, 0, 41);
    const char* modeText = autoMode ? "AUTO" : "MANUAL";
    n = 0;
    for (const char* p = modeText; *p; p++)
    {
      idx[n++] = Display_CharToFontIdx(*p);
    }
    Display_DrawText14(23, 45, idx, n);

    // 右下：转速比（PWM %）
    // 图标 (41,65)，文字起点 (45,87)
    Display_DrawIcon22(ICON_FAN, 65, 41);
    n = snprintf(buf, sizeof(buf), "%d", dutyCycle);
    for (int i = 0; i < n; i++)
    {
      idx[i] = Display_CharToFontIdx(buf[i]);
    }
    idx[n++] = FONT_IDX_PERC;
    Display_DrawText14(87, 45, idx, n);
  }

  display.display();
  lastDisplayTime = now;
}
