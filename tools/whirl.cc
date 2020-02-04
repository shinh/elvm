//--------------------------------------------
// WHIRL PROGRAMMING LANGUAGE
// by: BigZaphod sean@fifthace.com
// http://www.bigzaphod.org/whirl/
// 
// License: Public Domain
//--------------------------------------------
#include <vector>
#include <stdio.h>
#include <stdlib.h>

typedef std::vector<int>  mem_t;
typedef std::vector<bool> prog_t;

mem_t memory;
prog_t program;

mem_t::iterator mem_pos;
prog_t::iterator prog_pos;

typedef enum
{
    cw,
    ccw
} direction_t;

class ring
{
public:
    direction_t dir;
    short pos;
    int value;

    ring() : dir(cw), pos(0), value(0) {}

    void switch_dir()
    {
        (dir == cw)? dir = ccw: dir = cw;
    }
    
    void rotate()
    {
        if( dir == cw )
        {
            pos++;
            if( pos == 12 ) pos = 0;
        }
        else
        {
            pos--;
            if( pos == -1 ) pos = 11;
        }
    }
    
    virtual bool execute()  = 0;
};

class op_ring : public ring
{
public:
    bool execute();
};

class math_ring : public ring
{
public:
    bool execute();
};

op_ring ops;
math_ring math;
ring* cur = &ops;
bool next_instruction = true;   // lame

bool op_ring::execute()
{

#ifdef WHIRL_DEBUG
    if (pos != 0) {
        printf("Cmd (%d) Executing operations cmd %d ", (int) (prog_pos - program.begin()), pos);
    }
#endif

    if( pos != 0 ) switch( pos )
    {
        // One
        case 2:
            value=1;
            break;

        // Zero
        case 3:
            value=0;
            break;

        // Load
        case 4:
            value=*mem_pos;
            break;

        // Store
        case 5:
            *mem_pos = value;
            break;

        // DAdd
        case 7:
            {
                int t = mem_pos - memory.begin();
                t += value;
                if( t < 0 )
                    return false;   // out of bounds!
                if( t < (int) memory.size() )
                    mem_pos = memory.begin() + t;
                else
                {
                    while( (int) memory.size() <= t )
                        memory.push_back(0);
                    mem_pos = memory.begin() + t;
                }
            }
            break;

        // Logic
        case 8:
            if( *mem_pos != 0 )
                value = (value ? 1 : 0);
            else
                value = 0;
            break;

        // If
        case 9:
            if( *mem_pos == 0 )
                break;
            // else fall through!

        // PAdd
        case 6:
            {
                int t = prog_pos - program.begin();
                t += value;
                if( t < 0 || t >= (int) program.size() )
                    return false;   // out of bounds!
                prog_pos = program.begin() + t;
                next_instruction = false;
            }
            break;

        // IntIO
        case 10:
            if( value == 0 )
            {
                // read integer
                char buf[100];
                int c = 0;
                while( c < (int) sizeof(buf)-1 )
                { 
                    buf[c] = getchar();
                    c++;
                    buf[c] = 0;
            
                    if( buf[c-1] == '\n' )
                        break;
                }
                // swallow, just in case.
                if( c == sizeof(buf) )
                    while( getchar() != '\n' );
        
                (*mem_pos) = atoi( buf );
            }
            else
                printf( "%d", *mem_pos );
            break;
        
        // AscIO
        case 11:
            if( value == 0 )
            {
                // read character
                (*mem_pos) = getchar();

                // The default behavior of the whirl interpreter for some reason
                // is to only take the first ascii character per line. This does
                // not work the way ELVM expects input to work, so this next line
                // is commented out.

                // while ( getchar() != '\n' )
            }
            else
                printf( "%c", *mem_pos );
            break;

        // Exit
        case 1:
        default:
            return false;
    }

#ifdef WHIRL_DEBUG
    if (pos != 0) {
        printf("(val is %d, mem[%d] is %d)\n",
               value, (int)(mem_pos - memory.begin()), *mem_pos);
    }
#endif

    return true;
}

bool math_ring::execute()
{

#ifdef WHIRL_DEBUG
    if (pos != 0) {
        printf("Cmd (%d) Executing math cmd %d ", (int) (prog_pos - program.begin()), pos);
    }
#endif

    if( pos != 0 ) switch( pos )
    {
        // Load
        case 1:
            value = *mem_pos;
            break;

        // Store
        case 2:
            *mem_pos = value;
            break;

        // Add
        case 3:
            value += *mem_pos;
            break;

        // Mult
        case 4:
            value *= *mem_pos;
            break;

        // Div
        case 5:
            value /= *mem_pos;
            break;

        // Zero
        case 6:
            value = 0;
            break;
        
        // <
        case 7:
            value < *mem_pos? value = 1: value = 0;
            break;

        // >
        case 8:
            value > *mem_pos? value = 1: value = 0;
            break;

        // =
        case 9:
            value == *mem_pos? value = 1: value = 0;
            break;

        // Not
        case 10:
            value != 0? value = 0: value = 1;
            break;

        // Neg
        case 11:
            value *= -1;
            break;

        // error...
        default:
            return false;
    }

#ifdef WHIRL_DEBUG
    if (pos != 0) {
        printf("(val is %d, mem[%d] is %d)\n",
               value, (int)(mem_pos - memory.begin()), *mem_pos);
    }
#endif

    return true;
}

int main( int argc, char** argv )
{
	if( argc < 2 )
	{
		printf( "Usage: %s program.wrl\n\n", argv[0] );
		exit( 1 );
	}

	FILE* f = fopen( argv[1], "rb" );

	if( f == NULL )
	{
		printf( "Cannot open source file [%s].\n", argv[1] );
        exit( 1 );
    }

    while( !feof(f) )
    {
        char buf = fgetc( f );
        if( buf == '1' )
            program.push_back( true );
        else if( buf == '0' )
            program.push_back( false );
    }

	fclose( f );

    // init main memory.
    memory.push_back( 0 );
    mem_pos = memory.begin();

    prog_pos = program.begin();
    bool execute = false;
    while( prog_pos != program.end() )
    {
        next_instruction = true;

        if( *prog_pos )
        {
#ifdef WHIRL_DEBUG
            printf("1");
#endif
            cur->rotate();
            execute = false;
        }
        else
        {
            cur->switch_dir();
            if( execute )
            {
#ifdef WHIRL_DEBUG
                printf("0\n");
#endif
                if( !cur->execute() )
                    return 0;
                (cur == &ops)? cur = &math: cur = &ops;                
                execute = false;
            }
            else {
#ifdef WHIRL_DEBUG
                printf("0");
#endif
                execute = true;
            }
        }
        
        if( next_instruction )
            prog_pos++;
    }

	printf( "\n" );

	return 0;
}
