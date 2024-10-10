/*
 * logger.cpp
 *
 *  Created on: Oct 9, 2024
 *      Author: mhamz
 */

#include "logger.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

extern UART_HandleTypeDef huart1;

LoggerModule::LoggerModule(const char * ModuleName)
{
	strncpy(m_ModuleName, ModuleName, s_MaxModuleNameLength);

	m_ModuleName[s_MaxModuleNameLength] = 0;

	size_t Idx = strlen(ModuleName);
	m_ModuleName[Idx++] = ':';
	while (Idx < s_MaxModuleNameLength)
	{
		m_ModuleName[Idx++] = ' ';
	}

	asm("NOP");
}

LoggerModule::~LoggerModule()
{
}

const char * LoggerModule::GetModuleName(void) const
{
	return m_ModuleName;
}

Logger& Logger::Instance(void)
{
	static Logger Instance;
	return Instance;
}

Logger::Logger(void) :
	m_CurrentBuff(0),
	m_BuffPos(0),
	m_UART(&huart1)
{
}

Logger::~Logger()
{
}

bool Logger::LogF(const LoggerModule *const pModule, const char *Format, ...)
{
	bool ret = true;

	if (m_BuffPos < s_QueueSize)
	{
		strcpy(&m_Buff[m_CurrentBuff][m_BuffPos][0], pModule->GetModuleName());

		va_list Args;
		va_start(Args, Format);
		vsnprintf(&m_Buff[m_CurrentBuff][m_BuffPos][LoggerModule::s_MaxModuleNameLength], s_MaxMessageLength, Format, Args);
		va_end(Args);

		strcat(&m_Buff[m_CurrentBuff][m_BuffPos][0], s_NewLine);

		m_BuffPos++;
	}
	else
	{
		ret = false;
	}

	return ret;
}

void Logger::Flush(void)
{
	uint8_t Idx = 0;
	uint8_t BuffToFlush = m_CurrentBuff;
	m_CurrentBuff = !m_CurrentBuff;

	while (Idx < m_BuffPos)
	{
#if defined(LOG_TO_USB)
		CDC_Transmit_FS((uint8_t*)&m_Buff[BuffToFlush][Idx][0], strlen(&m_Buff[BuffToFlush][Idx][0]));
		m_Transmitting = true;
#elif defined(LOG_TO_UART)
		while (HAL_UART_Transmit_IT(m_UART, (uint8_t*)&m_Buff[BuffToFlush][Idx][0], strlen(&m_Buff[BuffToFlush][Idx][0])) != HAL_OK)
		{
			taskYIELD();
		}
#elif defined(LOG_TO_PRINTF)
		printf(&m_Buff[BuffToFlush][Idx][0]);
#endif

		Idx++;
	}

	m_BuffPos = 0;
}

void Logger_Task(void *pvParamaters)
{
	(void) pvParamaters;

	while (1)
	{
		osDelay(10);
		LOGGER.Flush();
	}
}

void Logger_Init(void)
{
	const osThreadAttr_t TaskAttributes = {
		.name = "Logger_Task",
		.stack_size = 128 * 4,
		.priority = (osPriority_t) osPriorityNormal,
	};

	LOGGER.m_TaskHandle = osThreadNew(Logger_Task, nullptr, &TaskAttributes);
}
