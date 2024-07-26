#include <seal/thread.hpp>


#if __cpp_exceptions
#include <format>
#include <iostream>
#if defined(__linux__)
#include <cxxabi.h>
#endif // __linux__
#endif // __cpp_exceptions

namespace seal {

#if (INCLUDE_vTaskDelete == 1)
    Thread::~Thread() noexcept(detail::noexcept_dtor) {
        // kill task for now, but we should send stop signal and then join
        // larid_Assert(!joinable());
        (void)this->kill();
    }
#endif

    void detail::ThreadState::run_thread(void* ctx) {
        seal::detail::ThreadState* state = reinterpret_cast<seal::detail::ThreadState*>(ctx);
#if __cpp_exceptions
        try {
#endif
            state->fun();
#if __cpp_exceptions
        }
//        catch (const abi::__forced_unwind& err) {
//            // this is the exception that gcc uses for pthread cancellation, must re-throw
//            throw;
//        }
        catch (const std::exception& err) {
            std::cerr << std::format("[{}] Exception was thrown \"{}\"\n", pcTaskGetName(NULL), err.what());
        }
//        catch (...) {
//            std::cerr << std::format("[{}] Unknown exception was thrown, terminating\n", pcTaskGetName(NULL));
//        }
#endif
        // thread is still attached to the thread class, cleanup will happen in join / destructor
        vTaskSuspend(NULL);
    }

    void Thread::start_thread(const char* name) {
        configASSERT(state_);
        auto* s = state_.get();

        s->alive.test_and_set();
        auto* h = xTaskCreateStatic(detail::ThreadState::run_thread,
                                    name,
                                    s->stack.size(),
                                    s,
                                    s->priority,
                                    s->stack.data(),
                                    &s->tcb);
        configASSERT(state_->get_native_handle() == h);
    }

    std::error_code Thread::kill() {
        taskENTER_CRITICAL();
        if (!state_) {
            taskEXIT_CRITICAL();
            return std::make_error_code(std::errc::no_such_process);
        }
        if (!joinable()) {
            taskEXIT_CRITICAL();
            return std::make_error_code(std::errc::invalid_argument);
        }
        if (state_->alive.test()) {
            vTaskDelete(state_->get_native_handle());
            state_->alive.clear();
        }
        taskEXIT_CRITICAL();
        return std::error_code{};
    }

}  // namespace seal
