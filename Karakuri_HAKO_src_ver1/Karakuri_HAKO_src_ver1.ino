// -----------------------------------------------------------------------
// Karakuri -HAKO- 制御用プログラム
//
// 機能(function)
//  - 電源投入時にWifi経由でNTPサーバへ時刻同期を行う :Synchronize the time to the NTP server via Wifi when the power is turned on.
//  - 温度センサーでケース内の温度を測定             :Measure the temperature inside the case with a temperature sensor.
//  - 測定温度によりファンの回転数を制御             :Fan speed is controlled by the measured temperature.
//  - 測定温度により時刻表示の文字色を変化           :The text color of the time display changes depending on the measurement temperature.
//  - 搭載FAN（ARGB対応FAN）のライティング          :Equipped FAN (ARGB compatible FAN) lighting [Single color (red, purple, blue, green), illumination, rolling]
//    [単色（赤、紫、青、緑）、グラデーション、アニメーション]
//
// LED点灯モードの選択方法（Webブラウザ経由）        :How to select LED lighting mode (via web browser).
//  - http[80]でM5StickCに接続                   :Connect to M5 Stick C with http [80].
//  - 画面上のボタンから点灯モードをクリックする      :Click the lighting mode from the button on the screen.
//
// FAN回転速度の手動指定方法（Webブラウザ経由）      :How to manually specify the FAN rotation speed (via a web browser).
//  - http[80]でM5StickCに接続                   :Connect to M5 Stick C with http [80].
//  - 画面上の「FAN手動設定」ボタンをクリックする     :Click the "FAN Manual Setting" button on the screen.
//  - 画面上の回転数指定スライダでPWMを指定          :Specify PWM with the rotation speed specification slider on the screen.
//  - 画面上の「FAN回転設定」ボタンをクリックする     :Click the "FAN Rotation Settings" button on the screen.
//
// 2020.11.6
// Author   : げんろく@karakuri-musha
// Device   : M5StickC
//            COOLER MASTER MASTERFAN MF120 HALO
//            DHT11
// License:
//    See the license file for the license.
//
// -----------------------------------------------------------------------
// ------------------------------------------------------------
// ライブラリインクルード部 Library include section.
// ------------------------------------------------------------
#include <M5StickC.h>               // M5StickC 用ライブラリ
#include <WiFi.h>                   // Wifi制御用ライブラリ
#include <time.h>                   // 時刻制御用ライブラリ
#include <DHT.h>                    // DHTxx（温度センサー）制御用ライブラリ
#include <FastLED.h>                // LED 制御用ライブラリ
#include <Preferences.h>            // 不揮発静メモリ制御ライブラリ
FASTLED_USING_NAMESPACE

// ------------------------------------------------------------
// 定数/変数　定義部　Constant / variable definition section.
// ------------------------------------------------------------
// GPIOの定義　GPIO definition.
// 5V出力用GPIOを使用　Use GPIO for 5V output.
const uint8_t TEMP_MEAN = 32;                     // 温度測定結果の取得 Acquisition of temperature measurement results.
const uint8_t FAN_PWM   = 26;                     // ファン回転数制御用PWM制御  PWM control for fan speed control.
const uint8_t ARGB_LED  = 0;                      // ARGB対応FAN制御  ARGB compatible FAN system.

// PWM制御用の定数　Constants for PWM control.
const uint8_t CH0       = 0;                      // PWM送信チャネル PWM transmission channel.
const double  PWM_FREQ  = 25000;                     // PWM周波数 PWM frequency.
const uint8_t PWM_BIT   = 8;                      // PWM分解能 PWM resolution.
int           Auto_set  = 0;                      // 温度によるPWM制御のON/OFF（ON:0,OFF:1) ON / OFF of PWM control by temperature (ON: 0, OFF: 1).
int           Current_Duty_val = 30;              // Webブラウザ表示時点のDuty比を格納
int           Duty_val  = 30;                     // 手動設定によるPWM制御時の指定Duty比　Specified duty ratio during PWM control by manual setting.

int           Temp_val1 = 50;                     // 自動制御によるPWM制御時の指定温度1　摂氏（℃）(Temp_val1 <= Temp_val2 <= more) 　
int           Temp_val2 = 70;                     // 自動制御によるPWM制御時の指定温度2　摂氏（℃）(Temp_val1 <= Temp_val2 <= more) 　
int           Duty_val1 = 30;                     // 自動制御によるPWM制御時の指定Duty比1(Duty_val1 <= Duty_val2 <= Duty_val3)
int           Duty_val2 = 60;                     // 自動制御によるPWM制御時の指定Duty比1(Duty_val1 <= Duty_val2 <= Duty_val3
int           Duty_val3 = 100;                    // 自動制御によるPWM制御時の指定Duty比1(Duty_val1 <= Duty_val2 <= Duty_val3)


