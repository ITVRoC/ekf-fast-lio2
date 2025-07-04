//=====================================================EKF-Fast-LIO2=====================================================================
//Institutions: Federal University of Minas Gerais (UFMG), Federal University of Ouro Preto (UFOP) and Instituto Tecnológico Vale (ITV)
//Description: This file is responsible for merging the wheel odometry with the IMU data and the Fast-LIO2 odometry.
//Milestones: 
//             Date: November 27, 2021
//             Description: Initial version of the code.
//             Members: Gilmar Pereira da Cruz Júnior and Adriano Resende
//             E-mails: gilmarpcruzjunior@gmail.com, adrianomcr18@gmail.com
//
//             Date: June 27, 2025
//             Description: Include the wheel odometry adaptive covariance and the Fast-LIO2 odometry input.
//             Members: Gilmar Pereira da Cruz Júnior and Gabriel Malaquias
//             E-mails: gilmarpcruzjunior@gmail.com, gmdeoliveira@ymail.com
//=======================================================================================================================================

#include "settings_adaptive_filter.h"

using namespace Eigen;
using namespace std;

//-----------------------------
// Global variables
//-----------------------------
bool enableFilter;
bool enableImu;
bool enableWheel;
bool enableLidar;

float lidarG;
float wheelG;
float imuG;

// adaptive wheel odometry covariance
float gamma_vx;
float gamma_omegaz;
float delta_vx; 
float delta_omegaz;

std::string filterFreq;
std::string imuTopic;
std::string wheelTopic;
std::string FastLIO2_OdometryTopic;
std::string filterTopic;

std::mutex mtx; 

//-----------------------------
// LiDAR Odometry class
//-----------------------------
class AdaptiveFilter{

private:
    // ros node
    ros::NodeHandle nh;

    // Subscriber
    ros::Subscriber subImu;
    ros::Subscriber subWheelOdometry;
    ros::Subscriber subLaserOdometry;

    // Publisher
    ros::Publisher pubFilteredOdometry;
    ros::Publisher pubIndLiDARMeasurement;

    // header
    std_msgs::Header headerI;
    std_msgs::Header headerW;
    std_msgs::Header headerL;

    // TF 
    tf::StampedTransform filteredOdometryTrans;
    tf::TransformBroadcaster tfBroadcasterfiltered;

    // filtered odom
    nav_msgs::Odometry filteredOdometry;
    nav_msgs::Odometry indLiDAROdometry;

    // Measure
    Eigen::VectorXd imuMeasure, wheelMeasure, lidarMeasure, lidarMeasureL;

    // Measure Covariance
    Eigen::MatrixXd E_imu, E_wheel, E_lidar, E_lidarL, E_pred;

    // States and covariances
    Eigen::VectorXd X, V;
    Eigen::MatrixXd P, PV;

    // pose and velocities
    Eigen::VectorXd pose, velocities;

    // Times
    double imuTimeLast;
    double wheelTimeLast;
    double lidarTimeLast;

    double imuTimeCurrent;
    double wheelTimeCurrent;
    double lidarTimeCurrent;

    double imu_dt;
    double wheel_dt;
    double lidar_dt;

    // imu varibles
    struct bias bias_linear_acceleration;
    struct bias bias_angular_velocity;

    // number of state or measure vectors
    int N_STATES = 12;
    int N_IMU = 9; 
    int N_WHEEL = 2; 
    int N_LIDAR = 6;
    
    // boolean
    bool imuActivated;
    bool wheelActivated;
    bool lidarActivated;
    bool imuNew;
    bool wheelNew;
    bool lidarNew;
    bool velComp;

public:
    AdaptiveFilter():
        nh("~")
    {
        // Subscriber
        subImu = nh.subscribe<sensor_msgs::Imu>(imuTopic, 50, &AdaptiveFilter::imuHandler, this);
        subWheelOdometry = nh.subscribe<nav_msgs::Odometry>(wheelTopic, 5, &AdaptiveFilter::wheelOdometryHandler, this);
        subLaserOdometry = nh.subscribe<nav_msgs::Odometry>(FastLIO2_OdometryTopic, 5, &AdaptiveFilter::laserOdometryHandler, this);
        
        // Publisher
        pubFilteredOdometry = nh.advertise<nav_msgs::Odometry> (filterTopic, 5);
        
        // Initialization
        allocateMemory();
        initialization();
    }

