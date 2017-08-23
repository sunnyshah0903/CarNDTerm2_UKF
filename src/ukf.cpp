#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;



// Global Initializations
MatrixXd H_laser_ = MatrixXd(2, 5);
MatrixXd R_laser_ = MatrixXd(2, 2);



/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {


	// if this is false, laser measurements will be ignored (except during init)
	use_laser_ = true;

	// if this is false, radar measurements will be ignored (except during init)
	use_radar_ = true;

	// initial state vector
	x_ = VectorXd(5);

	// initial covariance matrix
	P_ = MatrixXd(5, 5);

	// Process noise standard deviation longitudinal acceleration in m/s^2
	std_a_ = 0.2;

	// Process noise standard deviation yaw acceleration in rad/s^2
	std_yawdd_ = 0.68;

	// Laser measurement noise standard deviation position1 in m
	std_laspx_ = 0.15;

	// Laser measurement noise standard deviation position2 in m
 	std_laspy_ = 0.15;

	// Radar measurement noise standard deviation radius in m
	std_radr_ = 0.3;

	// Radar measurement noise standard deviation angle in rad
	std_radphi_ = 0.03;

	// Radar measurement noise standard deviation radius change in m/s
	std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

	//set state dimension
	n_x_ = 5;

	//set augmented dimension
	n_aug_ = 7;

	//define spreading parameter
	lambda_ = 3 - n_aug_;

	//create vector for weights
	weights_ = VectorXd(2*n_aug_+1);

	//create covariance matrix for prediction
	P_ = MatrixXd(n_x_, n_x_);

	//create matrix with predicted sigma points as columns
 	Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

	is_initialized_ = false;
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  /*****************************************************************************
   *  Initialization
   ****************************************************************************/

	if (!is_initialized_) {

		float px = 0;
		float py = 0;
		if (use_radar_ && meas_package.sensor_type_ == MeasurementPackage::RADAR) {

			float ro = meas_package.raw_measurements_(0);
			float phi = meas_package.raw_measurements_(1);
			px = ro * cos(phi);
			py = ro * sin(phi);
			float ro_dot = meas_package.raw_measurements_(2);
			x_ << px, py, ro_dot, phi, 0;
			previous_timestamp_ = meas_package.timestamp_;

		}
		else{ //LIDAR

			px = meas_package.raw_measurements_(0);
			py = meas_package.raw_measurements_(1);

			x_ << px, py, 0, 0, 0;

			previous_timestamp_ = meas_package.timestamp_;

		}

		P_ << 1, 0, 0, 0,0,
			0, 1, 0, 0,0,
			0, 0, 10, 0,0,
			0, 0, 0, 10,0,
			0, 0, 0, 0,10;

		is_initialized_ = true;
		return;
	}

	double delta_t = (meas_package.timestamp_ - previous_timestamp_) / 1000000.0;

	Prediction(delta_t);


	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
		UpdateRadar(meas_package);
	} else {
		UpdateLidar(meas_package);
	}

	previous_timestamp_ = meas_package.timestamp_;
	// print the output
	cout << "x_ = " << x_ << endl;
	cout << "P_ = " << P_ << endl;
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
	/**
	TODO:

	Complete this function! Estimate the object's location. Modify the state
	vector, x_. Predict sigma points, the state, and the state covariance matrix.
	*/

	VectorXd x_aug_ = VectorXd(n_aug_);
	MatrixXd Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);
	MatrixXd P_aug_ = MatrixXd(n_aug_, n_aug_);;


/*******************************************************************************
 * Create Augemented Sigma Points, Lesson 7, Section 18
 ******************************************************************************/

	//create augmented mean state
	x_aug_.head(5) = x_;
	x_aug_(5) = 0;
	x_aug_(6) = 0;

	//create augmented covariance matrix
	P_aug_.fill(0.0);
	P_aug_.topLeftCorner(5,5) = P_;
	P_aug_(5,5) = std_a_*std_a_;
	P_aug_(6,6) = std_yawdd_*std_yawdd_;

	//create square root matrix
	MatrixXd L = P_aug_.llt().matrixL();

	//create augmented sigma points
	Xsig_aug_.col(0)  = x_aug_;
	for (int i = 0; i< n_aug_; i++)
	{
	  Xsig_aug_.col(i+1)       = x_aug_ + sqrt(lambda_+n_aug_) * L.col(i);
	  Xsig_aug_.col(i+1+n_aug_) = x_aug_ - sqrt(lambda_+n_aug_) * L.col(i);
	}

