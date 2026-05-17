#include <string.h>
#include <stdlib.h> // За atoi
#include <stdint.h> // За int16_t
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "driver/i2c.h"
#pragma GCC diagnostic pop
#include "esp_http_server.h"

#define TAG "ESP32_I2C_CAM"

#define I2C_SDA_GPIO 8
#define I2C_SCL_GPIO 9
#define I2C_DEV_ADDR 0x66
#define I2C_FREQ_HZ 400000

#define DATA_COUNT 768
#define RAW_BUF_SIZE (DATA_COUNT * 2)
#define I2C_REG_HIGH 0x34
#define I2C_REG_LOW 0x2c
#define AMBIENT_REG_HIGH 0x3A
#define AMBIENT_REG_LOW 0x2C
#define RATE_REG_HIGH 0x11
#define RATE_REG_LOW 0xF0

#define AP_SSID "ESP32-I2C-CAM"
#define AP_PASS "12345678"
#define REFRESH_DELAY_MS 62

static uint8_t s_pixel_buf[DATA_COUNT + 6];
static SemaphoreHandle_t s_buf_mutex = NULL;

// ✅ ПРОМЯНА: Използваме int16_t за Two's Complement (знакови числа)
static int16_t s_raw_data[DATA_COUNT];
static uint8_t s_i2c_buf[RAW_BUF_SIZE];

static void init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C: SDA=%d, SCL=%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
}

static void i2c_read_task(void *pvParameters)
{
    while (1)
    {
        // 1. Четене на пиксели
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (I2C_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, I2C_REG_HIGH, true);
        i2c_master_write_byte(cmd, I2C_REG_LOW, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (I2C_DEV_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, s_i2c_buf, RAW_BUF_SIZE, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "I2C pixels err: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(REFRESH_DELAY_MS));
            continue;
        }

        // 2. Четене на околна температура
        uint8_t amb_buf[2];
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (I2C_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, AMBIENT_REG_HIGH, true);
        i2c_master_write_byte(cmd, AMBIENT_REG_LOW, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (I2C_DEV_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, amb_buf, 2, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        float ambient_c = 0.0f;
        if (ret == ESP_OK)
        {
            // ✅ ПРОМЯНА: Конвертиране в знаково число (Two's Complement)
            int16_t amb_raw = (int16_t)((amb_buf[0] << 8) | amb_buf[1]);
            ambient_c = (float)amb_raw / 50.0f;
        }

        // 3. Обработка на данните (Two's Complement)
        int16_t min_val = 32767, max_val = -32768; // ✅ Инициализация за int16_t

        for (int i = 0; i < DATA_COUNT; i++)
        {
            // ✅ ПРОМЯНА: Четене като знаково 16-битово число
            s_raw_data[i] = (int16_t)((s_i2c_buf[i * 2] << 8) | s_i2c_buf[i * 2 + 1]);
            if (s_raw_data[i] < min_val)
                min_val = s_raw_data[i];
            if (s_raw_data[i] > max_val)
                max_val = s_raw_data[i];
        }

        // Изчисляване на диапазон (използваме int32 за безопасност при изчисление)
        int32_t range = (int32_t)max_val - (int32_t)min_val;
        if (range == 0)
            range = 1;

        uint8_t pixels[DATA_COUNT];
        for (int i = 0; i < DATA_COUNT; i++)
        {
            // Нормализация за визуализация (0-255)
            int32_t val = s_raw_data[i];
            pixels[i] = ((val - min_val) * 255) / range;
        }

        // 4. Пакетиране: 768 пиксела + 6 байта статистика (int16 BE, ×10)
        uint8_t resp[DATA_COUNT + 6];
        memcpy(resp, pixels, DATA_COUNT);

        float min_c = (float)min_val / 50.0f;
        float max_c = (float)max_val / 50.0f;

        int16_t m16 = (int16_t)(min_c * 10.0f);
        resp[768] = (m16 >> 8) & 0xFF;
        resp[769] = m16 & 0xFF;
        int16_t M16 = (int16_t)(max_c * 10.0f);
        resp[770] = (M16 >> 8) & 0xFF;
        resp[771] = M16 & 0xFF;
        int16_t A16 = (int16_t)(ambient_c * 10.0f);
        resp[772] = (A16 >> 8) & 0xFF;
        resp[773] = A16 & 0xFF;

        xSemaphoreTake(s_buf_mutex, portMAX_DELAY);
        memcpy(s_pixel_buf, resp, sizeof(resp));
        xSemaphoreGive(s_buf_mutex);
        vTaskDelay(pdMS_TO_TICKS(REFRESH_DELAY_MS));
    }
}

// ✅ HTTP Handler за запис на честота в регистър 0x11F0
static esp_err_t set_rate_handler(httpd_req_t *req)
{
    char buf[64];
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1 || httpd_req_get_url_query_str(req, buf, qlen) != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
        return ESP_FAIL;
    }
    char val[8];
    if (httpd_query_key_value(buf, "rate", val, sizeof(val)) != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing rate param");
        return ESP_FAIL;
    }

    int rate = atoi(val);
    int reg_val = 0;
    if (rate == 2)
        reg_val = 2;
    else if (rate == 4)
        reg_val = 3;
    else if (rate == 8)
        reg_val = 4;
    else if (rate == 16)
        reg_val = 5;
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid rate");
        return ESP_FAIL;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, RATE_REG_HIGH, true);
    i2c_master_write_byte(cmd, RATE_REG_LOW, true);
    i2c_master_write_byte(cmd, (uint8_t)reg_val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
    {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "OK", 2);
        ESP_LOGI(TAG, "Rate set to %dHz (reg 0x11F0 = %d)", rate, reg_val);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "I2C write failed");
    }
    return ret;
}

