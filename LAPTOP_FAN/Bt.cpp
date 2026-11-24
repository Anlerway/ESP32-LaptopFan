#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "FanControl.h"
#include "Display.h"
#include "Bt.h"
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 设备名来自 config.h（BT_DEVICE_NAME，可用 -D 覆盖）
const String DEVICE_NAME = BT_DEVICE_NAME;
// 经典蓝牙串口（COM 模式）设备名加 -COM 后缀，便于与 BLE 设备区分
const String DEVICE_NAME_COM = BT_DEVICE_NAME_COM;

// Nordic UART Service（与上位机约定）
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BluetoothSerial SerialBT;
bool wasBtConnected = false;
BLEServer* bleServer = nullptr;
BLECharacteristic* bleTxChar = nullptr;
BLECharacteristic* bleRxChar = nullptr;
bool bleConnected = false;

// BLE 隐藏模式下的"无数据自动可见"：超时未收到数据且未连接 → 临时广播，收到数据后隐藏。
unsigned long lastBleDataTime = 0;
bool autoVisible = false;
const unsigned long BLE_AUTO_VISIBLE_TIMEOUT_MS = 10000;

// 最近收到上位机数据的时间（断连/无数据判定：超时后界面显示 "--"、转速归 0）
unsigned long lastDataMillis = 0;
const unsigned long Bt_DATA_ACTIVE_MS = 5000;

// 非阻塞按行接收缓冲：readStringUntil 会阻塞等待换行符（最长 1s），
// 主循环被卡住后时间按 millis 派生会在恢复时一次跳过两秒。
static String usbRxBuffer = "";
static String btRxBuffer = "";

class BleServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* server) override
  {
    bleConnected = true;
    if (autoVisible)
    {
      autoVisible = false;
      BLEDevice::stopAdvertising();
      LOG_INFO("BLE auto-visible off (connected)");
    }
    LOG_INFO("BLE client connected");
  }

  void onDisconnect(BLEServer* server) override
  {
    bleConnected = false;
    if (btVisible)
    {
      BLEDevice::startAdvertising();
    }
    LOG_INFO("BLE client disconnected");
  }
};

class BleRxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* characteristic) override
  {
    std::string value = characteristic->getValue();
    if (value.length() == 0)
    {
      return;
    }

    String line = String(value.c_str());
    line.trim();
    if (line.length() > 0)
    {
      Bt_HandleLine(line, false);
    }
  }
};