    //------------------
    // Auxliar functions
    //------------------
    void allocateMemory(){
        imuMeasure.resize(N_IMU);
        wheelMeasure.resize(N_WHEEL);
        lidarMeasure.resize(N_LIDAR);
        lidarMeasureL.resize(N_LIDAR);

        E_imu.resize(N_IMU,N_IMU);
        E_wheel.resize(N_WHEEL,N_WHEEL);
        E_lidar.resize(N_LIDAR,N_LIDAR);
        E_lidarL.resize(N_LIDAR,N_LIDAR);
        E_pred.resize(N_STATES,N_STATES);

        X.resize(N_STATES);
        P.resize(N_STATES,N_STATES);

        V.resize(N_STATES);
        PV.resize(N_STATES,N_STATES);
    }

    void initialization(){
        // times
        imuTimeLast = 0;
        lidarTimeLast = 0;
        wheelTimeLast = 0;

        imuTimeCurrent = 0;
        lidarTimeCurrent = 0;
        wheelTimeCurrent = 0;

        // auxliar 
        bias_linear_acceleration.x = 0.0001;
        bias_linear_acceleration.y = 0.0001;
        bias_linear_acceleration.z = 0.0001;

        bias_angular_velocity.x = 0.00000001;
        bias_angular_velocity.y = 0.00000001;
        bias_angular_velocity.z = 0.00000001;

        wheel_dt = 0.05;
        lidar_dt = 0.1;

        // boolean
        imuActivated = false;
        lidarActivated = false;
        wheelActivated = false;

        imuNew = false;
        wheelNew = false;
        lidarNew = false;

        velComp = false;

        // matrices and vectors
        imuMeasure = Eigen::VectorXd::Zero(N_IMU);
        wheelMeasure = Eigen::VectorXd::Zero(N_WHEEL);
        lidarMeasure = Eigen::VectorXd::Zero(N_LIDAR);
        lidarMeasureL = Eigen::VectorXd::Zero(N_LIDAR);
        
        E_imu = Eigen::MatrixXd::Zero(N_IMU,N_IMU);
        E_lidar = Eigen::MatrixXd::Zero(N_LIDAR,N_LIDAR);
        E_lidarL = Eigen::MatrixXd::Zero(N_LIDAR,N_LIDAR);
        E_wheel = Eigen::MatrixXd::Zero(N_WHEEL,N_WHEEL);
        E_pred = Eigen::MatrixXd::Zero(N_STATES,N_STATES);

        // state initial
        X = Eigen::VectorXd::Zero(N_STATES);
        P = Eigen::MatrixXd::Zero(N_STATES,N_STATES);
        V = Eigen::VectorXd::Zero(N_STATES);

        // covariance initial
        P(0,0) = 0.1;   // x
        P(1,1) = 0.1;   // y
        P(2,2) = 0.1;   // z
        P(3,3) = 0.1;   // roll
        P(4,4) = 0.1;   // pitch
        P(5,5) = 0.1;   // yaw
        P(6,6) = 0.1;   // vx
        P(7,7) = 0.1;   // vy
        P(8,8) = 0.1;   // vz
        P(9,9) = 0.1;   // wx
        P(10,10) = 0.1;   // wy
        P(11,11) = 0.1;   // wz

        // Fixed prediction covariance
        E_pred.block(6,6,6,6) = 0.01*P.block(6,6,6,6);
    }

    //-----------------
    // predict function
    //-----------------
    void prediction_stage(double dt){
        Eigen::MatrixXd F(N_STATES,N_STATES);

        // jacobian's computation
        F = jacobian_state(X, dt);

        // Priori state and covariance estimated
        X = f_prediction_model(X, dt);

        // Priori covariance
        P = F*P*F.transpose() + E_pred;
    }

