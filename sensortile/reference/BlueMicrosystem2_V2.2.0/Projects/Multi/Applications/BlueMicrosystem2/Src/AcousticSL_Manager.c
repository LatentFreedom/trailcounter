/**
 ******************************************************************************
 * @file    AcousticSL_Manager.c
 * @author  Central LAB
 * @version V2.2.0
 * @date    23-December-2016
 * @brief   This file includes Source location interface functions
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "TargetFeatures.h"

#ifdef OSX_BMS_ACOUSTIC_SOURCE_LOCALIZATION
#include "../../../../../Middlewares/ST/STM32_OSX_AcousticSL_Library/osx_license.h"


/* Volatile variables and its initialization --------------------------------------*/
volatile int32_t SourceLocationToSend=  OSX_ACOUSTIC_SL_NO_AUDIO_DETECTED;

/* Exported variables -------------------------------------------------------------*/
/*Handler and Config structure for Source Localization*/
osx_AcousticSL_Handler_t libSoundSourceLoc_Handler_Instance;
osx_AcousticSL_Config_t  libSoundSourceLoc_Config_Instance;

/* Imported Variables -------------------------------------------------------------*/
#ifdef USE_STM32F4XX_NUCLEO
extern uint16_t PDM_Buffer[];
#endif /* USE_STM32F4XX_NUCLEO */

extern uint16_t PCM_Buffer[];

/* Private function prototypes -----------------------------------------------*/
static void SW_Task2_Start(void);

/**
  * @brief  Initialize AcousticSL License
  * @param  MDM_PayLoadLic_t *PayLoad Pointer to the osx License MetaData
  * @retval None
  */
void AcousticSL_License_init(MDM_PayLoadLic_t *PayLoad)
{
  MCR_OSX_COPY_LICENSE_TO_MDM(osx_asl_license,PayLoad->osxLicense);
  
  if(osx_AcousticSL_Initialize()) {
    OSX_BMS_PRINTF("Error AcousticSL License authentication \n\r");
    while(1) {
      ;
    }
  } else {
	osx_AcousticSL_GetLibVersion(PayLoad->osxLibVersion);
	OSX_BMS_PRINTF("Enabled %s\n\r",PayLoad->osxLibVersion);
    if(PayLoad->osxLicenseInitialized==0) {
      NecessityToSaveMetaDataManager=1;
      PayLoad->osxLicenseInitialized=1;
    }   
  }
}

/**
* @brief  Initialises AcousticSL algorithm
* @param  None
* @retval None
*/
void AcousticSL_Manager_init(void)
{

  /*Setup Source Localization static parameters*/
  libSoundSourceLoc_Handler_Instance.channel_number= AUDIO_CHANNELS;

#ifdef STM32_NUCLEO  
  libSoundSourceLoc_Handler_Instance.M12_distance= SIDE;
#endif /* STM32_NUCLEO */
    
  libSoundSourceLoc_Handler_Instance.sampling_frequency= AUDIO_SAMPLING_FREQUENCY;  
  libSoundSourceLoc_Handler_Instance.algorithm= OSX_ACOUSTIC_SL_ALGORITHM_GCCP;
  libSoundSourceLoc_Handler_Instance.ptr_M1_channels= AUDIO_CHANNELS;
  libSoundSourceLoc_Handler_Instance.ptr_M2_channels= AUDIO_CHANNELS;
  libSoundSourceLoc_Handler_Instance.ptr_M3_channels= AUDIO_CHANNELS; 
  libSoundSourceLoc_Handler_Instance.ptr_M4_channels= AUDIO_CHANNELS;
  libSoundSourceLoc_Handler_Instance.samples_to_process = 512;  /* for new SL Version */

  osx_AcousticSL_getMemorySize( &libSoundSourceLoc_Handler_Instance);  
  libSoundSourceLoc_Handler_Instance.pInternalMemory=(uint32_t *)malloc(libSoundSourceLoc_Handler_Instance.internal_memory_size);
  
  if(libSoundSourceLoc_Handler_Instance.pInternalMemory == NULL)
  {
    OSX_BMS_PRINTF("Error AcousticSL Memory allocation\n\r");
    return;
  }
  
  if(osx_AcousticSL_Init( &libSoundSourceLoc_Handler_Instance))
  {
    OSX_BMS_PRINTF("Error AcousticSL Initialization\n\r");
    return;
  }

  /*Setup Source Localization dynamic parameters*/  
  libSoundSourceLoc_Config_Instance.resolution= ANGLE_RESOLUTION;
  libSoundSourceLoc_Config_Instance.threshold=  MIC_LEVEL_THRESHOLD;

  if(osx_AcousticSL_setConfig(&libSoundSourceLoc_Handler_Instance, &libSoundSourceLoc_Config_Instance))
  {
    OSX_BMS_PRINTF("Error AcousticSL Configuration\n\r");
    return;
  }
     
  /* If everything is ok */
  TargetBoardFeatures.osxAcousticSLIsInitalized=1;
  OSX_BMS_PRINTF("Initialized osxAcousticSL (%ld bytes allocated)\r\n", libSoundSourceLoc_Handler_Instance.internal_memory_size);
 
  HAL_NVIC_SetPriority((IRQn_Type)EXTI2_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ((IRQn_Type)EXTI2_IRQn);

  return;
}

/**
* @brief  User function that is called when 1 ms of PDM data is available.
*         In this application only PDM to PCM conversion and USB streaming
*         is performed.
*         User can add his own code here to perform some DSP or audio analysis.
* @param  none
* @retval None
*/
void AudioProcess_SL(void)
{
  if(osx_AcousticSL_Data_Input((int16_t *)&PCM_Buffer[M1_EXT_B],
                               (int16_t *)&PCM_Buffer[M4_EXT_B],
                               NULL,
                               NULL,
                               &libSoundSourceLoc_Handler_Instance))
  {
    /*Localization Processing Task*/
    SW_Task2_Start(); 
  }
}

/**
* @brief Throws Highest priority interrupt
* @param  None
* @retval None
*/
void SW_Task2_Start(void)
{
  HAL_NVIC_SetPendingIRQ(EXTI2_IRQn);
}

/**
* @brief Lower priority interrupt handler routine
* @param  None
* @retval None
*/
void  SW_Task2_Callback(void) 
{
  int32_t result=0;
  
  if(osx_AcousticSL_Process((int32_t *)&result, &libSoundSourceLoc_Handler_Instance))
  {
    OSX_BMS_PRINTF("osx_AcousticSL_Process error\r\n");
    return;
  }
  
  if(result != OSX_ACOUSTIC_SL_NO_AUDIO_DETECTED)
  {
    SourceLocationToSend= (int16_t)result;
    SourceLocationToSend= SourceLocationToSend + 90;
  }
}

#endif /* OSX_BMS_ACOUSTIC_SOURCE_LOCALIZATION */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
