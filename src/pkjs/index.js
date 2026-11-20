/* sundial/pkjs/index.js — GPS + manual location intake, Clay settings (steps 6.3/6.4) */

var settings = require('./settings.js');
var Clay = require('@rebble/clay');
var clay = new Clay(require('./config.js'), null, { autoHandleEvents: false });

/* Message keys, in package.json messageKeys order (keys start at 10000). */
var KEY = {
  REQUEST_LOCATION: 10000,
  LAT_E4: 10001,
  LON_E4: 10002,
  LOCATION_SOURCE: 10003,
  LOCATION_VALID: 10004
};

var GPS_RETRY_MS = 10 * 60 * 1000;
var lastGpsRequest = 0;

function sendLocation(latE4, lonE4, source) {
  Pebble.sendAppMessage({
    '10001': latE4,
    '10002': lonE4,
    '10003': source,
    '10004': 1
  });
}

function sendManual() {
  var s = settings.loadSettings();
  var loc = settings.parseLocation(String(s.lat), String(s.lon));
  if (loc) sendLocation(loc.lat_e4, loc.lon_e4, 1);
}

function sendGps() {
  var now = Date.now();
  if (now - lastGpsRequest < GPS_RETRY_MS) return;
  lastGpsRequest = now;

  navigator.geolocation.getCurrentPosition(function (pos) {
    var latE4 = Math.round(pos.coords.latitude * 10000);
    var lonE4 = Math.round(pos.coords.longitude * 10000);
    /* clamp to the watch's accepted range */
    latE4 = Math.max(-900000, Math.min(900000, latE4));
    lonE4 = Math.max(-1800000, Math.min(1800000, lonE4));
    sendLocation(latE4, lonE4, 2);
  }, function () {
    sendManual();
  }, { enableHighAccuracy: true, timeout: 15000, maximumAge: 60000 });
}

function updateLocation() {
  var s = settings.loadSettings();
  if (s.useGps) sendGps();
  else sendManual();
}

Pebble.addEventListener('ready', updateLocation);

/* Watch asks for a location (launch, reconnect). */
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload['10000'] !== undefined) updateLocation();
});

/* Clay settings page. */
Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var cfg = clay.getSettings(e.response, false);
  var loc = settings.parseLocation(String(cfg.lat), String(cfg.lon));
  /* invalid coordinates: keep the previous values, don't resend */
  if (!loc) return;
  settings.saveSettings({ useGps: !!cfg.useGps, lat: String(cfg.lat), lon: String(cfg.lon) });
  updateLocation();
});


