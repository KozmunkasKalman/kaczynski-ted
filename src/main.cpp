#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ncurses.h>
#include <cstdlib>

enum Mode {
  NORMAL, WRITE, SELECT, MOVE, SAVE, NEW, SHELL
};

Mode mode = NORMAL;

int lines; // terminal height
int cols;  // terminal width

int cur_char = 0;   // cursor global horizontal position
int cur_line = 0;   // cursor global vertical position
int scr_offset = 0; // cursor global vertical position

std::string filename = "";
std::vector<std::string> buffer;

std::string bottomline;
std::string setbottomline;
std::string input;

void gettermsize() {
  getmaxyx(stdscr, lines, cols);
}

void updatescreen() {
  gettermsize();

  clear();

  int linenum_digits = std::to_string(buffer.size()).length();
  int text_width = cols - (linenum_digits + 4);

  for (int i = 0; i < lines - 2; i++) {
    int buf_idx = i + scr_offset;
    if (buf_idx < buffer.size()) {
      mvprintw(i, 0, "%*d | %s", linenum_digits + 1, buf_idx + 1, buffer[buf_idx].c_str());
    } else {
      mvprintw(i, 0, "%*s |", linenum_digits + 1, " ");
    }
  }

  mvhline(lines - 2, 0, '-', cols);

  if (mode == NORMAL) {
    // bottomline = "NORMAL    | " + std::to_string(cur_line + 1) + ":" + std::to_string(cur_char + 1) + ", " + std::to_string(scr_offset) + ", " + std::to_string(buffer[cur_line].size() + 1) + " " + std::to_string(buffer.size()) + " | " + filename;
    bottomline = " NORMAL    | " + filename;
  } else if ( mode == WRITE ) {
    bottomline = " WRITE    | " + filename;
  } else if ( mode == SELECT) {
    bottomline = " SELECT   | " + filename;
  } else if (mode == MOVE) {
    bottomline = " MOVE     | " + filename;
  } else if (mode == SAVE) {
    bottomline = " SAVE AS  | " + input;
  } else if (mode == NEW) {
    bottomline = " NEW FILE | " + input;
  } else if (mode == SHELL) {
    bottomline = " SHELL    | $ " + input;
  }

  if (!setbottomline.empty()) {
    bottomline = setbottomline;
  }

  mvprintw(lines - 1, 0, "%s", bottomline.c_str());

  int render_y = cur_line - scr_offset;
  int render_x = linenum_digits + 4 + cur_char;

  if (render_x >= cols) render_x = cols - 1;

  move(render_y, render_x);

  refresh();
}

void move_up() {
  if (cur_line > 0) {
    cur_line--;
    if (cur_line < scr_offset) {
      scr_offset = cur_line;
    }
  }
  if (cur_char > buffer[cur_line].size()) {
    cur_char = buffer[cur_line].size();
  }
}
void move_down() {
  if (cur_line < buffer.size() - 1) {
    cur_line++;
    if (cur_line >= scr_offset + (lines - 2)) {
      scr_offset = cur_line - (lines - 3);
    }
  }
  if (cur_char > buffer[cur_line].size()) {
    cur_char = buffer[cur_line].size();
  }
}
void move_left() {
  if (cur_char > 0) {
    cur_char--;
  } else if (cur_line > 0) {
    move_up();
    cur_char = buffer[cur_line].size();
  }
}
void move_right() {
  if (cur_char < buffer[cur_line].size()) {
    cur_char++;
  } else if (cur_line < buffer.size() - 1) {
    move_down();
    cur_char = 0;
  }
}

void delchar(int del_offset) {
  if (del_offset != 0) {
    if (cur_char > 0) {
      buffer[cur_line].erase(cur_char - 1, 1);
      cur_char--;
    } else if (cur_line > 0) {
      int prev = buffer[cur_line - 1].size();
      buffer[cur_line - 1] += buffer[cur_line];
      buffer.erase(buffer.begin() + cur_line);

      move_up();

      cur_char = prev;
    }
  } else {
    if (cur_char < buffer[cur_line].size()) {
      buffer[cur_line].erase(cur_char, 1);
    } else if (cur_line < buffer.size() - 1) {
      buffer[cur_line] += buffer[cur_line + 1];
      buffer.erase(buffer.begin() + cur_line + 1);
    }
  }
}

