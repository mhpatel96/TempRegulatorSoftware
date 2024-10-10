
#include "app.h"

#include "logger.h"

osThreadId_t TestThreadHandle;

void Test_Task(void *pvParamaters)
{
	(void) pvParamaters;

	static LoggerModule TestLoggerModule("Test");
	LOGGER.LogF(&TestLoggerModule, "Started test task");

	while (1)
	{
		osDelay(10);
	}
}

void Test_Init(void)
{
	const osThreadAttr_t TaskAttributes = {
		.name = "Test_Task",
		.stack_size = 128 * 4,
		.priority = (osPriority_t) osPriorityNormal,
	};

	TestThreadHandle = osThreadNew(Test_Task, nullptr, &TaskAttributes);
}

extern "C" {
void App_Init(void)
{
	Logger_Init();
	Test_Init();
}
}