static const char index_html[] =
    "<html>"
    "<head>"
        "<meta charset='UTF-8'>"
        "<title>Thermal</title>"
        "<style>"
            "body { background: #000; color: #fff; font-family: monospace; text-align: center; padding: 10px; margin: 0; }"
            "canvas { border: 2px solid #444; background: #111; }"
            "#c { width: 640px; height: 480px; display: block; margin: 10px auto; }"
            "#bar { width: 640px; height: 30px; display: block; margin: 0 auto; }"
            "button, select { padding: 10px 15px; margin: 5px; font-size: 14px; background: #333; color: #fff; border: 1px solid #666; border-radius: 4px; cursor: pointer; }"
            "select { background: #222; }"
            "#st { color: #aaa; margin: 5px; }"
            "#inf { color: #4af; font-weight: bold; margin: 5px 0; }"
            "#crd { color: #ffa; font-size: 13px; margin: 5px 0; font-weight: bold; }"
            "#lbl { display: flex; justify-content: space-between; width: 640px; margin: 0 auto; font-size: 11px; color: #ccc; }"
        "</style>"
    "</head>"
    "<body>"
        "<h3>ESP32 Thermal Camera</h3>"
        "<canvas id='c' width='160' height='120'></canvas>"
        "<div id='st'>Ready</div>"
        "<div id='inf'>Min:--.-C | Max:--.-C | Amb:--.-C</div>"
        "<div id='crd'>Min: (0,0) | Max: (0,0)</div>"

        "<!-- Падащо меню за честота -->"
        "<select id='rateSel'>"
            "<option value='2'>2 Hz</option>"
            "<option value='4' selected>4 Hz</option>"
            "<option value='8'>8 Hz</option>"
            "<option value='16'>16 Hz</option>"
        "</select> "

        "<button onclick='setRate()'>⚡ Set Rate</button>"
        "<button onclick='doTest()'>Test</button> "
        "<button onclick='fetchData()'>Refresh</button> "
        "<button onclick='togglePal()'>🎨 Palette</button> "
        "<button onclick='toggleMarkers()'>📍 Markers</button>"

        "<canvas id='bar' width='640' height='30'></canvas>"
        "<div id='lbl'></div>"

        "<script>"
            "var c = document.getElementById('c'), x = c.getContext('2d');"
            "x.imageSmoothingEnabled = true;"
            "var r = document.createElement('canvas'); r.width = 32; r.height = 24;"
            "var rx = r.getContext('2d'), id = rx.createImageData(32, 24);"
            "var b = document.getElementById('bar'), bx = b.getContext('2d'), bd = bx.createImageData(640, 30);"
            "var st = document.getElementById('st'), inf = document.getElementById('inf'), crd = document.getElementById('crd'), lb = document.getElementById('lbl');"
            
            "for (var i = 0; i < 10; i++) {"
                "var s = document.createElement('span');"
                "lb.appendChild(s);"
            "}"
            "var sp = lb.children;"
            "var palMode = 0, showMarkers = 1;"

            "function togglePal() {"
                "palMode = 1 - palMode;"
                "st.innerHTML = 'Palette: ' + (palMode ? 'Thermal' : 'Grayscale');"
                "fetchData();"
            "}"

            "function toggleMarkers() {"
                "showMarkers = 1 - showMarkers;"
                "st.innerHTML = 'Markers: ' + (showMarkers ? 'ON' : 'OFF');"
                "fetchData();"
            "}"

            "function setRate() {"
                "var r = document.getElementById('rateSel').value;"
                "st.innerHTML = 'Set ' + r + 'Hz...';"
                "var q = new XMLHttpRequest();"
                "q.open('GET', '/set_rate?rate=' + r, true);"
                "q.onload = function() {"
                    "if (q.status === 200) st.innerHTML = 'Rate ' + r + 'Hz OK';"
                    "else st.innerHTML = 'Fail: ' + q.status;"
                "};"
                "q.onerror = function() { st.innerHTML = 'Net Err'; };"
                "q.send();"
            "}"

            "function gc(v) {"
                "if (palMode === 0) { return [v, v, v]; }"
                "if (v < 64) { return [0, Math.round(v * 4), 255]; }"
                "else if (v < 128) { return [0, 255, Math.round(255 - (v - 64) * 4)]; }"
                "else if (v < 192) { return [Math.round((v - 128) * 4), 255, 0]; }"
                "else { return [255, Math.round(255 - (v - 192) * 4), 0]; }"
            "}"

            "function dr(p) {"
                "var minIdx = 0, maxIdx = 0, minV = 256, maxV = -1;"
                "for (var j = 0; j < 768; j++) {"
                    "var v = p[j], k = j * 4, c = gc(v);"
                    "id.data[k] = c[0]; id.data[k+1] = c[1]; id.data[k+2] = c[2]; id.data[k+3] = 255;"
                    "if (v < minV) { minV = v; minIdx = j; }"
                    "if (v > maxV) { maxV = v; maxIdx = j; }"
                "}"
                "var x1 = minIdx % 32, y1 = Math.floor(minIdx / 32);"
                "var x2 = maxIdx % 32, y2 = Math.floor(maxIdx / 32);"
                "crd.innerHTML = 'Min: (' + x1 + ',' + y1 + ') | Max: (' + x2 + ',' + y2 + ')';"
                "if (showMarkers) {"
                    "var kM = minIdx * 4, kX = maxIdx * 4;"
                    "if (palMode === 0) {"
                        "id.data[kM] = 0; id.data[kM+1] = 0; id.data[kM+2] = 255;"
                        "id.data[kX] = 255; id.data[kX+1] = 0; id.data[kX+2] = 0;"
                    "} else {"
                        "id.data[kM] = 0; id.data[kM+1] = 0; id.data[kM+2] = 0;"
                        "id.data[kX] = 255; id.data[kX+1] = 255; id.data[kX+2] = 255;"
                    "}"
                "}"
                "rx.putImageData(id, 0, 0);"
                "x.drawImage(r, 0, 0, 160, 120);"
            "}"

            "function sc(mi, ma) {"
                "var rg = ma - mi;"
                "for (var i = 0; i < 640; i++) {"
                    "var v = Math.round((i / 639) * 255), c = gc(v);"
                    "for (var y = 0; y < 30; y++) {"
                        "var k = (y * 640 + i) * 4;"
                        "bd.data[k] = c[0]; bd.data[k+1] = c[1]; bd.data[k+2] = c[2]; bd.data[k+3] = 255;"
                    "}"
                "}"
                "bx.putImageData(bd, 0, 0);"
                "for (var n = 0; n < 10; n++) sp[n].innerHTML = (mi + rg * n / 9).toFixed(1) + 'C';"
            "}"

            "function doTest() {"
                "var p = new Uint8Array(768);"
                "for (var j = 0; j < 768; j++) p[j] = (j * 0.33) & 0xFF;"
                "dr(p); sc(20.0, 45.0);"
                "inf.innerHTML = 'Min:20.0C | Max:45.0C | Amb:22.5C';"
                "st.innerHTML = 'Test OK';"
            "}"

            "function fetchData() {"
                "st.innerHTML = 'Load...';"
                "var q = new XMLHttpRequest();"
                "q.open('GET', '/data', true);"
                "q.responseType = 'arraybuffer';"
                "q.onload = function() {"
                    "if (q.status !== 200) { st.innerHTML = 'Err ' + q.status; return; }"
                    "if (q.response.byteLength < 774) { st.innerHTML = 'Len ' + q.response.byteLength; return; }"
                    "var buf = new Uint8Array(q.response);"
                    "dr(new Uint8Array(q.response, 0, 768));"
                    "function r16(o) { var v = (buf[o] << 8) | buf[o+1]; return v >= 32768 ? v - 65536 : v; }"
                    "var mi = r16(768) / 10, ma = r16(770) / 10, ab = r16(772) / 10;"
                    "sc(mi, ma);"
                    "inf.innerHTML = 'Min:' + mi.toFixed(1) + 'C | Max:' + ma.toFixed(1) + 'C | Amb:' + ab.toFixed(1) + 'C';"
                    "st.innerHTML = 'OK';"
                "};"
                "q.onerror = function() { st.innerHTML = 'Net Err'; };"
                "q.send();"
            "}"

            "setInterval(fetchData, 62);"
            "doTest();"
        "</script>"
    "</body>"
