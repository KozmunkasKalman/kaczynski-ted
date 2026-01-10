#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <locale>
#include <clocale>
#include <cwchar>
#include <filesystem>
#include <ncurses.h>



enum Mode { NONE, NORMAL, WRITE, SELECT, MOVE, GOTO, FIND, SAVE, NEW, OPEN, RENAME, SHELL};
enum CursorType { HIDDEN, BAR, LINE, BLOCK };
enum BufferType { EMPTY, TEXT, FILEMANAGER };
// TODO: add terminal emulator mode

struct DirEntry { std::string name; bool is_dir; };



struct {
  Mode mode;

  int cur_line; // current global vertical position
  int cur_char; // current global horizontal position
  int scr_offset; // screen offset compared to editor.cur_line

  std::string bottomline;
  std::string bottomline_right;
  std::string input;

  bool insert;
  bool scrlock;

  std::string dir;
} editor;

struct {
  BufferType type;
  std::string name;
  std::vector<std::string> content;
  std::vector<DirEntry> dir_content;
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

  int text_height;
} ui;

struct {
  std::string query;
  int last_line;
  int last_char;
  std::string last_find;
} search;

struct {
  bool enable_line_numbers;
  bool verbose_open;
  int bottomline_height;
  int tab_size;

  // TODO: implement custom binds
} config;



bool file_exists(std::string path) {
  return std::filesystem::exists(path);
}

std::string get_config_path() {
  return std::string(std::getenv("HOME")) + "/.config/kaczynski/unabombrc";
}
bool to_bool(std::string v) {
  if (v == "true"  || v == "1" || v == "yes" || v == "on" ) return true;
  if (v == "false" || v == "0" || v == "no"  || v == "off") return false;
  else {
    std::cerr << "Error: Invalid value in unabombrc.\n" + v + " is not valid for bool. Valid values: \"true\", \"false\", \"yes\", \"no\", \"on\", \"off\", \"1\", \"0\".";
    exit(1);
  }
}
std::string trim(std::string s) {
  int start = s.find_first_not_of(" \t");
  if (start == std::string::npos) return "";
  int end = s.find_last_not_of(" \t");
  return s.substr(start, end - start + 1);
}
void parse_config(std::string path) {
  std::ifstream config_file(path);
  std::string line; 

  while (std::getline(config_file, line)) {
    line = trim(line);

    // empty or comment
    if (line.empty() || line[0] == '#') continue;

    // key = value pairs
    auto equals = line.find('=');
    if (equals == std::string::npos) continue;

    std::string key = trim(line.substr(0, equals));
    std::string value = trim(line.substr(equals + 1));

    if (key == "line_numbers" || key == "enable_line_nums") {
      config.enable_line_numbers = to_bool(value);
    }
    else if (key == "verbose_open") {
      config.verbose_open = to_bool(value);
    }
    else if (key == "bottomline_height") {
      if (std::stoi(value) > 0) config.bottomline_height = std::stoi(value);
    }
    else if (key == "tab_size" || key == "tab_chars" || key == "tab_width") {
      if (std::stoi(value) > 0) config.tab_size = std::stoi(value);
    }
  }
}
void load_config() {
  /* default values */
  config.enable_line_numbers = true;
  config.verbose_open = true;
  config.bottomline_height = 2;
  config.tab_size = 4;

  if (get_config_path().empty() || !file_exists(get_config_path())) return;

  parse_config(get_config_path());
}



std::string run_shellcmd(std::string cmd, bool do_msg) {
  // TODO: - fix freezes from commands that: - read stdin (like `$ cat` with no arguments)
  //                                         - run until killed with ^C (like `$ top`)
  //       - make the function take in std::string_view instead of std::string (cmd_full.append() could work)
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
    output += line + '\n';
  }

  if (do_msg) {
    std::string outmsg = (output.find('\n') != std::string::npos) ? output.substr(0, output.find('\n')) : output;
    editor.bottomline = " SHELL    │ > " + outmsg;
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

  int prev = editor.cur_char;
  editor.cur_line += str_vect.size() - 1;
  editor.cur_char = buffer.content[editor.cur_line].size() - tail.size();
}



