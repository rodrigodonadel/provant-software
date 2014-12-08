/**
  ******************************************************************************
  * @file    modules/rc/c_rc_receiver.c
  * @author  Martin Vincent Bloedorn
  * @version V1.0.0
  * @date    30-November-2013
  * @brief   Controle de estabilizacao para vôo com usando controle remoto manual.
  ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "c_rc_control.h"

/** @addtogroup Module_RC
  * @{
  */

/** @addtogroup Module_RC_Component_Control
  * \brief Controle de estabilização para o modo de operação RC.
  *
  * O controlador pode ser testado fazendo-se, por exemplo:
  * \code
  * GPIOPin test = c_common_gpio_init(GPIOB, GPIO_Pin_11, GPIO_Mode_OUT);
  *
  * pv_msg_io_actuation   actuation = {0,0.0f,0.0f,0.0f,0.0f};
  * pv_msg_datapr_attitude attitude = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
  * pv_msg_datapr_attitude attitude_reference = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
  * pv_msg_datapr_position position = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
  * pv_msg_datapr_position position_reference = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
  *
  * attitude_reference.roll = 1.0f;
  *
  * while(1) {
  *   	actuation = RC_controller(attitude,
  * 					attitude_reference,
  * 					position,
  * 					position_reference);
  * 	    c_common_gpio_toggle(test);
  * }
  * \endcode
  *
  * A frequência de cálculo pode ser vista então se olhando o pino comutado no osciloscópio.
  * Para o exemplo acima, gira em torno de 80us.
  * @{
  */

		//---------------------------------------------------------------------------------------------

/* Exported functions definitions --------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

//// PID controller gains
//#if 1
//	#define KPPHI    100.0f
//	#define KVPHI    10.0f
//	#define KIPHI	 0.0f
//	#define KPTHETA  100.0f
//	#define KVTHETA  10.0f
//	#define KITHETA	 0.0f
//	#define KPPSI    100.0f
//	#define KVPSI    10.0f
//	#define KIPSI	 0.0f
//	#define KPZ		 18.0f
//	#define KVZ		 6.0f
//#else
//	#define KPPHI    18.0f
//	#define KVPHI    6.0f
//	#define KIPHI	 0.0f
//	#define KPTHETA  6.0f
//	#define KVTHETA  18.0f
//	#define KITHETA	 0.0f
//	#define KPPSI    18.0f
//	#define KVPSI    6.0f
//	#define KIPSI	 0.0f
//	#define KVZ		 6.0f
//	#define KPZ		 18.0f
//#endif
// PID controller gains
#if 1
	#define KPPHI    72.0f
	#define KVPHI    15.0f
	#define KIPHI	 162.0f
	#define KPTHETA  72.0f
	#define KVTHETA  15.0f
	#define KITHETA	 162.0f
	#define KPPSI    18.0f
	#define KVPSI    6.0f
	#define KIPSI	 0.0f
	#define KVZ		 6.0f//6
	#define KPZ		 18.0f//18
#else
	#define KPPHI    18.0f
	#define KVPHI    6.0f
	#define KIPHI	 0.0f
	#define KPTHETA  6.0f
	#define KVTHETA  18.0f
	#define KITHETA	 0.0f
	#define KPPSI    18.0f
	#define KVPSI    6.0f
	#define KIPSI	 0.0f
	#define KVZ		 2.86f
	#define KPZ		 2.0f
#endif

// Environment parameters
#define G   9.81f //gravity

// Antigos valores magicos
//// Aircraft parameters
//#define M    1.672f   // mass kg
//#define L    0.27023f // aircraft's arm length. Y-axis distance between center of rotation(B) and rotor center of mass.
//#define H    0.03349f // center of mass displacement in Z-axis
// Aircraft's Moments of Inertia km*m²
//#define IXX  0.01905797115f
//#define IYY  0.00502396129f
//#define IZZ  0.01859602726f


// Aircraft parameters
//#define M    1.75192733f   // mass kg
#define M    2.3f   // mass kg
#define L    0.24737f // aircraft's arm length. Y-axis distance between center of rotation(B) and rotor center of mass.
//#define H    0.04942921f // center of mass displacement in Z-axis
#define H    0.05394f // center of mass displacement in Z-axis
// Aircraft's Moments of Inertia km*m²
//antigo
//#define IXX  0.00982678f
//#define IYY  0.01008018f
//#define IZZ  0.00979193f

// Sistema Inteiro
#define IXX  0.04794375f
#define IYY  0.01872182f
#define IZZ  0.03666388f

// Somente Corpo 1
//#define IXX  0.02775010f
//#define IYY  0.01759953f
//#define IZZ  0.01754078f

// Reference filter parameters
//#define REFERENCE_FILTER_POLE	40
//#define REFERENCE_FILTER_ZERO	100

// Fixed Sample Time
#define CONTROL_SAMPLE_TIME 	0.01f

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
float32_t gamma_f32[3]={0};
float32_t tau_f32[3]={0};
float32_t attitudeVector_f32[3]={0};
float32_t Mq_f32[3][3]={{0}};
float32_t Cqq_f32[3][3]={{0}};
float32_t Fzb=0;

//float32_t LQR_Ke_f32[4][8]={
//{21.207062455586751, -24.707408251121784,   0.069346468761083,   2.452130918218125,  12.216788626480916,  -4.011929550716840,  0.004475317798479,   0.557055453783041},
//{21.233444539785776,  24.686653404459395,  -0.003353700697260,  -2.445515972875630,  12.233162353241495,   4.011111755550325,  0.005166788138636,  -0.555396333501935},
//{-0.000719994314509,   0.171143370645070,   0.344766866536217,   2.043259478401853,  -0.000386473018984,   0.031030138533026,  0.348169765415673,   0.506637304021608},
//{-0.000523964029942,  -0.167138107269209,   0.375284925986611,  -2.029180987056317,  -0.000281018829228,  -0.030178402624864,  0.345138150637528,  -0.506235008031713}
//};
//float32_t LQR_Ki_f32[4][8]={
//{-18.360708418541652,  74.747291643686452,  -0.082091356993229,  -5.151947191333180},
//{-18.381627935970801, -74.663218767292690,   0.029590624926112,   5.137988576748046},
//{0.000669511614153,   -0.467900642908488,   -4.063031942899512,  -4.032256152392106},
//{0.000487812904700,   0.459532421289280,    -4.048507830710655,   4.047414146624359}
//};
float32_t state_vector_f32[8]={0};
float32_t result_f32[4]={0};

/** \brief Integral do erro dos angulos de orientacao VANT.*/
typedef struct {
	float roll, pitch, yaw;
} c_rc_control_error;

