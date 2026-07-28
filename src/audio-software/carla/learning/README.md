# Carla Learning Path

Run each session independently on Linux and Windows. Record application version,
audio driver, device or interface, MIDI endpoint, plugin format, and result in
reference/ after each test.

## 01. Establish A Host Baseline

Open Carla with no plugins loaded. Select one audio driver path and one output
device. Save the actual setting values before connecting controllers or loading
third-party plugins.

Success condition: Carla opens, the selected driver starts, and a project can be
saved and reopened without changing the intended settings.

## 02. Scan And Load One Plugin

Configure one known plugin location, refresh the plugin database, and load a
single instrument or effect. Record the plugin format, path, scan result, and
whether its interface opens.

Success condition: the plugin is discovered once, loads without a host error,
and can be removed cleanly.

## 03. Build A Rack Chain

Create a small ordered rack with one sound source or audio input followed by one
effect. Change one audible parameter and save the project.

Success condition: the rack order is clear, audio reaches the selected output,
and reopening restores the intended chain.

## 04. Patch Devices

Connect one MIDI controller and one audio interface or output path. Use the
patchbay only after the independent audio and MIDI checks work. Record every
port connection that the project requires.

Success condition: a controller action reaches the selected plugin or
parameter, and audio monitoring stays on the intended output.
