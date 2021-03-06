//============================================================================
// Name        : PlayoutState.cpp
// Author      : Jan Gutter
// Copyright   : To the extent possible under law, Jan Gutter has waived all
//             : copyright and related or neighboring rights to this work.
//             : For more information, go to:
//             : http://creativecommons.org/publicdomain/zero/1.0/
//             : or consult the README and COPYING files
// Description : Simulates a game state
//============================================================================

#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include "PlayoutState.h"
#include "SFMT.h"
#include "MCTree.h"
#include <string.h>
#include <limits.h>
#include <fstream>

#define ASSERT 0
#define DEBUG 0
#define DEBUGOBSTACLES 0

using namespace std;

void PlayoutState::drawTanks()
{
	int t;
	for (t = 0; t < 4; t++) {
		if (tank[t].active) {
			drawTank(t, B_TANK);
		}
	}
}

void PlayoutState::drawTinyTanks()
{
	int t;
	for (t = 0; t < 4; t++) {
		if (tank[t].active) {
			drawTinyTank(t, B_TANK);
		}
	}
}

void PlayoutState::drawBases()
{
	int i;
	for (i = 0; i < 2; i++) {
		board[base[i].x][base[i].y] = B_BASE;
	}
}

void PlayoutState::drawBullets()
{
	int i;
	for (i = 0; i < 4; i++) {
		if (bullet[i].active) {
			board[bullet[i].x][bullet[i].y] = B_LOOKUP(bullet[i].o);
		}
	}
}

bool PlayoutState::insideBounds(const int x, const int y)
{
	return (x >= min_x && y >= min_y && x < max_x && y < max_y);
}

bool PlayoutState::isTankInsideBounds(const int x, const int y)
{
	return (x > (min_x+1) && y > (min_y+1) && x < (max_x-2) && y < (max_y-2));
}

bool PlayoutState::canRotate(const int x, const int y, const int o, board_t& obstacles)
{
	bool clear = true;
	int i,x_bump,y_bump;
	for (i = 0; clear && (i < 5); i++) {
		x_bump = x+MOVEPATH_LOOKUP(o,i,O_X);
		y_bump = y+MOVEPATH_LOOKUP(o,i,O_Y);
		clear = insideBounds(x_bump,y_bump) &&
				(obstacles[x_bump][y_bump] & (B_WALL|B_OOB)) == 0;
	}
	return !clear;
}

bool PlayoutState::insideTank(const unsigned int t, const int x, const int y)
{
	//	return (tank[t].active && (abs(tank[t].x - x) < 3) && (abs(tank[t].y - y) < 3));
	return tank[t].active && (x < tank[t].x+3) && (x > tank[t].x-3) &&
			(y < tank[t].y+3) && (y > tank[t].y-3);
}

bool PlayoutState::insideAnyTank(const int x, const int y)
{
	int i;
	bool inside = false;
	for (i = 0; i < 4 && inside; i++) {
		inside = insideTank(i,x,y);
	}
	return inside;
}

bool PlayoutState::insideTinyTank(const int t, const int x, const int y)
{
	//	return (tank[t].active && (abs(tank[t].x - x) < 2) && (abs(tank[t].y - y) < 2));
	return tank[t].active && x < tank[t].x+2 && x > tank[t].x+2 && y < tank[t].y+2 && y > tank[t].y-2;
}

bool PlayoutState::isTankAt(const int t, const int x, const int y)
{
	return (tank[t].active && tank[t].x == x && tank[t].y == y);
}

void PlayoutState::drawTank(const int t, const int block)
{
	int i,j;
	for (i = tank[t].x-2; i < tank[t].x+3; i++) {
		for (j = tank[t].y-2; j < tank[t].y+3; j++) {
			board[i][j] = block;
		}
	}
}

void PlayoutState::drawTankObstacle(const int t, board_t& obstacles)
{
	int i,j;
	for (i = tank[t].x-2; i < tank[t].x+3; i++) {
		for (j = tank[t].y-2; j < tank[t].y+3; j++) {
			obstacles[i][j] = B_OOB;
		}
	}
}

void PlayoutState::drawTankObstacle(const int x, const int y, board_t& obstacles)
{
	int i,j;
	for (i = x-2; i < x+3; i++) {
		for (j = y-2; j < y+3; j++) {
			obstacles[i][j] = B_OOB;
		}
	}
}

void PlayoutState::drawTinyTank(const int t, const int block)
{
	int i,j;
	for (i = tank[t].x-1; i < tank[t].x+2; i++) {
		for (j = tank[t].y-1; j < tank[t].y+2; j++) {
			board[i][j] = block;
		}
	}
}

void PlayoutState::moveBullets()
{
	int i;
	for (i = 0; i < 4; i++) {
		if (bullet[i].active) {
			board[bullet[i].x][bullet[i].y] ^= B_LOOKUP(bullet[i].o);
			bullet[i].x += O_LOOKUP(bullet[i].o,O_X);
			bullet[i].y += O_LOOKUP(bullet[i].o,O_Y);
			if (insideBounds(bullet[i].x,bullet[i].y)) {
				board[bullet[i].x][bullet[i].y] |= B_LOOKUP(bullet[i].o);
			} else {
				bullet[i].active = 0;
			}
		}
	}
	//NOTE: MUST RESOLVE COLLISIONS AFTER THIS!
}

void PlayoutState::moveTanks()
{
	int i,j,t,c,x,y,square;
	int newx,newy;
	bool clear;
#if ASSERT
	bool fine = true;
	bool exists[4];
	for (i = 0; i < 4; i++) {
		exists[i] = false;
	}
	for (i = 0; i < 4; i++) {
		if (tank_priority[i] >= 0 && tank_priority[i] < 4) {
			exists[tank_priority[i]] = true;
		} else {
			cerr << "Tank priority is screwed!" << endl;
		}
	}
	for (i = 0; i < 4; i++) {
		fine = fine && exists[i];
	}
	if (!fine) {
		cerr << "Some tank priorities missing!" << endl;
		cerr << "Priorities: ";
		for (i = 0; i < 4; i++) {
			cerr << tank_priority[i] << " ";
		}
		cerr << endl;
	}
#endif
	for (i = 0; i < 4; i++) {
		t = tank_priority[i];
		c = command[t];
		if (tank[t].active && C_ISMOVE(c)) {
			tank[t].o = C_TO_O(c);
			x = tank[t].x;
			y = tank[t].y;
			newx = x + O_LOOKUP(tank[t].o,O_X);
			newy = y + O_LOOKUP(tank[t].o,O_Y);
			if (isTankInsideBounds(newx,newy)) {
				clear = true;
				for (j = 0; j < 5; j++) {
					square = board[x+BUMP_LOOKUP(tank[t].o,j,O_X)][y+BUMP_LOOKUP(tank[t].o,j,O_Y)];
					clear = clear && B_ISCLEAR(square);
				}
				//TODO: Check for win by ramming? base
				if (clear) {
					//Move tank
					tank[t].x = newx;
					tank[t].y = newy;
					//Set new points and unset old ones
					for (j = 0; j < 5; j++) {
						board[x+BUMP_LOOKUP(tank[t].o,j,O_X)][y+BUMP_LOOKUP(tank[t].o,j,O_Y)] |= B_TANK;
						board[newx-BUMP_LOOKUP(tank[t].o,j,O_X)][newy-BUMP_LOOKUP(tank[t].o,j,O_Y)] ^= B_TANK;
					}
				}
			} else {
				tank[t].active = 0;
			}
		}
	}
	//NOTE: MUST RESOLVE COLLISIONS AFTER THIS!
}

