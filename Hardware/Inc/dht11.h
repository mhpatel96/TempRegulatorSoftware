/*
 * dht.h
 *
 *  Created on: Sep 22, 2024
 *      Author: mhamz
 */

#ifndef HARDWARE_INC_DHT11_H_
#define HARDWARE_INC_DHT11_H_

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"

#include "logger.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define DHT11_LOGGING

#if defined (DHT11_LOGGING)
#define LOGF(...)		LOGGER.LogF(this, __VA_ARGS__)
#else
#define LOGF(...)
#endif

#if defined(__cplusplus)
/**
 * @class	DHT11
 * @brief	DHT11 driver type
 */
class DHT11 : private LoggerModule
{
public:
	/**
	 * @brief	Constructor
	 * @param	Port		Pointer to port register block
	 * @param	Pin			Pin number
	 * @param	Interrupt	Interrupt channel number
	 */
	DHT11(GPIO_TypeDef *pPort, uint32_t Pin, IRQn_Type Interrupt);

	/**
	 * @brief	Destructor
	 */
	~DHT11();

	enum class State
	{
		Idle,
		Waiting,
		Reading,
		ReadComplete
	};

	/**
	 * @brief	Reads full data packet from DHT11
	 * @param	pRxBuff	Pointer to read buffer
	 * @retval	true	Response is valid
	 */
	bool ReadBlocking(uint32_t *pRxBuff);

	/**
	 * @brief	Starts a non-blocking read
	 * @param	Callback	Function
	 * @retval	true		Read started
	 * @retval	false		Driver busy, read not started
	 */
	bool ReadNonBlocking(void (*Callback)(uint32_t *pReadBuff));

	/**
	 * @brief	Handles pin interrupt during a non-blocking read
	 * @param	Time		Time of interrupt occurrence
	 */
	void HandlePinInterrupt(uint32_t Time);

	/**
	 * @brief	Parses a response from the DHT11 and passes to callback
	 */
	void TransmissionComplete(void);

	/**
	 * @brief	Parses a response from the DHT11 and passes to callback
	 */
	void TransmissionComplete(uint32_t *pRxBuff);

	/**
	 * @brief	Parses a response from the DHT11
	 * @param	pReadBuff	Pointer to raw read response
	 * @param	pResult		Pointer to result buffer to populate
	 */
	enum State GetState(void);

	/**
	 * @brief	Starts DHT11 transmission
	 */
	void StartTransmission(void);

	/**
	 * @brief	Resets DHT11 driver
	 */
	void Reset(void);

	// Current active reader and read queue
	static DHT11 *s_ActiveReader;
	static constexpr size_t s_QueueLength = 8;
	static QueueHandle_t s_Queue;

private:
	enum State m_State;

	// Buffer for non-blocking reads
	static constexpr size_t s_ReadBufferSize = 79;
	uint32_t m_ReadBuff[s_ReadBufferSize];
	uint8_t m_ReadBuffPos;

	// Non-blocking read callback
	void  (*m_Callback)(uint32_t *);

	// GPIO information
	GPIO_TypeDef *const m_Port;
	const uint32_t m_Pin;
	const IRQn_Type m_InterruptChannel;

	// Timing requirements
	// Micro pulls line high for at least 18ms to start transmission
	static constexpr uint32_t s_StartConditionTimeInitialUs = 19000;

	// Then pull line high for 20-40us
	static constexpr uint32_t s_StartConditionTimeSecondaryLowUs = 20;
	static constexpr uint32_t s_StartConditionTimeSecondaryHighUs = 40;

	// DHT then pulls line low for 80us, than high for 80us
	static constexpr uint32_t s_StartConditionTimeTertiaryUs = 80;

	// Error margins
	static constexpr uint32_t s_ErrorMarginTimeUs = 4;

	// Each bit is prefixed with a 50us low
	static constexpr uint32_t s_BitStartTimeLowUs = 50 - s_ErrorMarginTimeUs;
	static constexpr uint32_t s_BitStartTimeHighUs = 50 + s_ErrorMarginTimeUs;

	// Zero is defined as 26-28 us
	static constexpr uint32_t s_ZeroTimeLowUs = 26 - s_ErrorMarginTimeUs;
	static constexpr uint32_t s_ZeroTimeHighUs = 28 + s_ErrorMarginTimeUs;

	// One is defined as 70 us
	static constexpr uint32_t s_OneTimeLowUs = 64 - s_ErrorMarginTimeUs;
	static constexpr uint32_t s_OneTimeHighUs = 76 + s_ErrorMarginTimeUs;

	// DHT11 pulls line high for 50us to end transmission
	static constexpr uint32_t s_EndConditionTimeUs = 50;

	/**
	 * @brief	Parses a response from the DHT11 and populates buffer
	 * @param	pRxBuff		Pointer to buffer to populate
	 * @retval	true		Response is valid
	 */
	bool ParseResponse(uint32_t *pRxBuff);
};
#endif /* __cplusplus */

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief	Updates DHT11 driver
 */
void DHT11_Update(void);

/**
 * @brief	Handles pin interrupt during a non-blocking read
 */
void DHT11_InterruptHandler(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* HARDWARE_INC_DHT11_H_ */