bool dirEntryComparator(DirEntry &a, DirEntry &b) {
    if (a.name == "..") return true;
    if (b.name == "..") return false;

    if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;

    std::string nameA = a.name;
    std::string nameB = b.name;
    std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
    std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
    return nameA < nameB;
}
void load_directory(std::string path) {
  buffer.content.clear();
  buffer.dir_content.clear();

  editor.dir = path;

  buffer.type = FILEMANAGER;
  buffer.name = path;

  if (path != "/") {
    buffer.content.push_back("..");
    buffer.dir_content.push_back({"..", true});
  }

  for (auto entry : std::filesystem::directory_iterator(path)) {
    DirEntry d;
    d.name = entry.path().filename().string();
    d.is_dir = entry.is_directory();
    buffer.dir_content.push_back(d);
    // TODO: add verbose output to it, which should look similar to the output of 'ls -Apho1 | tail -n +2'
    buffer.content.push_back(d.name + (d.is_dir ? "/" : ""));
  }

  std::sort(buffer.dir_content.begin(), buffer.dir_content.end(), dirEntryComparator);

  editor.cur_line = 0;
  editor.cur_char = 0;
  editor.scr_offset = 0;
}



void set_mode(Mode target_mode, int submode = 0) {
  switch (target_mode) {
    case NORMAL:
      editor.mode = NORMAL;
      cursor.type = BLOCK;
      if (submode != 1) {
        editor.bottomline = " NORMAL   │ " + buffer.name;
      }
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

    case FIND:
      editor.mode = FIND;
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
      cursor.type = BLOCK;
      load_directory(editor.dir);
      if (submode != 1) {
        editor.bottomline = " OPEN     │ " + editor.dir + "/";
      }
      break;

    case RENAME:
      editor.mode = RENAME;
      cursor.type = BLOCK;
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
      editor.bottomline = " SAVED    │ " + buffer.name;
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

void open_file(std::string file) {
  buffer.content.clear();
  buffer.type = TEXT;
  buffer.name = file;
  std::ifstream in(buffer.name);
  std::string line;
  while (std::getline(in, line)) {
    buffer.content.push_back(line);
  }
  editor.cur_line = 0;
  editor.cur_char = 0;
  editor.scr_offset = 0;
  set_mode(NORMAL);
}

int get_line_wraps(int line) {
  if (line < 0 || line >= buffer.content.size()) return 1;
  return std::max(1, (int(buffer.content[line].size()) + ui.text_width - 1) / ui.text_width);
}
int vis_lines_between(int line1, int line2) {
  int vis_lines = 0;
  for (int i = line1; i < line2; i++) {
    vis_lines += get_line_wraps(i);
  }
  return vis_lines;
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
    if (editor.cur_line < buffer.content.size() && buffer.content[editor.cur_line].size() > ui.text_width && editor.cur_char + ui.text_width < buffer.content[editor.cur_line].size()) {
    editor.cur_char += ui.text_width;
  } else if (editor.cur_line < buffer.content.size() - 1) {
    if (editor.cur_char / ui.text_width + 1 < get_line_wraps(editor.cur_line)) {
      editor.cur_char += ui.text_width;
    } else {
      editor.cur_line++;
      editor.cur_char %= ui.text_width;
    }
    if (editor.cur_line >= editor.scr_offset + ui.text_height) {
      editor.scr_offset = editor.cur_line - (ui.text_height - 1);
    }
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }

  int visual_row = 0;
  for (int i = editor.scr_offset; i < editor.cur_line; i++) {
    visual_row += get_line_wraps(i);
  }
  visual_row += editor.cur_char / ui.text_width;
  while (visual_row >= ui.text_height) {
    editor.scr_offset++;
    visual_row--;
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
void buffer_top() {
  editor.cur_line = 0;
  editor.scr_offset = 0;
  if (editor.cur_char > buffer.content[0].size() - 1) {
    editor.cur_char = buffer.content[0].size();
  }
}
void buffer_bottom() {
  editor.cur_line = buffer.content.size() - 1;
  editor.scr_offset = (editor.cur_line >= ui.text_height) ? editor.cur_line - (ui.lines - 3 - get_line_wraps(editor.cur_line)) : 0;
  if (buffer.content[editor.cur_line].size() > ui.text_width) {
    editor.scr_offset += int(buffer.content[editor.cur_line].size() / ui.text_width);
  }
  if (editor.cur_char > buffer.content[editor.cur_line].size()) {
    editor.cur_char = buffer.content[editor.cur_line].size();
  }
}
void page_up() {
  // very messy, TODO: optimize and refactor
  switch (buffer.type) {
    case TEXT: {
      if (editor.cur_line <= 0) {
        buffer_top();
        break;
      }
      int target = editor.cur_line;

      int vis_lines = 0;
      for (int i = editor.cur_line; i > editor.cur_line - ui.text_height + 1; i--) {
        vis_lines += get_line_wraps(i);
        target -= 1;
        if (vis_lines >= ui.text_height - 1) {
          break;
        }
      }
      if (target <= 0) target = 0;

      editor.cur_line = target;
      editor.scr_offset = target;

      if (editor.cur_char > buffer.content[editor.cur_line].size()) {
        editor.cur_char = buffer.content[editor.cur_line].size();
      }

      break;
    }
    case FILEMANAGER: {
      if (buffer.content.size() <= ui.text_height) {
        editor.cur_line = 0;
        editor.scr_offset = 0;
      } else if (editor.cur_line - ui.text_height - 1 < editor.scr_offset + ui.text_height) {
        editor.cur_line += ui.text_height - 1;
      } else if (editor.cur_line + ui.text_height - 1 >= buffer.content.size()) {
        editor.cur_line = buffer.content.size() - 1;
        editor.scr_offset = buffer.content.size() - ui.text_height;
      } else {
        editor.cur_line += ui.text_height - 1;
        editor.scr_offset += ui.text_height - 1;
      }
      break;
    }
  }
}
void page_down() {
  // very messy, TODO: optimize and refactor
  switch (buffer.type) {
    case TEXT: {
      if (editor.cur_line >= buffer.content.size()) {
        buffer_bottom();
        break;
      }
      int target = editor.cur_line;

      int vis_lines = 0;
      for (int i = editor.cur_line; i < editor.cur_line + ui.text_height - 1; i++) {
        vis_lines += get_line_wraps(i);
        target += 1;
        if (vis_lines >= ui.text_height - 1) {
          break;
        }
      }
      if (target >= buffer.content.size() - 1) target = buffer.content.size() - 1;

      int scr_offset_target = target;

      vis_lines = 0;
      for (int j = target; j > target - ui.text_height - 1; j--) {
        vis_lines += get_line_wraps(j);
        scr_offset_target -= 1;
        if (vis_lines >= ui.text_height - 1) {
          break;
        }
      }

      editor.cur_line = target;
      editor.scr_offset = scr_offset_target;

      if (buffer.content[editor.cur_line].size() > ui.text_width) {
        editor.scr_offset += int(buffer.content[editor.cur_line].size() / ui.text_width);
        // this doesnt work properly, commented out until fixed
        // editor.scr_offset -= vis_lines_between(editor.scr_offset, editor.cur_line) - (editor.cur_line - editor.scr_offset);
      }

      if (editor.cur_char > buffer.content[editor.cur_line].size()) {
        editor.cur_char = buffer.content[editor.cur_line].size();
      }

      break;
    }
    case FILEMANAGER: {
      if (buffer.content.size() <= ui.text_height) {
        editor.cur_line = buffer.content.size() - 1;
        editor.scr_offset = 0;
      } else if (editor.cur_line + ui.text_height - 1 < editor.scr_offset + ui.text_height) {
        editor.cur_line += ui.text_height - 1;
      } else if (editor.cur_line + ui.text_height - 1 >= buffer.content.size()) {
        editor.cur_line = buffer.content.size() - 1;
        editor.scr_offset = buffer.content.size() - ui.text_height;
      } else {
        editor.cur_line += ui.text_height - 1;
        editor.scr_offset += ui.text_height - 1;
      }
      break;
    }
  }
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

    // kinda goofy solution but it prevents visual glitches upon deleting whole buffer
    if (editor.cur_line == 0 && editor.cur_char == 0) page_up();

    selection.type = 0;
}
bool del_path(std::string path, bool is_dir) {
  std::error_code ec;

  if (is_dir) {
    std::filesystem::remove_all(path, ec);
  } else {
    std::filesystem::remove(path, ec);
  }

  return !ec;
}

void newline() {
  std::string rest = buffer.content[editor.cur_line].substr(editor.cur_char);
  buffer.content[editor.cur_line] = buffer.content[editor.cur_line].substr(0, editor.cur_char);
  buffer.content.insert(buffer.content.begin() + editor.cur_line + 1, rest);
  move_down();
  editor.cur_char = 0;
}

bool find_forward(std::string str) {
  if (str.empty()) return false;

  for (int i = editor.cur_line; i < buffer.content.size(); i++) {
    int start = (i == editor.cur_line) ? editor.cur_char + 1 : 0;
    int pos = buffer.content[i].find(str, start);

    if (pos != std::string::npos) {
      editor.cur_line = i;
      editor.cur_char = pos;

      if (editor.cur_line < editor.scr_offset) editor.scr_offset = editor.cur_line;
      else if (editor.cur_line >= editor.scr_offset + ui.text_height) editor.scr_offset = editor.cur_line - ui.text_height + 1;

      return true;
    }
  }
  return false;
}




void update_win_size() {
  getmaxyx(stdscr, ui.lines, ui.cols);
}
void render_buffer() {
  int rend_line = 0;

  switch (buffer.type) {
    case TEXT:
      for (int i = editor.scr_offset; i < buffer.content.size() + ui.text_height && rend_line < ui.text_height; i++) {
        if (i < buffer.content.size()) {
          std::string &line = buffer.content[i];

          for (int start = 0; start < std::max(1, int(line.size())); start += ui.text_width) {
            if (rend_line >= ui.text_height) break;

            if (config.enable_line_numbers) {
              if (start == 0)
                mvprintw(rend_line, 0, " %*d │ ", ui.linenum_digits, i + 1);
              else
                mvprintw(rend_line, 0, " %*s │ ", ui.linenum_digits, ".");
            }

            for (int j = start; j < start + ui.text_width && j < line.size(); j++) {
              if (is_selected(i, j)) attron(A_REVERSE);
              mvaddch(rend_line, ui.gutter_width + (j - start), line[j]);
              if (is_selected(i, j)) attroff(A_REVERSE);
            }
            rend_line++;
          }
        } else {
          if (config.enable_line_numbers) mvprintw(rend_line, 0, " %*s │", ui.linenum_digits, " ");
          rend_line++;
        }
      }
      break;

    case FILEMANAGER:
      for (int i = editor.scr_offset; i < buffer.dir_content.size() && rend_line < ui.text_height; i++) {
        auto entry = buffer.dir_content[i];

        if (i == editor.cur_line) attron(A_REVERSE);

        std::string label = entry.name + (entry.is_dir ? "/" : "");

        mvprintw(rend_line, 1, "%s", label.c_str());

        if (i == editor.cur_line) attroff(A_REVERSE);

        rend_line++;
       }
       break;
  }
}
void render_bottomline() {
  for (int i = 0; i < ui.cols; i++) {
    mvaddwstr(ui.lines - config.bottomline_height, i, L"─");
  }
  
  mvaddwstr(ui.lines - config.bottomline_height, 10, L"┬");   
  if (config.enable_line_numbers) {
    if (ui.gutter_width - 2 == 10) {
      mvaddwstr(ui.lines - config.bottomline_height, 10, L"┼");   
    } else {
      mvaddwstr(ui.lines - config.bottomline_height, ui.gutter_width - 2, L"┴");
    }
  }

  if (editor.bottomline.empty()) {
    // TODO: turn this into a switch statement 
    if (editor.mode == NONE) {
      editor.bottomline = "          │ [O]pen file   [N]ew file   [Q]uit";
    } else if (editor.mode == NORMAL) {
      editor.bottomline = " NORMAL   │ " + buffer.name;
    } else if (editor.mode == WRITE) {
      editor.bottomline = " WRITE    │ " + buffer.name;
    } else if (editor.mode == SELECT) {
      if (selection.start_line && selection.end_line && selection.start_char && selection.end_char) {
        editor.bottomline = " SELECT   │ " + std::to_string(selection.start_line) + ":" + std::to_string(selection.start_char) + " - " + std::to_string(selection.end_line) + ":" + std::to_string(selection.end_char);
      } else {
        editor.bottomline = " SELECT   │ ";
      }
    } else if (editor.mode == MOVE) {
      editor.bottomline = " MOVE     │ " + buffer.name;
    } else if (editor.mode == GOTO) {
      editor.bottomline = " GO TO    │ L" + editor.input;
    } else if (editor.mode == FIND) {
      if (editor.input.empty()) {
        editor.bottomline = " FIND     │ \"...\"";
      } else {
        editor.bottomline = " FIND     │ \"" + editor.input + "\"";
      }
    } else if (editor.mode == SAVE) {
      if (editor.input.empty()) {
        editor.bottomline = " SAVE AS  │ ...";  
      } else {
        editor.bottomline = " SAVE AS  │ " + editor.input;
      }
    } else if (editor.mode == NEW) {
      if (editor.input.empty()) {
        editor.bottomline = " NEW FILE │ ...";  
      } else {
        editor.bottomline = " NEW FILE │ " + editor.input;
      }
    } else if (editor.mode == OPEN) {
      editor.bottomline = " OPEN     │ " + editor.dir + "/";
    } else if (editor.mode == RENAME) {
      editor.bottomline = " RENAME   │ " + buffer.dir_content[editor.cur_line].name + " -> " + editor.input;
    } else if (editor.mode == SHELL) {
      editor.bottomline = " SHELL    │ $ " + editor.input;
    }
  }

  editor.bottomline += ' ';

  mvprintw(ui.lines - config.bottomline_height + 1, 0, "%s", editor.bottomline.c_str());

  if (buffer.type == TEXT || buffer.type == FILEMANAGER) {
    editor.bottomline_right = std::to_string(editor.cur_line + 1) + ":" + std::to_string(editor.cur_char + 1) + ", " + std::to_string(editor.scr_offset);
    mvprintw(ui.lines - config.bottomline_height + 1, ui.cols - (editor.bottomline_right.size() + 3), "│ %s ", editor.bottomline_right.c_str());
    mvaddwstr(ui.lines - config.bottomline_height, ui.cols - (editor.bottomline_right.size() + 3), L"┬");   
  }


}
void render_cursor() {
  static CursorType last_cursor = HIDDEN;
  if (cursor.type != HIDDEN) {
    curs_set(1);
    if (cursor.type != last_cursor) {
      if (cursor.type == BLOCK) printf("\033[2 q");
      else if (cursor.type == BAR) printf("\033[5 q");
      else if (cursor.type == LINE) printf("\033[3 q");
      fflush(stdout); 
      last_cursor = cursor.type;
    }
  } else {
    curs_set(0);
    last_cursor = HIDDEN;
  }

  cursor.row = 0;
  for (int i = editor.scr_offset; i < editor.cur_line; i++) {
    cursor.row += get_line_wraps(i);
  }
  cursor.row += editor.cur_char / ui.text_width;

  cursor.col = ui.gutter_width + (editor.cur_char % ui.text_width);

  move(cursor.row, cursor.col);
}
void update_screen() {
  update_win_size();

  erase();

  switch (buffer.type) {
    case EMPTY:
      ui.linenum_digits = 0;
      ui.gutter_width = 1;
      ui.text_width = ui.cols - ui.gutter_width - 1;
      ui.text_height = ui.lines - config.bottomline_height;
      break;
    case TEXT:
      if (config.enable_line_numbers) {
        ui.linenum_digits = std::to_string(buffer.content.size()).length();
        ui.gutter_width = ui.linenum_digits + 4;
      } else {
        ui.linenum_digits = 0;
        ui.gutter_width = 1;
      }
      ui.text_width = ui.cols - ui.gutter_width - 1;
      ui.text_height = ui.lines - config.bottomline_height;
      break;
    case FILEMANAGER:
      ui.linenum_digits = 0;
      ui.gutter_width = 1;
      ui.text_width = ui.cols - ui.gutter_width - 1;
      ui.text_height = ui.lines - config.bottomline_height;
      break;
  }

  render_buffer();

  render_bottomline();

  render_cursor();

  refresh();
}



int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::cerr << "Error: Incorrect usage: Too many arguments given.\nCorrect usage:\n  kcz [<file>]" << std::endl;
    exit(1);
  }

  setlocale(LC_ALL, "");
  setlocale(LC_CTYPE, "");

  load_config();

  editor.dir = std::filesystem::current_path();

  if (argc == 2 && argv[1]) {
    buffer.type = TEXT;
    open_file(argv[1]);
  } else {
    buffer.type = EMPTY;
    buffer.name = "";
    set_mode(NONE);
  }

  if (buffer.content.empty()) {
    buffer.content.push_back("");
  }

  initscr();

  raw();
  noecho();
  keypad(stdscr, true);

  while (true) {
    update_screen();

    wint_t ch;
    get_wch(&ch);

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
        if (ch == 27) search.last_find = "";
        else if (ch == '\n') set_mode(WRITE);
        else if (ch == 'v') set_mode(SELECT, 1);
        else if (ch == 'm') set_mode(MOVE);
        else if (ch == 'g') set_mode(GOTO);
        else if (ch == 'F') set_mode(FIND);
        else if (ch == 'f') {
          if (!search.last_find.empty()) {
            if (!find_forward(search.last_find)) {
              editor.bottomline = " FIND     │ No further matches.";
              search.last_find = "";
            }
          } else {
            set_mode(FIND);
          }
        }
        else if (ch == '$') set_mode(SHELL);
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == 'Q' || ch == 'q') goto end_loop;
        else if (ch == 's') save(true);
        else if (ch == 'S') set_mode(SAVE);
        else if (ch == 'X') { save(true); goto end_loop; }
        else if (ch == 'O') set_mode(OPEN);
        else if (ch == 'N') set_mode(NEW);
        else if (ch == ' ') move_right();
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') move_left();
        else if (ch == 'd' || ch == KEY_DC || ch == 330) del_char(0);
        else if (ch == 'D') del_line();
        else if (ch == 'x') set_mode(SELECT, 2);
        else if (ch == 't') buffer_top();
        else if (ch == 'b') buffer_bottom();
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
        else if (ch == '\t') { for (int i = 0; i < config.tab_size; i++) { insert_string(" "); } }
        else if (std::iswprint(ch) || std::iswspace(ch)) {
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
        // TODO: implement FIND for selected text as pattern
        else if (ch == KEY_UP) move_up();
        else if (ch == KEY_DOWN) move_down();
        else if (ch == KEY_LEFT) move_left();
        else if (ch == KEY_RIGHT) move_right();
        else if (ch == KEY_HOME) editor.cur_char = 0;
        else if (ch == KEY_END) editor.cur_char = buffer.content[editor.cur_line].size();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        else if (ch == 't') buffer_top();
        else if (ch == 'b') buffer_bottom();
        if (selection.type == 2) {
          if (editor.cur_line < selection.start_line) {
            selection.start_line = editor.cur_line;
            selection.start_char = 0;
          } else if (editor.cur_line > selection.end_line ){
            selection.end_line = editor.cur_line;
            selection.end_char = buffer.content[editor.cur_line].size();
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
        else if (ch == 't') buffer_top();
        else if (ch == 'b') buffer_bottom();
        break;

      case GOTO:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if (editor.input.empty() && ch == 't') { 
          buffer_top();
          editor.input.clear();
          set_mode(NORMAL); 
        } 
        else if (editor.input.empty() && ch == 'b') { 
          buffer_bottom();
          editor.input.clear();
          set_mode(NORMAL); 
        } 
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
             } else { editor.bottomline = " GO TO    │ Error: Line number out of range."; editor.input.clear(); set_mode(NORMAL, 1); }
           } else { editor.bottomline = " GO TO    │ Error: No line number inputted."; set_mode(NORMAL, 1); }
        }
        else if (std::isdigit(ch)) editor.input.push_back(ch);
        break;

      case FIND:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
          if (!editor.input.empty()) {
            search.last_find = editor.input;
            if (!find_forward(search.last_find)) {
              editor.bottomline = " FIND     │ Pattern not found.";
              set_mode(NORMAL, 1);
            } else {
              set_mode(NORMAL);
              editor.input.clear();
            }
          } else {
            editor.bottomline = " FIND     │ Error: No pattern given to match.";
            set_mode(NORMAL, 1);
          }
        }
        else if (std::isprint(ch) || std::isspace(ch)) editor.input.push_back(ch);
        break;

      case SAVE:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
           if (!editor.input.empty()) {
             if (file_exists(editor.input)) editor.bottomline = " SAVE AS  │ Warning: That file already exists: choose a different one, or remove it.";
             else {
               buffer.name = editor.input;
               editor.input.clear();
               save(true);
               set_mode(NORMAL); }
           } else editor.bottomline = " SAVE AS  │ Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case NEW:
        if (ch == 27) {
          if (buffer.type == TEXT) {
            set_mode(NORMAL);
          } else if (buffer.type == FILEMANAGER) {
            set_mode(OPEN);
          } else {
            goto end_loop;
          }
        }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
          if (!editor.input.empty()) {
            if (file_exists(editor.input)) editor.bottomline = " NEW FILE │ Warning: File already exists."; 
            else {
              if (buffer.type == TEXT) {
                buffer.name = editor.input;
                editor.input.clear();
                save(true);
                set_mode(NORMAL);
              } else if (buffer.type == FILEMANAGER) {
                std::string cmd = "touch " + editor.input;
                run_shellcmd(cmd, false);
                editor.input.clear();
                update_screen();
                set_mode(OPEN);
              } else if (buffer.type == EMPTY) {
                buffer.type = TEXT;
                buffer.name = editor.input;
                editor.input.clear();
                save(true);
                set_mode(NORMAL);
              }
            }
          } else editor.bottomline = " NEW FILE │ Error: No filename. Please input a filename.";  
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case OPEN:
        if (ch == 'Q' || ch == 'q') goto end_loop;
        else if (ch == KEY_UP && editor.cur_line > 0) move_up();
        else if (ch == KEY_DOWN && editor.cur_line < buffer.dir_content.size() - 1) move_down();
        else if (ch == 't' || ch == KEY_HOME) buffer_top();
        else if (ch == 'b' || ch == KEY_END) buffer_bottom();
        else if (ch == 339) page_up();
        else if (ch == 338) page_down();
        else if (ch == 'N' || ch == 'n') set_mode(NEW);
        else if (ch == 'D' || ch == 'd' || ch == KEY_DC || ch == 330) {
          if (buffer.dir_content.empty()) break;

          auto entry = buffer.dir_content[editor.cur_line];

          if (entry.name == "..") {
            editor.bottomline = " OPEN     │ Error: Cannot delete '..',";
            break;
          }

          std::string fullpath = editor.dir + "/" + entry.name;

          editor.bottomline = " DELETE   │ Are you sure you want to delete '" + entry.name + "'? Press [Y] [S-Y] or [enter] to delete.";
          update_screen();
          int confirm = getch();
          if (confirm == 'Y' || confirm == 'y' || confirm == '\n') {
            if (del_path(fullpath, entry.is_dir)) {
              editor.bottomline = " DELETE   │ Deleted: " + entry.name;
              load_directory(editor.dir);

              if (editor.cur_line >= buffer.dir_content.size()) editor.cur_line = buffer.dir_content.size() - 1;
              if (editor.cur_line < 0) editor.cur_line = 0;
            } else { editor.bottomline = " DELETE   │ Error: Failed to delete."; }
          } else { editor.bottomline = " DELETE   │ Cancelled by user."; }
        }
        else if (ch == 'r') {
          if (buffer.dir_content.empty()) break;
          auto entry = buffer.dir_content[editor.cur_line];
          if (entry.name == "..") break;
          editor.input = entry.name;
          set_mode(RENAME);
        }
        else if (ch == '\n') {
          auto entry = buffer.dir_content[editor.cur_line];
          std::string fullpath = editor.dir + "/" + entry.name;

          if (entry.is_dir) {
            load_directory(std::filesystem::canonical(fullpath).string());
          } else {
            open_file(fullpath);
          }
        }
        break;

      case RENAME:
        if (ch == 27) { editor.input.clear(); set_mode(OPEN); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
          if (editor.input.empty()) editor.bottomline = " RENAME   │ Error: Empty name.";
          else if (editor.input == ".." || editor.input == "." || editor.input == "/") editor.bottomline = " RENAME   │ Error: Invalid filename.";
          else {
            auto entry = buffer.dir_content[editor.cur_line];
            if (entry.name == "..") break;
            std::string oldpath = editor.dir + "/" + entry.name;
            std::string newpath = editor.dir + "/" + editor.input;
            if (std::filesystem::exists(newpath)) editor.bottomline = " RENAME   │ Warning: File already exists.";
            else {
              std::filesystem::rename(oldpath, newpath);
              editor.bottomline = " RENAME   │ Rename successful.";
            }
          }
          editor.input.clear();
          set_mode(OPEN);
          load_directory(editor.dir);
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;

      case SHELL:
        if (ch == 27) { editor.input.clear(); set_mode(NORMAL); }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 263 || ch == '\b') && !editor.input.empty()) editor.input.pop_back();
        else if (ch == '\n') {
          std::string cmdout = run_shellcmd(editor.input, true);
          insert_string(cmdout);
          editor.input.clear();
          set_mode(NORMAL);
        }
        else if (std::isprint(ch)) editor.input.push_back(ch);
        break;
    }

    if (buffer.type == TEXT) {
      if (editor.cur_line >= buffer.content.size()) editor.cur_line = buffer.content.size() - 1;
      if (editor.cur_char > buffer.content[editor.cur_line].size()) editor.cur_char = buffer.content[editor.cur_line].size();
    }
  }

  end_loop:

  noraw();
  echo();
  keypad(stdscr, FALSE);

  endwin();

  return 0;
}
