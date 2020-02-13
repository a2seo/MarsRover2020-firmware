
#include "ActuatorController.h"
#include <cmath>

ActuatorController::ActuatorController(t_actuatorConfig actuatorConfig, 
									   Motor &motor, Encoder &encoder, 
									   DigitalIn &limSwitchMin, 
									   DigitalIn &limSwitchMax) :

		m_actuatorConfig(actuatorConfig), r_motor(motor), r_encoder(encoder), 
		r_limSwitchMin(limSwitchMin), r_limSwitchMax(limSwitchMax),
		m_velocityPIDController(actuatorConfig.velocityPID.P, actuatorConfig.velocityPID.I, actuatorConfig.velocityPID.D, 0.0),
		m_positionPIDController(actuatorConfig.positionPID.P, actuatorConfig.positionPID.I, actuatorConfig.positionPID.D, 0.0) {

	m_limSwitchMin_Connected = (r_limSwitchMin != NULL_DIGITAL_IN && r_limSwitchMin.is_connected());
	m_limSwitchMax_Connected = (r_limSwitchMax != NULL_DIGITAL_IN && r_limSwitchMax.is_connected());

	initializePIDControllers();
	updateTimer.start();

	setControlMode(actuatorConfig.defaultControlMode);
}

ActuatorController::t_actuatorControlMode ActuatorController::getControlMode() {
	return m_controlMode;
}

float ActuatorController::getMotorPower_Percentage() {
	return r_motor.getPower();
}

float ActuatorController::getVelocity_DegreesPerSec() {
	return r_encoder.getVelocity_DegreesPerSec();
}

float ActuatorController::getAngle_Degrees() {
	return r_encoder.getAngle_Degrees();
}

mbed_error_status_t ActuatorController::setControlMode(t_actuatorControlMode controlMode) {

	m_controlMode = controlMode;

	 switch (controlMode) {

        case motorPower:
            m_controlMode = motorPower;
            setMotorPower_Percentage(0.0f);
            return MBED_SUCCESS;

        case velocity:
            m_velocityPIDController.reset();
            m_controlMode = velocity;
            return setVelocity_DegreesPerSec(0.0f);

        case position:
            m_positionPIDController.reset();
            m_controlMode = position;
            return setAngle_Degrees(getAngle_Degrees());

        default:
            return MBED_ERROR_INVALID_ARGUMENT;
    }
}
	
mbed_error_status_t ActuatorController::setMotorPower_Percentage(float percentage) {
	if (m_controlMode != motorPower) {
		return MBED_ERROR_INVALID_OPERATION;
	}

	if ((percentage < 0.0 && isLimSwitchMinTriggered()) ||
		(percentage > 0.0 && isLimSwitchMaxTriggered())) {
		
		percentage = 0.0;
	}

	r_motor.setPower(percentage);

	return MBED_SUCCESS;
}

mbed_error_status_t ActuatorController::setVelocity_DegreesPerSec(float degreesPerSec) {
	if (m_controlMode != velocity) {
		return MBED_ERROR_INVALID_OPERATION;
	}

	if ( (degreesPerSec < 0.0 && isPastMinAngle()) ||
		 (degreesPerSec > 0.0 && isPastMaxAngle()) ) {

		degreesPerSec = 0.0;
	}

	m_velocityPIDController.setSetPoint(degreesPerSec);

	return MBED_SUCCESS;
}

mbed_error_status_t ActuatorController::setAngle_Degrees(float degrees) {
	if (m_controlMode != position) {
		return MBED_ERROR_INVALID_OPERATION;
	}

	if (degrees <= m_actuatorConfig.minAngle_Degrees) {
		degrees = m_actuatorConfig.minAngle_Degrees;
	}

	else if (degrees >= m_actuatorConfig.maxAngle_Degrees) {
		degrees = m_actuatorConfig.maxAngle_Degrees;
	}

	m_positionPIDController.setSetPoint(degrees);

	return MBED_SUCCESS;

}

void ActuatorController::update() {
	float updateInterval = updateTimer.read();
	updateTimer.reset();

	switch (m_controlMode) {
		case motorPower:
			if ( (r_motor.getPower() < 0.0 && isLimSwitchMinTriggered()) ||
				 (r_motor.getPower() > 0.0 && isLimSwitchMaxTriggered()) ) {

				r_motor.setPower(0.0);
			}

			break;

		case velocity:
			m_velocityPIDController.setInterval(updateInterval);
			m_velocityPIDController.setProcessValue(getVelocity_DegreesPerSec());
			r_motor.setPower(m_velocityPIDController.compute());

			break;

		case position:
			m_positionPIDController.setInterval(updateInterval);
			m_positionPIDController.setProcessValue(getAngle_Degrees());
			r_motor.setPower(m_positionPIDController.compute());

			break;
	}
}

void ActuatorController::initializePIDControllers() {

	// Configure velocity PID
	m_velocityPIDController.setInputLimits(m_actuatorConfig.minVelocity_DegreesPerSec, m_actuatorConfig.maxVelocity_DegreesPerSec);
	m_velocityPIDController.setOutputLimits(m_actuatorConfig.minMotorPower_Percentage, m_actuatorConfig.maxMotorPower_Percentage);
	m_velocityPIDController.setBias(m_actuatorConfig.velocityPID.bias);
	m_velocityPIDController.setMode(PID_AUTO_MODE);
	m_velocityPIDController.setDeadZoneError(m_actuatorConfig.velocityPID.deadZoneError);

	// Configure position PID
	m_positionPIDController.setInputLimits(m_actuatorConfig.minAngle_Degrees, m_actuatorConfig.maxAngle_Degrees);
	m_positionPIDController.setOutputLimits(m_actuatorConfig.minMotorPower_Percentage, m_actuatorConfig.maxMotorPower_Percentage);
	m_positionPIDController.setBias(m_actuatorConfig.positionPID.bias);
	m_positionPIDController.setMode(PID_AUTO_MODE);
	m_positionPIDController.setDeadZoneError(m_actuatorConfig.positionPID.deadZoneError);
}

bool ActuatorController::isLimSwitchMinTriggered() {
	return m_limSwitchMin_Connected && r_limSwitchMin.read() == 1;
}

bool ActuatorController::isLimSwitchMaxTriggered() {
	return m_limSwitchMax_Connected && r_limSwitchMax.read() == 1;
}

bool ActuatorController::isPastMinAngle() {
	return (getAngle_Degrees() < m_actuatorConfig.minAngle_Degrees || isLimSwitchMinTriggered());
}

bool ActuatorController::isPastMaxAngle() {
	return (getAngle_Degrees() > m_actuatorConfig.maxAngle_Degrees || isLimSwitchMaxTriggered());
}