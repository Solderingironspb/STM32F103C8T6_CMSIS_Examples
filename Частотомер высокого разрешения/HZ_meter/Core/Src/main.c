/*
 ******************************************************************************
 *  @file stm32f103xx_CMSIS.c
 *  @brief Одноканальный частотомер высокого разрешения на CMSIS на примере МК STM32F103C8T6
 *  Замер частоты в одноканальном режиме до зависит от входящего фильтра. 
 *  В основном такие частотомеры используются для небольших частот, поэтому фильтр настраиваем 
 *  максимально до рабочей частоты
 *  1 канал измерительный - ножка PA12(External clock TIM1)
 *  PC14 - сигнал старт/стоп замера (идет на D-триггер)
 *  PA8 - внешний триггер от Gate(D-триггера)(Таймер 1)
 *  PB6 - внешний триггер от Gate(D-триггера)(Таймер 4)
 *  @author Волков Олег
 *  @date 21.03.2023
 *
 ******************************************************************************
*/



#include "main.h"
#include <stm32f103xx_CMSIS.h>
volatile uint32_t TIM1_CNT_OVF = 0; //Переполнение счетчика таймера 1
volatile uint32_t TIM4_CNT_OVF = 0; //Переполнение счетчика таймера 4
volatile uint32_t TIM1_FREQ = 0; //Частота, захваченная таймером 1
volatile uint32_t TIM4_FREQ = 0; //Частота, захваченная таймером 4

double FREQ = 0;
uint64_t FREQ_MCO = 8000040; //Опорная частота

void TIM1_UP_IRQHandler(void) {
	if (READ_BIT(TIM1->SR, TIM_SR_UIF)) {
		CLEAR_BIT(TIM1->SR, TIM_SR_UIF); //Сбросим флаг прерывания
		TIM1_CNT_OVF++;
	}
}

void TIM4_IRQHandler(void) {
	if (READ_BIT(TIM4->SR, TIM_SR_UIF)) {
		CLEAR_BIT(TIM4->SR, TIM_SR_UIF); //Сбросим флаг прерывания
		TIM4_CNT_OVF++;
	}
}


void CMSIS_TIM1_init(void) {
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN); //Запустим тактирование порта А
	//PA8 Input floating
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE8_Msk, 0b00 << GPIO_CRH_MODE8_Pos);
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_CNF8_Msk, 0b01 << GPIO_CRH_CNF8_Pos);
	//Настроим ножку под External clock 
	//TIM1 / 8_ETR External trigger timer input Input floating
	//PA12 - настройка под External trigger timer input Input floating
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
	TIM1->CR2 = 0; //Очистим CR2
	SET_BIT(TIM1->SMCR, TIM_SMCR_ECE); //External clock включен. Счетчик тактируется любым активным фронтом на ETRF.
	MODIFY_REG(TIM1->SMCR, TIM_SMCR_ETF_Msk, 0b1111 << TIM_SMCR_ETF_Pos); //подключим фильтр
	CLEAR_BIT(TIM1->SMCR, TIM_SMCR_MSM); //No action
	MODIFY_REG(TIM1->SMCR, TIM_SMCR_TS_Msk, 0b101 << TIM_SMCR_TS_Pos); //Работать по триггеру от PA8
	//Настроим таймер в Gate mode - Часы счетчика включаются, когда вход триггера (TRGI) имеет высокий уровень. 
	//счетчик останавливается (но не сбрасывается), как только триггер становится низким. Как запуск, так и остановка счетчик контролируется.
	MODIFY_REG(TIM1->SMCR, TIM_SMCR_SMS_Msk, 0b101 << TIM_SMCR_SMS_Pos); //Gated mode
	SET_BIT(TIM1->DIER, TIM_DIER_UIE); //Update interrupt enable
	TIM1->PSC = 0;
	TIM1->ARR = 65535;
	NVIC_EnableIRQ(TIM1_UP_IRQn); //Разрешить прерывания по таймеру 1
}


