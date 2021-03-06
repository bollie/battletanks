//============================================================================
// Name        : MCTree.cpp
// Author      : Jan Gutter
// Copyright   : To the extent possible under law, Jan Gutter has waived all
//             : copyright and related or neighboring rights to this work.
//             : For more information, go to:
//             : http://creativecommons.org/publicdomain/zero/1.0/
//             : or consult the README and COPYING files
// Description : Battletanks Monte Carlo Tree Search
//============================================================================

#include "MCTree.h"
#include <tinythread.h>
#include <string.h>
#include <SFMT.h>
#include <time.h>
#include <stdlib.h>
#include <stack>
#include <utility>
#include <algorithm>
#include <functional>
#include <iostream>
#include <iomanip>
#include <fstream>

#define DEBUG 0
#define ASSERT 0

inline double UCB1T_score_alpha(unsigned long int t_, double r_, double sigma_, double t)
{
	return r_ + sqrt(min(0.25,sigma_*sigma_+sqrt(2*log(t)/t_))*log(t)/t_);
}

inline double UCB1T_score_beta(unsigned long int t_, double r_, double sigma_, double t)
{
	return r_ - sqrt(min(0.25,sigma_*sigma_+sqrt(2*log(t)/t_))*log(t)/t_);
}

inline bool child_legalmove(tree_size_t child)
{
	return (child != THREADID_PRUNED);
}

inline bool child_unexplored(tree_size_t child)
{
	return (child == THREADID_UNEXPLORED);
}

inline bool child_explored(tree_size_t child)
{
	return (child > THREADID_PRUNED);
}

void Node::print(MCTree& tree)
{
	int i,j;

	ofstream fout("count.dat");

	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			if (child_explored(child[i][j])) {
				fout << tree.tree[child[i][j]].r.count() << " ";
			} else {
				fout << "0 ";
			}
		}
		fout << endl;
	}
	fout.close();
	fout.open("mean.dat");
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			if (child_explored(child[i][j])) {
				fout << tree.tree[child[i][j]].r.mean() << " ";
			} else {
				fout << "0.0 ";
			}
		}
		fout << endl;
	}
	fout.close();
	fout.open("variance.dat");
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			if (child_explored(child[i][j])) {
				fout << tree.tree[child[i][j]].r.variance() << " ";
			} else {
				fout << "0.0 ";
			}
		}
		fout << endl;
	}
	fout.close();

	fout.open("stddev.dat");
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			if (child_explored(child[i][j])) {
				fout << sqrt((double)tree.tree[child[i][j]].r.variance()) << " ";
			} else {
				fout << "0.0 ";
			}
		}
		fout << endl;
	}

	fout.open("confidence.dat");
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			if (child_explored(child[i][j]) && tree.tree[child[i][j]].r.count() > 30 ) {
				fout << sqrt((double)tree.tree[child[i][j]].r.variance())/sqrt((double)tree.tree[child[i][j]].r.count()) << " ";
			} else {
				fout << "0.0 ";
			}
		}
		fout << endl;
	}

}
inline int Node::alpha(MCTree& tree)
{
	unsigned int t0,t1,t2,t3;
	int i,j;
	unsigned long int t_;
	double r_;
	double sigma_;
	double score;
	double maxscore = W_PLAYER1;
	int bestmove = -1;
#if ASSERT
	if (expanded_to < 2) {
		cerr << "expanded_to < 2 !" << endl;
	}
#endif
	for (t1 = 0; t1 < expanded_to; t1++) {
		for (t0 = 0; t0 < expanded_to; t0++) {
			t_ = 0;
			r_ = 0.0;
			sigma_ = 0.0;
			i = t0+6*t1;
			for (t3 = 0; t3 < expanded_to; t3++) {
				for (t2 = 0; t2 < expanded_to; t2++) {
					j = t2+6*t3;
					//For all legal moves for P0 and P1 < width
					if (child_explored(child[i][j])) {
						Node& c = tree.tree[child[i][j]];
						t_ += c.r.count();
						if (r.mean() == 3.14) {
							cout << "Testing r.mean()" << endl;
						}
						r_ += c.r.count()*c.r.mean();
						sigma_ += c.r.count()*c.r.variance();
					}
				}
			}
			if (t_ > 0) {
				r_ /= t_;
				sigma_ /= t_;
				score = UCB1T_score_alpha(t_,r_,sigma_,r.count());
				if (score > maxscore) {
					maxscore = score;
					bestmove = i;
				}
			}
		}
	}
	return bestmove;
}