void PlayoutState::fireTanks()
{
	int i;
	for (i = 0; i < 4; i++) {
		if ((command[i] == C_FIRE) && (tank[i].active) && (!bullet[i].active)) {
			bullet[i].x = tank[i].x + FIRE_LOOKUP(tank[i].o,O_X);
			bullet[i].y = tank[i].y + FIRE_LOOKUP(tank[i].o,O_Y);
			if (insideBounds(bullet[i].x,bullet[i].y)) {
				bullet[i].active = 1;
				bullet[i].o = tank[i].o;
				board[bullet[i].x][bullet[i].y] |= B_LOOKUP(tank[i].o);
			}
		}
	}
	//NOTE: MUST RESOLVE COLLISIONS AFTER THIS!
}

void PlayoutState::checkCollisions()
{
	unsigned int j,square,other_bullet,x,y,prevsquare;
	int i;
	bool baseok[2];
	baseok[0] = true;
	baseok[1] = true;
	int active_tanks[2];

	for (i = 0; i < 4; i++) {
		bullet[i].tag = 0;
	}
	for (i = 0; i < 4; i++) {
		if (isTankInsideBounds(tank[i].x,tank[i].y)) {
			tank[i].tag = 0;
		} else {
			tank[i].tag = 1;
		}
	}

	for (j = 0; j < 2; j++) {
		baseok[j] = true;
		for (i = 0; i < 4; i++) {
			if (insideTank(i,base[j].x,base[j].y)) {
				baseok[j] = false;
			}
		}
	}

	if (!baseok[PLAYER0] && !baseok[PLAYER1]) {
		winner = W_DRAW;
		gameover = true;
		state_score = winner;
		stop_playout = true;
		return;
	} else if (baseok[PLAYER0] && !baseok[PLAYER1]) {
		winner = W_PLAYER0;
		gameover = true;
		state_score = winner;
		stop_playout = true;
		return;
	} else if (baseok[PLAYER1] && !baseok[PLAYER0]) {
		winner = W_PLAYER1;
		gameover = true;
		state_score = winner;
		stop_playout = true;
		return;
	}

	for (i = 0; i < 4; i++) {
		if (bullet[i].active) {
			prevsquare = board[bullet[i].x - O_LOOKUP(bullet[i].o,O_X)][bullet[i].y - O_LOOKUP(bullet[i].o,O_Y)];
			//If the bullet passed another one in the opposite direction, it's tagged for removal;
			//EXCEPT if the previous square contains the tank that fired it.
			bullet[i].tag = !(prevsquare & B_TANK) && (prevsquare & B_OPPOSITE(bullet[i].o));
			other_bullet = ((board[bullet[i].x][bullet[i].y] & (B_BULLET)) != B_LOOKUP(bullet[i].o));
			square = board[bullet[i].x][bullet[i].y] & (B_DESTRUCTABLES);
			switch (square) {
			case B_WALL:
				//Apply splash damage to B_WALL squares ONLY
				for (j = 0; j < 4;j++) {
					x = bullet[i].x + B_SPLASH(bullet[i].o,j,B_X);
					y = bullet[i].y + B_SPLASH(bullet[i].o,j,B_Y);
					if (insideBounds(x,y)) {
						if (board[x][y] == B_WALL) {
							board[x][y] = B_EMPTY;
						}
					}
				}
				if (!other_bullet) {
					//Remove wall under bullet
					board[bullet[i].x][bullet[i].y] = B_EMPTY;
					bullet[i].active = 0;
				} else {
					//Remove the bullet from the board
					//Leave the wall so the other bullet can also detect a collision
					board[bullet[i].x][bullet[i].y] ^= B_LOOKUP(bullet[i].o);
				}
				break;
			case B_TANK:
				for (j = 0; j < 4; j++) {
					if (insideTank(j,bullet[i].x,bullet[i].y)) {
						tank[j].tag = 1;
						break;
					}
				}
				bullet[i].active = 0;
				break;
			case B_BASE:
				bullet[i].active = 0;
				if (onBase(PLAYER0,bullet[i].x,bullet[i].y)) {
					//Player0's base has been destroyed
					baseok[PLAYER0] = false;
				} else {
#if ASSERT
					if (!onBase(PLAYER1,bullet[i].x,bullet[i].y)) {
						cerr << "Base collision WITH NO BASE!" << endl;
					}
#endif
					//cout << "W_PLAYER0" << endl;
					//Player1's base has been destroyed
					baseok[PLAYER1] = false;
				}
				gameover = true;
				break;
			case B_EMPTY:
			default:
				if (other_bullet) {
					//It's an empty square with at least one other bullet
					bullet[i].tag = 1;
				}
#if ASSERT
				else {
					if (square) {
						cerr << "Phantom collision! " << (int)square << endl;
					}
					//else: NO COLLISION!
				}
#endif
			}
		}

	}

	if (gameover) {
		if (!baseok[PLAYER0] && !baseok[PLAYER1]) {
			winner = W_DRAW;
		} else if (baseok[PLAYER0] && !baseok[PLAYER1]) {
			winner = W_PLAYER0;
		} else {
#if ASSERT
			if (baseok[PLAYER0] && baseok[PLAYER1]) {
				cerr << "gameover declared, but both bases are fine!" << endl;
			}
#endif
			winner = W_PLAYER1;
		}
	}
	//Erase tagged bullets
	for (i = 0; i < 4; i++) {
		if (bullet[i].active && bullet[i].tag) {
			board[bullet[i].x][bullet[i].y] = B_EMPTY;
			bullet[i].active = 0;
		}
	}
	//Erase tagged tanks
	for (i = 0; i < 4; i++) {
		if (tank[i].tag) {
			drawTank(i, B_EMPTY);
			tank[i].active = 0;
		}
	}

	//Check for draw (no tanks and no bullets)
	if (!gameover && (tank[0].active+tank[1].active+tank[2].active+tank[3].active +
			bullet[0].active+bullet[1].active+bullet[2].active+bullet[3].active == 0)) {
		gameover = true;
		winner = W_DRAW;
	}

	stop_playout = false;
	if (!gameover) {
		active_tanks[PLAYER0] = 0;
		active_tanks[PLAYER1] = 0;
		for (i = 0; i < 4; i++) {
			active_tanks[i/2] += tank[i].active;
		}
		state_score = active_tanks[PLAYER0]*0.20 - active_tanks[PLAYER1]*0.20 + 0.5;
		if (active_tanks[PLAYER0] != active_tanks[PLAYER1]) {
			stop_playout = true;
		}
	} else {
		state_score = winner;
		stop_playout = true;
	}
}

