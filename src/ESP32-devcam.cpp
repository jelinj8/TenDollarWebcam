#include <Arduino.h>

#include "OV2640.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <AutoConnect.h>

#include "SimStreamer.h"
#include "OV2640Streamer.h"
#include "OV2640.h"
#include "CRtspSession.h"

// #define USEBOARD_TTGO_T
// 
#define USEBOARD_AITHINKER
// #define USEBOARD_ESP_CAM

// #define ENABLE_OLED //if want use oled, turn on thi macro
// 
#define ENABLE_WEBSERVER
// 
#define ENABLE_RTSPSERVER

// when pulled low, forces AP mode
#define FACTORYRESET_BUTTON 16

#define HOSTNAME "ESP32-Camera"
#define HOSTNAME_CFG "ESP32-CameraCfg"

#ifdef ENABLE_OLED
#include "SSD1306.h"
#define OLED_ADDRESS 0x3c

#ifdef USEBOARD_TTGO_T
#define I2C_SDA 21
#define I2C_SCL 22
#else
#define I2C_SDA 14
#define I2C_SCL 13
#endif
SSD1306Wire display(OLED_ADDRESS, I2C_SDA, I2C_SCL, GEOMETRY_128_32);
bool hasDisplay; // we probe for the device at runtime
#endif

OV2640 cam;

WebServer server(80);
AutoConnect Portal(server);

AutoConnectConfig Config("CamCFG", "12345678");

#ifdef ENABLE_RTSPSERVER
WiFiServer rtspServer(8554);
#endif

#ifdef ENABLE_WEBSERVER
void handle_jpg_stream(void)
{
    Serial.println("stream");
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (1)
    {
        cam.run();
        if (!client.connected())
            break;
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);

        client.write((char *)cam.getfb(), cam.getSize());
        Serial.printf("%lu JPG frame\n", millis());
        server.sendContent("\r\n");
        if (!client.connected())
            break;
    }
}

void handle_jpg(void)
{
    Serial.printf("%lu jpg\n", millis());
    WiFiClient client = server.client();

    cam.run();
    if (!client.connected())
    {
        return;
    }
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-disposition: inline; filename=capture.jpg\r\n";
    response += "Content-type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write((char *)cam.getfb(), cam.getSize());
}

void handle_root(void)
{
    Serial.printf("%lu root\n", millis());
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-disposition: inline; filename=index.htm\r\n";
    response += "Content-type: text/html\r\n\r\nRoot\r\n";
    server.sendContent(response);
}

void handleNotFound()
{
    Serial.printf("%lu Not found: ", millis());
    Serial.println(server.uri());
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}
#endif

#ifdef ENABLE_OLED
void lcdMessage(String msg)
{
    if(hasDisplay) {
        display.clear();
        display.drawString(128 / 2, 32 / 2, msg);
        display.display();
    }
}
#endif

void setup()
{
  #ifdef ENABLE_OLED
    hasDisplay = display.init();
    if(hasDisplay) {
        display.flipScreenVertically();
        display.setFont(ArialMT_Plain_16);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
    }
    lcdMessage("booting");
  #endif

    Serial.begin(115200);
    while (!Serial)
    {
        ;
    }

    
#if defined(USEBOARD_TTGO_T)
    camera_config_t cconfig = esp32cam_ttgo_t_config;
#elif defined(USEBOARD_AITHINKER)
    camera_config_t cconfig = esp32cam_aithinker_config;
#elif defined(USEBOARD_ESP_CAM)
    camera_config_t cconfig = esp32cam_config;
#else
    #error "BOARD NOT DEFINED"
#endif

    if(psramFound()){
        //cconfig.frame_size = FRAMESIZE_UXGA;
        cconfig.frame_size = FRAMESIZE_SXGA;
        cconfig.jpeg_quality = 10;
        // cconfig.fb_count = 2;
        cconfig.fb_count = 1;
        Serial.println("PSRAM found.");
    } else {
        cconfig.frame_size = FRAMESIZE_SVGA;
        cconfig.jpeg_quality = 12;
        cconfig.fb_count = 1;
    }

    int camInit = cam.init(cconfig);
    Serial.printf("Camera init returned %d\n", camInit);

#ifdef ENABLE_OLED
    lcdMessage(ip.toString());
#endif

    #ifdef ENABLE_WEBSERVER
        server.on("/", HTTP_GET, handle_root);
        server.on("/stream", HTTP_GET, handle_jpg_stream);
        server.on("/jpg", HTTP_GET, handle_jpg);
        server.onNotFound(handleNotFound);
    #endif


    Config.autoReconnect = true;
    Config.title = "Camera config";
    Config.tickerPort = 4;
    Config.tickerOn = HIGH;
    Config.homeUri = "/jpg";

    #ifdef FACTORYRESET_BUTTON
    pinMode(FACTORYRESET_BUTTON, INPUT);
    if(!digitalRead(FACTORYRESET_BUTTON)){     // 1 means not pressed
        Serial.println("Force AP mode");
        Config.hostName = HOSTNAME_CFG;
        Config.immediateStart = true;
    }else{
        Config.hostName = HOSTNAME;
    }
    #else
        Config.hostName = HOSTNAME;
    #endif

    Portal.config(Config);

    if(Portal.begin()){
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    }
    else
        Serial.println("Failed to connect WiFi");

#ifdef ENABLE_RTSPSERVER
    rtspServer.begin();
    Serial.println("RTSP server started.");
#endif

}

#ifdef ENABLE_RTSPSERVER
CStreamer *streamer;
CRtspSession *session;
WiFiClient client; // FIXME, support multiple clients
#endif

void loop()
{

Portal.handleClient();

#ifdef ENABLE_RTSPSERVER
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    // (FIXME - support multiple simultaneous clients)
    if(session) {
        session->handleRequests(0); // we don't use a timeout here,
        // instead we send only if we have new enough frames

        uint32_t now = millis();
        if(now > lastimage + msecPerFrame || now < lastimage) { // handle clock rollover
            session->broadcastCurrentFrame(now);
            lastimage = now;
            printf("%d RTSP frame\n", lastimage);

            // check if we are overrunning our max frame rate
            now = millis();
            if(now > lastimage + msecPerFrame)
                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
        }

        if(session->m_stopped) {
            delete session;
            delete streamer;
            session = NULL;
            streamer = NULL;
            printf("%lu Closed RTSP session\n", millis());
        }
    }
    else {
        client = rtspServer.accept();

        if(client) {
            //streamer = new SimStreamer(&client, true);             // our streamer for UDP/TCP based RTP transport
            streamer = new OV2640Streamer(&client, cam);             // our streamer for UDP/TCP based RTP transport

            session = new CRtspSession(&client, streamer); // our threads RTSP session and state
            printf("%lu Started RTSP session\n", millis());
        }
    }
#endif
}
