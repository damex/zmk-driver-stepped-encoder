# zmk-driver-stepped-encoder

Detented quadrature (EC11-style) rotary encoder driver for ZMK. One detent, one
step.

Stock ZMK reads pins in a deferred thread with interrupts off. Fast or bouncy
detent drops a pulse. This driver arms the interrupt only to wake fixed-rate
polling, decodes each transition through a Gray-code table, and sleeps back on
the interrupt once the knob rests. Poll rate caps ISR cost, so a chattery
contact cannot storm the radio. Reports coalesce over an 8 ms window, so bounce
mostly cancels (`+1` then `-1`) and the split link is not flooded. Exposes
`SENSOR_CHAN_ROTATION`, so `zmk,keymap-sensors` and `&inc_dec_kp` use it
unchanged.

## Install

```yaml
  remotes:
    - name: damex
      url-base: https://github.com/damex
  projects:
    - name: zmk-driver-stepped-encoder
      remote: damex
      revision: v0.1.0
```

Then `west update`.

Local checkout: `-DZMK_EXTRA_MODULES=<path>/zmk-driver-stepped-encoder`.

## Configure

DT example, replacing an existing `alps,ec11` node:

```dts
&left_encoder {
    compatible = "zmk,stepped-encoder";
};
```

| Property  | Required | Description                          |
| --------- | -------- | ------------------------------------ |
| `a-gpios` | yes      | Channel A pin.                       |
| `b-gpios` | yes      | Channel B pin.                       |
| `steps`   | yes      | Quadrature pulses per full rotation. |

`steps` bounded to 1..360 at build time. One detent = one keymap trigger when
`triggers-per-rotation = steps / (pulses per detent)`. A 20-detent EC11 with 4
pulses per detent: `steps = <80>`, `triggers-per-rotation = <20>`.

`CONFIG_SENSOR_STEPPED_ENCODER_STATS=y` exposes per-encoder edge counters via
`stepped_encoder_stats_get()` from `<zmk/sensor/stepped_encoder.h>`, plus a
periodic log line. Diagnostic only.

## Limitations

Decode lossless within the physical envelope: microsecond ISR latency against
millisecond edge spacing on a hand-turned knob. No software timer debounce.
A bounce cancels through the state machine.
A very noisy encoder still needs the datasheet RC filter (10 nF per line).

## License

This module is MIT.

Dependencies (each keeps its own license):

| Dependency | License |
|---|---|
| ZMK | MIT |
| Zephyr | Apache-2.0 |
