#include "ai_passport_display.h"

#include <esp_timer.h>
#include <functional>
#include <lvgl.h>

#include "application.h"
#include "board.h"

static const char* TAG = "AiPassportDisplay";

AiPassportDisplay::AiPassportDisplay(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_handle_t panel,
                                     int width, int height,
                                     int offset_x, int offset_y,
                                     bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y,
                    mirror_x, mirror_y, swap_xy) {
}

AiPassportDisplay::~AiPassportDisplay() {
    if (battery_timer_ != nullptr) {
        esp_timer_stop(battery_timer_);
        esp_timer_delete(battery_timer_);
        battery_timer_ = nullptr;
    }
}

void AiPassportDisplay::SetupUI() {
    // 先构建标准 UI。battery_label_ 此时已由基类创建（基础为电池图标标签）。
    SpiLcdDisplay::SetupUI();
    if (battery_label_ == nullptr) {
        ESP_LOGW(TAG, "battery_label_ is null, skip iOS battery capsule");
        return;
    }

    DisplayLockGuard lock(this);

    // 把 battery_label_ 复用为 iOS 风格白色圆角胶囊：内嵌电量百分比。
    // 字体使用 UI 文本字体（本板内置 20px），保证数字可渲染且该字体一定已链接。
    lv_obj_set_style_text_font(battery_label_, lv_obj_get_style_text_font(lv_screen_active(), LV_PART_MAIN), LV_PART_MAIN);
    lv_obj_set_style_text_color(battery_label_, lv_color_black(), 0);

    // 白色圆角胶囊背景（半径 9 近似全圆角；高度随字体自适应，避免裁切）。
    lv_obj_set_style_bg_color(battery_label_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(battery_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(battery_label_, 9, 0);
    lv_obj_set_size(battery_label_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(battery_label_, 6, 0);
    lv_obj_set_style_pad_ver(battery_label_, 2, 0);
    lv_obj_set_style_margin_left(battery_label_, 4, 0);

    lv_label_set_text(battery_label_, "--%");

    // 每 5s 轮询一次电量；回调中调度到主线程，LVGL 操作安全。
    esp_timer_create_args_t args{};
    args.callback = &AiPassportDisplay::BatteryTimerCallback;
    args.arg = this;
    args.name = "ai_batt_pct";
    args.skip_unhandled_events = true;
    if (esp_timer_create(&args, &battery_timer_) == ESP_OK) {
        esp_timer_start_periodic(battery_timer_, 5000000);
    }

    UpdateBatteryPercent();
}

void AiPassportDisplay::UpdateBatteryPercent() {
    // 息屏时跳过：LVGL port 已停止，且无需刷新隐藏界面。
    if (power_save_mode_) {
        return;
    }

    int level = 0;
    bool charging = false, discharging = false;
    if (!Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
        return;  // CW2017 缺失时无电量计，保留 "--%"
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", level);

    DisplayLockGuard lock(this);
    if (battery_label_ != nullptr) {
        lv_label_set_text(battery_label_, buf);
    }
}

void AiPassportDisplay::UpdateStatusBar(bool update_all) {
    // 息屏（省电）时跳过状态栏刷新：
    //   1) 省电；
    //   2) 此时 LVGL port 已被 lvgl_port_stop() 停止，DisplayLock（=lvgl_port_lock）
    //      会阻塞主线程导致设备卡死，必须跳过。
    if (power_save_mode_) {
        return;
    }

    // 基类负责静音图标 / 时钟 / 低电量弹窗 / 电量图标。
    LvglDisplay::UpdateStatusBar(update_all);
    // 用百分比覆盖基类写入的电池图标，呈现 iOS 胶囊效果。
    UpdateBatteryPercent();
}

void AiPassportDisplay::SetPowerSaveMode(bool on) {
    power_save_mode_ = on;
    // 基类切 sleepy / neutral 表情。
    LvglDisplay::SetPowerSaveMode(on);
}

void AiPassportDisplay::BatteryTimerCallback(void* arg) {
    auto* self = static_cast<AiPassportDisplay*>(arg);
    Application::GetInstance().Schedule([self]() { self->UpdateBatteryPercent(); });
}
