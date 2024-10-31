/***********************************************************************
 * Project      :     tiny32_WiFiManager
 * Description  :     Management WiFi with File System
 * Hardware     :     tiny32_v4
 * Date         :     31/10/2024
 * website      :     http://www.tenergyinnovation.co.th
 * Email        :     uten.boonliam@tenergyinnovation.co.th
 * TEL          :     +66 89-140-7205
 ***********************************************************************/
#include <Arduino.h>
#include <tiny32_v3.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <stdint.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>

/**************************************/
/*          Firmware Version          */
/**************************************/
#define FIRMWARE_VERSION "1.0"

/**************************************/
/*          Header project            */
/**************************************/
void header_print(void)
{
    Serial.printf("\r\n***********************************************************************\r\n");
    Serial.printf("* Project      :     tiny32_WiFiManager\r\n");
    Serial.printf("* Description  :     Management WiFi with File System \r\n");
    Serial.printf("* Hardware     :     tiny32_v4\r\n");
    Serial.printf("* Author       :     Tenergy Innovation Co., Ltd.\r\n");
    Serial.printf("* Date         :     31/10/2024\r\n");
    Serial.printf("* Revision     :     %s\r\n", FIRMWARE_VERSION);
    Serial.printf("* website      :     http://www.tenergyinnovation.co.th\r\n");
    Serial.printf("* Email        :     uten.boonliam@tenergyinnovation.co.th\r\n");
    Serial.printf("* TEL          :     +66 89-140-7205\r\n");
    Serial.printf("***********************************************************************/\r\n");
}

/**************************************/
/*       MQTT COMMAND FROM USER       */
/**************************************/
#define CPU_RESET_CMD "cpu_reset"

/**************************************/
/*        define object variable      */
/**************************************/
tiny32_v3 mcu;
WiFiManager wm;
File file;

/**************************************/
/*            GPIO define             */
/**************************************/

/**************************************/
/*       Constand define value        */
/**************************************/
#define WDT_TIMEOUT 120
#define WIFI_TIMEOUT 20

/**************************************/
/*       eeprom address define        */
/**************************************/
#define EEPROM_SIZE 1024 // กำหนดขนาดของ eeprom *สำคัญมาก

/**************************************/
/*        define global variable      */
/**************************************/
String Json_string = "";
String Json_string_final = "";
char unit[20];
char macid[50];
char msg[50];
char string[40];
uint8_t wifistatus;
int WiFi_rssi;

/* upload flag */

StaticJsonDocument<50000> doc;
StaticJsonDocument<1000> doc1;
StaticJsonDocument<1000> doc2;

/**************************************/
/*           define function          */
/**************************************/
bool wifi_config(bool upload_flag);
void eeprom_init();

/**************************************/
/*          Special funtion           */
/**************************************/
#include <tiny32_SPIFSS.h>

