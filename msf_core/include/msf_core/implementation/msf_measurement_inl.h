/*
 * Copyright (C) 2012-2013 Simon Lynen, ASL, ETH Zurich, Switzerland
 * You can contact the author at <slynen at ethz dot ch>
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
#ifndef MEASUREMENT_INL_H_
#define MEASUREMENT_INL_H_
#include <msf_core/msf_core.h>

namespace msf_core {
template<typename EKFState_T>
MSF_MeasurementBase<EKFState_T>::MSF_MeasurementBase(bool isabsoluteMeasurement,
                                                     int sensorID, 
                                                     bool enable_mah_outlier_rejection,
                                                     double* mah_threshold,
                                                     double mah_rejection_modification,
                                                     double mah_acceptance_modification,
                                                     double mah_threshold_limit,
                                                     double* n_rejected, double* n_curr_rejected,
                                                     double* n_accepted)
    : sensorID_(sensorID),
      isabsolute_(isabsoluteMeasurement),
      enable_mah_outlier_rejection_(enable_mah_outlier_rejection),
      mah_threshold_(mah_threshold),
      mah_rejection_modification_(mah_rejection_modification),
      mah_acceptance_modification_(mah_acceptance_modification),
      mah_threshold_limit_(mah_threshold_limit),
      n_rejected_(n_rejected), n_curr_rejected_(n_curr_rejected),
      n_accepted_(n_accepted),
      time(0) {
		  //MSF_WARN_STREAM("init ne MeasurementBase class");
}

template<typename EKFState_T>
template<class H_type, class Res_type, class R_type>
void MSF_MeasurementBase<EKFState_T>::CalculateAndApplyCorrection(
    shared_ptr<EKFState_T> state, MSF_Core<EKFState_T>& core,
    const Eigen::MatrixBase<H_type>& H_delayed,
    const Eigen::MatrixBase<Res_type> & res_delayed,
    const Eigen::MatrixBase<R_type>& R_delayed) {

  EIGEN_STATIC_ASSERT_FIXED_SIZE (H_type);
  EIGEN_STATIC_ASSERT_FIXED_SIZE (R_type);

  // Get measurements.
  /// Correction from EKF update.
  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime, 1> correction_;

  R_type S;
  R_type S_inverse;
  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime,
      R_type::RowsAtCompileTime> K;
  typename MSF_Core<EKFState_T>::ErrorStateCov & P = state->P;
  //MSF_WARN_STREAM(state->P);
  
  //state->P*=2;

  S = H_delayed * P * H_delayed.transpose() + R_delayed;
  S_inverse = S.inverse();
  
  //MSF_WARN_STREAM(EKFState_T::w_m);
  

  //MSF_WARN_STREAM("new step with mah treshhold"<<(*mah_threshold_));
  if(enable_mah_outlier_rejection_){ //could do this earlier to save computation time
	  //MSF_WARN_STREAM("outlier rejection active");
    //calculate mahalanobis distance
    //Eigen::MatrixXd mah_dist_squared_temp = res_delayed.transpose() * S_inverse * res_delayed;//is this correct (this should output a scalar right?)
    double mah_dist_squared=res_delayed.transpose() * S_inverse * res_delayed;
    //double mah_dist_squared = mah_dist_squared_temp(0,0);

    //reject point as outlier if distance above threshold
    //if (sqrt(mah_dist_squared) > mah_threshold_){ //should not compute sqrt for efficiency
    if(mah_dist_squared>(*mah_threshold_)*(*mah_threshold_)){
      //in case of rejection computes the new threshold in case of rejection as a weighted sum of the current threshold and a
      //maximal threshold to be defined
      (*mah_threshold_)=(*mah_threshold_)*(1.0-mah_rejection_modification_)+mah_rejection_modification_*mah_threshold_limit_;
      (*n_rejected_)++;
      (*n_curr_rejected_)++;
	    //MSF_WARN_STREAM("new mah_threshold"<<mah_threshold_);
      //MSF_WARN_STREAM("rejecting reading as outlier with distance squared"<<mah_dist_squared);
      return;
    }
    //in case of a measurement being accepted computes a weighted average between the current threshold and the distance of the current 
    //measurement. Adds 0.1 times measurement to account for noise in it to not everfitt.
    if(mah_acceptance_modification_!=0.0)
    {
      (*mah_threshold_)=(*mah_threshold_)*(1.0-mah_acceptance_modification_)+(mah_acceptance_modification_+0.1)*sqrt(mah_dist_squared);
    }
    (*n_accepted_)++;
    (*n_curr_rejected_)=0;
  }

  K = P * H_delayed.transpose() * S_inverse;
  correction_ = K * res_delayed;
  const typename MSF_Core<EKFState_T>::ErrorStateCov KH =
      (MSF_Core<EKFState_T>::ErrorStateCov::Identity() - K * H_delayed);
  P = KH * P * KH.transpose() + K * R_delayed * K.transpose();

  // Make sure P stays symmetric.
  P = 0.5 * (P + P.transpose());

  core.ApplyCorrection(state, correction_);
}

//this function should be updated according to the one above
//however its currently unused
template<typename EKFState_T>
void MSF_MeasurementBase<EKFState_T>::CalculateAndApplyCorrection(
    shared_ptr<EKFState_T> state, MSF_Core<EKFState_T>& core,
    const Eigen::MatrixXd& H_delayed, const Eigen::MatrixXd & res_delayed,
    const Eigen::MatrixXd& R_delayed) {

  // Get measurements.
  /// Correction from EKF update.
  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime, 1> correction_;

  Eigen::MatrixXd S;
  Eigen::MatrixXd S_inverse;
  Eigen::MatrixXd K(
      static_cast<int>(MSF_Core<EKFState_T>::nErrorStatesAtCompileTime),
      R_delayed.rows());
  typename MSF_Core<EKFState_T>::ErrorStateCov & P = state->P;

  S = H_delayed * P * H_delayed.transpose() + R_delayed;
  S_inverse = S.inverse();

  //MSF_WARN_STREAM("getting close2");
  if(enable_mah_outlier_rejection_){
	//MSF_WARN_STREAM("outlier rejection active2");
    //calculate mahalanobis distance
    double mah_dist_squared = res_delayed.transpose() * S_inverse * res_delayed;

    //reject point as outlier if distance above threshold
    if (mah_dist_squared > (*mah_threshold_)*(*mah_threshold_)){
      MSF_WARN_STREAM("rejecting reading as outlier");
      (*mah_threshold_)*=2;
      return;
    }
    (*mah_threshold_)*=0.9;
  }

  K = P * H_delayed.transpose() * S_inverse; 
  correction_ = K * res_delayed;
  const typename MSF_Core<EKFState_T>::ErrorStateCov KH =
      (MSF_Core<EKFState_T>::ErrorStateCov::Identity() - K * H_delayed);
  P = KH * P * KH.transpose() + K * R_delayed * K.transpose();

  // Make sure P stays symmetric.
  P = 0.5 * (P + P.transpose());

  core.ApplyCorrection(state, correction_);
}

template<typename EKFState_T>
template<class H_type, class Res_type, class R_type>
void MSF_MeasurementBase<EKFState_T>::CalculateAndApplyCorrectionRelative(
    shared_ptr<EKFState_T> state_old, shared_ptr<EKFState_T> state_new,
    MSF_Core<EKFState_T>& core, const Eigen::MatrixBase<H_type>& H_old,
    const Eigen::MatrixBase<H_type>& H_new,
    const Eigen::MatrixBase<Res_type> & res,
    const Eigen::MatrixBase<R_type>& R) {
  //MSF_WARN_STREAM("using relative correction");
  EIGEN_STATIC_ASSERT_FIXED_SIZE (H_type);
  EIGEN_STATIC_ASSERT_FIXED_SIZE (R_type);

  // Get measurements.
  /// Correction from EKF update.
  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime, 1> correction_;

  enum {
    Pdim = MSF_Core<EKFState_T>::nErrorStatesAtCompileTime
  };

  // Get the accumulated system dynamics.
  Eigen::Matrix<double, Pdim, Pdim> F_accum;
  core.GetAccumulatedStateTransitionStochasticCloning(state_old, state_new, F_accum);

  /*[
   *        | P_kk       P_kk * F' |
   * P_SC = |                      |
   *        | F * P_kk   P_mk      |
   */
  Eigen::Matrix<double, 2 * Pdim, 2 * Pdim> P_SC;
  P_SC.template block<Pdim, Pdim>(0, 0) = state_old->P;
  // According to TRO paper, ICRA paper has a mistake here.
  P_SC.template block<Pdim, Pdim>(0, Pdim) = state_old->P * F_accum.transpose();
  P_SC.template block<Pdim, Pdim>(Pdim, 0) = F_accum * state_old->P;
  P_SC.template block<Pdim, Pdim>(Pdim, Pdim) = state_new->P;

  /*
   * H_SC = [H_kk  H_mk]
   */
  Eigen::Matrix<double, H_type::RowsAtCompileTime, H_type::ColsAtCompileTime * 2> H_SC;

  H_SC.template block<H_type::RowsAtCompileTime, H_type::ColsAtCompileTime>(0,
                                                                            0) =
      H_old;
  H_SC.template block<H_type::RowsAtCompileTime, H_type::ColsAtCompileTime>(
      0, H_type::ColsAtCompileTime) = H_new;

  R_type S_SC;
  S_SC = H_SC * P_SC * H_SC.transpose() + R;

  //TODO (slynen): Only compute the part of K_SC that we need.
  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime * 2,
      R_type::RowsAtCompileTime> K_SC;
  K_SC = P_SC * H_SC.transpose() * S_SC.inverse();

  Eigen::Matrix<double, MSF_Core<EKFState_T>::nErrorStatesAtCompileTime,
      R_type::RowsAtCompileTime> K;
  K = K_SC
      .template block<MSF_Core<EKFState_T>::nErrorStatesAtCompileTime,
          R_type::RowsAtCompileTime>(
      MSF_Core<EKFState_T>::nErrorStatesAtCompileTime, 0);

  correction_ = K * res;

  typename MSF_Core<EKFState_T>::ErrorStateCov & P = state_new->P;
  P = P - K * S_SC * K.transpose();

  // Make sure P stays symmetric.
  // TODO (slynen): EV, set Evalues<eps to zero, then reconstruct.
  state_new->P = 0.5 * (state_new->P + state_new->P.transpose());

  core.ApplyCorrection(state_new, correction_);
}

template<typename EKFState_T>
void MSF_InitMeasurement<EKFState_T>::Apply(
    shared_ptr<EKFState_T> stateWithCovariance, MSF_Core<EKFState_T>& core) {

  // Makes this state a valid starting point.
  stateWithCovariance->time = this->time;

  boost::fusion::for_each(stateWithCovariance->statevars,
                          msf_tmp::CopyInitStates<EKFState_T>(InitState));

  if (!(InitState.P.minCoeff() == 0 && InitState.P.maxCoeff() == 0)) {
    stateWithCovariance->P = InitState.P;
    MSF_WARN_STREAM("Using user defined initial error state covariance.");
  } else {
    MSF_WARN_STREAM(
        "Using simulated core plus fixed diag initial error state covariance.");
  }

  if (ContainsInitialSensorReadings_) {
    stateWithCovariance->a_m = InitState.a_m;
    stateWithCovariance->w_m = InitState.w_m;
  } else {
    stateWithCovariance->a_m.setZero();
    stateWithCovariance->w_m.setZero();
  }

  core.GetUserCalc().PublishStateInitial(stateWithCovariance);

}
}  // namespace msf_core
#endif  // MEASUREMENT_INL_H_
