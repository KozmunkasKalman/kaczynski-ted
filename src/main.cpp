#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <ncurses.h>



enum Mode {
  NONE, NORMAL, WRITE, SELECT, MOVE, GOTO, SAVE, OPEN, NEW, SHELL
};
Mode mode = NONE;

enum CursorType {
  HIDDEN, BAR, LINE, BLOCK
};
CursorType curs_type = BLOCK;

int lines; // terminal height
int cols;  // terminal width

int cur_line = 0;   // current global vertical position
int cur_char = 0;   // current global horizontal position
int scr_offset = 0; // screen offset compared to cur_line
int curs_row = 0;   // current row of cursor
int curs_col = 0;   // current column of cursor

std::string filename = "";
std::vector<std::string> buffer;

std::string bottomline;
std::string input;

int linenum_digits = std::to_string(buffer.size()).length();
int gutter_width = linenum_digits + 4;
int text_width = cols - gutter_width - 1;

int selecting = 0; // 0 = false, 1 = single character, 2 = whole line
int sel_anchor_line = 0;
int sel_anchor_char = 0;
int sel_start_line = 0;
int sel_start_char = 0;
int sel_end_line = 0;
int sel_end_char = 0;

bool ins_over = false;
bool scr_lock = false;



std::string run_shellcmd(std::string cmd, bool do_msg) {
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

  if (do_msg) {
    bottomline = " SHELL    | > " + output;
  }

  pclose(pipe);

  return output;
}

