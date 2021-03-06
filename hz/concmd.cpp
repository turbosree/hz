// HZ Engine
// Copyright (C) 1998 David W. Jeske

/*
 * ConCmd.cpp
 *
 * Handle console command processing
 */
#include <lua.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "net.h"
#include "view.h" // ConsoleView

#include "concmd.h"


/*
 * command processing 
 */

int cmd_print_help(char *, ConsoleView *);
int cmd_list_netif(char *, ConsoleView *);
int cmd_connect_netif(char *, ConsoleView *);
int cmd_join_dplay_game(char *,ConsoleView *);
int cmd_create_dplay_game(char *, ConsoleView *);
int cmd_enumerate_dplay_games(char *str, ConsoleView *myConsole);
int cmd_enumerate_dplay_players(char *str, ConsoleView *myConsole);
int cmd_new_server(char *, ConsoleView *);
int cmd_toggle_console_output(char *, ConsoleView *);
int cmd_add_text(char *, ConsoleView *);
int cmd_print_con(char *, ConsoleView *);

struct command_str {
	char *command;
	char *description;
	int (*fn)(char *, ConsoleView *);
} commands [] = {
	{ "QUIT" , "Quit the game.. (hit ESC)", NULL},
	{ "HI", "Hello...", NULL},
	{ "HELP", "List available commands", cmd_print_help},


	{ "ADDT" , "Add text to console", cmd_add_text},
	{ "PCON" , "Print the Console", cmd_print_con},
	{ "T", "Toggle console view output on or off", cmd_toggle_console_output},
	{ 0, 0}};


int cmd_add_text(char *str, ConsoleView *my_view) {
	my_view->conTest->addString(str);
	my_view->conTest->addChar('\n');
	return 0;
}

int cmd_print_con(char *str, ConsoleView *my_view) {
		int line_num;
	char s[80];

	if ((line_num = atoi(str)) < 1) {
		/* print all! */
		int x;
		int numLines = my_view->conTest->numLines();

		sprintf(s,"numLines() = %d",numLines);
		my_view->addText(s);
		
		for (x=numLines;x>0;x--) {
			my_view->conTest->getLineReverse(s,80,x);
			my_view->addText(s);
		}
		return 0;

	}

	if (my_view->conTest->getLineReverse(s,80,line_num) < 0) {
		my_view->addString("<null ptr>\n");
	} else {
		my_view->addString(s);
		my_view->addChar('\n');
	}
	return 0;
}

int cmd_toggle_console_output(char *str, ConsoleView *my_view) {
	if (my_view->consoleViewDisabled) {
		my_view->consoleViewDisabled = 0;
		my_view->addText("View Enabled..");
	} else {
		my_view->addText("View Disabled... type 'T' to re-enable");
		my_view->consoleViewDisabled = 1;
	}
	return 0;
}

const char *help_text[] = {
  " \n",
    " Welcome to HZ!\n",
    " \n",
    " In the command console you can type any command listed\n",
    " above. You can also type any Lua script command. To find\n",
    " a list of available Lua functions and variables type: \n",
    " \n",
    "     > dir()\n",
    " \n",
    " You can also scroll up/down by using the pageup/down keys.\n",
    " \n",
    NULL };

int cmd_print_help(char *, ConsoleView *myConsole) {
	struct command_str *walker = commands;
	char s[100];

	while (walker->command) {
		sprintf(s,"% -10s - %s",walker->command,walker->description);
		myConsole->addText(s);
		walker++;
	}

	const char **my_help_text = help_text;
	while (*my_help_text) {
	  myConsole->addText(*my_help_text);
	  my_help_text++;
	}

	return 0;
}



int strncmp_ic(char *st1, char *st2, int len) {
	int trylen = len;

	while (trylen && st1 && st2 && (toupper(*st1) == toupper(*st2))) {
		trylen--;
		st1++;
		st2++;
	}

	return (trylen);

};


int handleConsoleInput(ConsoleView *receiver, char *input) {
  struct command_str *walker = commands;

  // this is because old habits die hard and I used to require 
  // Lua commands be prepended with the single-quote character.
  if (input[0] == '\'') {
    input++;
  }
  
  while (walker->command) {
    if (!strncmp_ic(walker->command,input,strlen(walker->command))) {
				// matched!
      if (walker->fn) {
	walker->fn(input + strlen(walker->command),receiver);
      }
      return 1;
    } 
    walker++;
  };

  // Pass the string to lua!!!
  lua_beginblock();
  int lerror = lua_dostring(input);
  lua_endblock();

  if (lerror) {
    receiver->addText("[Invalid command, type HELP]");
  }

  return 1;
}
