#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <lvgl.h>

#include "Inconsolata_16px.h"
#include "Inconsolata_18px.h"
#include "Inconsolata_26px.h"

/* ----- LVGL OBJECTS ----- */
lv_obj_t* scr;

lv_obj_t* DividerLine1;
lv_obj_t* DividerLine2;
static lv_point_t line_points1[] = {{0, 0}, {300, 0}};
static lv_point_t line_points2[] = {{0, 0}, {0, 65}};

/* -- LVGL OBJECT STRUCTS -- */
struct NetworkUI {
    lv_obj_t* TypeTitle; lv_obj_t* TypeLabel;

    lv_obj_t* PingLabel; lv_obj_t* PingIndicator;
    lv_obj_t* JitterLabel; lv_obj_t* JitterIndicator;
    lv_obj_t* PacketLossLabel; lv_obj_t* PacketLossIndicator;
};

struct CpuUI {
    lv_obj_t* LoadLabel; lv_obj_t* TempLabel;
    lv_obj_t* LoadBar;
};
struct GpuUI {
    lv_obj_t* LoadLabel; lv_obj_t* TempLabel;
    lv_obj_t* LoadBar;
};
struct RamUI {
    lv_obj_t* UsageLabel; lv_obj_t* PercentLabel;
    lv_obj_t* PercentBar;
};

NetworkUI networkUI;
CpuUI     cpuUI;
GpuUI     gpuUI;
RamUI     ramUI;

/* --- LVGL COLORS --- */
#define COLOR_DIM      lv_color_make(192, 192, 192)
#define COLOR_OK       lv_color_make(0, 240, 0)
#define COLOR_WARN     lv_color_make(255, 192, 0)
#define COLOR_ERR      lv_color_make(255, 0, 0)
#define COLOR_NA       lv_color_make(96, 96, 96)
#define COLOR_INACTIVE lv_color_make(64, 64, 64)

/* --- DATA STRUCTS --- */
struct NetworkData {
    char network_type[32] = "N/A";
    int ping = -1, jitter = -1, packet_loss = -1;
};

struct CpuData { int cpu_load = -1; int cpu_temp = -1; };
struct GpuData { int gpu_load = -1; int gpu_temp = -1; };
struct RamData { float ram_used = -1; float ram_total = -1; int ram_percent = -1; };

NetworkData networkData;
CpuData cpuData;
GpuData gpuData;
RamData ramData;

/* -- TIMEOUT -- */
unsigned long lastDataTime = 0;        // Time when last data arrived
const uint32_t DATA_TIMEOUT_MS = 5000; // 5 Seconds
static bool cleared = false;

/* -- BUFFERS -- */
char stringBuffer[64];
char jsonBuffer[512];
size_t jsonPos = 0;

/* --- FLAGS --- */
bool messageReady = false;
bool newDataAvailable = false;

/* --- SCREEN --- */
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
#define BUF_HEIGHT 50
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * BUF_HEIGHT];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* LVGL Object Updaters */
/* int labels updater */
void updateLabelInt(lv_obj_t* label, const char* fmt, const char* naFmt, int value) {
    if (value >= 0) {
        snprintf(stringBuffer, sizeof(stringBuffer), fmt, value);
    } else {
        snprintf(stringBuffer, sizeof(stringBuffer), naFmt);
    }
    lv_label_set_text(label, stringBuffer);
}

/* float labels updater */
/*
void updateLabelFloat(lv_obj_t* label, const char* fmt, const char* naFmt, float value) {
    if (value >= 0) {
        snprintf(stringBuffer, sizeof(stringBuffer), fmt, value);
    } else {
        snprintf(stringBuffer, sizeof(stringBuffer), naFmt);
    }
    lv_label_set_text(label, stringBuffer);
}
*/

/* Percentage bar updater */
inline void updatePercentBar(lv_obj_t* bar, int percent, lv_color_t color) {
    if (percent >= 0) {
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        lv_bar_set_value(bar, percent, LV_ANIM_OFF);
    } else {
        lv_obj_set_style_bg_color(bar, COLOR_INACTIVE, LV_PART_INDICATOR);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    }
}

// All color helpers use: ERR → WARN → OK (high = bad)

/* Network metrics color helpers */
// Ping (ms)
lv_color_t getPingColor(int ping, const char* network_type) {
    if (ping < 0) return COLOR_NA;

    if (strcmp(network_type, "Wi-Fi") == 0) {
        if (ping > 50) return COLOR_ERR;
        if (ping > 20) return COLOR_WARN;
    } else if (strcmp(network_type, "Ethernet") == 0) {
        if (ping > 20) return COLOR_ERR;
        if (ping > 10) return COLOR_WARN;
    } else {
        return COLOR_NA;
    }
    return COLOR_OK;
}