void insertstring(std::string cont_str) {
  if (cont_str.empty()) { return; }

  std::vector<std::string> str_vect;
  std::string line;
  std::istringstream istrings(cont_str);
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

void set_mode_normal() {
  mode = NORMAL;
  curs_type = BLOCK;
  bottomline = " NORMAL   | " + filename;
}
void set_mode_write() {
  mode = WRITE;
  curs_type = BAR;
  ins_over = FALSE;
}
void set_mode_select(bool line) {
  mode = SELECT;
  sel_anchor_line = cur_line;
  sel_anchor_char = cur_char;
  if (line) {
    selecting = 2;
    sel_start_line = cur_line;
    sel_start_char = 0;
    sel_end_line = cur_line;
    sel_end_char = buffer[cur_line].size();
  } else {
    selecting = 1;
    sel_start_line = cur_line;
    sel_start_char = cur_char;
    sel_end_line = cur_line;
    sel_end_char = cur_char;
  }
}
void set_mode_open() {
  mode = OPEN;
  curs_type = HIDDEN;
  buffer.clear();
  buffer.push_back(run_shellcmd("find . -maxdepth 1 -type f -printf '%f   '", false));
}

void save(bool do_msg) {
  if (!filename.empty()) {
    std::ofstream out(filename);
    for (auto& line : buffer) {
      out << line << "\n";
    }
    if (do_msg) {
      bottomline = " SAVED    | " + filename;
    }
  } else {
    mode = SAVE;
  }
}

void new_file(std::string name) {
  buffer.clear();
  buffer.push_back("");
  filename = name;
  save(true);
}

bool check_if_file_exists(std::string name) {
  std::ifstream f(name.c_str());
  return f.good();
}

void open_file(std::string file) {
  if (check_if_file_exists(file)) {
    buffer.clear();
    filename = file;
    std::ifstream in(filename);
    std::string line;
    while (std::getline(in, line)) {
      buffer.push_back(line);
    }
    set_mode_normal();
  } else {
    bottomline = " OPEN     | Error: File \"" + file + "\" doesn't exist.";
  }
}

int get_line_wraps(int line) {
    return std::max(1, (int(buffer[line].size()) + text_width - 1) / text_width);
}

void move_up() {
  if (cur_line > 0 && buffer[cur_line].size() > text_width && cur_char - text_width < buffer[cur_line].size()){
    cur_char -= text_width;
  } else if (cur_line > 0 && buffer[cur_line - 1].size() > text_width && cur_char % text_width <= buffer[cur_line - 1].size()) {
    cur_line -= 1;
    for (int i = 0; i <= buffer[cur_line].size(); i += text_width) {
      cur_char += text_width;
    }
    cur_char -= text_width;
  } else if (cur_line > 0) {
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
  if (cur_line < buffer.size() - 1 && buffer[cur_line].size() > text_width && cur_char + text_width < buffer[cur_line].size()) {
    cur_char += text_width;
  } else if (cur_line < buffer.size() - 1) {
    if (cur_char + text_width - cur_char % text_width + 1 <= buffer[cur_line].size()) {
      cur_char += text_width;
    } else {
      cur_line++;
      cur_char = cur_char % text_width;
    }
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
  if (cur_char > buffer[cur_line].size()) {
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
  if (cur_char > buffer[cur_line].size()) {
    cur_char = buffer[cur_line].size();
  }
}
void page_up() {
  if (cur_line - (lines - 2) < 0) { cur_line = 0; scr_offset = 0; }
  else { cur_line -= (lines - 3); scr_offset = cur_line; }
  if (cur_char > buffer[cur_line].size()) cur_char = buffer[cur_line].size(); 
}
void page_down() {
  if (cur_line + (lines - 2) >= buffer.size()) { 
    cur_line = buffer.size() - 1; 
    scr_offset = (cur_line >= lines - 2) ? cur_line - (lines - 3) : 0;
  } else { 
    cur_line += (lines - 2) + get_line_wraps(cur_line) - 1 - 1; scr_offset = cur_line - (lines - 3); 
  }
  if (cur_char > buffer[cur_line].size()) cur_char = buffer[cur_line].size();
}

void normalize_selection() {
  if (sel_start_line > sel_end_line || (sel_start_line == sel_end_line && sel_start_char > sel_end_char)) {
    std::swap(sel_start_line, sel_end_line);
    std::swap(sel_start_char, sel_end_char);
  }
}
bool is_selected(int row, int col) {
  if (!selecting) return false;

  normalize_selection();

  if (row < sel_start_line || row > sel_end_line) return false;
  if (row == sel_start_line && col < sel_start_char) return false;
  if (row == sel_end_line && col > sel_end_char) return false;

  return true;
}

void delchar(int del_offset) {
  // TODO: merge del_offset if - else statement into single scope using del_offset variable properly
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
void delselect() {
    if (!selecting) return;

    normalize_selection();

    if (sel_start_line == sel_end_line) {
        buffer[sel_start_line].erase(sel_start_char, sel_end_char - sel_start_char);
    } else {
        buffer[sel_end_line].erase(0, sel_end_char);
        buffer[sel_start_line].erase(sel_start_char);
        buffer[sel_start_line] += buffer[sel_end_line];
        buffer.erase(buffer.begin() + sel_start_line + 1, buffer.begin() + sel_end_line + 1);
    }

    cur_line = sel_start_line;
    cur_char = sel_start_char;

    selecting = 0;
}

void newline() {
  std::string rest = buffer[cur_line].substr(cur_char);
  buffer[cur_line] = buffer[cur_line].substr(0, cur_char);
  buffer.insert(buffer.begin() + cur_line + 1, rest);
  move_down();
  cur_char = 0;
}



void update_win_size() {
  getmaxyx(stdscr, lines, cols);
}

void update_screen() {
  update_win_size();

  clear();

  linenum_digits = std::to_string(buffer.size()).length();
  gutter_width = linenum_digits + 4;
  text_width = cols - gutter_width - 1;

  int rend_line = 0;
  int rend_char = 0;

  for (int i = scr_offset; i < buffer.size() + (lines - 2) && rend_line < lines - 2; i++) {
    if (i < buffer.size()) {
      // jank 2-3am code for selection rendering, ~5 cups of coffee were the only thing keeping me alive at that point, eventually passed out, solution to line wrapping was revealed to me in a dream, so this is also jank 6-7am code
      std::string &line = buffer[i];

      for (int start = 0; start < std::max(1, int(line.size())); start += text_width) {
        if (rend_line >= lines - 2) break;

        if (start == 0)
          mvprintw(rend_line, 0, "%*d | ", linenum_digits + 1, i + 1);
        else
          mvprintw(rend_line, 0, "%*s | ", linenum_digits + 1, ".");

        for (int j = start; j < start + text_width && j < line.size(); j++) {
          if (is_selected(i, j)) attron(A_REVERSE);
          mvaddch(rend_line, gutter_width + (j - start), line[j]);
          if (is_selected(i, j)) attroff(A_REVERSE);
        }
        rend_line++;
      }
    } else {
      rend_line++;
      mvprintw(i, 0, "%*s |", linenum_digits + 1, " ");
    }
  }

  mvhline(lines - 2, 0, '-', cols);

  if (bottomline.empty()) {
    // TODO: torn this into a switch statement 
    if (mode == NONE) {
      bottomline = "          | [O]pen file   [N]ew file   [Q]uit ";
    } else if (mode == NORMAL) {
      bottomline = " NORMAL   | " + filename;
      // bottomline = " NORMAL   | " + filename + " | " + std::to_string(cur_line) + ":" + std::to_string(cur_char);
    } else if (mode == WRITE) {
      bottomline = " WRITE    | " + filename;
    } else if (mode == SELECT) {
      if (sel_start_line && sel_end_line && sel_start_char && sel_end_char) {
        bottomline = " SELECT   | " + std::to_string(sel_start_line) + ":" + std::to_string(sel_start_char) + " - " + std::to_string(sel_end_line) + ":" + std::to_string(sel_end_char);
      } else {
        bottomline = " SELECT   | ";
      }
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
    } else if (mode == OPEN) {
      bottomline = " OPEN     | " + input;
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

  mvprintw(lines - 1, 0, "%s", bottomline.c_str());

  if (curs_type != HIDDEN) {
    curs_set(1);
    if (curs_type == BLOCK) {
      std::cout << "\033[2 q" << std::endl;
    } else if (curs_type == BAR) {
      std::cout << "\033[5 q" << std::endl;
    } else if (curs_type == LINE) {
      std::cout << "\033[3 q" << std::endl;
    }
  } else {
    curs_set(0);
  }

  curs_row = 0;

  for (int i = scr_offset; i < cur_line; i++) {
    int wraps = get_line_wraps(i);
    curs_row += wraps;
  }

  curs_row += cur_char / text_width;

  int curs_col = gutter_width + (cur_char % text_width);

  move(curs_row, curs_col);

  refresh();
}



int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::cerr << "Error: Incorrect usage: Too many arguments given.\nCorrect usage:\n  kcz [<input>]" << std::endl;
    exit(1);
  }

  if (argc == 2 && argv[1]) {
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
    update_screen();

    int ch = getch();

    if (ch) {
      bottomline = "";
    }

    switch (mode) {
      case NONE:
        if (ch == 'O' || ch == 'o') set_mode_open(); 
        else if (ch == 'N' || ch == 'n') mode = NEW;
        else if (ch == 'Q' || ch == 'q') goto end_loop;
        break;

      case NORMAL:
        if (ch == '\n') set_mode_write();
        else if (ch == 'v') set_mode_select(false);
        else if (ch == 'm') mode = MOVE;
        else if (ch == 'g') mode = GOTO;
        else if (ch == '$') mode = SHELL;
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == 'Q') goto end_loop;
        else if (ch == 's') save(true);
        else if (ch == 'S') mode = SAVE;
        else if (ch == 'X') { save(true); goto end_loop; }
        else if (ch == 'O') set_mode_open();
        else if (ch == 'N') mode = NEW;
        else if (ch == 'd' || ch == KEY_DC || ch == 330) delchar(0);
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') move_left();
        else if (ch == 'D') delline();
        else if (ch == 'x') set_mode_select(true);
        else if (ch == 't') { 
          cur_line = 0; scr_offset = 0; 
          if (cur_char > buffer[0].size() - 1) cur_char = buffer[0].size();
        } 
        else if (ch == 'b') { 
          cur_line = buffer.size() - 1;
          scr_offset = (cur_line >= lines - 2) ? cur_line - (lines - 3) : 0;
          if (cur_char > buffer[cur_line].size()) cur_char = buffer[cur_line].size();
        } 
        else if (ch == KEY_HOME) cur_char = 0;
        else if (ch == KEY_END) cur_char = buffer[cur_line].size();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        break;

      case WRITE:
        if (ch == 27) set_mode_normal();
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') delchar(-1);
        else if (ch == KEY_DC || ch == 330) delchar(0);
        else if (ch == '\n') newline();
        else if (ch == KEY_IC) { // insert => toggle overwrite
          if (ins_over == true) { ins_over = false; curs_type = BAR;  }
          else { ins_over = true; curs_type = LINE; }
        }
        else if (ch == KEY_HOME) cur_char = 0;
        else if (ch == KEY_END) cur_char = buffer[cur_line].size();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        else if (std::isprint(ch) || std::isspace(ch)) {
          if (ins_over) delchar(0); // kind of a workaround tbh but it works
          buffer[cur_line].insert(cur_char, 1, ch);
          cur_char++;
        }
        break;

      case SELECT:
        if (ch == 27) { set_mode_normal(); selecting = 0; }
        else if (ch == 'x') set_mode_select(true);
        else if (ch == 'D' || ch == 'd' || ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127 || ch == 263 || ch == 330 || ch == '\b') {
          delselect();
          set_mode_normal();
        }
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == KEY_HOME) cur_char = 0;
        else if (ch == KEY_END) cur_char = buffer[cur_line].size();

        if (selecting == 2) {
          if (cur_line < sel_start_line) {
            sel_start_line = cur_line;
            sel_start_char = 0;
          } else if (cur_line > sel_end_line ){
            sel_end_line = cur_line;
            sel_end_char = buffer[cur_char].size();        
          }
        }
        else if (selecting == 1) {
          sel_start_line = sel_anchor_line;
          sel_start_char = sel_anchor_char;

          sel_end_line = cur_line;
          sel_end_char = cur_char;
        }
        break;

      case MOVE:
        // TODO: make it work with selection
        if (ch == 27) set_mode_normal();
        else if (ch == KEY_UP && cur_line > 0) {
          std::swap(buffer[cur_line], buffer[cur_line - 1]); move_up();
        }
        else if (ch == KEY_DOWN && cur_line < buffer.size() - 1) {
          std::swap(buffer[cur_line], buffer[cur_line + 1]); move_down();
        }
        else if (ch == 't') {
          std::swap(buffer[cur_line], buffer[0]); cur_line = 0; scr_offset = 0;
          if (cur_char > buffer[0].size() - 1) cur_char = buffer[0].size();
        }
        else if (ch == 'b') {
          std::swap(buffer[cur_line], buffer.back());
          cur_line = buffer.size() - 1;
          scr_offset = (cur_line >= lines - 2) ? cur_line - (lines - 3) : 0;
          if (cur_char > buffer[cur_line].size()) cur_char = buffer[cur_line].size();
        }
        break;

      case GOTO:
        if (ch == 27) { input.clear(); set_mode_normal(); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !input.empty()) input.pop_back();
        else if (ch == '\n') {
           // TODO: optimize this shit
           if (!input.empty()) {
             int inputnum; std::stringstream(input) >> inputnum;
             if (inputnum > 0 && inputnum <= buffer.size()) {
               cur_line = inputnum - 1;
               if (buffer.size() <= lines - 2) scr_offset = 0;
               else scr_offset = (inputnum <= lines - 2) ? 0 : inputnum - (lines - 2);
               input.clear(); set_mode_normal();
             } else { bottomline = " GO TO    | Error: Line number out of range."; input.clear(); mode = NORMAL; }
           } else { bottomline = " GO TO    | Error: No line number inputted."; mode = NORMAL; }
        }
        else if (std::isdigit(ch)) input.push_back(ch);
        break;

      case SAVE:
        if (ch == 27) { input.clear(); set_mode_normal(); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !input.empty()) input.pop_back();
        else if (ch == '\n') {
           if (!input.empty()) {
             filename = input; input.clear(); save(true); set_mode_normal();
           } else bottomline = " SAVE AS  | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) input.push_back(ch);
        break;

      case OPEN:
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !input.empty()) input.pop_back();
        else if (ch == '\n') {
           if (!input.empty()) { open_file(input); input.clear(); }
           else bottomline = " OPEN     | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) input.push_back(ch);
        break;

      case NEW:
        if (ch == 27) goto end_loop;
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !input.empty()) input.pop_back();
        else if (ch == '\n') {
           if (!input.empty()) { new_file(input); input.clear(); set_mode_normal(); }
           else bottomline = " NEW FILE | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) input.push_back(ch);
        break;

      case SHELL:
        if (ch == 27) { input.clear(); set_mode_normal(); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !input.empty()) input.pop_back();
        else if (ch == '\n') {
           std::string cmdout = run_shellcmd(input, true); insertstring(cmdout); input.clear(); set_mode_normal();
        }
        else if (std::isprint(ch)) input.push_back(ch);
        break;
    }

    if (cur_line >= buffer.size()) {
      cur_line = buffer.size() - 1;
    }
    if (cur_char > buffer[cur_line].size()) {
      cur_char = buffer[cur_line].size();
    }

    update_screen();
  }

  end_loop:

  noraw();
  echo();
  keypad(stdscr, FALSE);

  endwin();

  return 0;
}