    //-----------------
    // correction stage
    //-----------------
    void correction_wheel_stage(double dt){
        Eigen::VectorXd Y(N_WHEEL), hx(N_WHEEL);
        Eigen::MatrixXd H(N_WHEEL,N_STATES), K(N_STATES,N_WHEEL), E(N_WHEEL,N_WHEEL), S(N_WHEEL,N_WHEEL);

        // measure model of wheel odometry (only forward linear velocity)
        hx(0) = X(6);
        hx(1) = X(11);

        // measurement
        Y = wheelMeasure;

        // Jacobian of hx with respect to the states
        H = Eigen::MatrixXd::Zero(N_WHEEL,N_STATES);
        H(0,6) = 1; 
        H(1,11) = 1;

        // covariance matrices
        E << E_wheel;

        // Kalman's gain
        S = H*P*H.transpose() + E;
        K = P*H.transpose()*S.inverse();

        // correction
        X = X + K*(Y - hx);
        P = P - K*H*P;
    }

    void correction_imu_stage(double dt){
        Eigen::Matrix3d S, E;
        Eigen::Vector3d Y, hx;
        Eigen::MatrixXd H(3,N_STATES), K(N_STATES,3);

        // measure model
        hx = X.block(3,0,3,1);

        // IMU measurement
        Y = imuMeasure.block(6,0,3,1); // roll pitch yaw

        // Jacobian of hx with respect to the states
        H = Eigen::MatrixXd::Zero(3,N_STATES);
        H.block(0,3,3,3) = Eigen::MatrixXd::Identity(3,3);

        // covariance matrices
        E = E_imu.block(6,6,3,3);

        // Kalman's gain
        S = H*P*H.transpose() + E;
        K = P*H.transpose()*S.inverse();

        // correction - state
        Eigen::VectorXd residues(3), KR(N_STATES);
        residues(0) = atan2(sin(Y(0) - hx(0)), cos(Y(0) - hx(0)));
        residues(1) = atan2(sin(Y(1) - hx(1)), cos(Y(1) - hx(1)));
        residues(2) = atan2(sin(Y(2) - hx(2)), cos(Y(2) - hx(2)));
        KR = K*residues;

        X.block(0,0,3,1) = X.block(0,0,3,1) + KR.block(0,0,3,1);
        X(3) = atan2(sin(X(3) + KR(3)), cos(X(3) + KR(3)));
        X(4) = atan2(sin(X(4) + KR(4)), cos(X(4) + KR(4)));
        X(5) = atan2(sin(X(5) + KR(5)), cos(X(5) + KR(5)));
        X.block(6,0,6,1) = X.block(6,0,6,1) + KR.block(6,0,6,1);

        // X = X + K*(Y - hx); 
        // correction - covariance
        P = P - K*H*P;
    }

    void correction_lidar_stage(double dt){
        Eigen::MatrixXd K(N_STATES,N_LIDAR), S(N_LIDAR,N_LIDAR), G(N_LIDAR,N_LIDAR), Gl(N_LIDAR,N_LIDAR), Q(N_LIDAR,N_LIDAR);
        Eigen::VectorXd Y(N_LIDAR), hx(N_LIDAR);
        Eigen::MatrixXd H(N_LIDAR,N_STATES); 

        // measure model
        hx = X.block(6,0,6,1);
        // wheel measurement
        Y = indirect_lidar_measurement(lidarMeasure, lidarMeasureL, dt);

        // Jacobian of hx with respect to the states
        H = Eigen::MatrixXd::Zero(N_LIDAR,N_STATES);
        H.block(0,6,6,6) = Eigen::MatrixXd::Identity(N_LIDAR,N_LIDAR);

        // Error propagation     
        G = jacobian_lidar_measurement(lidarMeasure, lidarMeasureL, dt);
        Gl = jacobian_lidar_measurementL(lidarMeasure, lidarMeasureL, dt);

        Q =  G*E_lidar*G.transpose() + Gl*E_lidarL*Gl.transpose();
        // Q =  G*E_lidar*G.transpose();

        // data save 
        //publish_indirect_lidar_measurement(Y, Q);        

        // Kalman's gain
        S = H*P*H.transpose() + Q;
        K = P*H.transpose()*S.inverse();

        // correction
        X = X + K*(Y - hx);
        P = P - K*H*P;

        // last measurement
        lidarMeasureL = lidarMeasure;
        E_lidarL = E_lidar;
    }
    