/***********************************************************************
 * FUNCTION:    setup
 * DESCRIPTION: setup process
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void setup()
{
    Serial.begin(115200);
    header_print();
    mcu.buzzer_beep(1);
    Serial.println("Setup: Initial eeprom ...");
    eeprom_init();

    //*** Define Unit name ***
    String _mac1 = WiFi.macAddress().c_str();
    Serial.printf("Setup: macAddress = %s\r\n", WiFi.macAddress().c_str());
    String _mac2 = _mac1.substring(9, 17);
    _mac2.replace(":", "");
    String _mac3 = "tiny32-" + _mac2;
    _mac3.toCharArray(unit, _mac3.length() + 1);
    Serial.printf("Setup: Unit ID => %s\r\n", unit);

    // Reset SPIFSS and WiFi config
    if (mcu.Sw2())
    {
        mcu.buzzer_beep(4);
        while (mcu.Sw2())
            ;
        Serial.println("Warning: Reset SPIFSS and WiFi config...");
        if (!SPIFFS.begin(true))
        {
            Serial.println("Error: An Error has occurred while mounting SPIFFS");
        }
        else
        {
            Serial.printf("Info: List file in root directory\r\n");
            listDir(SPIFFS, "/", 0);
            Serial.println();
            writeFile(SPIFFS, "/wifi.conf", "");
            writeFile(SPIFFS, "/wifi_latest.conf", "");
            wm.resetSettings();
            vTaskDelay(2000);
            ESP.restart();
        }
    }

    strcpy(macid, WiFi.macAddress().c_str());
    //*** SPIFSS inital ***
    Serial.println("Setup: SPIFSS initial ...");
    if (!SPIFFS.begin(true))
    {
        Serial.println("\tError: An Error has occurred while mounting SPIFFS");
    }
    else
    {
        Serial.printf("\tPass: List file in root directory\r\n");
        listDir(SPIFFS, "/", 0);
    }

    if (true) // ไม่เชื่อมต่อ WiFi ในฟังก์ชั่น setup
    {
        Serial.println("Setup: WiFi initial ...");
        wifi_config(true);
    }

    //*** WiFi Workwi simulation ***
    // Serial.print("Connecting to WiFi");
    // mcu.TickBuildinLED(0.1);
    // WiFi.begin("Wokwi-GUEST", "", 6);

    uint8_t _error_cnt = 0;
    while ((WiFi.status() != WL_CONNECTED) && (_error_cnt++ < 5))
    {
        delay(1000);
        Serial.print(".");
    }
    if (_error_cnt < 4)
    {

        Serial.printf("\tSuccess to connected\r\n");
        Serial.print("\tIP address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("\tSSID: %s\r\n", WiFi.SSID().c_str());
        Serial.printf("\tRSSI: %d\r\n", WiFi.RSSI());
        mcu.TickBuildinLED(1);
    }
    else
    {
        wifi_config(true);
    }

    /* All wifi list */
    String _json_rawFile = getdataFile(SPIFFS, "/wifi.conf");
    String _json_string = "{\"wifi\": [";
    _json_string += _json_rawFile;
    _json_string += "]}";
    DeserializationError _error = deserializeJson(doc1, _json_string);

    if (_error)
    {
        Serial.printf("\tError: deserializeJson() failed: %s", _error.c_str());
    }

    else
    {
        /* แสดงค่า WiFi list ทั้งหมด */
        Serial.printf("\r\nSetup: All WiFi list =>\r\n");
        size_t _json_data_size = serializeJsonPretty(doc1, Serial);
        Serial.println("\r\n");
    }

    /* Watchdoc concig*/
    Serial.print("Setup: Configuring WDT...");
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);
    Serial.println("done");
    mcu.buzzer_beep(2);
    vTaskDelay(2);
}