// Jitter (ms)
lv_color_t getJitterColor(float jitter, const char* network_type) {
    if (jitter < 0) return COLOR_NA;
    
    if (strcmp(network_type, "Wi-Fi") == 0) {
        if (jitter > 10.0f) return COLOR_ERR;
        if (jitter > 5.0f)  return COLOR_WARN;
    } else if (strcmp(network_type, "Ethernet") == 0) {
        if (jitter > 5.0f) return COLOR_ERR;
        if (jitter > 2.0f) return COLOR_WARN;
    } else {
        return COLOR_NA;
    }
    return COLOR_OK;
}

// Packet loss (%)
lv_color_t getPacketLossColor(float loss, const char* network_type) {
    if (loss < 0) return COLOR_NA;

    if (strcmp(network_type, "Wi-Fi") == 0) {
        if (loss > 2.0f) return COLOR_ERR;
        if (loss > 0.5f) return COLOR_WARN;
    } else if (strcmp(network_type, "Ethernet") == 0) {
        if (loss > 1.0f) return COLOR_ERR;
        if (loss > 0.1f) return COLOR_WARN;
    } else {
        return COLOR_NA;
    }
    return COLOR_OK;
}

/* Metrics color helpers */
// CPU
lv_color_t getCPUbarColor(int load) {
    if (load < 0) return COLOR_NA;
    if (load > 75) return COLOR_ERR;
    if (load > 50) return COLOR_WARN;
    return COLOR_OK;
}

// GPU
lv_color_t getGPUbarColor(int load) {
    if (load < 0) return COLOR_NA;
    if (load > 75) return COLOR_ERR;
    if (load > 50) return COLOR_WARN;
    return COLOR_OK;
}

// RAM
lv_color_t getRAMpercentColor(int percent) {
    if (percent < 0) return COLOR_NA;
    if (percent > 80) return COLOR_ERR;
    if (percent > 60) return COLOR_WARN;
    return COLOR_DIM;
}

lv_color_t getRAMbarColor(int usage) {
    if (usage < 0) return COLOR_NA;
    if (usage > 80) return COLOR_ERR;
    if (usage > 60) return COLOR_WARN;
    return COLOR_OK;
}

/* Temperatures */
enum TempSource {
    CPU,
    GPU
};

lv_color_t getTempColor(int temp, TempSource src) {
    if (temp < 0) return COLOR_NA;

    if (src == CPU) {
        if (temp >= 85) return COLOR_ERR;
        if (temp >= 70) return COLOR_WARN;
    } else if (src == GPU) {
        if (temp >= 85) return COLOR_ERR;
        if (temp >= 75) return COLOR_WARN;
    }
    return COLOR_DIM;
}

/* LVGL Object helpers */
lv_obj_t* createLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, lv_align_t align, int x, int y, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* createBar(lv_obj_t* parent, lv_color_t bgColor, int width, int height, lv_align_t align, int x, int y) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_style_bg_color(bar, bgColor, LV_PART_MAIN);
    lv_obj_set_size(bar, width, height);
    lv_obj_align(bar, align, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    return bar;
}

