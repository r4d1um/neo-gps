/*
 * neo_gps - Arduino example (ESP32, Mega, Uno with SoftwareSerial, RP2040 ...)
 *
 * Wiring:  module TX -> MCU RX,  module RX -> MCU TX,  VCC -> 3.3V/5V,  GND -> GND
 * Factory default of all three modules: 9600 baud, NMEA.
 */
#include <neo_gps.h>
#include <neo_gps_time.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define GPS_SERIAL Serial1
  #define GPS_BEGIN(b) Serial1.begin(b, SERIAL_8N1, 16 /*RX*/, 17 /*TX*/)
#elif defined(HAVE_HWSERIAL1)
  #define GPS_SERIAL Serial1
  #define GPS_BEGIN(b) Serial1.begin(b)
#else
  #include <SoftwareSerial.h>
  SoftwareSerial gpsSerial(4 /*RX*/, 3 /*TX*/);
  #define GPS_SERIAL gpsSerial
  #define GPS_BEGIN(b) gpsSerial.begin(b)
#endif

static neo_gps_t gps;
static const neo_tz_t berlin = NEO_TZ_BERLIN;   // CET/CEST, DST automatic

static void gps_write(void *user, const uint8_t *data, uint16_t len)
{
  (void)user;
  GPS_SERIAL.write(data, len);
}

void setup()
{
  Serial.begin(115200);
  GPS_BEGIN(9600);
  neo_gps_init(&gps, gps_write, NULL);

  neo_gps_detect_model(&gps);                    // the answer arrives asynchronously in loop()
  neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL); // only RMC/GGA/GSA -> less CPU load
  neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);
  neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN); // reject GPS jumps, smooth while standing still
}

void loop()
{
  while (GPS_SERIAL.available()) {
    neo_event_t ev = neo_gps_feed(&gps, (uint8_t)GPS_SERIAL.read());
    if (ev == NEO_EV_UBX_MON_VER) {
      static const char *names[] = { "unknown", "NEO-6M", "NEO-7M", "NEO-M8N" };
      Serial.print(F("Module: "));
      Serial.println(names[neo_gps_model(&gps)]);
    }
  }

  uint16_t upd = neo_gps_updates(&gps);
  if (upd & NEO_UPD_POSITION) {
    const neo_gps_data_t *d = neo_gps_data(&gps);
    neo_local_time_t lt;
    char buf[96];
    if (neo_gps_local_time(d, &berlin, &lt)) {
      snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u %s  ",
               lt.t.year, lt.t.month, lt.t.day, lt.t.hour, lt.t.minute, lt.t.second,
               lt.is_dst ? "CEST" : "CET");
      Serial.print(buf);
    }
    // integer output: exact even on AVR (where double is only 32 bit)
    snprintf(buf, sizeof(buf), "%s%ld.%07ld %s%ld.%07ld  sv=%u",
             d->lat < 0 ? "-" : "", labs(d->lat) / 10000000L, labs(d->lat) % 10000000L,
             d->lon < 0 ? "-" : "", labs(d->lon) / 10000000L, labs(d->lon) % 10000000L,
             d->num_sv);
    Serial.print(buf);
    Serial.print(F("  alt ")); Serial.print(neo_gps_alt_m(&gps), 1);
    Serial.print(F(" m  ")); Serial.print(neo_gps_speed_kmh(&gps), 1);
    Serial.println(F(" km/h"));
  } else if (upd & NEO_UPD_REJECTED) {
    static const char *why[] = { "", "fix", "satellites", "HDOP", "accuracy", "jump" };
    Serial.print(F("Position rejected: "));
    Serial.println(why[gps.data.reject_reason]);
  } else if ((upd & NEO_UPD_FIX) && !neo_gps_has_fix(&gps)) {
    Serial.println(F("Waiting for fix ..."));
  }
}
