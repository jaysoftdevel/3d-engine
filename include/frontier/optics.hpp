#pragma once
#include "math.hpp"
#include <optional>
namespace rf::gfx {
// All lens lengths and propagation distances are metres; shutter time is seconds.
struct CameraOptics {
    float focalLengthMetres=.05f, sensorHeightMetres=.024f;
    float fNumber=2.8f, shutterSeconds=1.f/120, iso=100;
    bool physicalExposure=false;
    void validate() const;
    float verticalFovDegrees() const;
    double ev100() const;
    double exposureMultiplier() const;
    double circleOfConfusionMetres(double distanceMetres,double focusMetres) const;
    double blurRadiusPixels(double distanceMetres,double focusMetres,unsigned imageHeight) const;
};
// Photometry: lumen / solid angle = candela; candela / distance^2 = lux.
double pointCandela(double lumens);
double spotCandela(double lumens,double innerHalfAngleRadians,double outerHalfAngleRadians);
double illuminanceLux(double candela,double distanceMetres,double cosineIncidence=1);
// Linear RGB propagation helpers, not spectral transport.
V3 beerLambert(V3 absorptionPerMetre,double distanceMetres);
double dielectricFresnel(double cosineIncident,double incidentIor,double transmittedIor);
std::optional<V3> refractDirection(V3 incident,V3 normal,double incidentIor,double transmittedIor);
}