inline int Node::beta(MCTree& tree)
{
	unsigned int t0,t1,t2,t3;
	int i,j;
	unsigned long int t_;
	double r_;
	double sigma_;
	double score;
	double minscore = W_PLAYER0;
	int bestmove = -1;

	for (t3 = 0; t3 < expanded_to; t3++) {
		for (t2 = 0; t2 < expanded_to; t2++) {
			t_ = 0;
			r_ = 0.0;
			sigma_ = 0.0;
			j = t2+6*t3;
			for (t1 = 0; t1 < expanded_to; t1++) {
				for (t0 = 0; t0 < expanded_to; t0++) {
					//For all legal moves for P0 and P1 < width
					i = t0+6*t1;
					if (child_explored(child[i][j])) {
						Node& c = tree.tree[child[i][j]];
						t_ += c.r.count();
						r_ += c.r.count()*c.r.mean();
						sigma_ += c.r.count()*c.r.variance();
					}
				}
			}
			if (t_ > 0) {
				r_ /= t_;
				sigma_ /= t_;
				score = UCB1T_score_beta(t_,r_,sigma_,r.count());
				if (score < minscore) {
					minscore = score;
					bestmove = j;
				}
			}
		}
	}
	return bestmove;
}

inline void MCTree::handle_task(int taskid, int threadid) {
	expand_task_t* task = tasks+taskid;
	int command[4];
#if DEBUG > 2
	cout << "Task started on node [" << task->child_ptr << "] by thread " << threadid << endl;
#endif
	command[0] = C_T0(task->alpha,task->beta);
	command[1] = C_T1(task->alpha,task->beta);
	command[2] = C_T2(task->alpha,task->beta);
	command[3] = C_T3(task->alpha,task->beta);

#if ASSERT
	if (task->child_ptr == 0) {
		cout << "Something's wrong, child_ptr should never be 0!" << endl;
	}
#endif
	if (child_legalmove(task->child_ptr)) {
		memcpy(child_state[threadid],task->parent_state,sizeof(PlayoutState));
		memcpy(child_state[threadid]->command,command,sizeof(command));
		child_state[threadid]->simulateTick();
		if (child_state[threadid]->gameover) {
			tree[task->child_ptr].terminal = true;
		} else {
			tree[task->child_ptr].terminal = false;
			child_state[threadid]->playout(worker_sfmt[threadid],*root_u);
		}
		tree[task->child_ptr].expanded_to = 0;
		tree[task->child_ptr].r.init();
		tree[task->child_ptr].r.push(child_state[threadid]->state_score);
#if ASSERT
#if DEBUG > 2
		cout << "Node [" << task->child_ptr << "] result: " << tree[task->child_ptr].r.mean() << endl;
#endif
	} else {
		cout << "Something's wrong, a task should always be valid..." << endl;
#endif
	}
}

inline bool MCTree::taskqueue_empty()
{
	return (task_first == task_last);
}

inline void MCTree::post_result(int alpha, int beta)
{
	task_results[task_result_last].alpha = alpha;
	task_results[task_result_last].beta = beta;
	task_result_last = (task_result_last + 1) % RESULT_RING_SIZE;
}

inline unsigned int MCTree::num_results()
{
	return ((RESULT_RING_SIZE+task_result_last-task_result_first)%RESULT_RING_SIZE);
}