/* UI Drawing */
void drawUI() {
    /* --- NETWORK --- */
    // Network Type
    networkUI.TypeTitle = createLabel(scr, &Inconsolata_18px, lv_color_white(), LV_ALIGN_TOP_RIGHT, -10, 10, "Network Type");
    networkUI.TypeLabel = createLabel(scr, &Inconsolata_26px, lv_color_white(), LV_ALIGN_TOP_RIGHT, -10, 34, "N/A");

    // Ping
    networkUI.PingLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_TOP_LEFT, 10, 10, "Ping     N/A ms");
    networkUI.PingIndicator = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_TOP_MID, -2, 10, "●");

    // Jitter
    networkUI.JitterLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_TOP_LEFT, 10, 31, "Jitter   N/A ms");
    networkUI.JitterIndicator = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_TOP_MID, -2, 31, "●");

    // Packet Loss
    networkUI.PacketLossLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_TOP_LEFT, 10, 52, "Pkt loss  N/A %");
    networkUI.PacketLossIndicator = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_TOP_MID, -2, 52, "●");

    /* --- DIVIDERS --- */
    DividerLine1 = lv_line_create(scr);
    lv_line_set_points(DividerLine1, line_points1, 2);
    lv_obj_set_style_line_color(DividerLine1, lv_color_white(), 0);
    lv_obj_set_style_line_width(DividerLine1, 3, 0);
    lv_obj_set_style_line_rounded(DividerLine1, true, 0);
    lv_obj_align(DividerLine1, LV_ALIGN_TOP_MID, 0, 77);

    DividerLine2 = lv_line_create(scr);
    lv_line_set_points(DividerLine2, line_points2, 2);
    lv_obj_set_style_line_color(DividerLine2, lv_color_white(), 0);
    lv_obj_set_style_line_width(DividerLine2, 3, 0);
    lv_obj_set_style_line_rounded(DividerLine2, true, 0);
    lv_obj_align(DividerLine2, LV_ALIGN_TOP_MID, 15, 10);

    /* ----- CPU ----- */
    // CPU Load
    cpuUI.LoadLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_BOTTOM_LEFT, 10, -135, "CPU: N/A %");
    // CPU Temp
    cpuUI.TempLabel = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_BOTTOM_RIGHT, -10, -135, "(N/A°C)");

    // CPU Bar
    cpuUI.LoadBar = createBar(scr, COLOR_INACTIVE, 300, 16, LV_ALIGN_BOTTOM_MID, 0, -114);

    /* ----- GPU ----- */
    // GPU Load
    gpuUI.LoadLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_BOTTOM_LEFT, 10, -83, "GPU: N/A % ");
    // GPU Temp
    gpuUI.TempLabel = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_BOTTOM_RIGHT, -10, -83, "(N/A°C)");

    // GPU Bar
    gpuUI.LoadBar = createBar(scr, COLOR_INACTIVE, 300, 16, LV_ALIGN_BOTTOM_MID, 0, -62);

    /* ----- RAM ----- */
    // RAM Usage
    ramUI.UsageLabel = createLabel(scr, &Inconsolata_16px, lv_color_white(), LV_ALIGN_BOTTOM_LEFT, 10, -31, "RAM: N/A GB");
    // RAM Percentage
    ramUI.PercentLabel = createLabel(scr, &Inconsolata_16px, COLOR_NA, LV_ALIGN_BOTTOM_RIGHT, -10, -31, "(N/A %)");

    // RAM Bar
    ramUI.PercentBar = createBar(scr, COLOR_INACTIVE, 300, 16, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/* UI Updating */
void updateUI() {
    if (!lv_disp_get_default()) return;

    /* --- NETWORK --- */
    // Ping
    lv_obj_set_style_text_color(networkUI.PingIndicator, getPingColor(networkData.ping, networkData.network_type), LV_PART_MAIN);
    updateLabelInt(networkUI.PingLabel, "Ping      %3dms", "Ping     N/A ms", networkData.ping);

    // Jitter
    lv_obj_set_style_text_color(networkUI.JitterIndicator, getJitterColor(networkData.jitter, networkData.network_type), LV_PART_MAIN);
    updateLabelInt(networkUI.JitterLabel, "Jitter    %3dms", "Jitter   N/A ms", networkData.jitter);
    
    // Packet Loss 
    lv_obj_set_style_text_color(networkUI.PacketLossIndicator, getPacketLossColor(networkData.packet_loss, networkData.network_type), LV_PART_MAIN);
    updateLabelInt(networkUI.PacketLossLabel, "Pkt loss   %3d%%", "Pkt loss  N/A %%", networkData.packet_loss);

    // Network Type
    lv_label_set_text(networkUI.TypeLabel, networkData.network_type);


    /* ----- CPU ----- */
    // CPU Load
    updateLabelInt(cpuUI.LoadLabel, "CPU: %3d%%", "CPU: N/A %%", cpuData.cpu_load);
    // CPU Temp
    lv_obj_set_style_text_color(cpuUI.TempLabel, getTempColor(cpuData.cpu_temp, CPU), LV_PART_MAIN);
    updateLabelInt(cpuUI.TempLabel, "(%d°C)", "(N/A°C)", cpuData.cpu_temp);

    // CPU Bar
    updatePercentBar(cpuUI.LoadBar, cpuData.cpu_load, getCPUbarColor(cpuData.cpu_load));
    
    /* ----- GPU ----- */
    // GPU Load
    updateLabelInt(gpuUI.LoadLabel, "GPU: %3d%%", "GPU: N/A %%", gpuData.gpu_load);
    // GPU Temp
    lv_obj_set_style_text_color(gpuUI.TempLabel, getTempColor(gpuData.gpu_temp, GPU), LV_PART_MAIN);
    updateLabelInt(gpuUI.TempLabel, "(%d°C)", "(N/A°C)", gpuData.gpu_temp);

    // GPU Bar
    updatePercentBar(gpuUI.LoadBar, gpuData.gpu_load, getGPUbarColor(gpuData.gpu_load));

    /* ----- RAM ----- */
    // RAM Usage
    if (ramData.ram_used >= 0 && ramData.ram_total >= 0) { lv_label_set_text_fmt(ramUI.UsageLabel, "RAM: %4.1f/%.1f GB", ramData.ram_used, ramData.ram_total); }
    else { lv_label_set_text(ramUI.UsageLabel, "RAM: N/A GB"); }
    // RAM Percentage
    lv_obj_set_style_text_color(ramUI.PercentLabel, getRAMpercentColor(ramData.ram_percent), LV_PART_MAIN);
    updateLabelInt(ramUI.PercentLabel, "(%d%%)", "(N/A %%)", ramData.ram_percent);

    // RAM Bar
    updatePercentBar(ramUI.PercentBar, ramData.ram_percent, getRAMbarColor(ramData.ram_percent));
}

void clearUI() {
    strcpy(networkData.network_type, "N/A");
    networkData.ping = networkData.jitter = networkData.packet_loss = -1;

    cpuData.cpu_load = cpuData.cpu_temp = -1;
    gpuData.gpu_load = gpuData.gpu_temp = -1;
    ramData.ram_used = ramData.ram_total = ramData.ram_percent = -1;
    
    updateUI();
}

/* ----- Serial / JSON ----- */
void readSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            jsonBuffer[jsonPos] = '\0';
            messageReady = true;
            jsonPos = 0;
            return;
        } else if (jsonPos < sizeof(jsonBuffer) - 1) {
            jsonBuffer[jsonPos++] = c;
        }
    }
}

