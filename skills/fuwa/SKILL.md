---
name: fuwa
description: Write and play music in fuwa, a DAW for making music by talking to an AI. Use this whenever the user asks for notes to be written, changed, listened to, or played back in fuwa.
---

# fuwa

fuwa is a DAW for making music by talking to an AI. You operate it through the API.

The user works on screen too, so changes you did not make can be there. Read the current
state with `fuwa status` and `fuwa tracks`.

## What fuwa cannot do yet

When asked for one of these, say it cannot be done rather than papering over it.

- One track. There is no way to add another, so a drum or bass part cannot be separate —
  everything lands on the one track and plays through one instrument.
- 120 BPM in 4/4, fixed. Neither tempo nor time signature can change. To make something
  feel faster, write shorter notes.
- One sound. The instrument plays with whatever preset it loaded; the timbre cannot be
  changed and no parameter can be sent to it.
- No saving and no export. When fuwa quits, the notes are gone.
- No undo. Keep the MIDI files you generate under names you can tell apart. When the user
  asks to go back, write the earlier file again.

## Before anything else

fuwa has to be running. If a command answers `fuwa is not running`, ask the user to start
it. It takes over the audio output, so leave starting it to them.

```sh
fuwa serve    # the user runs this; it stays in the foreground
```

Every other command talks to that running instance.

## How you address things

What you change is addressed by a track and a range of bars.

```sh
fuwa notes set main 17-20 phrase.mid
#              ^     ^
#              |     bars 17, 18, 19 and 20 — counted from 1, both ends included
#              track name (or its id; `fuwa tracks` lists them)
```

A single bar is just `17`.

## Writing notes

Write a Standard MIDI File and hand fuwa the path.

```sh
fuwa notes set main 1-4 /tmp/melody.mid
```

What fuwa does with it:

- Beat 0 of the file lands on the first bar of the range. Write the phrase from time 0;
  there is no need to pad it out to where it sits in the song.
- Notes that start past the end of the range are dropped, and fuwa says how many. Widen the
  range if the phrase turned out longer than you thought.
- Existing notes that start inside the range are replaced. A long note that started earlier
  and rings through the range is left alone.
- Tempo inside the MIDI file is ignored. The project is 120 BPM in 4/4, so write at 120 BPM
  for the bar numbers to line up.

Format 0 and format 1 both work. Use ticks per quarter note; SMPTE time code cannot be read.
Only note on and note off are read — not program changes, controllers or channels.

## The commands

```
fuwa tracks                                 the tracks and what each one plays
fuwa plugins <bundle.vst3>                  the instruments inside a plugin bundle
fuwa instrument <track> <bundle.vst3> [class]
                                            load an instrument into a track
fuwa notes set <track> <bars> <file.mid>    replace the notes in a bar range
fuwa notes clear <track> <bars>             remove the notes in a bar range
fuwa notes list <track> [bars]              show the notes that are there
fuwa play [bar]                             play from a bar (default 1)
fuwa stop                                   stop playing
fuwa status                                 tempo, meter, length, whether it is playing
fuwa quit                                   shut fuwa down
```

Exit code 0 means it worked, 1 means it failed, 2 means the command was malformed.

## Checking your work

You cannot hear anything, so read these instead.

- `fuwa notes list <track> <bars>` — whether the notes landed where you meant. The `beat`
  column counts from 1 within its bar, and `length` is in quarter notes.
- `fuwa play` returns the length in seconds and the peak of what it rendered. A peak of 0
  means the instrument produced silence.
- `fuwa status` — how long the piece is, and whether anything is playing.

## The instrument

For now this is fixed to Surge XT.

```sh
fuwa instrument main ~/Library/Audio/Plug-Ins/VST3/"Surge XT.vst3"
```

Many instruments load and then produce no sound, and Surge XT is the only one confirmed to
play. If it is not installed, ask the user to install it; it is free.

If the user names a different instrument, use that one. `fuwa plugins <bundle.vst3>` shows
the instruments inside a bundle, and leaving out the class name picks the first. If
`fuwa play` reports a peak of 0, no sound came out — say so.

## Working with the user

The user does not edit MIDI. When they ask for a change, make it and play it.