void expand_subnodes(void* thread_param) {
	expand_thread_param_t* parameters = static_cast<expand_thread_param_t*>(thread_param);
	MCTree* mc_tree = parameters->mc_tree;
	unsigned int threadid = parameters->threadid;
	delete parameters;
	unsigned int taskid;
	int tasks_claimed;
#if DEBUG > 1
	cout << "Thread (" << threadid << ") starting up" << endl;
#endif
	mc_tree->taskqueue_mutex.lock();
	mc_tree->workers_running++;
	bool running = mc_tree->workers_keepalive;
	mc_tree->workers_awake++;
	tasks_claimed = 0;
	while (running) {
		if (mc_tree->taskqueue_empty()) {
#if DEBUG > 1
			cout << "Thread (" << threadid << ") going to sleep" << endl;
#endif
			mc_tree->workers_awake--;
			if (!mc_tree->workers_awake && tasks_claimed > 0) {
#if DEBUG > 1
				cout << "Thread (" << threadid << ") last one awake and claimed " << tasks_claimed << " tasks, notifying main thread." << endl;
#endif
				mc_tree->task_result_mutex.lock();
				mc_tree->workers_finished.notify_one();
				mc_tree->task_result_mutex.unlock();
			}
			mc_tree->tasks_available.wait(mc_tree->taskqueue_mutex);
			tasks_claimed = 0;
			mc_tree->workers_awake++;
			running = mc_tree->workers_keepalive;
#if DEBUG > 1
			cout << "Thread (" << threadid << ") waking up" << endl;
		} else {
			cout << "Thread (" << threadid << ") work in queue" << endl;
#endif
		}
		while (!mc_tree->taskqueue_empty()) {
			//Claim next task in queue
			taskid = mc_tree->task_first;
			mc_tree->task_first = (mc_tree->task_first + 1) % TASK_RING_SIZE;
			tasks_claimed++;
			mc_tree->taskqueue_mutex.unlock();
#if DEBUG > 1
			cout << "Thread (" << threadid << ") starting work on task " << taskid << endl;
#endif
			if (!running) {
#if DEBUG > 1
				cout << "Thread (" << threadid << ") there's work, but no more keepalive" << endl;
#endif
				break;
			}
			mc_tree->handle_task(taskid,threadid);
			mc_tree->taskqueue_mutex.lock();
			mc_tree->task_result_mutex.lock();
			mc_tree->post_result(mc_tree->tasks[taskid].alpha,mc_tree->tasks[taskid].beta);
			mc_tree->task_result_mutex.unlock();
			running = mc_tree->workers_keepalive;
		}
#if DEBUG > 1
		cout << "Thread (" << threadid << ") no tasks left" << endl;
#endif
	}
	mc_tree->workers_awake--;
	mc_tree->workers_running--;
	if (!mc_tree->workers_running) {
#if DEBUG > 1
		cout << "Thread (" << threadid << ") turning off the lights..." << endl;
#endif
		mc_tree->workers_quit.notify_one();
	}
	mc_tree->taskqueue_mutex.unlock();
#if DEBUG > 1
	cout << "Thread (" << threadid << ") exiting" << endl;
#endif
}

void MCTree::expand_all(tree_size_t node_id, PlayoutState* node_state, vector<Move>& path, vector<double>& results)
{
	expand_some(6,node_id,node_state,path,results);
}

