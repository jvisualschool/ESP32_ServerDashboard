#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"

#include "config.h"

// -------------------------------------------------------------------
// Wi-Fi 설정 (config.h에서 정의됨)
// -------------------------------------------------------------------


// 서버 정보
struct ServerInfo {
    const char* name;
    const char* url;
};

ServerInfo servers[] = {
    {"jvibeschool.com", "https://jvibeschool.com/status.json"},
    {"jvibeschool.org", "https://jvibeschool.org/status.json.php"}
};

const int server_count = 2;
int current_server = 0;

#define LVGL_PORT_ROTATION_DEGREE (90)

// UI 요소
static lv_obj_t *ui_screen;
static lv_obj_t *label_title;
static lv_obj_t *server_badge;
static lv_obj_t *badge_label;
static lv_obj_t *label_updated;

// 4개의 카드
static lv_obj_t *card_cpu;
static lv_obj_t *label_cpu_title;
static lv_obj_t *bar_cpu;
static lv_obj_t *label_cpu_val;

static lv_obj_t *card_mem;
static lv_obj_t *label_mem_title;
static lv_obj_t *bar_mem;
static lv_obj_t *label_mem_val;

static lv_obj_t *card_disk;
static lv_obj_t *label_disk_title;
static lv_obj_t *bar_disk;
static lv_obj_t *label_disk_val;

static lv_obj_t *card_uptime;
static lv_obj_t *label_uptime_title;
static lv_obj_t *label_uptime_val;

static lv_obj_t *led_status;

// 데이터 갱신 타이머
static unsigned long last_update_ms = 0;
const unsigned long update_interval = 30000; // 30초마다 갱신

// 테마 관리
enum Theme {
    THEME_DARK,
    THEME_LIGHT
};

static Theme current_theme = THEME_DARK;

// 트랜지션 효과
static lv_style_transition_dsc_t trans_dsc;
static const lv_style_prop_t props[] = {LV_STYLE_BG_COLOR, LV_STYLE_TEXT_COLOR, (lv_style_prop_t)0};