//typedef struct {
//	float servoLeft, servoRight, escRightSpeed, escLeftSpeed;
//} c_rc_control_actuation_signals;

// Variáveis criadas para guardar o erro e a integral do erro
// São globais porque precisamos do erro anterior e integracao anterior.
c_rc_control_error integrated_error={0};
c_rc_control_error error={0};

// Variáveis utilizadas no filtro de referencia
// São globais porque precisamos da entrada anterior e da saida anterior do filtro.
//c_rc_control_actuation_signals last_filtered_actuation_signals={0};
//c_rc_control_actuation_signals last_raw_actuation_signals={0};


/* Private function prototypes -----------------------------------------------*/
float32_t altitude_controller_step(float32_t altitude, float32_t altitude_reference, float32_t rateOfClimb, float32_t rateOfClimb_reference,
									pv_msg_datapr_attitude attitude);
arm_matrix_instance_f32 PD_gains_step(pv_msg_datapr_attitude attitude, pv_msg_datapr_attitude attitude_reference, pv_msg_datapr_sensor_time sensor_time);
arm_matrix_instance_f32 torque_calculation_step(pv_msg_datapr_attitude attitude, arm_matrix_instance_f32 gamma);
pv_msg_io_actuation actuators_signals_step(arm_matrix_instance_f32 tau, float32_t Fzb);
arm_matrix_instance_f32 inertia_matrix(pv_msg_datapr_attitude attitude);
arm_matrix_instance_f32 coriolis_matrix(pv_msg_datapr_attitude attitude);

float integrateTrapezoidal(float last_integration, float current_value, float last_value, float sample_time);
void integrateError(c_rc_control_error current_error, float sample_time);
//pv_msg_io_actuation filtra_sinal_atuadores(c_rc_control_actuation_signals actuators_signals);
//float filtro_primeira_ordem(float signal_raw, float last_signal_filtered, float last_signal_raw, float p, float z, float T);
float32_t Fzb_RC(float32_t signal_RC, pv_msg_datapr_attitude attitude);

