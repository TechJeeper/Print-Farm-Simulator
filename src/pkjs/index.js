// Phone side: Clay settings → AppMessage on the watch.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

function asInt(value, fallback) {
  var n = parseInt(value, 10);
  return isNaN(n) ? fallback : n;
}

function asBool(value, fallback) {
  if (value === true || value === false) return value;
  if (value === 'true' || value === '1') return true;
  if (value === 'false' || value === '0') return false;
  return fallback;
}

function pushSettings(settings) {
  settings = settings || clay.getSettings();
  var vibe = settings[messageKeys.VibeEnabled];
  if (vibe === undefined) vibe = settings.VibeEnabled;
  var quiet = settings[messageKeys.QuietHours];
  if (quiet === undefined) quiet = settings.QuietHours;
  var freq = settings[messageKeys.CrisisFrequency];
  if (freq === undefined) freq = settings.CrisisFrequency;

  Pebble.sendAppMessage({
    VibeEnabled: asBool(vibe, true) ? 1 : 0,
    QuietHours: asBool(quiet, false) ? 1 : 0,
    CrisisFrequency: asInt(freq, 1)
  });
}

Pebble.addEventListener('ready', function () {
  pushSettings();
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  pushSettings(clay.getSettings(e.response));
});
