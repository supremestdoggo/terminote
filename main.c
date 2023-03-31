#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>

const double phi = 1.61803398875;

const char default_note_content[] = "Hello! This note is created when you first run \n"
"terminote. To scroll through note options within \n"
"the sidebar, use the up and down arrow keys. If \n"
"a note's title is too long for the sidebar to \n"
"show, you can use the left and right arrow keys \n"
"to scroll through the text. Once you've chosen a \n"
"note, press ENTER to open it in the editor.";

#define VISCUE_WAIT 250000

typedef enum {SIDEBAR, EDITOR} focus_state;

void draw_sidebar(WINDOW *sidebar, int filesc, char *files[], int cursor_pos, int shift, int show, int scroll) {
	const int width = getmaxx(sidebar)-1;
	const int height = getmaxy(sidebar);
	if (show) {
		int limit;
		if (filesc - shift < height) limit = filesc - shift;
		else limit = height;
		for (int i = 0; i < limit; i++) {
			if (i == cursor_pos + shift) {
				wattron(sidebar, A_REVERSE);
				mvwaddnstr(sidebar, i, 0, files[i + shift] + scroll, width);
				wattroff(sidebar, A_REVERSE);
			} else mvwaddnstr(sidebar, i, 0, files[i + shift], width);
		}
	} else wclear(sidebar);
	mvwvline(sidebar, 0, width, 0, height);
	wrefresh(sidebar);
}

WINDOW *init_sidebar() {
	const double width = (double) getmaxx(stdscr);
	const int sidebar_width = (int) (width - width / phi);
	WINDOW *sidebar = newwin(getmaxy(stdscr)-2, sidebar_width, 0, 0);
	refresh();
	keypad(sidebar, TRUE);
	return sidebar;
}

void draw_editor(WINDOW *editor, char *content, int cursor_y, int cursor_x, int top_row, int left_col, int show) {
	wclear(editor);
	char *buf = strdup(content);
	char *tok;
    for (int i = 0; i < top_row; i++) {tok = strsep(&buf, "\n");} // skip unprinted rows
	int i = 0;
	while ((tok = strsep(&buf, "\n")) != NULL) {
		if (strlen(tok)-1 < left_col) ;
		else if (*tok == '\0') ;
		else {
			char *row = tok + left_col;
			waddnstr(editor, row, getmaxx(editor));
		}
		waddch(editor, '\n');
	}
	curs_set(show ? 2 : 0);
	wmove(editor, cursor_y, cursor_x);
	wrefresh(editor);
}

WINDOW *init_editor() {
	const double width = (double) getmaxx(stdscr);
	const int editor_width = (int) width / phi;
	WINDOW *editor = newwin(getmaxy(stdscr)-2, editor_width, 0, width - editor_width);
	refresh();
	keypad(editor, TRUE);
	return editor;
}

char *insert_ch(char *str, char ch, int line, int col) {
	char *buf = malloc(strlen(str) + 2);
	int i = 0;
	int current_line = 0;
	while (current_line < line) {
		if (*(str + i) == '\n') current_line++;
		i++;
	}
	i += col;
	strncpy(buf, str, i);
	*(buf + i) = ch;
	strcpy(buf+i+1, str+i);
	return buf;
}

char *insert_ch_pos(char *str, char ch, int pos) {
	char *buf = malloc(strlen(str) + 2);
	strncpy(buf, str, pos);
	*(buf + pos) = ch;
	strcpy(buf+pos+1, str+pos);
	return buf;
}

char *delete_ch(char *str, int line, int col) {
	if (line + col == 0) return str;
	char *buf = malloc(strlen(str));
	int i = 0;
	int current_line = 0;
	while (current_line < line) {
		if (*(str + i) == '\n') current_line++;
		i++;
	}
	i += col;
	strncpy(buf, str, i);
	strcpy(buf+i-1,str+i);
	return buf;
}

char *delete_ch_pos(char *str, int pos) {
	if (pos == 0) return str;
	char *buf = malloc(strlen(str));
	strncpy(buf, str, pos);
	strcpy(buf+pos-1,str+pos);
	return buf;
}

int count_ch(char *str, char ch) {
	int ret = 0;
	for (int i = 0; i < strlen(str); i++) {
		if (*(str + i) == ch) ret++;
	}
	return ret;
}

