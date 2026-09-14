#pragma once

#include <cmath>
#include <cstdlib>
#include <random>

namespace GMSMath {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;

inline double dsin(double degrees) {
    return std::sin(degrees * DEG2RAD);
}

inline double dcos(double degrees) {
    return std::cos(degrees * DEG2RAD);
}

inline double dtan(double degrees) {
    return std::tan(degrees * DEG2RAD);
}

inline double darcsin(double val) {
    return std::asin(std::max(-1.0, std::min(1.0, val))) * RAD2DEG;
}

inline double darccos(double val) {
    return std::acos(std::max(-1.0, std::min(1.0, val))) * RAD2DEG;
}

inline double darctan(double val) {
    return std::atan(val) * RAD2DEG;
}

inline double darctan2(double y, double x) {
    double angle = std::atan2(y, x) * RAD2DEG;
    if (angle < 0.0) angle += 360.0;
    return angle;
}

inline double point_distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline double point_distance_3d(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline double point_direction(double x1, double y1, double x2, double y2) {
    double angle = darctan2(-(y2 - y1), x2 - x1);
    return angle;
}

inline double lengthdir_x(double len, double dir) {
    return len * dcos(dir);
}

inline double lengthdir_y(double len, double dir) {
    return -len * dsin(dir);
}

inline double lengthdir_z(double len, double dir) {
    return -len * dsin(dir);
}

inline double angle_difference(double dest, double src) {
    double diff = std::fmod(dest - src + 180.0, 360.0);
    if (diff < 0) diff += 360.0;
    return diff - 180.0;
}

inline double clamp(double val, double minVal, double maxVal) {
    return std::max(minVal, std::min(maxVal, val));
}

inline double lerp(double a, double b, double amt) {
    return a + (b - a) * amt;
}

inline double map_range(double val, double inMin, double inMax, double outMin, double outMax) {
    return outMin + (val - inMin) * (outMax - outMin) / (inMax - inMin);
}

inline double gms_random(double maxVal) {
    return (static_cast<double>(std::rand()) / RAND_MAX) * maxVal;
}

inline int gms_irandom(int maxVal) {
    if (maxVal <= 0) return 0;
    return std::rand() % (maxVal + 1);
}

inline double gms_random_range(double minVal, double maxVal) {
    return minVal + gms_random(maxVal - minVal);
}

inline int gms_irandom_range(int minVal, int maxVal) {
    return minVal + gms_irandom(maxVal - minVal);
}

} // namespace GMSMath
