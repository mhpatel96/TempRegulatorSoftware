/*
 * logger.h
 *
 *  Created on: Oct 9, 2024
 *      Author: mhamz
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#include <stdint.h>

#include "cmsis_os2.h"

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

#include "usbd_cdc_if.h"

#define LOGGER			Logger::Instance()

#define ENABLE_LOGGING

//#define LOG_TO_USB	/* Untested!! */
#define LOG_TO_UART
//#define LOG_TO_PRINTF	/* Untested!! */

#if defined(__cplusplus)
/**
 * @class	Logger type
 * @brief	Handles storing/flushing log messages
 */
class LoggerModule
{
public:
	// Constructors/destructors
	LoggerModule(const char * ModuleName);
	~LoggerModule();

	/**
	 * @brief Gets module name as C-style string pointer
	 */
	const char * GetModuleName(void) const;

	static constexpr size_t s_MaxModuleNameLength = 8;

private:
	char m_ModuleName[s_MaxModuleNameLength + 1];
};

/**
 * @class	Logger type
 * @brief	Handles storing/flushing log messages
 */
class Logger
{
public:

	/**
	 * @brief Gets singleton instance
	 */
	static Logger& Instance(void);

	/**
	 * @brief Prints a formatted log string
	 * @param pModule	pointer to logger module
	 */
	bool LogF(const LoggerModule *const pModule, const char *Format, ...);

	/**
	 * @brief Flushes contents of log buffer
	 */
	void Flush(void);

	/**
	 * @brief UART Tx complete interrupt callback
	 */
	void TransmitComplete(void);

	// RTOS task handle
	osThreadId_t m_TaskHandle;

private:

	// Constructors/destructors
	Logger(void);
	~Logger();

	// Constants
	static constexpr size_t s_MaxMessageLength = 64;
	static constexpr size_t s_QueueSize = 8;

	static constexpr char s_NewLine[] = "\r\n";

	// Queue/queue position pointers
	char m_Buff[2][s_QueueSize][s_MaxMessageLength + 2];
	uint8_t m_CurrentBuff;
	uint8_t m_BuffPos;

	// UART driver handle
	UART_HandleTypeDef *m_UART;

	// Prevent singleton clones
	Logger(const Logger&) = delete;
	void operator=(const Logger&) = delete;
};

#endif /* __cplusplus */

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Logger FreeRTOS task
 */
void Logger_Task(void *pvParamaters);

/**
 * @brief Initialises logger task
 */
void Logger_Init(void);

#if defined(__cplusplus)
}
#endif

#endif /* INC_LOGGER_H_ */
