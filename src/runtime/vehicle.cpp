#include "jsd/vehicle.hpp"
#include <numbers>
#include <stdexcept>

namespace jsd {
namespace {
bool finite(V3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
V3 tangent(V3 center, V3 axle, V3 normal) {
    if (!finite(center) || !finite(axle) || !finite(normal) ||
        rf::length(axle) < 1e-6f || rf::length(normal) < 1e-6f)
        throw std::invalid_argument("Invalid rolling-wheel position or contact frame");
    V3 result = rf::cross(rf::normalized(axle), rf::normalized(normal));
    if (rf::length(result) < 1e-6f)
        throw std::invalid_argument("Wheel axle cannot be parallel to its contact normal");
    return rf::normalized(result);
}
}
void SuspensionSpring::update(double target,double seconds,double hz){
 if(!std::isfinite(target+seconds+hz)||seconds<=0||seconds>.25||hz<=0)
  throw std::invalid_argument("Invalid suspension spring step");
 // Exact critically damped response for this contact target, stable at any supported tick rate.
 double w=2*std::numbers::pi*hz,x=position_-target,c=velocity_+w*x,e=std::exp(-w*seconds);
 position_=target+(x+c*seconds)*e;velocity_=(velocity_-w*c*seconds)*e;
}
std::optional<WheelSupport> wheelSupport(const PhysicsWorld&world,V3 hub,V3 axle,float radius,
 const std::function<float(float,float)>&terrain,QueryFilter filter){
 if(!finite(hub)||!finite(axle)||!std::isfinite(radius)||radius<=0||rf::length(axle)<.1f)
  throw std::invalid_argument("Invalid wheel support probe");
 axle=rf::normalized(axle);V3 up=rf::normalized(V3{0,1,0}-axle*axle.y);
 if(up.y<.1f)throw std::invalid_argument("Wheel axle too vertical for suspension probe");
 V3 forward=rf::normalized(rf::cross(axle,up));
 struct Sample {float offset=0;std::optional<WheelSupport> contact;};
 std::optional<WheelSupport> best;
 auto sample=[&](float offset){
  float vertical=std::sqrt(std::max(0.f,radius*radius-offset*offset));
  V3 p=hub+forward*offset-up*vertical;
  std::optional<WheelSupport> result;
  if(terrain){float h=terrain(p.x,p.z);if(!std::isfinite(h))throw std::invalid_argument("Nonfinite support terrain");
   if(h<=hub.y+radius+.5f)result=WheelSupport{{p.x,h,p.z},{0,1,0},{},h+up.y*vertical};}
  auto hit=world.rayCast({p.x,hub.y+radius+.5f,p.z},{p.x,hub.y-radius-1.5f,p.z},filter);
  if(hit&&hit->normal.y>.15f&&(!result||hit->position.y>result->point.y+.0001f))
   result=WheelSupport{hit->position,hit->normal,hit->body,hit->position.y+up.y*vertical};
  if(result&&(!best||result->hubHeight>best->hubHeight)){
   result->normal=rf::normalized(up*vertical-forward*offset);best=result;
  }
  return Sample{offset,result};
 };
 Sample previous=sample(-radius);
 for(int i=1;i<=12;i++){
  auto current=sample(radius*(-1+2*i/12.f));
  if(previous.contact&&current.contact&&
     (previous.contact->body!=current.contact->body||std::abs(previous.contact->point.y-current.contact->point.y)>.07f)){
   auto a=previous,b=current;
   // Refine actual collision-surface boundaries so a curb cannot cause a one-ray jump.
   for(int j=0;j<7;j++){auto m=sample((a.offset+b.offset)*.5f);
    if(!m.contact)break;
    if(m.contact->body==a.contact->body&&std::abs(m.contact->point.y-a.contact->point.y)<.035f)a=m;else b=m;
   }
  }
  previous=current;
 }
 return best;
}
RollingWheel::RollingWheel(double radius) : radius_(radius) {
    if (!std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Wheel radius must be finite and positive");
}
void RollingWheel::reset(V3 center, V3 axle, V3 normal) {
    V3 direction = tangent(center, axle, normal);
    position_ = center;
    tangent_ = direction;
    distance_ = angularSpeed_ = 0;
    initialized_ = true;
}
void RollingWheel::advance(V3 center, V3 axle, V3 normal, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        throw std::invalid_argument("Wheel step duration must be finite and positive");
    V3 direction = tangent(center, axle, normal);
    if (!initialized_) { reset(center, axle, normal); return; }
    V3 average = rf::normalized(tangent_ + direction);
    if (rf::length(average) < 1e-6f)
        throw std::invalid_argument("Reset rolling history after discontinuous axle changes");
    // Project onto the rolling tangent: suspension bounce and lateral slip do not add spin.
    double travel = (double(center.x) - position_.x) * average.x +
                    (double(center.y) - position_.y) * average.y +
                    (double(center.z) - position_.z) * average.z;
    double accumulated = distance_ + travel;
    double speed = travel / (radius_ * seconds);
    if (!std::isfinite(accumulated / radius_) || !std::isfinite(speed))
        throw std::invalid_argument("Rolling-wheel motion exceeds the supported numeric range");
    distance_ = accumulated;
    angularSpeed_ = speed;
    position_ = center;
    tangent_ = direction;
}
double RollingWheel::phase() const { return std::remainder(angle(), 2 * std::numbers::pi); }
} // namespace jsd
