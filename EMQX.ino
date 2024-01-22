#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SSD1306Wire.h>
#include <string>
#include <Ticker.h>
#include <Servo.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>

Servo myservo;
Ticker flipper;
int pos = 0;
WiFiUDP UDP;
WakeOnLan WOL(UDP);
const char *MACAddress = "D8:5E:D3:8D:2E:CC";

SSD1306Wire display(0x3c, SDA, SCL);
// WiFi
const char *ssid = "AyachiNene_NEET"; // Enter your WiFi name
const char *password = "yuzusoft";  // Enter WiFi password
// const char *ssid = "0d000721";
// const char *password = "147258369";
// const char *ssid_ntp = "IAA";
// const char *password_ntp = "147258369";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io"; // broker address
const char *topic_sub = "/esp0721";
const char *topic_pub = "/esp0722"; // define topic pub
const char *mqtt_username = "emqx"; // username for authentication
const char *mqtt_password = "public"; // password for authentication
const int mqtt_port = 1883; // port of MQTT over TCP

bool timeset = false;
short month, day = 1;
unsigned long hour, minu, sec = 0;
bool time_mode = false;
unsigned char time_stat = '?';
unsigned long millis_temp = 0;
unsigned long interval_sec[2] = {0};
int reconnect_times = 0;
int buttonState = 0;
int wol_times = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void WOLwake() {
    WOL.sendMagicPacket(MACAddress);
}

void ICACHE_RAM_ATTR ISR() {    //硬件中断函数
    WOLwake();
    wol_times ++;
}

void setup() {
    // Set software serial baud to 115200;
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    myservo.attach(14);  //D5
    myservo.write(0);   //复位

    pinMode(12, INPUT_PULLUP);    //设置上拉
    pinMode(13, OUTPUT);   //外部开关，两根线
    digitalWrite(13, LOW);    //提供低电平
    attachInterrupt(12, ISR, FALLING);    //设置中断，下降沿触发

    flipper.attach(30, flip);   //定时器
    flipper.attach(1, flip2);

    WOL.setRepeat(3, 100);

    display.init();
    display.clear();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // connecting to a WiFi network
    WiFi.begin(ssid, password);
    display.drawString(0, 0, "Connecting to " + String(ssid));
    display.display();
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.printf(".");
    // }
    for(int i=0,j=20; WiFi.status() != WL_CONNECTED ;i+=2){
        delay(500);
        Serial.printf(".");
        display.setPixel(i, j);
        if(i >= 64){
            i = 0;
            j += 10;
        }
        display.display();
    }
    display.clear();
    // display.drawString(0, 0, "Connected to  " + String(ssid));
    Serial.println(WiFi.localIP());

    WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());

    //connecting to a mqtt broker
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    while (!client.connected()) {
        String client_id = "esp8266-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public emqx mqtt broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            display.drawString(0, 0, "mqtt fail:" + String(client.state()));
            delay(5000);
        }
        display.display();
    }
    display.clear();

    // publish and subscribe
    const char *sendbuf = "LEVY9 online";
    Serial.printf("Topic_pub name:%s\n",topic_pub);
    Serial.printf("Topic_sub name:%s\n",topic_sub);
    client.subscribe(topic_sub);
    client.publish(topic_pub, sendbuf);
    Serial.printf("message sent:%s\n",sendbuf);

}


void flip() {   //定时器
    // const char* systime = (char*)millis();
    String a = "LEVY9 系统运行时间：" + String(millis() / 3600000) + "h"+ String(millis() / 60000 % 60) + "m" + String(millis() / 1000 % 60) + "s";
    const char* systime = a.c_str();
    client.publish(topic_pub, systime);
    Serial.printf("sent:%s\n",a);
}

void flip2() {  //现实时间
    sec ++;
    minu += sec / 60;
    hour += minu / 60;
    sec %= 60;
    minu %= 60;
    hour %= 24;
}


