// game_core.cpp
//
// This file contains the core of the game program.
// It maintains updates of the state and displaying the results based on the given character input.
//
// The object of the GAME class must be singleton, so its constructor/destructor are not public.
// To start a game, it is required to call the init_game method.
// The user also must call the end_game method to finish.
// But if the game is over, the object is automatically destroyed.
//

#include "game_core.hpp"
#include "format_macro.hpp"
#include <iostream>
#include <iomanip>

#include <thread>
#include <mutex>

#include <unistd.h> // usleep
#include <stdlib.h> // random number gen
#include <time.h> // random number gen

using namespace std;

// ================================================================================= //
// the pointer to the object is initialized with NULL.
// so the program can tell if there is an existing one.
// ================================================================================= //
GAME* GAME::game = NULL;

// ================================================================================= //
// Constructor
//
// It cleans up the screen for setup.
// It also initializes the graphical parameters for the game.
// Then, it initializes the random number generator.
// Then, it initializes the internal parameters.
// Then, it draws the background including boxes and cells.
// Finally, it prepares a thread to independently run the update function.
//
// ================================================================================= //
GAME::GAME()
{
    CLEAR_SCREEN();
    CURSOR_OFF();

    nrow = NROW_BIN;
    ncol = NCOL_BIN;

    bin_start_x = START_CELL_X;
    bin_start_y = START_CELL_Y;

    next_start_x = START_CELL_NBOX_X;
    next_start_y = START_CELL_NBOX_Y;
    next_width = 4*WCELL;
    next_height = 4*HCELL;

    screen_width = next_start_x + next_width + 1;
    screen_height = bin_start_y + nrow * HCELL + 3;

    mess_start_x = START_CELL_NBOX_X;
    mess_start_y = START_CELL_NBOX_Y + next_height + 1 + 1;
    mess_width = next_width;
    mess_height = bin_start_y + nrow*HCELL - mess_start_y;

    bin = new int*[nrow];
    canvas = new int*[nrow];
    shadow = new int*[nrow];
    for(int i = 0; i < nrow; i++)
    {
        bin[i] = new int[ncol];
        canvas[i] = new int[ncol];
        shadow[i] = new int[ncol];
    }

    srand(time(NULL));

    init_stat();
    
    draw_background();
    draw_cells();
    FLUSH();

    n_step = 100;
    i_step = 0;
    t_update = thread(&GAME::update,this);
}

// ================================================================================= //
// Destructor
//
// If the game is not over, the method tells it to stop, and plays the end movie.
// Then, the main thread waits for the update thread to join.
// Then, it releases the heap memory.
// Finally, it displays a message.
// ================================================================================= //
GAME::~GAME()
{
    if(f_stat != 0)
    {
        mtx.lock();
        f_stat = 0;
        mtx.unlock();
    }
    t_update.join();
    if(bin != NULL)
    {
        for(int i = 0; i < nrow; i++)
            delete[] bin[i];
        delete[] bin;
    }
    if(canvas != NULL)
    {
        for(int i = 0; i < nrow; i++)
            delete[] canvas[i];
        delete[] canvas;
    }
    if(shadow != NULL)
    {
        for(int i = 0; i < nrow; i++)
            delete[] shadow[i];
        delete[] shadow;
    }

    CLEAR_SCREEN();
    CURSOR_ON();
    MOVE_CURSOR(1,1);
    cout << endl << "           go back to work now" << endl << endl;
}

// ================================================================================= //
// init_game
//
// This function initializes the state of the game, and restarts the thread.
// 
// ================================================================================= //
GAME *GAME::init_game()
{
    if(game != NULL)
    {
        delete game;
    }
    game = new GAME();

    return game;
}

