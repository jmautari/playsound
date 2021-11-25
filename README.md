# Playsound

Simple utility program to play a wav file.

Primarily used by [Widgets][1]

## Usage

```
playsound [options] [path-to-file]
```

Where `options` can be:

`-r` repeat the sound until `playsound` is called again with `-s` option

`-s` stop a sound that was previously played with `-r` option

`-t X` play the sound for up to `X` seconds. This option has no effect if the sound file duration is shorter than `X` seconds

**All options are mutually exclusive.**

## Examples

Plays `beep.wav` wave file repeatedly until `playsound -s` is called.

```
playsound -r beep.wav
```

Stops a sound that was started previously with `-r` option.

```
playsound -s
```

Plays `song.wav` for 5 seconds.

```
playsound -t 5 song.wav
```

[1]: https://github.com/jmautari/widgets
