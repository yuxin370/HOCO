#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h> // to use NULL pointer
#include "common/tadoc_compress.h"

#define NUM_TERMINALS 100000
#define PRIME 2265539
typedef unsigned long long ulong;

/* ---------------------------------------------------------------------
 * PREV DEFINITION :
*/
struct Symbol;
struct Rule;
static Symbol** hashTable;
static Rule** rules; // used to free
static int num_rules = 0;
static int detectedRulesNum = 0;
Symbol** find_digram(Symbol* s);


/*
 * the distinguishes of  a rule and a rule symbol
 * a rule symbol is a symbol whose key points to the rule
*/

/* ----------------------------------------------------------------------
 * Struct of Rule:
 * Rule: Symbol1 -> Symbol2 -> Symbol3 -> ....... ->Symboln
 *                 |                                                                           |                                                                          |
 *                  ----<< n <<---------- guard -------->> p >>-----
 * Guard is a special non_terminal whose key can be converted to correspond Rule
*/

struct Rule {
    /* guard:
     * Guard shoud point forward to the first symbol in the rule and backward to the last symbol in the rule 
     * Its own value points to the rule data structure, so that Symbol can find out which rule they're in
    */
    Symbol* guard;
    // Reference count: Keeps track of the number of times the rule is used in the grammar
    int count;
    // To number the Rule for printing
    int index;
};

/* -------------------------------------------------------------------------------
 * RULE FUNCTIONS
*/
Rule* genRule() {
    Rule* newRule = (Rule*)malloc(sizeof(Rule));
    num_rules++;
    newRule->count = 0;
    newRule->index = 0;  // index==0 means the rule has not been appended to rules array
    newRule->guard = genSymbol(newRule);
    join(newRule->guard, newRule->guard); // make guard points to itself
    return newRule;
}

void destroyRule(Rule* this) {
    num_rules--;
    destroySymbol(this->guard);
    free(this);
}   

void reuse(Rule* this) {
    this->count++;
}

void deuse(Rule* this) {
    this->count--;
}

Symbol* first(Rule* this) {
    return this->guard->n;
}

Symbol* last(Rule* this) {
    return this->guard->p;
}

/* -------------------------------------------------------------------------------
 * Symbol Class
*/

struct Symbol {
    // pointers: next-pointer and prev-pointer
    Symbol *n, *p;
    ulong key;
    ulong value; // value = key / 2
    // to distinct is terminal or not
    // int (*nonTerminal)(Symbol* this);
    // // to distinct is guard or not 
    // int (*isGuard)(Symbol* this); 
    // void (*insertAfter)(Symbol* this, Symbol* new);
    // // removes the digram(self) from the hash table
    // void (*deleteDigram)(Symbol* this);
    // // void (*join)(Symbol* left, Symbol* right);
    // Rule* (*getRule)(Symbol* this);
    // void (*destroySymbol)(Symbol* this);
    // void (*substitute)(Symbol* this, Rule *r);
    // void (*match)(Symbol* newS, Symbol* oldS);
    // int (*check)(Symbol* this);
};

/* -------------------------------------------------------------------------------------
 * SYMBOL FUNCIONS
*/

// symbol parameter shouldn't be NULL
// void functionBind(Symbol* symbol) {
//     symbol->nonTerminal = nonTerminal;
//     symbol->isGuard = isGuard;
//     symbol->insertAfter = insertAfter;
//     symbol->deleteDigram = deleteDigram;
//     symbol->getRule = getRule;
//     symbol->destroySymbol = destroySymbol;
// }

// plant function: generate a new symbol
Symbol* genSymbol(ulong sym) {
    Symbol* newSymbol = (Symbol*)malloc(sizeof(Symbol));
    newSymbol->key = sym * 2 + 1;
    newSymbol->value = newSymbol->key / 2;
    newSymbol->p = newSymbol->n = NULL; // initialize with NULL
    // functionBind(newSymbol);
    return newSymbol;
}

// convert a rule into a symbol
Symbol* genSymbol(Rule* rule) {
    Symbol* newSymbol = (Symbol*)malloc(sizeof(Symbol));
    newSymbol->key = (ulong)rule;
    newSymbol->value = newSymbol->key / 2;
    newSymbol->p = newSymbol->n = NULL;
    reuse(rule);
    // functionBind(newSymbol);
    return newSymbol;
}

void destroySymbol(Symbol* this) {
    join(this->p, this->n);
    if (!isGuard(this)) {
        deleteDigram(this);
        if (nonTerminal(this)) {
            Rule* rule = getRule(this);
            deuse(rule);
        }
    }
    free(this);
}

/*
 * get the rule represented by the symbol 
*/
Rule* getRule(Symbol* this) {
    return (Rule*) this->key;
}