// ================================================================================= //
// end_movie
//
// This plays the end movie.
// ================================================================================= //
void GAME::play_endmovie()
{
    int width = screen_width;
    int height = screen_height;

    CHANGE_COLOR_BRED();
    char symbols[5] = {' ', '.', ';', '*', '#'};
    int thresholds[5] = {0, 90, 30, 5, 0};
    for(int i = height; i >= 1; i--)
    {
        for(int isym = 0; isym < 5; isym++)
        {
            if(i - isym >= 1)
            {
                DRAW_HLINE_C(i - isym, 1, 1, string(width, symbols[isym]));
                for(int icol = 1; icol <= width && thresholds[isym] != 0; icol++)
                {
                    int rand_v = rand()%100;
                    if(rand_v < thresholds[isym])
                    {
                        MOVE_CURSOR(icol,i-isym);
                        cout << ' ';
                    }
                }
            }
        }
        FLUSH();
        usleep(60000);
    }
    CHANGE_COLOR_DEF();
}

// ================================================================================= //
// kill_game
// This function stops the running game and kills the thread.
// Finally, it plays the end movie.
// 
// ================================================================================= //
void GAME::kill_game()
{
    if(game != NULL)
    {
        delete game;
        game = NULL;
    }
}

// ================================================================================= //
// init_stat
//
// initializes all internal parameters.
// also it clears the message box.
// ================================================================================= //
void GAME::init_stat()
{
    for(int i = 0; i < nrow; i++)
    {
        for(int j =0; j < ncol; j++)
        {
            bin[i][j] = 0;
            canvas[i][j] = 0;
            shadow[i][j] = 0;
        }
    }
    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            cur_piece[i][j] = 0;
            next_piece[i][j] = 0;
        }
    }

    count_clearing_rows = 0;
    f_stat = 1;
    rand_next();
    copy_pieces();
    rand_next();

    clear_message();
}

// ================================================================================= //
// abort()
//
// This aborts the game and plays the end movie.
// ================================================================================= //
void GAME::abort()
{
    f_stat = 0;
    play_endmovie();
}

// ================================================================================= //
// rand_next()
//
// It generates a randomized piece in the next box.
// It first generates a square shape. Then, it randomly changes the shape.
// After all, each shape appears at a chance of 1/7.
// Finally, it turns the generated piece randomly.
//
// *This method assumes the size of the piece is 4x4.
// 
// ================================================================================= //
void GAME::rand_next()
{
    int color = rand()%12;
    if(color < 6) color++;
    else color += 5;

    next_piece[0][0] = 0; next_piece[0][1] = 0; next_piece[0][2] = 0; next_piece[0][3] = 0;
    next_piece[1][0] = 0; next_piece[1][1] = color; next_piece[1][2] = color; next_piece[1][3] = 0;
    next_piece[2][0] = 0; next_piece[2][1] = color; next_piece[2][2] = color; next_piece[2][3] = 0;
    next_piece[3][0] = 0; next_piece[3][1] = 0; next_piece[3][2] = 0; next_piece[3][3] = 0;

    double p_val = (double)rand()/RAND_MAX;
    if(p_val < 3.0/7.0)
    {
        next_piece[1][1] = 0;
        next_piece[0][2] = color;
        p_val = (double)rand()/RAND_MAX;
        if(p_val < 1.0/6.0)
        {
            next_piece[2][1] = 0;
            next_piece[3][2] = color;
        }
        else if(p_val < 2.0/6.0)
        {
            next_piece[1][1] = color;
            next_piece[2][1] = 0;
        }
        else if(p_val < 2.0/3.0)
        {
            next_piece[0][2] = 0;
            next_piece[1][3] = color;
        }
    }
    else if(p_val < 6.0/7.0)
    {
        next_piece[2][1] = 0;
        next_piece[3][2] = color;
        p_val = (double)rand()/RAND_MAX;
        if(p_val < 1.0/6.0)
        {
            next_piece[1][1] = 0;
            next_piece[0][2] = color;
        }
        else if(p_val < 2.0/6.0)
        {
            next_piece[1][1] = 0;
            next_piece[2][1] = color;
        }
        else if(p_val < 2.0/3.0)
        {
            next_piece[3][2] = 0;
            next_piece[2][3] = color;
        }
    }

    p_val = (double)rand()/RAND_MAX;
    if(p_val < 1.0/4.0)
        rotL_piece(next_piece);
    else if(p_val < 2.0/4.0)
        rotR_piece(next_piece);
    else if(p_val < 3.0/4.0)
    {
        rotL_piece(next_piece);
        rotL_piece(next_piece);
    }
}

