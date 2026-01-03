# Kaczynski
### "Kaczynski" Text EDitor is an entirely pragmatic modal text editor inspired by Vim, Nano and others. It depends on NCurses for the TUI.

> [!WARNING]
> The editor is currently very early in development. Not reccomended for proper use as its so unstable it crashes constantly. I can't even debug it with GDB because its a text editor made with NCurses. As of the latest commit its capable of editing plain text consistently.

The title came to be from when I came up with the name "TED", standing for Text EDitor. But I felt that was too generic, and so I changed it to the first thing that came to my mind from the word "TED": Kaczynski.

Concept for the text editor was essentially "Vim but entirely pragmatic, symbolic, and intuitive", which came from a joke about Vim being a "CIA honeypot to get you so used to the retarded keybinds you cant use any normal text editors anymore".

Compared to Vim it mainly lacks several things, but not limited to: Vim motions (w, b, f, t, etc.), and the COMMAND mode. I am planning on adding some alternative to Vim motions eventually to make text editing more efficient. As for the COMMAND mode, in Kaczynski you instead can just do things from NORMAL mode, or if you want something more complex go into SHELL mode and run shell commands, the output of which it will insert into the buffer at the cursor.

This by no means is supposed to be a replacement for Vim, and if it would be, then only for me. Primarily because Vim is just too damn useful for most things. And besides, this is currently way too unstable to be used for anything serious.

### Installation:

To install Kaczynski, run the following command:
```bash
git clone https://github.com/KozmunkasKalman/kaczynski-ted.git && cd kaczynski-ted && chmod +x install.sh && bash ./install.sh
```

### Targets:

- [x] Installation script
- [x] Create new files
- [x] Write text in files
- [x] Delete text in files
- [x] Save files
- [x] Open files (like emacs dired)
- [x] Visual selection of text
- [ ] Move selected text around in move mode
- [ ] Cut, copy, paste support for system clipboard
- [ ] Undo & redo
- [x] Home & End keys functionality
- [x] Page up & Page down keys functionality
- [x] Going to specified line
- [ ] Searching for text (like vim /)
- [ ] Find & replace (sed style)
- [x] Insert key functionality (overwriting text)
- [ ] Scroll lock key functionality (locking screen in place)
- [ ] Tabs (support for multiple simultaneously open buffers)
- [x] Unicode support
- [x] Proper TUI menus (navigable by keyboard & mouse, for things like file opening and such)
- [ ] Syntax highlighting
- [ ] Automatic line wrapping based on words
- [x] Run shell commands within editor
- [ ] Macros
- [x] Configuration file (~/.config/kaczynski-ted/unabombrc)

### Keybinds

The Kaczynski keybinds are not like Vim, Emacs, or Nano.
To learn the keybinds, please do the installation process and go through the tutorial provided.