void PlayoutState::checkDestroyedBullets()
//This checks if the bullet might be destroyed in the next turn.
//It's not 100% accurate: it will return some false negatives and positives, depending on weird situations.
//Hopefully they are rare.
{
	int i,j,square,other_bullet,prevsquare;

	for (i = 0; i < 4; i++) {
		bullet[i].tag = 0;
	}
	for (i = 0; i < 4; i++) {
		tank[i].tag = 0;
	}

	for (i = 0; i < 4; i++) {
		if (bullet[i].active) {
			prevsquare = board[bullet[i].x - O_LOOKUP(bullet[i].o,O_X)][bullet[i].y - O_LOOKUP(bullet[i].o,O_Y)];
			//If the bullet passed another one in the opposite direction, it's tagged for removal;
			bullet[i].tag = prevsquare & B_OPPOSITE(bullet[i].o);
			other_bullet = ((board[bullet[i].x][bullet[i].y] & (B_BULLET)) != B_LOOKUP(bullet[i].o));
			square = board[bullet[i].x][bullet[i].y] & (B_DESTRUCTABLES);
			if (square == B_TANK) {
				j = 0;
				while ((j < 3) && !insideTinyTank(j,bullet[i].x,bullet[i].y)) {
					j++;
				}
				tank[j].tag = 1;
				bullet[i].active = 0;
			}
			if (other_bullet) {
				bullet[i].tag = 1;
			}
		}
	}

	//Erase tagged bullets
	for (i = 0; i < 4; i++) {
		if (bullet[i].active && bullet[i].tag) {
			board[bullet[i].x][bullet[i].y] = B_EMPTY;
			bullet[i].active = 0;
		}
	}
	//Erase tagged tanks
	for (i = 0; i < 4; i++) {
		if (tank[i].tag) {
			drawTinyTank(i, B_EMPTY);
			tank[i].active = 0;
		}
	}
}

void PlayoutState::simulateTick()
{
	tickno++;

	if (tickno >= endgame_tick) {
		min_x++;
		max_x--;
	}
	gameover = false;
	moveBullets();
	checkCollisions();
	if (gameover) {
		return;
	}

	moveBullets();
	moveTanks();
	checkCollisions();
	if (gameover) {
		return;
	}

	fireTanks();
	checkCollisions();
}

void PlayoutState::updateCanFire()
//This is not 100% effective, but a good heuristic.
{
	PlayoutState *tmp_state = new PlayoutState(*this);
	int i;
	for (i = 0; i < 4; i++) {
		if (tank[i].active) {
			tmp_state->drawTank(i,B_EMPTY);
		}
	}
	tmp_state->drawTinyTanks();
	tmp_state->moveBullets();
	tmp_state->checkDestroyedBullets();
	tmp_state->moveBullets();
	tmp_state->checkDestroyedBullets();
	for (i = 0; i < 4; i++) {
		tank[i].canfire = !tmp_state->bullet[i].active;
	}
	delete tmp_state;
}

void PlayoutState::move(Move& m)
{
	command[0] = C_T0(m.alpha,m.beta);
	command[1] = C_T1(m.alpha,m.beta);
	command[2] = C_T2(m.alpha,m.beta);
	command[3] = C_T3(m.alpha,m.beta);
	simulateTick();
}

double PlayoutState::playout(sfmt_t* sfmt, UtilityScores& utility)
{
#if DEBUG
	bool pickedgreedy[4];
#endif
	int move,tankid;
	int maxmove = endgame_tick+(max_x/2)-tickno;
	winner = W_DRAW;
	scored_cmds_t cmds;
	for (move = 0; move < maxmove; move++) {
		//Expensive version
		/*
		updateCanFire();
		updateSimpleUtilityScores();
		updateExpensiveUtilityScores();*/
		for (tankid = 0; tankid < 4; tankid++) {
			if (sfmt_genrand_uint32(sfmt) % 100 < (EPSILON_GREEDY-1)) {
				//EPSILON_GREEDY% of the moves are "greedy" moves.
#if DEBUG
				pickedgreedy[tankid] = true;
#endif
				for (tankid = 0; tankid < 4; tankid++) {
					if (tank[tankid].active) {
						//Cheap version
						tank[tankid].canfire = !bullet[tankid].active;
						cmds.clear();
						command[tankid] = bestC(tankid,utility.simplecost[tankid/2],cmds);
					}
				}
			} else {
#if DEBUG
				pickedgreedy[tankid] = false;
#endif
				//Random playout
				command[tankid] = sfmt_genrand_uint32(sfmt) % 6;
				//command[tankid] = tank[tankid].active * ((tank[tankid].canfire)*(sfmt_genrand_uint32(&sfmt) % 6)
				//		+ (1-tank[tankid].canfire)*(sfmt_genrand_uint32(&sfmt) % 4));
			}
		}
#if DEBUG
		paintUtilityScores();
		cout << "commands:";
		for (tankid = 0; tankid < 4; tankid++) {
			if (tank[tankid].active) {
				cout << " [" << tankid << ": " << cmd2str(command[tankid]);
				if (pickedgreedy[tankid]) {
					cout << "*";
				}
				cout << "]";
			}
		}
		cout << endl;
#endif
		simulateTick();
		//paint();
		if (gameover) {
			return winner;
		}
		if (stop_playout && (sfmt_genrand_uint32(sfmt) % 10 == 0)) {
			return state_score;
		}
	}
	return state_score;
}

bool PlayoutState::clearFireTrajectory(int x, int y, int o, int t_x, int t_y, board_t& obstacles)
{
	//WARNING: DON'T CALL WITH OOB t_x and t_y
	bool clear = true;
	//We start from the center of tank: need to move 3 blocks to get where the bullet starts
	x += 3*O_LOOKUP(o,O_X);
	y += 3*O_LOOKUP(o,O_Y);
	while (clear && ((x != t_x) || (y != t_y))) {
		clear = insideBounds(x,y) && ((obstacles[x][y] & (B_WALL|B_OOB)) == 0);
		x += O_LOOKUP(o,O_X);
		y += O_LOOKUP(o,O_Y);
	}
	return clear;
}

bool PlayoutState::clearBallisticTrajectory(int x, int y, int o, int t_x, int t_y, board_t& obstacles)
{
	//WARNING: DON'T CALL WITH OOB t_x and t_y
	bool clear = true;
	while (clear && ((x != t_x) || (y != t_y))) {
		clear = insideBounds(x,y) && ((obstacles[x][y] & (B_WALL|B_OOB)) == 0);
		x += O_LOOKUP(o,O_X);
		y += O_LOOKUP(o,O_Y);
	}
	return clear;
}