/***********************************************************************
 * FUNCTION:    loop
 * DESCRIPTION: loop process
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void loop()
{

    if (mcu.Sw1())
    {
        mcu.buzzer_beep(1);
        Serial.printf("***** Show File Config *****\r\n");
        while (mcu.Sw1())
            ;
        if (!SPIFFS.begin(true))
        {
            Serial.println("\tError: An Error has occurred while mounting SPIFFS");
        }
        else
        {
            Serial.printf("\tPass: List file in root directory\r\n");
            listDir(SPIFFS, "/", 0);
        }

        file = SPIFFS.open("/wifi.conf");
        if (!file)
        {
            Serial.println("Error: Failed to open file 'wifi.config'");
        }
        else
        {
            /* All wifi list */
            String _json_rawFile = getdataFile(SPIFFS, "/wifi.conf");
            String _json_string = "{\"wifi\": [";
            _json_string += _json_rawFile;
            _json_string += "]}";
            DeserializationError _error = deserializeJson(doc1, _json_string);

            /* latest wifi */
            String _json_rawFile2 = getdataFile(SPIFFS, "/wifi_latest.conf");
            String _json_string2 = "{\"wifi\": [";
            _json_string2 += _json_rawFile2;
            _json_string2 += "]}";
            DeserializationError _error2 = deserializeJson(doc2, _json_string2);

            if (_error)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error.c_str());
            }
            else if (_error2)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error2.c_str());
            }
            else
            {
                bool _wifiConnected = false; // ตัวแปรตรวจสอบการเช่ือมต่อ WiFi

                /* แสดงค่า WiFi list ทั้งหมด */
                Serial.printf("Info: All WiFi list =>\r\n");
                size_t _json_data_size = serializeJsonPretty(doc1, Serial);
                Serial.println("\r\n");
                Serial.printf("Info: Data Size => %d bytes\r\n", _json_data_size);
                int _json_data_number = doc1["wifi"].size();
                Serial.printf("Info: Number of WiFi list => %d\r\n\r\n", _json_data_number);

                Serial.println("----------------");

                /* แสดงค่า WiFi ล่าสุดที่เชื่อมต่อ */
                Serial.printf("\r\nInfo: Latest WiFi =>\r\n");
                size_t _json_data_size2 = serializeJsonPretty(doc2, Serial);
                int _json_data_number2 = doc2["wifi"].size();
                Serial.println("\r\n");

                /* เชื่อมต่อกับ WiFi ตัวล่าสุด */
                String _ssid_string2 = doc2["wifi"][0]["ssid"];
                String _pass_string2 = doc2["wifi"][0]["pass"];
                char _ssid_char2[30];
                char _pass_char2[30];
                _ssid_string2.toCharArray(_ssid_char2, _ssid_string2.length() + 1);
                _pass_string2.toCharArray(_pass_char2, _pass_string2.length() + 1);
                Serial.printf("Info: WiFi(latest) Connecting to \r\n");
                Serial.printf("ssid[latest]: %s\r\n", _ssid_char2);
                Serial.printf("password[latest]: %s\r\n", _pass_char2);
                uint8_t _timeoutCount2 = 0;
                Serial.printf("");
            }
        }
    }
    // Reset SPIFSS and WiFi config
    if (mcu.Sw2())
    {
        mcu.buzzer_beep(4);
        while (mcu.Sw2())
            ;
        Serial.println("Warning: Reset SPIFSS and WiFi config...");
        if (!SPIFFS.begin(true))
        {
            Serial.println("Error: An Error has occurred while mounting SPIFFS");
        }
        else
        {
            Serial.printf("Info: List file in root directory\r\n");
            listDir(SPIFFS, "/", 0);
            Serial.println();
            writeFile(SPIFFS, "/wifi.conf", "");
            writeFile(SPIFFS, "/wifi_latest.conf", "");
            wm.resetSettings();
            vTaskDelay(2000);
            ESP.restart();
        }
    }

    esp_task_wdt_reset();
    vTaskDelay(10);
}

/***********************************************************************
 * FUNCTION:    wifi_config
 * DESCRIPTION: WiFi config function for check and upload data to server
 * PARAMETERS:  upload_flag (TRUE/FALSE)
 * RETURNED:    nothing
 ***********************************************************************/
