/**
  ******************************************************************************
  * @file    modules/io/pv_module_io.c
  * @author  Martin Vincent Bloedorn
  * @version V1.0.0
  * @date    02-Dezember-2013
  * @brief   Implementação do módulo de gerenciamento de sensores e atuadores.
  ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "pv_module_io.h"

/** @addtogroup ProVANT_Modules
  * @{
  */

/** @addtogroup Module_IO
  * \brief Componentes para atuação e sensoriamento do VANT.
  *
  * Reunião de todos os componentes relacionados às operações de I/O do VANT.
  * Leituras de todos os sensores, comandos para atuadores. O processamento destes
  * dados brutos NÃO é feito neste módulo.
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define MODULE_PERIOD	   10//ms

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
portTickType lastWakeTime;
int  accRaw[3], gyroRaw[3], magRaw[3];
char str[64];

/* Inboxes buffers */
pv_msg_io_actuation    iActuation;

/* Outboxes buffers*/
pv_msg_datapr_attitude oAttitude;
pv_msg_datapr_position oPosition;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions definitions --------------------------------------------*/

/** \brief Inicializacao componentes de IO.
  *
  * Incializa o hardware para comunicar com os sensores e atuadores. Rotinas de teste
  * ainda precisam ser executadas.
  * @param  None
  * @retval None
  */
void module_io_init() {
	/* Inicialização do hardware do módulo */
	c_common_i2c_init(I2C2); //imu
	c_common_i2c_init(I2C1); //esc

	c_common_usart2_init(115200);

	/* Inicializar do sonar */
	c_io_sonar_init();

	/* Inicializar os servos */
	c_io_rx24f_init(1000000);
	c_common_utils_delayms(2);
	c_io_rx24f_setSpeed(1, 20);
	c_io_rx24f_setSpeed(2, 20);
	c_common_utils_delayms(2);
	c_io_rx24f_move(1, 150);
	c_io_rx24f_move(2, 140);
	c_common_utils_delayms(1);
	c_common_utils_delayms(100);

	c_io_imu_init(I2C1);   
	c_io_blctrl_init(I2C2);

	/* Inicialização das filas do módulo. Apenas inboxes (i*!) são criadas! */
	pv_interface_io.iActuation = xQueueCreate(1, sizeof(pv_msg_io_actuation));

	/* Inicializando outboxes em 0 */
	pv_interface_io.oAttitude = 0;
	pv_interface_io.oPosition = 0;

	/* Verificação de criação correta das filas */
	if(pv_interface_io.iActuation == 0) {
		vTraceConsoleMessage("Could not create queue in pv_interface_io!");
		while(1);
	}
}

/** \brief Função principal do módulo de IO.
  * @param  None
  * @retval None
  *
  * Loop que amostra sensores e escreve nos atuadores como necessário.
  *
  */
void module_io_run() 
{
	float accRaw[3], gyrRaw[3], magRaw[3];
	char  ax[16], ay[16], az[16], r[16], p[16], y[16], dr[16], dp[16], dy[16];
	float rpy[] = {0,0,0,0,0,0};

	while(1)
	{
		lastWakeTime = xTaskGetTickCount();

		xQueueReceive(pv_interface_io.iActuation, &iActuation, 0);
		
		#if 1
		// imu data
		taskENTER_CRITICAL();
		c_io_imu_getComplimentaryRPY(rpy);
		taskEXIT_CRITICAL();
		#endif

		/// DEBUG
		#if 1
		c_common_utils_floatToString(rpy[PV_IMU_ROLL  ]*RAD_TO_DEG, r,  4);
		c_common_utils_floatToString(rpy[PV_IMU_PITCH ]*RAD_TO_DEG, p,  4);
		c_common_utils_floatToString(rpy[PV_IMU_YAW   ]*RAD_TO_DEG, y,  4);
		c_common_utils_floatToString(rpy[PV_IMU_DROLL ]*RAD_TO_DEG, dr, 4);
		c_common_utils_floatToString(rpy[PV_IMU_DPITCH]*RAD_TO_DEG, dp, 4);
		c_common_utils_floatToString(rpy[PV_IMU_DYAW  ]*RAD_TO_DEG, dy, 4);
		sprintf(str, "imu -> \t %s \t %s \t %s \t %s \t %s \t %s\n\r", r, p, y, dr, dp, dy);
		c_common_usart_puts(USART2, str);
		#endif

		#if 0
		// control data
		c_common_utils_floatToString(iActuation.servoRight, r,  3);
		c_common_utils_floatToString(iActuation.servoLeft , p,  3);
		c_common_utils_floatToString(iActuation.escRightSpeed, y,  3);
		c_common_utils_floatToString(iActuation.escLeftSpeed , dr, 3);
		sprintf(str, "Control -> \t %s \t %s \t %s \t %s \n\r", r, p, y, dr);
		c_common_usart_puts(USART2, str);
		#endif

		/// SONAR
		#if 0
		c_common_utils_floatToString(c_io_sonar_read(), r,  3);
		sprintf(str, "Distance: %s \n\r",r );
    	c_common_usart_puts(USART2, str);
    	#endif

		/// SERVOS		
		#if 0
		// servo actuation
		taskENTER_CRITICAL();
		if(abs(iActuation.servoRight)<90)
			c_io_rx24f_move(2, 150 - iActuation.servoRight);
		if(abs(iActuation.servoLeft)<90)
			c_io_rx24f_move(1, 130 + iActuation.servoLeft);	
		taskEXIT_CRITICAL();
		#else
		taskENTER_CRITICAL();
		if(abs(iActuation.servoRight*RAD_TO_DEG)<30)
			c_io_rx24f_move(2, 150+iActuation.servoRight*RAD_TO_DEG);
		if(abs(iActuation.servoLeft*RAD_TO_DEG)<30)
			c_io_rx24f_move(1, 130+iActuation.servoLeft*RAD_TO_DEG);	
		taskEXIT_CRITICAL();
		#endif

		#if 0
		if(iActuation.escRightSpeed > 0 && iActuation.escRightSpeed < 10)
		{
			float velo = 395.85*iActuation.escRightSpeed+3223.6; 
			c_io_blctrl_setSpeed(0, velo);
		}
		if(iActuation.escLeftSpeed  > 0 && iActuation.escLeftSpeed  < 10)
		{
			float velo = 395.85*iActuation.escLeftSpeed+3223.6 ;
			c_io_blctrl_setSpeed(1, velo );
		}
		#endif
		
		/// DADOS OUT
		oAttitude.roll     = rpy[PV_IMU_ROLL  ];
		oAttitude.pitch    = rpy[PV_IMU_YAW   ]; 
		oAttitude.yaw      = rpy[PV_IMU_PITCH ];
		oAttitude.dotRoll  = 0;//rpy[PV_IMU_DROLL ];
		oAttitude.dotPitch = 0;//rpy[PV_IMU_DPITCH];
		oAttitude.dotYaw   = 0;//rpy[PV_IMU_DYAW  ];

		if(pv_interface_io.oAttitude != 0)
      		xQueueOverwrite(pv_interface_io.oAttitude, &oAttitude);

		vTaskDelayUntil( &lastWakeTime, (MODULE_PERIOD / portTICK_RATE_MS));
		
	}
}
/* IRQ handlers ------------------------------------------------------------- */

/**
  * @}
  */

/**
  * @}
  */