"</html>";

// static const char index_html[] =
//     "<html><head><meta charset='UTF-8'><title>Thermal</title>"
//     "<style>body{background:#000;color:#fff;font-family:monospace;text-align:center;padding:10px;margin:0}"
//     "canvas{border:2px solid #444;background:#111}"
//     "#c{width:640px;height:480px;display:block;margin:10px auto}"
//     "#bar{width:640px;height:30px;display:block;margin:0 auto}"
//     "button,select{padding:10px 15px;margin:5px;font-size:14px;background:#333;color:#fff;border:1px solid #666;border-radius:4px;cursor:pointer}"
//     "select{background:#222;}"
//     "#st{color:#aaa;margin:5px}#inf{color:#4af;font-weight:bold;margin:5px 0}"
//     "#crd{color:#ffa;font-size:13px;margin:5px 0;font-weight:bold}"
//     "#lbl{display:flex;justify-content:space-between;width:640px;margin:0 auto;font-size:11px;color:#ccc}</style></head><body>"
//     "<h3>ESP32 Thermal Camera</h3>"
//     "<canvas id='c' width='160' height='120'></canvas>"
//     "<div id='st'>Ready</div>"
//     "<div id='inf'>Min:--.-C | Max:--.-C | Amb:--.-C</div>"
//     "<div id='crd'>Min: (0,0) | Max: (0,0)</div>"
//     // ✅ Падащо меню за честота
//     "<select id='rateSel'><option value='2'>2 Hz</option><option value='4' selected>4 Hz</option><option value='8'>8 Hz</option><option value='16'>16 Hz</option></select> "
//     "<button onclick='setRate()'>⚡ Set Rate</button>"
//     "<button onclick='doTest()'>Test</button> <button onclick='fetchData()'>Refresh</button> <button onclick='togglePal()'>🎨 Palette</button> <button onclick='toggleMarkers()'>📍 Markers</button>"
//     "<canvas id='bar' width='640' height='30'></canvas>"
//     "<div id='lbl'></div>"
//     "<script>"
//     "var c=document.getElementById('c'),x=c.getContext('2d');x.imageSmoothingEnabled=true;"
//     "var r=document.createElement('canvas');r.width=32;r.height=24;"
//     "var rx=r.getContext('2d'),id=rx.createImageData(32,24);"
//     "var b=document.getElementById('bar'),bx=b.getContext('2d'),bd=bx.createImageData(640,30);"
//     "var st=document.getElementById('st'),inf=document.getElementById('inf'),crd=document.getElementById('crd'),lb=document.getElementById('lbl');"
//     "for(var i=0;i<10;i++){var s=document.createElement('span');lb.appendChild(s);}"
//     "var sp=lb.children;"
//     "var palMode=0,showMarkers=1;"
//     "function togglePal(){palMode=1-palMode;st.innerHTML='Palette: '+(palMode?'Thermal':'Grayscale');fetchData();}"
//     "function toggleMarkers(){showMarkers=1-showMarkers;st.innerHTML='Markers: '+(showMarkers?'ON':'OFF');fetchData();}"
//     "function setRate(){var r=document.getElementById('rateSel').value;st.innerHTML='Set '+r+'Hz...';var q=new XMLHttpRequest();q.open('GET','/set_rate?rate='+r,true);q.onload=function(){if(q.status===200)st.innerHTML='Rate '+r+'Hz OK';else st.innerHTML='Fail: '+q.status};q.onerror=function(){st.innerHTML='Net Err'};q.send();}"
//     "function gc(v){if(palMode===0){return[v,v,v];}if(v<64){return[0,Math.round(v*4),255];}else if(v<128){return[0,255,Math.round(255-(v-64)*4)];}else if(v<192){return[Math.round((v-128)*4),255,0];}else{return[255,Math.round(255-(v-192)*4),0];}}"
//     "function dr(p){"
//     "  var minIdx=0,maxIdx=0,minV=256,maxV=-1;"
//     "  for(var j=0;j<768;j++){"
//     "    var v=p[j],k=j*4,c=gc(v);"
//     "    id.data[k]=c[0];id.data[k+1]=c[1];id.data[k+2]=c[2];id.data[k+3]=255;"
//     "    if(v<minV){minV=v;minIdx=j;}if(v>maxV){maxV=v;maxIdx=j;}"
//     "  }"
//     "  var x1=minIdx%32, y1=Math.floor(minIdx/32);"
//     "  var x2=maxIdx%32, y2=Math.floor(maxIdx/32);"
//     "  crd.innerHTML='Min: ('+x1+','+y1+') | Max: ('+x2+','+y2+')';"
//     "  if(showMarkers){"
//     "    var kM=minIdx*4,kX=maxIdx*4;"
//     "    if(palMode===0){"
//     "      id.data[kM]=0;id.data[kM+1]=0;id.data[kM+2]=255;"
//     "      id.data[kX]=255;id.data[kX+1]=0;id.data[kX+2]=0;"
//     "    }else{"
//     "      id.data[kM]=0;id.data[kM+1]=0;id.data[kM+2]=0;"
//     "      id.data[kX]=255;id.data[kX+1]=255;id.data[kX+2]=255;"
//     "    }"
//     "  }"
//     "  rx.putImageData(id,0,0);"
//     "  x.drawImage(r,0,0,160,120);"
//     "}"
//     "function sc(mi,ma){var rg=ma-mi;for(var i=0;i<640;i++){var v=Math.round((i/639)*255),c=gc(v);for(var y=0;y<30;y++){var k=(y*640+i)*4;bd.data[k]=c[0];bd.data[k+1]=c[1];bd.data[k+2]=c[2];bd.data[k+3]=255;}}bx.putImageData(bd,0,0);for(var n=0;n<10;n++)sp[n].innerHTML=(mi+rg*n/9).toFixed(1)+'C';}"
//     "function doTest(){var p=new Uint8Array(768);for(var j=0;j<768;j++)p[j]=(j*0.33)&0xFF;dr(p);sc(20.0,45.0);inf.innerHTML='Min:20.0C | Max:45.0C | Amb:22.5C';st.innerHTML='Test OK';}"
//     "function fetchData(){st.innerHTML='Load...';var q=new XMLHttpRequest();q.open('GET','/data',true);q.responseType='arraybuffer';q.onload=function(){if(q.status!==200){st.innerHTML='Err '+q.status;return}if(q.response.byteLength<774){st.innerHTML='Len '+q.response.byteLength;return}var buf=new Uint8Array(q.response);dr(new Uint8Array(q.response,0,768));function r16(o){var v=(buf[o]<<8)|buf[o+1];return v>=32768?v-65536:v;}var mi=r16(768)/10,ma=r16(770)/10,ab=r16(772)/10;sc(mi,ma);inf.innerHTML='Min:'+mi.toFixed(1)+'C | Max:'+ma.toFixed(1)+'C | Amb:'+ab.toFixed(1)+'C';st.innerHTML='OK';};q.onerror=function(){st.innerHTML='Net Err';};q.send();}"
//     "setInterval(fetchData,62);doTest();"
//     "</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, sizeof(index_html) - 1);
}