void MCTree::expand_some(unsigned char width, tree_size_t node_id, PlayoutState* node_state, vector<Move>& path, vector<double>& results)
{

	unsigned int i,j,tankid;
	unsigned int t[4];

	if (unallocated_count < ((tree_size_t)width*width*width*width)) {
		//Revert to playouts only
		if (tree[node_id].terminal) {
			//Terminal state, don't need a playout
			for (i = 0; i < ((tree_size_t)width*width*width*width); i++) {
				results.push_back(tree[node_id].r.mean());
			}
		} else {
			for (i = 0; i < (tree_size_t)width; i++) {
				memcpy(child_state[0],node_state,sizeof(PlayoutState));
				results.push_back(child_state[0]->playout(worker_sfmt[0],*root_u));
			}
		}
		//cerr << "Ran out of tree!" << endl;
		return; //Silently fail
	}
#if DEBUG > 1
	cout << "Expanding [" << node_id << "] to width: " << (int)width << endl;
#endif
	if (tree[node_id].r.count() == 1) {
		//This is the first time we're trying to expand this node. Set all children to UNEXPLORED;
		memset(&tree[node_id].child,0,sizeof(tree[node_id].child));
		//Calculate the move order.
		//node_state.populateUtilityScores(u);
		scored_cmds_t cmds;
		for (i = 0; i < 4; i++) {
			if (node_state->tank[i].active) {
				//Get the cost of the moves
				if (node_id == root_id) {
					node_state->bestCExpensive(i,root_u->expensivecost[i],root_obstacles[i],cmds);
				} else {
					node_state->bestC(i,root_u->simplecost[i/2],cmds);
				}
				for (j = 0;j < 6; j++) {
					tree[node_id].cmd_order[i][j] = (unsigned char) cmds[j].first;
				}
			} else {
				for (j = 0; j < 6; j++) {
					tree[node_id].cmd_order[i][j] = j;
				}
			}
		}
	}

#if ASSERT
	long int total_allocated_nodes = 0;
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			total_allocated_nodes += allocated_count[i][j];
		}
	}
	if ((total_allocated_nodes + unallocated_count) != (tree_size-2)) {
		cerr << "before creating tasks:" << endl;
		cerr << "tree size: " << tree_size << " t_a_n: " << total_allocated_nodes << " u_c: " << unallocated_count << endl;
	}
#endif

	int& root_alpha = path[0].alpha;
	int& root_beta = path[0].beta;
	unsigned int num_tasks = 0;
	unsigned int count_results;
	taskqueue_mutex.lock();
	for (t[1] = 0; t[1] < width; t[1]++) {
		for (t[0] = 0; t[0] < width; t[0]++) {
			for (t[3] = 0; t[3] < width; t[3]++) {
				for (t[2] = 0; t[2] < width; t[2]++) {
					i = t[0]+6*t[1];
					j = t[2]+6*t[3];
					if (child_unexplored(tree[node_id].child[i][j])) {
						for (tankid = 0; tankid < 4; tankid++) {
							if (!node_state->tank[tankid].active && t[tankid] != C_NONE) {
								tree[node_id].child[i][j] = THREADID_PRUNED;
								break;
							}
							if (!node_state->tank[tankid].canfire && t[tankid] == C_FIRE) {
								tree[node_id].child[i][j] =  THREADID_PRUNED;
								break;
							}
						}
						if (tree[node_id].child[i][j] == THREADID_PRUNED) {
							continue;
						}
						//the task returned a valid move and leads to a new leaf
						//the node is now allocated to the first alpha/beta move
						//in the chain.
						tree[node_id].child[i][j] = unallocated.front();
						allocated[root_alpha][root_beta].splice(
								allocated[root_alpha][root_beta].begin(),
								unallocated,
								unallocated.begin());
						unallocated_count--;
						allocated_count[root_alpha][root_beta]++;
						tasks[task_last].child_ptr = tree[node_id].child[i][j];
						tasks[task_last].alpha = i;
						tasks[task_last].beta = j;
						tasks[task_last].parent_state = node_state;
						task_last = (task_last + 1) % TASK_RING_SIZE;
						num_tasks++;
					}
				}
			}
		}
	}

	if (num_tasks < 20) {
		task_result_mutex.lock();
		while (task_first != task_last) {
			handle_task(task_first,0);
			post_result(tasks[task_first].alpha,tasks[task_first].beta);
			task_first = (task_first + 1) % TASK_RING_SIZE;
		}
		count_results = 0;
		while (task_result_first != task_result_last) {
			count_results++;
			expand_result_t& r = task_results[task_result_first];
			tree_size_t& child = tree[node_id].child[r.alpha][r.beta];
			if (child_legalmove(child)) {
				//store results for backprop
				results.push_back(tree[child].r.mean());
			}
#if ASSERT
			else {
				cerr << "Pruned move!";
			}
#endif
			task_result_first = (task_result_first + 1) % RESULT_RING_SIZE;
		}
		taskqueue_mutex.unlock();
		task_result_mutex.unlock();
	} else {
		taskqueue_mutex.unlock();
		tasks_available.notify_all();
		task_result_mutex.lock();
		while (num_results() != num_tasks) {
			workers_finished.wait(task_result_mutex);
		}
		count_results = 0;
		while (task_result_first != task_result_last) {
			count_results++;
			expand_result_t& r = task_results[task_result_first];
			tree_size_t& child = tree[node_id].child[r.alpha][r.beta];
			if (child_legalmove(child)) {
				//store results for backprop
				results.push_back(tree[child].r.mean());
			} else {
				cerr << "Pruned move!";
			}
			task_result_first = (task_result_first + 1) % RESULT_RING_SIZE;
		}
		task_result_mutex.unlock();
	}