//pv_msg_io_actuation LQR(arm_matrix_instance_f32 state_vector);

/* Private functions ---------------------------------------------------------*/

// rateOfClimb (RoC) = aircraft's vertical speed (velocity in Z-axis)
float32_t altitude_controller_step(float32_t altitude, float32_t altitude_reference, float32_t rateOfClimb, float32_t rateOfClimb_reference, pv_msg_datapr_attitude attitude)
{
	float32_t Zeta3=0;

	Zeta3 = -KPZ*(altitude - altitude_reference) - KVZ*(rateOfClimb - rateOfClimb_reference);

	return (M * G + M * Zeta3) / (cos(attitude.roll) * cos(attitude.pitch));
}

/** \brief Calcula o Fzb de acordo com o sinal do radio controle */
float32_t Fzb_RC(float32_t throttle_control, pv_msg_datapr_attitude attitude){
	return (M*G*(0.4+0.6*throttle_control/100)) / (cos(attitude.roll) * cos(attitude.pitch));
}

/** \brief Integral numérica utilizando o método trapezoidal (Tustin) */
float integrateTrapezoidal(float last_integration, float current_value, float last_value, float sample_time){

	return last_integration + sample_time * (current_value + last_value)/2;
}

/** \brief Integra o erro, para ser usado no controle.
 * @param current_error erro calculado nesta iteracão
 * @param sample_time tempo entre uma medicão e outra - Periodo de amostragem variável.
 *
 * A resposta é guardada na variável global integrated_error. Foi utilizada uma variável globla pois precisamos guardar o valor da integral do erro
 * da última iteracão.
 * Outra vairavel global foic riada para guardar o valor do erro atual para a proxima iteracao
 */
void integrateError(c_rc_control_error current_error, float sample_time){

	integrated_error.roll = integrateTrapezoidal(integrated_error.roll, current_error.roll, error.roll, sample_time);
	integrated_error.pitch =integrateTrapezoidal(integrated_error.pitch, current_error.pitch, error.pitch, sample_time);
	integrated_error.yaw =integrateTrapezoidal(integrated_error.yaw, current_error.yaw, error.yaw, sample_time);

	/* guarda o valor do erro para a proxima iteracao */
	error.roll  = current_error.roll;
	error.pitch = current_error.pitch;
	error.yaw   = current_error.yaw;
}


arm_matrix_instance_f32 PD_gains_step(pv_msg_datapr_attitude attitude, pv_msg_datapr_attitude attitude_reference,
										pv_msg_datapr_sensor_time sensor_time){
	arm_matrix_instance_f32 gamma;
	c_rc_control_error current_error={0};

	current_error.roll = attitude.roll - attitude_reference.roll;
	current_error.pitch = attitude.pitch - attitude_reference.pitch;
	current_error.yaw = attitude.yaw - attitude_reference.yaw;

	/* Integra o erro, utiliza as variaveis globais:
	 * integrated_error;
	 * error
	 * Já guarda o valor do erro para a proxima iteracao
	 */
//	integrateError(current_error, sensor_time.IMU_sample_time);
	integrateError(current_error, sensor_time.IMU_sample_time);

	//gamma1
	gamma_f32[0] = -KPPHI * (attitude.roll - attitude_reference.roll)
						- KVPHI * (attitude.dotRoll - attitude_reference.dotRoll)
						- KIPHI * integrated_error.roll;
	//gamma2
	gamma_f32[1] = -KPTHETA * (attitude.pitch - attitude_reference.pitch)
						- KVTHETA * (attitude.dotPitch - attitude_reference.dotPitch)
						- KITHETA * integrated_error.pitch;

	//gamma3
	gamma_f32[2] = -KPPSI * (attitude.yaw - attitude_reference.yaw)
						- KVPSI * (attitude.dotYaw - attitude_reference.dotYaw)
						- KIPSI * integrated_error.yaw;

	arm_mat_init_f32(&gamma, 3, 1, (float32_t *)gamma_f32);

	return gamma;
}