    //---------
    // Models
    //---------
    VectorXd f_prediction_model(VectorXd x, double dt){ 
        // state: {x, y, z, roll, pitch, yaw, vx, vy, vz, wx, wy, wz}
        //        {         (world)         }{        (body)        }
        Eigen::Matrix3d R, Rx, Ry, Rz, J;
        Eigen::VectorXd xp(N_STATES), x2;
        Eigen::MatrixXd A(6,6);    

        // Rotation matrix
        Rx = Eigen::AngleAxisd(x(3), Eigen::Vector3d::UnitX());
        Ry = Eigen::AngleAxisd(x(4), Eigen::Vector3d::UnitY());
        Rz = Eigen::AngleAxisd(x(5), Eigen::Vector3d::UnitZ());
        R = Rz*Ry*Rx;
        
        // Jacobian matrix
        J << 1.0, sin(x(3))*tan(x(4)), cos(x(3))*tan(x(4)),
             0.0, cos(x(3)), -sin(x(3)),
             0.0, sin(x(3))/cos(x(4)), cos(x(3))/cos(x(4));
        
        // model
        A = Eigen::MatrixXd::Identity(6,6);
        A.block(0,0,3,3) = R;
        A.block(3,3,3,3) = J;

        // teste para corredor de 500 m
        //x2 = Eigen::VectorXd(N_STATES);
        x2 = x;
        x2.block(7,0,4,1) << 0.0, 0.0, 0.0, 0.0;

        xp.block(0,0,6,1) = x2.block(0,0,6,1) + A*x2.block(6,0,6,1)*dt;
        xp.block(6,0,6,1) = x2.block(6,0,6,1);

        return xp;
    }

    VectorXd indirect_lidar_measurement(VectorXd u, VectorXd ul, double dt){
        Eigen::Matrix3d R, Rx, Ry, Rz, J;
        Eigen::VectorXd up(N_LIDAR), u_diff(N_LIDAR);  
        Eigen::MatrixXd A(N_LIDAR,N_LIDAR);  

        // Rotation matrix
        Rx = Eigen::AngleAxisd(ul(3), Eigen::Vector3d::UnitX());
        Ry = Eigen::AngleAxisd(ul(4), Eigen::Vector3d::UnitY());
        Rz = Eigen::AngleAxisd(ul(5), Eigen::Vector3d::UnitZ());
        R = Rz*Ry*Rx;
        
        // Jacobian matrix
        J << 1.0, sin(ul(3))*tan(ul(4)), cos(ul(3))*tan(ul(4)),
             0.0, cos(ul(3)), -sin(ul(3)),
             0.0, sin(ul(3))/cos(ul(4)), cos(ul(3))/cos(ul(4));
        
        // model
        u_diff.block(0,0,3,1) = (u.block(0,0,3,1) - ul.block(0,0,3,1));
        u_diff(3) = atan2(sin(u(3) - ul(3)),cos(u(3) - ul(3)));
        u_diff(4) = atan2(sin(u(4) - ul(4)),cos(u(4) - ul(4)));
        u_diff(5) = atan2(sin(u(5) - ul(5)),cos(u(5) - ul(5)));

        A = Eigen::MatrixXd::Zero(N_LIDAR,N_LIDAR);
        A.block(0,0,3,3) = R.transpose();
        A.block(3,3,3,3) = J.inverse();

        up = A*u_diff/dt;

        return up;
    }