// WIFI接続用情報　WIFI connection information.
Preferences preferences;
char ssid[33];                                    // アクセスポイント情報（SSID）の格納用 
char password[65];                                // アクセスポイント情報（パスワード）の格納用

// NTP接続情報　NTP connection information.
const char* NTPSRV      = "ntp.jst.mfeed.ad.jp";  // NTPサーバーアドレス NTP server address.
const long  GMT_OFFSET  = 9 * 3600;               // GMT-TOKYO(時差９時間）9 hours time difference.
const int   DAYLIGHT_OFFSET = 0;                  // サマータイム設定なし No daylight saving time setting

// ケース内温度情報　Case temperature information
float case_temp = 0;
int dht11_dat[5] = {0, 0, 0, 0, 0};
DHT dht( TEMP_MEAN, DHT11 );
String Temp_text = "";

// 時刻・日付の生成　Time / date generation.
RTC_TimeTypeDef RTC_TimeStruct;                   // RTC時刻　Times of Day.
RTC_DateTypeDef RTC_DateStruct;                   // RTC日付  Date
int smin = 0;                                     // 画面書き換え判定用（分）

// ARGB対応FAN制御用　For ARGB compatible FAN control.
const uint8_t LED_COUNT = 24;                     // 制御対象のLED数　Number of LEDs to be controlled.
const uint8_t DELAYVAL = 100;
const uint8_t FPS = 30;                           // フレームレートの指定
uint8_t gHue = 24;
uint8_t Led_select = 0;                           // LED点灯モード選択状態格納用
uint8_t Led_selected = 0;                         // LED点灯モード選択結果格納用
// LED表示方法の定義
char* Led_pattern[] = {"OFF",                     // OFF：消灯
                       "Single:Count",                   // Count     ：一つずつ順に点灯させていく
                       "Single:RED",              // Single    ：Red
                       "Single:PURPLE",           // Single    ：Purple
                       "Single:BLUE",             // Single    ：Blue
                       "Single:GREEN",            // Single    ：Green
                       "RAINBOW",                 // Graduation：グラデーション
                       "Animation"                // Animation ：アニメーション（グラデーションイメージを回転させる）
                      };
CRGB leds[LED_COUNT];                             // インスタンスの生成

// Webサーバ制御用 For web server control.
WiFiServer server(80);                            // Webサーバポートの設定
const long TIMEOUT_TIME = 2000;                   // タイムアウト時間の定義（2s=2000ms)
String Header;                                    // HTTPリクエストの格納用
unsigned long CurrentTime = millis();             // 現在時刻格納用
unsigned long PreviousTime = 0;                   // 前の時刻格納用

// html生成用
int Tab_control = 0;                              // 表示タブ状態の格納用


