
# EKF-Fast-LIO2

This module integrates a customized Extended Kalman Filter (EKF) into the Fast-LIO2 codebase. The EKF performs sensor fusion using:

- **Fast-LIO2 odometry**
- **Wheel encoder odometry**
- **IMU data** 

The EKF is adapted from the `adaptive_filter` module of EKF-LOAM and incorporates dynamic confidence adjustment based on sensor noise profiles.

---

## 📦 Features

- Real-time adaptive sensor fusion.
- Configurable parameters through a YAML file.
- Supports LiDAR, wheel, and IMU odometry streams.
- Lightweight implementation with extendable structure.

---

## 🚀 How to Launch

1. Ensure all required topics are being published:
    - `/Odometry` (Fast-LIO2)
    - `/wheel_odom` (wheel encoders)
    - `/imu/data` (IMU)

2. Run the launch file:
```bash
roslaunch ekf_fast_lio2 ekf_fast_lio2.launch
```

---

## 🔧 Configuration

The configuration file is located at `config/adaptive_filter_parameters.yaml`. Key parameters include:

```yaml
 # Filter settings
  enableImu: true
  enableWheel: true
  enableLidar: true
  filterFreq: "l"
  
  # Covariance gains
  lidarG: 75
  wheelG: 0.5
  imuG: 100

  # Topic names
  imuTopic: "/imu/data"
  wheelTopic: "/wheel_odom"
  FastLIO2_OdometryTopic: "/Odometry"
  filterTopic: "/filter_odom"

  # Wheel odometry covariance adaptive positive constants
  gamma_vx: 0.05
  gamma_omegaz: 0.01
  delta_vx: 0.0001
  delta_omegaz: 0.00001
```

---

## 📤 Output

- `/filter_odom` (type: `nav_msgs/Odometry`)

---

## 📂 File Structure


This EKF module is merged into the existing **Fast-LIO2** directory structure as follows:

```
EKF-Fast-LIO2/
├── src/
│   └── EKFAdaptiveFilter.cpp         # EKF filter implementation
├── include/
│   └── settings_adaptive_filter.h   # EKF parameter definitions
├── config/
│   └── adaptive_filter_parameters.yaml  # YAML configuration file
├── launch/
│   └── ekf.launch                   # ROS launch file (optional)
```

---

## 🧩 Dependencies

- `roscpp`
- `std_msgs`
- `geometry_msgs`
- `nav_msgs`
- `sensor_msgs`
- `tf`
- **Fast-LIO2 core dependencies**

---