void delline() {
  if (buffer.empty()) return;

  buffer.erase(buffer.begin() + cur_line);

  if (buffer.empty()) {
    buffer.push_back("");
    cur_line = 0;
    cur_char = 0;
    return;
  }

  if (cur_line >= buffer.size()) {
    cur_line = buffer.size() - 1;
  }

  cur_char = 0;
}

void newline() {
  std::string rest = buffer[cur_line].substr(cur_char);
  buffer[cur_line] = buffer[cur_line].substr(0, cur_char);
  buffer.insert(buffer.begin() + cur_line + 1, rest);
  move_down();
  cur_char = 0;
}

void save() {
  std::ofstream out(filename);
  for (auto& line : buffer) {
    out << line << "\n";
  }
  setbottomline = " SAVED    | " + filename;
}

std::string run_shellcmd(std::string cmd) {
  std::string output;
  std::string full_cmd = cmd + " 2>&1";
  FILE* pipe = popen(full_cmd.c_str(), "r");

  if (!pipe) {
    output = "[error]";
    return output;
  }

  char cmdbuf[4096];
  while (fgets(cmdbuf, sizeof(cmdbuf), pipe)) {
    std::string line(cmdbuf);
    if (!line.empty() && line.back() == '\n')
      line.pop_back();
    output = output + line + "\n";
  }

  setbottomline = " SHELL    | > " + output;

  pclose(pipe);

  return output;
}

void insertstring(std::string content) { // renamed parameter
  if (content.empty()) { return; }

  std::vector<std::string> str_vect;
  std::string line;
  std::istringstream istrings(content); //updated usage
  while (std::getline(istrings, line)) {
    str_vect.push_back(line);
  }

  std::string tail = buffer[cur_line].substr(cur_char);
  buffer[cur_line].erase(cur_char);

  buffer[cur_line] += str_vect[0];

  for (size_t i = 1; i < str_vect.size(); i++) {
    buffer.insert(buffer.begin() + cur_line + i, str_vect[i]);
  }

  buffer[cur_line + str_vect.size() - 1] += tail;

  cur_line += str_vect.size() - 1;
  cur_char = buffer[cur_line + str_vect.size() - 1].size() - tail.size();
}