    //----------
    // Jacobians
    //----------
    MatrixXd jacobian_state(VectorXd x, double dt){
        Eigen::MatrixXd J(N_STATES,N_STATES);
        Eigen::VectorXd f0(N_STATES), f1(N_STATES), x_plus(N_STATES);

        f0 = f_prediction_model(x, dt);

        double delta = 0.0001;
        for (size_t i = 0; i < N_STATES; i++){
            x_plus = x;
            x_plus(i) = x_plus(i) + delta;

            f1 = f_prediction_model(x_plus, dt);
           
            J.block(0,i,N_STATES,1) = (f1 - f0)/delta;       
            J(3,i) = sin(f1(3) - f0(3))/delta;
            J(4,i) = sin(f1(4) - f0(4))/delta;
            J(5,i) = sin(f1(5) - f0(5))/delta; 
        }

        return J;
    }

    MatrixXd jacobian_lidar_measurement(VectorXd u, VectorXd ul, double dt){ 
        Eigen::MatrixXd J(N_LIDAR,N_LIDAR);
        Eigen::VectorXd f0(N_LIDAR), f1(N_LIDAR), u_plus(N_LIDAR);

        f0 = indirect_lidar_measurement(u, ul, dt);

        double delta = 0.0000001;
        for (size_t i = 0; i < N_LIDAR; i++){
            u_plus = u;
            u_plus(i) = u_plus(i) + delta;

            f1 = indirect_lidar_measurement(u_plus, ul, dt);
           
            J.block(0,i,N_LIDAR,1) = (f1 - f0)/delta;       
            J(3,i) = sin(f1(3) - f0(3))/delta;
            J(4,i) = sin(f1(4) - f0(4))/delta;
            J(5,i) = sin(f1(5) - f0(5))/delta; 
        }

        return J;
    }

    MatrixXd jacobian_lidar_measurementL(VectorXd u, VectorXd ul, double dt){ 
        Eigen::MatrixXd J(N_LIDAR,N_LIDAR);
        Eigen::VectorXd f0(N_LIDAR), f1(N_LIDAR), ul_plus(N_LIDAR);

        f0 = indirect_lidar_measurement(u, ul, dt);

        double delta = 0.0000001;
        for (size_t i = 0; i < N_LIDAR; i++){
            ul_plus = ul;
            ul_plus(i) = ul_plus(i) + delta;

            f1 = indirect_lidar_measurement(u, ul_plus, dt);
           
            J.block(0,i,N_LIDAR,1) = (f1 - f0)/delta;       
            J(3,i) = sin(f1(3) - f0(3))/delta;
            J(4,i) = sin(f1(4) - f0(4))/delta;
            J(5,i) = sin(f1(5) - f0(5))/delta; 
        }

        return J;
    }

    void wheelOdometryAdaptiveCovariance(){
        // wheel odometry covariance adaptive inputs
        double omegaz_wheel_odom = wheelMeasure(1);
        double omegaz_imu = imuMeasure(5);

        E_wheel(0,0) = gamma_vx * abs(omegaz_wheel_odom - omegaz_imu) + delta_vx;
        E_wheel(1,1) = gamma_omegaz * abs(omegaz_wheel_odom - omegaz_imu) + delta_omegaz;
    }       