bool PlayoutState::canMove(int x, int y, int o, board_t& obstacles)
//Check for any obstacles
{
	bool clear = insideBounds(x+MOVEPATH_LOOKUP(o,2,O_X),y+MOVEPATH_LOOKUP(o,2,O_Y));
	int i;
	for (i = 0; clear && (i < 5); i++) {
		clear = !(obstacles[x+MOVEPATH_LOOKUP(o,i,O_X)][y+MOVEPATH_LOOKUP(o,i,O_Y)] & (B_BASE|B_OOB|B_WALL|B_TANK));
#if ASSERT
		if (!insideBounds(x+MOVEPATH_LOOKUP(o,i,O_X),y+MOVEPATH_LOOKUP(o,i,O_Y))) {
			//	cerr << "clearPath going OOB! x: " << x << " y: " << y << " min_y: " << min_y << endl;
		}
#endif
	}
	return clear;
}

bool PlayoutState::clearPath(int x, int y, int o, board_t& obstacles)
//Check for OOB obstacles
{
	bool clear = insideBounds(x+MOVEPATH_LOOKUP(o,2,O_X),y+MOVEPATH_LOOKUP(o,2,O_Y));
	int i;
	for (i = 0; clear && (i < 5); i++) {
		clear = !(obstacles[x+MOVEPATH_LOOKUP(o,i,O_X)][y+MOVEPATH_LOOKUP(o,i,O_Y)] & B_OOB);
#if ASSERT
		if (!insideBounds(x+MOVEPATH_LOOKUP(o,i,O_X),y+MOVEPATH_LOOKUP(o,i,O_Y))) {
			//	cerr << "clearPath going OOB! x: " << x << " y: " << y << " min_y: " << min_y << endl;
		}
#endif
	}
	return clear;
}

bool PlayoutState::clearPath(int x, int y, int o)
//Check for walls
{
	bool clear = insideBounds(x+MOVEPATH_LOOKUP(o,2,O_X),y+MOVEPATH_LOOKUP(o,2,O_Y));
	int i;
	for (i = 0; clear && (i < 5); i++) {
		clear = !(board[x+MOVEPATH_LOOKUP(o,i,O_X)][y+MOVEPATH_LOOKUP(o,i,O_Y)] & (B_WALL|B_TANK));
#if ASSERT
		if (!insideBounds(x+MOVEPATH_LOOKUP(o,i,O_X),y+MOVEPATH_LOOKUP(o,i,O_Y))) {
			//	cerr << "clearPath going OOB! x: " << x << " y: " << y << " min_y: " << min_y << endl;
		}
#endif
	}
	return clear;
}

bool PlayoutState::clearablePath(int x, int y, int o, board_t& obstacles)
{
	//NOT A SUBSTITUTE FOR OOB x+move, y+move!
	bool destructable = true;
	int i;
	for (i = 0;destructable && (i < 5); i++) {
		destructable = (obstacles[x+MOVEPATH_LOOKUP(o,i,O_X)][y+MOVEPATH_LOOKUP(o,i,O_Y)] & B_OOB) == 0;
	}
	return destructable && (obstacles[x+FIRE_LOOKUP(o,O_X)][y+FIRE_LOOKUP(o,O_Y)] & B_WALL) == B_WALL;
}

bool PlayoutState::clearablePath(int x, int y, int o)
{
	//NOT A SUBSTITUTE FOR OOB x+move, y+move!
	return (board[x+FIRE_LOOKUP(o,O_X)][y+FIRE_LOOKUP(o,O_Y)] & B_WALL) == B_WALL;
}

void PlayoutState::seedBase(const int player, priority_queue<Tank>& frontier, board_t& obstacles)
{
	int i,o;
	Tank t;
	int wallcount;
	for (o = 0; o < 4; o++) {
		//The 4 dirs for the base
		t.cost = 1;
		t.x = base[player].x - 3*O_LOOKUP(o,O_X);
		t.y = base[player].y - 3*O_LOOKUP(o,O_Y);
		int dx = -O_LOOKUP(o,O_X);
		int dy = -O_LOOKUP(o,O_Y);
		t.o = o;
		wallcount = 1;
		for (i = 0; i < max(max_x,max_y); i++) {
			if ((i & 1) == 0) {
				t.cost+=wallcount;
			}
			if (!isTankInsideBounds(t.x,t.y)) {
				break;
			}
			if (obstacles[t.x-3*dx][t.y-3*dy] & B_WALL) {
				wallcount++;
				t.cost+=wallcount;
			}
			frontier.push(t);
			t.x += dx;
			t.y += dy;
		}
	}
}

void PlayoutState::findPath(priority_queue<Tank>& frontier, costmatrix_t& costmatrix, board_t& obstacles)
{
	Tank t,g;
	//Flood-fill backward to determine shortest greedy path
	while (!frontier.empty()) {
		g = frontier.top();
		frontier.pop();
#if ASSERT
		if (!isTankInsideBounds(g.x,g.y)) {
			cerr << "Tank OOB! x: " << g.x << " y: " << g.y << endl;
		}
#endif
		if (g.cost < costmatrix[g.x][g.y][g.o]) {
			costmatrix[g.x][g.y][g.o] = g.cost;
			t.x = g.x - O_LOOKUP(g.o,O_X);
			t.y = g.y - O_LOOKUP(g.o,O_Y);
			if (isTankInsideBounds(t.x,t.y)) {
				if (canMove(t.x,t.y,g.o,obstacles)) {
					//Tank can move from t to g: no obstacles.
					t.cost = g.cost+1; //Move
					for (t.o = 0; t.o < 4; t.o++) {
						if (t.cost < costmatrix[t.x][t.y][t.o]) {
							frontier.push(t);
						}
					}
				} else {
					if (clearablePath(t.x,t.y,g.o,obstacles)) {
						//Tank can move from t to g: it needs to whack the wall
						for (t.o = 0; t.o < 4; t.o++) {
							if (t.o == g.o) {
								//It's facing the wall
								t.cost = g.cost+2; //Fire+Move
							} else {
								//It's not facing the wall
								t.cost = g.cost+3; //Turn+Fire+Move
							}
							if (t.cost < costmatrix[t.x][t.y][t.o]) {
								frontier.push(t);
							}
						}
					}
				}
			}
			if (canRotate(g.x,g.y,g.o,obstacles)) {
				//Tank can rotate on to g
				t.x = g.x;
				t.y = g.y;
				t.cost = g.cost+1; //Turn
				for (t.o = 0; t.o < 4; t.o++) {
					if (t.cost < costmatrix[t.x][t.y][t.o]) {
						frontier.push(t);
					}
				}
			}
		}
	}
}

void PlayoutState::updateSimpleUtilityScores(UtilityScores& utility, obstacles_t& obstacles)
{
	int i,j,player,o;
	priority_queue<Tank> frontier;

	for (player = 0; player < 2; player++) {
		for (i = 0; i < max_x; i++) {
			for (j = 0; j < max_y; j++) {
				for (o = 0; o < 4; o++) {
					utility.simplecost[player][i][j][o] = INT_MAX;
				}
			}
		}
	}

	//Obstacles are the same for both sides.
	memcpy(obstacles[0],board,sizeof(board_t));

	for (player = 0; player < 2; player++) {
		//Just go after the base
		seedBase(1-player,frontier,obstacles[0]);
		findPath(frontier,utility.simplecost[player],obstacles[0]);
	}

	for (o = 0; o < 4; o++) {
		utility.simplecost[PLAYER0][base[PLAYER1].x][base[PLAYER1].y][o] = 0;
		utility.simplecost[PLAYER1][base[PLAYER0].x][base[PLAYER0].y][o] = 0;
	}
}

