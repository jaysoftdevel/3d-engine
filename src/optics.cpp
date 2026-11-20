#include <frontier/optics.hpp>
#include <stdexcept>
#include <numbers>
namespace rf::gfx {
namespace {
void positive(double n){if(!std::isfinite(n)||n<=0)throw std::invalid_argument("Invalid optical quantity");}
void nonnegative(double n){if(!std::isfinite(n)||n<0)throw std::invalid_argument("Negative optical quantity");}
constexpr double pi=std::numbers::pi;
}
void CameraOptics::validate()const{
    positive(focalLengthMetres);positive(sensorHeightMetres);positive(fNumber);positive(shutterSeconds);positive(iso);
    double fov=2*std::atan(double(sensorHeightMetres)/(2*focalLengthMetres))*180/pi;
    if(fov<10||fov>150||fNumber<.1||fNumber>128||shutterSeconds>60||iso>1e7)
        throw std::invalid_argument("Lens/exposure outside supported camera range");
}
float CameraOptics::verticalFovDegrees()const{validate();return float(2*std::atan(double(sensorHeightMetres)/(2*focalLengthMetres))*180/pi);}
double CameraOptics::ev100()const{validate();return std::log2(double(fNumber)*fNumber/shutterSeconds*100/iso);}
double CameraOptics::exposureMultiplier()const{return 1/(1.2*std::exp2(ev100()));}
double CameraOptics::circleOfConfusionMetres(double distance,double focus)const{
    validate();positive(distance);positive(focus);if(focus<=focalLengthMetres)throw std::invalid_argument("Focus must be beyond focal length");
    return double(focalLengthMetres)*focalLengthMetres*std::abs(distance-focus)/(fNumber*distance*(focus-focalLengthMetres));
}
double CameraOptics::blurRadiusPixels(double distance,double focus,unsigned height)const{return .5*circleOfConfusionMetres(distance,focus)*height/sensorHeightMetres;}
double pointCandela(double lumens){nonnegative(lumens);return lumens/(4*pi);}
double spotCandela(double lumens,double inner,double outer){
    nonnegative(lumens);nonnegative(inner);positive(outer);if(inner>=outer||outer>=pi/2)throw std::invalid_argument("Spot angles require 0 <= inner < outer < pi/2");
    // Integral of the shader's smoothstep cone profile over solid angle.
    return lumens/(2*pi*(1-.5*(std::cos(inner)+std::cos(outer))));
}
double illuminanceLux(double candela,double distance,double cosine){nonnegative(candela);positive(distance);if(!std::isfinite(cosine)||cosine<0||cosine>1)throw std::invalid_argument("Invalid incidence cosine");return candela*cosine/(distance*distance);}
V3 beerLambert(V3 a,double distance){nonnegative(a.x);nonnegative(a.y);nonnegative(a.z);nonnegative(distance);return {float(std::exp(-a.x*distance)),float(std::exp(-a.y*distance)),float(std::exp(-a.z*distance))};}
double dielectricFresnel(double cosine,double n1,double n2){
    positive(n1);positive(n2);if(!std::isfinite(cosine)||cosine<0||cosine>1)throw std::invalid_argument("Invalid Fresnel cosine");
    if(n1==n2)return 0;
    double sin2=n1*n1/(n2*n2)*(1-cosine*cosine);if(sin2>=1)return 1;
    double ct=std::sqrt(1-sin2),rs=(n1*cosine-n2*ct)/(n1*cosine+n2*ct),rp=(n2*cosine-n1*ct)/(n2*cosine+n1*ct);
    return .5*(rs*rs+rp*rp);
}
std::optional<V3> refractDirection(V3 incident,V3 normal,double n1,double n2){
    positive(n1);positive(n2);
    for(float v:{incident.x,incident.y,incident.z,normal.x,normal.y,normal.z})if(!std::isfinite(v))throw std::invalid_argument("Nonfinite optical direction");
    if(length(incident)<1e-8||length(normal)<1e-8)throw std::invalid_argument("Zero optical direction");
    incident=normalized(incident);normal=normalized(normal);double cosine=-dot(incident,normal);
    if(cosine<0)throw std::invalid_argument("Normal must face the incident ray");
    double eta=n1/n2,k=1-eta*eta*(1-cosine*cosine);if(k<0)return {};
    return normalized(incident*float(eta)+normal*float(eta*cosine-std::sqrt(k)));
}
}
