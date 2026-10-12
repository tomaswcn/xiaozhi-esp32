#pragma once

#include "display/lcd_display.h"

// AI Passport 定制显示层。
// 在 SpiLcdDisplay 之上做两件事：
//   1) 把电池指示从“图标”改为 iOS 风格白色圆角胶囊，内嵌电量百分比（如 82%）。
//   2) 息屏（省电）状态下跳过状态栏刷新，避免 LVGL port 已停止时 DisplayLock 阻塞主线程。
class AiPassportDisplay : public SpiLcdDisplay {
public:
    AiPassportDisplay(esp_lcd_panel_io_handle_t panel_io,
                      esp_lcd_panel_handle_t panel,
                      int width, int height,
                      int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy);
    ~AiPassportDisplay() override;

    void SetupUI() override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetPowerSaveMode(bool on) override;

private:
    void UpdateBatteryPercent();
    static void BatteryTimerCallback(void* arg);

    bool power_save_mode_ = false;
    esp_timer_handle_t battery_timer_ = nullptr;
};
