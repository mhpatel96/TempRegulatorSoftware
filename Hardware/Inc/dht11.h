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

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#ifdef __cplusplus
class DHT11
{
public:
	DHT11(GPIO_TypeDef *Port, uint8_t Pin);
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
	bool ReadBlocking(uint8_t *pRxBuff);

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
	 * @brief	Parses a response from the DHT11
	 * @param	pReadBuff	Pointer to raw read response
	 * @param	pResult		Pointer to result buffer to populate
	 */
	void ParseResponse(uint32_t *pReadBuff, uint32_t *pResult);

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

	static DHT11 *s_ActiveReader;

	static constexpr size_t s_QueueLength = 8;
	static QueueHandle_t s_Queue;

private:
	enum State m_State;

	// Buffer for non-blocking reads
	static constexpr size_t s_ReadBufferSizeBytes = 40;
	uint32_t m_ReadBuff[s_ReadBufferSizeBytes];
	uint8_t m_ReadBuffPos;

	// Non-blocking read callback
	void  (*m_Callback)(uint32_t *);

	// GPIO information
	GPIO_TypeDef *m_Port;
	uint8_t m_Pin;

	//Timing requirements
	static constexpr uint32_t s_StartConditionTimeMs = 18;
};
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif
void DHT11_Update(void);
/**
 * @brief	Handles pin interrupt during a non-blocking read
 */
void DHT11_InterruptHandler(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HARDWARE_INC_DHT11_H_ */