void Bt_Init()
{
  if (btMode == BT_MODE_COM)
  {
    SerialBT.begin(DEVICE_NAME_COM.c_str());
    Bt_ApplyVisibility();
    LOG_INFO("Bluetooth COM started: %s", DEVICE_NAME.c_str());
  }
  else
  {
    BLEDevice::init(DEVICE_NAME.c_str());
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());

    BLEService* service = bleServer->createService(NUS_SERVICE_UUID);

    bleRxChar = service->createCharacteristic(
      NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    bleRxChar->setCallbacks(new BleRxCallbacks());

    bleTxChar = service->createCharacteristic(
      NUS_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
    bleTxChar->addDescriptor(new BLE2902());

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(NUS_SERVICE_UUID);
    advertising->setScanResponse(true);

    Bt_ApplyVisibility();
    LOG_INFO("Bluetooth BLE started: %s", DEVICE_NAME.c_str());
  }

  lastBleDataTime = millis();
}

void Bt_ApplyVisibility()
{
  if (btMode == BT_MODE_COM)
  {
    esp_bt_gap_set_scan_mode(
      ESP_BT_CONNECTABLE,
      btVisible ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE);
  }
  else
  {
    if (btVisible)
    {
      BLEDevice::startAdvertising();
    }
    else
    {
      BLEDevice::stopAdvertising();
    }
  }
  LOG_DEBUG("BT visible -> %s", btVisible ? "ON" : "OFF");
}

void Bt_Loop()
{
  // 物理串口（USB，对应上位机"串口模式"连接 COMx）
  while (Serial.available())
  {
    char c = (char)Serial.read();
    if (c == '\n')
    {
      usbRxBuffer.trim();
      if (usbRxBuffer.length() > 0)
      {
        Bt_HandleLine(usbRxBuffer, true);
      }
      usbRxBuffer = "";
    }
    else if (usbRxBuffer.length() < 127)
    {
      usbRxBuffer += c;
    }
  }

  // 经典蓝牙 SPP（固件 COM 模式，对应电脑配对后的虚拟 COM 口）
  if (btMode == BT_MODE_COM)
  {
    while (SerialBT.available())
    {
      char c = (char)SerialBT.read();
      if (c == '\n')
      {
        btRxBuffer.trim();
        if (btRxBuffer.length() > 0)
        {
          Bt_HandleLine(btRxBuffer, false);
        }
        btRxBuffer = "";
      }
      else if (btRxBuffer.length() < 127)
      {
        btRxBuffer += c;
      }
    }
  }

  // 经典蓝牙客户端连接状态变化：连上时主动回 READY 作为握手信号
  if (btMode == BT_MODE_COM)
  {
    bool connected = SerialBT.hasClient();
    if (connected != wasBtConnected)
    {
      wasBtConnected = connected;
      if (connected)
      {
        SerialBT.println("READY");
        LOG_INFO("BT SPP client connected");
      }
      else
      {
        LOG_INFO("BT SPP client disconnected");
      }
    }
  }

  // BLE 隐藏模式：超过 N 秒无数据且未连接 → 临时可见，便于上位机重连
  if (btMode == BT_MODE_BLE && !btVisible && !autoVisible && !bleConnected
      && (millis() - lastBleDataTime >= BLE_AUTO_VISIBLE_TIMEOUT_MS))
  {
    autoVisible = true;
    BLEDevice::startAdvertising();
    LOG_INFO("BLE auto-visible: no data for %d ms", (int)BLE_AUTO_VISIBLE_TIMEOUT_MS);
  }
}

void Bt_HandleLine(const String& line, bool fromUsb)
{
  LOG_DEBUG("RX: %s", line.c_str());
  lastDataMillis = millis();

  if (btMode == BT_MODE_BLE && !fromUsb)
  {
    lastBleDataTime = millis();
    if (autoVisible)
    {
      autoVisible = false;
      BLEDevice::stopAdvertising();
      LOG_INFO("BLE hidden again after data received");
    }
  }

  if (line.startsWith("PWM:"))
  {
    int value = line.substring(4).toInt();
    value = constrain(value, 0, 100);
    if (autoMode)
    {
      Fan_SetDuty(value);
    }
    hasSensorData = true;
  }
  else if (line.startsWith("TIME:"))
  {
    String rest = line.substring(5);
    DateTime t;
    t.year = rest.substring(0, 4).toInt();
    t.month = rest.substring(5, 7).toInt();
    t.day = rest.substring(8, 10).toInt();
    t.hour = rest.substring(11, 13).toInt();
    t.minute = rest.substring(14, 16).toInt();
    t.second = rest.substring(17, 19).toInt();
    Time_SetExternal(t);
    LOG_INFO("Time set: %04d/%02d/%02d %02d:%02d:%02d",
             currentTime.year, currentTime.month, currentTime.day,
             currentTime.hour, currentTime.minute, currentTime.second);
  }
  else if (line.startsWith("STATUS?"))
  {
    String status = "STATUS:mode=" + String(autoMode ? "AUTO" : "MANUAL")
                  + " fan=" + String(dutyCycle)
                  + " freq=" + String(fanFrequency)
                  + " cpu=" + String(cpuValue)
                  + " gpu=" + String(gpuValue)
                  + " bt=" + String(btMode == BT_MODE_BLE ? "BLE" : "COM");
    // 从哪个通道来的就回哪个通道（USB 串口 / 经典蓝牙 / BLE）
    if (fromUsb)
    {
      Serial.println(status);
    }
    else if (btMode == BT_MODE_BLE)
    {
      if (bleConnected && bleTxChar != nullptr)
      {
        bleTxChar->setValue((uint8_t*)status.c_str(), status.length());
        bleTxChar->notify();
      }
    }
    else
    {
      SerialBT.println(status);
    }
    LOG_DEBUG("TX: %s", status.c_str());
  }
  else if (line.startsWith("CPU"))
  {
    cpuValue = line.substring(3).toFloat();
    hasSensorData = true;
  }
  else if (line.startsWith("GPU"))
  {
    gpuValue = line.substring(3).toFloat();
    hasSensorData = true;
  }
}

// 最近 5 秒内是否收到上位机数据（USB 串口 / 经典蓝牙 / BLE 任一通道均可）
bool Bt_IsDataActive()
{
  return (millis() - lastDataMillis) < Bt_DATA_ACTIVE_MS;
}

bool Bt_IsConnected()
{
  if (btMode == BT_MODE_COM)
  {
    return SerialBT.hasClient();
  }
  return bleConnected;
}

void Bt_SwitchModeAndRestart()
{
  btMode = 1 - btMode;
  Storage_Save();  // 立即落盘，重启后生效
  LOG_INFO("BT mode -> %s, restarting...", btMode == BT_MODE_BLE ? "BLE" : "COM");
  Display_ShowRestarting();
  delay(1000);
  ESP.restart();
}