// ------------------------------------------------------------
// 関数定義部 Function definition section.
// ------------------------------------------------------------
// 時計画面の表示用関数　Clock screen display function.
// Clock_screen_display()
// ------------------------------------------------------------
void Clock_screen_display() {
  static const char *_wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"}; // 曜日の定義　Definition of the day of the week.

  // 時刻・日付の取り出し　Extraction of time and date.
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);

  // 画面書き換え処理　Screen rewriting process.
  if (smin == RTC_TimeStruct.Minutes) {
    M5.Lcd.fillRect(140, 10, 20, 10, BLACK); // 秒の表示エリアだけ書き換え Rewrite only the display area of seconds.
  } else {
    M5.Lcd.fillScreen(BLACK);               // 「分」が変わったら画面全体を書き換え Rewrite the entire screen when the "minute" changes.
  }

  // 現在温度確認
  // Green  :0 < cate_temp <= Temp_val1
  // YELLOW :Temp_val1 < case_temp <= Temp_val2
  // RED    :over Temp_val2
  if ( 0 < case_temp && case_temp <= Temp_val1 ) {
    M5.Lcd.setTextColor(GREEN);
  }
  else if ( case_temp <= Temp_val2 ) {
    M5.Lcd.setTextColor(YELLOW);
  }
  else {
    M5.Lcd.setTextColor(RED);
  }

  // 数字・文字表示部分
  // 時刻表示
  M5.Lcd.setCursor(0, 10, 7);               //x,y,font 7:48ピクセル7セグ風フォント
  M5.Lcd.printf("%02d:%02d", RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes); // 時分を表示

  // 秒表示
  M5.Lcd.setTextFont(1); // 1:Adafruit 8ピクセルASCIIフォント
  M5.Lcd.fillRect(150, 10, 10, 10, BLACK);
  M5.Lcd.printf(":%02d\n", RTC_TimeStruct.Seconds); // 秒を表示

  // 日付表示
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(25, 65, 1);              //x,y,font 1:Adafruit 8ピクセルASCIIフォント
  M5.Lcd.printf("Date:%04d.%02d.%02d %s\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, _wd[RTC_DateStruct.WeekDay]);

  // 温度・湿度表示
  M5.Lcd.println(Temp_text);

  smin = RTC_TimeStruct.Minutes; // 「分」を保存
}
// ------------------------------------------------------------
// PWM出力用の関数 Function for PWM output.
// Send_PWM()
// ------------------------------------------------------------
void Send_PWM() {

  // FANへのPWM送信制御（Auto:ON or OFF）で切り替え
  if (Auto_set == 0) {
    // ---------------------------------------------------
    // 温度によるPWM制御：ON
    // 温度測定結果によりファンのコントロールDuty比を変更する
    // 閾値（温度、Duty比）はFAN制御タブで設定された値
    // 温度は摂氏（℃）、Duty比（％）を単位とする
    // ---------------------------------------------------
    // 測定結果が 0 <= Temp_val1の時：PWM [Duty_val1]%
    if ( 0 < case_temp && case_temp <= Temp_val1 ) {
      ledcWrite(CH0, Duty_val1);
      Current_Duty_val = Duty_val1;
    }
    // 測定結果が Temp_val1 < case_temp <= Temp_val2の時：PWM [Temp_val2]%
    else if ( case_temp <= Temp_val2 ) {
      ledcWrite(CH0, Duty_val2);
      Current_Duty_val = Duty_val2;
    }
    // 測定結果が Temp_val2 < case_tempの時：PWM [Duty_val3]%
    else {
      ledcWrite(CH0, Duty_val3);
      Current_Duty_val = Duty_val3;
    }
  }
  else { // 温度によるPWM制御：OFF
    // ---------------------------------------------------
    // 温度によるPWM制御：OFF
    // スライダーで指定されたDuty比に変更する
    // ---------------------------------------------------
    ledcWrite(CH0, Duty_val);
    Current_Duty_val = Duty_val;
  }
}
// ------------------------------------------------------------
// 温度測定の関数　Temperature measurement function.
// Check_TEMP()
// ------------------------------------------------------------
void Check_TEMP() {
  delay(3000);

  bool isFahrenheit = true;
  float percentHumidity = dht.readHumidity();
  float temperature = dht.readTemperature( isFahrenheit );

  if (isnan(percentHumidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  float heatIndex = dht.computeHeatIndex(
                      temperature,
                      percentHumidity,
                      isFahrenheit);

  Temp_text = "Temp: ";
  Temp_text += String((temperature - 32) / 1.8, 1);
  Temp_text += "[C] Humi: ";
  Temp_text += String(percentHumidity, 1);
  Temp_text += "[%] HI: ";
  Temp_text += String(heatIndex, 1);

  case_temp = (temperature - 32) / 1.8;
  Serial.println(Temp_text);
}

// ------------------------------------------------------------
// Webリクエスト応答関数
// Run_WEB()
// ------------------------------------------------------------
void Run_WEB()
{
  WiFiClient client = server.available();                             // Listen for incoming clients

  if (client) {                                                       // 新しいクライアント接続がある場合に以下を実行
    CurrentTime = millis();                                           // 現在時刻の取得
    PreviousTime = CurrentTime;                                       // 前回時刻への現在時刻の格納

    // for debug
    Serial.println("New Client.");                                    // デバッグ用シリアルに表示

    String _CurrentLine = "";                                         // クライアントからの受信データを保持する文字列の生成
    while (client.connected() && CurrentTime - PreviousTime <= TIMEOUT_TIME) {  // クライアントが接続している間繰り返し実行する
      CurrentTime = millis();                                         // 現在時刻の更新
      if (client.available()) {                                       // クライアントから読み取る情報（バイト）がある場合
        char _c = client.read();                                      // 値（バイト）を読み取り

        // for debug
        Serial.write(_c);                                             // デバッグ用シリアルに表示

        Header += _c;                                                 // リクエストに値（バイト）を格納
        if (_c == '\n') {                                             // 値が改行文字の場合に以下を実行
          //現在の行が空白の場合、2つの改行文字が連続して表示
          //クライアントのHTTPリクエストは終了であるため、応答を送信
          if (_CurrentLine.length() == 0) {
            // HTTPヘッダーは常に応答コードで始まる（例：HTTP/1.1 200 OK)
            // Content-typeでクライアントが何が来るのか知ることができその後空白行になる
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // 押されたボタンにより処理を変更
            if (Header.indexOf("GET /LED/OFF") >= 0) {                // LED点灯モード設定で[OFF]が押された場合
              Led_selected = 0;
              Tab_control = 0;

           } else if (Header.indexOf("GET /LED/COUNT") >= 0) {        // LED点灯モード設定で[COUNT]が押された場合
              Led_selected = 1;
              Tab_control = 0;
              
            } else if (Header.indexOf("GET /LED/RED") >= 0) {         // LED点灯モード設定で[RED]が押された場合
              Led_selected = 2;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/PURPLE") >= 0) {      // LED点灯モード設定で[PURPLE]が押された場合
              Led_selected = 3;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/BLUE") >= 0) {        // LED点灯モード設定で[BLUE]が押された場合
              Led_selected = 4;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/GREEN") >= 0) {       // LED点灯モード設定で[GREEN]が押された場合
              Led_selected = 5;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/RAINBOW") >= 0) {     // LED点灯モード設定で[RAINBOW]が押された場合
              Led_selected = 6;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/ROLLING") >= 0) {     // LED点灯モード設定で[ROLLING]が押された場合
              Led_selected = 7;
              Tab_control = 0;

            } else if (Header.indexOf("GET /LED/") >= 0) {
              Led_selected = 8;
              Tab_control = 0;

            } else if (Header.indexOf("GET /FAN_on") >= 0) {         // FAN制御設定で自動制御[ON]が押された場合
              Auto_set = 0;
              Tab_control = 1;

              Send_PWM();

            } else if (Header.indexOf("GET /FAN_off") >= 0) {        // FAN制御設定で自動制御[OFF]が押された場合
              Auto_set = 1;
              Tab_control = 1;
              Duty_val = 30;

            } else if (Header.indexOf("GET /FAN_Set") >= 0) {        // FAN制御設定で自動制御[ON]状態で閾値のSUBMITボタンが押された場合
              int pos1 = 0;
              int pos2 = 0;

              // Header String から各閾値を取得し、変数に格納
              pos1 = Header.indexOf("temp1=", pos2);
              pos2 = Header.indexOf("&", pos1);
              Temp_val1 = Header.substring(pos1 + 6, pos2).toInt();
              Serial.println(Header.substring(pos1 + 6, pos2));

              pos1 = Header.indexOf("duty1=", pos2);
              pos2 = Header.indexOf("&", pos1);
              Duty_val1 = Header.substring(pos1 + 6, pos2).toInt();
              Serial.println(Header.substring(pos1 + 6, pos2));

              pos1 = Header.indexOf("temp2=", pos2);
              pos2 = Header.indexOf("&", pos1);
              Temp_val2 = Header.substring(pos1 + 6, pos2).toInt();
              Serial.println(Header.substring(pos1 + 6, pos2));

              pos1 = Header.indexOf("duty2=", pos2);
              pos2 = Header.indexOf("&", pos1);
              Duty_val2 = Header.substring(pos1 + 6, pos2).toInt();
              Serial.println(Header.substring(pos1 + 6, pos2));

              pos1 = Header.indexOf("duty3=", pos2);
              pos2 = Header.indexOf(" ", pos1);
              Duty_val3 = Header.substring(pos1 + 6, pos2).toInt();
              Serial.println(Header.substring(pos1 + 6, pos2));

              Auto_set = 0;
              Tab_control = 1;
              
              Send_PWM();

            } else if (Header.indexOf("GET /FAN_DutySet") >= 0) {    // FAN制御設定で自動制御[OFF]状態でDuty比のSUBMITボタンが押された場合
              int pos1 = 0;
              int pos2 = 0;

              // Header String からDuty比を取得し、変数に格納
              pos1 = Header.indexOf("duty=", pos2);
              pos2 = Header.indexOf(" ", pos1);
              Duty_val = Header.substring(pos1 + 5, pos2).toInt();
              Serial.println(Header.substring(pos1 + 5, pos2));

              Auto_set = 1;
              Tab_control = 1;

              Send_PWM();
              
            }

            // HTML Webページの表示
            client.println("<!DOCTYPE html><html>");
            client.println("<html lang=\"ja\">");
            client.println("<head><meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");

            client.println("<title>KARAKURI -HAKO- LED点灯モード選択画面</title>");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            // CSSのスタイルの指定
            client.println("<style>");
            client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".btn_1 {background-color: #e64e20; border: none; color: white; padding: 16px 40px;text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;border-radius: 100vh;width: 200px;}");
            client.println(".btn_2 {background-color: #555555;}");
            client.println(".btn_3 {background-color: #e64e20; border: none; color: white; padding: 16px 20px;text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;border-radius: 100vh;width: 100px;}");
            client.println(".btn_4 {background-color: #555555;}");
            client.println(".btn_5 {background-color: #e64e20; border: none; color: white; padding: 16px 40px;text-decoration: none; font-size: 20px; margin: 20px; cursor: pointer;border-radius: 100vh;width: 200px;}");
            client.println(".txt_1 {border: none; padding: 5px 25px;text-decoration: none; font-size: 20px; margin: 5px; cursor: pointer; width: 50px; text-align: center;}");
            client.println(".p001 {color: Brack;;font-size: 20px;}");
            client.println(".p002 {color: Brack;;font-size: 20px; padding: 15px 15px; margin: 2px;}");
            client.println(".box8 {padding: 0.5em 1em; margin: 2em 0; color: #232323; background: #fff8e8; border-left: solid 10px #ffc06e;}");
            client.println(".box8 p {margin: 0; padding: 0;}");
            client.println("table {border-collapse: collapse; border: solid 2px orange;}");
            client.println("table th, table td {border: dashed 1px orange;}");
            client.println("@keyframes tabAnim{0%{opacity:0;} 100%{opacity:1;}}");
            client.println(".tab_wrap{width:500px; margin:20px auto;}");
            client.println("input[type=\"radio\"]{display:none;}");
            client.println(".tab_area{font-size:0; margin:0 10px;}");
            client.println(".tab_area label{width:150px; margin:0 5px; display:inline-block; padding:12px 0; color:#999; background:#ddd; text-align:center; font-size:13px; cursor:pointer; transition:ease 0.2s opacity;}");
            client.println(".tab_area label:hover{opacity:0.5;}");
            client.println(".tab_panel{width:100%; opacity:0; padding:20px 0; display:none;}");
            client.println(".tab_panel p{font-size:14px; letter-spacing:1px; text-align:center;}");
            client.println(".panel_area{background:#fff;}");
            client.println("#tab1:checked ~ .tab_area .tab1_label{background:#fff; color:#000;}");
            client.println("#tab1:checked ~ .panel_area #panel1{display:block; animation:tabAnim ease 0.6s forwards; -ms-animation:tabAnim ease 0.6s forwards;}");
            client.println("#tab2:checked ~ .tab_area .tab2_label{background:#fff; color:#000;}");
            client.println("#tab2:checked ~ .panel_area #panel2{display:block; animation:tabAnim ease 0.6s forwards; -ms-animation:tabAnim ease 0.6s forwards;}");
            client.println("</style></head>");

            // Webページ本体(タイトルと説明)
            client.println("<body><h1>KARAKURI -HAKO-</h1>");

            // リクエストにより表示するタブを制御
            client.println("<div class=\"tab_wrap\">");
            if (Tab_control == 0) {
              client.println("<input id=\"tab1\" type=\"radio\" name=\"tab_btn\" checked>");
              client.println("<input id=\"tab2\" type=\"radio\" name=\"tab_btn\">");
            } else {
              client.println("<input id=\"tab1\" type=\"radio\" name=\"tab_btn\">");
              client.println("<input id=\"tab2\" type=\"radio\" name=\"tab_btn\" checked>");
            }
            client.println("<div class=\"tab_area\">");
            client.println("<label class=\"tab1_label\" for=\"tab1\">LED点灯モード</label>");
            client.println("<label class=\"tab2_label\" for=\"tab2\">FAN制御</label></div>");

            // ------------------------------------------------------
            // Tab1:LED点灯モードの生成
            // ------------------------------------------------------
            // LED点灯モードタブの描画
            client.println("<div class=\"panel_area\">");

            // LED点灯モードタブの描画(Start)
            client.println("<div id=\"panel1\" class=\"tab_panel\">");
            client.println("<h4>LED lighting mode selection</h4>");

            // ボタンの表示（選択状態により変更)
            // OFFボタンの表示
            if (Led_pattern[Led_selected] == "OFF") {
              client.println("<p ><a href=\"/LED/OFF\"><button class=\"btn_1\">OFF</button></a></p>");
            }
            else {
              client.println("<p ><a href=\"/LED/OFF\"><button class=\"btn_1  btn_2\">OFF</button></a></p>");
            }
            // 単色設定用ボタン表示部
            client.println("<p class=\"p001\">単色:Single</p>");
            // カウント
            if (Led_pattern[Led_selected] == "Single:Count") {
              client.println("<p><a href=\"/LED/COUNT\"><button class=\"btn_1\">COUNT</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/COUNT\"><button class=\"btn_1 btn_2\">COUNT</button></a></p>");
            }
            // 赤
            if (Led_pattern[Led_selected] == "Single:RED") {
              client.println("<p><a href=\"/LED/RED\"><button class=\"btn_1\">RED</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/RED\"><button class=\"btn_1 btn_2\">RED</button></a></p>");
            }
            // 紫
            if (Led_pattern[Led_selected] == "Single:PURPLE") {
              client.println("<p><a href=\"/LED/PURPLE\"><button class=\"btn_1\">PURPLE</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/PURPLE\"><button class=\"btn_1 btn_2\">PURPLE</button></a></p>");
            }
            // 青
            if (Led_pattern[Led_selected] == "Single:BLUE") {
              client.println("<p><a href=\"/LED/BLUE\"><button class=\"btn_1\">BLUE</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/BLUE\"><button class=\"btn_1 btn_2\">BLUE</button></a></p>");
            }
            // 緑
            if (Led_pattern[Led_selected] == "Single:GREEN") {
              client.println("<p><a href=\"/LED/GREEN\"><button class=\"btn_1\">GREEN</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/GREEN\"><button class=\"btn_1 btn_2\">GREEN</button></a></p>");
            }
            // アニメーション設定用ボタン表示部
            client.println("<p class=\"P001\">複数色:Multiple colors</p>");
            // Rainbow
            if (Led_pattern[Led_selected] == "RAINBOW") {
              client.println("<p><a href=\"/LED/RAINBOW\"><button class=\"btn_1\">RAINBOW</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/RAINBOW\"><button class=\"btn_1 btn_2\">RAINBOW</button></a></p>");
            }
            // Animation
            if (Led_pattern[Led_selected] == "Animation") {
              client.println("<p><a href=\"/LED/ROLLING\"><button class=\"btn_1\">Animation</button></a></p>");
            }
            else {
              client.println("<p><a href=\"/LED/ROLLING\"><button class=\"btn_1 btn_2\">Animation</button></a></p>");
            }

            // LED点灯モードタブの描画(End)
            client.println("</div>");

            // ------------------------------------------------------
            // Tab2:FAN制御タブの生成
            // ------------------------------------------------------
            // FAN制御タブの描画(Start)
            client.println("<div id=\"panel2\" class=\"tab_panel\">");
            client.println("<h3>温度センサーによるFAN制御</h3>");

            // FAN制御（Auto(0):温度による制御、Non(1):手動設定）
            if (Auto_set == 0) {
              // ----------------------------------------------------------
              // FAN制御（Auto:ON）の場合
              // 三段階の温度とDuty比の関係を設定できる項目を表示
              // ※SUBMITボタンで決定
              // ----------------------------------------------------------
              client.println("<p ><a href=\"/FAN_on\"><button class=\"btn_3\">ON</button></a><a href=\"/FAN_off\"><button class=\"btn_3 btn_4\">OFF</button></a></p>");
              String Duty_text = "<h3>現在のFAN回転比率（Duty比）：" + String(Current_Duty_val) + "%</h3>";
              client.println(Duty_text);

              // 温度による制御が有効である場合、閾値の設定を表示
              client.println("<form method=\"get\" name=\"Duty_Threshold\" action=\"FAN_Set\">");
              client.println("<div class=\"box8\">");
              client.println("<p>温度の閾値設定</p><p>（設定の反映はSUBMITボタンを押してください。）</p></br>");
              client.println("<table border=\"1\" align=\"center\">");
              client.println("<tr><th><p>設定</p></th><th><p>温度（℃）</p></th><th><p>回転比率（％）</p></th></tr>");
              client.println("<tr><th><p>設定1</p></th>");
              client.println("<th><p class=\"p002\">≦<input type=\"text\" name=\"temp1\" class=\"txt_1\" value=\"" + String(Temp_val1) + "\"></p></th>");
              client.println("<th><input type=\"text\" name=\"duty1\" class=\"txt_1\" value=\"" + String(Duty_val1) + "\"></th></tr>");
              client.println("<tr><th><p>設定2</p></th>");+
              client.println("<th><p class=\"p002\">≦<input type=\"text\" name=\"temp2\" class=\"txt_1\" value=\"" + String(Temp_val2) + "\"></p></th>");
              client.println("<th><input type=\"text\" name=\"duty2\" class=\"txt_1\" value=\"" + String(Duty_val2) + "\"></th></tr>");
              client.println("<tr><th><p>設定3</p></th>");
              client.println("<th><p class=\"p002\">＞設定2</p></th>");
              client.println("<th><input type=\"text\" name=\"duty3\" class=\"txt_1\" value=\"" + String(Duty_val3) + "\"></th></tr>");
              client.println("</table>");
              client.println("<p ><input type=\"submit\" value=\"SUBMIT\" class=\"btn_5\" /></p>");
              client.println("</div></form>");
            } else {
              // ----------------------------------------------------------
              // FAN制御（Auto:OFF）の場合
              // Duty比を設定するスライダーを表示
              // ※SUBMITボタンで決定
              // ----------------------------------------------------------
              client.println("<p ><a href=\"/FAN_on\"><button class=\"btn_3 btn_4\">ON</button></a><a href=\"/FAN_off\"><button class=\"btn_3\">OFF</button></a></p>");
              String Duty_text = "<h3>現在のFAN回転比率（Duty比）：" + String(Current_Duty_val) + "%</h3>";
              client.println(Duty_text);

              // 手動設定の場合、Duty比を入力するスライダーを表示
              client.println("<form method=\"get\" name=\"Duty_raito\" action=\"FAN_DutySet\">");
              client.println("<div class=\"box8\">");
              client.println("<p>FANを回転させる比率（Duty比）を設定し</p><p>SUBMITボタンを押してください。</p>");
              client.println("<h4>Duty比（％）</h4>");
              client.println("<input type=\"range\" name=\"duty\" id=\"rangebar\" min=\"0\" max=\"100\" step=\"5\" value=\"" + String(Duty_val) + "\">");
              client.println("<span id=\"val_1\">" + String(Duty_val) + "</span>");
              client.println("<script>");
              client.println("var elem = document.getElementById('rangebar');");
              client.println("var target = document.getElementById('val_1');");
              client.println("var rangeValue = function (elem, target) {");
              client.println("return function(evt){");
              client.println("target.innerHTML = elem.value;}}");
              client.println("elem.addEventListener('input', rangeValue(elem, target));</script>");
              client.println("<p ><input type=\"submit\" value=\"SUBMIT\" class=\"btn_5\" /></p>");
              client.println("</div></form>");
            }

            // 本文末
            client.println("</div></div></div></body></html>");
            // HTTP応答は別の空白行で終了
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            _CurrentLine = "";
          }
        } else if (_c != '\r') {  // if you got anything else but a carriage return character,
          _CurrentLine += _c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    Header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

// ------------------------------------------------------------
// LED発光制御用の関数 Function for LED emission control.
// led_show()
// ------------------------------------------------------------
void led_show(){

  // OFF：消灯
  if (Led_selected == 0 ) {
    FastLED.clear();
    FastLED.show();
  }
  // Count     ：一つずつ順に点灯させていく
  else if (Led_selected == 1 ) {
    for (int i = 0; i < LED_COUNT; i++) {
      if ( i != 0) {leds[i-1] = CRGB::Black;}  
      else if (i == 0 ) {leds[LED_COUNT-1] = CRGB::Black;} 
      leds[i] = CRGB::Red;
      FastLED.show();
      delay(50);
    }
  }
  // 単色：Single
  else if (Led_selected < 6) {
    CRGB SetColored;
    // Single    ：Red
    if (Led_selected == 2) {
      SetColored = CRGB::Red;
    }
    // Single    ：Purple
    else if (Led_selected == 3) {
      SetColored = CRGB::Purple;
    }
    // Single    ：Blue
    else if (Led_selected == 4) {
      SetColored = CRGB::Blue;
    }
    // Single    ：Green
    else if (Led_selected == 5) {
      SetColored = CRGB::Green;
    }
    for (int i = 0; i < LED_COUNT; i++) {
      leds[i] = SetColored;
      FastLED.show();
    }
    delay(5000);
  }
  //　Graduation：グラデーション
  else if (Led_selected == 6) {
    fill_rainbow( leds, LED_COUNT, gHue, 24);
    FastLED.show();
    FastLED.delay(1000 / FPS);
    EVERY_N_MILLISECONDS( 800 ) {
      gHue++;
    }
    delay(5000);
  }
  // Animation ：アニメーション（グラデーションイメージを回転させる）
  else if (Led_selected == 7) {
    
    int roll = 0;
    while (roll < 480) {
      for (int i = 0; i < LED_COUNT; i++) {
        leds[i] = CHSV(((i + roll) * 10) % 240, 255, 255);
      }
      FastLED.show();
      FastLED.delay(1000 / FPS);
      roll = roll + 1;
    }
  }  
}

// ------------------------------------------------------------
// Setup 関数　Setup function.
// setup()
// ------------------------------------------------------------
void setup() {
  // --------------------------------------------------
  // Step1: 初期化 Initialize
  // --------------------------------------------------
  // M5StickCの初期化と動作設定　Initialization and operation settings of M5StickC.
  M5.begin();                                                   // 開始
  M5.Lcd.setRotation(1);                                        // 画面の向きを変更（右横向き）Change screen orientation (left landscape orientation).
  M5.Axp.ScreenBreath(9);                                       // 液晶バックライト電圧設定 LCD backlight voltage setting.
  M5.Lcd.fillScreen(BLACK);                                     // 画面の塗りつぶし　Screen fill.

  // シリアルコンソールの開始　Start serial console.
  Serial.begin(115200);
  delay(500);

  // --------------------------------------------------
  // Step2: WIFI接続と時刻同期
  //        WIFI connection and time synchronization
  // --------------------------------------------------
  // WiFiでNTPサーバーから時刻を取得（電源On時のみ）Get time from NTP server via WiFi (only when power is on).
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause(); // スリープからの復帰理由を取得　Get the reason for waking up from sleep.

  // Wi-Fiアクセスポイント情報取得
  preferences.begin("AP-info", true);                           // 名前空間"AP-info"の指定と読み込みモード（true) 
  preferences.getString("ssid", ssid, sizeof(ssid));            // ssidの値を指定
  preferences.getString("pass", password, sizeof(password));    // passwordの値を指定 
  preferences.end();

  // Wifi 接続およびNTPサーバとの時刻同期　Wifi connection and time synchronization with NTP server.
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0      :                           // スリープからの復帰の場合
      break;                                                    // WiFiで時刻同期なし

    default                         :                           //　電源On時の場合
      Serial.printf("スリープ以外からの起動\n");                   // WiFiで時刻同期あり
      Serial.println(getCpuFrequencyMhz());
      M5.Lcd.setCursor(0, 0, 1);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(1);
      M5.Lcd.print("WiFi connecting");

      // Wifi 接続　Wifi connection
      WiFi.begin(ssid, password);
      // Wifi 接続の定期確認（1秒）Periodic confirmation of Wifi connection (1 second).
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        M5.Lcd.print(".");
      }
      M5.Lcd.println("\nconnected");
      Serial.println("IP address: "); 
      Serial.println(WiFi.localIP());

      // NTPサーバの時間をローカルの時刻に同期　Synchronize NTP server time to local time
      configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTPSRV);

      // Get local time
      struct tm timeInfo;
      if (getLocalTime(&timeInfo)) {
        M5.Lcd.print("NTP : ");
        M5.Lcd.println(NTPSRV);

        // Set RTC time
        RTC_TimeTypeDef TimeStruct;
        TimeStruct.Hours   = timeInfo.tm_hour;
        TimeStruct.Minutes = timeInfo.tm_min;
        TimeStruct.Seconds = timeInfo.tm_sec;
        M5.Rtc.SetTime(&TimeStruct);

        RTC_DateTypeDef DateStruct;
        DateStruct.WeekDay = timeInfo.tm_wday;
        DateStruct.Month = timeInfo.tm_mon + 1;
        DateStruct.Date = timeInfo.tm_mday;
        DateStruct.Year = timeInfo.tm_year + 1900;
        M5.Rtc.SetData(&DateStruct);
      }
      delay(1000);
      break; // switch 終わり

  }

  // --------------------------------------------------
  // Step3: 温度測定とFAN回転制御（PWM）の設定
  //        emperature measurement and FAN rotation control (PWM) settings
  // --------------------------------------------------
  // 温度測定とPWM送信のための環境設定　Preferences for temperature measurement and PWM transmission.
  // GPIOの入出力設定　GPIO input / output settings.
  //pinMode(TEMP_MEAN, INPUT);
  pinMode(FAN_PWM, OUTPUT);

  // PWMのチャネル設定　PWM Chanel set
  ledcSetup(CH0, PWM_FREQ, PWM_BIT);

  // GPIOとチャネルの紐づけ　Linking GPIO and channel.
  ledcAttachPin(FAN_PWM, CH0);

  // 温湿度計の開始　Start of thermo-hygrometer
  dht.begin();

  // --------------------------------------------------
  // Step4: LED点灯設定用WEBサーバの設定
  //        Setting of WEB server for LED lighting setting
  // --------------------------------------------------
  // Webサーバの開始　Web server start
  server.begin();

  // --------------------------------------------------
  // Step5: LED点灯制御の設定
  //        LED lighting control settings
  // --------------------------------------------------
  FastLED.addLeds<SK6812, ARGB_LED, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(254);
  set_max_power_in_volts_and_milliamps(5, 100);

}

// ------------------------------------------------------------
// Loop 関数 Loop function.
// loop()
// ------------------------------------------------------------
void loop() {
  M5.update();                    // M5状態更新　M5 status update.
  Clock_screen_display();         // 時計表示　Clock display.
  delay(980);                     // 0.98秒待ち
  Send_PWM();                     // FAN 回転制御用 PWM 送信 FAN rotation control PWM transmission.
  Check_TEMP();                   // ケース内の温度を取得　Get the temperature inside the case.
  Run_WEB();                      // WEB応答の生成　WEB response generation.

  led_show();                     // LEDライティングの更新
  
}