int pos_to_index(char *str, int line, int col) {
	int i = -1;
	int current_line = 0;
	while (current_line < line) {
		i++;
		if (*(str + i) == '\n') current_line++;
	}
	i += col;
	return i;
}

typedef struct {
	int y;
	int x;
} position;

position index_to_pos(char *str, int index) {
	int line, col;
	line = col = 0;
	for (int i = 0; i <= index; i++) {
		if (*(str + i) == '\n') {
			line++;
			col = 0;
		} else col++;
	}
	return (position) {line, col};
}

void overwrite_file(char *path, char *content) {
	truncate(path, strlen(content));
	FILE *file = fopen(path, "w");
	fputs(content,file);
	fflush(file);
	fclose(file);
}

char *read_file(char *path) {
	FILE *file = fopen(path, "r");
	// Allocate buffer
	fseek(file, 0L, SEEK_END);
	long len = ftell(file);
	rewind(file);
	char *buf = malloc(len+1);
	for (long i = 0; i < len; i++) {
		*(buf + i) = fgetc(file);
	}
	*(buf + len) = '\0';
	return buf;
}

char *pathcat(char *start, char *end) {
	char *buf = malloc(strlen(start) + strlen(end) + 1);
	strcpy(buf, start);
	strcat(buf, end);
	return buf;
}

WINDOW *init_options() {
	WINDOW *opts = newwin(2, getmaxx(stdscr), getmaxy(stdscr)-2, 0);
	refresh();
	return opts;
}

// Concatenate keybind and description strings
char **keybind_strings(char *option_keybinds[], char *option_descriptions[], int optionsc) {
	char **ret = malloc(optionsc * sizeof(char*));
	for (int i = 0; i < optionsc; i++) {
		char *keybind = option_keybinds[i];
		char *desc = option_descriptions[i];
		char *full_string = malloc(strlen(keybind) + strlen(desc) + 2);
		strcpy(full_string, keybind);
		strcat(full_string, " ");
		strcat(full_string, desc);
		*(ret + i) = full_string;
	}
	return ret;
}

// Free array of strings
void free_string_array(char **strings, int stringsc) {
	for (int i = 0; i < stringsc; i++) {
		free(*(strings + i));
	}
	free(strings);
}

// GNU nano-style list of commands
int draw_options_row(WINDOW *options, char *option_keybinds[], char *option_descriptions[], int optionsc, int row) {
	curs_set(0);
	wclear(options);
	int content_len = 0;
	char **full_descriptions = keybind_strings(option_keybinds, option_descriptions, optionsc);
	for (int i = 0; i < optionsc; i++) {
		content_len += strlen(*(full_descriptions+i));
	}
	free_string_array(full_descriptions, optionsc);
	int space = (getmaxx(options) - content_len)/(optionsc+1);
	if (space < 1) return -1;
	wmove(options, row, 0);
	for (int i = 0; i < optionsc; i++) {
		wattron(options, A_REVERSE);
		mvwaddstr(options, row, getcurx(options)+space, option_keybinds[i]);
		wattroff(options, A_REVERSE);
		mvwaddstr(options, row, getcurx(options)+1, option_descriptions[i]);
	}
	return 0;
}

bool is_str_not_in_arr(char **strings, char *string, int stringsc) {
	for (int i = 0; i < stringsc; i++) {
		if (strcmp(*(strings + i), string) == 0) return false;
	}
	return true;
}

void print_row_scroll(char *str, int cursor_pos, int scroll, int width) {
	int x = getcurx(stdscr);
	addnstr(str + scroll, width);
	curs_set(2);
	move(getcury(stdscr), x + cursor_pos);
	refresh();
}

char **append_str(char **strings, char *string, int stringsc) {
	char **ret = realloc(strings, (stringsc + 1) * sizeof(char*));
	*(ret + stringsc) = string;
	return ret;
}