    //----------
    // callbacks
    //----------
    void imuHandler(const sensor_msgs::Imu::ConstPtr& imuIn){
        double timeL = ros::Time::now().toSec();

        // time
        if (imuActivated){
            imuTimeLast = imuTimeCurrent;
            imuTimeCurrent = imuIn->header.stamp.toSec();
        }else{
            imuTimeCurrent = imuIn->header.stamp.toSec();
            imuTimeLast = imuTimeCurrent + 0.01;
            imuActivated = true;
        }       

        // roll, pitch and yaw 
        double roll, pitch, yaw;
        geometry_msgs::Quaternion orientation = imuIn->orientation;
        tf::Matrix3x3(tf::Quaternion(orientation.x, orientation.y, orientation.z, orientation.w)).getRPY(roll, pitch, yaw);

        // measure
        imuMeasure.block(0,0,3,1) << imuIn->linear_acceleration.x, imuIn->linear_acceleration.y, imuIn->linear_acceleration.z;
        imuMeasure.block(3,0,3,1) << imuIn->angular_velocity.x, imuIn->angular_velocity.y, imuIn->angular_velocity.z; 
        imuMeasure.block(6,0,3,1) << roll, pitch, yaw;

        // covariance
        E_imu.block(0,0,3,3) << imuIn->linear_acceleration_covariance[0], imuIn->linear_acceleration_covariance[1], imuIn->linear_acceleration_covariance[2],
                                imuIn->linear_acceleration_covariance[3], imuIn->linear_acceleration_covariance[4], imuIn->linear_acceleration_covariance[5],
                                imuIn->linear_acceleration_covariance[6], imuIn->linear_acceleration_covariance[7], imuIn->linear_acceleration_covariance[8];
        E_imu.block(3,3,3,3) << imuIn->angular_velocity_covariance[0], imuIn->angular_velocity_covariance[1], imuIn->angular_velocity_covariance[2],
                                imuIn->angular_velocity_covariance[3], imuIn->angular_velocity_covariance[4], imuIn->angular_velocity_covariance[5],
                                imuIn->angular_velocity_covariance[6], imuIn->angular_velocity_covariance[7], imuIn->angular_velocity_covariance[8];
        E_imu.block(6,6,3,3) << imuIn->orientation_covariance[0], imuIn->orientation_covariance[1], imuIn->orientation_covariance[2],
                                imuIn->orientation_covariance[3], imuIn->orientation_covariance[4], imuIn->orientation_covariance[5],
                                imuIn->orientation_covariance[6], imuIn->orientation_covariance[7], imuIn->orientation_covariance[4]; //the yaw covariance was zero 

        E_imu.block(3,3,3,3) = imuG*E_imu.block(3,3,3,3);
        E_imu.block(6,6,3,3) = imuG*E_imu.block(6,6,3,3);

        // time
        imu_dt = imuTimeCurrent - imuTimeLast;
        imu_dt = 0.02;

        // header
        double timediff = ros::Time::now().toSec() - timeL + imuTimeCurrent;
        headerI = imuIn->header;
        headerI.stamp = ros::Time().fromSec(timediff);

        imuNew = true;
    }

    void wheelOdometryHandler(const nav_msgs::Odometry::ConstPtr& wheelOdometry){
        double timeL = ros::Time::now().toSec();

        // time
        if (wheelActivated){
            wheelTimeLast = wheelTimeCurrent;
            wheelTimeCurrent = wheelOdometry->header.stamp.toSec();
        }else{
            wheelTimeCurrent = wheelOdometry->header.stamp.toSec();
            wheelTimeLast = wheelTimeCurrent + 0.05;
            wheelActivated = true;
        } 

        // measure
        wheelMeasure << 1.0*wheelOdometry->twist.twist.linear.x, wheelOdometry->twist.twist.angular.z;

        // covariance
        wheelOdometryAdaptiveCovariance();

        // time
        wheel_dt = wheelTimeCurrent - wheelTimeLast;
        wheel_dt = 0.02;

        // header
        double timediff = ros::Time::now().toSec() - timeL + wheelTimeCurrent;
        headerW = wheelOdometry->header;
        headerW.stamp = ros::Time().fromSec(timediff);

        // new measure
        wheelNew =  true;
    }

