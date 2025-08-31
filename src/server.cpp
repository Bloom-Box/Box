#include "../include/ntcp/main.h"

Ntcp serverNtcp;

void logLine(const String &s) { Serial.println(s); }

void onAppReceive(const uint8_t *data, uint16_t len, const uint8_t *mac, uint8_t flags)
{
    Serial.print("Rx len=");
    Serial.print(len);
    Serial.print(" from ");
    for (int i = 0; i < 6; ++i)
    {
        Serial.print(mac[i], HEX);
        if (i < 5)
            Serial.print(":");
    }
    Serial.print(" flags=0x");
    Serial.println(flags, HEX);
    Serial.write(data, len);
    Serial.println();
    serverNtcp.send(data, len, mac);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    if (!serverNtcp.begin(logLine))
    {
        Serial.println("ntcp begin fail");
        while (true)
            delay(100);
    }
    serverNtcp.onReceive(onAppReceive);
    serverNtcp.listen();
    Serial.println("NTCP server listening");
}
void loop()
{
    serverNtcp.heartbeat();
    delay(10);
}