int main() {
	// initialize path
	chdir(getenv("HOME"));
	chdir(".terminote");
	// initialize files
	chdir("notes");
	DIR *d;
  	struct dirent *dir;
	start:
  	d = opendir(".");
	int filesc = 0;
    while ((dir = readdir(d)) != NULL) {
      	if (dir->d_type == DT_REG) { /* If the entry is a regular file */
        	filesc++;
    	}
    }
	if (filesc == 0) {
		FILE *default_note = fopen("note", "w");
		fputs(default_note_content, default_note);
		fflush(default_note);
		fclose(default_note);
		goto start;
	}
    rewinddir(d);
	char **files = malloc(filesc * sizeof(char*));
	int i = 0;
	while ((dir = readdir(d)) != NULL) {
    	if (dir->d_type == DT_REG) { /* If the entry is a regular file */
        	files[i++] = strdup(dir->d_name);
    	}
    }
	closedir(d);

	fputs("\x1B[2JScreen is too small", stdout);
    initscr();
	if (has_colors()) start_color();
	if (has_colors()) use_default_colors();
	if (has_colors()) init_pair(1, COLOR_RED, -1);
	clear();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	char *file_content = read_file(files[0]);
	char *note_name;
	int cursor_pos, shift, scroll, cursor_y, cursor_x, top_row, left_col;
	cursor_pos = shift = scroll = cursor_y = cursor_x = top_row = left_col = 0;
	WINDOW *sidebar = init_sidebar();
	WINDOW *editor = init_editor();
	WINDOW *options = init_options();

	focus_state focus = SIDEBAR;
	int ch, old_ch, choice, old_choice;
	old_ch = old_choice = 0;

	const int shortcuts = 3;
	char *shortcut_keys[shortcuts] = {"^A", "^R", "^X"};
	char *shortcut_descriptions[shortcuts] = {"Add note", "Rename note", "Exit terminote"};

	bool focus_change = false;

	while (1) {
		clear();
		if (focus_change) {
			focus_change = false;
			if (focus == EDITOR) shortcut_descriptions[1] = "Exits editor";
			else shortcut_descriptions[1] = "Exits terminote";
		}
		curs_set(0);
		if (draw_options_row(options, shortcut_keys, shortcut_descriptions, shortcuts, 1) != 0) {
			endwin();
		} else {
			mvwhline(options, 0, 0, 0, getmaxx(options));
			const double width = (double) getmaxx(stdscr);
			const int sidebar_width = (int) (width - width / phi);
			mvwaddch(options, 0, sidebar_width-1, '+');
			wrefresh(options);
			draw_sidebar(sidebar, filesc, files, cursor_pos, shift, 1, scroll);
			draw_editor(editor, file_content, cursor_y, cursor_x, top_row, left_col, 1);
		}

		if (focus == SIDEBAR) {
			ch = wgetch(sidebar);
			old_choice = cursor_pos + shift;
			if (ch == KEY_UP) {
				if (shift + cursor_pos == 0) ;
				else if (cursor_pos == 0) shift--;
				else cursor_pos--;
				if (shift + cursor_pos != 0) scroll = 0;
			} else if (ch == KEY_DOWN) {
				if (shift + cursor_pos == filesc-1) ;
				else if (cursor_pos == getmaxy(sidebar)) shift++;
				else cursor_pos++;
				if (shift + cursor_pos != filesc-1) scroll = 0;
			} else if (ch == '\n' || ch == KEY_ENTER) {
				focus = EDITOR;
				focus_change = true;
				// visual cue
				draw_editor(editor, "", cursor_y, cursor_x, top_row, left_col, 0);
				usleep(VISCUE_WAIT);
			} else if (ch == KEY_RESIZE) {
				endwin();
				refresh();
				clear();
				delwin(sidebar);
				delwin(editor);
				delwin(options);
				sidebar = init_sidebar();
				editor = init_editor();
				options = init_options();
			} else if (ch == 24) {
				endwin();
				return 0;
			} else if (ch == KEY_LEFT) {
				if (scroll > 0) scroll--;
			} else if (ch == KEY_RIGHT) {
				if (strlen(files[cursor_pos + shift] + scroll) > getmaxx(sidebar)-1) scroll++;
			}
			choice = cursor_pos + shift;
			if (choice != old_choice) {
				file_content = read_file(files[choice]);
			}
		} else if (focus == EDITOR) {
			ch = wgetch(sidebar);
			if (ch == '\n' || ch == KEY_ENTER) {
				file_content = insert_ch(file_content, '\n', cursor_y + top_row, cursor_x + left_col);
				if (cursor_y == getmaxy(editor)-1) top_row++;
				else cursor_y++;
				cursor_x = 0;
				left_col = 0;
			} else if (ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127) {
				int new_cursor_x = cursor_x;
				if (cursor_x + left_col == 0) {
					if (cursor_y + top_row != 0) {
						int pos = pos_to_index(file_content, cursor_y + top_row, 0);
						position behind_pos = index_to_pos(file_content, pos - 1);
						new_cursor_x = behind_pos.x == 0 ? 0 : behind_pos.x + 1;
						if (new_cursor_x > getmaxx(editor)-1) {
							left_col = new_cursor_x - new_cursor_x % getmaxx(editor);
							new_cursor_x = new_cursor_x % getmaxx(editor);
						}
						if (cursor_y == 0) top_row--;
						else cursor_y--;
					}
				} else if (cursor_x == 0) left_col--;
				else new_cursor_x--;

				file_content = delete_ch(file_content, cursor_y + top_row, cursor_x + left_col);

				cursor_x = new_cursor_x;
			} else if (ch == KEY_RESIZE) {
				endwin();
				refresh();
				clear();
				delwin(sidebar);
				delwin(editor);
				delwin(options);
				sidebar = init_sidebar();
				editor = init_editor();
				options = init_options();
			} else if (ch == '\t' || ch == KEY_STAB) {
					// Convert tabs to spaces because it's the easiest way to go about handling tabs
					for (int i = 0; i < TABSIZE; i++) {
						file_content = insert_ch(file_content, ' ', cursor_y + top_row, cursor_x + left_col);
						if (cursor_x != getmaxx(editor)-1) cursor_x++;
						else left_col++;
					}
			} else if (ch < KEY_MIN && old_ch != KEY_RESIZE && ch != 1 && ch != 18) {
				if (ch == 24) {
					focus = SIDEBAR;
					focus_change = true;
					overwrite_file(files[choice], file_content);
					cursor_y = cursor_x = top_row = left_col = 0;
					// visual cue
					draw_sidebar(sidebar, 0, NULL, 0, 0, 0, scroll);
					usleep(VISCUE_WAIT);
				} else {
					file_content = insert_ch(file_content, ch, cursor_y + top_row, cursor_x + left_col);
					if (cursor_x != getmaxx(editor)-1) cursor_x++;
					else left_col++;
				}
			} else if (ch == KEY_LEFT) {
				if (cursor_x + left_col == 0) {
					if (cursor_y + top_row != 0) {
						int pos = pos_to_index(file_content, cursor_y + top_row, 0);
						position behind_pos = index_to_pos(file_content, pos - 1);
						cursor_x = behind_pos.x == 0 ? 0 : behind_pos.x + 1;
						if (cursor_x > getmaxx(editor)-1) {
							left_col = cursor_x - cursor_x % getmaxx(editor);
							cursor_x = cursor_x % getmaxx(editor);
						}
						if (cursor_y == 0) top_row--;
						else cursor_y--;
					}
				} else if (cursor_x == 0) left_col--;
				else cursor_x--;
			} else if (ch == KEY_RIGHT) {
				if (pos_to_index(file_content, cursor_y + top_row, left_col + cursor_x) == strlen(file_content)-1) ;
				else if (*(file_content + pos_to_index(file_content, cursor_y + top_row, left_col + cursor_x) + 1) == '\n') {
					if (cursor_y == getmaxy(editor)-1) top_row++;
					else cursor_y++;
					cursor_x = 0;
					left_col = 0;
				}
				else if (cursor_x == getmaxx(editor)-1) left_col++;
				else cursor_x++;
			}
		}
		// Add note
		if (ch == 1) {
			clear();
			delwin(sidebar);
			delwin(editor);
			delwin(options);
			refresh();
			note_name = strdup("");
			int add_pos = 0;
			int add_scroll = 0;
			int old_add_ch = 0;
			bool is_invalid = false;
			while (1) {
				if (getmaxx(stdscr) < 20 || getmaxy(stdscr) < 4) {
					endwin();
				} else {
					clear();
					refresh();
					attron(A_REVERSE);
					mvaddstr(3, 0, "^X");
					attroff(A_REVERSE);
					addstr(" Exit prompt");
					if (is_invalid) {
						if (has_colors()) attron(COLOR_PAIR(1));
						mvaddstr(1, 0, "Note already exists");
						if (has_colors()) attroff(COLOR_PAIR(1));
					}
					mvaddstr(0, 0, "Note name: ");
					int row_width = getmaxx(stdscr)-12;
					print_row_scroll(note_name, add_pos, add_scroll, row_width);
					int add_ch = getch();
					is_invalid = false;
					if (add_ch == '\n' || add_ch == KEY_ENTER) {
						if (is_str_not_in_arr(files, note_name, filesc)) {
							files = append_str(files, note_name, filesc);
							filesc++;
							fclose(fopen(note_name, "a"));
							break;
						}
					} else if (add_ch == KEY_LEFT) {
						if (add_scroll + add_pos != 0) {
							if (add_pos == 0) add_scroll--;
							else add_pos--;
						}
					} else if (add_ch == KEY_RIGHT) {
						if (add_scroll + add_pos != strlen(note_name)) {
							if (add_pos == row_width-1) add_scroll++;
							else add_pos++;
						}
					} else if (add_ch == KEY_BACKSPACE || add_ch == KEY_DC || add_ch == 127) {
						if (add_scroll + add_pos != 0) {
							note_name = delete_ch_pos(note_name, add_scroll + add_pos);
							if (add_pos == 0) add_scroll--;
							else add_pos--;
						}
					} else if (add_ch == 24) {
						break;
					} else if (add_ch < KEY_MIN && old_add_ch != KEY_RESIZE) {
						note_name = insert_ch_pos(note_name, add_ch, add_scroll + add_pos);
						if (add_pos == row_width-1) add_scroll++;
						else add_pos++;
					}
					old_add_ch = add_ch;
					if (!is_str_not_in_arr(files, note_name, filesc)) is_invalid = true;
				}
			}
			sidebar = init_sidebar();
			editor = init_editor();
			options = init_options();
		}
		// Rename note
		if (ch == 18) {
			clear();
			delwin(sidebar);
			delwin(editor);
			delwin(options);
			refresh();
			note_name = strdup("");
			int add_pos = 0;
			int add_scroll = 0;
			int old_add_ch = 0;
			enum {INVALID, EMPTY, NONE} error_state = NONE;
			while (1) {
				if (getmaxx(stdscr) < 20 || getmaxy(stdscr) < 4) {
					endwin();
				} else {
					clear();
					refresh();
					attron(A_REVERSE);
					mvaddstr(3, 0, "^X");
					attroff(A_REVERSE);
					addstr(" Exit prompt");
					if (error_state != NONE) {
						if (has_colors()) attron(COLOR_PAIR(1));
						if (error_state == INVALID) mvaddstr(1, 0, "Note already exists");
						if (error_state == EMPTY) mvaddstr(1, 0, "Name can't be empty");
						if (has_colors()) attroff(COLOR_PAIR(1));
					}
					mvaddstr(0, 0, "Note name: ");
					int row_width = getmaxx(stdscr)-12;
					print_row_scroll(note_name, add_pos, add_scroll, row_width);
					int add_ch = getch();
					error_state = NONE;
					if (add_ch == '\n' || add_ch == KEY_ENTER) {
						if (is_str_not_in_arr(files, note_name, filesc) && strlen(note_name) > 0) {
							rename(*(files + cursor_pos + shift), note_name);
							*(files + cursor_pos + shift) = note_name;
							break;
						}
					} else if (add_ch == KEY_LEFT) {
						if (add_scroll + add_pos != 0) {
							if (add_pos == 0) add_scroll--;
							else add_pos--;
						}
					} else if (add_ch == KEY_RIGHT) {
						if (add_scroll + add_pos != strlen(note_name)) {
							if (add_pos == row_width-1) add_scroll++;
							else add_pos++;
						}
					} else if (add_ch == KEY_BACKSPACE || add_ch == KEY_DC || add_ch == 127) {
						if (add_scroll + add_pos != 0) {
							note_name = delete_ch_pos(note_name, add_scroll + add_pos);
							if (add_pos == 0) add_scroll--;
							else add_pos--;
						}
					} else if (add_ch == 24) {
						break;
					} else if (add_ch < KEY_MIN && old_add_ch != KEY_RESIZE) {
						note_name = insert_ch_pos(note_name, add_ch, add_scroll + add_pos);
						if (add_pos == row_width-1) add_scroll++;
						else add_pos++;
					}
					old_add_ch = add_ch;
					if (!is_str_not_in_arr(files, note_name, filesc)) error_state = INVALID;
					if (strlen(note_name) == 0) error_state = EMPTY;
				}
			}
			sidebar = init_sidebar();
			editor = init_editor();
			options = init_options();
		}
		old_ch = ch;
	}
}