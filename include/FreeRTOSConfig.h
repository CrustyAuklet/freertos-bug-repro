#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/********** project specific FreeRTOS configuration **************/
#define configPRIO_BITS         2

/* The lowest interrupt priority that can be used in a call to a "set priority" function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   3

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1

#define configCPU_CLOCK_HZ                                      (8000000UL)
#define configTICK_RATE_HZ                                      ((TickType_t) 1000)


/********** Standard FreeRTOS config macros *********************/

#define configUSE_PREEMPTION                                    (1)
#define configUSE_TIME_SLICING                                  (0)
#define configMAX_PRIORITIES                                    (32UL)
#define configTICK_TYPE_WIDTH_IN_BITS                           (TICK_TYPE_WIDTH_32_BITS)
#define configIDLE_SHOULD_YIELD                                 (1)
#define configUSE_TASK_NOTIFICATIONS                            (1)
#define configMAX_TASK_NAME_LEN                                 (6)
#define configENABLE_BACKWARD_COMPATIBILITY                     (0)

#define configSUPPORT_STATIC_ALLOCATION                         (0)

#define configUSE_MUTEXES                                       (1)
#define configUSE_RECURSIVE_MUTEXES                             (1)
#define configUSE_COUNTING_SEMAPHORES                           (1)
#define configQUEUE_REGISTRY_SIZE                               (10)

/* Hook function related definitions. */
#define configCHECK_FOR_STACK_OVERFLOW                          (2)
#define configUSE_IDLE_HOOK                                     (0)
#define configUSE_TICK_HOOK                                     (0)

/* Software timer related definitions. */
#define configUSE_TIMERS                                        1
#define configTIMER_TASK_PRIORITY                               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                                10

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskDelete                                     1
#define INCLUDE_vTaskSuspend                                    1
#define INCLUDE_vTaskDelayUntil                                 1
#define INCLUDE_vTaskDelay                                      1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Define to trap errors during development.
void vAssertCalled(const char *, int);

#if defined(configUSE_TICKLESS_IDLE) && configUSE_TICKLESS_IDLE > 0
void vPreSleepProcessing(uint32_t *v);

void vPostSleepProcessing(uint32_t v);
#endif

#ifdef __cplusplus
}
#endif

#define configASSERT(x) if ((x) == 0) vAssertCalled(__FILE__, __LINE__)

#if defined(configUSE_TICKLESS_IDLE) && configUSE_TICKLESS_IDLE > 0
#define configPRE_SLEEP_PROCESSING(x) vPreSleepProcessing(&(x))
#define configPOST_SLEEP_PROCESSING(x) vPostSleepProcessing(x)
#endif

#ifndef configMINIMAL_STACK_SIZE
#if(__unix)
    /**
     * When emulating on linux the stack must be at least `PTHREAD_STACK_MIN` otherwise
     * the call to `pthread_attr_setstack` will fail and the stack space we allocated won't
     * be used. This results in errors printed to STDOUT but also we can't inspect the stack
     * memory as we would on target.
     *
     * Not that the macro `PTHREAD_STACK_MIN` returns the result of a function call so we can't use
     * it directly. But the value can be found by running `man pthread_attr_setstack`
     */
    #define configMINIMAL_STACK_SIZE                                (16384)
    #define configTIMER_TASK_STACK_DEPTH                            configMINIMAL_STACK_SIZE
#else // arm cortex-m
    #define configMINIMAL_STACK_SIZE                                (128)
#endif
#endif // ifndef configMINIMAL_STACK_SIZE

#ifndef configTIMER_TASK_STACK_DEPTH
#define configTIMER_TASK_STACK_DEPTH                                configMINIMAL_STACK_SIZE
#endif

#ifndef configENABLE_MPU
#define configENABLE_MPU 0
#endif

#ifndef configENABLE_FPU
#define configENABLE_FPU 0
#endif

#if !defined(configTOTAL_HEAP_SIZE) || configTOTAL_HEAP_SIZE < 0
#define configTOTAL_HEAP_SIZE 0
#endif

#if configTOTAL_HEAP_SIZE > 0
#define configSUPPORT_DYNAMIC_ALLOCATION                        (1)
#define configAPPLICATION_ALLOCATED_HEAP                        (0)
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP               (0)
#define configUSE_MALLOC_FAILED_HOOK                            (0)
#define configENABLE_HEAP_PROTECTOR                             (0)
#else
#define configSUPPORT_DYNAMIC_ALLOCATION                        (0)
#define configUSE_MALLOC_FAILED_HOOK                            (0)
#endif

/* Interrupt nesting behavior configuration. */
#ifndef configLIBRARY_LOWEST_INTERRUPT_PRIORITY
#error "configLIBRARY_LOWEST_INTERRUPT_PRIORITY must be defined!"
#endif

#ifndef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#error "configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY must be defined!"
#endif

#ifndef configPRIO_BITS
#error "configPRIO_BITS must be defined!"
#endif

/*
 * Interrupt priorities used by the kernel port layer itself.
 * These are generic to all Cortex-M ports, and do not rely on any particular library functions.
 */
/* The priority at which the tick interrupt runs.  This should probably be kept at lowest priority. */
#define configKERNEL_INTERRUPT_PRIORITY        ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/*
 * he maximum interrupt priority from which FreeRTOS.org API functions can be called.
 * Only API functions that end in ...FromISR() can be used within interrupts.
 * @warning !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
 * @ref http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#endif /* FREERTOS_CONFIG_H */