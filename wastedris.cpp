// wastedris.cpp
//
// This file contains the main function.
// It uses other sub-routine parts of the code.
// Basically, the main part just receives the user inputs and gives it to the core part.
// 

#include <iostream>
#include <unistd.h>
#include "noncanonical.hpp"
#include "game_core.hpp"

using namespace std;

// ============================================================================== //
// main
//
// Description:
//   The main function first sets up the environment to a non-canonical mode.
//   It then repeatedly interacts with the user while updating the state and display.
//   When the user tells to stop or the interaction is supposed to be end,
//   it breaks the loop.
//   At the end, it does the final task including deletion of the executable itself.
// 
// The condition to break the loop:
//   when the interaction is supposed to end, e.g., the game is over.
//   when the user gives Ctrl-D.
//
// ============================================================================== //
int main()
{
    int f_fail = set_input_mode();

    GAME* gm = GAME::init_game();

    while(gm->isRunning())
    {
        char c = readOneChar();
        gm->play_game(c);
        usleep(1000);
    }

    GAME::kill_game();

    return f_fail;
}


