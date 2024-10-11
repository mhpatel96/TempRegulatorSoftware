

#include "dht11.h"

#define PIN_INPUT(Port, Pin)	\
	{\
		volatile uint32_t *Reg = (volatile uint32_t *)(Pin < 8 ? &Port->CRL : &Port->CRH);	\
		uint32_t Pos =  Pin < 8 ? Pin : Pin - 8;	\
		*Reg &= (~(0x0F << Pos));	\
		*Reg |= 0x04 << Pos;	\
	}
#define PIN_OUTPUT(Port, Pin)	\
	{\
		volatile uint32_t *Reg = (volatile uint32_t *)(Pin < 8 ? &Port->CRL : &Port->CRH);	\
		uint32_t Pos =  Pin < 8 ? Pin : Pin - 8;	\
		*Reg &= (~(0x0F << Pos));	\
		*Reg |= 0x01 << Pos;	\
	}

#define PIN_HIGH(Port, Pin)					(Port->ODR |= (1 << Pin))
#define PIN_LOW(Port, Pin)					(Port->ODR &= ~(1 << Pin))

#define PIN_READ(Port, Pin)					(Port->IDR & (1 << Pin))

#define INTERRUPT_ENABLE(Channel)			HAL_NVIC_EnableIRQ(Channel)
#define INTERRUPT_DISABLE(Channel)			HAL_NVIC_DisableIRQ(Channel)

#define RESET_PIN(Port, Pin, Interrupt)	\
	{\
		INTERRUPT_DISABLE(Interrupt);	\
		PIN_OUTPUT(Port, Pin);	\
		PIN_HIGH(Port, Pin);	\
	}

#define TIMER_RESET							DWT->CYCCNT = 0
#define TIMER_CURRENT						DWT->CYCCNT
#define TIMER_TICKS_TO_US(Time)				((Time)/72)
#define TIMER_US_TO_TICKS(Time)				((Time)*72)

extern TIM_HandleTypeDef htim2;

DHT11 *DHT11::s_ActiveReader = nullptr;
QueueHandle_t DHT11::s_Queue = nullptr;

DHT11::DHT11(GPIO_TypeDef *pPort, uint32_t Pin, IRQn_Type Interrupt) :
		LoggerModule("DHT11"),
		m_State(State::Idle),
		m_ReadBuffPos(0),
		m_Callback(nullptr),
		m_Port(pPort),
		m_Pin(Pin),
		m_InterruptChannel(Interrupt)
{
	RESET_PIN(m_Port, m_Pin, m_InterruptChannel);

	// Create queue on first instance
	if (s_Queue == nullptr)
	{
		s_Queue = xQueueCreate(s_QueueLength, sizeof(DHT11*));
	}

	LOGF("%d Created", m_InterruptChannel);
}

DHT11::~DHT11()
{
	LOGF("%d Destroyed", (uint32_t)m_Port, m_Pin, m_InterruptChannel);
}

bool DHT11::ReadBlocking(uint8_t *pRxBuff)
{
	LOGF("%d Blocking read", m_InterruptChannel);

	uint32_t StartTime = 0;

	m_ReadBuffPos = 0;
	TIMER_RESET;

	RESET_PIN(m_Port, m_Pin, m_InterruptChannel);

	// Send start condition
	PIN_LOW(m_Port, m_Pin);
	osDelay(pdMS_TO_TICKS(s_StartConditionTimeInitialUs/1000));

	// Pull line high and wait for response
	PIN_HIGH(m_Port, m_Pin);
	StartTime = TIMER_CURRENT;
	while ((TIMER_CURRENT - StartTime) <= TIMER_US_TO_TICKS(s_StartConditionTimeSecondaryUs)) {}

	PIN_INPUT(m_Port, m_Pin);
	while (!PIN_READ(m_Port, m_Pin)) {}
	while (PIN_READ(m_Port, m_Pin)) {}

	bool PinState = false, PinStateOld = false;

	while (m_ReadBuffPos < s_ReadBufferSize)
	{
		PinState = PIN_READ(m_Port, m_Pin);

		if (PinState != PinStateOld)
		{
			m_ReadBuff[m_ReadBuffPos++] = TIMER_CURRENT;
			PinStateOld = PinState;
		}
	}

	TransmissionComplete(pRxBuff);

	return false;
}

bool DHT11::ReadNonBlocking(void (*Callback)(uint8_t *))
{
	LOGF("%d Non-blocking read", m_InterruptChannel);

	m_Callback = Callback;

	bool ret = xQueueSendToBack(s_Queue, this, 10);
	return ret;
}

