#define _CRT_SECURE_NO_WARNINGS 1
#pragma warning(disable: 4996)

#define _CRTDBG_MAP_ALLOC

#include <iostream>
#include <crtdbg.h>

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define ACT     32
#define RES     4
#define SUC     10
#define MAX_T   500     /* safe upper bound on project makespan */

int  n, m, a[RES], d[ACT], r[ACT][RES], nrsu[ACT], su[ACT][SUC];
int  nrpr[ACT], pr[ACT][SUC], of[ACT];
int  reach[ACT][ACT];          /* reach[i][j] = 1  iff  j is a successor of i */
double allowed_time;
char   choice[4];

/* INPUT (similar to hw2.cpp) */

int inputFromPath(const char* path) {
    int i1, i2, i3;  FILE* fp;
    fp = fopen(path, "r");  if (fp == NULL) return 0;
    fscanf(fp, "%d %d", &n, &m);  if (n > ACT || m > RES) return 0;
    for (i1 = 0; i1 < m; i1++) fscanf(fp, "%d", a + i1);
    for (i1 = 0; i1 < n; i1++) nrpr[i1] = 0;
    for (i1 = 0; i1 < n; i1++) {
        fscanf(fp, "%d", d + i1);
        for (i2 = 0; i2 < m; i2++) {
            fscanf(fp, "%d", r[i1] + i2);  if (r[i1][i2] > a[i2]) return 0;
        }
        fscanf(fp, "%d", nrsu + i1);
        for (i2 = 0; i2 < nrsu[i1]; i2++) {
            fscanf(fp, "%d", &i3);  if (i3 <= i1 || i3 > n) return 0;
            su[i1][i2] = --i3;  pr[i3][nrpr[i3]++] = i1;
        }
    }
    fclose(fp);  return 1;
}

/* BUILD REACHABILITY MATRIX
   reach[i][j] = 1 means activity j is a direct or indirect
   successor of activity i  */

void buildReachability(void) {
    int i, j, k;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            reach[i][j] = 0;
    /* seed with direct successor edges */
    for (i = 0; i < n; i++)
        for (j = 0; j < nrsu[i]; j++)
            reach[i][su[i][j]] = 1;
    /* transitive closure */
    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            if (reach[i][k])
                for (j = 0; j < n; j++)
                    if (reach[k][j]) reach[i][j] = 1;
}

/* COMPUTE LATEST START TIMES  (CPM, ignoring resources)
   Forward pass  ? EST / EFT
   Backward pass ? LFT ? LST
   Input indices are guaranteed topologically sorted (enforced
   by input() check: successor index > current index). */

void computeLST(int* lst) {
    int est[ACT], eft[ACT], lft[ACT];
    int i, j, T;

    /* forward pass */
    for (i = 0; i < n; i++) {
        est[i] = 0;
        for (j = 0; j < nrpr[i]; j++)
            if (eft[pr[i][j]] > est[i]) est[i] = eft[pr[i][j]];
        eft[i] = est[i] + d[i];
    }
    T = eft[n - 1];   /* resource-free project duration (sink finish) */

    /* backward pass */
    for (i = 0; i < n; i++) lft[i] = T;
    for (i = n - 2; i >= 0; i--)
        for (j = 0; j < nrsu[i]; j++) {
            int s = su[i][j];
            int s_lst = lft[s] - d[s];
            if (s_lst < lft[i]) lft[i] = s_lst;
        }

    for (i = 0; i < n; i++) lst[i] = lft[i] - d[i];
}

/* GENERATE INITIAL SOLUTION  —  smart LST-priority topological sort
   Uses Kahn's algorithm: at each step, among all activities whose
   predecessors are already placed, pick the one with the smallest
   LST (most time-critical first).  */

void generateInitialSolution(int* seq) {
    int lst[ACT], in_deg[ACT], sched[ACT], ready[ACT];
    int pos, i, nr_ready, best_act, best_lst;

    computeLST(lst);
    for (i = 0; i < n; i++) { in_deg[i] = nrpr[i]; sched[i] = 0; }

    for (pos = 0; pos < n; pos++) {
        nr_ready = 0;
        for (i = 0; i < n; i++)
            if (!sched[i] && in_deg[i] == 0) ready[nr_ready++] = i;

        best_act = ready[0];  best_lst = lst[ready[0]];
        for (i = 1; i < nr_ready; i++)
            if (lst[ready[i]] < best_lst) { best_lst = lst[ready[i]]; best_act = ready[i]; }

        seq[pos] = best_act;
        sched[best_act] = 1;
        for (i = 0; i < nrsu[best_act]; i++) in_deg[su[best_act][i]]--;
    }
}

