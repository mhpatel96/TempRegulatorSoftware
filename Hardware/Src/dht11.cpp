

#include "dht11.h"

#define PIN_INPUT(Port, Pin)	\
	{\
		volatile uint32_t *Reg = Pin < 8 ? Port->CRL : Port->CRH;	\
		uint32_t Pos =  Pin < 8 ? Pin : Pin - 8;	\
		Reg &= (~(0x0F << Pos));	\
		Reg |= 0x04 << Pos;	\
	}
#define PIN_OUTPUT(Port, Pin)	\
	{\
		volatile uint32_t *Reg = Pin < 8 ? Port->CRL : Port->CRH;	\
		uint32_t Pos =  Pin < 8 ? Pin : Pin - 8;	\
		Reg &= (~(0x0F << Pos));	\
		Reg |= 0x01 << Pos;	\
	}

#define PIN_HIGH(Port, Pin)	(Port->ODR |= (1 << Pin))
#define PIN_LOW(Port, Pin)	(Port->ODR &= ~(1 << Pin))

#define PIN_READ(Port, Pin)	(Port->IDR & (1 << Pin))

extern TIM_HandleTypeDef htim2;

DHT11 *DHT11::s_ActiveReader = nullptr;
QueueHandle_t DHT11::s_Queue = nullptr;

DHT11::DHT11(GPIO_TypeDef *Port, uint8_t Pin) :
	m_State(State::Idle),
	m_ReadBuffPos(0),
	m_Callback(nullptr),
	m_Port(Port),
	m_Pin(Pin)
{
	if (s_Queue == nullptr)
	{
		s_Queue = xQueueCreate(s_QueueLength, sizeof(DHT11*));
	}
}

DHT11::~DHT11()
{
}


bool DHT11::ReadBlocking(uint8_t *pRxBuff)
{
	return false;
}

bool DHT11::ReadNonBlocking(void (*Callback)(uint32_t *))
{
	bool ret = xQueueSendToBack(s_Queue, this, 10);

	m_Callback = Callback;

	return ret;
}

void DHT11::HandlePinInterrupt(uint32_t Time)
{
	if (m_State == State::Reading)
	{

		if (m_ReadBuffPos < s_ReadBufferSizeBytes)
		{
			m_ReadBuff[m_ReadBuffPos++] = Time;
		}
		else
		{
			m_State = State::ReadComplete;
		}
	}
}

void DHT11::ParseResponse(uint32_t *pReadBuff, uint32_t *pResult)
{

}

void DHT11::StartTransmission(void)
{
//	PIN_HIGH(m_Port, m_Pin);
//	vTaskDelay(pdMS_TO_TICKS(s_StartConditionTimeMs));
//	HAL_GPIO_WritePin(m_Port, m_Pin, GPIO_PIN_RESET);
}

void DHT11::Reset(void)
{
	m_Callback = nullptr;
}

extern "C" {

void DHT11_Update(void)
{
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
			DHT11::s_ActiveReader->Reset();
			DHT11::s_ActiveReader = nullptr;
		}
	}
}

void DHT11_InterruptHandler(void)
{
	if (DHT11::s_ActiveReader != nullptr)
	{
		DHT11::s_ActiveReader->HandlePinInterrupt(0);
	}
}
}

