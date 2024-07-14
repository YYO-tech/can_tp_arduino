#include <mcp_can.h>
#include <SPI.h>

#define CAN0_INT 9
MCP_CAN CAN0(10);

#define CAN_ID_TESTER_TO_ALL 0x7DF
#define CAN_ID_TESTER_TO_ECU 0x7E0
#define CAN_ID_ECU_TO_TESTER 0x7E8

#define EXT_CAN_ID 1
#define STD_CAN_ID 0

#define LENGTH_CAN_FRAME 8

void SendCan(uint8_t *data, uint8_t length, bool extended, uint32_t id)
{
    uint8_t sndStat = CAN0.sendMsgBuf(id, (extended == true) ? EXT_CAN_ID : STD_CAN_ID, LENGTH_CAN_FRAME, data);
    if (sndStat != CAN_OK)
    {
        Serial.println("Error Sending Message...");
    }
}

void SendDiagCan(uint8_t *data)
{
    SendCan(data, LENGTH_CAN_FRAME, false, CAN_ID_ECU_TO_TESTER);
}

void CompleteRxTP(uint8_t *message, uint8_t length)
{
    UdsResponse(message, length);
}

void setup()
{
    Serial.begin(9600);

    // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
    if (CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    { //< -------- - CHANGE if using different board
        Serial.println("MCP2515 Initialized Successfully!");
    }
    else
    {
        Serial.println("Error Initializing MCP2515...");
        while (1)
            ;
    }

    SetStMin(2);

    CAN0.setMode(MCP_NORMAL); // Set operation mode to normal so the MCP2515 sends acks to received data.

    pinMode(CAN0_INT, INPUT); // Configuring pin for /INT input
}

void loop()
{
    long unsigned int rxId;
    uint8_t len = 0;
    uint8_t rxBuf[LENGTH_CAN_FRAME];

    if (!digitalRead(CAN0_INT)) // If CAN0_INT pin is low, read receive buffer
    {
        CAN0.readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

        if (rxId == CAN_ID_TESTER_TO_ECU)
        {
            HandlingRxTP(rxBuf);
        }
    }
}

void CompleteRxTP(uint8_t *message, uint8_t length)
{
    switch (message[0])
    {
    case UDS_SID_10_DSC: // start Diagnostic Session
    {
        uint8_t TxBytes[2] = {UDS_SID_10_DSC + POSITIVE_VALUE, 0x81};
        TxBytes[1] = message[1];

        SendCanTP(TxBytes, 2);
    }
    break;
    }
}