arm_matrix_instance_f32 torque_calculation_step(pv_msg_datapr_attitude attitude, arm_matrix_instance_f32 gamma)
{
	float32_t r1_f32[3]={0}; // used as intermediate variable for matrices operations
	float32_t r2_f32[3]={0}; // used as intermediate variable for matrices operations
	arm_matrix_instance_f32 tau;
	arm_matrix_instance_f32 attitudeVector;
	arm_matrix_instance_f32 r1;
	arm_matrix_instance_f32 r2;
	arm_matrix_instance_f32 cor_att;
	arm_matrix_instance_f32 inr_att;

	arm_mat_init_f32(&r1, 3, 1, (float32_t *)r1_f32);
	arm_mat_init_f32(&r2, 3, 1, (float32_t *)r2_f32);

	attitudeVector_f32[0]= attitude.dotRoll;
	attitudeVector_f32[1]= attitude.dotPitch;
	attitudeVector_f32[2]= attitude.dotYaw;

	arm_mat_init_f32(&tau, 3, 1, (float32_t *)tau_f32);
	arm_mat_init_f32(&attitudeVector, 3, 1, (float32_t *)attitudeVector_f32);

	/*tau = ( inertia_matrix(pv_msg_datapr_attitude attitude) * gamma )
	+ ( coriolis_matrix(pv_msg_datapr_attitude attitude) * dotTheta );*/
	inr_att = inertia_matrix(attitude);
	cor_att = coriolis_matrix(attitude);

	arm_mat_mult_f32(&inr_att, &gamma, &r1);
	arm_mat_mult_f32(&cor_att, &attitudeVector, &r2);
	arm_mat_add_f32(&r1, &r2, &tau);

	return tau;
	/* tau[0] = tauRoll
	   tau[1] = tauPitch
	   tau[2] = tauYaw */
}

pv_msg_io_actuation actuators_signals_step(arm_matrix_instance_f32 tau, float32_t Fzb)
{
	/* tau[0] = tauRoll
	   tau[1] = tauPitch
	   tau[2] = tauYaw */

	pv_msg_io_actuation actuation;
	float32_t * tauData = tau.pData;

	actuation.escRightSpeed = 0.5 * sqrt(
				pow((tauData[1] / H + tauData[2] / L), 2)
				+ pow((-Fzb + tauData[0] / L), 2));

	actuation.escLeftSpeed = 0.5 * sqrt(
				pow((-tauData[1] / H + tauData[2] / L), 2)
				+ pow((Fzb + tauData[0] / L), 2));

	actuation.servoRight = atan2(
				(tauData[1] / H + tauData[2] / L)
				, (Fzb - tauData[0] / L));

	actuation.servoLeft = atan2(
				(tauData[1] / H - tauData[2] / L)
				, (Fzb + tauData[0] / L));

	// Declares that the servos will use angle control, rather than torque control
	actuation.servoTorqueControlEnable = 0;

	return actuation;
}

// Inertia matrix
arm_matrix_instance_f32 inertia_matrix(pv_msg_datapr_attitude attitude)
{
	arm_matrix_instance_f32 Mq;

	Mq_f32[0][0] = IXX;
	Mq_f32[0][1] = 0;
	Mq_f32[0][2] = -IXX * sin(attitude.pitch);
	Mq_f32[1][0] = 0;
	//Mq_f32[1][1] = IYY * pow(cos(attitude.roll), 2) + IZZ * pow(sin(attitude.roll), 2);
	Mq_f32[1][1] = IYY * (cos(attitude.roll)*cos(attitude.roll)) + IZZ * (sin(attitude.roll)*sin(attitude.roll));
	Mq_f32[1][2] = (IYY - IZZ) * cos(attitude.roll) * sin(attitude.roll)* cos(attitude.pitch);
	Mq_f32[2][0] = - IXX * sin(attitude.pitch);
	Mq_f32[2][1] = (IYY - IZZ) * cos(attitude.roll) * sin(attitude.roll)* cos(attitude.pitch);
	Mq_f32[2][2] = IXX * pow(sin(attitude.pitch), 2)
			+ IYY * pow(sin(attitude.roll), 2)
			* pow(cos(attitude.pitch), 2)
			+ IZZ * pow(cos(attitude.roll), 2)
			* pow(cos(attitude.pitch), 2);

	arm_mat_init_f32(&Mq, 3, 3, (float32_t *)Mq_f32);

	return Mq;
}

