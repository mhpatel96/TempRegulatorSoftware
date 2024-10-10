
#include "app.h"

#include "logger.h"

#include "dht11.h"

osThreadId_t TestThreadHandle;

void Test_Task(void *pvParamaters)
{
	(void) pvParamaters;

	static LoggerModule TestLoggerModule("Test");
	static int Count = 0;
	LOGGER.LogF(&TestLoggerModule, "Started test task");

	static DHT11 DHT11Test(GPIOC, 0, EXTI0_IRQn);
	static uint32_t DHT11RxBuff[8] = {0};

	while (1)
	{
		osDelay(1000);
		LOGGER.LogF(&TestLoggerModule, "Kushal %d", Count++);
		DHT11Test.ReadBlocking(DHT11RxBuff);
	}
}

extern "C" {
void App_Init(void)
{
	Logger_Init();

	const osThreadAttr_t TaskAttributes =
	{
		.name = "Test_Task",
		.stack_size = 128 * 4,
		.priority = (osPriority_t) osPriorityNormal,
	};

	TestThreadHandle = osThreadNew(Test_Task, nullptr, &TaskAttributes);
}
}
