#include "Effects.h"

/* Seleciona sobre cual buffer se esta trabajando */
volatile uint8_t mem;

/* Indica si el procesador esta ocupado y tambien bloquea para que no se vuelva
 * a procesar el mismo Buffer.
 * */
volatile uint8_t busy;

/* Ajusta el noise gate */
volatile float32_t alpha = 0.98;

/* Indica cual es el efecto que se ejecutará */
volatile uint8_t efecto = 0;

volatile uint16_t tremolo_control = 0;
volatile uint16_t rate = 10;

/* Indica en que posicion se quedo en el buffer del delay */
volatile uint16_t delay_Buffer_Index = 0;

/* Bufferes de entrada y de salida del MCU */
volatile uint16_t Ping[2][BLOCK_SIZE];
volatile uint16_t Pong[2][BLOCK_SIZE];

/* Bloques temporales para pasar a punto flotante */
volatile q15_t TempIn[BLOCK_SIZE];
volatile q15_t TempOut[BLOCK_SIZE];

/* Bufferes para el procesamiento de la señal */
volatile float32_t BufferIn[2][BLOCK_SIZE];
volatile float32_t BufferOut[2][BLOCK_SIZE];
volatile float32_t TempFloat[BLOCK_SIZE];
volatile float32_t DelayBuffer[DELAY_BLOCK_SIZE];

/* Generada mediante Matlab.
 *
 * 	Para el effecto de Flanger y Trémolo se ocupa modular la amplitud y
 * 	el retraso sobre una señal senoidal.
 * 	La seniodal varia de 0 a 1.
 * */
volatile float32_t SineWave[63] = {
		0.5000,    0.5498,    0.5991,    0.6474,    0.6942,    0.7391,    0.7817,    0.8214,    0.8579,    0.8909,    0.9200,    0.9449,
		0.9654,    0.9813,    0.9924,    0.9986,    0.9998,    0.9961,    0.9875,    0.9740,    0.9558,    0.9330,    0.9060,    0.8749,
		0.8401,    0.8019,    0.7607,    0.7169,    0.6710,    0.6234,    0.5745,    0.5249,    0.4751,    0.4255,    0.3766,    0.3290,
		0.2831,    0.2393,    0.1981,    0.1599,    0.1251,    0.0940,    0.0670,    0.0442,    0.0260,    0.0125,    0.0039,    0.0002,
		0.0014,    0.0076,    0.0187,    0.0346,    0.0551,    0.0800,    0.1091,    0.1421,    0.1786,    0.2183,    0.2609,    0.3058,
		0.3526,    0.4009,    0.4502
};


/* Configuramos el ADC y el canal para poder leer */
void ADC_DMA_Config(void){
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
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 1, ADC_SampleTime_56Cycles  );

	/* Enable DMA request after last transfer (Single-ADC mode) */
	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);

	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void DAC_DMA_Config(void){
	DMA_InitTypeDef DMA_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	DAC_InitTypeDef  DAC_InitStructure;

	/* Habilitamos los CLK de cada periférico  */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

	/* Configuracion del pin de salida para el DAC */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configuración del canal 2 del DAC */
	DAC_InitStructure.DAC_Trigger = DAC_Trigger_T2_TRGO;
	DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
	DAC_Init(DAC_Channel_2, &DAC_InitStructure);

	/* DMA1_Stream5 channel7 configuration **************************************/
	DMA_InitStructure.DMA_Channel = DMA_Channel_7;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)DAC_DHR12R2_ADDRESS;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Pong[0][0];
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
	DMA_DoubleBufferModeConfig(DMA1_Stream6,(uint32_t)&Pong[1][0],DMA_Memory_1);
	DMA_DoubleBufferModeCmd(DMA1_Stream6,ENABLE);
	DMA_Init(DMA1_Stream6, &DMA_InitStructure);

	/* Enable DMA1_Stream5 */
	DMA_Cmd(DMA1_Stream6, ENABLE);

	/* Enable DAC Channel2 */
	DAC_Cmd(DAC_Channel_2, ENABLE);

	/* Enable DMA for DAC Channel2 */
	DAC_DMACmd(DAC_Channel_2, ENABLE);
}

void ADC2_Config(void){
	ADC_InitTypeDef       ADC_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	GPIO_InitTypeDef      GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC1 Init ****************************************************************/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_8b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 3;
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_SoftwareStartConv(ADC2);

	ADC_RegularChannelConfig(ADC2, ADC_Channel_0, 1, ADC_SampleTime_3Cycles);
	ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 2, ADC_SampleTime_3Cycles);
	ADC_RegularChannelConfig(ADC2, ADC_Channel_2, 3, ADC_SampleTime_3Cycles);

	//ADC_SoftwareStartConvCmd(ADC2, ENABLE);
	ADC_SoftwareStartConv(ADC2);

	ADC_GetMultiModeConversionValue();


}

void TIM_Config(void){
	TIM_TimeBaseInitTypeDef    TIM_TimeBaseStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	/* Timer configurado para muestrear a 50 KHz */
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Period = 20;
	TIM_TimeBaseStructure.TIM_Prescaler = 83;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);

	/* Habilitamos el timer */
	TIM_Cmd(TIM2, ENABLE);
}

