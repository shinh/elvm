#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <stack>
#include <vector>
#include <unordered_map>

#if defined __GNUC__ && __GNUC__*100+__GNUC_MINOR__<405
	#define __builtin_unreachable()
#endif
#define case(x) break;case x:;
#define OP_INT 0
#define OP_JMP 1
#define OP_ADD 2
#define OP_SUB 3
#define OP_MUL 4
#define OP_DIV 5
#define OP_MOD 6
#define OP_CMP 7
#define OP_NOT 8
#define OP_POP 9
#define OP_DUP 10
#define OP_SWP 11
#define OP_PRI 12
#define OP_PRC 13
#define OP_GC 14
#define OP_GI 15
#define OP_REM 16
#define OP_WEM 17
#define OP_RNG 18
#define OP_VIF_TRUE 19
#define OP_HIF_TRUE 20
#define OP_VIF_FALSE 21
#define OP_HIF_FALSE 22
#define OP_IF_TRUE_RANGE 19 ... 20
#define OP_IF_FALSE_RANGE 21 ... 22

struct coord {
	int32_t x, y;

	bool operator==(const coord &other) const {
		return x == other.x && y == other.y;
	}
};

struct cell {
	// joff stores the jit code offset for this cell in the given direction
	size_t joff[4];
	int32_t val;
	// store whether we execute a cell to avoid recompile when modifying non executable cells
	bool exec;

	cell(){
		memset(joff, -1, sizeof(joff));
		val = 0;
		exec = false;
	}

	cell(int32_t v){
		memset(joff, -1, sizeof(joff));
		val = v;
		exec = false;
	}
};

struct hash_coord {
	size_t operator()(const coord &xy) const
	{
		uint64_t xy64 = *(uint64_t*)&xy;
		if (sizeof(size_t) == 8) {
			return (size_t)xy64;
		} else {
			return std::hash<uint64_t>()(xy64);
		}
	}
};

std::stack<int32_t, std::vector<int32_t>> st;
std::vector<uint8_t> code;
std::unordered_map<coord, cell, hash_coord> ps;
int32_t mnx, mny, mxx, mxy; // all coords of ps fall within these

// convert character value to interpreter opcode
int opc(int i){
	static const uint8_t loc[]={16,29,31,17,14,23,36,36,36,12,10,21,11,20,13,0,1,2,3,4,5,6,7,8,9,18,36,34,36,32,26,30,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,19,36,33,28,15,36,36,36,36,36,36,24,36,36,36,36,36,36,36,36,25,36,36,36,36,36,35,36,36,36,36,36,27,36,22};
	return i<33||i>126?36:loc[i-33];
}

int32_t pop() {
	if (st.empty()) return 0;
	int32_t v = st.top();
	st.pop();
	return v;
}

int32_t *top() {
	if (st.empty()) {
		st.push(0);
	}
	return &st.top();
}

size_t readsize(size_t offs){
	return *(size_t*)&code[offs];
}
void writesize(size_t offs, size_t val){
	*(size_t*)&code[offs] = val;
}
void pushsize(size_t val){
	code.resize(code.size() + sizeof(size_t));
	*(size_t*)&code[code.size() - sizeof(size_t)] = val;
}
int32_t readint32(size_t offs){
	return *(int32_t*)&code[offs];
}
void pushint32(int32_t val){
	code.resize(code.size() + 4);
	*(int32_t*)&code[code.size() - 4] = val;
}

coord readcoord(size_t offs){
	return coord {
		.x = readint32(offs),
		.y = readint32(offs+4),
	};
}

void pushcoord(coord xy){
	pushint32(xy.x);
	pushint32(xy.y);
}

struct cursor {
	coord xy;
	uint8_t dir;

	void readcurse(size_t offs){
		xy = readcoord(offs);
		dir = code[offs+8];
	}

	void pushcurse(){
		pushcoord(xy);
		code.push_back(dir);
	}

	void mv(){
		switch (dir) {
			default:__builtin_unreachable();
			case(0) xy.x = xy.x == mxx ? mnx : xy.x+1;
			case(1) xy.y = xy.y == mny ? mxy : xy.y-1;
			case(2) xy.x = xy.x == mnx ? mxx : xy.x-1;
			case(3) xy.y = xy.y == mxy ? mny : xy.y+1;
		}
	}

