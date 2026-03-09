# Pebble Health Companion

**Note**: I don't have a working Pebble anymore. This repository is archived and will not be updated.

This is a fork of Natacha Porté's excellent Pebble Health Export watchapp. It has been modified to work with a native Android companion application instead of the standard PebbleKit JS.

## What's Different?

The original `Pebble Health Export` used a JavaScript component running on the phone to configure and upload health data to a web server. This fork removes all of that JavaScript logic.

Instead, this watchapp simply sends the raw health data, minute by minute, to the phone via `AppMessage`. A separate, native Android application is required to listen for these messages and handle the data upload.

**This repository only contains the Pebble watchapp. You will need to provide your own native Android companion app.**

The `src/js/app.js` file is merely a stub to allow the watchapp to function with the Pebble Android app. All configuration and upload logic (setting the server URL, formatting the request, etc.) must now be handled by the native companion app.

## How it Works

1.  When you start the app on your watch, it requests the timestamp of the last successfully sent data point from the companion app.
2.  The watchapp begins reading all health data from the watch's internal storage, starting from where it left off.
3.  For each minute of data, it sends a message to the phone containing a CSV string.
4.  The watchapp displays the progress of the export.
5.  The watchapp now stores the timestamp of the last acknowledged data point in its own persistent storage. This makes the sync process more resilient, as it can resume correctly even if the companion app "forgets" the last sync point.

## Data Format

The watchapp sends one `AppMessage` per minute of health data. The message dictionary contains the data line as a string with the key `MSG_KEY_DATA_LINE` (220). The format of the string is a CSV line like this:

```
2016-05-25T21:22:00Z,120,0,4,9915,1,0,0
```

The fields are:

- **absolute time** of the line, in UTC time zone and in RFC-3339
  format.
- **number of steps** taken in the given minute.
- **yaw** (angle of the watch in the x-y plane) in 1/16th of a turn.
- **pitch** (angle of the watchface to the z axis) in 1/16th of a turn.
- **VMC** (Vigorous Motion Count), a measure of how much movement happened during the minute.
- **ambient light level**, from 1 (darkest) to 4 (brightest), with 0 meaning unknown.
- **activity mask**, currently 3 for deep sleep, 1 for light sleep, or 0 for not sleeping.
- **heart rate** in beats per minute, or 0 if unavailable.

If there is no data for a given minute, the fields after the time will be empty:

```
2016-05-25T05:14:00Z,,,,,,,
```

## Removed Features (from the watchapp)

Since the PebbleKit JS configuration page is gone, the following features are no longer configured on the watch. They should be implemented in your native companion app:

- Web server endpoint URL
- Data bundling
- Adding extra form fields
- Data signing (HMAC)

The "Auto Wakeup" and "Auto Close" features are still present in the C code but have been hardcoded or simplified.