/*
 *  key can be used to distinct whether a symbol is terminal(rule) or not 
 *  rule is a kind of symbol
 *  If key is odd, the symbol is a terminal
 *  If key is even, the symbol is  non-terminal: a pointer to the referenced rule
 */
int nonTerminal(Symbol* this) {
    // rule pointer is 4-aligned or 8-aligned, while terminal pointer is always a odd number
    return (this->key%2 == 0) && (this->key != 0);
}

/*
 * as a guard, it should be a non-terminal, so that can be converted to a rule
 * the key of guard stores the address of the rule
*/
int isGuard(Symbol* this) {
    if (nonTerminal(this)) {
        Rule* rule = getRule(this);
        Symbol* rFirst = first(rule);
        return (rFirst->p==this);
    } 
    else
        return 0;
}

 /* 
  * links two symbols together
  * removing any old digram(contains the left/right) from the hash table
  */
void join(Symbol* left, Symbol* right) {
    if (left->n) {
        deleteDigram(left);
        if (right->p && right->n && right->value == right->p->value && right->value == right->n->value) {
            *find_digram(right) = right; // insert
        }
        if (left->p && left->n && left->value == left->n->value && left->value == left->p->value) {
            *find_digram(left->p) = left->p;
        }
    }
    left->n = right;
    right->p = left;
}

void deleteDigram(Symbol* this) {
    if (isGuard(this) || isGuard(this->n)) 
        return; // guard shouldn't be deleted
    Symbol** target = find_digram(this);
    if (*target == this) // to check the digram is existed in digram_table
        *target = (Symbol*) 1;
}

void insertAfter(Symbol* this, Symbol* new) {
    join(new, this->n);
    join(this, new);
}

/*
 * to check whether a new digram began with `this` has emerged in digram_table or not
 * if not emerged in digram_table, put it
 * else call function match to generate a new rule or reuse an old rule
 */
int check_put(Symbol* this) {
    if (isGuard(this) || isGuard(this->n))
        return 0;
    Symbol** x = find_digram(this);
    if ((ulong)*x <= 1) { // an empty place or a deleted element
        *x = this;  // the digram has not emerged before, insert it
        return 0;
    }
    // if find a existed digram
    if ((ulong)*x > 1 && (*x)->n != this)
        match(this, *x);
    return 1;
}

/* if a symbol is the last reference to its rule, we should delete the rule, 
 *   and replace the contents of the rule substituted in its place. 
 */
void expand(Symbol* this) {
    Symbol* left = this->p;
    Symbol* right = this->n;
    Rule* rule = getRule(this);
    Symbol* firstSym = first(rule);
    Symbol* lastSym = last(rule);

    destroyRule(rule);
    Symbol** m = find_digram(this);
    if (*m == this)
        *m = (Symbol*) 1; // delete the digram
    this->key = 0; // not point to the rule any more
    destroySymbol(this);
    // expand and delete a rule will generates two new digrams
    join(left, firstSym);
    join(lastSym, right);
    *find_digram(lastSym) = 1;
}

/*
 * if a digram has been emerged before
    * if has a rule to record the digram, use the rule to substitude the digram
    * else generate a new rule
 * newS: new coming digram 
 * oldS: digram which has been existed
 */ 
void match(Symbol* newS, Symbol* oldS) {
    Rule* r;
    // if oldS has been recorded as a rule
    if (isGuard(oldS->p) && isGuard(oldS->n->n)) {
        r = getRule(oldS->p);  // oldS->p is the guard of the rule
        substitute(newS, r);  
    }
    // the rule is not recorded
    else {
        r = genRule();    
        // generate symbols to insert into rule, origin symbols are deleted  
        if (nonTerminal(newS)) // if newS is a nonTerminal
            insertAfter(last(r), genSymbol(getRule(newS)));
        else 
            insertAfter(last(r), genSymbol(newS->value));

        if (nonTerminal(newS->n)) 
            insertAfter(last(r), genSymbol(getRule(newS->n)));
        else 
            insertAfter(last(r), genSymbol(newS->n->value));
        
        substitute(oldS, r);    
        substitute(newS, r);
        //  insert digram that has been converted to rule into table
        *find_digram(first(r)) = first(r);  
        // the symbol digram in the rule is not the origin symbol any more
    } 
    Symbol* firstSym = first(r);
    // if a rule is deused to 1 times, expand it and delete the rule
    if (nonTerminal(firstSym) && getRule(firstSym)->count == 1)
        expand(firstSym);
}

// A digram can be replaced with a non-terminal r
void substitute(Symbol* this, Rule* r) {
    Symbol *q = this->p;
    // destroy two symbols(which makes up of a digram)
    // destroy origin symbols is OK, because we will create new symbols to the rule in match function 
    destroySymbol(q->n); // q->n will be updated, so redestroy will not cause double free
    destroySymbol(q->n);
    // generate a new rule symbol
    Symbol* new = genSymbol(r); // generate a new symbol with rule
    insertAfter(q, new);
    if (!check_put(q))
        check_put(q->n);
}