#if ASSERT
	if (num_tasks != count_results) {
		cerr << "Had " << num_tasks << " tasks but only " << count_results << " results!" << endl;
	}
	total_allocated_nodes = 0;
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			total_allocated_nodes += allocated_count[i][j];
		}
	}
	if ((total_allocated_nodes + unallocated_count) != (tree_size-2)) {
		cerr << "after getting results:" << endl;
		cerr << "tree size: " << tree_size << " t_a_n: " << total_allocated_nodes << " u_c: " << unallocated_count << endl;
	}
#endif
	tree[node_id].expanded_to = max(width,tree[node_id].expanded_to);
#if DEBUG > 1
	cout << "[" << node_id << "] expanded to: " << (int)tree[node_id].expanded_to << endl;
#endif
}

void MCTree::select(unsigned char width, vector<Move>& path, tree_size_t& node_id, PlayoutState* node_state)
{
	Move m;
#if DEBUG > 1
	cout << "Select at node: " << node_id << endl;
#endif
	while (tree[node_id].expanded_to >= width && !tree[node_id].terminal) {
		m.alpha = tree[node_id].alpha(*this);
		m.beta = tree[node_id].beta(*this);
#if DEBUG > 1
		cout << "UCB1Tuned returned alpha:" << m.alpha << " beta:" << m.beta << endl;
#endif
		if (m.alpha != -1 && m.beta != -1 && child_explored(tree[node_id].child[m.alpha][m.beta])) {
			path.push_back(m);
			node_id = tree[node_id].child[m.alpha][m.beta];
			node_state->move(m);
			node_state->updateCanFire();
		} else {
			cerr << "Oops, select alpha/beta is invalid! [" << node_id << "]" << endl;
			break;
		}
#if DEBUG > 1
		cout << "Select at node [" << node_id << "] with c:" << tree[node_id].r.count() << endl;
#endif
	}
#if DEBUG > 1
	cout << "[" << node_id << "] c:" << tree[node_id].r.count() << " terminal:" << tree[node_id].terminal << endl;
#endif
}

void MCTree::redistribute(vector<Move>& path)
{
	int i,j,root_alpha,root_beta;

	//int new_total = allocated_count[alpha][beta];
	//allocated_to_root.splice(allocated_to_root.begin(),allocated[alpha][beta]);
	allocated[path[0].alpha][path[0].beta].clear();
	//allocated_count[alpha][beta] = 0;

	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			unallocated.splice(unallocated.begin(),allocated[i][j]);
			unallocated_count += allocated_count[i][j];
			allocated_count[i][j] = 0;

		}
	}

	for (root_alpha = 0; root_alpha < 36; root_alpha++) {
		for (root_beta = 0; root_beta < 36; root_beta++) {
			stack<tree_size_t> frontier;
			tree_size_t current;
			if (tree[root_id].child[root_alpha][root_beta]) {
				frontier.push(tree[root_id].child[root_alpha][root_beta]);
			}
			while (frontier.size() > 0) {
				current = frontier.top();
				frontier.pop();
				allocated[root_alpha][root_beta].push_back(current);
				allocated_count[root_alpha][root_beta]++;
				unallocated_count--;
				if (tree[current].r.count() > 1 && !tree[current].terminal) {
					for (i = 0; i < 36; i++) {
						for (j = 0; j < 36; j++) {
							if (tree[current].child[i][j]) {
								frontier.push(tree[current].child[i][j]);
							}
						}
					}
				}
			}
		}
	}
}

