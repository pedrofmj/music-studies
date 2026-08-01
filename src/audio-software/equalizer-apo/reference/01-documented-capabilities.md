# Documented Capabilities

Sources: [Equalizer APO SourceForge project](https://sourceforge.net/projects/equalizerapo/)
and [configuration reference](https://sourceforge.net/p/equalizerapo/wiki/Configuration%20reference/).

Equalizer APO is a Windows parametric and graphic equalizer implemented as an
Audio Processing Object in the Windows system-effects infrastructure. The
project lists a modular configuration editor, VST support, low latency, and
filter processing across any number of audio channels.

## Documented Boundaries

- Applications that bypass Windows system effects, including ASIO and WASAPI
  exclusive mode, do not use Equalizer APO processing.
- The configuration reference supports per-device selection, channels, preamp,
  parametric filters, includes, GraphicEQ, convolution, delay, and other
  control commands.
- Negative preamp should be considered before any positive filter gain to leave
  headroom. The actual safe value depends on program material and must be
  measured on the selected endpoint.

These statements do not prove that a Windows device, driver, application mode,
or filter setting works locally. Record each selected endpoint and restore test
in the device and application checklists.
