#include <iostream>
#include <string>

enum class TrafficLight { RED, YELLOW, GREEN };
std::string get_action(TrafficLight color) {
  switch (color) {
  case TrafficLight::RED:
    return "Stop";
  case TrafficLight::YELLOW:
    return "Caution";
  case TrafficLight::GREEN:
    return "Go";
  default:
    return "Invalid State";
  }
}

int main() {
  TrafficLight current_light = TrafficLight::GREEN;

  std::cout << "Current light is GREEN. Action: " << get_action(current_light)
            << std::endl;
  std::cout << "Current light is RED. Action: " << get_action(TrafficLight::RED)
            << std::endl;

  TrafficLight invalid_light = static_cast<TrafficLight>(5);
  std::cout << "Current light is INVALID. Action: " << get_action(invalid_light)
            << std::endl;

  return 0;
}