	size_t compile(){
		struct PeepData {
			size_t*joff;
			size_t code;
		};
		std::vector<PeepData> peep;
		while(true){
			cell*ch = &ps[xy];
			if (ch->joff[dir] != (size_t)-1){
				if (ch->joff[dir] == code.size()) {
					while(true);
				}
				code.push_back(OP_JMP);
				pushsize(ch->joff[dir]);
				return ch->joff[dir];
			}
			int op=opc(ch->val);
			ch->exec = true;
			if (op < 30) {
				// op >= 30 is movement; only store joff on codegen ops
				// this way peephole doesn't have to fix up multiple cells
				ch->joff[dir] = code.size();
				PeepData pd = {
					.joff = ch->joff+dir,
					.code = code.size(),
				};
				peep.push_back(pd);
			}
			switch(op){
			default:__builtin_unreachable();
			case(0 ... 9)
				code.push_back(OP_INT);
				pushint32(op);
				st.push(op);
			case(10 ... 24)
				code.push_back(OP_ADD + op - 10);
				switch(op){
				default:__builtin_unreachable();
				case(10)if(st.size()>1){
					int32_t x = st.top();
					st.pop();
					*&st.top() += x;
				}
				case(11){
					int32_t x = pop();
					int32_t *yp = top();
					*yp -= x;
				}
				case(12)if(!st.empty()){
					if (st.size()==1)st.pop();
					else {
						int32_t x = st.top();
						st.pop();
						*&st.top() *= x;
					}
				}
				case(13){
					int32_t x = pop();
					int32_t *yp = top();
					*yp /= x;
				}
				case(14){
					int32_t x = pop();
					int32_t *yp = top();
					*yp %= x;
				}
				case(15){
					int32_t x = pop();
					int32_t *yp = top();
					*yp = *yp>x;
				}
				case(16)
					if(st.empty()) st.push(1);
					else {
						int32_t *x = &st.top();
						*x = !*x;
					}
					if (peep.size()>1 && code[peep[peep.size()-2].code]==OP_INT) {
						*peep[peep.size()-1].joff = -1;
						peep.resize(peep.size()-1);
						code.resize(code.size()-5);
						pushint32(st.top());
					}
				case(17)
					if(!st.empty())st.pop();
					if (peep.size()>1 && code[peep[peep.size()-2].code]==OP_INT) {
						*peep[peep.size()-1].joff = -1;
						if(peep[peep.size()-2].joff) *peep[peep.size()-2].joff = -1;
						peep.resize(peep.size()-2);
						code.resize(code.size()-6);
					}
				case(18)
					if(!st.empty()){
						st.push(st.top());
						if (peep.size()>1 && code[peep[peep.size()-2].code]==OP_INT) {
							*peep[peep.size()-1].joff = -1;
							code[code.size()-1] = OP_INT;
							pushint32(st.top());
						}
					}
				case(19){
					int32_t x = pop();
					int32_t *yp = top();
					int32_t y = *yp;
					*yp = x;
					st.push(y);
				}
				case(20)printf("%d ",pop());
				case(21)putchar(pop());
				case(22)st.push(getchar());
				case(23){
					int x;
					st.push(scanf("%d",&x)!=1?-1:x);
				}
				case(24){
					coord getxy;
					getxy.y = pop();
					getxy.x = pop();
					st.push(ps[getxy].val);
				}
				}
				if(op<16 && peep.size()>2 && code[peep[peep.size()-2].code]==OP_INT && code[peep[peep.size()-3].code]==OP_INT){
					*peep[peep.size()-1].joff = -1;
					if (peep[peep.size()-2].joff) *peep[peep.size()-2].joff = -1;
					peep.resize(peep.size()-2);
					code.resize(code.size()-10); // truncate to first const op
					pushint32(st.size()?st.top():0);
				}
			case(25){
				coord putxy;
				putxy.y = pop();
				putxy.x = pop();
				cell*ch = &ps[putxy];
				ch->val = pop();
				if (ch->exec){
					code.clear();
					peep.clear();
					for (auto& kv : ps) {
						memset(kv.second.joff, -1, sizeof(kv.second.joff));
						kv.second.exec = false;
					}
				} else {
					code.push_back(OP_WEM);
					pushcurse();
				}
			}
			case(26){
				dir = rand()&3;
				code.push_back(OP_RNG);
				pushcoord(xy);
				code.resize(code.size() + 4*sizeof(size_t), -1);
				writesize(code.size() - dir*sizeof(size_t), code.size());
			}
			case(27 ... 28){
				int32_t x = pop();
				int opcode;
				if (op==27){
					opcode = x?OP_VIF_FALSE:OP_VIF_TRUE;
					dir = x?1:3;
				} else {
					opcode = x?OP_HIF_FALSE:OP_HIF_TRUE;
					dir = x?2:0;
				}
				code.push_back(opcode);
				pushcoord(xy);
				pushsize(-1);
			}
			case(29){
				int j = 1;
				while(true){
					mv();
					cell*sch = &ps[xy];
					sch->exec = true;
					if (sch->val == '"'){
						if(j) peep[peep.size()-1].joff[dir]=(size_t)-1;
						peep.resize(peep.size()-1);
						break;
					}
					j = 0;
					st.push(sch->val);
					PeepData pd = {
						.joff = nullptr,
						.code = code.size(),
					};
					peep.push_back(pd);
					code.push_back(OP_INT);
					pushint32(sch->val);
				}
			}
			case(30)exit(0);
			case(31)mv();
			case(32 ... 35)dir=op&3;
			case(36);
			}
			mv();
		}
	}
};