void callback(char *topic, byte *payload, unsigned int length) {
    // Serial.print("Message arrived in topic: ");
    // Serial.println(topic);
    // Serial.print("Message:");
    display.clear();
    display.setFont(ArialMT_Plain_16);
    String message;
    for (int i = 0; i < length; i++) {
        message = message + (char) payload[i];  // convert *byte to string
    }
    Serial.printf("Message received:%s\n",message);

    if(time_mode) {   //时间设置模式
        switch(time_stat){
            case 'M':
                month = message.toInt();
                client.publish(topic_pub, "输入天:");
                time_stat = 'D';
                break;
            case 'D':
                day = message.toInt();
                client.publish(topic_pub, "输入小时:");
                time_stat = 'h';
                break;
            case 'h':
                hour = message.toInt();
                client.publish(topic_pub, "输入分钟:");
                time_stat = 'm';
                break;
            case 'm':
                minu = message.toInt();
                client.publish(topic_pub, "输入秒:");
                time_stat = 's';
                break;
            case 's':
                sec = message.toInt();
                client.publish(topic_pub, "设置完成.");
                time_stat = '!';
                break;
        }
        if(time_stat == '!'){
            String currenttime = "已设置时间:"+String(month)+"月"+String(day)+"日"+String(hour)+'h'+String(minu)+'m'+String(sec)+'s';
            const char* currenttime2 = currenttime.c_str();
            client.publish(topic_pub, currenttime2);
            millis_temp = millis();
            time_mode = false;
        }
        return;
    }

    //收到on信号
    if (message == "1") {
        //  digitalWrite(LED_BUILTIN, LOW); 
        client.publish(topic_pub, "启动中");
        display.drawString(0, 28, "SERVO START");
        digitalWrite(LED_BUILTIN, LOW);
        display.display();
        for (pos = 0; pos <= 180; pos += 1) {
            myservo.write(pos); 
            delay(15);
        }
        delay(2500);    //等待电流扭矩到位
        myservo.write(0);   //复位
        client.publish(topic_pub, "启动完成，复位完成");
        digitalWrite(LED_BUILTIN, HIGH);
        display.clear();
        display.drawString(0, 40, "START OVER");
    }
    //收到零状态信号
    else if (message == "0") {
        //  digitalWrite(LED_BUILTIN, HIGH); 
        myservo.write(0);   //复位
        client.publish(topic_pub, "复位完成");
        display.drawString(0, 40, "RESET");
    }
    //WOL模式启动
    else if (message == "2") {
        WOLwake();
        client.publish(topic_pub, "WOL启动完成");
        display.drawString(0, 40, "WOL WAKE UP");
    }
    //时间设置模式
    else if (message == "timeset") {
        time_mode = true;
        client.publish(topic_pub, "时间设置模式，请输入月份:");
        time_stat = 'M';
    }
    //查重连次数
    else if (message == "recon") {
        String recon = "重新连接次数:" + String(reconnect_times);
        const char* recon2 = recon.c_str();
        client.publish(topic_pub, recon2);
    }
    //问询模式
    else if (message == "help") {
        client.publish(topic_pub, "输入1舵机开机并自动复位,输入0复位,输入2用WOL开机,输入time获取时间,输入timeset设置时间,recon查询重连次数,输入其他将直接打印");
    }
    //其他情况
    else{
        client.publish(topic_pub, "听不懂,重来!输入help获取帮助");
        display.drawString(0, 40, message);
    }
}

void reconnect(){
    display.clear();
    Serial.println("Reconnecting...");
    display.drawString(0, 0, "Reconnecting...");
    display.display();
    while (!client.connected()) {
        String client_id = "esp8266-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s REconnecting to the public mqtt broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public emqx mqtt broker REconnected\n");
        } else {
            Serial.print("Reconnect failed with state ");
            Serial.println(client.state());
            display.drawString(0, 20, "MQTT re fail:" + String(client.state()));
            display.display();
            delay(5000);
        }
    }
    display.clear();
    client.subscribe(topic_sub);
    reconnect_times ++;
}

void loop() {
    // display.clear();
    if(!client.connected()){
        reconnect();
    }
    client.loop();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "LEVY9");
    display.drawString(0, 10, "AP: " + String(ssid));
    display.drawString(0, 20, "sub:" + String(topic_sub));
    display.drawString(0, 30, "pub:" + String(topic_pub));
    // display.drawHorizontalLine(0, 41, 64);

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setColor(BLACK);
    for(int i=128;i>40;i--){
        display.drawLine(i, 0, i, 10);
    }
    for(int j=128;j>40;j--){
        display.drawLine(j, 54, j, 64);
    }
    display.setColor(WHITE);

    String syssec = String(hour) + ":"+ String(minu) + ":" + String(sec);
    display.drawString(128, 0, syssec);

    String wol_info = "WOL times: " + String(wol_times);
    display.drawString(128, 54, wol_info);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.display();
}