/*	Coriolis and Centrifugal Matrix = C(THETA,dotTHETA) - Cqq - obtained by
*  christoffel symbols of the first kind page 40 of "ROBUST CONTROL STRATEGIES
*  FOR A QUADROTOR HELICOPTER - An Underactuated Mechanical System" by Raffo G.
*  */
arm_matrix_instance_f32 coriolis_matrix(pv_msg_datapr_attitude attitude)
{
	arm_matrix_instance_f32 Cqq;

	Cqq_f32[0][0] = 0;

	Cqq_f32[0][1] = (IYY - IZZ)
			* (attitude.dotPitch * cos(attitude.roll)
			* sin(attitude.roll)
			+ 0.5 * attitude.dotYaw
			* pow(sin(attitude.roll), 2)
			* cos(attitude.pitch))
			+ 0.5 * (IZZ - IYY) * attitude.dotYaw
			* pow(cos(attitude.roll), 2)
			* cos(attitude.pitch)
			- 0.5 * IXX * attitude.dotYaw * cos(attitude.pitch);

	Cqq_f32[0][2] = (IZZ - IYY)
			* (attitude.dotYaw * cos(attitude.roll)
			* sin(attitude.roll)
			* pow(cos(attitude.pitch), 2)
			+ 0.5 * attitude.dotPitch * cos(attitude.pitch)
			* pow(cos(attitude.roll), 2))
			+ 0.5 * (IYY - IZZ) * attitude.dotPitch
			* cos(attitude.pitch)
			* pow(sin(attitude.roll), 2)
			- 0.5 * IXX * attitude.dotPitch * cos(attitude.pitch);

	Cqq_f32[1][0] = (IZZ - IYY)
			* (attitude.dotPitch * cos(attitude.roll)
			* sin(attitude.roll)
			+ 0.5 * attitude.dotYaw
			* pow(sin(attitude.roll), 2)
			* cos(attitude.pitch))
			+ 0.5 * (IYY - IZZ)
			* (attitude.dotYaw * pow(cos(attitude.roll), 2)
			* cos(attitude.pitch))
			+ 0.5 * IXX * attitude.dotYaw * cos(attitude.pitch);

	Cqq_f32[1][1] = (IZZ - IYY) * attitude.dotRoll * cos(attitude.roll)
			* sin(attitude.roll);

	Cqq_f32[1][2] = -IXX * attitude.dotYaw * sin(attitude.pitch)
			* cos(attitude.pitch)
			+ IYY * attitude.dotYaw * pow(sin(attitude.roll), 2)
			* cos(attitude.pitch) * sin(attitude.pitch)
			+ IZZ * attitude.dotYaw * pow(cos(attitude.roll), 2)
			* sin(attitude.pitch) * cos(attitude.pitch)
			+ 0.5 * (IYY - IZZ) * attitude.dotRoll
			* cos(attitude.pitch)
			* pow(cos(attitude.roll), 2)
			+ 0.5 * (IZZ - IYY) * attitude.dotRoll
			* cos(attitude.pitch)
			* pow(sin(attitude.roll), 2)
			+ 0.5 * IXX * attitude.dotRoll * cos(attitude.pitch);

	Cqq_f32[2][0] = (IYY - IZZ)
			* (attitude.dotYaw * pow(cos(attitude.pitch), 2)
			* sin(attitude.roll) * cos(attitude.roll)
			+ 0.5 * attitude.dotPitch * cos(attitude.pitch)
			* pow(cos(attitude.roll), 2))
			+ 0.5 * (IZZ - IYY) * attitude.dotPitch
			* cos(attitude.pitch)
			* pow(sin(attitude.roll), 2)
			- 0.5 * IXX * attitude.dotPitch * cos(attitude.pitch);

	Cqq_f32[2][1] = (IZZ - IYY)
			* (attitude.dotPitch * cos(attitude.roll)
			* sin(attitude.roll) * sin(attitude.pitch)
			+ 0.5 * attitude.dotRoll
			* pow(sin(attitude.roll), 2)
			* cos(attitude.pitch))
			+ 0.5 * (IYY - IZZ) * attitude.dotRoll
			* pow(cos(attitude.roll), 2)
			* cos(attitude.pitch)
			- 0.5 * IXX * attitude.dotRoll * cos(attitude.pitch)
			+ IXX * attitude.dotYaw * sin(attitude.pitch)
			* cos(attitude.pitch)
			- IYY * attitude.dotYaw * pow(sin(attitude.roll), 2)
			* sin(attitude.pitch) * cos(attitude.pitch)
			- IZZ * attitude.dotYaw * pow(cos(attitude.roll), 2)
			* sin(attitude.pitch) * cos(attitude.pitch);

	Cqq_f32[2][2] = (IYY - IZZ) * attitude.dotRoll * cos(attitude.roll)
			* sin(attitude.roll) * pow(cos(attitude.pitch), 2)
			- IYY * attitude.dotPitch * pow(sin(attitude.roll), 2)
			* cos(attitude.pitch) * sin(attitude.pitch)
			- IZZ * attitude.dotPitch * pow(cos(attitude.roll), 2)
			* cos(attitude.pitch) * sin(attitude.pitch)
			+ IXX * attitude.dotPitch * cos(attitude.pitch)
			* sin(attitude.pitch);

	arm_mat_init_f32(&Cqq, 3, 3, (float32_t *)Cqq_f32);

	return Cqq;
}

