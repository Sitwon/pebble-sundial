import { test } from 'node:test';
import { strictEqual } from 'node:assert';
import { parseCoord, parseLocation } from '../src/pkjs/settings.js';

// parseCoord tests
test('parseCoord: normal latitudes', () => {
  strictEqual(parseCoord('51.4779', -90, 90), 514779);
  strictEqual(parseCoord('-33.8688', -90, 90), -338688);
  strictEqual(parseCoord('0', -90, 90), 0);
});

test('parseCoord: boundary values', () => {
  strictEqual(parseCoord('90', -90, 90), 900000);
  strictEqual(parseCoord('-90', -90, 90), -900000);
});

test('parseCoord: rounding', () => {
  strictEqual(parseCoord('51.47799', -90, 90), 514780);
  strictEqual(parseCoord('51.47794', -90, 90), 514779);
});

test('parseCoord: rejects invalid', () => {
  strictEqual(parseCoord('', -90, 90), null);
  strictEqual(parseCoord('abc', -90, 90), null);
  strictEqual(parseCoord('91', -90, 90), null);
  strictEqual(parseCoord('-91', -90, 90), null);
  strictEqual(parseCoord('NaN', -90, 90), null);
  strictEqual(parseCoord('Infinity', -90, 90), null);
});

// parseLocation tests
test('parseLocation: valid pair', () => {
  const result = parseLocation('51.4779', '0');
  strictEqual(result.lat_e4, 514779);
  strictEqual(result.lon_e4, 0);
});

test('parseLocation: invalid lat returns null', () => {
  strictEqual(parseLocation('91', '0'), null);
});

test('parseLocation: invalid lon returns null', () => {
  strictEqual(parseLocation('51.4779', '181'), null);
});