/*GENERATE RANDOM SOLUTION  —  random topological sort (Kahn's)
   Used for SA restarts when temperature hits the floor. */

void generateRandomSolution(int* seq) {
    int in_deg[ACT], sched[ACT], ready[ACT];
    int pos, i, nr_ready, chosen;

    for (i = 0; i < n; i++) { in_deg[i] = nrpr[i]; sched[i] = 0; }

    for (pos = 0; pos < n; pos++) {
        nr_ready = 0;
        for (i = 0; i < n; i++)
            if (!sched[i] && in_deg[i] == 0) ready[nr_ready++] = i;

        chosen = ready[rand() % nr_ready];
        seq[pos] = chosen;
        sched[chosen] = 1;
        for (i = 0; i < nrsu[chosen]; i++) in_deg[su[chosen][i]]--;
    }
}

/* SERIAL SCHEDULE GENERATION SCHEME  (Serial SGS)
   Decodes an activity list into a schedule.
   For each activity (in list order):
     1. EST = max finish time of all predecessors.
     2. Scan forward from EST to find the earliest t where
        all required resources are free for the full duration.
     3. Book the activity at t, update resource profile.
   Fills the global of[] array with finish times.
   Returns the project makespan (finish time of sink activity).
    */

int SGS(int* seq) {
    int finish_a[ACT];
    int res_use[MAX_T][RES];
    int i, j, k, t, tau, act, feasible;

    for (i = 0; i < ACT; i++)
        finish_a[i] = 0;
    for (i = 0; i < MAX_T; i++)
        for (k = 0; k < RES; k++)
            res_use[i][k] = 0;

    for (i = 0; i < n; i++) {
        act = seq[i];

        /* step 1: earliest start from precedence */
        t = 0;
        for (j = 0; j < nrpr[act]; j++)
            if (finish_a[pr[act][j]] > t) t = finish_a[pr[act][j]];

        /* step 2: try each candidate start time t from EST upward.
           outer loop: advances t one step at a time until a feasible slot is found.
           inner loops: check every time slot in [t, t+d[act]) against every resource. */

        if (d[act] > 0) {
            feasible = 0;
            while (!feasible) {
                feasible = 1;
                for (tau = t; tau < t + d[act]; tau++) {
                    for (k = 0; k < m; k++) {
                        if (res_use[tau][k] + r[act][k] > a[k]) {
                            feasible = 0;
                        }
                    }
                }
                if (!feasible) t++;
            }
        }

        /* step 3: book */
        finish_a[act] = t + d[act];
        for (tau = t; tau < t + d[act]; tau++)
            for (k = 0; k < m; k++)
                res_use[tau][k] += r[act][k];
    }

    for (i = 0; i < n; i++) of[i] = finish_a[i];
    return finish_a[n - 1];   /* sink (index n-1) has d=0, so finish = start = makespan */
}

/* PRECEDENCE-FEASIBILITY CHECK FOR A SWAP
   Swapping positions pi < pj in seq is feasible iff:
     • seq[pi] is not a predecessor of seq[pj]  (direct or indirect)
     • For every position k strictly between pi and pj:
         – seq[pi] is not a predecessor of seq[k]
           (moving seq[pi] to position pj would put it AFTER seq[k])
         – seq[k]  is not a predecessor of seq[pj]
           (moving seq[pj] to position pi would put it BEFORE seq[k])
 */

int isFeasibleSwap(int* seq, int pi, int pj) {
    int k;
    if (reach[seq[pi]][seq[pj]]) return 0;
    for (k = pi + 1; k < pj; k++) {
        if (reach[seq[pi]][seq[k]]) return 0;
        if (reach[seq[k]][seq[pj]]) return 0;
    }
    return 1;
}

/* TABU SEARCH
   ? Representation: activity list (0-based indices)
   ? Neighbourhood: all precedence-feasible pairwise swaps
   ? Selection: steepest descent on adjusted delta
   ? Frequency penalty: adjusted_delta = raw_delta + freq[a][b] / 3 ? frequently-used swaps are penalised
   ? Tabu list  : move-based; stores the iteration at which the
       tabu on pair (a,b) expires.  Indexed on ACTIVITY pairs (not
       positions) for consistency across iterations.
   ? Dynamic tenure  : random integer in [sqrt(n), 2*sqrt(n)]
   ? Aspiration : a tabu move is accepted if it yields a new global best makespan.
  */

