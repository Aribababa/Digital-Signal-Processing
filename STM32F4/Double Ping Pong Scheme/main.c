#include "stm32f4_discovery.h"

#define DAC_DHR12R2_ADDRESS    0x40007414
#define DAC_DHR8R1_ADDRESS     0x40007410
#define BLOCK_SIZE 1024
#define ADC1_ADDRESS	((uint32_t)(&(ADC1->DR)))

DAC_InitTypeDef  DAC_InitStructure;

const uint8_t Escalator8bit[6] = {0x0, 0x33, 0x66, 0x99, 0xCC, 0xFF};

unsigned char SwitchFlag = 0;
uint16_t Ping[2][BLOCK_SIZE+1];
uint16_t Pong[2][BLOCK_SIZE+1];

__IO uint8_t SelectedWavesForm = 0;
__IO uint8_t KeyPressed = SET;

void TIM6_Config(void);

void DAC_Ch2_SineWaveConfig(void);
void ADC1_CH6_DMA_Config(void);

int main(void){

	GPIO_InitTypeDef GPIO_InitStructure;

	/* DMA1 clock and GPIOA clock enable (to be used with DAC) */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_GPIOA, ENABLE);

	/* DAC Periph clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

	/* DAC channel 1 & 2 (DAC_OUT1 = PA.4)(DAC_OUT2 = PA.5) configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* TIM6 Configuration ------------------------------------------------------*/
	TIM6_Config();

	//DAC_DeInit();
	ADC1_CH6_DMA_Config();
	DAC_Ch2_SineWaveConfig();

	//__set_MSP(0x00);
	for(;;);
}

void TIM6_Config(void){
	TIM_TimeBaseInitTypeDef    TIM_TimeBaseStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Period = 4;
	TIM_TimeBaseStructure.TIM_Prescaler = 76;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);

	TIM_Cmd(TIM2, ENABLE);
}

void DAC_Ch2_SineWaveConfig(void){
	DMA_InitTypeDef DMA_InitStructure;

	/* DAC channel2 Configuration */
	DAC_InitStructure.DAC_Trigger = DAC_Trigger_T2_TRGO;
	DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
	DAC_Init(DAC_Channel_2, &DAC_InitStructure);

	/* DMA1_Stream5 channel7 configuration **************************************/
	DMA_InitStructure.DMA_Channel = DMA_Channel_7;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)DAC_DHR12R2_ADDRESS;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Ping[0][0];
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStructure.DMA_BufferSize = BLOCK_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_DoubleBufferModeConfig(DMA1_Stream6,(uint32_t)&Ping[1][0],DMA_Memory_0);
	DMA_DoubleBufferModeCmd(DMA1_Stream6,ENABLE);
	DMA_Init(DMA1_Stream6, &DMA_InitStructure);

	/* Enable DMA1_Stream5 */
	DMA_Cmd(DMA1_Stream6, ENABLE);

	/* Enable DAC Channel2 */
	DAC_Cmd(DAC_Channel_2, ENABLE);

	/* Enable DMA for DAC Channel2 */
	DAC_DMACmd(DAC_Channel_2, ENABLE);
}

void ADC1_CH6_DMA_Config(void){
	ADC_InitTypeDef       ADC_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	DMA_InitTypeDef       DMA_InitStructure;
	GPIO_InitTypeDef      GPIO_InitStructure;
	NVIC_InitTypeDef       NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2 | RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* Configure ADC1 Channel6 pin as analog input ******************************/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	DMA_DeInit(DMA2_Stream0);
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC1_ADDRESS;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Ping[0][0];
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = BLOCK_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

	DMA_DoubleBufferModeConfig(DMA2_Stream0,(uint32_t)&Ping[1][0],DMA_Memory_1);
	DMA_DoubleBufferModeCmd(DMA2_Stream0,ENABLE);
	DMA_Init(DMA2_Stream0, &DMA_InitStructure);

	DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);

	DMA_Cmd(DMA2_Stream0, ENABLE);

	/* ADC Common Init **********************************************************/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC1 Init ****************************************************************/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_TRGO;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	/* ADC1 regular channe6 configuration *************************************/
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 1, ADC_SampleTime_15Cycles);

	/* Enable DMA request after last transfer (Single-ADC mode) */
	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);

	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 14;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void DMA2_Stream0_IRQHandler(void){

	DMA_ClearFlag(DMA2_Stream0, DMA_IT_TC);
}
