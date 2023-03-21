/*
 ******************************************************************************
 *  @file stm32f103xx_CMSIS.c
 *  @brief Двухканальный частотомер на CMSIS на примере МК STM32F103C8T6
 *  Замер частоты в одноканальном режиме до 36МГц, в двухканальном режиме до 18 MHz на канал
 *  1 канал - ножка PA12
 *  2 канал - ножка PA0
 *  Таймер 3 отсчитывает 1 секунду, при этом работая в режиме Master, запускает собой таймеры
 *  1 и 2, которые работают в режиме счетчиков импульсов(ETR).
 *  P.S. Это самый точный частотомер, который мне удавалось сделать на STM32.
 *  @author Волков Олег
 *  @date 21.03.2023
 *
 ******************************************************************************
*/
#include "main.h"
#include <stm32f103xx_CMSIS.h>
volatile uint32_t TIM1_CNT_OVF = 0; //Переполнение счетчика таймера 1
volatile uint32_t TIM2_CNT_OVF = 0; //Переполнение счетчика таймера 2
volatile uint32_t TIM1_FREQ = 0; //Частота, захваченная таймером 1
volatile uint32_t TIM2_FREQ = 0; //Частота, захваченная таймером 2

void TIM1_UP_IRQHandler(void) {
    if (READ_BIT(TIM1->SR, TIM_SR_UIF)) {
        CLEAR_BIT(TIM1->SR, TIM_SR_UIF); //Сбросим флаг прерывания
        TIM1_CNT_OVF++;
    }
}
void TIM2_IRQHandler(void) {
    if (READ_BIT(TIM2->SR, TIM_SR_UIF)) {
        CLEAR_BIT(TIM2->SR, TIM_SR_UIF); //Сбросим флаг прерывания
        TIM2_CNT_OVF++;
    }
}

void TIM3_IRQHandler(void) {
    if (READ_BIT(TIM3->SR, TIM_SR_UIF)) {
        CLEAR_BIT(TIM3->SR, TIM_SR_UIF); //Сбросим флаг прерывания
        TIM1_FREQ = (TIM1->CNT) + (TIM1_CNT_OVF * (TIM1->ARR)) + TIM1_CNT_OVF;
        TIM2_FREQ = (TIM2->CNT) + (TIM2_CNT_OVF * (TIM2->ARR)) + TIM2_CNT_OVF;
        TIM1->CNT = 0; //Сбросим счетчик таймера 1
        TIM2->CNT = 0; //Сбросим счетчик таймера 2
        TIM1_CNT_OVF = 0; //Сбросим счетчик переполнения таймера 1
        TIM2_CNT_OVF = 0; //Сбросим счетчик переполнения таймера 2
        SET_BIT(TIM3->CR1, TIM_CR1_CEN); //Запуск таймера 3
    }
    
}

