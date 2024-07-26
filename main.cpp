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
    TaskHandle_t h = nullptr;
    const auto r = xTaskCreate(
        [](void*) {
            for(unsigned i = 0;; ++i) {
                std::cerr << std::format("TEST1: Iteration {}\n", i);
                vTaskDelay(100);
            }
        },
        "test1",
        test_stack_size,
        nullptr,
        1,
        &h);
    configASSERT(r == pdTRUE);
    vTaskDelay(500);
    vTaskDelete(h);
    return true;
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

}  // extern "C"