/** \brief Implementa um filtro passa baixa do tipo:
 * (p/z)*(s+z)/(s+p)
 * discretizado com Tustin
 *
 * @param signal_raw - sinal a ser filtrado
 * @param p - polo p do filtro
 * @param z - zero z do filtro
 * @param T - Periodo de amostragem (sample time) do sinal
 */
//float filtro_primeira_ordem(float signal_raw, float last_signal_filtered, float last_signal_raw, float p, float z, float T){
//
//	float a, b, c;
//
//	a = (2-p*T)/(2+p*T);
//	b = (p/z)*(2+z*T)/(2+p*T);
//	c = (p/z)*(z*T-2)/(2+p*T);
//
//	return (a*last_signal_filtered + b*signal_raw + c*last_signal_raw);
//}

//c_rc_control_actuation_signals last_filtered_actuation_signals;
//c_rc_control_actuation_signals last_raw_actuation_signals;
/** \brief Filtra o sinal vindo dos atuadores
 *  Utiliza as variaveis globais:
 *  - last_filtered_actuation_signals;
 *  - last_raw_actuation_signal.
 *
 *  @param actuators_signals - sinal raw saido do controle do Chowdhurry
 */
//pv_msg_io_actuation filtra_sinal_atuadores(c_rc_control_actuation_signals actuators_signals){
//
//	pv_msg_io_actuation actuation_filtered;
//
//	actuation_filtered.servoRight =  filtro_primeira_ordem(actuators_signals.servoRight, last_filtered_actuation_signals.servoRight,
//															last_raw_actuation_signals.servoRight, REFERENCE_FILTER_POLE, REFERENCE_FILTER_ZERO,
//															CONTROL_SAMPLE_TIME );
//	actuation_filtered.servoLeft = filtro_primeira_ordem(actuators_signals.servoLeft, last_filtered_actuation_signals.servoLeft,
//															last_raw_actuation_signals.servoLeft, REFERENCE_FILTER_POLE, REFERENCE_FILTER_ZERO,
//															CONTROL_SAMPLE_TIME );
////	actuation_filtered.escRightSpeed = filtro_primeira_ordem(actuators_signals.escRightSpeed, last_filtered_actuation_signals.escRightSpeed,
////																last_raw_actuation_signals.escRightSpeed, REFERENCE_FILTER_POLE, REFERENCE_FILTER_ZERO,
////																CONTROL_SAMPLE_TIME );
////	actuation_filtered.escLeftSpeed = filtro_primeira_ordem(actuators_signals.escLeftSpeed, last_filtered_actuation_signals.escLeftSpeed,
////																last_raw_actuation_signals.escLeftSpeed, REFERENCE_FILTER_POLE, REFERENCE_FILTER_ZERO,
////																CONTROL_SAMPLE_TIME );
//
//	actuation_filtered.escRightSpeed = actuators_signals.escRightSpeed;
//	actuation_filtered.escLeftSpeed = actuators_signals.escLeftSpeed;
//
//	// Guarda os valores para a proxima iteracao
//	last_filtered_actuation_signals.servoRight = actuation_filtered.servoRight;
//	last_filtered_actuation_signals.servoLeft  = actuation_filtered.servoLeft;
//	last_filtered_actuation_signals.escRightSpeed = actuation_filtered.escRightSpeed;
//	last_filtered_actuation_signals.escLeftSpeed = actuation_filtered.escLeftSpeed;
//	last_raw_actuation_signals = actuators_signals;
//
//	return actuation_filtered;
//}