static esp_err_t data_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/octet-stream");
    uint8_t local_buf[DATA_COUNT + 6];
    xSemaphoreTake(s_buf_mutex, portMAX_DELAY);
    memcpy(local_buf, s_pixel_buf, sizeof(local_buf));
    xSemaphoreGive(s_buf_mutex);
    return httpd_resp_send(req, (const char *)local_buf, sizeof(local_buf));
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t u1 = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
        httpd_uri_t u2 = {.uri = "/data", .method = HTTP_GET, .handler = data_handler, .user_ctx = NULL};
        httpd_uri_t u3 = {.uri = "/set_rate", .method = HTTP_GET, .handler = set_rate_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &u1);
        httpd_register_uri_handler(server, &u2);
        httpd_register_uri_handler(server, &u3);
    }
    return server;
}

static void init_wifi_ap(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t ap = {.ap = {.ssid = AP_SSID, .ssid_len = strlen(AP_SSID), .channel = 6, .password = AP_PASS, .max_connection = 4, .authmode = WIFI_AUTH_WPA2_PSK}};
    if (strlen(AP_PASS) == 0)
        ap.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP: %s pass:%s", AP_SSID, AP_PASS);
}

void app_main(void)
{
    s_buf_mutex = xSemaphoreCreateMutex();
    init_wifi_ap();
    start_webserver();
    init_i2c();
    xTaskCreatePinnedToCore(i2c_read_task, "i2c_read", 8192, NULL, 5, NULL, 1);
}
