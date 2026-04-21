#include <bits/stdc++.h>
using namespace std;

struct Submission {
    int time;
    int prob; // 0..M-1
    string status; // Accepted, Wrong_Answer, Runtime_Error, Time_Limit_Exceed
};

struct ProblemState {
    int wrong_before = 0;      // wrong attempts before first AC (revealed so far)
    bool solved = false;
    int solve_time = 0;        // time of first AC (revealed so far)

    // Freeze-cycle tracking
    int wrong_before_freeze = 0;                 // snapshot at FREEZE time
    vector<pair<string,int>> frozen_events;      // submissions after FREEZE for this cycle
    bool is_frozen = false;                      // had post-freeze submissions while unsolved at freeze
};

struct Team {
    string name;
    int id;
    vector<Submission> subs;   // all submissions history

    vector<ProblemState> probs; // size M

    // Cached scoring state for current revealed info
    int solved_count = 0;
    long long penalty = 0;
    vector<int> solve_times; // ascending order

    int last_rank = 0; // rank after last FLUSH (or initial lex order before first flush)
};

struct SystemState {
    bool started = false;
    bool frozen = false;
    int duration = 0;
    int M = 0; // problems count

    vector<Team> teams;
    unordered_map<string,int> team_index;
};

// Recompute a team's aggregate metrics from per-problem revealed state
static void recompute_team(Team& t, int M){
    t.solved_count = 0;
    t.penalty = 0;
    t.solve_times.clear();
    for(int p=0;p<M;p++){
        const auto &ps = t.probs[p];
        if(ps.solved){
            t.solved_count++;
            t.penalty += 20LL * ps.wrong_before + ps.solve_time;
            t.solve_times.push_back(ps.solve_time);
        }
    }
    sort(t.solve_times.begin(), t.solve_times.end()); // ascending
}

struct RankEntry{int idx;};

static bool rank_cmp_idx(int ai, int bi, const vector<Team>& teams){
    const Team &a = teams[ai], &b = teams[bi];
    if(a.solved_count != b.solved_count) return a.solved_count > b.solved_count;
    if(a.penalty != b.penalty) return a.penalty < b.penalty;
    // tie-breaker: compare maximum solve time, then second maximum, ... smaller is better
    const auto &ta = a.solve_times, &tb = b.solve_times;
    int na = (int)ta.size(), nb = (int)tb.size();
    int n = max(na, nb);
    for(int k=1;k<=n;k++){
        int va = (na - k >= 0 ? ta[na - k] : INT_MAX);
        int vb = (nb - k >= 0 ? tb[nb - k] : INT_MAX);
        if(va != vb) return va < vb; // smaller is better
    }
    return a.name < b.name;
}

static vector<int> order_by_rank(const SystemState& S){
    vector<int> idx(S.teams.size());
    iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(), [&](int a, int b){ return rank_cmp_idx(a,b,S.teams); });
    return idx;
}

static void flush_scoreboard(SystemState& S){
    for(auto &t : S.teams) recompute_team(t, S.M);
    auto ord = order_by_rank(S);
    for(size_t i=0;i<ord.size();++i) S.teams[ord[i]].last_rank = (int)i+1;
}

static void output_scoreboard(const SystemState& S){
    auto ord = order_by_rank(S);
    for(size_t pos=0; pos<ord.size(); ++pos){
        const Team &t = S.teams[ord[pos]];
        cout << t.name << ' ' << (pos+1) << ' ' << t.solved_count << ' ' << t.penalty;
        for(int p=0;p<S.M;p++){
            const auto &ps = t.probs[p];
            cout << ' ';
            if(S.frozen && ps.is_frozen){
                int x = ps.wrong_before_freeze;
                int y = (int)ps.frozen_events.size();
                if(x==0) cout << "0/" << y;
                else cout << '-' << x << '/' << y;
            } else {
                if(ps.solved){
                    if(ps.wrong_before==0) cout << '+';
                    else cout << '+' << ps.wrong_before;
                } else {
                    if(ps.wrong_before==0) cout << '.';
                    else cout << '-' << ps.wrong_before;
                }
            }
        }
        cout << '\n';
    }
}

