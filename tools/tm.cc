/* Simulator for deterministic Turing machines.

   Based on the definition in Sipser's Introduction to the Theory of
   Computation.

   A state can be any integer. The start state is 0. Any negative
   state is an accept state.

   A symbol can be any string of non-whitespace characters. The blank
   symbol is _.

   Each line in the machine description specifies a transition:

   q a r b d

   where 
   - q is the current state
   - a is the symbol read from the tape
   - r is the new state
   - b is the symbol written to the tape
   - d is a direction: L (left), N (no move), or R (right)

   The tape extends infinitely to the right, but not to the left.

   Initially, characters are read from stdin, all the way to EOF, and
   are encoded on the tape as 8-bit ASCII codes, most significant bit
   first. The remainder of the tape is filled with blank symbols
   (_). For example, "ABC" would be encoded as

   0 1 0 0 0 0 0 1 0 1 0 0 0 0 1 0 0 1 0 0 0 0 1 1 _ _ _ ...

   The head starts on the first square of the tape.

   If the machine attempts to move the head to the left of the first
   square, the head remains on the first square.

   If the machine enters an accept state, the simulator exits with
   status 0. If a move is not possible, the simulator exists with
   status 1. In either case, the contents of the tape are written to
   stdout, using the same encoding as described above. Symbols that
   are neither 0 nor 1 are ignored, as are incomplete bytes.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <unistd.h>
using namespace std;

typedef int state_t;
typedef string symbol_t;
const symbol_t BLANK = "_";
const symbol_t ONE = "1";
const symbol_t ZERO = "0";

class parse_error: public std::runtime_error { 
public:
  parse_error(const std::string &message): runtime_error(message) { }
};

typedef std::tuple<state_t, symbol_t> condition;
typedef std::tuple<state_t, symbol_t, int> action;

class dtm {
  std::map<condition, action> transitions;
public:
  void add_transition(state_t q, symbol_t a, state_t r, symbol_t b, int d) {
    if (transitions.count(make_tuple(q,a)) > 0)
      throw std::runtime_error("machine is not deterministic at state " + to_string(q));
    transitions.insert({make_tuple(q,a),make_tuple(r,b,d)});
  }
  bool has_transition(state_t q, symbol_t a) const {
    return transitions.count(make_tuple(q,a));
  }
  const action &get_transition(state_t q, symbol_t a) const {
    return transitions.at(make_tuple(q,a));
  }
};

state_t convert_state(const string &s) {
  size_t idx;
  state_t q = stoi(s, &idx);
  if (idx != s.size())
    throw parse_error("invalid state: " + s);
  return q;
}

int convert_direction(const string &s) {
  if (s == "L")
    return -1;
  else if (s == "N")
    return 0;
  else if (s == "R")
    return 1;
  else
    throw parse_error("invalid direction: " + s);
}

void remove_comment(string &line) {
  auto pos = line.find("//");
  if (pos != string::npos)
    line = line.substr(0, pos);
}

void split(const string &line, vector<string> &fields) {
  istringstream iss(line);
  string field;
  fields.clear();
  while (iss >> field)
    fields.push_back(field);
}

void read_dtm(ifstream &is, dtm &m) {
  string line;
  while (getline(is, line)) {
    remove_comment(line);
    vector<string> fields;
    split(line, fields);
    if (fields.size() == 0)
      continue;
    try {
      condition cond;
      action act;
      if (fields.size() != 5)
	throw parse_error("could not read transition: " + line);
      m.add_transition(convert_state(fields[0]), fields[1],
		       convert_state(fields[2]), fields[3],
		       convert_direction(fields[4]));
    } catch (parse_error e) {
      cerr << e.what() << endl;
      exit(2);
    }
  }
}

bool run_dtm(const dtm &m, vector<symbol_t> &tape, bool verbose=false) {
  state_t q = 0;
  vector<symbol_t>::size_type pos = 0;
  while (true) {
    while (pos+1 > tape.size())
      tape.push_back(BLANK);
    while (tape.back() == BLANK && tape.size() > pos+1)
      tape.pop_back();
    if (verbose) {
      cerr << q << " | ";
      for (vector<symbol_t>::size_type i=0; i<tape.size(); i++) {
	if (i == pos)
	  cerr << "[" << tape[i] << "]";
	else
	  cerr << tape[i];
      }
      cerr << endl;
    }
    if (q < 0)
      return true; // accept
    int d;
    if (!m.has_transition(q, tape[pos]))
      return false;
    tie(q, tape[pos], d) = m.get_transition(q, tape[pos]);
    if (d == 1)
      ++pos;
    else if (d == -1 and pos > 0)
      --pos;
  }
}

void encode_tape(const string &s, vector<symbol_t> &tape) {
  tape.clear();
  for (auto c: s) {
    for (int i=7; i>=0; i--)
      tape.push_back(c & (1<<i) ? ONE : ZERO);
  }
}

void decode_tape(const vector<symbol_t> &tape, string &s) {
  s.clear();
  char c = 0;
  int k = 0;
  for (const auto &a: tape) {
    if (a == ONE) {
      c = (c<<1) | 1;
      k++;
    } else if (a == ZERO) {
      c = (c<<1) | 0;
      k++;
    }
    if (k == 8) {
      s.push_back(c);
      c = 0;
      k = 0;
    }
  }
}

void usage() {
  cerr << "usage: tm [-v] <filename>\n";
  exit(2);
}

int main(int argc, char *argv[]) {
  bool verbose = 0;
  int ch;
  while ((ch = getopt(argc, argv, "v")) != -1) {
    switch (ch) {
    case 'v': 
      verbose = true;
      break;
    default: 
      usage();
    }
  }
  argc -= optind;
  argv += optind;
  if (argc != 1) usage();

  ifstream is(argv[0]);
  dtm m;
  read_dtm(is, m);

  char c;
  string s;
  while (cin.get(c))
    s.push_back(c);

  vector<symbol_t> tape;
  encode_tape(s, tape);

  if (run_dtm(m, tape, verbose)) {
    decode_tape(tape, s);
    cout << s;
    return 0;
  } else {
    return 1;
  }
}