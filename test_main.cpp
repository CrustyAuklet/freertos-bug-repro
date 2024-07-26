#include <snitch/snitch_cli.hpp>
#include <snitch/snitch_registry.hpp>
#include <array>
#include <iostream>
#include <format>
#include <FreeRTOS.h>
#include <task.h>

namespace {

    constexpr size_t test_stack_size = std::max(configMINIMAL_STACK_SIZE, 4096);
    const auto posix_minimal_stack = PTHREAD_STACK_MIN;
    StaticTask_t main_tcb;
    std::array<StackType_t, test_stack_size> main_stack;

    struct main_state {
        snitch::cli::input args;
        bool result = false;
    };

} // namespace

SNITCH_EXPORT int main(int argc, char* argv[]) {
    std::optional<snitch::cli::input> args = snitch::cli::parse_arguments(argc, argv);
    if (!args) {
        return 1;
    }
    main_state state{.args = *args, .result = false};
    snitch::tests.configure(state.args);

    xTaskCreateStatic(
        [](void* ctx) {
            auto* s   = reinterpret_cast<main_state*>(ctx);
            s->result = snitch::tests.run_tests(s->args);
            for(;;) {
                vTaskEndScheduler();
            }
        },
        "main",
        main_stack.size(),
        &state,
        configMAX_PRIORITIES - 1U,
        main_stack.data(),
        &main_tcb);

    vTaskStartScheduler();
    return state.result ? 0 : 1;
}

extern "C" {
void vAssertCalled(const char* file, int line) {
    auto err = std::format("\n[CRITICAL] OS Assert called: {}, line {}\n", file, line);
#if __cpp_exceptions
    throw std::runtime_error(std::move(err));
#else
    std::cerr << err;
    std::terminate();
#endif
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    std::cerr << std::format("\n[CRITICAL] Stack overflow in {}\n", pcTaskName == nullptr ? "unknown" : pcTaskName);
    std::terminate();
}

}  // extern "C"
