# karakuri-HAKO-

## Overview
このプログラムは、karakuri-musha.comで公開している作品「Karakuri-HAKO-」の制御用プログラムです。
This program is a control program for the work "Karakuri -HAKO-" published on karakuri-musha.com.
The following functions are implemented.

### 使用方法や具体的な内容 （Usage and specific contents）
以下のサイトで紹介しています。It is introduced on the following site.
 - Blog https://karakuri-musha.com/category/products/karakuri/case01-hako/
 - Youtube https://youtu.be/UxkQyWFf1Vs

### 動作環境 (Operating environment)
 - Arduino IDE 1.8.15
 - Library：M5StickC（0.2.0）、FastLED（3.3.3）、DHT sensor library（1.4.0）
 - Boadmgr：esp32（1.0.6）
 - MicroConputer：M5StickC
 - FAN：COOLER MASTER MASTERFAN MF120 HALO
 - Sensor：DHT11

## 機能(function)
 - 電源投入時にWifi経由でNTPサーバへ時刻同期を行う    :Synchronize the time to the NTP server via Wifi when the power is turned on.
 - 温度センサーでケース内の温度を測定                 :Measure the temperature inside the case with a temperature sensor.
 - 測定温度によりファンの回転数を制御                 :Fan speed is controlled by the measured temperature.
 - 測定温度により時刻表示の文字色を変化               :The text color of the time display changes depending on the measurement temperature.
 - 搭載FAN（ARGB対応FAN）のライティング               :Equipped FAN (ARGB compatible FAN) lighting [Single color (red, purple, blue, green), illumination, rolling]
   [単色（赤、紫、青、緑）、グラデーション、アニメーション]

### (1)LED点灯モードの選択方法（Webブラウザ経由）      :How to select LED lighting mode (via web browser).
 - http[80]でM5StickCに接続                           :Connect to M5 Stick C with http [80].
 - 画面上のボタンから点灯モードをクリックする          :Click the lighting mode from the button on the screen.

### (2)FAN回転速度の手動指定方法（Webブラウザ経由）    :How to manually specify the FAN rotation speed (via a web browser).
 - http[80]でM5StickCに接続                           :Connect to M5 Stick C with http [80].
 - 画面上の「FAN手動設定」ボタンをクリックする         :Click the "FAN Manual Setting" button on the screen.
 - 画面上の回転数指定スライダでPWMを指定               :Specify PWM with the rotation speed specification slider on the screen.
 - 画面上の「FAN回転設定」ボタンをクリックする         :Click the "FAN Rotation Settings" button on the screen.
