#include "muslimtify.h"

#include "config.h"
#include "country.h"
#include "lib/muslimtify_internal.h"
#include "location.h"
#include "platform.h"
#include "prayertimes.h"
#include "string_util.h"

// The public code for a GPS status the caller should hear about. A fix and a
// transient no-fix are not problems, so they map to MUSLIMTIFY_OK.
static MuslimtifyError gps_error(GpsStatus status) {
  switch (status) {
  case GPS_NO_DAEMON:
    return MUSLIMTIFY_ERR_GPS_NO_DAEMON;
  case GPS_NO_DEVICE:
    return MUSLIMTIFY_ERR_GPS_NO_DEVICE;
  case GPS_NO_PERMISSION:
    return MUSLIMTIFY_ERR_GPS_NO_PERMISSION;
  case GPS_UNAVAILABLE:
    return MUSLIMTIFY_ERR_GPS_UNAVAILABLE;
  case GPS_OK:
  case GPS_NO_FIX:
    return MUSLIMTIFY_OK;
  }
  // No default label above, so -Wswitch fails the build if a new GpsStatus is
  // added without deciding what it maps to. This return only satisfies the
  // compiler's flow analysis.
  return MUSLIMTIFY_OK;
}

bool muslimtify_location_needs_detect(const Muslimtify *mt) {
  return mt && config_location_needs_detect(&mt->cfg);
}

MuslimtifyError muslimtify_detect_location_with(Muslimtify *mt,
                                                int (*detect)(Config *, GpsStatus *),
                                                MuslimtifyDetection *out) {
  if (!mt || !detect)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  // Detect into a copy so a failed or partial lookup cannot damage the stored
  // location. Config holds only fixed-size fields, so the copy is complete.
  Config candidate = mt->cfg;
  bool gps_was_on = candidate.use_gps;
  GpsStatus status = GPS_OK;
  if (detect(&candidate, &status) != 0)
    return MUSLIMTIFY_ERR_DETECT_FAILED;

  candidate.auto_detect = true;
  mt->cfg = candidate;

  if (out) {
    out->source = (gps_was_on && status == GPS_OK) ? MUSLIMTIFY_SOURCE_GPS : MUSLIMTIFY_SOURCE_IP;
    out->gps = gps_error(status);
    out->gps_disabled = gps_was_on && !candidate.use_gps;
  }
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_detect_location(Muslimtify *mt, MuslimtifyDetection *out) {
  return muslimtify_detect_location_with(mt, location_detect, out);
}

const char *muslimtify_detection_warning(const MuslimtifyDetection *detection) {
  if (!detection)
    return NULL;
  switch (detection->gps) {
  case MUSLIMTIFY_ERR_GPS_NO_DAEMON:
    return gps_status_message(GPS_NO_DAEMON);
  case MUSLIMTIFY_ERR_GPS_NO_DEVICE:
    return gps_status_message(GPS_NO_DEVICE);
  case MUSLIMTIFY_ERR_GPS_NO_PERMISSION:
    return gps_status_message(GPS_NO_PERMISSION);
  case MUSLIMTIFY_ERR_GPS_UNAVAILABLE:
    return gps_status_message(GPS_UNAVAILABLE);
  default:
    return NULL;
  }
}

MuslimtifyError muslimtify_set_gps_with(Muslimtify *mt, bool enabled, GpsStatus (*probe)(Config *),
                                        bool *has_fix) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  if (!enabled) {
    mt->cfg.use_gps = false;
    if (has_fix)
      *has_fix = false;
    return MUSLIMTIFY_OK;
  }
  if (!probe)
    return MUSLIMTIFY_ERR_INVALID_ARG;

  // Probe into a copy: only a real fix may move the stored coordinates.
  Config candidate = mt->cfg;
  GpsStatus status = probe(&candidate);
  // Turning GPS on means the location is detected automatically, receiver
  // first, so auto-detect goes on with it. Otherwise a location set by hand
  // would leave GPS switched on and never consulted again.
  if (status == GPS_OK) {
    candidate.use_gps = true;
    candidate.auto_detect = true;
    mt->cfg = candidate;
    if (has_fix)
      *has_fix = true;
    return MUSLIMTIFY_OK;
  }
  if (status == GPS_NO_FIX) {
    // A receiver is present, so GPS engages once it gets a fix.
    mt->cfg.use_gps = true;
    mt->cfg.auto_detect = true;
    if (has_fix)
      *has_fix = false;
    return MUSLIMTIFY_OK;
  }
  return gps_error(status);
}

MuslimtifyError muslimtify_set_gps(Muslimtify *mt, bool enabled, bool *has_fix) {
  return muslimtify_set_gps_with(mt, enabled, location_fetch_gps, has_fix);
}

MuslimtifyError muslimtify_set_method_from_country(Muslimtify *mt) {
  if (!mt)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  copy_string(mt->cfg.calculation_method, sizeof(mt->cfg.calculation_method),
              method_to_string(country_default_method(mt->cfg.country)));
  return MUSLIMTIFY_OK;
}

MuslimtifyError muslimtify_check_country(const char *iso2) {
  if (!iso2)
    return MUSLIMTIFY_ERR_INVALID_ARG;
  return country_is_valid_alpha2(iso2) ? MUSLIMTIFY_OK : MUSLIMTIFY_ERR_INVALID_COUNTRY;
}
