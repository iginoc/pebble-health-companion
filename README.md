**Note**: I don't have a working Pebble anymore.
This repository is archived and will not be updated.

# Pebble Health Export

This is a simple application for Pebble Watch that extract all the raw
Pebble Health data gathered by the watch and POST it to a user-configured
HTTP endpoint.

Note that this involves streaming a lot of data from the watch to the phone
through the Bluetooth link, so it consumes a lot of battery. Hopefully this
will be offset by a high transfer rate, so that the high battery usage only
lasts for a short time.

Preliminary tests suggest that when running at full capacity, the app would
drain empty a fully-charged Pebble Time Round battery in about 2h30, but
transferring a full day worth of Pebble Health data would only take about
3 minutes. That means roughly 2% of battery per day, for the Pebble model
with the smallest battery.

## Basic Operation

To use this watchapp, you need a webserver to receive the POST request
sent the watchapp. The append server in my
[Simple Web Applications (in Ada)][webapps] was the reference used when
developing this watchapp, but any webapp that can work with a POSTed HTML
form should work, and the actual HTML from doesn't even need to exist.

[webapps]: https://github.com/faelys/simple-webapps

The watchapp needs to be configured to do anything. The required
configuration fields are the URL endpoint to which the form is POSTed, and
the name of the data field.

When configured, the watchapp makes one POST request for each recorded
Pebble Health entry, filling the data field with string like:

	2016-05-25T21:22:00Z,120,0,4,9915,1,0,0

That is a typical CSV line, with the fields interpreted as follow:

- **absolute time** of the line, in UTC time zone and in RFC-3339
format,
- **number of steps** taken in the given minute (120 in this example),
- **yaw** (angle of the watch in the x-y plane) in 1/16th of turn
- **pitch** (angle of the watchface to the z axis) in 1/16th of turn
- **VMC** which is a measure of how much movement happened during the
  minute (9915 here is a lot, my desk activity is a few thousands,
  light sleep or watching TV is a few hundreds, and it goes down to
  zero during deep sleep)
- **ambient light level**, from 1 (darkest) to 4 (brightest) with 0
  meaning unknown
- **activity mask**, currently 3 for deep sleep, 1 for non-deep sleep or 0
  for not sleeping,
- **heart beats**, or 0 when there is no heart rate sensor or when the
  data is not available for another reason.

Sometimes Pebble Health has no available data for a given minute, then all
fields except time are empty, so it looks like this:

	2016-05-25T05:14:00Z,,,,,,,

## Optional Features

### Auto Wakeup

I found that the activity mask is reset much more frequently than the rest
of Pebble Health data, and it seems it happens on a daily basis. To prevent
forgetting to export the data, it can be automated by configuring an
automatic wakeup time. The data will then be exported every day at that
time.

Note that this watchapp has currently a bad management for Wakeup errors.
So for example if you set a time when some other watchapps has already
taken, it will fail silently.

### Data Bundling

The HTTP overhead of a POST request is quite larger than the data line
itself, so it can improve throughput to bundle several line in a single
POST request.

If your receiving webapp supports this, you can configure bundling lines,
with a percent-encoded separator of your choice. For example, `%0d%0a` might
be good for a webapp like my Append Server that appends the POSTed field
directly to a text file.

### Extra Form Fields

The webapp might need some other fields than the health data, for example a
Submit button, or a field to select which file to affect, or a password to
prevent unauthorized access to the webapp.

Any number of fields can be added to each POST request made by the
watchapp, with the configured static value.

### Data Signature

Authentication to the webapp might need something stronger than a password,
and the watchapp javascript engine doesn't do well with cookies. In order
to strengthen the authentication, my Append Server uses HMAC authentication
of data lines.

The watchapps can be configured to provide a signature on the given form
field, with the usual HMAC-SHA\* algorthm, and can encode the signature in
hexadecimal or base-64 or send it unencoded.

### Auto Close

With this option the watchapp can automatically close when the data export
is complete. It can be useful for example to assign to a shortcut button,
or even just start the watchapp and forget about it.

Note that even when this option is disabled, the watchapp will still
auto-close when it is started through auto-wakeup, in order to be as
unobtrusive as possible.

Be warned that the auto-close is faster than the loading of the
configuration page, so if you have auto-close enabled and have synced
recently, when trying to load the configuration screen the watchapp might
close before you have a chance to configure anything. The only solution
then is to wait for more health data to be generated, so that while it
upload the configuration page has enough time to load and suspend
auto-close.
