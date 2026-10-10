#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* F407 @168 MHz，内核 tick 1 ms。HAL 的 1 ms 时基另走 TIM6，
   所以这里的 SysTick 只服务调度器。 */

#define configCPU_CLOCK_HZ                       (168000000UL)
#define configTICK_RATE_HZ                       (1000UL)
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configMAX_PRIORITIES                     (5)
#define configMINIMAL_STACK_SIZE                 (128)
#define configMAX_TASK_NAME_LEN                  (16)
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_TIMERS                         0
#define configUSE_CO_ROUTINES                    0

/* 任务栈从这里切。16 KB 够 4 个任务加队列，PCM 缓冲不在这一步。 */
#define configTOTAL_HEAP_SIZE                    (16 * 1024)

#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2

#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configASSERT(x)  do { if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); } } while(0)

#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelete            1
#define INCLUDE_xTaskGetSchedulerState  1

/* 向量表里的名字是 CMSIS 的，port 里的名字不同，在这里接上。 */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
