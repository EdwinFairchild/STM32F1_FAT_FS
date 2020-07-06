#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_utils.h>
#include <stm32f1xx_ll_spi.h>
#include "rtc_stm32f1.h"
#include "xprintf.h"
#include "ff.h"
#include "diskio.h"

//-----------| NRF Macros / Variables |-----------

#define NRF_SPI			SPI1
#define NRF_CE_PIN		LL_GPIO_PIN_2  //chip enable pin to make the transceiver transmit or listen
#define NRF_CE_PORT		GPIOA
#define NRF_IRQ_PIN		LL_GPIO_PIN_4 // [ interrupt pin active Low ]
#define NRF_IRQ_PORT	GPIOA
#define NRF_CSN_PIN		LL_GPIO_PIN_3 //Low when SPI active
#define NRF_CSN_PORT    GPIOA
// [ SPI : keep at or below 2Mbs ]

#define NRF_CLK_PIN		LL_GPIO_PIN_5
#define NRF_MOSI_PIN	LL_GPIO_PIN_7
#define NRF_MISO_PIN	LL_GPIO_PIN_6

#define NRF_PINS_CLOCK_ENABLE() (RCC->APB2ENR |= RCC_APB2ENR_IOPAEN )  //given the pins are on GPIOA


FATFS FatFs; /* FatFs work area needed for each volume */
FIL Fil; /* File object needed for each open file */



void init_spi1(void);
void setSysClockTo72(void);
int main(void)
{
	const char stringy[] = "Hello world! This is text message written to sd card. For more information, please visit www.studentcompanion.co.za\r\n";
	UINT bw;
	setSysClockTo72();
	LL_InitTick(72000000, 1000);
	disk_initialize(0);
	f_mount(&FatFs, "", 0);	
	f_open(&Fil, "test.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	f_write(&Fil, stringy, 20,0); /* Write data to the file */            
    f_close(&Fil); /* Close the file */

	//Warning: if the line below triggers an error, GPIOC is not connected to a AHB1 (Group 1) on this device.
	//In this case, please search the stm32xxxx_ll_bus.h file for 'PERIPH_GPIOC' to find out the correct
	//macro name and use it to replace LL_AHB1_GRP1_PERIPH_$$com.sysprogs.examples.lBedblink.LEDPORT$$ and LL_AHB1_GRP1_EnableClock() below. 
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);
	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_13, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_SetPinOutputType(GPIOC, LL_GPIO_PIN_13, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinSpeed(GPIOC, LL_GPIO_PIN_13, LL_GPIO_SPEED_FREQ_LOW);


	
	for (;;)
	{
		LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);
		LL_mDelay(50);
		LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_13);
		LL_mDelay(50);
	}
}

void init_spi1(void)
{
	//clock enable GPIOA
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	
	LL_GPIO_InitTypeDef nrfPins;	
	LL_GPIO_StructInit(&nrfPins);	
	
	// CE pin as output : active high/ normally low
	// CSN chip select for spi active low / normally high
	nrfPins.Pin			= NRF_CE_PIN | NRF_CSN_PIN;
	nrfPins.Mode		= LL_GPIO_MODE_OUTPUT;
	nrfPins.OutputType	= LL_GPIO_OUTPUT_PUSHPULL;	
	nrfPins.Speed		= LL_GPIO_SPEED_FREQ_HIGH;
	LL_GPIO_Init(NRF_CE_PORT, &nrfPins);  // CE & CSN on same port only need this once
	
	
	// CLOCK  [ Alt Function ] [ GPIOA ] [ SPI1 ]
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN; 	
	
	// GPIO [ PA5:SCK:output:push ] [ PA6:MISO:input:float/pullup ] [ PA7:MOSI:output:push ]
	LL_GPIO_InitTypeDef spiGPIO;
	LL_GPIO_StructInit(&spiGPIO);
	
	spiGPIO.Pin			= NRF_MOSI_PIN | NRF_CLK_PIN;
	spiGPIO.Mode		= LL_GPIO_MODE_ALTERNATE;
	spiGPIO.OutputType	= LL_GPIO_OUTPUT_PUSHPULL;
	spiGPIO.Speed		= LL_GPIO_SPEED_FREQ_MEDIUM;
	
	LL_GPIO_Init(GPIOA, &spiGPIO);
	
	spiGPIO.Pin		= NRF_MISO_PIN;
	spiGPIO.Mode	= LL_GPIO_MODE_FLOATING;
	spiGPIO.Pull	= LL_GPIO_PULL_UP;
	LL_GPIO_Init(GPIOA, &spiGPIO);
		
	// SPI
	LL_SPI_InitTypeDef mySPI;
	LL_SPI_StructInit(&mySPI);
	
	mySPI.Mode		= LL_SPI_MODE_MASTER;
	mySPI.NSS		= LL_SPI_NSS_SOFT;
	mySPI.BaudRate	= LL_SPI_BAUDRATEPRESCALER_DIV32;

	LL_SPI_Init(NRF_SPI, &mySPI);
	
	LL_SPI_Enable(NRF_SPI);	
	
}

void setSysClockTo72(void)
{


	//turn on external crystal
	RCC->CR |= RCC_CR_HSEON;

	//wait for HSE crystal be stable
	//there are better ways to handle this but ill use simple polling
	while(!(RCC->CR & RCC_CR_HSERDY))
	       ;
 
 
	//activate prefetch buffer but it should already be on
	FLASH->ACR |= FLASH_ACR_PRFTBE;

	// Flash 2 wait state 
	FLASH->ACR &= ~(FLASH_ACR_LATENCY);      //reset just to be sure
	FLASH->ACR |= (uint32_t)0x2;    

 
	//configure RCC and PLL settings while PLL is off
	RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |  RCC_CFGR_PLLMULL);      //reset
 
	RCC->CFGR &= ~(RCC_CFGR_PLLXTPRE);       //PLLXTPRE bit set to 0
	RCC->CFGR |= RCC_CFGR_PLLSRC;       //pll source
	RCC->CFGR |= RCC_CFGR_PLLMULL9;      //pll miultiplier 7 = 28 MHz   9 = 72MHz
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;      //AHB prescaler  2 = 28 MHz   1 = 72MHz
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;      //APB1 presacaler 
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;      //APB2 prescaler
 
  
	//turn on PLL
	RCC->CR |= RCC_CR_PLLON; 
	while (!(RCC->CR & RCC_CR_PLLRDY)) ;
 
	//set pll as clock source
	RCC->CFGR &= ~(RCC_CFGR_SW);      //reset just in case 
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while (!(RCC->CFGR & RCC_CFGR_SWS_PLL)) ;

	SystemCoreClockUpdate();
	uint32_t temp = SystemCoreClock;  //just used to verify
 
}