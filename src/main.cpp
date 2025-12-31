#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <ncurses.h>



enum Mode {
  NONE, NORMAL, WRITE, SELECT, MOVE, GOTO, SAVE, NEW, SHELL
};

Mode mode = NONE;

int lines; // terminal height
int cols;  // terminal width

int cur_char = 0;   // cursor global horizontal position
int cur_line = 0;   // cursor global vertical position
int scr_offset = 0; // cursor global vertical position

std::string filename = "";
std::vector<std::string> buffer;

std::string bottomline;
std::string input;

bool ins_over = false;
bool scr_lock = false;



bool isnumber(const std::string &s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}



void gettermsize() {
  getmaxyx(stdscr, lines, cols);
}

void updatescreen() {
  gettermsize();

  clear();

  int linenum_digits = std::to_string(buffer.size()).length();
  int text_width = cols - (linenum_digits + 4) - 1;

  for (int i = 0; i < lines - 2; i++) {
    if (i < buffer.size()) {
      mvprintw(i, 0, "%*i | %s", linenum_digits + 1, i + 1 + scr_offset, buffer[i + scr_offset].c_str());
    } else {
      mvprintw(i, 0, "%*s |", linenum_digits + 1, " ");
    }
  }

  mvhline(lines - 2, 0, '-', cols);

  if (bottomline.empty()) {
    // switch statement didnt work, stuck with if-else if-else wall
    if (mode == NONE) {
      bottomline = "          | [O]pen file   [N]ew file   [Q]uit ";
    } else if (mode == NORMAL) {
      bottomline = " NORMAL   | " + filename;
    } else if (mode == WRITE) {
      bottomline = " WRITE    | " + filename;
    } else if (mode == SELECT) {
      bottomline = " SELECT   | " + filename;
    } else if (mode == MOVE) {
      bottomline = " MOVE     | " + filename;
    } else if (mode == GOTO) {
      bottomline = " GO TO    | L" + input;
    } else if (mode == SAVE) {
      if (input.empty()) {
        bottomline = " SAVE AS  | ...";  
      } else {
        bottomline = " SAVE AS  | " + input;
      }
    } else if (mode == NEW) {
      if (input.empty()) {
        bottomline = " NEW FILE | ...";  
      } else {
        bottomline = " NEW FILE | " + input;
      }
    } else if (mode == SHELL) {
      bottomline = " SHELL    | $ " + input;
    }
  }

  if (buffer[cur_line].size() > text_width && cur_char > text_width) {
    cur_char = text_width;
  }

  mvprintw(lines - 1, 0, "%s", bottomline.c_str());

  move(cur_line - scr_offset, linenum_digits + 4 + cur_char);

  refresh();
}