/*******************************************************************************
 * Predict Sigma Points, Lesson 7, Section 21
 ******************************************************************************/

	//predict sigma points
	for (int i = 0; i< 2*n_aug_+1; i++)
	{
		//extract values for better readability
		double p_x = Xsig_aug_(0,i);
		double p_y = Xsig_aug_(1,i);
		double v = Xsig_aug_(2,i);
		double yaw = Xsig_aug_(3,i);
		double yawd = Xsig_aug_(4,i);
		double nu_a = Xsig_aug_(5,i);
		double nu_yawdd = Xsig_aug_(6,i);
		//predicted state values
		double px_p, py_p;

		//avoid division by zero
		if (fabs(yawd) > 0.001) {
			px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
			py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
		}
		else {
			px_p = p_x + v*delta_t*cos(yaw);
			py_p = p_y + v*delta_t*sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd*delta_t;
		double yawd_p = yawd;
		//add noise
		px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
		py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
		v_p = v_p + nu_a*delta_t;
		yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
		yawd_p = yawd_p + nu_yawdd*delta_t;

		//write predicted sigma point into right column
		Xsig_pred_(0,i) = px_p;
		Xsig_pred_(1,i) = py_p;
		Xsig_pred_(2,i) = v_p;
		Xsig_pred_(3,i) = yaw_p;
		Xsig_pred_(4,i) = yawd_p;
	}

/*******************************************************************************
 * Predict Mean and Covariance, Lesson 7, Section 24
 ******************************************************************************/

	// set weights
 	double weight_0 = lambda_/(lambda_+n_aug_);
	weights_(0) = weight_0;
	for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
		double weight = 0.5/(n_aug_+lambda_);
		weights_(i) = weight;
	}

	//predicted state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
 		x_ = x_+ weights_(i) * Xsig_pred_.col(i);
	}

	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;
		P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
	}

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
	/**
	TODO:

	Complete this function! Use lidar data to update the belief about the object's
	position. Modify the state vector, x_, and covariance, P_.

	You'll also need to calculate the lidar NIS.
	*/


	H_laser_ << 1,0,0,0,0,
		0, 1,0,0,0;
	R_laser_ << 0.0225, 0,
	        0, 0.0225;

	MatrixXd H_ = H_laser_;
	MatrixXd R_ = R_laser_;
	VectorXd z = meas_package.raw_measurements_;

	VectorXd y =  z - (H_ * x_);
	//y[1] = atan2(sin(y[1]),cos(y[1]));

	MatrixXd Ht = H_.transpose();
	MatrixXd S = H_ * P_ * Ht + R_;
	MatrixXd Si = S.inverse();

	MatrixXd K =  P_ * Ht * Si;
	long x_size = x_.size();
	MatrixXd I = MatrixXd::Identity(x_size, x_size);

	//new state
	x_ = x_ + (K * y);
	P_ = (I - K * H_) * P_;
	cout << "x_ = " << x_ << endl;
	cout << "P_ = " << P_ << endl;

}


/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
	/**
	TODO:

	Complete this function! Use radar data to update the belief about the object's
	position. Modify the state vector, x_, and covariance, P_.

	You'll also need to calculate the radar NIS.
	*/

	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 3;

	//create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		// extract values for better readibility
		double p_x = Xsig_pred_(0,i);
		double p_y = Xsig_pred_(1,i);
		double v  = Xsig_pred_(2,i);
		double yaw = Xsig_pred_(3,i);
		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

		// measurement model
		Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig(1,i) = atan2(p_y,p_x);                                 //phi
		Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
	}

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
	for (int i=0; i < 2*n_aug_+1; i++) {
		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}

	//measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z,n_z);
	S.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	//add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_z,n_z);
	R <<    std_radr_*std_radr_, 0, 0,
        	0, std_radphi_*std_radphi_, 0,
		0, 0,std_radrd_*std_radrd_;
	S = S + R;



	//create matrix for cross correlation Tc
   	 MatrixXd Tc = MatrixXd(5, n_z);

	//calculate cross correlation matrix
	Tc.fill(0.0);
	std::cout << "2 " << std::endl;
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		//residual
		VectorXd z_diff = Zsig.col(i) - z_pred;
//		VectorXd z_diff = Zsig.col(i) - z;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

		Tc = Tc + (weights_(i) * x_diff * z_diff.transpose());

	}

	//Kalman gain K;
	MatrixXd K = Tc * S.inverse();

	//residual
	VectorXd z_diff =  meas_package.raw_measurements_ - z_pred;

	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S*K.transpose();
	cout << "x_ = " << x_ << endl;
	cout << "P_ = " << P_ << endl;

}