// ================================================================================= //
// copy_pieces()
//
// It copies a piece in the next box to the current box.
// Then, it initializes the curr piece's location.
// ================================================================================= //
void GAME::copy_pieces()
{
    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            cur_piece[i][j] = next_piece[i][j];
        }
    }
    cur_p_x = (ncol - NCOL_PIECE)/2;
    cur_p_y = -1*NROW_PIECE;
}

// ================================================================================= //
// rotR_piece
//
// This rotates a piece clockwise.
// ================================================================================= //
void GAME::rotR_piece(int (*piece)[NCOL_PIECE])
{
    int buff[NROW_PIECE][NCOL_PIECE];

    for(int i = 0; i < NROW_PIECE; i++)
        for(int j = 0; j < NCOL_PIECE; j++)
            buff[j][NCOL_PIECE-1-i] = piece[i][j];
    for(int i = 0; i < NROW_PIECE; i++)
        for(int j = 0; j < NCOL_PIECE; j++)
            piece[i][j] = buff[i][j];
}

// ================================================================================= //
// rotL_piece
//
// This rotates a piece anti-clockwise.
// ================================================================================= //
void GAME::rotL_piece(int (*piece)[NCOL_PIECE])
{
    int buff[NROW_PIECE][NCOL_PIECE];

    for(int i = 0; i < NROW_PIECE; i++)
        for(int j = 0; j < NCOL_PIECE; j++)
            buff[NROW_PIECE-1-j][i] = piece[i][j];
    for(int i = 0; i < NROW_PIECE; i++)
        for(int j = 0; j < NCOL_PIECE; j++)
            piece[i][j] = buff[i][j];
}

// ================================================================================= //
// play_game
//
// This function updates the state of the game based on the given character.
//
// input:
//   char c: a character provided by the user
// output:
//   the state to continue the game
//       1: running
//       0: stopped
//       others: error or the game is not running correctly
//       
// ================================================================================= //
int GAME::play_game(char c)
{
    mtx.lock();

    if(c == '\x04')
    {
        abort();
    }
    else if(c == 'C') // right arrow 
    {
        if(isMovable(1,0))
            cur_p_x++;
    }
    else if(c == 'D') // left arrow
    {
        if(isMovable(-1,0))
            cur_p_x--;
    }
    else if(c == 'B') // down arrow
    {
        if(isMovable(0,1))
            cur_p_y++;
    }
    else if(c == ' ' || c == 'x') // for clockwise rotation
    {
        if(isRotatable(true))
            rotR_piece(cur_piece);
    }
    else if(c == 'z') // for anti-clockwise rotation
    {
        if(isRotatable(false))
            rotL_piece(cur_piece);
    }
    draw_cells();

    mtx.unlock();

    return f_stat;
}

// ================================================================================= //
// draw_background
//
// This method draws the background including the bin and the next box.
//
// ================================================================================= //
void GAME::draw_background()
{
    CLEAR_SCREEN();
    CHANGE_COLOR_CYAN();
    DRAW_RECT(bin_start_x-1, bin_start_y-1, bin_start_x+WCELL*ncol, bin_start_y+HCELL*nrow);
    DRAW_RECT(next_start_x-1, next_start_y-1, next_start_x+next_width, next_start_y+next_height);
    MOVE_CURSOR(next_start_x+WCELL*2-2, next_start_y-1);
    cout << "NEXT";
    DRAW_RECT(mess_start_x-1, mess_start_y-1, mess_start_x+mess_width, mess_start_y+mess_height);
    CHANGE_COLOR_DEF();
    FLUSH();
}

