
# EKF-Fast-LIO2

This ROS package implements a customized Extended Kalman Filter (EKF) for sensor fusion using:

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

```
ekf_fast_lio2/
├── src/
│   └── EKFAdaptiveFilter.cpp
├── include/
│   └── settings_adaptive_filter.h
├── config/
│   └── adaptive_filter_parameters.yaml
├── launch/
│   └── ekf_fast_lio2.launch
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## 🧩 Dependencies

- `roscpp`
- `std_msgs`
- `geometry_msgs`
- `nav_msgs`
- `sensor_msgs`
- `tf`

---