void parseJsonData(const char* input) {
    StaticJsonDocument<512> doc;

    // Attempt to deserialize JSON
    DeserializationError error = deserializeJson(doc, input);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    /* --- NETWORK --- */
    JsonObject network = doc["network"];
    const char* tmpStr = network["network_type"] | "N/A";  // use default "N/A" if missing

    strncpy(networkData.network_type, tmpStr, sizeof(networkData.network_type) - 1);
    networkData.network_type[sizeof(networkData.network_type) - 1] = '\0'; // ensure null-terminated
    networkData.ping = network["ping"].is<int>() ? network["ping"].as<int>() : -1;
    networkData.jitter = network["jitter"].is<int>() ? network["jitter"].as<int>() : -1;
    networkData.packet_loss = network["packet_loss"].is<int>() ? network["packet_loss"].as<int>() : -1;

    /* ----- CPU ----- */
    JsonObject cpu = doc["cpu"];
    cpuData.cpu_load = cpu["load"].is<int>() ? cpu["load"].as<int>() : -1;
    cpuData.cpu_temp = cpu["temp"].is<int>() ? cpu["temp"].as<int>() : -1;

    /* ----- GPU ----- */
    JsonObject gpu = doc["gpu"];
    gpuData.gpu_load = gpu["load"].is<int>() ? gpu["load"].as<int>() : -1;
    gpuData.gpu_temp = gpu["temp"].is<int>() ? gpu["temp"].as<int>() : -1;

    /* ----- RAM ----- */
    JsonObject ram = doc["ram"];
    ramData.ram_used = ram["used_gb"].is<float>() ? ram["used_gb"].as<float>() : -1;
    ramData.ram_total = ram["total_gb"].is<float>() ? ram["total_gb"].as<float>() : -1;
    ramData.ram_percent = ram["percent"].is<int>() ? ram["percent"].as<int>() : -1;

    lastDataTime = millis();
    newDataAvailable = true;
}

void setup() {
    Serial.begin(115200); /* prepare for possible serial debug */
    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

    tft.begin();          /* TFT init */
    tft.setRotation(1); /* Landscape orientation */

    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * BUF_HEIGHT);

    /* Initialize the display */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize screen */
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    drawUI();
}

void loop() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last >= 5) {
        lv_tick_inc(now - last);
        last = now;
    }
    lv_timer_handler();


    readSerial();
    if (messageReady) {
        parseJsonData(jsonBuffer);
        messageReady = false;
    }

    // Update UI if new data came in
    if (newDataAvailable) {
        updateUI();
        newDataAvailable = false;
        cleared = false;
    }

    if (!cleared && millis() - lastDataTime > DATA_TIMEOUT_MS) {
        clearUI();
        cleared = true;
    }
}