//pv_msg_io_actuation LQR(arm_matrix_instance_f32 state_vector){
//
//arm_matrix_instance_f32 Ke;
//arm_matrix_instance_f32 result;
////float32_t * actuation_f32;
//pv_msg_io_actuation actuation;
//
//arm_mat_init_f32(&Ke, 4, 8, (float32_t *)LQR_Ke_f32);
//arm_mat_init_f32(&result, 4, 1, (float32_t *)result_f32);
//
//arm_mat_mult_f32(&Ke, &state_vector, &result);
//
////actuation_f32 = result.pData;
//actuation.escRightSpeed =result.pData[0]+8.52937;
//actuation.escLeftSpeed =result.pData[1]+8.55244;
//actuation.servoRight =result.pData[2]+0.0791888;
//actuation.servoLeft =result.pData[3]+0.0789936;
//
////[0.852937E1,0.855244E1,0.791888E-1,0.789936E-1]
//
//}

/* Exported functions definitions --------------------------------------------*/

/** \brief Inicilização do controle de estabilidade.
 *
 * O controlador utilza a API de DSP da CMSIS, e portanto se baseia fortemente no uso do
 * tipo arm_matrix_instance_f32. Esta \b struct contêm os valores de número de linhas e
 * colunas de matriz, além de um ponteiro para seus elementos (na forma de array).
 * Estes arrays são prealocados globalmente (ver código fonte), para evitar overhead
 * de alocação dinâmica em cada chamada e para evitar que, a cada alocação em uma função, a memória para
 * a qual o ponteiro aponta saia de escopo e seja deletada. Uma vez que as funções são privadas e chamadas
 * em ordem determinística, mutexes não são implementadas (por simplicidade apenas)
 */
void c_rc_control_init() {

}

/* \brief Controlador.
 *
 * Implemented based on the article "Back-stepping Control Strategy for Stabilization of a Tilt-rotor UAV" by Chowdhury, A. B., with some modifications.
 * It implements an estabilization controller and an altitude controller. It is meant to be used with the radio controller.
 * The struct pv_msg_io_attitude includes the angular velocity.
 */
pv_msg_io_actuation RC_controller(pv_msg_datapr_attitude attitude,
				  pv_msg_datapr_attitude attitude_reference,
				  pv_msg_datapr_position position,
				  pv_msg_datapr_position position_reference,
				  pv_msg_datapr_sensor_time sensor_time,
				  int throttle_control) {

	arm_matrix_instance_f32 gamma;
	arm_matrix_instance_f32 tau;
	pv_msg_io_actuation actuation_signals;

	arm_matrix_instance_f32 state_vector;

//	state_vector_f32[0]=position.z-position_reference.z;
//	state_vector_f32[1]=attitude.roll;
//	state_vector_f32[2]=attitude.pitch+0.0791f;
//	state_vector_f32[3]=attitude.yaw;
//	state_vector_f32[4]=position.dotZ;
//	state_vector_f32[5]=attitude.dotRoll;
//	state_vector_f32[6]=attitude.dotPitch;
//	state_vector_f32[7]=attitude.dotYaw;

//	arm_mat_init_f32(&state_vector, 8, 1, (float32_t *)state_vector_f32);
//
//	actuation_signals = LQR(state_vector);

	gamma = PD_gains_step(attitude, attitude_reference, sensor_time);

	tau = torque_calculation_step(attitude, gamma);

	Fzb = altitude_controller_step(position.z, position_reference.z, position.dotZ, position_reference.dotZ, attitude);

	actuation_signals = actuators_signals_step(tau, Fzb);

	 Declares that the servos will use angle control, rather than torque control
	filtered_actuators_signals.servoTorqueControlEnable = 0;

	return actuation_signals;
}

/* IRQ handlers ------------------------------------------------------------- */

/**
  * @}
  */

/**
  * @}
  */