void move_up() {
  if (cur_line > 0) {
    cur_line--;
    if (scr_offset > 0) {
      scr_offset--;
    }
  }
  if (cur_char > buffer[cur_line].size()) {
    cur_char = buffer[cur_line].size();
  }
}
void move_down() {
  if (cur_line < buffer.size() - 1) {
    cur_line++;
    if (cur_line - scr_offset >= lines - 2) {
      scr_offset++;
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
    move_up();
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



void open_file(char* file) {
  filename = file;
  std::ifstream in(filename);
  std::string line;
  while (std::getline(in, line)) {
    buffer.push_back(line);
  }
  mode = NORMAL;
}

void save() {
  std::ofstream out(filename);
  for (auto& line : buffer) {
    out << line << "\n";
  }
  bottomline = " SAVED    | " + filename;
}



std::string run_shellcmd(std::string cmd) {
  std::string output;

  FILE* pipe = popen(cmd.c_str(), "r");

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

  bottomline = " SHELL    | > " + output;

  pclose(pipe);

  return output;
}



void insertstring(std::string string) {
  if (string.empty()) { return; }

  std::vector<std::string> str_vect;
  std::string line;
  std::istringstream istrings(string);
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





int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::cerr << "Error: Incorrect usage: Too many arguments given\nCorrect usage:\n  kcz [<input>]" << std::endl;
    exit(1);
  }

  if (argv[1]) {
    open_file(argv[1]);
  } else {
    filename = "";
    mode = NONE;
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

    if (ch) {
      bottomline = "";
    }

    // switch statement didnt work, stuck with if-else if-else wall
    if (mode == NONE) {
      if (ch == 'Q' || ch == 'q' || ch == 'X' || ch == 'x') { // Q => quit
        break;
      } else if (ch == 'N' || ch == 'n') {
        mode = NEW;
      } else if (ch == 'O' || ch == 'o') {
        break; // not implemented yet
      }
    } else if (mode == NORMAL) {
      if (ch == '\n') { // enter => write mode
        mode = WRITE;
      } else if (ch == 'v') { // v => select mode
        mode = SELECT;
      } else if (ch == 'm') { // m => move mode
        mode = MOVE;
      } else if (ch == 'g') { // g => go-to mode
        mode = GOTO;
      } else if (ch == '$') { // $ => shell mode
        mode = SHELL;
      } else if (ch == KEY_UP) {
        move_up();
      } else if (ch == KEY_DOWN) {
        move_down();               // arrow keys => movement
      } else if (ch == KEY_LEFT) {
        move_left();
      } else if (ch == KEY_RIGHT) {
        move_right();
      } else if (ch == 'Q') { // Q => quit
        break;
      } else if (ch == 's') { // s => save
        save();
      } else if (ch == 'S') { // S => save as
        mode = SAVE;
      } else if (ch == 'X') { // X => save & quit
        save();
        break;
      } else if (ch == 'd' || ch == 330) { // d | delete => delete at cursor 
        delchar(0);
      } else if (ch == 263) { // backspace => in normal mode moves cursor left
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
        scr_offset = buffer.size() - (lines - 2);
        if (cur_char > buffer[cur_line].size()) {
          cur_char = buffer[cur_line].size();
        }
      } else if (ch == KEY_HOME) { // home => start of line
        cur_char = 0;
      } else if (ch == KEY_END) { // home => start of line
        cur_char = buffer[cur_line].size();
      } else if (ch == 339) { // pgup => scroll up
        cur_line = buffer.size() - 1;
        scr_offset = buffer.size() - (lines - 2);
        if (cur_char > buffer[cur_line].size()) {
          cur_char = buffer[cur_line].size();
        }
      } else if (ch == 338) { // pgdn => scroll down
        cur_line = buffer.size() - 1;
        scr_offset = buffer.size() - (lines - 2);
        if (cur_char > buffer[cur_line].size()) {
          cur_char = buffer[cur_line].size();
        }
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
      } else if (ch == 263) { // backspace => delete before cursor
        delchar(-1);
      } else if (ch == 330) { // del => delete at cursor
        delchar(0);
      } else if (ch == '\n') { // enter => new line
        newline();
      } else {
        buffer[cur_line].insert(cur_char, 1, ch);
        cur_char++;
      }
    } else if (mode == SELECT) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
      } else {
        insertstring("SELECT mode not implemented yet\n");
      }
    } else if (mode == MOVE) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
      } else {
        insertstring("MOVE mode not implemented yet\n");
      }
    } else if (mode == GOTO) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
        input.clear();
      } else if (ch == 263) { // backspace => delete before cursor
        if (!input.empty()) {
          input.pop_back();
        }
      } else if (ch == '\n') {
        if (!input.empty()) {
          if (isnumber(input)) {
            int inputnum;
            std::stringstream inpststr(input);
            inpststr >> inputnum;
            if (inputnum > 0 && inputnum <= buffer.size()) {
              cur_line = inputnum - 1;
              if (buffer.size() <= lines - 2) {
                scr_offset = 0;
              } else {
                if (inputnum <= lines - 2) {
                  scr_offset = 0;
                } else {
                  scr_offset = inputnum - (lines - 2);
                }
              }
              input.clear();
              mode = NORMAL;
            } else {
              bottomline = " GO TO    | Error: Line number out of range.";  
              input.clear();
              mode = NORMAL;
            }
          } else {
            bottomline = " GO TO    | Error: Line number is not an integer.";  
            input.clear();
            mode = NORMAL;
          }
        } else {
          bottomline = " GO TO    | Error: No line number inputted.";  
        }
      } else {
        input.push_back(ch);
      }
    } else if (mode == SAVE) {
      if (ch == 27) { // escape => normal mode
        mode = NORMAL;
        input.clear();
      } else if (ch == 263) { // backspace => delete before cursor
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
          bottomline = " SAVE AS  | Error: No file name. Please input a filename.";  
        }
      } else {
        input.push_back(ch);
      }
    } else if (mode == NEW) {
      if (ch == 27) { // escape => normal mode
        break;
      } else if (ch == 263 && !input.empty()) { // backspace => delete before cursor
        input.pop_back();
      } else if (ch == '\n') {
        if (!input.empty()) {
          filename = input;
          input.clear();
          save();
          mode = NORMAL;
        } else {
          bottomline = " NEW FILE | Error: No file name. Please input a filename.";  
        }
      } else {
        input.push_back(ch);
      }
    } else if (mode == SHELL) {
      if (ch == 27) { // escape => normal mode
        input.clear();
        mode = NORMAL;
      } else if (ch == 263) { // backspace => delete before cursor
        if (!input.empty()) {
          input.pop_back();
        }
      } else if (ch == '\n') {
        std::string cmdout = run_shellcmd(input);
        insertstring(cmdout);
        input.clear();
        mode = NORMAL;
      } else {
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
