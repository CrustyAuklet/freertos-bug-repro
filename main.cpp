#include <FreeRTOS.h>
#include <array>
#include <task.h>
#include <atomic>
#include <iostream>
#include <format>

namespace {

    constexpr size_t test_stack_size = std::max(configMINIMAL_STACK_SIZE, 4096);
#if __unix
    const auto posix_minimal_stack = PTHREAD_STACK_MIN;
#endif

    struct TaskState {
        int argc;
        char** argv;
        bool result;
    };

}  // namespace

bool test_function(int argc, char* argv[]);

int main(int argc, char* argv[]) {
    TaskState state{argc, argv, false};

    TaskHandle_t xHandle = nullptr;
    const auto result    = xTaskCreate(
        [](void* ctx) {
            for (;;) {
                auto* s   = reinterpret_cast<TaskState*>(ctx);
                s->result = test_function(s->argc, s->argv);
                vTaskEndScheduler();
            }
        },
        "test",
        test_stack_size,
        &state,
        configTIMER_TASK_PRIORITY - 1U,
        &xHandle);
    configASSERT(result == pdTRUE);
    vTaskStartScheduler();
    std::cout << std::format("Result: {}\n", state.result);
    return state.result ? 0 : 1;
}

bool test_function(int argc, char* argv[]) {
    return argc > 1;
}

extern "C" {
void vAssertCalled(const char* file, int line) {
    std::cerr << std::format("\n[CRITICAL] OS Assert called: {}, line {}\n", file, line);
    std::terminate();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    std::cerr << std::format("\n[CRITICAL] Stack overflow in {}\n", pcTaskName == nullptr ? "unknown" : pcTaskName);
    std::terminate();
}

#if configSUPPORT_STATIC_ALLOCATION

void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t** ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE* pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
#endif  // configSUPPORT_STATIC_ALLOCATION
#if configUSE_TIMERS

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t** ppxTimerTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE* pulTimerTaskStackSize) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

#endif  // configUSE_TIMERS

}  // extern "C"