void apply_theme() {
    if (current_theme == THEME_DARK) {
        lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0x0a0a0a), 0);
        lv_obj_set_style_text_color(label_title, lv_color_hex(0x00BFFF), 0);
        lv_obj_set_style_text_color(label_updated, lv_color_hex(0x888888), 0);
        
        // 카드 배경
        lv_obj_set_style_bg_color(card_cpu, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(card_mem, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(card_disk, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(card_uptime, lv_color_hex(0x1a1a1a), 0);
        
        // 배지 색상
        lv_obj_set_style_bg_color(server_badge, lv_color_hex(0x00BFFF), 0);
        lv_obj_set_style_text_color(badge_label, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_text_color(label_title, lv_color_hex(0x1976D2), 0);
        lv_obj_set_style_text_color(label_updated, lv_color_hex(0x666666), 0);
        
        // 카드 배경
        lv_obj_set_style_bg_color(card_cpu, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_color(card_mem, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_color(card_disk, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_color(card_uptime, lv_color_hex(0xffffff), 0);
        
        // 배지 색상
        lv_obj_set_style_bg_color(server_badge, lv_color_hex(0x1976D2), 0);
        lv_obj_set_style_text_color(badge_label, lv_color_hex(0xffffff), 0);
    }
}

static void screen_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED) {
        // 클릭: 테마 전환
        current_theme = (current_theme == THEME_DARK) ? THEME_LIGHT : THEME_DARK;
        apply_theme();
        Serial.printf("Theme changed: %s\n", (current_theme == THEME_DARK) ? "DARK" : "LIGHT");
    }
    else if(code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        
        if(dir == LV_DIR_LEFT) {
            // 왼쪽 스와이프: 다음 서버
            current_server = (current_server + 1) % server_count;
            Serial.printf("Swipe LEFT -> Server: %s\n", servers[current_server].name);
            
            bsp_display_lock(0);
            lv_label_set_text(label_title, servers[current_server].name);
            lv_label_set_text(badge_label, strstr(servers[current_server].name, ".org") ? ".org" : ".com");
            lv_label_set_text(label_uptime_val, "Loading...");
            bsp_display_unlock();
            
            fetch_server_data();
        }
        else if(dir == LV_DIR_RIGHT) {
            // 오른쪽 스와이프: 이전 서버
            current_server = (current_server - 1 + server_count) % server_count;
            Serial.printf("Swipe RIGHT -> Server: %s\n", servers[current_server].name);
            
            bsp_display_lock(0);
            lv_label_set_text(label_title, servers[current_server].name);
            lv_label_set_text(badge_label, strstr(servers[current_server].name, ".org") ? ".org" : ".com");
            lv_label_set_text(label_uptime_val, "Loading...");
            bsp_display_unlock();
            
            fetch_server_data();
        }
    }
}

void setup_ui() {
    lv_style_transition_dsc_init(&trans_dsc, props, lv_anim_path_linear, 300, 0, NULL);
    
    ui_screen = lv_scr_act();
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0x0a0a0a), 0);
    
    // 화면 터치 및 제스처 이벤트 등록
    lv_obj_add_flag(ui_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_screen, screen_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_transition(ui_screen, &trans_dsc, 0);
    
    // 타이틀 (호스트명)
    label_title = lv_label_create(ui_screen);
    lv_label_set_text(label_title, servers[current_server].name);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x00BFFF), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, -30, 10);

    // 서버 배지 (.org / .com)
    server_badge = lv_obj_create(ui_screen);
    lv_obj_set_size(server_badge, 60, 30);
    lv_obj_align_to(server_badge, label_title, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(server_badge, lv_color_hex(0x00BFFF), 0);
    lv_obj_set_style_border_width(server_badge, 0, 0);
    lv_obj_set_style_radius(server_badge, 6, 0);
    lv_obj_set_style_pad_all(server_badge, 0, 0);
    lv_obj_clear_flag(server_badge, LV_OBJ_FLAG_SCROLLABLE);

    badge_label = lv_label_create(server_badge);
    lv_label_set_text(badge_label, strstr(servers[current_server].name, ".org") ? ".org" : ".com");
    lv_obj_set_style_text_font(badge_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_label, lv_color_hex(0x000000), 0);
    lv_obj_center(badge_label);

    // 상태 LED
    led_status = lv_led_create(ui_screen);
    lv_obj_set_size(led_status, 15, 15);
    lv_obj_align(led_status, LV_ALIGN_TOP_RIGHT, -20, 25);
    lv_led_set_color(led_status, lv_color_hex(0x00FF00));
    lv_led_off(led_status);

    // ===== 2x2 그리드 카드 =====
    
    // [1] CPU 카드 (좌상)
    card_cpu = lv_obj_create(ui_screen);
    lv_obj_set_size(card_cpu, 210, 110);
    lv_obj_align(card_cpu, LV_ALIGN_TOP_LEFT, 20, 60);
    lv_obj_set_style_bg_color(card_cpu, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(card_cpu, 1, 0);
    lv_obj_set_style_border_color(card_cpu, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(card_cpu, 10, 0);
    lv_obj_clear_flag(card_cpu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card_cpu, LV_OBJ_FLAG_EVENT_BUBBLE);

    label_cpu_title = lv_label_create(card_cpu);
    lv_label_set_text(label_cpu_title, "CPU LOAD (5m)");
    lv_obj_set_style_text_font(label_cpu_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_cpu_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_cpu_title, LV_ALIGN_TOP_LEFT, 0, 0);

    bar_cpu = lv_bar_create(card_cpu);
    lv_obj_set_size(bar_cpu, 180, 15);
    lv_obj_align(bar_cpu, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(bar_cpu, 0, 100);
    lv_obj_set_style_bg_color(bar_cpu, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_cpu, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

    label_cpu_val = lv_label_create(card_cpu);
    lv_label_set_text(label_cpu_val, "0.0%");
    lv_obj_set_style_text_font(label_cpu_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_cpu_val, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_cpu_val, LV_ALIGN_BOTTOM_MID, 0, 5);

    // [2] MEMORY 카드 (우상)
    card_mem = lv_obj_create(ui_screen);
    lv_obj_set_size(card_mem, 210, 110);
    lv_obj_align(card_mem, LV_ALIGN_TOP_RIGHT, -20, 60);
    lv_obj_set_style_bg_color(card_mem, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(card_mem, 1, 0);
    lv_obj_set_style_border_color(card_mem, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(card_mem, 10, 0);
    lv_obj_clear_flag(card_mem, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card_mem, LV_OBJ_FLAG_EVENT_BUBBLE);

    label_mem_title = lv_label_create(card_mem);
    lv_label_set_text(label_mem_title, "MEMORY USAGE");
    lv_obj_set_style_text_font(label_mem_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_mem_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_mem_title, LV_ALIGN_TOP_LEFT, 0, 0);

    bar_mem = lv_bar_create(card_mem);
    lv_obj_set_size(bar_mem, 180, 15);
    lv_obj_align(bar_mem, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(bar_mem, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_mem, lv_color_hex(0xFF4500), LV_PART_INDICATOR);

    label_mem_val = lv_label_create(card_mem);
    lv_label_set_text(label_mem_val, "0.0%");
    lv_obj_set_style_text_font(label_mem_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_mem_val, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_mem_val, LV_ALIGN_BOTTOM_MID, 0, 5);

    // [3] SSD 카드 (좌하)
    card_disk = lv_obj_create(ui_screen);
    lv_obj_set_size(card_disk, 210, 110);
    lv_obj_align(card_disk, LV_ALIGN_BOTTOM_LEFT, 20, -40);
    lv_obj_set_style_bg_color(card_disk, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(card_disk, 1, 0);
    lv_obj_set_style_border_color(card_disk, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(card_disk, 10, 0);
    lv_obj_clear_flag(card_disk, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card_disk, LV_OBJ_FLAG_EVENT_BUBBLE);

    label_disk_title = lv_label_create(card_disk);
    lv_label_set_text(label_disk_title, "SSD USAGE");
    lv_obj_set_style_text_font(label_disk_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_disk_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_disk_title, LV_ALIGN_TOP_LEFT, 0, 0);

    bar_disk = lv_bar_create(card_disk);
    lv_obj_set_size(bar_disk, 180, 15);
    lv_obj_align(bar_disk, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(bar_disk, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_disk, lv_color_hex(0xFFD700), LV_PART_INDICATOR);

    label_disk_val = lv_label_create(card_disk);
    lv_label_set_text(label_disk_val, "0%");
    lv_obj_set_style_text_font(label_disk_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_disk_val, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_disk_val, LV_ALIGN_BOTTOM_MID, 0, 5);

    // [4] UPTIME 카드 (우하)
    card_uptime = lv_obj_create(ui_screen);
    lv_obj_set_size(card_uptime, 210, 110);
    lv_obj_align(card_uptime, LV_ALIGN_BOTTOM_RIGHT, -20, -40);
    lv_obj_set_style_bg_color(card_uptime, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(card_uptime, 1, 0);
    lv_obj_set_style_border_color(card_uptime, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(card_uptime, 10, 0);
    lv_obj_clear_flag(card_uptime, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card_uptime, LV_OBJ_FLAG_EVENT_BUBBLE);

    label_uptime_title = lv_label_create(card_uptime);
    lv_label_set_text(label_uptime_title, "UPTIME");
    lv_obj_set_style_text_font(label_uptime_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_uptime_title, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_uptime_title, LV_ALIGN_TOP_LEFT, 0, 0);

    label_uptime_val = lv_label_create(card_uptime);
    lv_obj_set_width(label_uptime_val, 190);
    lv_label_set_text(label_uptime_val, "Loading...");
    lv_label_set_long_mode(label_uptime_val, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label_uptime_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_uptime_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_uptime_val, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label_uptime_val, LV_ALIGN_CENTER, 0, 10);

    // Last Sync (하단, 폰트 크게)
    label_updated = lv_label_create(ui_screen);
    lv_obj_set_width(label_updated, 480);
    lv_label_set_text(label_updated, "Last Sync: Never");
    lv_obj_set_style_text_align(label_updated, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_updated, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_updated, lv_color_hex(0x888888), 0);
    lv_obj_align(label_updated, LV_ALIGN_BOTTOM_MID, 0, -10);

    apply_theme();
}

void update_dashboard(String json_str) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json_str);

    if (error) {
        Serial.print("JSON Parse Failed: ");
        lv_led_set_color(led_status, lv_palette_main(LV_PALETTE_RED));
        lv_led_on(led_status);
        return;
    }

    const char* uptime = doc["uptime"];
    const char* updated_at = doc["updated_at"];
    float cpu_5m = doc["cpu_load"]["5min"];
    float mem_percent = doc["memory"]["percent"];
    int mem_used = doc["memory"]["used_mb"];
    int mem_total = doc["memory"]["total_mb"];
    int disk_percent = doc["disk"]["percent"];

    bsp_display_lock(0);
    
    char sync_buf[128];
    sprintf(sync_buf, "Last Sync: %s", updated_at);
    lv_label_set_text(label_updated, sync_buf);
    
    // CPU 로드 표시
    float cpu_percent = cpu_5m * 100.0;
    int cpu_bar_value = (cpu_percent <= 5.0 && cpu_percent > 0) ? 5 : (int)cpu_percent;
    lv_bar_set_value(bar_cpu, cpu_bar_value, LV_ANIM_ON);
    
    char cpu_buf[32];
    sprintf(cpu_buf, "%.1f%%", cpu_percent);
    lv_label_set_text(label_cpu_val, cpu_buf);

    // 메모리 표시
    lv_bar_set_value(bar_mem, (int)mem_percent, LV_ANIM_ON);
    
    char mem_buf[32];
    sprintf(mem_buf, "%.1f%%", mem_percent);
    lv_label_set_text(label_mem_val, mem_buf);

    // 디스크 표시
    lv_bar_set_value(bar_disk, disk_percent, LV_ANIM_ON);
    
    char disk_buf[32];
    sprintf(disk_buf, "%d%%", disk_percent);
    lv_label_set_text(label_disk_val, disk_buf);

    // Uptime 표시 (주+일을 합쳐서 총 일수로 변환)
    char uptime_buf[64];
    int weeks = 0, days = 0, hours = 0, minutes = 0;
    
    // "24 weeks, 6 days, 18 hours, 28 minutes" 형식 파싱
    sscanf(uptime, "%d weeks, %d days, %d hours, %d minutes", &weeks, &days, &hours, &minutes);
    
    // 주가 없는 경우도 처리
    if (weeks == 0) {
        sscanf(uptime, "%d days, %d hours, %d minutes", &days, &hours, &minutes);
    }
    
    int total_days = weeks * 7 + days;
    sprintf(uptime_buf, "%d days, %dh %dm", total_days, hours, minutes);
    lv_label_set_text(label_uptime_val, uptime_buf);

    // LED 깜빡임 효과
    lv_led_off(led_status);
    bsp_display_unlock();
    delay(100);
    bsp_display_lock(0);
    lv_led_set_color(led_status, lv_color_hex(0x00FF00));
    lv_led_on(led_status);
    bsp_display_unlock();
}

void fetch_server_data() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure *client = new WiFiClientSecure;
        if(client) {
            client->setInsecure();
            {
                HTTPClient http;
                http.begin(*client, servers[current_server].url);
                int httpCode = http.GET();

                if (httpCode > 0) {
                    String payload = http.getString();
                    update_dashboard(payload);
                } else {
                    Serial.printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
                    bsp_display_lock(0);
                    lv_led_set_color(led_status, lv_palette_main(LV_PALETTE_RED));
                    lv_led_on(led_status);
                    bsp_display_unlock();
                }
                http.end();
            }
            delete client;
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90,
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    setup_ui();
    bsp_display_unlock();

    WiFi.begin(ssid, password);
    
    bsp_display_lock(0);
    lv_label_set_text(label_uptime_val, "Connecting...");
    bsp_display_unlock();

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected! Monitoring: %s\n", servers[current_server].name);
        fetch_server_data();
    } else {
        bsp_display_lock(0);
        lv_label_set_text(label_uptime_val, "Wi-Fi Error!");
        bsp_display_unlock();
    }
}

void loop() {
    unsigned long now = millis();
    if (now - last_update_ms > update_interval) {
        last_update_ms = now;
        fetch_server_data();
    }
    delay(10);
}