int main(void) {
    CMSIS_Debug_init(); //Настройка дебага
    CMSIS_RCC_SystemClock_72MHz(); //Настройка частоты МК на 72 МГц
    CMSIS_SysTick_Timer_init(); //Настройка системного таймера
    CMSIS_PA8_MCO_init(); //Настройка ножки MCO(PA8) на 8 МГц.
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN); //Запуск тактирования альтернативных функций
    
    //Настройка таймера 3. Он будет отсчитывать 1 секунду и управлять таймером 1 и таймером 2.
    /*Включим тактирование таймера (страница 48)*/
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM3EN); //Запуск тактирования таймера 3
    
    CLEAR_BIT(TIM3->CR1, TIM_CR1_UDIS); //Генерировать событие Update
    CLEAR_BIT(TIM3->CR1, TIM_CR1_URS); //Генерировать прерывание
    SET_BIT(TIM3->CR1, TIM_CR1_OPM); //One pulse mode on(Счетчик не останавливается при обновлении)
    CLEAR_BIT(TIM3->CR1, TIM_CR1_DIR); //Считаем вверх
    MODIFY_REG(TIM3->CR1, TIM_CR1_CMS_Msk, 0b00 << TIM_CR1_CMS_Pos); //Выравнивание по краю
    CLEAR_BIT(TIM3->CR1, TIM_CR1_ARPE); //Auto-reload preload disable
    MODIFY_REG(TIM3->CR1, TIM_CR1_CKD_Msk, 0b00 << TIM_CR1_CKD_Pos); //Предделение выключено
    
    MODIFY_REG(TIM3->CR2, TIM_CR2_MMS_Msk, 0b001 << TIM_CR2_MMS_Pos);//Включить — сигнал включения счетчика, CNT_EN, используется в качестве триггерного выхода (TRGO). 
    //Это полезно для одновременного запуска нескольких таймеров или для управления окном, в котором запущен подчиненный таймер.
    SET_BIT(TIM3->SMCR, TIM_SMCR_MSM);//Влияние события на триггерный вход (TRGI) задерживается, чтобы обеспечить идеальную синхронизацию между текущим таймером и его ведомыми (через TRGO). 
    //Полезно, если мы хотите синхронизировать несколько таймеров по одному внешнему событию.
    
    SET_BIT(TIM3->DIER, TIM_DIER_UIE); //Update interrupt enable
    //Настройка таймера на 1 Гц.
    TIM3->PSC = 7200 - 1;
    TIM3->ARR = 10000 - 1;
    
    NVIC_EnableIRQ(TIM3_IRQn); //Разрешить прерывания по таймеру 3
    
    //Настройка таймера 1:
    //Настроим ножку под External clock 
    //TIM1 / 8_ETR External trigger timer input Input floating
    //PA12 - настройка под External trigger timer input Input floating
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN);//Запустим тактирование порта А
    MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE12_Msk, 0b00 << GPIO_CRH_MODE12_Pos);
    MODIFY_REG(GPIOA->CRH, GPIO_CRH_CNF12_Msk, 0b01 << GPIO_CRH_CNF12_Pos);
    
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_TIM1EN); //Запуск тактирования таймера 1
    MODIFY_REG(TIM1->CR1, TIM_CR1_CKD_Msk, 0b00 << TIM_CR1_CKD_Pos); //Без предделителя
    CLEAR_BIT(TIM1->CR1, TIM_CR1_ARPE); //0: TIMx_ARR register is not buffered
    MODIFY_REG(TIM1->CR1, TIM_CR1_CMS_Msk, 0b00 << TIM_CR1_CMS_Pos); //Выравнивание по краю
    CLEAR_BIT(TIM1->CR1, TIM_CR1_DIR); //Считаем вниз
    CLEAR_BIT(TIM1->CR1, TIM_CR1_OPM); //One pulse mode off
    CLEAR_BIT(TIM1->CR1, TIM_CR1_UDIS); //Генерировать событие Update
    CLEAR_BIT(TIM1->CR1, TIM_CR1_URS); //Генерировать прерывание
    TIM1->CR2 = 0;//Очистим CR2
    //MODIFY_REG(TIM1->CR2, TIM_CR2_MMS_Msk, 0b000 < TIM_CR2_MMS_Pos); //Master mode reset
    SET_BIT(TIM1->SMCR, TIM_SMCR_ECE); //External clock включен. Счетчик тактируется любым активным фронтом на ETRF.
    CLEAR_BIT(TIM1->SMCR, TIM_SMCR_MSM); //No action
    MODIFY_REG(TIM1->SMCR, TIM_SMCR_TS_Msk, 0b010 << TIM_SMCR_TS_Pos); //Работать по триггеру от ITR2
    //Настроим таймер в Gate mode - Часы счетчика включаются, когда вход триггера (TRGI) имеет высокий уровень. 
    //счетчик останавливается (но не сбрасывается), как только триггер становится низким. Как запуск, так и остановка счетчик контролируется.
    MODIFY_REG(TIM1->SMCR, TIM_SMCR_SMS_Msk, 0b101 << TIM_SMCR_SMS_Pos); //Gated mode
    SET_BIT(TIM1->DIER, TIM_DIER_UIE); //Update interrupt enable
    TIM1->PSC = 0;
    TIM1->ARR = 65535;
    NVIC_EnableIRQ(TIM1_UP_IRQn); //Разрешить прерывания по таймеру 1
    
    
    //Настройка таймера 2:
    //Настроим ножку под External clock 
    //TIM1 / 8_ETR External trigger timer input Input floating
    //PA0 - настройка под External trigger timer input Input floating
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN); //Запустим тактирование порта А
    MODIFY_REG(GPIOA->CRL, GPIO_CRL_MODE0_Msk, 0b00 << GPIO_CRL_MODE0_Pos);
    MODIFY_REG(GPIOA->CRL, GPIO_CRL_CNF0_Msk, 0b01 << GPIO_CRL_CNF0_Pos);
    
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM2EN); //Запуск тактирования таймера 2
    MODIFY_REG(TIM2->CR1, TIM_CR1_CKD_Msk, 0b00 << TIM_CR1_CKD_Pos); //Без предделителя
    CLEAR_BIT(TIM2->CR1, TIM_CR1_ARPE); //0: TIMx_ARR register is not buffered
    MODIFY_REG(TIM2->CR1, TIM_CR1_CMS_Msk, 0b00 << TIM_CR1_CMS_Pos); //Выравнивание по краю
    CLEAR_BIT(TIM2->CR1, TIM_CR1_DIR); //Считаем вниз
    CLEAR_BIT(TIM2->CR1, TIM_CR1_OPM); //One pulse mode off
    CLEAR_BIT(TIM2->CR1, TIM_CR1_UDIS); //Генерировать событие Update
    CLEAR_BIT(TIM2->CR1, TIM_CR1_URS); //Генерировать прерывание
    TIM2->CR2 = 0; //Очистим CR2
    //MODIFY_REG(TIM1->CR2, TIM_CR2_MMS_Msk, 0b000 < TIM_CR2_MMS_Pos); //Master mode reset
    SET_BIT(TIM2->SMCR, TIM_SMCR_ECE); //External clock включен. Счетчик тактируется любым активным фронтом на ETRF.
    CLEAR_BIT(TIM2->SMCR, TIM_SMCR_MSM); //No action
    MODIFY_REG(TIM2->SMCR, TIM_SMCR_TS_Msk, 0b010 << TIM_SMCR_TS_Pos); //Работать по триггеру от ITR2
    //Настроим таймер в Gate mode - Часы счетчика включаются, когда вход триггера (TRGI) имеет высокий уровень. 
    //счетчик останавливается (но не сбрасывается), как только триггер становится низким. Как запуск, так и остановка счетчик контролируется.
    MODIFY_REG(TIM2->SMCR, TIM_SMCR_SMS_Msk, 0b101 << TIM_SMCR_SMS_Pos); //Gated mode
    SET_BIT(TIM2->DIER, TIM_DIER_UIE); //Update interrupt enable
    TIM2->PSC = 0;
    TIM2->ARR = 65535;
    
    NVIC_EnableIRQ(TIM2_IRQn); //Разрешить прерывания по таймеру 2
    
    CLEAR_BIT(TIM1->SR, TIM_SR_UIF); //Сбросим флаг прерывания таймера 1
    CLEAR_BIT(TIM2->SR, TIM_SR_UIF); //Сбросим флаг прерывания таймера 2
    CLEAR_BIT(TIM3->SR, TIM_SR_UIF); //Сбросим флаг прерывания таймера 3
    SET_BIT(TIM3->CR1, TIM_CR1_CEN); //Запуск таймера 3
    SET_BIT(TIM1->CR1, TIM_CR1_CEN); //Запуск таймера 1
    SET_BIT(TIM2->CR1, TIM_CR1_CEN); //Запуск таймера 2
    
    while (1) {

	}
}