// ================================================================================= //
// draw_cells
//
// It draws all cells in the bin and the next box.
//
// For the bin, it remembers which color is stored for each cell.
// Only if a cell is to be changed in color, it draws the cell.
//
// To draw the bin, it first generates a table of color information for each cell.
// Then, the table is compared with the old one.
// Only if different, the cell is redrawn.
//
// color index:
//   1: red       11: bright red
//   2: green     12: bright green 
//   3: yellow    13: bright yellow
//   4: blue      14: bright blue
//   5: magenta   15: bright magenta
//   6: cyan      16: bright cyan
//   7: white     17: bright white
// ================================================================================= //
void GAME::draw_cells()
{
    for(int i = 0; i < ncol; i++)
    {
        for(int j = 0; j < nrow; j++)
        {
            if(bin[j][i]==0)
            {
                canvas[j][i] = 0;
            }
            else
            {
                canvas[j][i] = bin[j][i];
            }
        }
    }
    for(int i = 0; i < NCOL_PIECE; i++)
    {
        for(int j = 0; j < NROW_PIECE; j++)
        {
            if(cur_piece[j][i]>0)
            {
                if(0<=cur_p_x+i&&cur_p_x+i<ncol&&0<=cur_p_y+j&&cur_p_y+j<nrow)
                    canvas[cur_p_y+j][cur_p_x+i] = cur_piece[j][i];
            }
        }
    }
    CHANGE_COLOR_GREEN();
    for(int i = 0; i < ncol; i++)
    {
        for(int j = 0; j < nrow; j++)
        {
            if(canvas[j][i] != shadow[j][i])
            {
                PUT_CELL_COLOR(i,j,canvas[j][i]);
                shadow[j][i] = canvas[j][i];
            }
        }
    }
    for(int i = 0; i < NCOL_PIECE; i++)
    {
        for(int j = 0; j < NROW_PIECE; j++)
        {
            if(next_piece[j][i]==0)
            {
                DEL_CELL_NBOX(i,j);
            }
            else
            {
                CHANGE_COLOR(next_piece[j][i]);
                PUT_CELL_NBOX(i,j);
            }
        }
    }
    CHANGE_COLOR_DEF();
    FLUSH();
}

// ================================================================================= //
// put_message()
//
// put a message in the message box
// ================================================================================= //
void GAME::put_message()
{
    CHANGE_COLOR_MAGENTA();
    if(count_clearing_rows == 1)
    {
        MOVE_CURSOR(mess_start_x+1,mess_start_y+1);
        cout << "YOU WASTED";
        MOVE_CURSOR(mess_start_x+1,mess_start_y+2);
        cout << "YOUR TIME";
    }
    else if(count_clearing_rows == 2)
    {
        MOVE_CURSOR(mess_start_x+1,mess_start_y+3);
        cout << "AGAIN";
    }
    else if(count_clearing_rows > 2)
    {
        MOVE_CURSOR(mess_start_x+1,mess_start_y+3);
        cout << count_clearing_rows << " TIMES";
    }
    if(count_clearing_rows > 10)
    {
        MOVE_CURSOR(mess_start_x+1,mess_start_y+5);
        cout << "It's time";
        MOVE_CURSOR(mess_start_x+1,mess_start_y+6);
        cout << "to regret";
    }
    CHANGE_COLOR_DEF();
    FLUSH();
}

// ================================================================================= //
// clear_message
//
// It clears everything in the message box
// ================================================================================= //
void GAME::clear_message()
{
    for(int y = mess_start_y; y < mess_start_y + mess_height; y++)
    {
        MOVE_CURSOR(mess_start_x,y);
        cout << string(mess_width,' ');
    }
}

// ================================================================================= //
// update
//
// This method takes one step to update the game status controlling mutex.
//
// It checks if the current piece can fall by one cell.
// If so, just let it go.
// Otherwise, it checks if the current piece is off the area of the bin.
// If so, the game is over.
// Otherwise, it places the current piece in the bin, and evaluates the game.
// 
// ================================================================================= //
void GAME::update()
{
    while(isRunning())
    {
        
        mtx.lock();
        if(i_step == 0)
        {
            if(isMovable(0,1))
            {
                cur_p_y++; 
                draw_cells();
            }
            else if(cur_p_y < 0)
            {
                f_stat = 0;
                CHANGE_COLOR_BRED();
                MOVE_CURSOR(screen_width/2-6,screen_height/2-2);
                cout << "#############";
                MOVE_CURSOR(screen_width/2-6,screen_height/2-1);
                cout << "#           #";
                MOVE_CURSOR(screen_width/2-6,screen_height/2);
                cout << "# GAME OVER #";
                MOVE_CURSOR(screen_width/2-6,screen_height/2+1);
                cout << "#           #";
                MOVE_CURSOR(screen_width/2-6,screen_height/2+2);
                cout << "#############";
                MOVE_CURSOR(screen_width/2-4,screen_height/2);
                CHANGE_COLOR_DEF();
                play_endmovie();
                MOVE_CURSOR(1,1);
                cout << "press any button." << endl;
            }
            else
            {
                placePiece();
                copy_pieces();
                rand_next();
                eval_and_clean();
            }
            put_message();
        }
        FLUSH();
        i_step = (i_step + 1) % n_step;
        mtx.unlock();

        usleep(5000);
    }
    FLUSH();
}

