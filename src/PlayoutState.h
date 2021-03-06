//============================================================================
// Name        : PlayoutState.h
// Author      : Jan Gutter
// Copyright   : To the extent possible under law, Jan Gutter has waived all
//             : copyright and related or neighboring rights to this work.
//             : For more information, go to:
//             : http://creativecommons.org/publicdomain/zero/1.0/
//             : or consult the README and COPYING files
// Description : Interface for simulating a game state
//============================================================================

#ifndef PLAYOUTSTATE_H_
#define PLAYOUTSTATE_H_

#include <iostream>
#include "consts.h"
#include <SFMT.h>
#include <queue>
using namespace std;

struct TankState {
	int id;
	int active;
	int canfire;
	int x,y; //Position
	int o; //Orientation
	int a; //Action (probably going to get ignored)
	int tag; //Tagged to be destroyed
};

struct BulletState {
	int id;
	int active;
	int x,y; //Position
	int o; //Orientation
	int tag; //Tagged to be destroyed
};

struct BaseState {
	int x,y;
};

typedef int costmatrix_t[MAX_BATTLEFIELD_DIM][MAX_BATTLEFIELD_DIM][4];
class UtilityScores {
public:
	//(player)(x)(y)(o)
	costmatrix_t simplecost[2];
	//(tankid)(x)(y)(o)
	costmatrix_t expensivecost[4];
};

struct Tank {
	int x,y,o,cost;
};

typedef pair<int,int> scored_cmd_t;
typedef vector<scored_cmd_t> scored_cmds_t;

inline bool operator<(const Tank& a,const Tank& b)
//The priority is HIGHER if the cost is LOWER
{
  return a.cost > b.cost;
}

typedef unsigned char board_t[MAX_BATTLEFIELD_DIM][MAX_BATTLEFIELD_DIM];
typedef board_t obstacles_t[4];

//POD structure
class PlayoutState {
public:
	board_t board;
	int tickno;
	int command[4]; //commands given to the tanks (index is the same as tank[4])
	int tank_priority[4]; //order in which tanks should be moved
	TankState tank[4]; //Tank 0-1 belongs to PLAYER0, Tank 2-3 belongs to PLAYER1
	BulletState bullet[4]; //(index is the same as tank[4])
	BaseState base[2]; //Base 0 belongs to PLAYER0, Base 1 belongs to PLAYER1
	int min_x,min_y;
	int max_x,max_y;
	bool gameover;
	bool stop_playout;
	double state_score;
	double winner;
	int endgame_tick;
	void drawTanks();
	void drawTinyTanks();
	void drawBases();
	void drawBullets();
	void moveBullets();
	void moveTanks();
	void fireTanks();
	void checkCollisions();
	void checkDestroyedBullets();
	void move(Move& m);
	void simulateTick();
	double playout(sfmt_t* sfmt, UtilityScores& utility);
	void updateCanFire();
	bool insideBounds(const int x, const int y);
	bool isTankInsideBounds(const int x, const int y);
	bool canRotate(const int x, const int y, const int o, board_t& obstacles);
	bool clearFireTrajectory(int x, int y, int o, int t_x, int t_y, board_t& obstacles);
	bool clearBallisticTrajectory(int x, int y, int o, int t_x, int t_y, board_t& obstacles);
	bool clearPath(int x, int y, int o, board_t& obstacles);
	bool canMove(int x, int y, int o, board_t& obstacles);
	bool clearPath(int x, int y, int o);
	bool clearablePath(int x, int y, int o, board_t& obstacles);
	bool clearablePath(int x, int y, int o);
	bool insideTank(const unsigned int t, const int x, const int y);
	bool insideAnyTank(const int x, const int y);
	bool insideTinyTank(const int t, const int x, const int y);
	bool onBase(const int b, const int x, const int y);
	bool onTarget(const int t, const int x, const int y);
	bool onFriendly(const int t, const int x, const int y);
	bool incomingBullet(const int x, const int y, const int o);
	bool isTankAt(const int t, const int x, const int y);
	bool isTankInFiringLine(const int t, board_t& obstacles);
	void drawTank(const int t, const int block);
	void drawTankObstacle(const int t, board_t& obstacles);
	void drawTankObstacle(const int x, const int y, board_t& obstacles);
	void drawTinyTank(const int t, const int block);
	void seedBase(const int player, priority_queue<Tank>& frontier, board_t& obstacles);
	void findPath(priority_queue<Tank>& frontier, costmatrix_t& costmatrix, board_t& obstacles);
	void updateSimpleUtilityScores(UtilityScores& utility, obstacles_t& obstacles);
	void updateExpensiveUtilityScores(UtilityScores& utility, obstacles_t& obstacles);
	bool lineOfSight(const int sx, const int sy, const int o, const int tx, const int ty);
	void save();
	int bestOCMD(int x, int y, costmatrix_t& costmatrix, scored_cmds_t& cmds);
	int bestOCMD(int x, int y, costmatrix_t& costmatrix, board_t& obstacles, scored_cmds_t& cmds);
	int bestOCMDDodgeCanfire(int t, board_t& obstacles, scored_cmds_t& cmds);
	int bestOCMDDodgeCantfire(int t, board_t& obstacles, scored_cmds_t& cmds);
	int bestO(int x, int y, costmatrix_t& costmatrix);
	//int cmdToSimpleUtility(int c, int t);
	int bestC(int tank_id, costmatrix_t& costmatrix, scored_cmds_t& cmds);
	int bestCExpensive(int tank_id, costmatrix_t& costmatrix, board_t& obstacles, scored_cmds_t& cmds);
	//int cmdToExpensiveUtility(int c, int t);
	//friend ostream &operator<<(ostream &output, const PlayoutState &p);
	//friend istream &operator>>(istream  &input, PlayoutState &p);
	void paintUtilityScores(UtilityScores& utility);
	void paint();
	void paintObstacles(board_t& obstacles);
};

ostream &operator<<(ostream &output, const PlayoutState &p);
istream &operator>>(istream  &input, PlayoutState &p);
#endif /* PLAYOUTSTATE_H_ */
