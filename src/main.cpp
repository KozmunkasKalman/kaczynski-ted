#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <ncurses.h>



enum Mode { NONE, NORMAL, WRITE, SELECT, MOVE, GOTO, SAVE, OPEN, NEW, SHELL };
enum CursorType { HIDDEN, BAR, LINE, BLOCK };

struct {
  Mode mode;

  int cur_line; // current global vertical position
  int cur_char; // current global horizontal position
  int scr_offset; // screen offset compared to editor.cur_line

  std::string bottomline;
  std::string input;

  bool insert;
  bool scrlock;
} editor;

struct {
  std::string name;
  std::vector<std::string> content;
} buffer;

struct {
  int type; // 0 = false, 1 = single character, 2 = whole line

  int anchor_line;
  int anchor_char;

  int start_line;
  int start_char;

  int end_line;
  int end_char;
} selection;

struct {
  CursorType type;
  int row; // current visual cursor row
  int col; // current visual cursor column
} cursor;

struct {
  int lines; // terminal height
  int cols;  // terminal width

  int linenum_digits;
  int gutter_width;
  int text_width;

  int bottomline_height;
  int text_height;
} ui;



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
    output += line + "\n";
  }

  if (do_msg) {
    editor.bottomline = " SHELL    | > " + output;
  }

  pclose(pipe);

  return output;
}

void insert_string(std::string cont_str) {
  if (cont_str.empty()) { return; }

  std::vector<std::string> str_vect;
  std::string line;
  std::istringstream istrings(cont_str);
  while (std::getline(istrings, line)) {
    str_vect.push_back(line);
  }

  std::string tail = buffer.content[editor.cur_line].substr(editor.cur_char);
  buffer.content[editor.cur_line].erase(editor.cur_char);

  buffer.content[editor.cur_line] += str_vect[0];

  for (size_t i = 1; i < str_vect.size(); i++) {
    buffer.content.insert(buffer.content.begin() + editor.cur_line + i, str_vect[i]);
  }

  buffer.content[editor.cur_line + str_vect.size() - 1] += tail;

  editor.cur_line += str_vect.size() - 1;
  editor.cur_char = buffer.content[editor.cur_line + str_vect.size() - 1].size() - tail.size();
}

void set_mode(Mode target_mode, int submode = 0) {
  switch (target_mode) {
    case NORMAL:
      editor.mode = NORMAL;
      cursor.type = BLOCK;
      editor.bottomline = " NORMAL   | " + buffer.name;
      break;

    case WRITE:
      editor.mode = WRITE;
      cursor.type = BAR;
      editor.insert = FALSE;
      break;

    case SELECT:
      if (submode == 2) {
        editor.mode = SELECT;
        cursor.type = BLOCK;
        selection.type = 2;
        selection.start_line = editor.cur_line;
        selection.start_char = 0;
        selection.end_line = editor.cur_line;
        selection.end_char = buffer.content[editor.cur_line].size();
      } else if (submode == 1){
        editor.mode = SELECT;
        cursor.type = BLOCK;
        selection.anchor_line = editor.cur_line;
        selection.anchor_char = editor.cur_char;
        selection.type = 1;
        selection.start_line = editor.cur_line;
        selection.start_char = editor.cur_char;
        selection.end_line = editor.cur_line;
        selection.end_char = editor.cur_char;
      }
      break;

    case MOVE:
      editor.mode = MOVE;
      cursor.type = BLOCK;
      break;

    case GOTO:
      editor.mode = GOTO;
      cursor.type = BLOCK;
      break;

    case SAVE:
      editor.mode = SAVE;
      cursor.type = HIDDEN;
      break;

    case NEW:
      editor.mode = NEW;
      cursor.type = HIDDEN;
      break;

    case OPEN:
      editor.mode = OPEN;
      cursor.type = HIDDEN;
      buffer.content.clear();
      buffer.content.push_back(run_shellcmd("find . -maxdepth 1 -type f -printf '%f   '", false));
      break;

    case SHELL:
      editor.mode = SHELL;
      cursor.type = HIDDEN;
      break;
  }
}