void PlayoutState::paintObstacles(board_t& obstacles)
{
	int i,j;
	for (j = min_y; j < max_y; j++) {
		for (i = min_x; i < max_x; i++) {
			if ((obstacles[i][j] & (B_WALL|B_OOB)) == (B_WALL|B_OOB)) {
				cout << "#";
			} else {
				if ((obstacles[i][j] & (B_WALL)) == B_WALL) {
					cout << "=";
				} else if ((obstacles[i][j] & (B_OOB)) == B_OOB) {
					cout << "+";
				} else {
					cout << " ";
				}
			}
		}
		cout << endl;
	}
}

bool PlayoutState::lineOfSight(const int sx, const int sy, const int o, const int tx, const int ty)
{
	switch (o) {
	default:
	case O_UP:
		return (sx == tx) && (ty < sy);
	case O_DOWN:
		return (sx == tx) && (ty > sy);
	case O_LEFT:
		return (sy == ty) && (tx < sx);
	case O_RIGHT:
		return (sy == ty) && (tx > sx);
	}
}

void PlayoutState::updateExpensiveUtilityScores(UtilityScores& utility, obstacles_t& obstacles)
{
	int o,i,j,tankid,comradeid,traveldistance[4];
	//int enemyid;
	int targetx,targety,deltax,deltay,playerid;
	priority_queue<Tank> frontier;

	for (tankid = 0; tankid < 4; tankid++) {
		for (i = 0; i < max_x; i++) {
			for (j = 0; j < max_y; j++) {
				for (o = 0; o < 4; o++) {
					utility.expensivecost[tankid][i][j][o] = INT_MAX;
				}
			}
		}
	}
	for (playerid = 0; playerid < 2; playerid++) {
		for (tankid = (playerid*2); tankid < (playerid*2+2); tankid++) {
			//Each tank has a different set of obstacles
			memcpy(obstacles[tankid],board,sizeof(board_t));
			/*
			for (enemyid = (1-playerid)*2; enemyid < ((1-playerid)*2+2); enemyid++) {
				if (tank[enemyid].active) {
					if (abs(tank[enemyid].x - tank[tankid].x)
							+ abs(tank[enemyid].y - tank[tankid].y) < TANK_PROXIMITY_WARNING) {
						drawTankObstacle(enemyid,obstacles[tankid]);
					}
				}
			}*/
			comradeid = tankid^1;
			//Friendly tank counts as immovable obstacle
			if (tank[comradeid].active) {
				drawTankObstacle(comradeid,obstacles[tankid]);
				if (tankid & 1) {
					//Tank 0 has priority.
					targetx = tank[comradeid].x;
					targety = tank[comradeid].y;
					for (i = 0; i < max_y; i++) {
						//Bail out at 80+ moves ahead.
						o = bestO(targetx,targety,utility.expensivecost[comradeid]);
						targetx += O_LOOKUP(o,O_X);
						targety += O_LOOKUP(o,O_Y);
						drawTankObstacle(targetx,targety,obstacles[tankid]);
						if (lineOfSight(targetx,targety,o,base[1-playerid].x,base[1-playerid].y)) {
							break;
						}
					}
				}
			}
			//Path of bullets count as immovable obstacle
			for (j = 0; j < 4; j++) {
				if (bullet[j].active && j != tankid) {
					targetx = bullet[j].x;
					targety = bullet[j].y;
					deltax = O_LOOKUP(bullet[j].o,O_X);
					deltay = O_LOOKUP(bullet[j].o,O_Y);
				} else {
					continue;
				}
				for (i = 0; i < max_y/2; i++) {
					if (insideBounds(targetx,targety)
							&& !(board[targetx][targety] & (B_WALL|B_OPPOSITE(bullet[j].o)))) {
						obstacles[tankid][targetx][targety] = B_OOB;
						targetx += deltax;
						targety += deltay;
					} else {
						break;
					}
				}
				traveldistance[j] = i;
			}

			for (j = 0; j < 4; j++) {
				if (j != tankid && j != comradeid && tank[j].active && tank[j].canfire
						&& (abs(tank[j].x - tank[tankid].x)
								+ abs(tank[j].y - tank[tankid].y) < TANK_PROXIMITY_WARNING)) {
					o = tank[j].o;
					traveldistance[j] = 10;
					targetx = tank[j].x + FIRE_LOOKUP(o,O_X);
					targety = tank[j].y + FIRE_LOOKUP(o,O_Y);
					deltax = O_LOOKUP(o,O_X);
					deltay = O_LOOKUP(o,O_Y);
					for (i = 0; i < traveldistance[j]; i++) {
						if (insideBounds(targetx,targety)
								&& !(board[targetx][targety] & B_OPPOSITE(bullet[j].o))) {
							if (board[targetx][targety] & B_WALL) {
								i*=2;
							}
							obstacles[tankid][targetx][targety] = B_OOB;
							targetx += deltax;
							targety += deltay;
						} else {
							break;
						}
					}
					traveldistance[j] = 10;
				}
			}
			seedBase(1-playerid,frontier,obstacles[tankid]);
#if DEBUGOBSTACLES
			cout << "Obstacles for tank " << tankid << endl;
			cout << "=======================" << endl;
			paintObstacles(obstacles[tankid]);
#endif
			findPath(frontier,utility.expensivecost[tankid],obstacles[tankid]);

			if (tank[tankid].canfire) {
				//Put down breadcrumbs to turn and fire for active defence
				for (j = (1-playerid); j < (1-playerid)*2+2; j++) {

					if (bullet[j].active) {
						targetx = bullet[j].x;
						targety = bullet[j].y;
						deltax = O_LOOKUP(bullet[j].o,O_X);
						deltay = O_LOOKUP(bullet[j].o,O_Y);
						o = bullet[j].o;
					} else if (tank[j].active && (abs(tank[j].x - tank[tankid].x)
							+ abs(tank[j].y - tank[tankid].y) < TANK_PROXIMITY_WARNING)) {
						targetx = tank[j].x + FIRE_LOOKUP(tank[j].o,O_X);
						targety = tank[j].y + FIRE_LOOKUP(tank[j].o,O_Y);
						deltax = O_LOOKUP(tank[j].o,O_X);
						deltay = O_LOOKUP(tank[j].o,O_Y);
						o = tank[j].o;
					} else {
						continue;
					}
					for (i = 0; i < 24; i++) {
						if (isTankInsideBounds(targetx,targety)
								&& (board[targetx][targety] & (B_WALL|B_OPPOSITE(o))) == 0) {
							utility.expensivecost[tankid][targetx][targety][O_OPPOSITE(o)] = (i/2)+1;
							targetx += deltax;
							targety += deltay;
						} else {
							break;
						}
					}
				}
			}

			for (o = 0; o < 4; o++) {
				utility.expensivecost[tankid][base[1-playerid].x][base[1-playerid].y][o] = 0;
			}
		}
	}
	//The 4 dirs for each enemy tank
	/* int tankno;
	 for (j = 0; j < 5; j++) {
		for (tankno = 0; tankno < 2; tankno++) {
			if (tank[(1-player)*2+tankno].active) {
				for (i = 0; i < max(max_x,max_y); i++) {
					t.cost = i/2 + WEAKSPOT_LOOKUP(j); //rather go for the base than for the tank!
					t.o = o;
					targetx = tank[(1-player)*2+tankno].x - BUMP_LOOKUP(o,j,O_X);
					targety = tank[(1-player)*2+tankno].y - BUMP_LOOKUP(o,j,O_Y);
					t.x = targetx - (3+i)*O_LOOKUP(o,O_X);
					t.y = targety - (3+i)*O_LOOKUP(o,O_Y);
					if (isTankInsideBounds(t.x,t.y) && clearFireTrajectory(t.x,t.y,t.o,targetx,targety)) {
						u.cost[player][t.x][t.y][t.o] = t.cost;
						frontier.push(t);
					} else {
						break;
					}
				}
			}
		}
	}*/
}