    void laserOdometryHandler(const nav_msgs::Odometry::ConstPtr& laserOdometry){
        double timeL = ros::Time::now().toSec();

        if (lidarActivated){
            lidarTimeLast = lidarTimeCurrent;
            lidarTimeCurrent = laserOdometry->header.stamp.toSec();
        }else{
            lidarTimeCurrent = laserOdometry->header.stamp.toSec();
            lidarTimeLast = lidarTimeCurrent + 0.1;
            lidarActivated = true;
        }  
        
        // roll, pitch and yaw 
        double roll, pitch, yaw;
        geometry_msgs::Quaternion orientation = laserOdometry->pose.pose.orientation;
        tf::Matrix3x3(tf::Quaternion(orientation.x, orientation.y, orientation.z, orientation.w)).getRPY(roll, pitch, yaw);

        lidarMeasure.block(0,0,3,1) << laserOdometry->pose.pose.position.x, laserOdometry->pose.pose.position.y, laserOdometry->pose.pose.position.z;
        lidarMeasure.block(3,0,3,1) << roll, pitch, yaw;    

        //old covariance calculation method
        //double corner = double(laserOdometry->twist.twist.linear.x);
        //double surf = double(laserOdometry->twist.twist.angular.x); 
        //E_lidar = adaptive_covariance(corner, surf);

        // lidar pose convariance
        int k = 0;
        for (int i = 0; i < 6; i++){
            for (int j = 0; j < 6; j++){
                E_lidar(i,j) = lidarG*laserOdometry->pose.covariance[k];
                k++;
            }
        } 
        //E_lidar = laserOdometry->pose.covariance;

        // time
        lidar_dt = lidarTimeCurrent - lidarTimeLast;
        lidar_dt = 0.1;

        // header
        double timediff = ros::Time::now().toSec() - timeL + lidarTimeCurrent;
        headerL = laserOdometry->header;
        headerL.stamp = ros::Time().fromSec(timediff);
        
        //New measure
        lidarNew = true;
    }

    //----------
    // publisher
    //----------
    void publish_odom(char model){
        switch(model){
                case 'i':
                    filteredOdometry.header = headerI;
                    break;
                case 'w':
                    filteredOdometry.header = headerW;
                    break;
                case 'l':
                    filteredOdometry.header = headerL;
            }
        
        filteredOdometry.header.frame_id = "chassis_init";
        filteredOdometry.child_frame_id = "ekf_odom_frame";

        geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw (X(3), X(4), X(5));

        // pose
        filteredOdometry.pose.pose.orientation.x = geoQuat.x;
        filteredOdometry.pose.pose.orientation.y = geoQuat.y;
        filteredOdometry.pose.pose.orientation.z = geoQuat.z;
        filteredOdometry.pose.pose.orientation.w = geoQuat.w;
        filteredOdometry.pose.pose.position.x = X(0); 
        filteredOdometry.pose.pose.position.y = X(1);
        filteredOdometry.pose.pose.position.z = X(2);

        // pose convariance
        int k = 0;
        for (int i = 0; i < 6; i++){
            for (int j = 0; j < 6; j++){
                filteredOdometry.pose.covariance[k] = P(i,j);
                k++;
            }
        }      

        // twist
        filteredOdometry.twist.twist.linear.x = X(6);
        filteredOdometry.twist.twist.linear.y = X(7);
        filteredOdometry.twist.twist.linear.z = X(8);
        filteredOdometry.twist.twist.angular.x = X(9);
        filteredOdometry.twist.twist.angular.y = X(10);
        filteredOdometry.twist.twist.angular.z = X(11);

        // twist convariance
        k = 0;
        for (int i = 6; i < 12; i++){
            for (int j = 6; j < 12; j++){
                filteredOdometry.twist.covariance[k] = P(i,j);
                k++;
            }
        } 

        pubFilteredOdometry.publish(filteredOdometry);
    }

