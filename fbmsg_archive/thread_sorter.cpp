/*
 ID: bney81
 PROG: thread_sorter
 LANG: C++11
*/
#include <algorithm>
#include <cmath>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#define tstamp std::tuple<int,int,int,int,bool,int,int,bool>//year,month(jan=1),day(1=1),dofw(sun=0),pm?,hh(12=0),mm,pst?
/*#define str_list std::vector<std::string>
#define dict std::map<str_list, int>
#define dictd std::map<str_list, double>*/
#define dict std::map<std::string, int>
#define dictd std::map<std::string, double>
/*#define IFNAME "test_threads.txt"
#define PFNAME "test_threads_processed.txt"
#define OFNAME "test_thread_list.txt"*/
#define IFNAME "messages.txt"
#define PFNAME "messages_processed.txt"
#define OFNAME "thread_list.txt"
#define AFNAME "analysis_dump.txt"
/*#define PFNAME "test_threads_processed_test.txt"
#define OFNAME "test_thread_list_test.txt"*/
#define DELIM char(31)//instances of this character in the file are replaced with char(30)

const std::string REP[][2]{
    {"&quot;", "\""},
    {"&#039;", "'"},
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&#064;", "@"},
    {"&#123;", "{"},
    {"&#125;", "}"},
    {"&amp;", "&"}//ampersand should be replaced last
};
const std::string DOFWS[7]{"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
const std::string MONTHS[12]{"January","February","March","April","May","June","July","August","September","October","November","December"};
const int DAYS[12]{31,28,31,30,31,30,31,31,30,31,30,31};//must check leap day

void replace_substrs(std::string& s, std::string sub, std::string rep) {
    size_t nPos = s.find(sub);
    while (nPos != std::string::npos)
        nPos = s.replace(nPos, sub.size(), rep).find(sub, nPos);
}

void print_ts(std::ostream& stream, tstamp ts, char ch = 0) {
    stream << std::get<0>(ts) << ' '
           << std::get<1>(ts) << ' '
           << std::get<2>(ts) << ' '
           << std::get<3>(ts) << ' '
           << std::get<4>(ts) << ' '
           << std::get<5>(ts) << ' '
           << std::get<6>(ts) << ' '
           << std::get<7>(ts) << ch;
}

struct message {
    tstamp ts;
    std::string sender, msg;
    friend bool operator<(const message& l, const message& r) {
        return l.ts < r.ts;
    }
};

struct thread {
    std::string name;
    std::vector<message> msg_list;
    friend bool operator>(const thread& l, const thread& r) {
        return l.msg_list.size() > r.msg_list.size();
    }
};

std::vector<thread> thread_list;
int message_count;
//parses a string into a list of non-empty strings separated by delim
std::vector<std::string> parse(std::string s, std::string delim) {
    std::vector<std::string> names;
    size_t aPos = 0;
    while (aPos < s.size()) {
        size_t bPos = s.find(delim, aPos);
        if (bPos == std::string::npos)
            bPos = s.size();
        std::string ss(s, aPos, bPos - aPos);
        if (!ss.empty())
            names.push_back(ss);
        aPos = bPos + delim.size();
    }
    return names;
}
//{
void preprocess() {
    std::ifstream ifs(IFNAME);
    std::ofstream ofs(PFNAME);
    std::string garbage;
    const std::string CUE = R"(class="thread">)";
    while (garbage.substr(0, CUE.size()) != CUE)
        ifs >> garbage;
    std::string pre = garbage.erase(0, CUE.size());
    while (!ifs.eof()) {
        const int SIZE = (1 << 12);
        char buffer[SIZE + 1]{};
        ifs.read(buffer, SIZE);
        std::string s = std::string(buffer);
        replace_substrs(s, std::string(1, DELIM), std::string(1, 30));
        s.insert(0, pre);
        const std::string JUNK[]{
            R"(<div class="message"><div class="message_header"><span class="user">)",
            R"(</span><span class="meta">)",
            R"(</span></div></div><p>)",
            R"(</p>)",
            R"(</div><div class="thread">)",
            R"(</div></div><div><div class="thread">)"
        };
        for (std::string ss : JUNK)
            replace_substrs(s, ss, std::string(ss != JUNK[3], DELIM));
        const int PRE = 200;//large enough to grab garbage at end of file (max ~= 170)
        pre = s.substr(s.size() - PRE);
        ofs << s.erase(s.size() - PRE);
    }
    ofs << pre.erase(pre.find('<'));
    ifs.close();
    ofs.close();
}

int process_read() {
    std::ifstream ifs(PFNAME);
    std::vector<int> deviants_list;
    std::vector<thread> ten_k;
    int deviants = 0;
    std::vector<message> chat;
    std::string chat_name;
    std::vector<std::string> names;
    while (!ifs.eof()) {
        std::string s, ss;
        std::getline(ifs, s, DELIM);
        bool flag = s.find(',') != std::string::npos, weird = 0;//weird=one-name convo
        if (!flag) {
            ifs >> ss;
            if (ss.back() != ',')
                flag = weird = 1;
        }
        if (flag) {
            //new thread
            if (!chat_name.empty()) {
                std::reverse(chat.begin(), chat.end());
                thread th{chat_name, chat};
                if (chat.size() < 10000) {
                    thread_list.push_back(th);
                    deviants_list.push_back(deviants);
                } else
                    ten_k.push_back(th);
                deviants = 0;
                chat.clear();
            }
            chat_name = s;
            names = parse(chat_name, ", ");
        }
        if (weird) {
            std::getline(ifs, s, DELIM);
            s.insert(0, ss);
        }
        if (!flag || weird) {
            //new message
            message me;
            me.sender = s;
            if (find(names.begin(), names.end(), me.sender) == names.end())
                deviants++;
            tstamp ts;
            if (ss.empty() || ss.back() != ',')
                ifs >> ss;
            std::vector<std::string> dofws;
            for (std::string dofw : DOFWS)
                dofws.push_back(dofw.substr(0, 2));
            std::get<3>(ts) = std::find(dofws.begin(), dofws.end(), ss.substr(0, 2)) - dofws.begin();
            ifs >> ss;
            std::vector<std::string> months;
            for (std::string month : MONTHS)
                months.push_back(month.substr(0, 3));
            std::get<1>(ts) = std::find(months.begin(), months.end(), ss.substr(0, 3)) - months.begin() + 1;
            char ch;
            ifs >> std::get<2>(ts) >> ss >> std::get<0>(ts) >> ss >> std::get<5>(ts) >> ch >> std::get<6>(ts) >> ss;
            if (std::get<5>(ts) == 12)
                std::get<5>(ts) = 0;
            std::get<4>(ts) = (ss[0] == 'p');
            std::getline(ifs, ss, DELIM);
            std::get<7>(ts) = (ss[1] == 'S');
            me.ts = ts;
            std::getline(ifs, me.msg, DELIM);
            for (std::string sss[2] : REP) {
                replace_substrs(me.sender, sss[0], sss[1]);
                replace_substrs(me.msg, sss[0], sss[1]);
            }
            chat.push_back(me);
        }
    }
    std::reverse(chat.begin(), chat.end());
    if (chat.size() < 10000) {
        thread_list.push_back({chat_name, chat});
        deviants_list.push_back(deviants);
    } else
        ten_k.push_back({chat_name, chat});
    for (thread th : ten_k) {
        size_t min_i = thread_list.size();
        int min_dev = (1 << 30);
        for (size_t i = 0; i < thread_list.size(); i++)
            if (th.name == thread_list[i].name && deviants_list[i] < min_dev) {
                min_i = i;
                min_dev = deviants_list[i];
        }
        if (min_i == thread_list.size())
            thread_list.push_back(th);
        else
            thread_list[min_i].msg_list.insert(thread_list[min_i].msg_list.end(), th.msg_list.begin(), th.msg_list.end());
    }
    int total = 0;
    for (thread& th : thread_list) {
        total += th.msg_list.size();
        for (std::string ss[2] : REP)
            replace_substrs(th.name, ss[0], ss[1]);
        /*auto first = th.msg_list.begin(), last = th.msg_list.end();
        std::reverse(first, last);
        std::stable_sort(first, last);*/
    }
    std::sort(thread_list.begin(), thread_list.end(), std::greater<thread>());
    ifs.close();
    return total;
}

void check_order() {
    for (thread th : thread_list) {
        auto v = th.msg_list;
        for (size_t i = 0; i < v.size() - 1; i++) {
            if (v[i + 1] < v[i]) {
                print_ts(std::cout, v[i].ts);
                std::cout << "    ";
                print_ts(std::cout, v[i + 1].ts, '\n');
            }
        }
    }
}

void process_write() {
    std::ofstream ofs(OFNAME);
    ofs << thread_list.size() << DELIM;
    for (thread th : thread_list) {
        ofs << th.name << DELIM;
        ofs << th.msg_list.size() << ' ';
        for (message me : th.msg_list) {
            print_ts(ofs, me.ts, DELIM);
            ofs << me.sender << DELIM;
            ofs << me.msg << DELIM;
        }
    }
    ofs.close();
}

void print() {
    std::cout << "Chats: " << thread_list.size() << '\n';
    std::cout << '\n';
    for (thread th : thread_list) {
        std::cout << "Chat: " << th.name << '\n';
        std::cout << "Msgs: " << th.msg_list.size() << '\n';
    }
    std::cout << "Output file: " << OFNAME << '\n';
}

void process(bool toPre) {
    if (toPre)
        preprocess();
    int total = process_read();
    //check_order();
    std::cout << "Messages: " << total << '\n';
    process_write();
    //print();
}

std::string fix_name(std::map<std::string, std::string>& names, std::string str) {
    if (str.size() > 15 && str.substr(15) == "@facebook.com") {
        str.erase(15);
        if (!names[str].empty())
            str = names[str];
    }
    return str;
}

void read(bool toPrint) {
    std::ifstream ifs(OFNAME);
    std::fstream is("fb_ids.txt", std::ios::in | std::ios::out | std::ios::app);
    int ids;
    is >> ids;
    std::map<std::string, std::string> names;
    std::string id;
    while (is >> id) {
        std::string name;
        is >> name;
        names[id] = name;
    }
    char ch;
    int threads;
    ifs >> threads >> ch;
    for (int i = 0; i < threads; i++) {
        thread th;
        std::getline(ifs, th.name, DELIM);
        std::vector<std::string> name_list = parse(th.name, ", ");
        th.name.clear();
        for (auto s : name_list)
            th.name += fix_name(names, s) + ", ";
        th.name.erase(th.name.size() - 2);
        int msgs;
        ifs >> msgs;
        message_count += msgs;
        for (int j = 0; j < msgs; j++) {
            message me;
            tstamp t;
            ifs >> std::get<0>(t)
                >> std::get<1>(t)
                >> std::get<2>(t)
                >> std::get<3>(t)
                >> std::get<4>(t)
                >> std::get<5>(t)
                >> std::get<6>(t)
                >> std::get<7>(t)
                >> ch;
            me.ts = t;
            std::getline(ifs, me.sender, DELIM);
            me.sender = fix_name(names, me.sender);
            std::getline(ifs, me.msg, DELIM);
            th.msg_list.push_back(me);
        }
        thread_list.push_back(th);
    }
    is.clear();
    for (auto& p : names)
        if (p.second.empty())
            is << p.first << '\n';
    if (toPrint) {
        std::cout << "Messages: " << message_count << '\n';
        print();
    }
    ifs.close();
    is.close();
}
//}
void calc_sender_ratios() {
    std::ofstream ofs(AFNAME);
    for (thread th : thread_list) {
        ofs << th.name << '\n';
        ofs << th.msg_list.size() << '\n';
        std::vector<std::pair<int, std::string>> senders;
        auto names = parse(th.name, ", ");
        for (auto s : names)
            senders.push_back({0, s});
        for (message me : th.msg_list) {
            bool found = 0;
            for (auto& pis : senders)
            if (me.sender == pis.second) {
                pis.first++;
                found = 1;
                break;
            }
            if (!found)
                senders.push_back({1, me.sender});
        }
        std::sort(senders.begin(), senders.end(), std::greater<std::pair<int, std::string>>());
        for (auto& pis : senders)
            ofs << pis.second << ": " << pis.first << " (" << (int)(100.0 * pis.first / th.msg_list.size()) << "%)\n";
        ofs << '\n';
    }
    ofs.close();
}

int num_len(int n) {
    int i = 1;
    while (n > 9) {
        n /= 10;
        i++;
    }
    return i;
}

void print_msg_hours(std::ostream& ofs, int bins[24], int total) {
    int perc[24];
    for (int i = 0; i < 24; i++)
        perc[i] = 100.0 * bins[i] / total;
    int len[23];
    for (int i = 0; i < 23; i++) {
        int h = i % 12;
        if (h == 0) h = 12;
        ofs << h;
        int l = (h > 9 ? 2 : 1);
        if (h == 12) {
            ofs << (i == 0 ? 'a' : 'p') << 'm';
            l += 2;
        }
        len[i] = std::max(l, std::max(num_len(bins[i]), num_len(perc[i]) + 1));
        ofs << std::string(std::max(0, len[i] - l), ' ') << ' ';
    }
    ofs << "11\n";
    for (int i = 0; i < 23; i++)
        ofs << bins[i] << std::string(std::max(0, len[i] - num_len(bins[i])), ' ') << ' ';
    ofs << bins[23] << '\n';
    for (int i = 0; i < 23; i++)
        ofs << perc[i] << '%' << std::string(std::max(0, len[i] - (num_len(perc[i]) + 1)), ' ') << ' ';
    ofs << perc[23] << "%\n\n";
}

void special_allen_6am(message me) {
    print_ts(std::cout, me.ts, '\n');
    std::cout << me.sender << '\n';
    std::cout << me.msg << '\n';
    std::cout << '\n';
}

void calc_msg_hours() {
    std::ofstream ofs(AFNAME);
    int big_bins[24]{};
    for (thread th : thread_list) {
        ofs << th.name << " (" << th.msg_list.size() << ")\n";
        int bins[24]{};//bins[0] is 12:00am to 12:59am
        for (message me : th.msg_list) {
            int hour = std::get<5>(me.ts) + 12 * std::get<4>(me.ts);
            bins[hour]++;
            big_bins[hour]++;
            //if (th.name == "Allen Shen, Brendan Ney" && hour == 6) special_allen_6am(me);
        }
        print_msg_hours(ofs, bins, th.msg_list.size());
    }
    ofs << "Total messages by hour (" << message_count << ")\n";
    print_msg_hours(ofs, big_bins, message_count);
    ofs.close();
}

void calc_my_hours() {
    std::ofstream ofs(AFNAME);
    int bins[7][24]{}, big_bins[7]{}, total = 0;
    for (thread th : thread_list) {
        for (message me : th.msg_list)
            if (me.sender == "Brendan Ney") {
                int dofw = std::get<3>(me.ts);
                int hour = std::get<5>(me.ts) + 12 * std::get<4>(me.ts);
                bins[dofw][hour]++;
                big_bins[dofw]++;
                total++;
            }
    }
    ofs << "My messages by hour (" << total << ")\n";
    for (int i = 0; i < 7; i++) {
        ofs << '\n' << DOFWS[i] << " (" << big_bins[i] << ", " << (int)(100.0 * big_bins[i] / total) << "%)\n";
        print_msg_hours(ofs, bins[i], big_bins[i]);
    }
    ofs.close();
}
//day 1 = 01/01/2001 Monday
int ts_to_day(tstamp ts) {
    int year = std::get<0>(ts) - 2001;
    int month = std::get<1>(ts) - 1;
    int day = std::get<2>(ts);
    int leaps = year / 4;
    if (year % 4 == 3 && month > 1) leaps++;
    int days_cum[12];
    days_cum[0] = 0;
    for (int i = 1; i < 12; i++)
        days_cum[i] = days_cum[i - 1] + DAYS[i - 1];
    return year * 365 + days_cum[month] + day + leaps;
}

bool within_hour(tstamp lt, tstamp rt) {
    const int l[]{std::get<0>(lt),std::get<1>(lt),std::get<2>(lt),std::get<3>(lt),std::get<4>(lt),std::get<5>(lt),std::get<6>(lt),std::get<7>(lt)};
    const int r[]{std::get<0>(rt),std::get<1>(rt),std::get<2>(rt),std::get<3>(rt),std::get<4>(rt),std::get<5>(rt),std::get<6>(rt),std::get<7>(rt)};
    bool same_diff = l[4] == 1 && r[4] == 0 && l[5] == 11 && r[5] == 0 && l[6] >= r[6];//different day but same hour
    int lday = ts_to_day(lt);
    int rday = ts_to_day(rt);
    if (lday == rday - 1) return same_diff;
    if (lday != rday) return 0;
    int lhour = l[5] + 12 * l[4] + l[7];
    int rhour = r[5] + 12 * r[4] + r[7];
    return (lhour == rhour - 1 && l[6] >= r[6]) || lhour == rhour;
}

void calc_densest_hour() {
    std::ofstream ofs(AFNAME);
    for (thread th : thread_list) {
        ofs << th.name << " (" << th.msg_list.size() << ")\n";
        size_t i = 0, j = 1, max_i = 0, max_j = 0;
        while (j < th.msg_list.size()) {
            while (j < th.msg_list.size() && within_hour(th.msg_list[i].ts, th.msg_list[j].ts))
                j++;
            if (j - i - 1 > max_j - max_i) {
                j--;
                max_i = i;
                max_j = j;
            }
            i++;
            j++;
            while (j < th.msg_list.size() && th.msg_list[i].ts == th.msg_list[i - 1].ts) {
                i++;
                j++;
            }
        }
        ofs << max_j - max_i + 1 << '\n';
        print_ts(ofs, th.msg_list[max_i].ts);
        ofs << " (" << max_i << ")\n";
        print_ts(ofs, th.msg_list[max_j].ts);
        ofs << " (" << max_j << ")\n";
        ofs << '\n';
    }
    ofs.close();
}

void add_ngrams(dict& tf, std::string msg) {
    std::vector<std::string> p = parse(msg, " ");
    for (int len = 1; len <= 4; len++) {
        std::set<std::string> s;
        for (size_t i = 0; i + len <= p.size(); i++) {
            std::string ngram = p[i];
            for (int j = 1; j < len; j++)
                ngram += ' ' + p[i + j];
            s.insert(ngram);
        }
        for (std::string ngram : s)
            tf[ngram]++;
    }
}

const double CBRT[6]{0,1,std::cbrt(2),std::cbrt(3),std::cbrt(4),std::cbrt(5)};

bool overlap(std::string s1, std::string s2) {
    return s1.find(s2) != std::string::npos || s2.find(s1) != std::string::npos;
}

void calc_repeated_phrases() {
    std::ofstream ofs(AFNAME);
    //count
    std::vector<std::string> individuals;
    std::vector<int> msg_count;
    for (thread& th : thread_list)
    for (message& me : th.msg_list) {
        int in;
        if (individuals.empty() || std::find(individuals.begin(), individuals.end(), me.sender) == individuals.end()) {
            in = individuals.size();
            individuals.push_back(me.sender);
            msg_count.push_back(0);
        } else
            in = std::find(individuals.begin(), individuals.end(), me.sender) - individuals.begin();
        msg_count[in]++;
    }
    //prune
    for (size_t i = individuals.size(); i --> 0;)
    if (msg_count[i] < 1500) {
        individuals.erase(individuals.begin() + i);
        msg_count.erase(msg_count.begin() + i);
    }
    //tf
    std::vector<dict> tf(individuals.size());
    for (thread& th : thread_list)
    for (message& me : th.msg_list) {
        std::string s;
        std::remove_copy_if(me.msg.begin(), me.msg.end(), std::back_inserter(s), std::ptr_fun<int, int>(&std::ispunct));
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        size_t in = std::find(individuals.begin(), individuals.end(), me.sender) - individuals.begin();
        if (in < individuals.size())
            add_ngrams(tf[in], s);
    }
    //df
    dict df;
    for (dict& d : tf)
        for (auto& p : d)
            df[p.first]++;
    //tf-idf
    std::vector<dictd> tfidf(individuals.size());
    for (size_t i = 0; i < individuals.size(); i++)
    for (auto& p : tf[i]) {
        std::string ngram = p.first;
        int t = p.second;
        double d = log((double)individuals.size() / df[ngram]);
        int norm = CBRT[std::count(ngram.begin(), ngram.end(), ' ') + 1];
        tfidf[i][ngram] = t * d * norm;
    }
    //sort and prune
    std::vector<std::vector<std::pair<std::string, double>>> ngrams(individuals.size());
    for (size_t in = 0; in < individuals.size(); in++) {
        std::vector<std::pair<double, std::string>> ngram_list;
        for (auto& p : tfidf[in])
            ngram_list.push_back(std::make_pair(p.second, p.first));
        std::sort(ngram_list.begin(), ngram_list.end(), std::greater<std::pair<double, std::string>>());
        size_t i = 0;
        while (ngrams[in].size() < 20 && i < ngram_list.size()) {
            std::string s1 = ngram_list[i].second;
            bool distinct = 1;
            for (auto& p : ngrams[in])
            if (overlap(s1, p.first)) {
                distinct = 0;
                break;
            }
            if (distinct)
                ngrams[in].push_back(std::make_pair(s1, ngram_list[i].first));
            i++;
        }
    }
    //print
    for (size_t in = 0; in < individuals.size(); in++) {
        ofs << individuals[in] << " (" << msg_count[in] << ")\n";
        for (auto& p : ngrams[in])
            ofs << p.first << ' ' << p.second << '\n';
        ofs << '\n';
    }
    ofs.close();
}

std::pair<int,int> day_limits(const std::vector<thread>& thread_list_to_use) {
    int first = (1 << 30);
    int last = 0;
    for (thread th : thread_list_to_use)
        for (message me : th.msg_list) {
            int day = ts_to_day(me.ts);
            first = std::min(first, day);
            last = std::max(last, day);
        }
    return std::make_pair(first, last);
}

tstamp day_to_ts(int day) {
    int d = 0;
    int year = 2001;
    while (d + 365 + (year % 4 ? 0 : 1) < day) {
        d += 365 + (year % 4 ? 0 : 1);
        year++;
    }
    int days[12];
    for (int i = 0; i < 12; i++)
        days[i] = DAYS[i];
    if (year % 4 == 0)
        days[1]++;
    int month = 1;
    while (d + days[month - 1] < day) {
        d += days[month - 1];
        month++;
    }
    tstamp ts;
    std::get<0>(ts) = year;
    std::get<1>(ts) = month;
    std::get<2>(ts) = day - d;
    return ts;
}

std::string ts_to_date(tstamp ts) {
    std::stringstream ss;
    ss << std::get<1>(ts) << '/' << std::get<2>(ts) << '/' << (std::get<0>(ts) % 100);
    return ss.str();
}

void calc_area_chart(int thread_to_use = -1) {//default use all threads
    std::ofstream ofs(AFNAME);
    std::vector<thread> thread_list_to_use;
    if (thread_to_use == -1)
        thread_list_to_use = thread_list;
    else
        thread_list_to_use.push_back(thread_list[thread_to_use]);
    auto p = day_limits(thread_list_to_use);
    const int FIRST = p.first / 7, LAST = p.second / 7;//week numbers, beginning with sunday
    std::vector<std::string> senders;
    std::vector<std::vector<int>> msgs;
    for (thread th : thread_list_to_use)
        for (message me : th.msg_list) {
            int sender;
            if (senders.empty() || std::find(senders.begin(), senders.end(), me.sender) == senders.end()) {
                sender = senders.size();
                senders.push_back(me.sender);
                std::vector<int> v(LAST - FIRST + 1);
                msgs.push_back(v);
            } else
                sender = std::find(senders.begin(), senders.end(), me.sender) - senders.begin();
            int week = ts_to_day(me.ts) / 7 - FIRST;
            msgs[sender][week]++;
        }
    for (int i = FIRST; i <= LAST; i++)
        ofs << '\t' << ts_to_date(day_to_ts(i * 7));
    for (size_t i = 0; i < senders.size(); i++) {
        ofs << '\n' << senders[i];
        for (size_t j = 0; j < msgs[i].size(); j++)
            ofs << '\t' << msgs[i][j];
    }
    ofs.close();
}

void analyze(int path) {
    read(!path);
    switch (path) {
        case 1: calc_sender_ratios(); break;
        case 2: calc_msg_hours(); break;
        case 3: calc_my_hours(); break;
        case 4: calc_densest_hour(); break;
        case 5: calc_repeated_phrases(); break;
        case 6: calc_area_chart(0); break;
    }
}

int main() {
    //process(1);//preprocess=1
    analyze(0);//print to console=0
    return 0;
}