/* --------------------------------------------------------------------------------------
 * Hash table structure:
 * key -> a memory pointer
 * Hash table function:
*/
#define HASH(one) (17 - ((one) % 17))
#define HASH2(one, two) (((one) << 16 | (two)) % PRIME)

/*
 * To find a digram began with symbol `s` 
*/
Symbol** find_digram(Symbol* s) {
    // s and s->n make up of a digram
    unsigned long one = s->key;
    unsigned long two = s->n->key;
    int jump = HASH(one);
    int i = HASH2(one, two);
    int insert = -1;
    
    while (1) {
        Symbol *m = hashTable[i];
        if (!m) { // not happen hash coincidence and can't find anything
            if (insert == -1)  
                insert = i;
            return &hashTable[insert]; // return an empty place
        }
        else if ((ulong)m == 1) // if the pointer has been deleted
            insert = i;
        else if (m->key == one && m->n->key == two) // find target digram
            return &hashTable[i];
        i = (i + jump) % PRIME;
    }
}

/* ---------------------------------------------------------------------------------------------
 * Compressed Data Structure 
*/
typedef struct LOCAL_OFFSET_UNIT 
{
    int32_t index; // index of the rule in rule_array array
    int32_t offset;          // the offset of the unit in data     
} LocalOffsetUnit;   // local offset unit can be completely initialized at the beginning

typedef struct RULE_DATA
{
    int32_t num_subrules; // LOCAL HEADER
    LocalOffsetUnit* local_offset_table;
    char* symbol_array;
    int32_t num_symbols;  // no acctually write into rule array
} RuleData; // rule data can be completely initialized at the beginning

typedef struct COMPRESSED_DATA
{
    int32_t num_rules; // GLOBAL HEADER
    int32_t* global_offset_table;
    RuleData** rule_array;
} CompressedData; // Compressed Data should be gradualy completed

/* 
 * discover a new rule that is not recorded in compressedData's ruleDatas array 
 * rule: discoverd rule
 * compressedData: used to record all discoverd rule
*/
int32_t discover(Rule* rule, CompressedData* compressedData) {
    // detected and record in rules
    rule->index = detectedRulesNum; 
    rules[detectedRulesNum] = rule;
    detectedRulesNum++;

    RuleData* ruleData = (RuleData*)malloc(sizeof(ruleData));
    int symbolNum = 0, i = 0, j = 0; // i is number of detected symbols,  j is number of detected rules 
    // check out all non-terminals of rule
    for (Symbol* s = first(rule); !isGuard(s); s = s->n, symbolNum++)
        if (nonTerminal(s)) 
            ruleData->num_subrules++;
    ruleData->num_symbols = symbolNum;
    ruleData->symbol_array = (char*)malloc(sizeof(char) * symbolNum);
    ruleData->local_offset_table = (LocalOffsetUnit*)malloc(sizeof(LocalOffsetUnit) * ruleData->num_subrules);
    // traverse all the symbol in rule
    for (Symbol* s = first(rule); !isGuard(s); s = s->n, i++) {
        if (nonTerminal(s)) { // if s is a rule
            ruleData->symbol_array[i] = '$'; // use $ as a placeholder for rule
            ruleData->local_offset_table[j].offset = i;
            // if the rule has been discovered and appeneded to rules array
            // the rule has not been discoverd will have an index 0
            if (getRule(s) == rules[getRule(s)->index]) {
                // rule.index is also the index in the rule array (of global structure)
                ruleData->local_offset_table[j].index = getRule(s)->index;
            }   
            // if the rule has not been detected
            else { 
                discover(s, compressedData); // DFS
                ruleData->local_offset_table[j].index = getRule(s)->index;
            }
            j++;
        } 
        // if s is not a rule, write into ruleData derectly
        else {
            ruleData->symbol_array[i] = (char)(s->value);
        }
    }
    // write rule data into compressedData
    compressedData->rule_array[rule->index] = ruleData;
}

/*
 * write rule data into an address 
 * dst: address to be written
 * ruleData: target rule(struct RuleData)
 * attention: no need to write `num_symbols`
 * return: how many byte of the rule data
*/
int32_t writeRuleData(void* dst, RuleData* ruleData) {
    ((int*)dst)[0] = ruleData->num_subrules;
    for (int i = 0; i < ruleData->num_subrules; ++i) {
        ((LocalOffsetUnit*)dst)[i] = ruleData->local_offset_table[i];
    }
    void* symbol_array_start = ((char*)dst) + (sizeof(int32_t)+sizeof(LocalOffsetUnit)*ruleData->num_subrules)/sizeof(char);
    memcpy(
        symbol_array_start, 
        ruleData->symbol_array, 
        ruleData->num_symbols
    );
    return sizeof(int32_t) + sizeof(LocalOffsetUnit) * ruleData->num_subrules + ruleData->num_symbols;
}