void RNG_Config(void){
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd(ENABLE);
}

void Start_Sampling(void){

	/* Para cambiar el efecto cuando esta corriendo */
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_EXTI);

	/* Inicializamos el módulo de numeros aleatorios */
	RNG_Config();

	/* Iniciamos los DMA para muestrear la señal */
	TIM_Config();
	ADC_DMA_Config();
	DAC_DMA_Config();
}

/* Cuando se interrumpe se hace el cambio de Buffer */
void DMA2_Stream0_IRQHandler(void){
	mem^=1;
	busy = 1;
	DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
}

/* Interrupcion para cambiar de efecto */
void EXTI0_IRQHandler(void){
	if(EXTI_GetITStatus(USER_BUTTON_EXTI_LINE) != RESET){
		efecto = (efecto + 1)%4;
		/* Clear the Right Button EXTI line pending bit */
		EXTI_ClearITPendingBit(USER_BUTTON_EXTI_LINE);
	}
}


/*************** Efectos del pedal ************************/

/* Overdrive:
 * Hace un Soft-Clipping a la señal de entrada para que esta genere una distorsión
 * a la salida. Para darle forma se utiliza una funcion seccionada.
 *
 * TODO: Agregar los parametros del effecto.
 * 	- Drive: (1 a 30)
 * 	- Gain:  (1 a 10)
 * 	- Offset (0 a 0.25)
 * */

float32_t absf(float32_t sample){
	if(sample < 0)
		return -1*sample;
	else
		return sample;
}

void Overdrive(void){
	//float32_t offset = -0.5f;
	float32_t gain = 250.0f;
	float32_t drive = 98.5f;	/* Varia de 90.0 a 99.9 */
	float32_t a = sin(((drive+1)/101)*(Pi/2.0f));
	float32_t k = 2.0f*a/(1.0f-a);

	for(uint16_t n = 0; n < BLOCK_SIZE; n++){
		BufferOut[mem][n] = (gain*(1+BufferIn[mem][n])*(BufferIn[mem][n]) / (1+k *absf(BufferIn[mem][n])));
		//BufferOut[mem][n] = (1+BufferIn[mem][n])*(BufferIn[mem][n]) / (1+k *absf(BufferIn[mem][n]))/10;
	}
}

/*
 * Trémolo:
 * 	Hace una modulacion en amplitud de la señal entrante.
 * 	TODO: Agregar parametros del efecto:
 * 		- Rate(velocidad) : Que el rate varia de 1 a 20.
 * 		- Depth (Profuncidad)
 * 		- Offset (Offset)
 *
 * */
void Tremolo(void){
	uint16_t speed = 10;
	float32_t gain = 1.2f;
	float32_t offset = 0.01f;

	if(tremolo_control == speed){
		tremolo_control = 0;
		rate+=4;
	} else{
		tremolo_control++;
	}
	for(uint16_t n = 0; n < BLOCK_SIZE; n++){
		if(n < (BLOCK_SIZE/4)){
			Y[n] = X[n]*(gain*SineWave[(rate)%63]+offset);
		} else if(n < (BLOCK_SIZE/2)){
			Y[n] = X[n]*(gain*SineWave[(rate+1)%63]+offset);
		}else if(n < ((3*BLOCK_SIZE)/4)){
			Y[n] = X[n]*(gain*SineWave[(rate+2)%63]+offset);
		} else{
			Y[n] = X[n]*(gain*SineWave[(rate+3)%63]+offset);
		}
	}
}

/*
 * Delay:
 * 	Retrasa la señal un numero n de muestras y provoca un efecto de eco a la salida.
 * */
void Delay(void){
	uint16_t n = 0;


	for(uint16_t i = 0; i < BLOCK_SIZE; i++){
		DelayBuffer[i + ((delay_Buffer_Index)%DELAY_BLOCK_SIZE)] = BufferIn[mem][i];
	}

	for(n = delay_Buffer_Index; n < (delay_Buffer_Index + BLOCK_SIZE); n++){
		BufferOut[mem][n] = DelayBuffer[n];
		n++;
	}

	delay_Buffer_Index = (delay_Buffer_Index + BLOCK_SIZE)%DELAY_BLOCK_SIZE;
}


/*
 * ByPass:
 * 	Deja pasar la señal sin ninguna modificación.
 *
 * */
void ByPass(void){
	arm_copy_f32((float32_t*)&BufferIn[mem][0],(float32_t*)&BufferOut[mem][0], BLOCK_SIZE);
}


/* NoiseGate:
 * 	Reduce el ruido contenido en la señal
 *
 * */

void NoiseGate(void){
	float32_t w = TempFloat[BLOCK_SIZE-1];

	for(uint16_t n = 0; n < BLOCK_SIZE; n++){
		TempFloat[n] = (alpha)*w + (1-alpha)*BufferOut[mem][n];
		w = TempFloat[n];
	}

	arm_copy_f32((float32_t*)&TempFloat[0], (float32_t*)&BufferOut[mem][0], BLOCK_SIZE);
}


