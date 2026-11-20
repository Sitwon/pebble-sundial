/* sundial/pkjs/settings.js — coordinate parsing and defaults */

/* Parse a decimal-degree string to E4 (×10000) integer.
 * Returns null on invalid input or out-of-range. */
function parseCoord(str, min, max) {
  if (typeof str !== 'string' || str.trim() === '') return null;
  const v = parseFloat(str);
  if (isNaN(v) || !isFinite(v)) return null;
  if (v < min || v > max) return null;
  return Math.round(v * 10000);
}

/* Validate a lat/lon pair. Returns {lat_e4, lon_e4} or null. */
function parseLocation(latStr, lonStr) {
  var lat = parseCoord(latStr, -90, 90);
  var lon = parseCoord(lonStr, -180, 180);
  if (lat === null || lon === null) return null;
  return { lat_e4: lat, lon_e4: lon };
}

/* Default location: Greenwich. */
var DEFAULT_LAT = 51.4779;
var DEFAULT_LON = -0.0025;

/* Load stored settings from localStorage; fall back to defaults. */
function loadSettings() {
  var useGps = localStorage.getItem('useGps');
  var lat = localStorage.getItem('lat');
  var lon = localStorage.getItem('lon');
  return {
    useGps: useGps === 'true',
    lat: lat || DEFAULT_LAT,
    lon: lon || DEFAULT_LON,
  };
}

/* Save settings to localStorage. */
function saveSettings(settings) {
  localStorage.setItem('useGps', settings.useGps ? 'true' : 'false');
  localStorage.setItem('lat', settings.lat);
  localStorage.setItem('lon', settings.lon);
}

module.exports = { parseCoord, parseLocation, loadSettings, saveSettings, DEFAULT_LAT, DEFAULT_LON };
