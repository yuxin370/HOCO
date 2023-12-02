#include <stdint.h>

typedef struct Rule Rule;
typedef struct Symbol Symbol;
typedef struct LocalOffsetUnit LocalOffsetUnit;
typedef struct RuleData RuleData;
typedef struct CompressedData CompressedData;

Rule* genRule();
void destroyRule(Rule*);
void reuse(Rule*);
void deuse(Rule*);
Symbol* first(Rule*);
Symbol* last(Rule*);
Symbol* genSymbol1(uint64_t);
Symbol* genSymbol2(Rule*);
Rule* getRule(Symbol*);
void destroySymbol(Symbol*);
int nonTerminal(Symbol*);
int isGuard(Symbol*);
void join(Symbol*, Symbol*);
void deleteDigram(Symbol*);
void insertAfter(Symbol*, Symbol*);
int check_put(Symbol*);
void expand(Symbol*);
void match(Symbol*, Symbol*);
void substitute(Symbol*, Rule*);

void initHashTable(int32_t);
void clearHashTable();
Symbol** findDigram(Symbol*);

int32_t discover(Rule*, CompressedData*);
int32_t writeRuleData(void*, RuleData*);
RuleData* readRuleData(void*, int32_t);
int32_t writeCompressedData(void*, CompressedData*);
void displayCompressedData(CompressedData*);
void destroyCompressedData(CompressedData*);
int32_t restore(void*, RuleData*, CompressedData*);

int32_t tadoc_compress(const char *, int32_t, char *);
int32_t tadoc_decompress(const char *, int32_t, char *, int32_t, int32_t);