/* sundial/pkjs/config.js — Clay settings page definition */
module.exports = [
  {
    'type': 'toggle',
    'label': 'Use phone GPS for location',
    'key': 'useGps',
    'defaultValue': false
  },
  {
    'type': 'input',
    'label': 'Latitude (-90..90)',
    'key': 'lat',
    'defaultValue': '51.4779'
  },
  {
    'type': 'input',
    'label': 'Longitude (-180..180)',
    'key': 'lon',
    'defaultValue': '-0.0015'
  },
  {
    'type': 'text',
    'defaultValue': 'Sundial shows apparent solar time, which can differ from clock time by up to ±16 minutes. Accurate between latitudes 15° and 75°. No digital time is shown, by design.'
  },
  {
    'type': 'submit',
    'defaultValue': 'Save'
  }
];
