# Kaczynski
### "Kaczynski" Text EDitor is a modal text editor inspired by Vim, Nano and others. It depends on NCurses for the TUI.

> [!WARNING]
> The editor is currently very early in development. Not reccomended for proper use as its so unstable it crashes constantly. I can't even debug it with GDB because its a text editor made with NCurses. As of the latest commit its capable of editing plain text consistently, unless when it cant for some reason.

The title came to be from when I came up with the name TED for Text EDitor, but I felt that was too generic, so I changed it to Kaczynski

Concept for the text editor was essentially "Vim but entirely pragmatic, symbolic, and intuitive", which came from a joke about Vim being a "CIA honeypot to get you so used to the retarded keybinds you cant use any normal text editors anymore". And so of course the main thing it lacks compared to Vim is COMMAND mode. instead you can just do things from NORMAL mode, or if you want something more complex go into SHELL mode and run shell commands, the output of which it will insert into the buffer at the cursor. But this by no means is supposed to be a replacement for Vim, and if it would be, then only for me. Primarily because Vim is just too damn useful. And besides, this is currently way too unstable to be used for anything serious.

## Keybinds

### NORMAL mode
- arrow keys => move cursor

- [enter] => write mode
- v => select mode (not implemented yet)
- m => move mode (not implemented yet)
- $ => shell mode

- Q => quit
- s => save
- S => save as
- X => save & quit

- d/[delete] => delete character at cursor
- D => delete line

- x => select line (not implemented yet)

- T => go to top
- B => go to bottom

- [pgup] => scroll up one screen
- [pgdn] => scroll down one screen

### WRITE mode
- arrow keys => move cursor

- [escape] => normal mode

- [backspace] => delete character before cursor
- [delete] => delete character at cursor

- [enter] => new line
