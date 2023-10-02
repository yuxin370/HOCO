/*-------------------------------------------------

TADOC COMPRESS FUNCTION:



--------------------------------------------------*/

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/tadoc_compress.hh"

/* ----------
 * Local definitions
 * ----------
 */
double timestamp ()
{
  struct timeval tv;
  gettimeofday (&tv, 0);
  return tv.tv_sec + 1e-6*tv.tv_usec;
}

map<ulong,string> dictionary_use;
map<string,ulong> dictionary_reverse;
  int fileid;

void print_wc();
string getWordDictionary(ifstream& in)
{
    int c;
  char t;
    string word;
    t = in.get();
    if( t == ' ' || t == '\t' || t == '\n' ) {
      return string(1,t);
    }
    word += t;
    while( !in.eof() ){
        c = in.get();
        if( c == '\n' ) {
          break;
        }
        word += c;
    }
    return word;
}

rules *S;                 // pointer to main rule of the grammar

int num_rules = 0;        // number of rules in the grammar
int num_symbols = 0;      // number of symbols in the grammar
int min_terminal,         // minimum and maximum value among terminal symbols
    max_terminal;         //
int max_rule_len = 2;     // maximum rule length
bool compression_initialized = false;

int compress = 0,
  do_uncompress = 0,
  do_print = 0,
  reproduce = 0,
  quiet = 0,
  phind = 0,
  numbers = 0,
  print_rule_freq = 0,
  print_rule_usage = 0,
  delimiter = -1,
  memory_to_use = 1000000000,

  // minimum number of times a digram must occur to form rule minus one
  // (e.g. if K is 1, two occurrences are required to form rule)
    K = 1;

char *delimiter_string = 0;
// new
string output_path;

void uncompress(), print(), number(), forget(symbols *s),
  forget_print(symbols *s);
void start_compress(bool), end_compress(), stop_forgetting();
ofstream *rule_S = 0;

/* ----------
 * tadoc_compress - 
 *		Compresses source into dest using strategy. Returns the number of
 *		bytes written in buffer dest, or -1 if compression fails.
 * ----------
 */
int32 
tadoc_compress(const char *source, int32 slen, char *dest) 
{
    int i;
  extern int min_terminal, max_terminal, max_rule_len;

  keep = create_context(KEEPI_LENGTH, STATIC);
  install_symbol(keep, KEEPI_NO);
  install_symbol(keep, KEEPI_YES);
  install_symbol(keep, KEEPI_DUMMY);

  binary_context *file_type = create_binary_context();
  int context_type;

  if (compress) {

    // this is specific to compression

    startoutputtingbits();
    start_encode();

    binary_encode(file_type, all_input_read);
    context_type = all_input_read ? STATIC : DYNAMIC;

    min_terminal = TERM_TO_CODE(min_terminal);
    max_terminal = TERM_TO_CODE(max_terminal);

    arithmetic_encode(min_terminal, min_terminal + 1, MINMAXTERM_TARGET);
    arithmetic_encode(max_terminal, max_terminal + 1, MINMAXTERM_TARGET);
    arithmetic_encode(max_rule_len, max_rule_len + 1, MAXRULELEN_TARGET);

  }
  else {

    // this is specific to decompression

    startinputtingbits();
    start_decode();

    context_type = binary_decode(file_type) ? STATIC : DYNAMIC;

    min_terminal = arithmetic_decode_target(MINMAXTERM_TARGET);
    arithmetic_decode(min_terminal, min_terminal + 1, MINMAXTERM_TARGET);
    max_terminal = arithmetic_decode_target(MINMAXTERM_TARGET);
    arithmetic_decode(max_terminal, max_terminal + 1, MINMAXTERM_TARGET);
    max_rule_len = arithmetic_decode_target(MAXRULELEN_TARGET);
    arithmetic_decode(max_rule_len, max_rule_len + 1, MAXRULELEN_TARGET);
  }

  symbol = create_context(SPECIAL_SYMBOLS + max_terminal - min_terminal + 1,
			  context_type);
  install_symbol(symbol, START_RULE);
  install_symbol(symbol, END_OF_FILE);
  install_symbol(symbol, STOP_FORGETTING);
  for (i = min_terminal; i <= max_terminal; i+=2) install_symbol(symbol, i);

  lengths = create_context(max_rule_len, context_type);
  for (i = 2; i <= max_rule_len; i++) install_symbol(lengths, i);
}

/* ----------
 * tadoc_decompress -
 *		Decompresses source into dest. Returns the number of bytes
 *		decompressed into the destination buffer, or -1 if the
 *		compressed data is corrupted.
 *
 *		If check_complete is true, the data is considered corrupted
 *		if we don't exactly fill the destination buffer.  Callers that
 *		are extracting a slice typically can't apply this check.
 * ----------
 */
int32
pglz_decompress(const char *source, int32 slen, char *dest,
				int32 rawsize, bool check_complete)
{
    extern int numbers;     // true - we output (terminal) symbol codes in decimal form, one per line; false - as characters

  R = (rules **) malloc(UNCOMPRESS_RSIZE * sizeof(rules *));

  start_compress(true);

  while (1) {
    int current = current_rule;

    // read a symbol
    int i = get_symbol();

    if (i == END_OF_FILE) break;
    else if (i == STOP_FORGETTING) forgetting = 0;
    // symbol is a yet unknown terminal
    else if (i == NOT_KNOWN)
    {
      int j = arithmetic_decode_target(MINMAXTERM_TARGET);
      arithmetic_decode(j, j + 1, MINMAXTERM_TARGET);
      install_symbol(symbol, j);

      cout << char(CODE_TO_TERM(j));
    }
    // symbol is a (known) terminal
    else if (IS_TERMINAL(i))
    {
      if (numbers) cout <<  int(CODE_TO_TERM(i)) << endl;
      else         cout << char(CODE_TO_TERM(i));
    }
    // symbol is a non-terminal
    else
    {
      int j = CODE_TO_NONTERM(i);
      // if we are "forgetting rules", non-terminal is followed
      if (i < current && forgetting) {
	// by keep index
        int keepi = decode(keep);

	// reproduce rule's full expansion, unless keep index says not to
        if (keepi != KEEPI_DUMMY) R[j]->reproduce();

	// delete rule from memory, if keep index says so
        if (keepi == KEEPI_NO || keepi == KEEPI_DUMMY) {
           delete_symbol(symbol, i);
           while (R[j]->first()->next() != R[j]->first()) delete R[j]->first();
           delete R[j];
        }
      }
      // we are not "forgetting rules", rule is not followed
      // by keep index, just reproduce
      else R[j]->reproduce();
    }
  }

  end_compress();
}