/*
 * read rule data from src
 * len: length of the rule
 * return rule data
*/
RuleData* readRuleData(void* src, int32_t len) {
    RuleData* ruleData = (RuleData*)malloc(sizeof(RuleData));
    ruleData->num_subrules = ((int*)src)[0];
    LocalOffsetUnit* local_offset_unit_start = (LocalOffsetUnit*)(((int*)src) + 1);
    for (int i = 0; i < ruleData->num_subrules; ++i) {
        ruleData->local_offset_table[i] = local_offset_unit_start[i];
    }
    int32_t metadata_len = (sizeof(int32_t)+sizeof(LocalOffsetUnit)*ruleData->num_subrules) / sizeof(char);
    void* symbol_array_start = ((char*)src) + metadata_len;
    ruleData->num_symbols = len - metadata_len;
    ruleData->symbol_array = (char*)malloc(ruleData->num_subrules * sizeof(char));
    memcpy(ruleData->symbol_array, symbol_array_start, ruleData->num_subrules);
    return ruleData;
}

/*
 * 
*/
int32_t writeCompressedData(void* dst, CompressedData* compressedData) {
    ((int32_t*)dst)[0] = compressedData->num_rules; // global header 
    int32_t start = compressedData->num_rules * sizeof(int32_t) / sizeof(char);
    int32_t firstRuleDataLen = writeRuleData(&(((char*)dst)[start]), compressedData->rule_array[0]);
    int32_t offset = start + firstRuleDataLen;
    for (int i = 1; i < compressedData->num_rules; ++i) {
        int32_t writeBytes = writeRuleData(&(((char*)dst)[offset]), compressedData->rule_array[i]);
        ((int32_t*)dst)[i] = compressedData->global_offset_table[i-1]  = offset - start;
        offset += writeBytes;
    }
    return offset;
}

void destroyCompressedData(CompressedData* compressedData) {
    for (int i = 0; i < compressedData->num_rules; ++i) {
        free(compressedData->rule_array[i]);
    }
    free(compressedData->global_offset_table);
}

/*
 * tadoc_compress -
 *            Compress Source into dest. 
 *            Return the number of bytes written in buffer dest, 
 *            or -1 if compression fails.
*/
int32_t tadoc_compress(const char *source, int32_t slen, char *dest) {
    Rule *S = genRule();
    insertAfter(last(S), genSymbol(source[0]));
    // generate Rule S
    for (int i = 1; i < slen; ++i) {
        char ch = source[i];
        insertAfter(last(S), genSymbol(ch));
        check_put(last(S)->p);
    }   
    // now all the rules are stored in heap memory, we need to parse them
    CompressedData* compressedData = (CompressedData*)malloc(sizeof(CompressedData));
    compressedData->num_rules = num_rules;
    compressedData->rule_array = (RuleData**)malloc(sizeof(RuleData*) * num_rules);
    compressedData->global_offset_table = (int32_t*)malloc(sizeof(int32_t) * (num_rules - 1));
    discover(S, compressedData);
    int32_t totalWriteBytes = writeCompressedData(dest, compressedData);
    // free rules
    for (int i  = 0; i < detectedRulesNum; ++i) {
        destroyRule(rules[i]);
    }
    destroyCompressedData(compressedData);
    return totalWriteBytes;
}

/*
 * tadoc_decompress -
 * 
 *      Decompress source into dest.
 *      Return the number of bytes decompressed into the destination buffer, 
 *      or -1 if the compressed data is corrupted
 *  	
 *      If check_complete is true, the data is considered corrupted
 *		if we don't exactly fill the destination buffer.  Callers that
 *		are extracting a slice typically can't apply this check.
*/
int32_t tadoc_decompress(const char *source, int32_t slen, char *dest,
			                        int32_t rawsize, int32_t check_complete){
    // 1. decode compress data
    CompressedData *compressedData = (CompressedData*)malloc(sizeof(CompressedData));
    // 1.1 decode compress data header
    compressedData->num_rules = ((int32_t*)source)[0];
    // 1.2 decode compress data offset table
    compressedData->global_offset_table = (int32_t*)malloc(sizeof(int32_t) * num_rules - 1);
    for (int i = 1; i < compressedData->num_rules; ++i) {
        compressedData->global_offset_table[i-1] = ((int32_t*)source)[i];
    }
    // 1.3 decode compress data rules array
    compressedData->rule_array = (RuleData**)malloc(sizeof(RuleData*) * compressedData->num_rules);
    int32_t start = sizeof(int32_t)* compressedData->num_rules / sizeof(char*);

    // read rules array
    

    // TODO: decompressRuleData algorithm (recurse)

}