int main(int argc, char* argv[]){
  if (argc > 2) {
    std::cerr << "Error: Incorrect usage: Too many arguments given\nCorrect usage:\n  kcz [<input>]" << std::endl;
    exit(1);
  }

  if (argc == 2 && argv[1]) {
    filename = argv[1];
    std::ifstream in(filename);
    std::string line;
    while (std::getline(in, line)) {
      buffer.push_back(line);
    }
  } else {
    filename = "";
    mode = NEW;
    setbottomline = " NEW FILE | ...";   
  }

  if (buffer.empty()) {
    buffer.push_back("");
  }

  initscr();

  raw();
  noecho();
  keypad(stdscr, true);

  while (true) {
    // offset = cur_line - cur_line;
    
    updatescreen();

    int ch = getch();

    if (ch != ERR && mode != SAVE && mode != NEW && mode != SHELL) {
      setbottomline = "";
    }

    if (mode == NORMAL) {
      if (ch == '\n') { // enter => write mode
        mode = WRITE;
      } else if (ch == 'v') { // v => select mode
        mode = SELECT;
      } else if (ch == 'm') { // m => move mode
        mode = MOVE;
      } else if (ch == '$') { // $ => shell mode
        mode = SHELL;
      } else if (ch == KEY_UP) {
        move_up();
      } else if (ch == KEY_DOWN) {
        move_down();
      } else if (ch == KEY_LEFT) {
        move_left();
      } else if (ch == KEY_RIGHT) {
        move_right();

      } else if (ch == 'Q') { // Q => quit
        break;
      } else if (ch == 's') { // s => save
        if (!filename.empty()) save(); else mode = SAVE;
      } else if (ch == 'S') { // S => save as
        mode = SAVE;
      } else if (ch == 'X') { // X => save & quit
        if (!filename.empty()) save();
        break;
      } else if (ch == 'd' || ch == KEY_DC || ch == 330) { // d | delete => delete at cursor 
        delchar(0);
      } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 263) { // backspace => in normal mode moves cursor left
        move_left();
      } else if (ch == 'D') { // D => delete line
        delline();
      } else if (ch == 'x') { // x => select line
        
      } else if (ch == 'T') { // T => top line
        cur_line = 0;
        scr_offset = 0;
        if (cur_char > buffer[0].size() - 1) {
          cur_char = buffer[0].size();
        }
      } else if (ch == 'B') { // B => bottom line
        cur_line = buffer.size() - 1;
        if (cur_line >= lines - 2) scr_offset = cur_line - (lines - 3);
        else scr_offset = 0;
        if (cur_char > buffer[cur_line].size()) {
          cur_char = buffer[cur_line].size();
        }
      } else if (ch == 339) { // pgup => scroll up
      } else if (ch == 338) { // pgdn => scroll down
      }
    } else if (mode == WRITE) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
      } else if (ch == KEY_UP) {
        move_up();
      } else if (ch == KEY_DOWN) {
        move_down();
      } else if (ch == KEY_LEFT) {
        move_left();
      } else if (ch == KEY_RIGHT) {
        move_right();
      } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 263) { // backspace => delete before cursor
        delchar(-1);
      } else if (ch == KEY_DC || ch == 330) { // del => delete at cursor
        delchar(0);
      } else if (ch == '\n') { // enter => new line
        newline();
      } else if (ch >= 32 && ch <= 126) {
        buffer[cur_line].insert(cur_char, 1, ch);
        cur_char++;
      }
    } else if (mode == SELECT) { // not implemented yet, selects
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
      } else {
        insertstring("SELECT mode not implemented yet\n");
      }
    } else if (mode == MOVE) { // not implemented yet, swaps selection with something else
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
      } else {
        insertstring("MOVE mode not implemented yet\n");
      }
    } else if (mode == SHELL) {
      if (ch == 27) { // escape => normal mode
        input.clear();
        mode = NORMAL;
      } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 263) { // backspace => delete before cursor
        if (!input.empty()) {
          input.pop_back();
        }
      } else if (ch == '\n') {
        std::string cmdout = run_shellcmd(input);
        insertstring(cmdout);
        input.clear();
        mode = NORMAL;
      } else if (ch >= 32 && ch <= 126) {
        input.push_back(ch);
      }
    } else if (mode == SAVE) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
        input.clear();
      } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 263) { // backspace => delete before cursor
        if (!input.empty()) {
          input.pop_back();
        }
      } else if (ch == '\n') {
        if (!input.empty()) {
          filename = input;
          input.clear();
          save();
          mode = NORMAL;
        } else {
          setbottomline = " SAVE AS  | Error: No file name. Please input a filename.";  
        }
      } else if (ch >= 32 && ch <= 126) {
        input.push_back(ch);
      }
    } else if (mode == NEW) {
      if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 263) { // backspace => delete before cursor
        if (!input.empty()) {
          input.pop_back();
        }
      } else if (ch == '\n') {
        if (!input.empty()) {
          filename = input;
          input.clear();
          save();
          mode = NORMAL;
        } else {
          setbottomline = " NEW FILE | Error: No file name. Please input a filename.";  
        }
      } else if (ch >= 32 && ch <= 126) {
        input.push_back(ch);
      }
    }

    if (cur_line >= buffer.size()) {
      cur_line = buffer.size() - 1;
    }
    if (cur_char > buffer[cur_line].size()) {
      cur_char = buffer[cur_line].size();
    }

    updatescreen();
  }

  endwin();

  return 0;
}