static void set_initial_lex_ranks(SystemState& S){
    vector<int> idx(S.teams.size());
    iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(), [&](int a, int b){ return S.teams[a].name < S.teams[b].name; });
    for(size_t i=0;i<idx.size();++i) S.teams[idx[i]].last_rank = (int)i+1;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    SystemState S;

    string cmd;
    while (cin >> cmd){
        if(cmd=="ADDTEAM"){
            string name; cin >> name;
            if(S.started){
                cout << "[Error]Add failed: competition has started.\n";
            } else {
                if(S.team_index.count(name)){
                    cout << "[Error]Add failed: duplicated team name.\n";
                } else {
                    Team t; t.name = name; t.id = (int)S.teams.size();
                    S.teams.push_back(t);
                    S.team_index[name] = t.id;
                    cout << "[Info]Add successfully.\n";
                }
            }
        } else if(cmd=="START"){
            string tmp; int duration; int pc;
            cin >> tmp >> duration >> tmp >> pc; // DURATION, PROBLEM
            if(S.started){
                cout << "[Error]Start failed: competition has started.\n";
            } else {
                S.started = true; S.duration = duration; S.M = pc;
                for(auto &t : S.teams){ t.probs.assign(S.M, ProblemState()); }
                // Initialize last_rank as lex order before first FLUSH
                set_initial_lex_ranks(S);
                cout << "[Info]Competition starts.\n";
            }
        } else if(cmd=="SUBMIT"){
            string prob, tmp, team, tmp2, status, tmp3; int tm;
            cin >> prob >> tmp >> team >> tmp2 >> status >> tmp3 >> tm; // BY, WITH, AT
            int ti = S.team_index[team];
            Submission sub{tm, prob[0]-'A', status};
            S.teams[ti].subs.push_back(sub);

            auto &ps = S.teams[ti].probs[sub.prob];
            if(S.frozen){
                // Only track freezes for problems that were unsolved at FREEZE
                if(!ps.solved){
                    ps.is_frozen = true;
                    ps.frozen_events.emplace_back(sub.status, sub.time);
                }
            } else {
                if(!ps.solved){
                    if(sub.status=="Accepted"){
                        ps.solved = true; ps.solve_time = sub.time;
                    } else {
                        ps.wrong_before++;
                    }
                }
            }
        } else if(cmd=="FLUSH"){
            cout << "[Info]Flush scoreboard.\n";
            flush_scoreboard(S);
        } else if(cmd=="FREEZE"){
            if(S.frozen){
                cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            } else {
                S.frozen = true;
                for(auto &t : S.teams){
                    for(int p=0;p<S.M;p++){
                        auto &ps = t.probs[p];
                        ps.wrong_before_freeze = ps.wrong_before;
                        ps.frozen_events.clear();
                        ps.is_frozen = false; // will turn true when receiving post-freeze submissions while unsolved
                    }
                }
                cout << "[Info]Freeze scoreboard.\n";
            }
        } else if(cmd=="SCROLL"){
            if(!S.frozen){
                cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            } else {
                cout << "[Info]Scroll scoreboard.\n";
                // Flush before scrolling as required
                flush_scoreboard(S);
                // Output scoreboard before scrolling (with frozen view)
                output_scoreboard(S);

                // Prepare ranking order for change detection
                auto current_order = order_by_rank(S);

                // Unfreeze until no frozen problems remain
                while(true){
                    // Pick lowest-ranked team with any frozen problems (has events)
                    int chosen_team = -1, chosen_prob = -1;
                    for(int i=(int)current_order.size()-1; i>=0; --i){
                        int ti = current_order[i];
                        for(int p=0;p<S.M;p++){
                            auto &ps = S.teams[ti].probs[p];
                            if(ps.is_frozen && !ps.frozen_events.empty()){
                                chosen_team = ti; chosen_prob = p; break;
                            }
                        }
                        if(chosen_team!=-1) break;
                    }
                    if(chosen_team==-1) break; // done

                    // Position before unfreeze
                    int old_pos = find(current_order.begin(), current_order.end(), chosen_team) - current_order.begin();

                    // Apply unfreeze for this problem
                    auto &team = S.teams[chosen_team];
                    auto &ps = team.probs[chosen_prob];
                    int wrong_added = 0;
                    int accept_time = -1;
                    for(auto &ev : ps.frozen_events){
                        if(accept_time==-1 && ev.first=="Accepted"){
                            accept_time = ev.second;
                            break; // first AC stops
                        } else if(accept_time==-1){
                            wrong_added++;
                        }
                    }
                    ps.wrong_before += wrong_added;
                    if(!ps.solved && accept_time!=-1){
                        ps.solved = true;
                        ps.solve_time = accept_time;
                    }
                    // Clear frozen flags for this problem
                    ps.frozen_events.clear();
                    ps.is_frozen = false;

                    // Recompute this team's aggregates only (others unchanged)
                    recompute_team(team, S.M);

                    // New order and detect movements upward; others maintain relative order
                    auto new_order = order_by_rank(S);
                    int new_pos = find(new_order.begin(), new_order.end(), chosen_team) - new_order.begin();
                    if(new_pos < old_pos){
                        // Output a line for each position ascended, from old_pos-1 down to new_pos
                        for(int p = old_pos-1; p>=new_pos; --p){
                            int replaced_team_idx = current_order[p];
                            cout << team.name << ' ' << S.teams[replaced_team_idx].name << ' '
                                 << team.solved_count << ' ' << team.penalty << "\n";
                        }
                    }
                    // Update current order for next selection
                    current_order = move(new_order);
                }

                // Output scoreboard after scrolling (no additional flush per spec)
                output_scoreboard(S);

                // Lift frozen state
                S.frozen = false;
            }
        } else if(cmd=="QUERY_RANKING"){
            string name; cin >> name;
            auto it = S.team_index.find(name);
            if(it==S.team_index.end()){
                cout << "[Error]Query ranking failed: cannot find the team.\n";
            } else {
                cout << "[Info]Complete query ranking.\n";
                if(S.frozen){
                    cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
                }
                cout << name << " NOW AT RANKING " << S.teams[it->second].last_rank << "\n";
            }
        } else if(cmd=="QUERY_SUBMISSION"){
            string team, where, probkv, AND, statkv;
            cin >> team >> where >> probkv >> AND >> statkv; // WHERE, PROBLEM=*, AND, STATUS=*
            auto it = S.team_index.find(team);
            if(it==S.team_index.end()){
                cout << "[Error]Query submission failed: cannot find the team.\n";
            } else {
                string probq = probkv.substr(strlen("PROBLEM="));
                string statusq = statkv.substr(strlen("STATUS="));
                const Team &t = S.teams[it->second];
                const Submission* last = nullptr;
                for(const auto &s : t.subs){
                    bool okp = (probq=="ALL" || (char)('A'+s.prob)==probq[0]);
                    bool oks = (statusq=="ALL" || s.status==statusq);
                    if(okp && oks){
                        if(!last || s.time >= last->time) last = &s;
                    }
                }
                cout << "[Info]Complete query submission.\n";
                if(!last) cout << "Cannot find any submission.\n";
                else cout << t.name << ' ' << char('A'+last->prob) << ' ' << last->status << ' ' << last->time << "\n";
            }
        } else if(cmd=="END"){
            cout << "[Info]Competition ends.\n";
            break;
        }
    }
    return 0;
}
