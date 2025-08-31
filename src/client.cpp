#include "../include/ntcp/main.h"

uint8_t serverMac[6] = {0x24, 0x6F, 0x28, 0x65, 0x43, 0x21};
Ntcp server;

void log(const String &s) { Serial.println(s); }

void onAppReceive(const uint8_t *data, uint16_t len, const uint8_t *mac, uint8_t flags)
{
    Serial.print("Server reply: ");
    Serial.write(data, len);
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    WiFi.mode(WIFI_STA);
    if (!server.begin(log))
    {
        Serial.println("ntcp begin fail");
        while (true)
            delay(100);
    }
    if (!server.connect(serverMac))
    {
        Serial.println("connect fail");
        while (true)
            delay(100);
    }
    server.onReceive(onAppReceive);
    const char msg[] = "Hello from NTCP client";
    clientNtcp.send(reinterpret_cast<const uint8_t *>(msg), sizeof(msg) - 1, serverMac);
}

void loop()
{

    if (!server.begin(log)) {
        log("Starting nTcp failed!");
        return;
    }

    if (!server.connect(serverMac)) {
        log("Server connection failed!");
        return;
    }


    server.heartbeat();

    delay(1000);
    
}