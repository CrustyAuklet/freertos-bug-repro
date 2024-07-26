#pragma once
#include <thread>
#include <array>
#include <span>
#include <system_error>
#include <concepts>
#include <cstring>
#include <larid/memory.hpp>
#include <FreeRTOS.h>
#include <task.h>

namespace seal {

    class Thread;

    namespace detail {
#if __linux__
        constexpr bool noexcept_dtor = __linux__;
#else
        constexpr bool noexcept_dtor = __linux__;
#endif

        struct ThreadState {
            constexpr ThreadState()                    = default;
            ThreadState(const ThreadState&)            = delete;
            ThreadState& operator=(const ThreadState&) = delete;
            ThreadState(ThreadState&&)                 = delete;
            ThreadState& operator=(ThreadState&&)      = delete;
            ~ThreadState()                             = default;

        private:
            friend Thread;
            static void run_thread(void* ctx);

            TaskHandle_t get_native_handle() {
                return alive.test() ? reinterpret_cast<TaskHandle_t>(&tcb) : nullptr;
            }

            StaticTask_t tcb{};                   // FreeRTOS Task Control Block
            std::span<StackType_t> stack;         // reference to the stack memory
            larid::inplace_function<void()> fun;  // the thread function
            std::atomic_flag alive;               // indicates the thread is running
            uint16_t priority = 1;                // task priority
        };

        template<size_t StackSize = configMINIMAL_STACK_SIZE, size_t FuncSize = sizeof(void*)>
        class InplaceThreadState {
        public:
            constexpr InplaceThreadState()                           = default;
            InplaceThreadState(const InplaceThreadState&)            = delete;
            InplaceThreadState& operator=(const InplaceThreadState&) = delete;
            InplaceThreadState(InplaceThreadState&&)                 = delete;
            InplaceThreadState& operator=(InplaceThreadState&&)      = delete;
            ~InplaceThreadState()                                    = default;

        private:
            friend Thread;

#if defined(configCHECK_FOR_STACK_OVERFLOW) && configCHECK_FOR_STACK_OVERFLOW == 2
            static constexpr size_t stack_size = StackSize + (16 / sizeof(StackType_t));
#else
            static constexpr size_t stack_size = StackSize;
#endif
            ThreadState state_;
            larid::inplace_function<void(), FuncSize> func_storage_;  // storage for thread function
            std::array<StackType_t, stack_size> stack_;               // storage area for the stack
        };

    }  // namespace detail

    class Thread {
    public:
        using native_handle_type              = TaskHandle_t;
        static constexpr int default_priority = tskIDLE_PRIORITY;

        class id {
        public:
            constexpr id() = default;

            constexpr id(std::nullptr_t) {}

            constexpr id(native_handle_type thread_id) : native_handle_(thread_id) {}  // NOLINT(*-explicit-constructor)

            friend bool operator==(id lhs, id rhs) {
                return lhs.native_handle_ == rhs.native_handle_;
            }

            friend std::strong_ordering operator<=>(id lhs, id rhs) {
                return lhs.native_handle_ <=> rhs.native_handle_;
            }

        private:
            friend class Thread;
            friend struct std::hash<id>;
            native_handle_type native_handle_ = nullptr;
        };

        ///  Creates a new std::thread object which does not represent a thread.
        Thread() = default;

        /**
         * Creates a new std::thread object and associates it with a thread of execution.
         * The new thread of execution starts executing:
         * @param name
         * @param priority
         * @param stack
         * @param f
         */
        template<size_t StackSize = configMINIMAL_STACK_SIZE, size_t FuncSize = sizeof(void*)>
        Thread(const char* name,
               int priority,
               detail::InplaceThreadState<StackSize, FuncSize>& state,
               std::invocable auto&& f);

        /**
         * Destroys the thread object.
         * @warning If *this has an associated thread (joinable() == true), an assert is fired
         */
#if (INCLUDE_vTaskDelete == 1)
        ~Thread() noexcept(detail::noexcept_dtor);
#else
        ~Thread() = delete;
#endif
        Thread(Thread&)                          = delete;
        Thread& operator=(Thread&)               = delete;
        Thread(Thread&& t) noexcept              = default;
        Thread& operator=(Thread&& t) & noexcept = default;

        void swap(Thread& t) noexcept {
            std::swap(state_, t.state_);
        }

        /**
         * Checks if the seal::Thread object identifies an active thread of execution.
         * Specifically, returns true if get_id() != std::thread::id(). So a default constructed
         * thread is not joinable.
         *
         * A thread that has finished executing code, but has not yet been joined is still
         * considered an active thread of execution and is therefore joinable.
         * @return true if the std::thread object identifies an active thread of execution, false otherwise
         */
        bool joinable() const {
            return !(native_handle() == id());
        }

        /**
         * Returns a value of type std::thread::id identifying the thread associated
         * with *this. If there is no thread associated, default constructed std::thread::id is returned.
         */
        [[nodiscard]] id get_id() const {
            return native_handle();
        }