void MCTree::backprop(vector<Move>& path,vector<double>& result)
{
	tree_size_t node = root_id;
	for (vector<double>::iterator result_iter = result.begin(); result_iter != result.end(); ++result_iter) {
		tree[node].r.push(*result_iter);
	}
	for (vector<Move>::iterator move_iter = path.begin(); move_iter != path.end(); ++move_iter) {
		node = tree[node].child[(*move_iter).alpha][(*move_iter).beta];
		for (vector<double>::iterator result_iter = result.begin(); result_iter != result.end(); ++result_iter) {
			tree[node].r.push(*result_iter);
		}
	}
}

unsigned int MCTree::best_alpha(unsigned int greedyalpha)
{
	unsigned int alpha,beta;
	unsigned long int maxcount;
	maxcount = 0;

	double confidence[36][36];

	for (alpha = 0; alpha < 36; alpha++) {
		for (beta = 0; beta < 36; beta++) {
			tree_size_t& child_id = tree[root_id].child[alpha][beta];
			if (tree[child_id].terminal) {
				continue;
			}
			if (child_explored(child_id) && tree[child_id].r.count() > 30 ) {
				maxcount = max(tree[child_id].r.count(),maxcount);
			} else {
				confidence[alpha][beta] = 0.0;
			}
		}
	}
	for (alpha = 0; alpha < 36; alpha++) {
		for (beta = 0; beta < 36; beta++) {
			tree_size_t& child_id = tree[root_id].child[alpha][beta];
			if (tree[child_id].terminal) {
				confidence[alpha][beta] = (tree[child_id].r.mean() - 0.5) * maxcount*2;
				continue;
			}
			if (child_explored(child_id) && tree[child_id].r.count() > 30 ) {
				confidence[alpha][beta] = tree[child_id].r.count();
			} else {
				confidence[alpha][beta] = 0.0;
			}
		}
	}

	unsigned int bestalpha;
	double conf;
	double bestconf;
	double greedyconf = 0.0;
	for (beta = 0; beta < 36; beta++) {
		greedyconf += confidence[greedyalpha][beta];
	}
	bestalpha = greedyalpha;
	bestconf = greedyconf;
	for (alpha = 0; alpha < 36; alpha++) {
		conf = 0;
		for (beta = 0; beta < 36; beta++) {
			conf += confidence[alpha][beta];
		}
		if (conf > bestconf) {
			bestconf = conf;
			bestalpha = alpha;
		}
	}
#if DEBUG
	cout << "Greedy confidence: " << greedyconf << endl;
	cout << "MCTS confidence: " << bestconf << endl;
#endif
	if ((bestconf - greedyconf) > MCTS_CONF_MARGIN) {
#if DEBUG
		cout << "Chose MCTS" << endl;
#endif
		return bestalpha;
	} else {
#if DEBUG
		cout << "Chose Greedy" << endl;
#endif
		return greedyalpha;
	}
}

void MCTree::init(PlayoutState* reference_state)
{
	vector<Move> path;
	vector<double> results;
	Move zero;
	unsigned int i,j;

	unallocated.clear();
	unallocated_count = tree_size-2; //0 is reserved and 1 belongs to root
	for (i = 2; i < tree_size; i++) {
		unallocated.push_back(i);
	}
	memset(allocated_count,0,sizeof(allocated_count));
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			allocated[i][j].clear();
		}
	}

	root_id = 1;
	allocated_to_root.push_back(root_id);
	memcpy(root_state,reference_state,sizeof(PlayoutState));
	root_state->drawBases();
	root_state->drawTanks();
	root_state->drawBullets();
	root_state->updateCanFire();
	root_state->updateSimpleUtilityScores(*root_u,root_obstacles);
	root_state->updateExpensiveUtilityScores(*root_u,root_obstacles);
	memcpy(child_state[0],root_state,sizeof(PlayoutState));
	tree[root_id].r.init();
	tree[root_id].r.push(child_state[0]->playout(worker_sfmt[0],*root_u));
	tree[root_id].terminal = false;
	zero.alpha = 0;
	zero.beta = 0;
	path.push_back(zero);
	tree[root_id].expanded_to = 0;
	//no need to select when priming root
	expand_all(root_id,root_state,path,results);
	//only necessary to redistribute when priming root or promoting a node to root
	redistribute(path);
	//Only backprop to root!
	path.clear();
	backprop(path,results);
}