void save(bool do_msg) {
  if (!buffer.name.empty()) {
    std::ofstream out(buffer.name);
    for (auto& line : buffer.content) {
      out << line << "\n";
    }
    if (do_msg) {
      editor.bottomline = " SAVED    | " + buffer.name;
    }
  } else {
    set_mode(SAVE);
  }
}

void new_file(std::string name) {
  buffer.content.clear();
  buffer.content.push_back("");
  buffer.name = name;
  save(true);
}

bool check_if_file_exists(std::string name) {
  std::ifstream f(name.c_str());
  return f.good();
}

void open_file(std::string file) {
  if (check_if_file_exists(file)) {
    buffer.content.clear();
    buffer.name = file;
    std::ifstream in(buffer.name);
    std::string line;
    while (std::getline(in, line)) {
      buffer.content.push_back(line);
    }
    set_mode(NORMAL);
  } else {
    editor.bottomline = " OPEN     | Error: File \"" + file + "\" doesn't exist.";
  }
}

int get_line_wraps(int line) {
    return std::max(1, (int(buffer.content[line].size()) + ui.text_width - 1) / ui.text_width);
}

void move_up() {
  if (editor.cur_line > 0 && buffer.content[editor.cur_line].size() > ui.text_width && editor.cur_char - ui.text_width < buffer.content[editor.cur_line].size()){
    editor.cur_char -= ui.text_width;
  } else if (editor.cur_line > 0 && buffer.content[editor.cur_line - 1].size() > ui.text_width && editor.cur_char % ui.text_width <= buffer.content[editor.cur_line - 1].size()) {
    editor.cur_line -= 1;
    for (int i = 0; i <= buffer.content[editor.cur_line].size(); i += ui.text_width) {
      editor.cur_char += ui.text_width;
    }
    editor.cur_char -= ui.text_width;
  } else if (editor.cur_line > 0) {
    editor.cur_line--;
    if (editor.cur_line < editor.scr_offset) {
      editor.scr_offset = editor.cur_line;
    }
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
}
void move_down() {
  if (editor.cur_line < buffer.content.size() - 1 && buffer.content[editor.cur_line].size() > ui.text_width && editor.cur_char + ui.text_width < buffer.content[editor.cur_line].size()) {
    editor.cur_char += ui.text_width;
  } else if (editor.cur_line < buffer.content.size() - 1) {
    if (editor.cur_char + ui.text_width - editor.cur_char % ui.text_width + 1 <= buffer.content[editor.cur_line].size()) {
      editor.cur_char += ui.text_width;
    } else {
      editor.cur_line++;
      editor.cur_char = editor.cur_char % ui.text_width;
    }
    if (editor.cur_line >= editor.scr_offset + ui.text_height) {
      editor.scr_offset = editor.cur_line - (ui.lines - 3);
    }
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
}
void move_left() {
  if (editor.cur_char > 0) {
    editor.cur_char--;
  } else if (editor.cur_line > 0) {
    move_up();
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
}
void move_right() {
  if (editor.cur_char < buffer.content[editor.cur_line].size()) {
    editor.cur_char++;
  } else if (editor.cur_line < buffer.content.size() - 1) {
    move_down();
    editor.cur_char = 0;
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
}
void page_up() {
  if (editor.cur_line - ui.text_height < 0) { editor.cur_line = 0; editor.scr_offset = 0; }
  else { editor.cur_line -= (ui.lines - 3); editor.scr_offset = editor.cur_line; }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) editor.cur_char = buffer.content[editor.cur_line].size(); 
}
void page_down() {
  if (editor.cur_line + ui.text_height >= buffer.content.size()) { 
    editor.cur_line = buffer.content.size() - 1; 
    editor.scr_offset = (editor.cur_line >= ui.text_height) ? editor.cur_line - (ui.lines - 3) : 0;
  } else { 
    editor.cur_line += ui.text_height + get_line_wraps(editor.cur_line) - 1 - 1; editor.scr_offset = editor.cur_line - (ui.lines - 3); 
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) editor.cur_char = buffer.content[editor.cur_line].size();
}

void normalize_selection() {
  if (selection.start_line > selection.end_line || (selection.start_line == selection.end_line && selection.start_char > selection.end_char)) {
    std::swap(selection.start_line, selection.end_line);
    std::swap(selection.start_char, selection.end_char);
  }
}
bool is_selected(int row, int col) {
  if (!selection.type) return false;

  normalize_selection();

  if (row < selection.start_line || row > selection.end_line) return false;
  if (row == selection.start_line && col < selection.start_char) return false;
  if (row == selection.end_line && col > selection.end_char) return false;

  return true;
}

void del_char(int del_offset) {
  // TODO: merge del_offset if - else statement into single scope using del_offset variable properly
  if (del_offset != 0) {
    if (editor.cur_char > 0) {
      buffer.content[editor.cur_line].erase(editor.cur_char - 1, 1);
      editor.cur_char--;
    } else if (editor.cur_line > 0) {
      int prev = buffer.content[editor.cur_line - 1].size();
      buffer.content[editor.cur_line - 1] += buffer.content[editor.cur_line];
      buffer.content.erase(buffer.content.begin() + editor.cur_line);
      editor.cur_line--;
      editor.cur_char = prev;
      insert_string(" ");
    }
  } else {
    if (editor.cur_char < buffer.content[editor.cur_line].size()) {
      buffer.content[editor.cur_line].erase(editor.cur_char, 1);
    } else if (editor.cur_line < buffer.content.size() - 1) {
      buffer.content[editor.cur_line] += buffer.content[editor.cur_line + 1];
      buffer.content.erase(buffer.content.begin() + editor.cur_line + 1);
    }
  }
}
void del_line() {
  if (buffer.content.empty()) return;

  buffer.content.erase(buffer.content.begin() + editor.cur_line);

  if (buffer.content.empty()) {
    buffer.content.push_back("");
    editor.cur_line = 0;
    editor.cur_char = 0;
    return;
  }

  if (editor.cur_line >= buffer.content.size()) {
    editor.cur_line = buffer.content.size() - 1;
  }

  editor.cur_char = 0;
}
void del_select() {
    if (!selection.type) return;

    normalize_selection();

    if (selection.start_line == selection.end_line) {
        buffer.content[selection.start_line].erase(selection.start_char, selection.end_char - selection.start_char);
    } else {
        buffer.content[selection.end_line].erase(0, selection.end_char);
        buffer.content[selection.start_line].erase(selection.start_char);
        buffer.content[selection.start_line] += buffer.content[selection.end_line];
        buffer.content.erase(buffer.content.begin() + selection.start_line + 1, buffer.content.begin() + selection.end_line + 1);
    }

    editor.cur_line = selection.start_line;
    editor.cur_char = selection.start_char;

    selection.type = 0;
}

void newline() {
  std::string rest = buffer.content[editor.cur_line].substr(editor.cur_char);
  buffer.content[editor.cur_line] = buffer.content[editor.cur_line].substr(0, editor.cur_char);
  buffer.content.insert(buffer.content.begin() + editor.cur_line + 1, rest);
  move_down();
  editor.cur_char = 0;
}



void update_win_size() {
  getmaxyx(stdscr, ui.lines, ui.cols);
}

void update_screen() {
  update_win_size();

  clear();

  ui.linenum_digits = std::to_string(buffer.content.size()).length();
  ui.gutter_width = ui.linenum_digits + 4;
  ui.text_width = ui.cols - ui.gutter_width - 1;

  ui.text_height = ui.lines - ui.bottomline_height;

  int rend_line = 0;
  int rend_char = 0;

  for (int i = editor.scr_offset; i < buffer.content.size() + ui.text_height && rend_line < ui.text_height; i++) {
    if (i < buffer.content.size()) {
      // jank 2-3am code for selection rendering, ~5 cups of coffee were the only thing keeping me alive at that point, eventually passed out, solution to line wrapping was revealed to me in a dream, so this is also jank 6-7am code
      std::string &line = buffer.content[i];

      for (int start = 0; start < std::max(1, int(line.size())); start += ui.text_width) {
        if (rend_line >= ui.text_height) break;

        if (start == 0)
          mvprintw(rend_line, 0, " %*d | ", ui.linenum_digits, i + 1);
        else
          mvprintw(rend_line, 0, " %*s | ", ui.linenum_digits, ".");

        for (int j = start; j < start + ui.text_width && j < line.size(); j++) {
          if (is_selected(i, j)) attron(A_REVERSE);
          mvaddch(rend_line, ui.gutter_width + (j - start), line[j]);
          if (is_selected(i, j)) attroff(A_REVERSE);
        }
        rend_line++;
      }
    } else {
      rend_line++;
      mvprintw(i, 0, " %*s |", ui.linenum_digits, " ");
    }
  }

  mvhline(ui.lines - ui.bottomline_height, 0, '-', ui.cols);

  if (editor.bottomline.empty()) {
    // TODO: torn this into a switch statement 
    if (editor.mode == NONE) {
      editor.bottomline = "          | [O]pen file   [N]ew file   [Q]uit";
    } else if (editor.mode == NORMAL) {
      editor.bottomline = " NORMAL   | " + buffer.name;
      // editor.bottomline = " NORMAL   | " + buffer.name + " | " + std::to_string(editor.cur_line) + ":" + std::to_string(editor.cur_char);
    } else if (editor.mode == WRITE) {
      editor.bottomline = " WRITE    | " + buffer.name;
    } else if (editor.mode == SELECT) {
      if (selection.start_line && selection.end_line && selection.start_char && selection.end_char) {
        editor.bottomline = " SELECT   | " + std::to_string(selection.start_line) + ":" + std::to_string(selection.start_char) + " - " + std::to_string(selection.end_line) + ":" + std::to_string(selection.end_char);
      } else {
        editor.bottomline = " SELECT   | ";
      }
    } else if (editor.mode == MOVE) {
      editor.bottomline = " MOVE     | " + buffer.name;
    } else if (editor.mode == GOTO) {
      editor.bottomline = " GO TO    | L" + editor.input;
    } else if (editor.mode == SAVE) {
      if (editor.input.empty()) {
        editor.bottomline = " SAVE AS  | ...";  
      } else {
        editor.bottomline = " SAVE AS  | " + editor.input;
      }
    } else if (editor.mode == OPEN) {
      if (editor.input.empty()) {
        editor.bottomline = " OPEN     | ...";  
      } else {
        editor.bottomline = " OPEN     | " + editor.input;
      }
    } else if (editor.mode == NEW) {
      if (editor.input.empty()) {
        editor.bottomline = " NEW FILE | ...";  
      } else {
        editor.bottomline = " NEW FILE | " + editor.input;
      }
    } else if (editor.mode == SHELL) {
      editor.bottomline = " SHELL    | $ " + editor.input;
    }
  }

  editor.bottomline += ' ';

  mvprintw(ui.lines - ui.bottomline_height + 1, 0, "%s", editor.bottomline.c_str());

  if (cursor.type != HIDDEN) {
    curs_set(1);
    if (cursor.type == BLOCK) {
      std::cout << "\033[2 q" << std::endl;
    } else if (cursor.type == BAR) {
      std::cout << "\033[5 q" << std::endl;
    } else if (cursor.type == LINE) {
      std::cout << "\033[3 q" << std::endl;
    }
  } else {
    curs_set(0);
  }

  cursor.row = 0;

  for (int i = editor.scr_offset; i < editor.cur_line; i++) {
    int wraps = get_line_wraps(i);
    cursor.row += wraps;
  }

  cursor.row += editor.cur_char / ui.text_width;

  cursor.col = ui.gutter_width + (editor.cur_char % ui.text_width);

  move(cursor.row, cursor.col);

  refresh();
}



int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::cerr << "Error: Incorrect usage: Too many arguments given.\nCorrect usage:\n  kcz [<editor.input>]" << std::endl;
    exit(1);
  }

  if (argc == 2 && argv[1]) {
    open_file(argv[1]);
  } else {
    buffer.name = "";
    set_mode(NONE);
  }

  if (buffer.content.empty()) {
    buffer.content.push_back("");
  }

  ui.bottomline_height = 2;

  initscr();

  raw();
  noecho();
  keypad(stdscr, true);

  while (true) {
    update_screen();

    int ch = getch();

    if (ch) {
      editor.bottomline = "";
    }

    switch (editor.mode) {
      case NONE:
        if (ch == 27) goto end_loop;
        else if (ch == 'O' || ch == 'o') set_mode(OPEN); 
        else if (ch == 'N' || ch == 'n') set_mode(NEW);
        else if (ch == 'Q' || ch == 'q') goto end_loop;
        break;

      case NORMAL:
        if (ch == '\n') set_mode(WRITE);
        else if (ch == 'v') set_mode(SELECT, 1);
        else if (ch == 'm') set_mode(MOVE);
        else if (ch == 'g') set_mode(GOTO);
        else if (ch == '$') set_mode(SHELL);
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == 'Q') goto end_loop;
        else if (ch == 's') save(true);
        else if (ch == 'S') set_mode(SAVE);
        else if (ch == 'X') { save(true); goto end_loop; }
        else if (ch == 'O') set_mode(OPEN);
        else if (ch == 'N') set_mode(NEW);
        else if (ch == 'd' || ch == KEY_DC || ch == 330) del_char(0);
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') move_left();
        else if (ch == 'D') del_line();
        else if (ch == 'x') set_mode(SELECT, 2);
        else if (ch == 't') { 
          editor.cur_line = 0; editor.scr_offset = 0; 
          if (editor.cur_char > buffer.content[0].size() - 1) editor.cur_char = buffer.content[0].size();
        } 
        else if (ch == 'b') { 
          editor.cur_line = buffer.content.size() - 1;
          editor.scr_offset = (editor.cur_line >= ui.text_height) ? editor.cur_line - (ui.lines - 3) : 0;
          if (editor.cur_char > buffer.content[editor.cur_line].size()) editor.cur_char = buffer.content[editor.cur_line].size();
        } 
        else if (ch == KEY_HOME) editor.cur_char = 0;
        else if (ch == KEY_END) editor.cur_char = buffer.content[editor.cur_line].size();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        break;

      case WRITE:
        if (ch == 27) set_mode(NORMAL);
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') del_char(-1);
        else if (ch == KEY_DC || ch == 330) del_char(0);
        else if (ch == '\n') newline();
        else if (ch == KEY_IC) { // insert => toggle overwrite
          if (editor.insert == true) { editor.insert = false; cursor.type = BAR;  }
          else { editor.insert = true; cursor.type = LINE; }
        }
        else if (ch == KEY_HOME) editor.cur_char = 0;
        else if (ch == KEY_END) editor.cur_char = buffer.content[editor.cur_line].size();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        else if (std::isprint(ch) || std::isspace(ch)) {
          if (editor.insert) del_char(0); // kind of a workaround tbh but it works
          buffer.content[editor.cur_line].insert(editor.cur_char, 1, ch);
          editor.cur_char++;
        }
        break;

      case SELECT:
        if (ch == 27) { set_mode(NORMAL); selection.type = 0; }
        else if (ch == 'x') set_mode(SELECT, 2);
        else if (ch == 'D' || ch == 'd' || ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127 || ch == 263 || ch == 330 || ch == '\b') {
          del_select();
          set_mode(NORMAL);
        }
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == KEY_HOME) editor.cur_char = 0;
        else if (ch == KEY_END) editor.cur_char = buffer.content[editor.cur_line].size();

        if (selection.type == 2) {
          if (editor.cur_line < selection.start_line) {
            selection.start_line = editor.cur_line;
            selection.start_char = 0;
          } else if (editor.cur_line > selection.end_line ){
            selection.end_line = editor.cur_line;
            selection.end_char = buffer.content[editor.cur_char].size();        
          }
        }
        else if (selection.type == 1) {
          selection.start_line = selection.anchor_line;
          selection.start_char = selection.anchor_char;

          selection.end_line = editor.cur_line;
          selection.end_char = editor.cur_char;
        }
        break;

      case MOVE:
        // TODO: make it work with selection
        if (ch == 27) set_mode(NORMAL);
        else if (ch == KEY_UP && editor.cur_line > 0) {
          std::swap(buffer.content[editor.cur_line], buffer.content[editor.cur_line - 1]); move_up();
        }
        else if (ch == KEY_DOWN && editor.cur_line < buffer.content.size() - 1) {
          std::swap(buffer.content[editor.cur_line], buffer.content[editor.cur_line + 1]); move_down();
        }
        else if (ch == 't') {
          std::swap(buffer.content[editor.cur_line], buffer.content[0]); editor.cur_line = 0; editor.scr_offset = 0;
          if (editor.cur_char > buffer.content[0].size() - 1) editor.cur_char = buffer.content[0].size();
        }
        else if (ch == 'b') {
          std::swap(buffer.content[editor.cur_line], buffer.content.back());
          editor.cur_line = buffer.content.size() - 1;
          editor.scr_offset = (editor.cur_line >= ui.text_height) ? editor.cur_line - (ui.lines - 3) : 0;
          if (editor.cur_char > buffer.content[editor.cur_line].size()) editor.cur_char = buffer.content[editor.cur_line].size();
        }
        break;

      case GOTO:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           // TODO: optimize this shit
           if (!editor.input.empty()) {
             int inputnum; std::stringstream(editor.input) >> inputnum;
             if (inputnum > 0 && inputnum <= buffer.content.size()) {
               editor.cur_line = inputnum - 1;
               if (buffer.content.size() <= ui.text_height) editor.scr_offset = 0;
               else editor.scr_offset = (inputnum <= ui.text_height) ? 0 : inputnum - ui.text_height;
               editor.input.clear(); set_mode(NORMAL);
             } else { editor.bottomline = " GO TO    | Error: Line number out of range."; editor.input.clear(); set_mode(NORMAL); }
           } else { editor.bottomline = " GO TO    | Error: No line number inputted."; set_mode(NORMAL); }
        }
        else if (std::isdigit(ch)) editor.input.push_back(ch);
        break;

      case SAVE:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           if (!editor.input.empty()) {
             buffer.name = editor.input; editor.input.clear(); save(true); set_mode(NORMAL);
           } else editor.bottomline = " SAVE AS  | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case OPEN:
        if (ch == 27) goto end_loop;
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           if (!editor.input.empty()) { open_file(editor.input); editor.input.clear(); }
           else editor.bottomline = " OPEN     | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case NEW:
        if (ch == 27) goto end_loop;
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           if (!editor.input.empty()) { new_file(editor.input); editor.input.clear(); set_mode(NORMAL); }
           else editor.bottomline = " NEW FILE | Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case SHELL:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           std::string cmdout = run_shellcmd(editor.input, true); insert_string(cmdout); editor.input.clear(); set_mode(NORMAL);
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;
    }

    if (editor.cur_line >= buffer.content.size()) {
      editor.cur_line = buffer.content.size() - 1;
    }
    if (editor.cur_char > buffer.content[editor.cur_line].size()) {
      editor.cur_char = buffer.content[editor.cur_line].size();
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
