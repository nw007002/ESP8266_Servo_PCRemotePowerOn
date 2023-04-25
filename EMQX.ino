#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SSD1306Wire.h>
#include <string>
#include <Ticker.h>
#include <Servo.h>
Servo myservo;
Ticker flipper;
int pos = 0;

SSD1306Wire display(0x3c, SDA, SCL);
// WiFi
const char *ssid = "Nanami0721"; // Enter your WiFi name
const char *password = "yuzusoft";  // Enter WiFi password
// const char *ssid = "0d000721";
// const char *password = "147258369";
const char *ssid_ntp = "IAA";
const char *password_ntp = "147258369";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io"; // broker address
const char *topic_sub = "/esp0721";
const char *topic_pub = "/esp0722"; // define topic pub
const char *mqtt_username = "emqx"; // username for authentication
const char *mqtt_password = "public"; // password for authentication
const int mqtt_port = 1883; // port of MQTT over TCP

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    // Set software serial baud to 115200;
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, OUTPUT);
    myservo.attach(14);  //D5
    myservo.write(0);   //复位

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

    flipper.attach(30, flip);   //定时器
}


void flip() {
    // const char* systime = (char*)millis();
    String a = "LEVY9 运行时间：" + String(millis() / 1000) + "s";
    const char* systime = a.c_str();
    client.publish(topic_pub, systime);
    Serial.printf("sent:%s",a);
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

    //收到on信号
    if (message == "1") {
        //  digitalWrite(LED_BUILTIN, LOW); 
        client.publish(topic_pub, "启动中");
        display.drawString(0, 28, "SERVO START");
        display.display();
        for (pos = 0; pos <= 180; pos += 1) {
            myservo.write(pos); 
            delay(15);
        }
        delay(2000);    //等待电流扭矩到位
        myservo.write(0);   //复位
        client.publish(topic_pub, "启动完成，复位完成");
        display.clear();
        display.drawString(0, 42, "START OVER");
    }
    //收到off信号
    else if (message == "0") {
        //  digitalWrite(LED_BUILTIN, HIGH); 
        myservo.write(0);   //复位
        client.publish(topic_pub, "复位完成");
        display.drawString(0, 42, "RESET");
    }
    else if (message == "help") {
        client.publish(topic_pub, "输入1开机并自动复位,输入0复位,输入其他将直接打印");
    }
    //其他情况
    else{
        client.publish(topic_pub, "听不懂,重来!输入help获取帮助");
        display.drawString(0, 42, message);
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
            Serial.println("Public emqx mqtt broker REconnected");
        } else {
            Serial.print("Reconnect failed with state ");
            Serial.print(client.state());
            display.drawString(0, 20, "mqtt re fail:" + String(client.state()));
            display.display();
            delay(5000);
        }
    }
    display.clear();
    client.subscribe(topic_sub);
}

void loop() {
    // display.clear();
    if(!client.connected()){
        reconnect();
    }
    client.loop();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Public LEVY9");
    display.drawString(0, 10, "WLAN AP: " + String(ssid));
    display.drawString(0, 20, "pub:" + String(topic_pub));
    display.drawString(0, 30, "sub:" + String(topic_sub));
    display.drawHorizontalLine(0, 41, 64);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    String syssec = String(millis() / 1000) + "s";
    display.drawString(128, 0, "syssec");
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.display();
}