bool PlayoutState::onBase(const int b, const int x, const int y)
{
	return (x == base[b].x && y == base[b].y);
}

bool PlayoutState::onTarget(const int p, const int x, const int y)
{
	return (onBase(1-p,x,y) || insideTank((1-p)*2,x,y) || insideTank((1-p)*2+1,x,y));
}

bool PlayoutState::onFriendly(const int p, const int x, const int y)
{
	return (onBase(p,x,y) || insideTank(p,x,y) || insideTank(p+1,x,y));
}

bool PlayoutState::incomingBullet(const int x, const int y, const int o)
{
	int i;
	bool incoming = false;
	for (i = 0; i < 4 && !incoming; i++) {
		incoming = bullet[i].active && (bullet[i].o == (o^1)) && (bullet[i].x == x) && (bullet[i].y == y);
	}
	return incoming;
}

int PlayoutState::bestC(int tank_id, costmatrix_t& costmatrix, scored_cmds_t& cmds)
{
	scored_cmd_t cmd;
	int i;
	int wallcount;
	int hitcost;
	int x,y;
	int player = tank_id/2;
	TankState& t = tank[tank_id];
	TankState& comrade = tank[tank_id^1];
	BaseState& enemybase = base[1-player];
	int playerid = tank_id/2;
	int friendly_tanks = t.active+comrade.active;
	int enemy_tanks = tank[(1-playerid)*2].active+tank[(1-playerid)*2+1].active;

	int besto = bestOCMD(t.x,t.y,costmatrix,cmds);
	cmd.first = C_NONE;
	cmd.second = costmatrix[t.x][t.y][t.o]+1;
	//One tank goes limp
	if (enemy_tanks == 0 && friendly_tanks == 2) {
		int tank_cost = abs(t.x-enemybase.x)+abs(t.y-enemybase.y);
		int comrade_cost = abs(comrade.x-enemybase.x)+abs(comrade.y-enemybase.y);
		if ((tank_cost > comrade_cost) || (tank_cost == comrade_cost && (tank_id & 1) == 1)) {
			cmd.second = costmatrix[t.x][t.y][t.o]-2;
		}
	}
	cmds.push_back(cmd);

	cmd.first = C_FIRE;
	if (!t.canfire) {
		cmd.second = INT_MAX;
	} else {
		x = t.x + FIRE_LOOKUP(t.o,O_X);
		y = t.y + FIRE_LOOKUP(t.o,O_Y);
		wallcount = 0;
		i = 0;
		cmd.second = INT_MAX-1;
		while (insideBounds(x,y) && !onTarget(player,x,y) && !onFriendly(player,x,y)) {
			//TODO: switch to map lookup?
			if (wallcount == 0 && (board[x][y] & B_OPPOSITE(t.o))) {
				cmd.second = 0;
				break;
			}
			wallcount += (board[x][y] & B_WALL)*((i/2) + 1);
			i++;
			x += O_LOOKUP(t.o,O_X);
			y += O_LOOKUP(t.o,O_Y);
		}
		if (cmd.second) {
			hitcost = INT_MAX-1;
			if (onTarget(player,x,y)) {
				//HIT!
				hitcost = ((i/2)+wallcount);
			}
			if ((t.o == besto-C_UP) && isTankInsideBounds(t.x + O_LOOKUP(t.o,O_X),t.y + O_LOOKUP(t.o,O_Y)) && clearablePath(t.x,t.y,t.o) ) {
				//Shoot to clear a space to move in
				cmd.second = min(hitcost,costmatrix[t.x + O_LOOKUP(t.o,O_X)][t.y + O_LOOKUP(t.o,O_Y)][t.o]);
			} else {
				//Just shoot
				cmd.second = hitcost;
			}
		}
	}
	cmds.push_back(cmd);
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

bool PlayoutState::isTankInFiringLine(const int t, board_t& obstacles)
{
	bool hit = false;
	int i,j;
	for (i = tank[t].x-2; !hit && i < tank[t].x+3;i++) {
		for (j = tank[t].y-2; !hit && j < tank[t].y+3;j++) {
			hit = (obstacles[i][j] & B_OOB) != 0;
		}
	}
	return hit;
}

int PlayoutState::bestCExpensive(int tank_id, costmatrix_t& costmatrix, board_t& obstacles, scored_cmds_t& cmds)
{
	scored_cmd_t cmd;
	bool dodge;
	int i;
	int wallcount;
	int hitcost;
	int x,y;
	int player = tank_id/2;
	TankState& t = tank[tank_id];
	TankState& comrade = tank[tank_id^1];
	BaseState& enemybase = base[1-player];
	int playerid = tank_id/2;
	int friendly_tanks = t.active+comrade.active;
	int enemy_tanks = tank[(1-playerid)*2].active+tank[(1-playerid)*2+1].active;
	int besto;
	dodge = false;
	if (isTankInFiringLine(tank_id,obstacles)) {
#if DEBUGOBSTACLES
		cout << "Tank [" << tank_id << "] needs to dodge! Using the following obstacles:" << endl;
		paintObstacles(obstacles);
#endif
		dodge = true;
		if (tank[tank_id].canfire) {
			besto = bestOCMDDodgeCanfire(tank_id,obstacles,cmds);
		} else {
			besto = bestOCMDDodgeCantfire(tank_id,obstacles,cmds);
		}
		cmd.first = C_NONE;
		cmd.second = INT_MAX;
		cmds.push_back(cmd);
	} else {
		besto = bestOCMD(t.x,t.y,costmatrix,obstacles,cmds);
		cmd.first = C_NONE;
		cmd.second = costmatrix[t.x][t.y][t.o]+1;
		//One tank goes limp
		if (enemy_tanks == 0 && friendly_tanks == 2) {
			int tank_cost = abs(t.x-enemybase.x)+abs(t.y-enemybase.y);
			int comrade_cost = abs(comrade.x-enemybase.x)+abs(comrade.y-enemybase.y);
			if ((tank_cost > comrade_cost) || (tank_cost == comrade_cost && (tank_id & 1) == 1)) {
				cmd.second = costmatrix[t.x][t.y][t.o]-2;
			}
		}
		cmds.push_back(cmd);

	}

	cmd.first = C_FIRE;
	if (!t.canfire) {
		cmd.second = INT_MAX;
	} else {
		if (!dodge) {
			x = t.x + FIRE_LOOKUP(t.o,O_X);
			y = t.y + FIRE_LOOKUP(t.o,O_Y);
			wallcount = 0;
			i = 0;
			cmd.second = INT_MAX-1;
			while (insideBounds(x,y) && !onTarget(player,x,y) && !onFriendly(player,x,y)) {
				//TODO: switch to map lookup?
				if (wallcount == 0 && (board[x][y] & B_OPPOSITE(t.o))) {
					cmd.second = 0;
					break;
				}
				wallcount += (board[x][y] & B_WALL)*((i/2) + 1);
				i++;
				x += O_LOOKUP(t.o,O_X);
				y += O_LOOKUP(t.o,O_Y);
			}
			if (cmd.second) {
				hitcost = INT_MAX-1;
				if (onTarget(player,x,y)) {
					//HIT!
					hitcost = ((i/2)+wallcount);
				}
				if ((t.o == besto-C_UP) && isTankInsideBounds(t.x + O_LOOKUP(t.o,O_X),t.y + O_LOOKUP(t.o,O_Y)) && clearablePath(t.x,t.y,t.o) ) {
					//Shoot to clear a space to move in
					cmd.second = min(hitcost,costmatrix[t.x + O_LOOKUP(t.o,O_X)][t.y + O_LOOKUP(t.o,O_Y)][t.o]);
				} else {
					//Just shoot
					cmd.second = hitcost;
				}
			}
		} else {
			if ((t.o == besto-C_UP) && isTankInsideBounds(t.x + O_LOOKUP(t.o,O_X),t.y + O_LOOKUP(t.o,O_Y)) && clearablePath(t.x,t.y,t.o) ) {
				cmd.second = 0; //Need to clear a space for dodging
			} else {
				cmd.second = INT_MAX; //We're not taking potshots if we're being shot at.
			}
		}
	}
	cmds.push_back(cmd);
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

int PlayoutState::bestOCMD(int x, int y, costmatrix_t& costmatrix, scored_cmds_t& cmds)
{
	scored_cmd_t o_score;
	int o;
	for (o = 0; o < 4; o++) {
		o_score.first = o+C_UP;
		if (clearPath(x,y,o)) {
			o_score.second = costmatrix[x + O_LOOKUP(o,O_X)][y + O_LOOKUP(o,O_Y)][o];
		} else {
			o_score.second = costmatrix[x][y][o];
		}
		cmds.push_back(o_score);
	}
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

int PlayoutState::bestOCMD(int x, int y, costmatrix_t& costmatrix, board_t& obstacles, scored_cmds_t& cmds)
{
	scored_cmd_t o_score;
	int o;
	for (o = 0; o < 4; o++) {
		o_score.first = o+C_UP;
		if (clearPath(x,y,o,obstacles)) {
			//No nasty OOB stuff.
			if (clearPath(x,y,o)) {
				o_score.second = costmatrix[x + O_LOOKUP(o,O_X)][y + O_LOOKUP(o,O_Y)][o];
			} else {
				o_score.second = costmatrix[x][y][o];
			}
		} else {
			o_score.second = INT_MAX;
		}
		cmds.push_back(o_score);
	}
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

int PlayoutState::bestOCMDDodgeCanfire(int t, board_t& obstacles, scored_cmds_t& cmds)
{
	scored_cmd_t o_score;
	int o,i,j,x,y,ai,aj,bx,by,range;
	for (o = 0; o < 4; o++) {
		x = tank[t].x + O_LOOKUP(o,O_X);
		y = tank[t].y + O_LOOKUP(o,O_Y);
		o_score.first = o+C_UP;
		if (!isTankInsideBounds(x,y) || !(clearPath(x,y,o) || clearablePath(x,y,o))) {
			//We can only rotate
			x = tank[t].x;
			y = tank[t].y;
		}
		o_score.second = 0;
		for (i = x-2, ai = 0; ai < 5; i++, ai++) {
			for (j = y-2, aj = 0; aj < 5; j++, aj++) {
				o_score.second += (obstacles[i][j] & (B_WALL|B_OOB))*AVOIDANCE_CANFIRE_MATRIX[ai][aj];
			}
		}
		if (o_score.second == 0) {
			range = 0;
			//It's either the wrong way or the right way!
			bx = tank[t].x + 3*O_LOOKUP(o,O_X);
			by = tank[t].y + 3*O_LOOKUP(o,O_X);
			while(insideBounds(bx,by)) {
				if ((board[bx][by] & B_OPPOSITE(o)) || onTarget(t/2,bx,by)) {
					break;
				}
				range++;
				bx += O_LOOKUP(o,O_X);
				by += O_LOOKUP(o,O_Y);
			}
			if (!insideBounds(bx,by)) {
				o_score.second = range;
			}
		}

		cmds.push_back(o_score);
	}
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

int PlayoutState::bestOCMDDodgeCantfire(int t, board_t& obstacles, scored_cmds_t& cmds)
{
	scored_cmd_t o_score;
	int o,i,j,x,y,ai,aj;
	for (o = 0; o < 4; o++) {
		x = tank[t].x + O_LOOKUP(o,O_X);
		y = tank[t].y + O_LOOKUP(o,O_Y);
		o_score.first = o+C_UP;
		if (!isTankInsideBounds(x,y) || !clearPath(x,y,o)) {
			//We can only rotate.
			x = tank[t].x;
			y = tank[t].y;
		}
		o_score.second = 0;
		for (i = x-2, ai = 0; ai < 5; i++, ai++) {
			for (j = y-2, aj = 0; aj < 5; j++, aj++) {
				if (obstacles[i][j] & (B_WALL|B_OOB)) {
					o_score.second += B_OOB*AVOIDANCE_CANTFIRE_MATRIX[ai][aj];
				}
			}
		}
		cmds.push_back(o_score);
	}
	sort(cmds.begin(), cmds.end(), sort_pair_second<int, int>());
	return cmds[0].first;
}

int PlayoutState::bestO(int x, int y, costmatrix_t& costmatrix)
{
	int besto = O_UP;
	int bestscore = INT_MAX;
	int score;
	int o;
	for (o = 0; o < 4; o++) {
		score = costmatrix[x + O_LOOKUP(o,O_X)][y + O_LOOKUP(o,O_Y)][o];
		if (score < bestscore) {
			bestscore = score;
			besto = o;
		}
	}
	return besto;
}

void PlayoutState::paintUtilityScores(UtilityScores& utility)
{
	int o,x,y,besto,bestscore,score,bestcount;
	unsigned char direction[MAX_BATTLEFIELD_DIM][MAX_BATTLEFIELD_DIM];
	int count[MAX_BATTLEFIELD_DIM][MAX_BATTLEFIELD_DIM];
	Tank t;
	paint();

	memset(direction,0xff,sizeof(direction));
	memset(count,0,sizeof(count));

	for (y = 0; y < max_y; y++) {
		for (x = 0; x < max_x; x++) {
			besto = O_UP;
			bestscore = INT_MAX;
			bestcount = 0;
			if (isTankInsideBounds(x,y)) {
				for (o = 0; o < 4; o++) {
					t.x = x + O_LOOKUP(o,O_X);
					t.y = y + O_LOOKUP(o,O_Y);
					score = utility.simplecost[0][t.x][t.y][o];
					if (score == bestscore) {
						bestcount++;
					}
					if (score < bestscore) {
						bestscore = score;
						besto = o;
						bestcount = 1;
					}
				}
			}
			direction[x][y] = besto;
			count[x][y] = bestcount;
		}
	}
	for (y = 0; y < max_y; y++) {
		for (x = 0; x < max_x; x++) {
			/*if (count[x][y] != 1) {
				cout << "?";
				continue;
			}*/
			switch (direction[x][y]) {
			default:
				cout << "?";
				break;
			case O_UP:
				cout << "^";
				break;
			case O_DOWN:
				cout << "v";
				break;
			case O_LEFT:
				cout << "<";
				break;
			case O_RIGHT:
				cout << ">";
				break;
			}
		}
		cout << endl;
	}
	cout << endl;
#if 0
	for (o = 0; o < 4; o++) {
		cout << "====" << endl;
		cout << o2str(o) << endl;
		int maxcost = 0;
		for (i = min_x; i < max_x; i++) {
			for (j = min_y; j < max_y; j++) {
				if (u.expensivecost[0][i][j][o] != INT_MAX) {
					maxcost = max(maxcost,u.simplecost[0][i][j][o]);
				}
			}
		}
		cout << maxcost << endl;
		for (j = min_y; j < max_y; j++) {
			for (i = min_x; i < max_x; i++) {
				if (u.expensivecost[0][i][j][o] != INT_MAX) {
					cout << ((u.simplecost[0][i][j][o] - 1)*10/maxcost);
				} else {
					cout << "*";
				}
			}
			cout << endl;
		}
	}
#endif
}

void PlayoutState::paint()
{
	unsigned char canvas[MAX_BATTLEFIELD_DIM][MAX_BATTLEFIELD_DIM];
	int i,j;
	cout << "-=-=- tick: " << tickno << endl;
	cout << "base[0] x:" << setw(3) << base[0].x << " y:" << setw(3) << base[0].y << endl;
	for (i = 0; i < 4; i++) {
		cout << "t[" << i << "]";
		if (tank[i].active) {
			cout << " x:" << setw(3) << tank[i].x << " y:"<< setw(3) << tank[i].y << setw(6) << o2str(tank[i].o) << " f:" << setw(2) << tank[i].canfire;
		} else {
			cout << " x:" << setw(3) << "--" << " y:" << setw(3) << "--" << setw(6) << "--" << " f:" << setw(2) << "-";
		}
		if (bullet[i].active) {
			cout << " b[" << i << "] x:" << setw(3) << bullet[i].x << " y:" << setw(3) << bullet[i].y << setw(6) << o2str(bullet[i].o);
		}
		cout << endl;
	}
	cout << "base[1] x:" << setw(3) << base[1].x << " y:" << setw(3) << base[1].y << endl;
	cout << "=-=-= board" << endl;
	for (j = min_y; j < max_y; j++) {
		for (i = min_x; i < max_x; i++) {
			if (board[i][j] == B_EMPTY) {
				canvas[i][j] = ' ';
			} else if (board[i][j] == B_WALL) {
				canvas[i][j] = '#';
			} else if (board[i][j] == B_BASE) {
				canvas[i][j] = '&';
			}else if (board[i][j] == B_TANK) {

				canvas[i][j] = '@';
			} else if (board[i][j] & B_BULLET) {
				canvas[i][j] = '*';
			} else {
				canvas[i][j] = '?';
			}
		}
	}
	for (i = 0; i < 4; i++) {
		if (tank[i].active) {
			canvas[tank[i].x+2*O_LOOKUP(tank[i].o,O_X)][tank[i].y+2*O_LOOKUP(tank[i].o,O_Y)] = '+';
		}
	}
	for (j = min_y; j < max_y; j++) {
		for (i = min_x; i < max_x; i++) {
			cout << canvas[i][j];
		}
		cout << endl;
	}
}

ostream &operator<<(ostream &output, const PlayoutState &p)
{
	int i,j;
	output << p.tickno << endl;
	for (i = 0; i < 4; i++) {
		output	<< p.tank[i].id << ' '
				<< p.tank[i].active << ' '
				<< p.tank[i].o << ' '
				<< p.tank[i].x << ' '
				<< p.tank[i].y << '\n';
	}
	for (i = 0; i < 4; i++) {
		output	<< p.bullet[i].id << ' '
				<< p.bullet[i].active << ' '
				<< p.bullet[i].o << ' '
				<< p.bullet[i].x << ' '
				<< p.bullet[i].y << '\n';
	}
	for (i = 0; i < 2; i++) {
		output	<< p.base[i].x << ' '
				<< p.base[i].y << '\n';
	}
	output << p.max_x << ' ' << p.max_y << '\n';
	for (i = 0; i < p.max_x; i++) {
		for (j = 0; j < p.max_y; j++) {
			output << setw(3) << (int)p.board[i][j];
		}
		output << '\n';
		//output << P.board[i][P.dimy-1] << '\n';
	}

	return output;
}

istream &operator>>(istream &input, PlayoutState &p)
{
	int i,j,square;
	pair<int,int> tank;
	vector< pair<int,int> > tanks(4);
	input >> p.tickno;
	for (i = 0; i < 4; i++) {
		input >> p.tank[i].id
		>> p.tank[i].active
		>> p.tank[i].o
		>> p.tank[i].x
		>> p.tank[i].y;
		tank.first = i;
		tank.second = p.tank[i].id;
		tanks[i] = tank;
	}
	sort(tanks.begin(), tanks.end(), sort_pair_second<int, int>());
	for (i = 0; i < 4; i++) {
		p.tank_priority[i] = tanks[i].first;
	}
	for (i = 0; i < 4; i++) {
		input >> p.bullet[i].id
		>> p.bullet[i].active
		>> p.bullet[i].o
		>> p.bullet[i].x
		>> p.bullet[i].y;
	}
	for (i = 0; i < 2; i++) {
		input >> p.base[i].x
		>> p.base[i].y;
	}
	p.min_x = 0;
	p.min_y = 0;
	input >> p.max_x >> p.max_y;
	for (i = 0; i < p.max_x; i++) {
		for (j = 0; j < p.max_y; j++) {
			input >> square;
			p.board[i][j] = (unsigned char)square;
		}
	}
	return input;
}