int main(int,char**argv){
	srand(time(NULL));
	FILE*prog=fopen(argv[1],"r");
	coord readxy = { .x = 0, .y = 0 };
	for(;;){
		int c=getc(prog);
		if(c=='\n') {
			readxy.x=0;
			readxy.y++;
			continue;
		} else if(c==-1) break;
		ps[readxy] = cell(c);
		readxy.x++;
		if (readxy.x>mxx)mxx=readxy.x;
	}
	fclose(prog);
	mxy=readxy.y;
	cursor curse = {
		.xy = { .x = 0, .y = 0 },
		.dir = 0,
	};
	size_t pc = curse.compile();
	while (true) {
		switch (code[pc++]) {
		default:__builtin_unreachable();
		case(OP_INT){
			st.push(readint32(pc));
			pc += 4;
		}
		case(OP_ADD)if(st.size()>1){
			int32_t x = st.top();
			st.pop();
			*&st.top() += x;
		}
		case(OP_SUB){
			int32_t x=pop();
			int32_t *yp=top();
			*yp -= x;
		}
		case(OP_MUL)if(!st.empty()){
			if (st.size()==1)st.pop();
			else {
				int32_t x = st.top();
				st.pop();
				*&st.top() *= x;
			}
		}
		case(OP_DIV){
			int32_t x=pop();
			int32_t *yp=top();
			*yp /= x;
		}
		case(OP_MOD){
			int32_t x=pop();
			int32_t *yp=top();
			*yp %= x;
		}
		case(OP_CMP){
			int32_t x=pop();
			int32_t *yp=top();
			*yp = *yp>x;
		}
		case(OP_NOT)
			if(st.empty()) st.push(1);
			else {
				int32_t *x = &st.top();
				*x = !*x;
			}
		case(OP_POP)if(!st.empty())st.pop();
		case(OP_DUP)if(!st.empty()){
			st.push(st.top());
		}
		case(OP_SWP){
			int32_t x=pop();
			int32_t *yp=top();
			int32_t y = *yp;
			*yp = x;
			st.push(y);
		}
		case(OP_PRI)printf("%d ",pop());
		case(OP_PRC)putchar(pop());
		case(OP_GC)st.push(getchar());
		case(OP_GI){
			int32_t x;
			st.push(scanf("%d",&x)!=1?-1:x);
		}
		case(OP_REM){
			coord getxy;
			getxy.y=pop();
			getxy.x=pop();
			st.push(ps[getxy].val);
		}
		case(OP_WEM){
			coord putxy;
			putxy.y=pop();
			putxy.x=pop();
			int32_t z=pop();
			cell*ch=&ps[putxy];
			ch->val = z;
			if (ch->exec){
				curse.readcurse(pc);
				code.clear();
				for (auto& kv : ps) {
					memset(kv.second.joff, -1, sizeof(kv.second.joff));
					kv.second.exec = false;
				}
				curse.mv();
				pc = curse.compile();
			} else {
				pc += 9;
			}
		}
		case(OP_RNG){
			int dir=rand()&3;
			size_t jo = readsize(pc + 8 + dir*sizeof(size_t));
			if (jo == (size_t)-1){
				writesize(pc + 8 + dir*sizeof(size_t), code.size());
				curse.xy = readcoord(pc);
				curse.dir = dir;
				curse.mv();
				pc = curse.compile();
			} else {
				pc = jo;
			}
		}
		case(OP_IF_TRUE_RANGE){
			if (pop()) {
				int ish = code[pc-1] == OP_HIF_TRUE;
				size_t jo = readsize(pc+8);
				if (jo == (size_t)-1){
					writesize(pc+8, code.size());
					curse.xy = readcoord(pc);
					curse.dir = ish ? 2 : 1;
					curse.mv();
					pc = curse.compile();
				} else {
					pc = jo;
				}
			} else {
				pc += 8 + sizeof(size_t);
			}
		}
		case(OP_IF_FALSE_RANGE){
			if (!pop()) {
				int ish = code[pc-1] == OP_HIF_FALSE;
				size_t jo = readsize(pc+8);
				if (jo == (size_t)-1){
					writesize(pc+8, code.size());
					curse.xy = readcoord(pc);
					curse.dir = ish ? 0 : 3;
					curse.mv();
					pc = curse.compile();
				} else {
					pc = jo;
				}
			} else {
				pc += 8 + sizeof(size_t);
			}
		}
		case(OP_JMP){
			pc = readsize(pc);
		}
		}
	}
	return 0;
}