void TS(double time_limit) {
    int  cur_seq[ACT], best_seq[ACT];
    int  best_makespan, cur_makespan, new_makespan;
    int  tabu_exp[ACT][ACT];   /* tabu_exp[a][b] = iteration at which tabu expires */
    double freq[ACT][ACT];     /* how many times activity-pair (a,b) has been swapped */
    int  i, j, k, iter;
    double elapsed;
    clock_t ts_start;
    int  sq_n;

    for (i = 0; i < ACT; i++)
        for (j = 0; j < ACT; j++)
            tabu_exp[i][j] = 0;
    for (i = 0; i < ACT; i++)
        for (j = 0; j < ACT; j++)
            freq[i][j] = 0.0;

    generateInitialSolution(cur_seq);
    cur_makespan = SGS(cur_seq);
    for (k = 0; k < n; k++) best_seq[k] = cur_seq[k];
    best_makespan = cur_makespan;

    sq_n = (int)sqrt((double)n);
    ts_start = clock();
    iter = 0;

    while (1) {
        elapsed = (double)(clock() - ts_start) / CLOCKS_PER_SEC;
        if (elapsed >= time_limit) break;
        iter++;

        /* steepest descent over all feasible swaps  */
        int    best_pi = -1, best_pj = -1, best_raw_ms = 0;
        double best_adj = 1e18;

        for (i = 0; i < n - 1; i++) {
            for (j = i + 1; j < n; j++) {
                if (!isFeasibleSwap(cur_seq, i, j)) continue;

                int aa = cur_seq[i], ab = cur_seq[j];
                int fa = (aa < ab) ? aa : ab;   /* canonical pair (smaller index first) */
                int fb = (aa < ab) ? ab : aa;

                /* temporary swap ? evaluate ? undo */
                cur_seq[i] = ab;  cur_seq[j] = aa;
                new_makespan = SGS(cur_seq);
                cur_seq[i] = aa;  cur_seq[j] = ab;

                int    raw_delta = new_makespan - cur_makespan;
                double adj_delta = raw_delta + freq[fa][fb] / 3.0;

                int is_tabu = (tabu_exp[fa][fb] > iter);
                int aspiration = (new_makespan < best_makespan);

                if ((!is_tabu || aspiration) && adj_delta < best_adj) {
                    best_adj = adj_delta;
                    best_pi = i;
                    best_pj = j;
                    best_raw_ms = new_makespan;
                }
            }
        }

        if (best_pi == -1) continue;   /* no feasible move found */

        /* commit best move */
        int aa = cur_seq[best_pi], ab = cur_seq[best_pj];
        int fa = (aa < ab) ? aa : ab;
        int fb = (aa < ab) ? ab : aa;

        cur_seq[best_pi] = ab;
        cur_seq[best_pj] = aa;
        cur_makespan = SGS(cur_seq);   /* re-evaluates and refreshes of[] */

        /* update frequency and tabu tenure */
        freq[fa][fb] += 1.0;
        tabu_exp[fa][fb] = iter + sq_n + rand() % (sq_n + 1);

        /* update global best */
        if (cur_makespan < best_makespan) {
            best_makespan = cur_makespan;
            for (k = 0; k < n; k++) best_seq[k] = cur_seq[k];
        }
    }

    /* restore of[] to reflect the best solution */
    for (k = 0; k < n; k++) cur_seq[k] = best_seq[k];
    SGS(cur_seq);

    printf("\n--- Tabu Search Results ---\n");
    printf("Best sequence : ");
    for (k = 0; k < n; k++) printf("%d ", best_seq[k] + 1);   /* 1-based output */
    printf("\nBest makespan : %d\n", best_makespan);
}

/* SIMULATED ANNEALING
   ? Representation : activity list (same as TS)
   ? Move selection: single random precedence-feasible swap per step
   ? Acceptance : accept if delta <= 0 (improvement or neutral);if delta > 0, accept with probability exp(-delta / T)
   ? Cooling : geometric  T ? T × 0.8  every n iterations
   ? Initial temp: T? = 15
   ? Restart : when T < 0.001, reset T = 15 and generate
       a new random topological sort (diversification restart)
  */

