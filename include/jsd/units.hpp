#pragma once
#include <compare>
#include <cmath>
#include <stdexcept>
namespace jsd::si {
// SI base dimensions: length (m), mass (kg), time (s). Values use double precision.
template<int L,int M,int T> class Quantity {
    double value_ = 0;
public:
    constexpr Quantity() = default;
    explicit constexpr Quantity(double value) : value_(value) {}
    constexpr double value() const { return value_; }
    constexpr auto operator<=>(const Quantity &) const = default;
    constexpr Quantity operator+(Quantity v) const { return Quantity(value_ + v.value_); }
    constexpr Quantity operator-(Quantity v) const { return Quantity(value_ - v.value_); }
    constexpr Quantity operator-() const { return Quantity(-value_); }
    constexpr Quantity operator*(double scale) const { return Quantity(value_ * scale); }
    constexpr Quantity operator/(double scale) const { return Quantity(value_ / scale); }
};
template<int L,int M,int T> constexpr Quantity<L,M,T> operator*(double s,Quantity<L,M,T> q){return q*s;}
template<int L,int M,int T,int A,int B,int C>
constexpr Quantity<L+A,M+B,T+C> operator*(Quantity<L,M,T> a,Quantity<A,B,C> b){return Quantity<L+A,M+B,T+C>(a.value()*b.value());}
template<int L,int M,int T,int A,int B,int C>
constexpr Quantity<L-A,M-B,T-C> operator/(Quantity<L,M,T> a,Quantity<A,B,C> b){return Quantity<L-A,M-B,T-C>(a.value()/b.value());}
using Length=Quantity<1,0,0>; using Mass=Quantity<0,1,0>; using Time=Quantity<0,0,1>;
using Velocity=Quantity<1,0,-1>; using Acceleration=Quantity<1,0,-2>;
using Force=Quantity<1,1,-2>; using Impulse=Quantity<1,1,-1>;
using Energy=Quantity<2,1,-2>; using Torque=Energy; using Inertia=Quantity<2,1,0>;
using Density=Quantity<-3,1,0>; using Volume=Quantity<3,0,0>; using Damping=Quantity<0,0,-1>;
constexpr Length metres(double v){return Length(v);} constexpr Length millimetres(double v){return Length(v*.001);}
constexpr Mass kilograms(double v){return Mass(v);} constexpr Time seconds(double v){return Time(v);}
constexpr Force newtons(double v){return Force(v);} constexpr Density kilogramsPerCubicMetre(double v){return Density(v);}
inline constexpr Acceleration standardGravity{9.80665};
inline double dampingFactor(Damping rate,Time dt){
    if(!std::isfinite(rate.value())||rate.value()<0||!std::isfinite(dt.value())||dt.value()<0)
        throw std::invalid_argument("Invalid SI damping rate or duration");
    return std::exp(-rate.value()*dt.value());
}
}
