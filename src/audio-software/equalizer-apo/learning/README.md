# Equalizer APO Learning Path

These sessions apply to Windows only. Record Windows version, Equalizer APO
version, selected playback device, active filter configuration, test
application, and result in reference/ after each session.

## 01. Establish A Device Baseline

Identify the intended playback device before applying filters. Confirm ordinary
playback with no deliberate Equalizer APO configuration change, then record the
device name and audio interface driver path when applicable.

Success condition: the selected device is identified and ordinary playback is
observed before a filter test.

## 02. Apply One Minimal Filter

Create one small, reversible filter adjustment and compare monitored playback
with the configuration enabled and disabled. Record the filter type, frequency,
gain, channel scope, and expected change.

Success condition: the configuration can be enabled and bypassed deliberately,
with an observed result recorded for the selected device.

## 03. Verify Application Behavior

Use the same selected playback device with one intended application, such as a
media player, browser, LMMS, or Carla. Record only the tested application and
result; do not generalize the outcome to all Windows audio clients.

Success condition: the target application plays through the selected device and
the active filter state is known.

## 04. Recover The Baseline

Return to the documented neutral configuration or disable the tested filter.
Restart the test application and verify ordinary playback. Preserve the working
configuration name and rollback steps.

Success condition: the original playback behavior can be restored without
guesswork.