void SA(double time_limit) {
    int    cur_seq[ACT], best_seq[ACT];
    int    best_makespan, cur_makespan, new_makespan;
    double T = 15.0;
    int    k, pi, pj, attempt, delta, it;
    double prob, elapsed;
    clock_t sa_start;

    generateInitialSolution(cur_seq);
    cur_makespan = SGS(cur_seq);
    for (k = 0; k < n; k++) best_seq[k] = cur_seq[k];
    best_makespan = cur_makespan;

    sa_start = clock();

    while (1) {
        elapsed = (double)(clock() - sa_start) / CLOCKS_PER_SEC;
        if (elapsed >= time_limit) break;

        /* n iterations at current temperature */
        for (it = 0; it < n; it++) {
            elapsed = (double)(clock() - sa_start) / CLOCKS_PER_SEC;
            if (elapsed >= time_limit) break;

            /* find a random precedence-feasible swap */
            pi = -1;  pj = -1;
            for (attempt = 0; attempt < n * n; attempt++) {
                int a1 = rand() % n;
                int a2 = rand() % n;
                if (a1 == a2) continue;
                if (a1 > a2) { int tmp = a1; a1 = a2; a2 = tmp; }
                if (isFeasibleSwap(cur_seq, a1, a2)) { pi = a1; pj = a2; break; }
            }
            if (pi == -1) continue;   /* no feasible swap found in budget */

            /* perform swap and evaluate */
            int aa = cur_seq[pi], ab = cur_seq[pj];
            cur_seq[pi] = ab;  cur_seq[pj] = aa;
            new_makespan = SGS(cur_seq);
            delta = new_makespan - cur_makespan;

            if (delta <= 0) {
                /* accept improvement (or equal) */
                cur_makespan = new_makespan;
                if (cur_makespan < best_makespan) {
                    best_makespan = cur_makespan;
                    for (k = 0; k < n; k++) best_seq[k] = cur_seq[k];
                }
            }
            else {
                /* accept worse solution with Boltzmann probability */
                prob = exp(-(double)delta / T);
                if ((double)rand() / RAND_MAX < prob) {
                    cur_makespan = new_makespan;   /* keep the worse solution */
                }
                else {
                    cur_seq[pi] = aa;  cur_seq[pj] = ab;   /* undo */
                }
            }
        }

        /* geometric cooling */
        T *= 0.8;

        /* restart if temperature has effectively reached zero */
        if (T < 0.001) {
            T = 15.0;
            generateRandomSolution(cur_seq);
            cur_makespan = SGS(cur_seq);
        }
    }

    /* restore of[] to reflect the best solution */
    for (k = 0; k < n; k++) cur_seq[k] = best_seq[k];
    SGS(cur_seq);

    printf("\n--- Simulated Annealing Results ---\n");
    printf("Best sequence : ");
    for (k = 0; k < n; k++) printf("%d ", best_seq[k] + 1);   /* 1-based output */
    printf("\nBest makespan : %d\n", best_makespan);
}

/* MAIN (Based on hw8_cpp)
   1. Read problem data (input).
   2. Build reachability matrix.
   3. Ask for computation time and method choice.
   4. Dispatch:
        TS  ? Tabu Search for full allowed_time
        SA  ? Simulated Annealing for full allowed_time
        A   ? TS for allowed_time/2, then SA for allowed_time/2
 */

int main(void) {
    int i1, x;
    const char* rcp_path = NULL;
    const char* method = NULL;
    int argi;

    /* Usage:
       GarvitRetake.exe --rcp <path> --time <seconds> --method TS|SA|A */
    for (argi = 1; argi < __argc; argi++) {
        if (strcmp(__argv[argi], "--rcp") == 0 && argi + 1 < __argc) {
            rcp_path = __argv[++argi];
        }
        else if (strcmp(__argv[argi], "--time") == 0 && argi + 1 < __argc) {
            allowed_time = atof(__argv[++argi]);
        }
        else if (strcmp(__argv[argi], "--method") == 0 && argi + 1 < __argc) {
            method = __argv[++argi];
        }
    }

    if (rcp_path == NULL || method == NULL || allowed_time <= 0.0) {
        printf("Usage: GarvitRetake.exe --rcp <path-to-rcp> --time <seconds> --method TS|SA|A\n");
        _CrtDumpMemoryLeaks();
        return 2;
    }

    choice[0] = method[0];
    choice[1] = '\0';

    if (!inputFromPath(rcp_path)) {
        printf("\nWrong input data\n");
        _CrtDumpMemoryLeaks();
        return 3;
    }

    buildReachability();
    srand((unsigned int)time(NULL));

    x = 0;
    if (choice[0] == 65) x = 2;   /* 'A' ? run both */

    for (i1 = 0; i1 <= x; i1++) {
        if (choice[0] == 84) TS(allowed_time);          /* 'T' */
        if (choice[0] == 83) SA(allowed_time);          /* 'S' */
        if (choice[0] == 65) {                          /* 'A' */
            if (i1 == 0) TS(allowed_time / 2.0);
            if (i1 == 1) SA(allowed_time / 2.0);
        }
    }
    _CrtDumpMemoryLeaks();
    return 0;
}
