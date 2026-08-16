/*
 * ============================================================================
 * AtomS3R Chain OSC Sender
 * ============================================================================
 *
 * M5Stack AtomS3R + Atomic ToChain Base 向け OSC 送信ファームウェア
 * （主に VRChat アバターパラメータ操作用）
 *
 * ----------------------------------------------------------------------------
 * 対応デバイス
 *   - Chain Key
 *   - Chain Encoder
 *   - Chain Angle
 *   - Chain Joystick
 *
 * 主な機能
 *   - 初回は AP モード（SSID: AtomS3R-OSC / Pass: 12345678）で WiFi 設定
 *   - ブラウザから OSC 送信先・デバイスごとのパラメータを設定
 *   - 設定はデバイス UID 単位で NVS に保存（ホットスワップ対応）
 *   - 画面 10 秒長押しで全設定リセット
 *
 * ----------------------------------------------------------------------------
 * 必要なライブラリ（platformio.ini の lib_deps から自動取得）
 *   - M5Unified
 *   - M5Chain          （M5Stack 公式）
 *   - ArduinoOSC       （hideakitai / ArduinoOSCWiFi）
 *
 * PlatformIO 環境
 *   - board: m5stack-atoms3（AtomS3R / ESP32-S3）
 *   - framework: arduino
 *
 * ----------------------------------------------------------------------------
 * ファイル構成
 *
 *   M5ChainOSC.ino          … Arduino IDE用エントリーポイント
 *   src/
 *     main.cpp              … 共通実装・PlatformIO用エントリーポイント
 *     app.h                 … 共通アプリケーション関数の宣言
 *     config.h              … ピン・定数・列挙型
 *     types.h               … 構造体定義
 *     globals.h / .cpp      … 共有状態・小さなヘルパ
 *     display.h / .cpp      … 画面描画・回転・リセット演出
 *     storage.h / .cpp      … NVS 永続化・Known リスト
 *     osc_send.h / .cpp     … OSC 送信
 *     chain_devices.h / .cpp… 列挙・ホットスワップ・ポーリング
 *     wifi_manager.h / .cpp … STA / AP 接続
 *     web_ui.h / .cpp       … Web 設定画面
 *
 * PlatformIO プロジェクトとして、src/ 以下をコンパイルします。
 *
 * ----------------------------------------------------------------------------
 * 開発
 *   本ファームウェアは Grok (xAI) との反復開発により作成・整理されています。
 * ============================================================================
 */

#include <M5Unified.h>

#include "app.h"
#include "globals.h"
#include "display.h"
#include "storage.h"
#include "chain_devices.h"
#include "wifi_manager.h"
#include "web_ui.h"
#include "memory_debug.h"

void appSetup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  Serial.begin(115200);
  delay(200);
  MEMORY_DEBUG_LOG("SETUP_M5_READY", 0);

  loadWifiAndOscCommon();
  MEMORY_DEBUG_LOG("SETUP_SETTINGS_LOADED", 0);
  applyDisplayRotation();
  showMessage("START");

  connectOrStartAP();
  MEMORY_DEBUG_LOG("SETUP_WIFI_READY", 0);
  initChainBus();
  MEMORY_DEBUG_LOG("SETUP_CHAIN_READY", 0);

  if (!isAPMode) drawMainScreen();
  lastReenumMs = millis();
  MEMORY_DEBUG_LOG("SETUP_END", 0);
}

void appLoop() {
  M5.update();
  server.handleClient();
  handleWifiLoop();

  // --- Factory reset (10s hold on BtnA) ---------------------------------
  if (M5.BtnA.isPressed()) {
    if (resetPressStart == 0) {
      resetPressStart = millis();
      resetInProgress = true;
      lastResetDrawPercent = -1;
    }
    unsigned long held = millis() - resetPressStart;
    showResetProgress(held);
    if (held >= RESET_HOLD_MS) resetAllSettings();
  } else if (resetInProgress) {
    resetInProgress = false;
    resetPressStart = 0;
    lastResetDrawPercent = -1;
    if (!isAPMode)
      drawMainScreen();
    else
      showMessage("AP Mode", "192.168.4.1");
  }
  if (resetInProgress) {
    delay(20);
    return;
  }

  // --- Hot-swap re-enumeration ------------------------------------------
  if (millis() - lastReenumMs >= REENUM_INTERVAL_MS) {
    lastReenumMs = millis();
    refreshChainDevices(false);
  }

  if (isAPMode) {
    delay(10);
    return;
  }

  // --- Poll connected Chain devices & send OSC --------------------------
  pollAllDevices();
  delay(10);
}

// PlatformIOはプロジェクトルートのM5ChainOSC.inoをビルドしないため、
// ここからArduinoのエントリーポイントを提供する。
#ifdef M5CHAINOSC_PLATFORMIO
void setup() {
  appSetup();
}

void loop() {
  appLoop();
}
#endif
