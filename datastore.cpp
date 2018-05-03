#include "datastore.h"
#include <Arduino.h>
#include <EEPROM.h>

std::shared_ptr<DataStore> DataStore::m_instance;

static const char door_position_strings [][20] PROGMEM = {"STARTUP","UNKNOWN","OPEN","CLOSED","OPEN_CLOSED","CLOSED_OPEN","MANUAL_OPEN_CLOSED","MANUAL_CLOSED_OPEN"};

void handleInHomeAreaMessage(AdafruitIO_Data *data) {
  DataStore::get()->inHomeMessage(data);
}

DataStore::DataStore():
  door_position(dpStartup),
  // start_move_door_position(dpStartup),
  door_moving(false),
  in_geofence(false),
  is_locked(false)
{

}

std::shared_ptr<DataStore> DataStore::init(){
  m_instance = std::make_shared<DataStore>();
  return m_instance;
}

std::shared_ptr<DataStore> DataStore::get(){
  return m_instance;
}

void DataStore::initFeeds(AdafruitIO_WiFi &io){
  io_door_action = std::shared_ptr<AdafruitIO_Feed>(io.feed(IO_FEED_DOOR_ACTION));
  io_door_position = std::shared_ptr<AdafruitIO_Feed>(io.feed(IO_FEED_POSITION));
  io_garage_temperature = std::shared_ptr<AdafruitIO_Feed>(io.feed(IO_FEED_TEMPERATURE));
  io_in_home_area = std::shared_ptr<AdafruitIO_Feed>(io.feed(IO_FEED_IN_HOME_AREA));
}

void DataStore::afterIOConnect(){
  io_door_action_value = io_door_action->lastValue()->toString();
  io_door_position_value = io_door_position->lastValue()->toString();

  EEPROM.begin(EEPROM_SIZE);

  io_in_home_area->onMessage(handleInHomeAreaMessage);
  in_geofence = EEPROM.read(EEPROM_IN_GEOFENCE_ADDR) == 1;
  Serial.println(in_geofence ? F("restored inside geofence") : F("restored outside geofence"));

  is_locked = EEPROM.read(EEPROM_LOCKED_ADDR) == 1;
  Serial.println(is_locked ? F("restored locked") : F("restored unlocked"));
  EEPROM.end();

}

void DataStore::inHomeMessage(AdafruitIO_Data *data){
  Serial.print(F("Action: Received in home area -> "));
  Serial.print(data->value());
  
  in_geofence = strcmp_P(data->value(), PSTR("entered")) == 0;
  Serial.println(in_geofence ? F(" inside geofence") : F(" outside geofence"));

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_IN_GEOFENCE_ADDR, in_geofence ? 1 : 0);
  EEPROM.end();

}

bool DataStore::doorAction(AdafruitIO_Data *data){
  Serial.print(F("Action: Received door action -> "));
  io_door_action_value = data->value();
  Serial.println(io_door_action_value);

  if(!in_geofence){
    Serial.println(F("Not near home, won't action door!"));
    return false;
  }

  return true;
}


void DataStore::updateDoorPosition(doorPositions current_door_position, doorPositions _door_position){
  if (current_door_position != _door_position){
    door_position = _door_position;
    Serial.print(F("Action: Update Door Position:"));
    Serial.print(FPSTR(door_position_strings[current_door_position]));
    Serial.print(F(" --> "));
    io_door_position_value =  String(FPSTR(door_position_strings[door_position]));
    Serial.println(io_door_position_value);
    io_door_position->save(io_door_position_value);
  }
}

void DataStore::updateDoorPosition(doorPositions current_door_position, doorPositions _door_position, bool _door_moving){
  door_moving = _door_moving;
  updateDoorPosition(current_door_position, _door_position);
}

void DataStore::updateTemperature(float temp){
  io_garage_temperature_value = String(temp,2);
  Serial.print(F("Temperature: "));
  Serial.println(io_garage_temperature_value);
  io_garage_temperature->save(temp);
}

void DataStore::toggleLocked(){
  setLocked(!is_locked);
}

void DataStore::setLocked(bool locked){
  Serial.print(F("Setting lock to "));
  Serial.println(is_locked ? F("locked") : F("unlocked"));
  digitalWrite(LED_RED, is_locked ? LED_ON : LED_OFF);
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_LOCKED_ADDR, is_locked ? 1 : 0);
  EEPROM.end();
}