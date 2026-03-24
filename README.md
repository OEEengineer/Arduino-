# 项目名称

AI+Arduino智能家居

## 📌 项目简介

一个基于ESP32-CAM的人脸识别门禁系统，同时集成了识物模块。以及一个语音识别系统，返还各个传感器的信息，完成对应操作

## ✨ 功能特性

*   **功能一**：人脸识别门禁控制。
*   **功能二**：拍照识别物体（调用大模型API）。
*   **功能三**：根据语音识别实现对应功能(读取传感器信息，控制家居)

## 🛠️ 硬件需求

<!-- 列出项目所需的所有硬件及其简要作用，可用表格形式。 -->

| 组件名称 | 型号 | 数量 | 作用 |
| :--- | :--- | :--- | :--- |
| 主控板1 | ESP32-CAM | 1 | 图像采集与WiFi传输 |
| 主控板2 | ESP32 | 1 | 接受传感器，语音模块信息 |
| 显示屏 | 0.96寸 OLED | 1 | 显示识别结果 |
| 按键 | 轻触开关 | 1 | 触发物体识别 |
| 语音识别模块 | SU-03T | 1 | 触发语音识别 |
| 水位传感器 | FC-28 | 1 | 测量水位 |
| 温度传感器 | DHT11 | 1 | 测量温湿度 |
| LED模块 | KY-016 | 1 | 亮灯 |
| 风扇 | 4线风扇 | 1 | 吹风，降温 |
| 抽水机 |  | 1 | 实现灌溉 |

## 🔧 软件与依赖

<!-- 说明项目依赖的软件环境、库或API。 -->
*   **开发环境**：Arduino IDE / Python 3.11 
*   **Python库**：`face_recognition`, `opencv-python`, `requests` ,`dashscope`,`flask`,`dlib`
*   **API**：千问API（或其他大模型API）
*   **Arduino库**：`WiFi.h`, `HTTPClient.h`, `Wire.h`,`Adafruit_GFX.h`,`Adafruit_SSD1306.h`,`"esp_camera.h"`，`LiquidCrystal_I2C.h`,`DHT11`,`SoftwareSerial.h`

## 🚀 快速开始

### 安装与配置

1.  **克隆仓库**
    ```bash
    git clone https://github.com/OEEengineer/Arduino-.git
    ```
2.  **硬件连接，实物组装**

    <img width="377" height="502" alt="73b2500af5b0e46403bef4f5104a53ce_720" src="https://github.com/user-attachments/assets/c8ca79b9-2439-4998-87a4-cab5140680b9" />

    <img width="377" height="503" alt="733616432ea7f55d6f50f918ceb28c19_720" src="https://github.com/user-attachments/assets/ba7992cb-113c-48b1-8dbd-6c69f8415665" />
    
    <img width="377" height="503" alt="IMG_20260324_150257" src="https://github.com/user-attachments/assets/cab924ad-5223-479b-95e6-c67b30b59d18" />
    <img width="377" height="503" alt="IMG_20260324_150208" src="https://github.com/user-attachments/assets/95e83580-100c-438f-9271-f4daef8b1d25" />
    <img width="377" height="503" alt="954cc71bfca8e0f053951ea05c1da3bd" src="https://github.com/user-attachments/assets/04aa2190-34ac-412e-8386-3a0839d4c514" />
    
    




    
4.  **配置软件环境**
    对与人脸识别与识物系统:安装Python依赖库、在Arduino IDE中配置ESP32开发板、在代码中填入你的API密钥（或者将API加入环境变量）、修改你的WiFissid与密码、电脑的内网地址、传入你的人脸照片到python文件相同目录下,增加python文件中的数据库
    对于家居系统，只需设定语音AI的唤醒，识别词，安装项目所需的库
    
6.  **上传代码**
    esp32-cam:通过usb转ttl，先短接IO0与GND上传，再拔掉短接帽，即上传成功
    esp32:直接通过typeC接口上传代码
    
### 使用说明

<!-- 介绍如何使用你的系统，例如： -->
1.  **启动系统**：给ESP32上电。
2.  **人脸识别**：摄像头前出现人脸，系统自动识别。
3.  **物体识别**：按下按键，等待约3秒，OLED显示识别出的物体名称。
4.  **语音识别**: 说出对应操作，返回相应的信息(如询问当前温度，帮忙浇水，打开风扇等)


## 🗺️ 项目结构

文件夹是python服务器文件，可以运行人脸识别以及将数据传输给千问识别图像
face_recognition文件是esp32-cam的运行代码
temhumi是语音模块代码
