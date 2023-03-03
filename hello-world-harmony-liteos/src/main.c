#include "ch32v_it.h"
#include "debug.h"
#include "los_tick.h"
#include "los_task.h"
#include "los_config.h"
#include "los_interrupt.h"
#include "los_debug.h"
#include "los_compiler.h"

/* Global define */

/* Global Variable */
__attribute__((aligned(8))) UINT8 g_memStart[LOSCFG_SYS_HEAP_SIZE];
UINT32 g_VlaueSp = 0;

/*********************************************************************
 * @fn      taskSampleEntry2
 *
 * @brief   taskSampleEntry2 program.
 *
 * @return  none
 */
VOID taskSampleEntry2(VOID)
{
    while (1)
    {
        LOS_TaskDelay(5000);
        printf("taskSampleEntry2 running,task2 SP:%08x\n", (unsigned) __get_SP());
    }
}

/*********************************************************************
 * @fn      taskSampleEntry1
 *
 * @brief   taskSampleEntry1 program.
 *
 * @return  none
 */
VOID taskSampleEntry1(VOID)
{
    while (1)
    {
        LOS_TaskDelay(1000);
        printf("taskSampleEntry1 running,task1 SP:%08x\n", (unsigned) __get_SP());
    }
}

/*********************************************************************
 * @fn      EXTI0_INT_INIT
 *
 * @brief   Initializes EXTI0 collection.
 *
 * @return  none
 */
void EXTI0_INT_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* GPIOA ----> EXTI_Line0 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 4;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*********************************************************************
 * @fn      taskSample
 *
 * @brief   taskSample program.
 *
 * @return  none
 */
UINT32 taskSample(VOID)
{
    UINT32 uwRet;
    UINT32 taskID1, taskID2;
    TSK_INIT_PARAM_S stTask = {0};
    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC)taskSampleEntry1;
    stTask.uwStackSize = 0X500;
    stTask.pcName = "taskSampleEntry1";
    stTask.usTaskPrio = 6; /* 高优先级 */
    uwRet = LOS_TaskCreate(&taskID1, &stTask);
    if (uwRet != LOS_OK)
    {
        printf("create task1 failed\n");
    }

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC)taskSampleEntry2;
    stTask.uwStackSize = 0X500;
    stTask.pcName = "taskSampleEntry2";
    stTask.usTaskPrio = 7; /* 低优先级 */
    uwRet = LOS_TaskCreate(&taskID2, &stTask);
    if (uwRet != LOS_OK)
    {
        printf("create task2 failed\n");
    }

    EXTI0_INT_INIT();
    return LOS_OK;
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
LITE_OS_SEC_TEXT_INIT int main(void)
{
    unsigned int ret;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    Delay_Ms(1000); // give serial monitor time to open

    printf("SystemClk:%lu\r\n", SystemCoreClock);
    #if defined(CH32V30X)
    printf("ChipID: %08x\r\n", (unsigned)DBGMCU_GetCHIPID());
    #else
    printf("DeviceID: %08x\r\n", (unsigned)DBGMCU_GetDEVID());
    #endif

    ret = LOS_KernelInit();
    taskSample();
    if (ret == LOS_OK)
    {
        LOS_Start();
    }

    while (1)
    {
        __asm volatile("nop");
    }
}

/*********************************************************************
 * @fn      EXTI0_IRQHandler
 *
 * @brief   This function handles EXTI0 Handler.
 *
 * @return  none
 */
void EXTI0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI0_IRQHandler(void)
{
    /* 中断栈使用的是原来调用main设置的值，将中断栈和线程栈分开，这样线程跳中断，中断函数如果嵌套深度较大，不至于
     * 线程栈被压满溢出，但是采用当前方式，线程进中断时，编译器保存到的16个caller寄存器任然压入线程栈，如果需要希
     * 望caller寄存器压入中断栈，则中断函数的入口和出口需要使用汇编，中间调用用户中断处理函数即可，详见los_exc.S
     * 中的ipq_entry例子
     *  */
    GET_INT_SP();
    HalIntEnter();
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        g_VlaueSp = __get_SP();
        printf("Run at EXTI:");
        printf("interruption sp:%08x\r\n", g_VlaueSp);
        HalDisplayTaskInfo();
        EXTI_ClearITPendingBit(EXTI_Line0); /* Clear Flag */
    }
    HalIntExit();
    FREE_INT_SP();
}