// ================================================================================= //
// eval_and_clean
//
// it checks if there are full rows and remove them.
// ================================================================================= //
void GAME::eval_and_clean()
{
    bool f_cleared_at_least_one = false;
    for(int irow_search = nrow-1; irow_search >= 0;)
    {
        bool f_full = true;
        for(int icol = 0; icol < ncol; icol++)
            if(bin[irow_search][icol]==0)
                f_full = false;
        if(f_full)
        {
            for(int irow_clean = irow_search; irow_clean > 0; irow_clean--)
            {
                for(int icol = 0; icol < ncol; icol++)
                    bin[irow_clean][icol] = bin[irow_clean-1][icol];
            }
            f_cleared_at_least_one = true;
        }
        else
        {
            irow_search --;
        }
    }
    if(f_cleared_at_least_one)
        count_clearing_rows++;
}

// ================================================================================= //
// isMovable
//
// it returns true if the current piece can move by the specified values.
// otherwise, it returns false.
//
// the piece cannot move into the existing cells, the walls, and the floor.
// the method does not care about the ceiling.
// ================================================================================= //
bool GAME::isMovable(int dx, int dy)
{
    int proposed_x = cur_p_x + dx;
    int proposed_y = cur_p_y + dy;
    bool f_movable = true;

    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            int x = proposed_x+j;
            int y = proposed_y+i;
            if(cur_piece[i][j]!=0)
            {
                if(0<=x && x<ncol && 0<=y && y<nrow && bin[y][x]!=0)
                    f_movable = false;
                if(x < 0 || ncol <= x || nrow <= y)
                    f_movable = false;
            }
        }
    }

    return f_movable;
}

// ================================================================================= //
// isRotatable
//
// It tells if the current piece can be rotated.
// The argument indicates the rotation is clockwise or not.
// ================================================================================= //
bool GAME::isRotatable(bool clockwise)
{
    int buff[NROW_PIECE][NCOL_PIECE];
    bool f_rotatable = true;
    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            if(clockwise)
                buff[j][NCOL_PIECE-1-i] = cur_piece[i][j];
            else
                buff[NROW_PIECE-1-j][i] = cur_piece[i][j];
        }
    }

    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            int x = cur_p_x+j;
            int y = cur_p_y+i;
            if(buff[i][j]!=0)
            {
                if(0<=x && x<ncol && 0<=y && y<nrow && bin[y][x]!=0)
                    f_rotatable = false;
                if(x < 0 || ncol <= x || nrow <= y)
                    f_rotatable = false;
            }
        }
    }

    return f_rotatable;
}

// ================================================================================= //
// placePiece
//
// It places the current piece into the bin.
// ================================================================================= //
void GAME::placePiece()
{
    for(int i = 0; i < NROW_PIECE; i++)
    {
        for(int j = 0; j < NCOL_PIECE; j++)
        {
            if(cur_piece[i][j] != 0)
                bin[cur_p_y+i][cur_p_x+j] = cur_piece[i][j];
        }
    }
}

// ================================================================================= //
// isRunning
//
// it tells if the game is running or not
// f_stat == 1 indicates it is running. Otherwise, it stops for some reason.
// 
// ================================================================================= //
int GAME::isRunning()
{
    return (f_stat == 1)? 1: 0;
}

