#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"

// -------------------------------------------------------------------
// ⚠️ Wi-Fi 설정
// -------------------------------------------------------------------
const char* ssid = "MIHO-2.4G";
const char* password = "20140312";

// ⚠️ Quota API 서버 주소 (로컬 서버 IP로 변경하세요)
const char* quota_api_url = "http://192.168.0.185:3000/quota.json";

#define LVGL_PORT_ROTATION_DEGREE (90)
#define MAX_QUOTAS 10

// Quota 데이터 구조
struct QuotaItem {
    char name[50];
    float percent;
    char reset_time[20];
    char status[10];  // green, yellow, red
};

QuotaItem quotas[MAX_QUOTAS];
int quota_count = 0;

// UI 요소
static lv_obj_t *ui_screen;
static lv_obj_t *label_title;
static lv_obj_t *label_updated;
static lv_obj_t *scroll_container;
static lv_obj_t *quota_items[MAX_QUOTAS];

// 데이터 갱신 타이머
static unsigned long last_update_ms = 0;
const unsigned long update_interval = 60000; // 60초마다 갱신

// 테마
enum Theme {
    THEME_DARK,
    THEME_LIGHT
};

static Theme current_theme = THEME_DARK;

lv_color_t get_status_color(const char* status) {
    if (strcmp(status, "green") == 0) {
        return lv_color_hex(0x00FF00);
    } else if (strcmp(status, "yellow") == 0) {
        return lv_color_hex(0xFFD700);
    } else {
        return lv_color_hex(0xFF0000);
    }
}

void apply_theme() {
    if (current_theme == THEME_DARK) {
        lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0x0a0a0a), 0);
        lv_obj_set_style_text_color(label_title, lv_color_hex(0x00BFFF), 0);
        lv_obj_set_style_text_color(label_updated, lv_color_hex(0x888888), 0);
    } else {
        lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_text_color(label_title, lv_color_hex(0x1976D2), 0);
        lv_obj_set_style_text_color(label_updated, lv_color_hex(0x666666), 0);
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
}

void setup_ui() {
    ui_screen = lv_scr_act();
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(0x0a0a0a), 0);
    
    // 화면 터치 이벤트 등록
    lv_obj_add_flag(ui_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_screen, screen_event_cb, LV_EVENT_ALL, NULL);
    
    // 타이틀
    label_title = lv_label_create(ui_screen);
    lv_label_set_text(label_title, "Antigravity Quota Monitor");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x00BFFF), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

    // 스크롤 컨테이너
    scroll_container = lv_obj_create(ui_screen);
    lv_obj_set_size(scroll_container, 460, 240);
    lv_obj_align(scroll_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 5, 0);
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Last Sync
    label_updated = lv_label_create(ui_screen);
    lv_obj_set_width(label_updated, 480);
    lv_label_set_text(label_updated, "Last Sync: Never");
    lv_obj_set_style_text_align(label_updated, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_updated, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_updated, lv_color_hex(0x888888), 0);
    lv_obj_align(label_updated, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void create_quota_item(int index) {
    if (index >= MAX_QUOTAS) return;
    
    QuotaItem *q = &quotas[index];
    
    // 카드 생성
    lv_obj_t *card = lv_obj_create(scroll_container);
    lv_obj_set_size(card, 440, 50);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // 상태 LED
    lv_obj_t *led = lv_led_create(card);
    lv_obj_set_size(led, 12, 12);
    lv_obj_align(led, LV_ALIGN_LEFT_MID, 0, 0);
    lv_led_set_color(led, get_status_color(q->status));
    lv_led_on(led);
    
    // 모델명
    lv_obj_t *label_name = lv_label_create(card);
    lv_label_set_text(label_name, q->name);
    lv_obj_set_style_text_font(label_name, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_name, lv_color_hex(0xffffff), 0);
    lv_obj_align(label_name, LV_ALIGN_LEFT_MID, 20, 0);
    
    // 프로그레스 바
    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_set_size(bar, 100, 10);
    lv_obj_align(bar, LV_ALIGN_RIGHT_MID, -100, 0);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, (int)q->percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, get_status_color(q->status), LV_PART_INDICATOR);
    
    // 퍼센트 + 리셋 시간
    char info_text[50];
    sprintf(info_text, "%.1f%% | %s", q->percent, q->reset_time);
    lv_obj_t *label_info = lv_label_create(card);
    lv_label_set_text(label_info, info_text);
    lv_obj_set_style_text_font(label_info, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_info, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_info, LV_ALIGN_RIGHT_MID, -5, 0);
    
    quota_items[index] = card;
}

void update_quota_display() {
    bsp_display_lock(0);
    
    // 기존 아이템 삭제
    lv_obj_clean(scroll_container);
    
    // 새 아이템 생성
    for (int i = 0; i < quota_count; i++) {
        create_quota_item(i);
    }
    
    bsp_display_unlock();
}

void fetch_quota_data() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(quota_api_url);
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            
            // JSON 파싱
            StaticJsonDocument<4096> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                const char* updated_at = doc["updated_at"];
                JsonArray quotas_array = doc["quotas"].as<JsonArray>();
                
                quota_count = 0;
                for (JsonObject quota_obj : quotas_array) {
                    if (quota_count >= MAX_QUOTAS) break;
                    
                    strncpy(quotas[quota_count].name, quota_obj["name"], 49);
                    quotas[quota_count].percent = quota_obj["percent"];
                    strncpy(quotas[quota_count].reset_time, quota_obj["reset_time"], 19);
                    strncpy(quotas[quota_count].status, quota_obj["status"], 9);
                    
                    quota_count++;
                }
                
                update_quota_display();
                
                bsp_display_lock(0);
                char sync_buf[128];
                sprintf(sync_buf, "Last Sync: %s", updated_at);
                lv_label_set_text(label_updated, sync_buf);
                bsp_display_unlock();
                
                Serial.printf("✅ Quota 데이터 갱신 완료: %d개 항목\n", quota_count);
            } else {
                Serial.printf("❌ JSON 파싱 실패: %s\n", error.c_str());
            }
        } else {
            Serial.printf("❌ HTTP GET 실패: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("❌ Wi-Fi 연결 끊김");
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
    
    Serial.print("Wi-Fi 연결 중");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("✅ Wi-Fi 연결 성공!\n");
        Serial.printf("📡 IP 주소: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("🔗 API URL: %s\n", quota_api_url);
        fetch_quota_data();
    } else {
        Serial.println("❌ Wi-Fi 연결 실패!");
        bsp_display_lock(0);
        lv_label_set_text(label_updated, "Wi-Fi Error!");
        bsp_display_unlock();
    }
}

void loop() {
    unsigned long now = millis();
    if (now - last_update_ms > update_interval) {
        last_update_ms = now;
        fetch_quota_data();
    }
    delay(10);
}
