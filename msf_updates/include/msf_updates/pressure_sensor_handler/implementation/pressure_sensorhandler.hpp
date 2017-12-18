/*
 * Copyright (C) 2012-2013 Simon Lynen, ASL, ETH Zurich, Switzerland
 * You can contact the author at <slynen at ethz dot ch>
 * Copyright (C) 2011-2012 Stephan Weiss, ASL, ETH Zurich, Switzerland
 * You can contact the author at <stephan dot weiss at ieee dot org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <msf_core/eigen_utils.h>
#ifndef PRESSURE_SENSORHANDLER_HPP_
#define PRESSURE_SENSORHANDLER_HPP_

namespace msf_pressure_sensor {
PressureSensorHandler::PressureSensorHandler(
    msf_core::MSF_SensorManager<msf_updates::EKFState>& meas,
    std::string topic_namespace, std::string parameternamespace)
    : SensorHandler<msf_updates::EKFState>(meas, topic_namespace,
                                           parameternamespace),
      n_zp_(1e-6) {
  ros::NodeHandle pnh("~/pressure_sensor");
  ros::NodeHandle nh("msf_updates");

  pnh.param("enable_mah_outlier_rejection", enable_mah_outlier_rejection_, false);
  pnh.param("mah_threshold", mah_threshold_, msf_core::kDefaultMahThreshold_);
  mah_threshold_base_=mah_threshold_;
  pnh.param("mah_threshold_limit", mah_threshold_limit_, msf_core::kDefaultMahThresholdLimit_);
  pnh.param("mah_rejection_modification", mah_rejection_modification_, msf_core::kDefaultMahRejectionModification_);
  pnh.param("mah_acceptance_modification", mah_acceptance_modification_, msf_core::kDefaultMahAcceptanceModification_);
  
  
    if(enable_mah_outlier_rejection_)
  {
	  MSF_INFO_STREAM("Pressure sensor is using outlier rejection with initial threshold: " <<
	  mah_threshold_ << ", rejection modificator: " << mah_rejection_modification_ <<
	  ", acceptance modificator: " << mah_acceptance_modification_ <<
	  " and reset limit: "<< mah_threshold_limit_);
  }
  
  
  subPressure_ =
      nh.subscribe<geometry_msgs::PointStamped>
      ("pressure_height", 20, &PressureSensorHandler::MeasurementCallback, this);

  memset(heightbuff, 0, sizeof(double) * heightbuffsize);

}

void PressureSensorHandler::SetNoises(double n_zp) {
  n_zp_ = n_zp;
}

void PressureSensorHandler::MeasurementCallback(
    const geometry_msgs::PointStampedConstPtr & msg) {

  received_first_measurement_ = true;

  this->SequenceWatchDog(msg->header.seq, subPressure_.getTopic());
  MSF_INFO_STREAM_ONCE(
      "*** pressure sensor got first measurement from topic "
          << this->topic_namespace_ << "/" << subPressure_.getTopic()
          << " ***");

  bool throttle = true;
  if (throttle && msg->header.seq % 10 != 0) {
    return;
  }
  shared_ptr<pressure_measurement::PressureMeasurement> meas(
      new pressure_measurement::PressureMeasurement(
          n_zp_, true, this->sensorID, enable_mah_outlier_rejection_,
          &mah_threshold_, mah_rejection_modification_, mah_acceptance_modification_, mah_threshold_limit_,
          &n_rejected_, &n_curr_rejected_, &n_accepted_));
  meas->MakeFromSensorReading(msg, msg->header.stamp.toSec());

  z_p_ = meas->z_p_;  // Store this for the init procedure.

  // Make averaged measurement.
  memcpy(heightbuff, heightbuff + 1, sizeof(double) * (heightbuffsize - 1));
  heightbuff[heightbuffsize - 1] = meas->z_p_(0);
  double sum = 0;
  for (int k = 0; k < heightbuffsize; ++k)
    sum += heightbuff[k];
  z_average_p(0) = sum / heightbuffsize;

  this->manager_.msf_core_->AddMeasurement(meas);

  /*bool rejected_as_outlier=this->manager_.msf_core_->AddMeasurement(meas);
  if(rejected_as_outlier)
  {
	  mah_threshold_factor_*=msf_core::MahThresholdRejectionPunishement_;
  }
  else
  {
	  mah_threshold_factor_*=msf_core::MahThresholdRejectionReliefe_;
  }
  if(mah_threshold_factor_>=msf_core::MahThresholdLimit_)
  {
	  mah_threshold_factor_=1;
	  //REINIT THIS SENSOR
  }*/
}
}  // namespace msf_pressure_sensor
#endif  // PRESSURE_SENSORHANDLER_HPP_