bool wifi_config(bool upload_flag)
{
    if (upload_flag)
    {
        Serial.println("\tInfo: Wifi config ...");
        mcu.TickBuildinLED(0.1);

        file = SPIFFS.open("/wifi.conf");
        if (!file)
        {
            Serial.println("Error: Failed to open file 'wifi.config'");
            return false;
        }
        else
        {
            /* All wifi list */
            String _json_rawFile = getdataFile(SPIFFS, "/wifi.conf");
            String _json_string = "{\"wifi\": [";
            _json_string += _json_rawFile;
            _json_string += "]}";
            DeserializationError _error = deserializeJson(doc1, _json_string);

            /* latest wifi */
            String _json_rawFile2 = getdataFile(SPIFFS, "/wifi_latest.conf");
            String _json_string2 = "{\"wifi\": [";
            _json_string2 += _json_rawFile2;
            _json_string2 += "]}";
            DeserializationError _error2 = deserializeJson(doc2, _json_string2);

            if (_error)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error.c_str());
                return false;
            }
            else if (_error2)
            {
                Serial.printf("Error: deserializeJson() failed: %s", _error2.c_str());
                return false;
            }
            else
            {
                bool _wifiConnected = false; // ตัวแปรตรวจสอบการเช่ือมต่อ WiFi

                /* แสดงค่า WiFi list ทั้งหมด */
                Serial.printf("Info: All WiFi list =>\r\n");
                size_t _json_data_size = serializeJsonPretty(doc1, Serial);
                Serial.println("\r\n");
                Serial.printf("Info: Data Size => %d bytes\r\n", _json_data_size);
                int _json_data_number = doc1["wifi"].size();
                Serial.printf("Info: Number of WiFi list => %d\r\n\r\n", _json_data_number);

                Serial.println("----------------");

                /* แสดงค่า WiFi ล่าสุดที่เชื่อมต่อ */
                Serial.printf("\r\nInfo: Latest WiFi =>\r\n");
                size_t _json_data_size2 = serializeJsonPretty(doc2, Serial);
                int _json_data_number2 = doc2["wifi"].size();
                Serial.println("\r\n");

                /* เชื่อมต่อกับ WiFi ตัวล่าสุด */
                String _ssid_string2 = doc2["wifi"][0]["ssid"];
                String _pass_string2 = doc2["wifi"][0]["pass"];
                char _ssid_char2[30];
                char _pass_char2[30];
                _ssid_string2.toCharArray(_ssid_char2, _ssid_string2.length() + 1);
                _pass_string2.toCharArray(_pass_char2, _pass_string2.length() + 1);
                Serial.printf("Info: WiFi(latest) Connecting to \r\n");
                Serial.printf("ssid[latest]: %s\r\n", _ssid_char2);
                Serial.printf("password[latest]: %s\r\n", _pass_char2);
                uint8_t _timeoutCount2 = 0;
                Serial.printf("");

                // check emtry ssid and password from config file
                bool _hasNull = false;
                for (int i = 0;; ++i)
                {
                    if (_ssid_char2[i] == '\0')
                    {
                        _hasNull = true;
                        break;
                    }
                }

                if (_hasNull)
                {
                    WiFi.begin(_ssid_char2, _pass_char2); // ทำการเชื่อมต่อ WiFi ตัวล่าสุด
                    while ((WiFi.status() != WL_CONNECTED) && (_timeoutCount2 < WIFI_TIMEOUT))
                    {
                        delay(1000);
                        Serial.print(".");
                        _timeoutCount2++;
                    }
                }

                if ((WiFi.status() == WL_CONNECTED)) // เชื่อมต่อ WiFi ตัวล่าสุดสำเร็จ
                {
                    Serial.println("\n\rWiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
                    Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                    Serial.printf("RSSI: %d\r\n", WiFi.RSSI());
                    _wifiConnected = true;
                    mcu.TickBuildinLED(1.0);
                    return true;
                }
                else // พยายามเชื่อมต่อ WiFi ที่อยู่ใน list ของไฟล์ 'wifi.conf'
                {
                    Serial.println();
                    Serial.printf("Info: WiFi Connecting to \r\n");
                    for (int i = 0; i < _json_data_number; i++)
                    {
                        String _ssid_string = doc1["wifi"][i]["ssid"];
                        String _pass_string = doc1["wifi"][i]["pass"];
                        char _ssid_char[30];
                        char _pass_char[30];
                        _ssid_string.toCharArray(_ssid_char, _ssid_string.length() + 1);
                        _pass_string.toCharArray(_pass_char, _pass_string.length() + 1);
                        Serial.printf("ssid[%d]: %s\r\n", i, _ssid_char);
                        Serial.printf("password[%d]: %s\r\n", i, _pass_char);
                        WiFi.begin(_ssid_char, _pass_char);
                        uint8_t _timeoutCount = 0;
                        Serial.printf("");
                        while ((WiFi.status() != WL_CONNECTED) && (_timeoutCount < WIFI_TIMEOUT))
                        {
                            delay(1000);
                            Serial.print(".");
                            _timeoutCount++;
                        }

                        if (WiFi.status() == WL_CONNECTED)
                        {
                            Serial.println("\n\rWiFi connected");
                            Serial.print("IP address: ");
                            Serial.println(WiFi.localIP());
                            Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                            Serial.printf("RSSI: %d\r\n", WiFi.RSSI());
                            _wifiConnected = true;

                            // record latest wifi to 'wifi_latest.conf'
                            String _json_OverwriteFile = "";
                            _json_OverwriteFile += "{";
                            _json_OverwriteFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_OverwriteFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_OverwriteFile += "}";
                            writeFile(SPIFFS, "/wifi_latest.conf", _json_OverwriteFile.c_str());

                            _wifiConnected = true;
                            mcu.TickBuildinLED(1.0);
                            // break;
                            return true;
                        }
                    }

                    /* ถ้าไม่มีการเชื่อมต่อ WiFi จะเข้าสู่ AP Mode สำหรับเชื่อมต่อ และบันทึกค่า WiFi */
                    if (!_wifiConnected)
                    {

                        wm.setConfigPortalTimeout(300); // sets timeout before AP,webserver loop ends and exits even if there has been no setup.
                        if (wm.autoConnect(unit, "password"))
                        {
                            Serial.printf("Success to connected\r\n");
                            Serial.print("IP address: ");
                            Serial.println(WiFi.localIP());
                            Serial.printf("SSID: %s\r\n", WiFi.SSID().c_str());
                            Serial.printf("RSSI: %d\r\n", WiFi.RSSI());

                            /* record ssid and password to 'wifi.conf' */
                            String _json_appendFile = "";
                            if (_json_data_number == 0)
                            {
                                _json_appendFile += "{";
                            }
                            else
                            {
                                _json_appendFile += ",{";
                            }
                            _json_appendFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_appendFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_appendFile += "}";
                            appendFile(SPIFFS, "/wifi.conf", _json_appendFile.c_str());

                            /* record latest wifi to 'wifi_latest.conf' */
                            String _json_OverwriteFile = "";
                            _json_OverwriteFile += "{";
                            _json_OverwriteFile += "\"ssid\":\"" + wm.getWiFiSSID() + "\",";
                            _json_OverwriteFile += "\"pass\":\"" + wm.getWiFiPass() + "\"";
                            _json_OverwriteFile += "}";
                            writeFile(SPIFFS, "/wifi_latest.conf", _json_OverwriteFile.c_str());

                            mcu.TickBuildinLED(1.0);
                            return true;
                        }
                        else
                        {
                            Serial.println("Error: Failed to connect wifi");
                            Serial.println("System is resetting");
                            mcu.buzzer_beep(3);
                            // ESP.restart();
                            return false;
                        }
                    }
                }
            }
        }
    }
    else
    {
        Serial.printf("\tInfo: No wifi config");
        return false;
    }
    return true;
}

/***********************************************************************
 * FUNCTION:    eeprom_init
 * DESCRIPTION: Read EEPROM then record to variable
 * PARAMETERS:  nothing
 * RETURNED:    nothing
 ***********************************************************************/
void eeprom_init()
{

    if (!EEPROM.begin(EEPROM_SIZE)) // fail eeprom
    {
        Serial.println("\tError: failed to initialise EEPROM");
        Serial.println();
    }
    else
    {
        Serial.println("\tPass: Initial eeprom complete");
    }
}