void MCTree::reset(PlayoutState* reference_state)
{
	vector<Move> path;
	vector<double> results;
	Move zero;
	unsigned int i,j;

	unallocated_count = tree_size-2; //0 is reserved and 1 belongs to root
	memset(allocated_count,0,sizeof(allocated_count));
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			unallocated.splice(unallocated.begin(),
					allocated[i][j]);
		}

	}

	memcpy(root_state,reference_state,sizeof(PlayoutState));
	root_state->drawBases();
	root_state->drawTanks();
	root_state->drawBullets();
	root_state->updateCanFire();
	root_state->updateSimpleUtilityScores(*root_u,root_obstacles);
	root_state->updateExpensiveUtilityScores(*root_u,root_obstacles);
	memcpy(child_state[0],root_state,sizeof(PlayoutState));
	tree[root_id].r.init();
	tree[root_id].r.push(child_state[0]->playout(worker_sfmt[0],*root_u));
	tree[root_id].terminal = false;
	zero.alpha = 0;
	zero.beta = 0;
	path.push_back(zero);
	tree[root_id].expanded_to = 0;
	//no need to select when priming root
	expand_all(root_id,root_state,path,results);
	//only necessary to redistribute when priming root or promoting a node to root
	redistribute(path);
	//Only backprop to root!
	path.clear();
	backprop(path,results);
}


MCTree::MCTree()
{
	tree_size_t i;

	//TODO: figure out a good value for tree_size
	//tree_size = 100000l;
	tree_size = 100000l;
	tree = new Node[tree_size];
	unallocated_count = tree_size-2; //0 is reserved and 1 belongs to root
	for (i = 2; i < tree_size; i++) {
		unallocated.push_back(i);
	}
	memset(allocated_count,0,sizeof(allocated_count));
	root_state = new PlayoutState;
	root_u = new UtilityScores;
	root_id = 1;
	allocated_to_root.push_back(root_id);
	num_workers = min(tthread::thread::hardware_concurrency(),MAXTHREADS);
	num_workers = max(num_workers,MINTHREADS);
	workers_keepalive = true;
	srand((unsigned int)(time(NULL)));
	//workqueue_mutex.lock();
	workers_awake = 0;
	task_first = 0;
	task_last = 0;
	task_result_first = 0;
	task_result_last = 0;
	workers_running = 0;
	workers_awake = 0;
	//workqueue_mutex.unlock();
	for (i = 0; i < num_workers; i++) {
		PlayoutState* child = new PlayoutState;
		child_state.push_back(child);
		expand_thread_param_t* expand_param = new expand_thread_param_t;
		expand_param->threadid = i;
		expand_param->mc_tree = this;
		sfmt_t* sfmt = new sfmt_t;
		sfmt_init_gen_rand(sfmt, rand());
		worker_sfmt.push_back(sfmt);
		expand_worker[i] = new tthread::thread(expand_subnodes,expand_param);
	}

}

MCTree::~MCTree()
{
	tree_size_t i;
	taskqueue_mutex.lock();
	workers_keepalive = false;
	taskqueue_mutex.unlock();
	tasks_available.notify_all();

	taskqueue_mutex.lock();
	while (workers_running) {
		workers_quit.wait(taskqueue_mutex);
	}
	taskqueue_mutex.unlock();

	for (i = 0; i < num_workers; i++) {
		expand_worker[i]->join();
		delete expand_worker[i];
		delete child_state[i];
		delete worker_sfmt[i];
	}
	delete[] tree;
	delete root_state;
	delete root_u;
}