    //----------
    // runs
    //----------
    void run(){
        // rate
        ros::Rate r(200);        

        double t_last = ros::Time::now().toSec();
        double t_now;
        double dt_now;
        bool pub_lidar, pub_wheel, pub_imu, pub_pred;
        pub_lidar = false;
        pub_wheel = false;
        pub_imu = false;
        pub_pred = false;

        //ROS_INFO("Running...");  
        while (ros::ok())
        {
            // Prediction
            if (enableFilter){
                // prediction stage
                t_now = ros::Time::now().toSec();
                dt_now = t_now-t_last;
                t_last = t_now;

                // prediction_stage(1/200.0);
                prediction_stage(dt_now);
                
                // publish state
                if (filterFreq == "p"){
                    // publish_odom('p');
                    pub_pred =  true;
                }
            }

            //Correction IMU
            if (enableFilter && enableImu && imuActivated && imuNew){
                // correction stage
                correction_imu_stage(imu_dt);

                // publish state
                if (filterFreq == "i"){
                    // publish_odom('i');
                    pub_imu =  true;
                }

                // control variable
                imuNew =  false;
            }

            // Correction wheel
            if (enableFilter && enableWheel && wheelActivated && wheelNew){                
                // correction stage
                correction_wheel_stage(wheel_dt);

                if (filterFreq == "w"){
                    // publish_odom('w');
                    pub_wheel = true;
                }                

                // control variable
                wheelNew =  false;
            }

            //Corection LiDAR
            if (enableFilter && enableLidar && lidarActivated && lidarNew){      
                //ROS_INFO("Lidar correction"); //DEBUG
                // correction stage
                correction_lidar_stage(lidar_dt);

                // publish state
                if (filterFreq == "l"){
                    // publish_odom('l');
                    pub_lidar = true;
                }

                // controle variable
                lidarNew =  false;
            }

            // publishing
            if (pub_pred){
                publish_odom('p');
                pub_pred = false;
            }else if (pub_lidar){
                publish_odom('l');
                pub_lidar =  false;
            }else if (pub_wheel){
                publish_odom('w');
                pub_wheel = false;
            }else if (pub_imu){
                publish_odom('i');
                pub_imu = false;
            }

            
            ros::spinOnce();
            r.sleep();        
        }
    }
};

//-----------------------------
// Main 
//-----------------------------
int main(int argc, char** argv)
{
    ros::init(argc, argv, "adaptive_filter");

    //Parameters init:    
    ros::NodeHandle nh_;
    try
    {
        nh_.param("/ekf_fast_lio2/enableFilter", enableFilter, true);
        nh_.param("/adaptive_filter/enableImu", enableImu, true);
        nh_.param("/adaptive_filter/enableWheel", enableWheel, true);
        nh_.param("/adaptive_filter/enableLidar", enableLidar, true);
        nh_.param("/adaptive_filter/filterFreq", filterFreq, std::string("l"));

        nh_.param("/adaptive_filter/lidarG", lidarG, float(75));
        nh_.param("/adaptive_filter/wheelG", wheelG, float(0.5));
        nh_.param("/adaptive_filter/imuG", imuG, float(100));

        //wheel odometry covariance adaptive positive constants
        nh_.param("/adaptive_filter/gamma_vx", gamma_vx, float(0.05));       
        nh_.param("/adaptive_filter/gamma_omegaz", gamma_omegaz, float(0.01));
        nh_.param("/adaptive_filter/delta_vx", delta_vx, float(0.0001));
        nh_.param("/adaptive_filter/delta_omegaz", delta_omegaz, float(0.00001));

        nh_.param("/adaptive_filter/imuTopic", imuTopic, std::string("/imu/data"));
        nh_.param("/adaptive_filter/wheelTopic", wheelTopic, std::string("/wheel_odom"));
        nh_.param("/adaptive_filter/FastLIO2_OdometryTopic", FastLIO2_OdometryTopic, std::string("/Odometry"));
        nh_.param("/adaptive_filter/filterTopic", filterTopic, std::string("/filter_odom"));
    }
    catch (int e)
    {
        ROS_INFO("\033[1;31m---->\033[0m Exception occurred when importing parameters in Adaptive Filter Node. Exception Nr. %d", e);
    }
    
    AdaptiveFilter AF;

    if (enableFilter){
        ROS_INFO("\033[1;32m---->\033[0m Adaptive Filter Started.");
        // runs
        AF.run();
    }else{
        ROS_INFO("\033[1;32m---->\033[0m Adaptive Filter Stopped.");
    }
    
    ros::spin();
    return 0;
}
