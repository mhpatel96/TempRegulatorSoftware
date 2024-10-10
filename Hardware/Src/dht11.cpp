

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

extern TIM_HandleTypeDef htim2;
extern volatile uint32_t TickCountUs;

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

	LOGF("Created Port %#04X Pin %4d Int %4d", (uint32_t)m_Port, m_Pin, m_InterruptChannel);
}

DHT11::~DHT11()
{
	LOGF("Destroyed Port %#04X Pin %4d Int %4d", (uint32_t)m_Port, m_Pin, m_InterruptChannel);
}

bool DHT11::ReadBlocking(uint32_t *pRxBuff)
{
	LOGF("Blocking read %#04X Pin %4d Int %4d", (uint32_t)m_Port, m_Pin, m_InterruptChannel);

	m_ReadBuffPos = 0;

	RESET_PIN(m_Port, m_Pin, m_InterruptChannel);

	// Send start condition
	PIN_LOW(m_Port, m_Pin);
	osDelay(pdMS_TO_TICKS(s_StartConditionTimeInitialUs/1000));

	// Pull line high and wait for response
	PIN_HIGH(m_Port, m_Pin);
	uint32_t StartTime = TickCountUs;
	while ((TickCountUs - StartTime) <= s_StartConditionTimeSecondaryLowUs) {}

	PIN_INPUT(m_Port, m_Pin);
	while (!PIN_READ(m_Port, m_Pin)) {}
	while (PIN_READ(m_Port, m_Pin)) {}

	bool PinState = false, PinStateOld = false;

	while (m_ReadBuffPos < s_ReadBufferSize)
	{
		PinState = PIN_READ(m_Port, m_Pin);

		if (PinState != PinStateOld)
		{
			m_ReadBuff[m_ReadBuffPos++] = TickCountUs;
			PinStateOld = PinState;
		}
	}

	TransmissionComplete(pRxBuff);

	return false;
}

bool DHT11::ReadNonBlocking(void (*Callback)(uint32_t *))
{
	LOGF("Non-blocking read %#04X Pin %4d Int %4d", (uint32_t)m_Port, m_Pin, m_InterruptChannel);

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

void DHT11::TransmissionComplete(uint32_t *pRxBuff)
{
	ParseResponse(pRxBuff);
}

void DHT11::TransmissionComplete(void)
{
	uint32_t ParsedResponse[4] = {0};
	ParseResponse(ParsedResponse);
	m_Callback(ParsedResponse);
}

bool DHT11::ParseResponse(uint32_t *pRxBuff)
{
	uint32_t ErrorCount = 0, Idx = 0;

	while (Idx < s_ReadBufferSize)
	{
//		m_ReadBuff;
	}

	LOGF("Response %4d %4d %4d %4d Errors %d", pRxBuff[0], pRxBuff[1], pRxBuff[2], pRxBuff[3], ErrorCount);

	return (ErrorCount == 0);
}

void DHT11::StartTransmission(void)
{
	LOGF("Starting transmission");
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
	LOGF("Reset");
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
		DHT11::s_ActiveReader->HandlePinInterrupt(TickCountUs);
	}
}

}