        /**
         *
         * @return the implementation defined underlying thread handle
         */
        [[nodiscard]] native_handle_type native_handle() const;

        /**
         * Obtains the number of concurrent threads supported by the implementation.
         * The value should be considered only a hint.
         * @return concurrent threads supported. If the value is not well defined or not computable, returns 0
         */
        static unsigned int hardware_concurrency() {
            return 1;
        }

        /**
         * Kill a running thread non-cooperatively and remove it from the
         * kernel scheduling queue. Resources are not freed until the owning class
         * is destructed or moved.
         * @note This is a RTOS extension, and not available in pthreads or standard C++
         */
        std::error_code kill();

        /**
         * Set the tasks priority
         * @note A context switch will occur before the function returns if the priority
         *       being set is higher than the currently executing task.
         * @pre priority must be less than max_priority()
         * @param priority
         */
        void set_priority(uint32_t priority) {  // NOLINT(*-make-member-function-const)
            vTaskPrioritySet(native_handle(), priority);
        }

        /**
         * Get the thread priority. This is the current actual priority
         * of the thread, including any priority inheritance that may happen.
         * @return The current priority of the thread
         */
        uint32_t priority() const;

        /// the maximum possible priority available for a task
        static constexpr uint32_t max_priority() {
            return configMAX_PRIORITIES;
        }

    private:
        void start_thread(const char* name);

        larid::erased_unique_ptr<detail::ThreadState> state_;
    };

    namespace this_thread {

        /// suggests that the implementation reschedule execution of threads
        inline void yield() {
            taskYIELD();
        }

        /// returns the thread id of the current thread
        inline Thread::native_handle_type native_handle() {
            return xTaskGetCurrentTaskHandle();
        }

        /// returns the thread id of the current thread
        inline Thread::id get_id() {
            return {xTaskGetCurrentTaskHandle()};
        }

        /// stops the execution of the current thread for a specified time duration
        template<class Rep, class Period>
        void sleep_for(const std::chrono::duration<Rep, Period>& sleep_duration) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_duration).count();
            vTaskDelay(pdMS_TO_TICKS(ms));
        }

        /// stops the execution of the current thread until a specified time point
        template<class Clock, class Duration>
        void sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time);

        /**
         * Object that allows a consistent sleep period, accounting for time spent
         * working between calls to sleep.
         */
        class PeriodicSleep {
            TickType_t last_wake_;  // tick count of the last wakeup

        public:
            /// Constructs a period sleep object and initializes it with the current time
            PeriodicSleep() : last_wake_{xTaskGetTickCount()} {}

            /**
             * Sleep for a specific amount of time measured from the last wakeup
             * @note The first time this is called, the last wake time was the time of this object construction
             * @param period amount of time to sleep
             * @return True if there was any sleeping performed
             * @return False if the deadline was missed, and the target time is in the past
             */
            template<class Rep, class Period>
            bool sleep(const std::chrono::duration<Rep, Period>& period) {
                const auto p = std::chrono::duration_cast<std::chrono::milliseconds>(period);
                return xTaskDelayUntil(&last_wake_, pdMS_TO_TICKS(p.count())) != 0;
            }
        };

        /**
         * get the priority of the current thread. This is the current actual priority
         * of the thread, including any priority inheritance that may happen.
         * @return The current priority of the thread
         */
        inline uint32_t priority() {
            return uxTaskPriorityGet(nullptr);
        }

        ///
        inline uint32_t stack_high_water() {
            return uxTaskGetStackHighWaterMark(nullptr);
        }

    }  // namespace this_thread

    namespace os {

        inline uint32_t number_of_threads() {
#if configUSE_TIMERS
            static constexpr uint32_t os_tasks = 2;
#else
            static constexpr uint32_t os_tasks = 1;
#endif
            return uxTaskGetNumberOfTasks() - os_tasks;
        }

    }  // namespace os

    template<size_t StackSize, size_t FuncSize>
    Thread::Thread(const char* name,
                   int priority,
                   detail::InplaceThreadState<StackSize, FuncSize>& state,
                   std::invocable auto&& f) {
        state.func_storage_ = std::forward<decltype(f)>(f);
        configASSERT(!state.state_.alive.test());

        detail::ThreadState* s = &state.state_;
        std::memset(&s->tcb, 0, sizeof(StaticTask_t));
        s->priority = priority;
        s->fun      = [&state]() { state.func_storage_(); };
        s->stack    = state.stack_;

        state_ = {s, [&state](void* p) {
                      taskENTER_CRITICAL();
                      if (auto* handle = state.state_.get_native_handle(); handle != nullptr) {
                          vTaskDelete(handle);
                          state.state_.alive.clear();
                      }
                      taskEXIT_CRITICAL();
                  }};
        start_thread(name);
    }

    inline Thread::native_handle_type Thread::native_handle() const {
        if (state_) {
            return state_->get_native_handle();
        }
        return nullptr;
    }

    inline uint32_t Thread::priority() const {
        return uxTaskPriorityGet(native_handle());
    }

}  // namespace seal
