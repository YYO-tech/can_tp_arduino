#define TP_SF 0x00
#define TP_FF 0x10
#define TP_CF 0x20
#define TP_FCF 0x30

#define PADDING_ECU 0x55
#define PADDING_TESTER 0xAA

#define FF_DATA_OFFSET 2
#define FF_DATA_LENGTH 6

#define CF_DATA_OFFSET 1
#define CF_DATA_LENGTH 7

#define SF_DATA_OFFSET 1
#define SF_DATA_LENGTH 7

#define LENGTH_CAN_TP_BUFFER 255 // Max Length of buffer for CAN TP
#define LENGTH_CAN_FRAME 8

#define CAN_TP_RX_BS 8

uint8_t RxStMin = 5;

uint8_t CanTpRx[LENGTH_CAN_TP_BUFFER];
uint8_t CanTpRxBufferIndex;
uint16_t CanTpRxLength;

uint8_t CanTpTx[LENGTH_CAN_TP_BUFFER];
uint8_t CanTpTxBufferIndex;
uint16_t CanTpTxLength;

uint8_t CanTpTxBlockSize;
uint8_t CanTpTxBlockCount;

uint8_t CanTpRxBlockCount;
bool isRxJobIdle = true;

//----------------------------------------------------------------
// void CompleteRxTP(uint8_t *message, uint8_t length)
// this function should be defined where it use
//----------------------------------------------------------------

void SetStMin(uint8_t value)
{
	RxStMin = value;
}

void SendSF()
{
	uint8_t TxBytes[LENGTH_CAN_FRAME] = {PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU};

	TxBytes[0] = TP_SF | CanTpTxLength;
	memcpy(&TxBytes[SF_DATA_OFFSET], &CanTpTx[CanTpTxBufferIndex], CanTpTxLength);
	SendDiagCan(TxBytes);
}

void SendFF()
{
	uint8_t TxBytes[LENGTH_CAN_FRAME];

	TxBytes[0] = TP_FF;
	TxBytes[1] = CanTpTxLength;
	memcpy(&TxBytes[FF_DATA_OFFSET], &CanTpTx[CanTpTxBufferIndex], FF_DATA_LENGTH);
	SendDiagCan(TxBytes);

	CanTpTxBufferIndex += FF_DATA_LENGTH;
	CanTpTxBlockCount++;
}

void SendCFs()
{
	for (int Index = 0; Index < CanTpTxBlockSize; Index++)
	{
		uint8_t TxBytes[LENGTH_CAN_FRAME] = {PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU, PADDING_ECU};

		TxBytes[0] = TP_CF | CanTpTxBlockCount;
		memcpy(&TxBytes[CF_DATA_OFFSET], &CanTpTx[CanTpTxBufferIndex], CF_DATA_LENGTH);
		SendDiagCan(TxBytes);

		CanTpTxBufferIndex += CF_DATA_LENGTH;
		CanTpTxBlockCount++;
		CanTpTxBlockCount = CanTpTxBlockCount & 0x0F;

		if (CanTpTxBufferIndex >= CanTpTxLength)
		{
			CanTpTxLength = 0;
			break;
		}
	}
}

void SendFCF()
{
	// ------------------------------------- FCF   BS            ST
	uint8_t TxBytes[LENGTH_CAN_FRAME] = {0x30, CAN_TP_RX_BS, 0x05, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
	TxBytes[2] = RxStMin;
	SendDiagCan(TxBytes);
	CanTpRxBlockCount = 0;
}

void SendCanTP(uint8_t *message, uint16_t length)
{
	isRxJobIdle = true;
	if (length > LENGTH_CAN_TP_BUFFER)
	{
		Serial.println("TX TP Buffer is not frepared");
	}
	else
	{
		memcpy(CanTpTx, message, length);
		CanTpTxLength = length;
		CanTpTxBufferIndex = 0;
		CanTpTxBlockCount = 0;

		if (length <= SF_DATA_LENGTH)
		{
			SendSF();
		}
		else
		{
			SendFF();
		}
	}
}

void HandlingRxTP(const uint8_t *CanByte)
{
	if ((CanByte[0] & 0xF0) == TP_SF) // SF
	{
		memset(CanTpRx, 0, LENGTH_CAN_TP_BUFFER);

		CanTpRxLength = CanByte[0] & 0x0F;
		memcpy(CanTpRx, &CanByte[1], CanTpRxLength);

		if (isRxJobIdle == false)
		{
			Serial.println("Rx TP Error with SF");
		}

		isRxJobIdle = true;

		CompleteRxTP(CanTpRx, CanTpRxLength);
	}
	else if ((CanByte[0] & 0xF0) == TP_FF) // FF
	{
		memset(CanTpRx, 0, LENGTH_CAN_TP_BUFFER);

		CanTpRxLength = CanByte[0] & 0x0F;
		CanTpRxLength *= 0x100;
		CanTpRxLength += CanByte[1];

		if (isRxJobIdle == false)
		{
			Serial.println("Rx TP Error with FF");
		}

		isRxJobIdle = false;

		if (CanTpRxLength > LENGTH_CAN_TP_BUFFER)
		{
			Serial.println("RX TP Buffer is not frepared");
		}

		CanTpRxBufferIndex = 0;
		memcpy(&CanTpRx[CanTpRxBufferIndex], &CanByte[FF_DATA_OFFSET], FF_DATA_LENGTH);
		CanTpRxBufferIndex += FF_DATA_LENGTH;

		SendFCF();
	}
	else if ((CanByte[0] & 0xF0) == TP_CF) // CF
	{
		memcpy(&CanTpRx[CanTpRxBufferIndex], &CanByte[CF_DATA_OFFSET], CF_DATA_LENGTH);
		CanTpRxBufferIndex += CF_DATA_LENGTH;

		CanTpRxBlockCount++;
		if (CanTpRxBufferIndex >= CanTpRxLength)
		{
			CompleteRxTP(CanTpRx, CanTpRxLength);
			isRxJobIdle = true;
		}
		else
		{
			if (CanTpRxBlockCount >= CAN_TP_RX_BS)
			{
				CanTpRxBlockCount = 0;
				SendFCF();
			}
		}
	}
	else if ((CanByte[0] & 0xF0) == TP_FCF) // FCF
	{
		// Send All Can TP
		CanTpTxBlockSize = CanByte[1];
		SendCFs();
	}
}

bool isRxJobDone()
{
	return isRxJobIdle;
}