void DHT11::HandlePinInterrupt(uint32_t Time)
{
	if (m_State == State::Reading)
	{
		if (m_ReadBuffPos < s_ReadBufferSize)
		{
			m_ReadBuff[m_ReadBuffPos++] = Time;
		}
		else
		{
			m_State = State::ReadComplete;
		}
	}
}

void DHT11::TransmissionComplete(uint8_t *pRxBuff)
{
	ParseResponse(pRxBuff);
}

void DHT11::TransmissionComplete(void)
{
	uint8_t ParsedResponse[5] = {0};
	ParseResponse(ParsedResponse);
	m_Callback(ParsedResponse);
}

bool DHT11::ParseResponse(uint8_t *pRxBuff)
{
	uint32_t ErrorCount = 0, Idx = 0, Time = 0;
	uint64_t Result = 0;

	for (Idx = 0; Idx < s_ReadBufferSize; Idx++)
	{
		m_ReadBuff[Idx] = TIMER_TICKS_TO_US(m_ReadBuff[Idx]);
	}

	for (Idx = 0; Idx < s_ReadBufferSize - 1; Idx++)
	{
		Time = m_ReadBuff[Idx + 1] - m_ReadBuff[Idx];

		if (Idx % 2)
		{
			// Odd index, expect a bit start condition
			if ((Time >= s_RxBitStartTimeHighUs) || (Time <= s_RxBitStartTimeLowUs))
			{
				ErrorCount++;
			}
		}
		else
		{
			// Even index, parse bit
			if ((Time >= s_RxZeroTimeHighUs) || (Time <= s_RxZeroTimeLowUs))
			{
				// Received 0
			}
			else if ((Time >= s_RxOneTimeHighUs) || (Time <= s_RxOneTimeLowUs))
			{
				// Received 1
				Result |= (1 << (s_NumBitsPerTransmission - (Idx / 2)));
			}
			else
			{
				ErrorCount++;
			}
		}
	}

	uint8_t *pTempRxBuff = (uint8_t *)&Result;
	pRxBuff[0] = pTempRxBuff[4];
	pRxBuff[1] = pTempRxBuff[3];
	pRxBuff[2] = pTempRxBuff[2];
	pRxBuff[3] = pTempRxBuff[1];
	pRxBuff[4] = pTempRxBuff[0];

	uint8_t Checksum = pRxBuff[0] + pRxBuff[1] + pRxBuff[2] + pRxBuff[3];

	LOGF("%d Response %4d %4d %4d %4d %4d Check %d Errors %d", m_InterruptChannel,
			pRxBuff[0], pRxBuff[1], pRxBuff[2], pRxBuff[3], pRxBuff[4],
			Checksum, ErrorCount);

	return ((ErrorCount == 0) && (Checksum == pRxBuff[4]));
}

void DHT11::StartTransmission(void)
{
	LOGF("%d Starting transmission", m_InterruptChannel);
	m_ReadBuffPos = 0;
	PIN_LOW(m_Port, m_Pin);
	osDelay(pdMS_TO_TICKS(s_StartConditionTimeInitialUs/1000));
	PIN_INPUT(m_Port, m_Pin);
	INTERRUPT_ENABLE(m_InterruptChannel);
}

void DHT11::Reset(void)
{
	RESET_PIN(m_Port, m_Pin, m_InterruptChannel);
	m_Callback = nullptr;
	m_ReadBuffPos = 0;
	m_State = DHT11::State::Idle;
	LOGF("%d Reset", m_InterruptChannel);
}

extern "C" {

void DHT11_Update(void)
{
	// Check if we are currently in a non-blocking transmission
	// If yes, wait till transmission is complete
	// If no, start next transmission
	if (DHT11::s_ActiveReader == nullptr)
	{
		DHT11 *NextTransmission;
		bool TransmissionPending = xQueueReceive(DHT11::s_Queue, &NextTransmission, 10);

		if (TransmissionPending)
		{
			DHT11::s_ActiveReader = NextTransmission;
			DHT11::s_ActiveReader->StartTransmission();
		}
	}
	else
	{
		DHT11::State State = DHT11::s_ActiveReader->GetState();

		if (State == DHT11::State::ReadComplete)
		{
			DHT11::s_ActiveReader->TransmissionComplete();
			DHT11::s_ActiveReader->Reset();
			DHT11::s_ActiveReader = nullptr;
		}
	}
}

void DHT11_InterruptHandler(void)
{
	if (DHT11::s_ActiveReader != nullptr)
	{
		DHT11::s_ActiveReader->HandlePinInterrupt(TIMER_CURRENT);
	}
}

}