void CMSIS_TIM4_init(void) {
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN); //Запустим тактирование порта B
	//PA6 Input floating
	MODIFY_REG(GPIOB->CRL, GPIO_CRL_MODE6_Msk, 0b00 << GPIO_CRL_MODE6_Pos);
	MODIFY_REG(GPIOB->CRL, GPIO_CRL_CNF6_Msk, 0b01 << GPIO_CRL_CNF6_Pos);
	
	/*Включим тактирование таймера (страница 48)*/
	SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM4EN); //Запуск тактирования таймера 4
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN); //Запуск тактирования альтернативных функций

	MODIFY_REG(TIM4->CR1, TIM_CR1_CKD_Msk, 0b00 << TIM_CR1_CKD_Pos); //Без предделителя
	CLEAR_BIT(TIM4->CR1, TIM_CR1_ARPE); //0: TIMx_ARR register is not buffered
	MODIFY_REG(TIM4->CR1, TIM_CR1_CMS_Msk, 0b00 << TIM_CR1_CMS_Pos); //Выравнивание по краю
	CLEAR_BIT(TIM4->CR1, TIM_CR1_DIR); //Считаем вниз
	CLEAR_BIT(TIM4->CR1, TIM_CR1_OPM); //One pulse mode off
	CLEAR_BIT(TIM4->CR1, TIM_CR1_UDIS); //Генерировать событие Update
	CLEAR_BIT(TIM4->CR1, TIM_CR1_URS); //Генерировать прерывание
	TIM2->CR2 = 0; //Очистим CR2
	MODIFY_REG(TIM4->CR1, TIM_CR1_CKD_Msk, 0b00 << TIM_CR1_CKD_Pos); //Предделение выключено
	//Попробуем управлять таймером с внешней ножки МК
	MODIFY_REG(TIM4->SMCR, TIM_SMCR_TS_Msk, 0b101 << TIM_SMCR_TS_Pos); //Работать по триггеру от PB6
	MODIFY_REG(TIM4->SMCR, TIM_SMCR_SMS_Msk, 0b101 << TIM_SMCR_SMS_Pos); //Gated mode
	SET_BIT(TIM4->DIER, TIM_DIER_UIE); //Update interrupt enable
	TIM4->PSC = 9 - 1;
	TIM4->ARR = 65535;
	NVIC_EnableIRQ(TIM4_IRQn); //Разрешить прерывания по таймеру 4
	
}

int main(void) {
	CMSIS_Debug_init(); //Настройка дебага
	CMSIS_RCC_SystemClock_72MHz(); //Настройка частоты МК на 72 МГц
	CMSIS_SysTick_Timer_init(); //Настройка системного таймера
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN); //Запуск тактирования альтернативных функций
	CMSIS_TIM1_init(); //Инит таймера 1
	CMSIS_TIM4_init(); //Инит таймера 4
	
    /*Настройка ножки PC14 на выход в режиме push-pull*/
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPCEN); //Запуск тактирования порта C
	MODIFY_REG(GPIOC->CRH, GPIO_CRH_MODE14, 0b10 << GPIO_CRH_MODE14_Pos); //Настройка GPIOC порта 14 на выход со максимальной скоростью в 50 MHz
	MODIFY_REG(GPIOC->CRH, GPIO_CRH_CNF14, 0b00 << GPIO_CRH_CNF14_Pos); //Настройка GPIOC порта 14 на выход в режиме Push-Pull

	/*Запуск таймеров*/
	SET_BIT(TIM1->CR1, TIM_CR1_CEN); //Запуск таймера 1
	SET_BIT(TIM4->CR1, TIM_CR1_CEN); //Запуск таймера 4

   		
	while (1) {
        	
		GPIOC->BSRR = GPIO_BSRR_BS14; //Дать команду на замер сигнала.
		Delay_ms(1000);
		GPIOC->BSRR = GPIO_BSRR_BR14; //Закончить замер сигнала.
		Delay_ms(1000);
		TIM1_FREQ = (TIM1->CNT) + (TIM1_CNT_OVF * (TIM1->ARR)) + TIM1_CNT_OVF;
		TIM4_FREQ = (TIM4->CNT) + (TIM4_CNT_OVF * (TIM4->ARR)) + TIM4_CNT_OVF;
		FREQ = (double)TIM1_FREQ * (double)FREQ_MCO / (double)TIM4_FREQ;
		TIM1->CNT = 0; //Сбросим счетчик таймера 1
		TIM4->CNT = 0; //Сбросим счетчик таймера 2
		TIM1_CNT_OVF = 0; //Сбросим счетчик переполнения таймера 1
		TIM4_CNT_OVF = 0; //Сбросим счетчик переполнения таймера 2
    	
	}
}