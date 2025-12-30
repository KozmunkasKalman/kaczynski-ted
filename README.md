# Kaczynski
### "Kaczynski" Text EDitor is a modal text editor inspired by Vim, Nano and others. It depends on NCurses for the TUI.

The title came to be from when I came up with the name TED for Text EDitor, but I felt that was too generic, so I changed it to Kaczynski

> [!WARNING]
> The editor is currently very early in development. Not reccomended for proper use as its so unstable it crashes constantly. I can't even debug it with GDB because its a text editor made with NCurses. As of the latest commit its capable of editing plain text consistently, unless when it cant for some reason.
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
