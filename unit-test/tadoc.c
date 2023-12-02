#include <limits.h>
#include <string.h>
#include <stdlib.h> // to use NULL pointer
#include <stdio.h>
#include "tadoc.h"

#define DISPLAY 1
#define debug printf

#define PRIME 2265539

/* ---------------------------------------------------------------------
 * PREV DEFINITION :
*/
static Symbol** hashTable;
static Rule** rules; // used to free
static int num_rules = 0;
static int detectedRulesNum = 0;

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
 * Symbol Struct
*/
struct Symbol {
    // pointers: next-pointer and prev-pointer
    Symbol *n, *p;
    uint64_t key;
    uint64_t value; // value = key / 2
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

/* -------------------------------------------------------------------------------
 * RULE FUNCTIONS
*/
Rule* genRule() {
    Rule* newRule = (Rule*)malloc(sizeof(Rule));
    num_rules++;
    newRule->count = 0;
    newRule->index = 0;  // index==0 means the rule has not been appended to rules array
    newRule->guard = genSymbol2(newRule);
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
Symbol* genSymbol1(uint64_t sym) {
    Symbol* newSymbol = (Symbol*)malloc(sizeof(Symbol));
    newSymbol->key = sym * 2 + 1;
    newSymbol->value = newSymbol->key / 2;
    newSymbol->p = newSymbol->n = NULL; // initialize with NULL
    // functionBind(newSymbol);
    return newSymbol;
}

// convert a rule into a symbol
Symbol* genSymbol2(Rule* rule) {
    Symbol* newSymbol = (Symbol*)malloc(sizeof(Symbol));
    newSymbol->key = (uint64_t)rule;
    newSymbol->value = newSymbol->key / 2;
    newSymbol->p = newSymbol->n = NULL;
    reuse(rule);
    // functionBind(newSymbol);
    return newSymbol;
}

/*
 * get the rule represented by the symbol 
*/
Rule* getRule(Symbol* this) {
    return (Rule*) this->key;
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
        Symbol* rLast = last(rule);
        return (rLast->n==this);
    } 
    else
        return 0;
}

 /* 
  * links two symbols together
  * removing any old digram(contains the left/right) from the hash table
  */
void join(Symbol* left, Symbol* right) {
    debug("join: left->%c, right->%c\n", nonTerminal(left) ? '$' : (char)(left->value), nonTerminal(right) ? '$' : (char)(right->value));
    if (left->n) {
        debug("join: left has next %c\n", nonTerminal(left->n) ? '$' : (char)(left->n->value));
        if (isGuard(left))
            debug("join: left is guard\n");
        deleteDigram(left);
        if (right->p && right->n && right->value == right->p->value && right->value == right->n->value) {
            *findDigram(right) = right; // insert
        }
        if (left->p && left->n && left->value == left->n->value && left->value == left->p->value) {
            *findDigram(left->p) = left->p;
        }
    }
    left->n = right;
    right->p = left;
    debug("---- finish join ----\n");
}

void deleteDigram(Symbol* this) {
    if (isGuard(this) || isGuard(this->n)) {
        debug("deleteDigram: (%c, %c)\n", '-', '-');
        return; // guard shouldn't be deleted
    }
    Symbol** target = findDigram(this);
    if (*target == this) // to check the digram is existed in digram_table
        *target = (Symbol*) 1;
}

void insertAfter(Symbol* this, Symbol* new) {
    debug("insert %c after %c\n", nonTerminal(new) ? '$' : (char)(new->value), nonTerminal(this) ? '$' : (char)(this->value));
    join(new, this->n);
    join(this, new);
}

/*
 * to check whether a new digram began with `this` has emerged in digram_table or not
 * if not emerged in digram_table, put it
 * else call function match to generate a new rule or reuse an old rule
 */
int check_put(Symbol* this) {
    if (isGuard(this) || isGuard(this->n)) {
        return 0;
    }
    Symbol** x = findDigram(this);
    if ((uint64_t)*x <= 1) { // an empty place or a deleted element
        *x = this;  // the digram has not emerged before, insert it
        return 0;
    }
    // if find a existed digram
    if ((uint64_t)*x > 1 && (*x)->n != this)
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
    Symbol** m = findDigram(this);
    if (*m == this)
        *m = (Symbol*) 1; // delete the digram
    this->key = 0; // not point to the rule any more
    destroySymbol(this);
    // expand and delete a rule will generates two new digrams
    join(left, firstSym);
    join(lastSym, right);
    *findDigram(lastSym) = (Symbol*)1;
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
            insertAfter(last(r), genSymbol2(getRule(newS)));
        else 
            insertAfter(last(r), genSymbol1(newS->value));

        if (nonTerminal(newS->n)) 
            insertAfter(last(r), genSymbol2(getRule(newS->n)));
        else 
            insertAfter(last(r), genSymbol1(newS->n->value));
        
        substitute(oldS, r);    
        substitute(newS, r);
        //  insert digram that has been converted to rule into table
        *findDigram(first(r)) = first(r);  
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
    Symbol* new = genSymbol2(r); // generate a new symbol with rule
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

void initHashTable(int32_t size) {
    hashTable = (Symbol**)malloc(sizeof(Symbol*) * size);
}

void clearHashTable() {
    free(hashTable);
}

/*
 * To find a digram began with symbol `s` 
*/
Symbol** findDigram(Symbol* s) {
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
        else if ((uint64_t)m == 1) // if the pointer has been deleted
            insert = i;
        else if (m->key == one && m->n->key == two) // find target digram
            return &hashTable[i];
        i = (i + jump) % PRIME;
    }
}

/* ---------------------------------------------------------------------------------------------
 * Compressed Data Structure 
*/
struct LocalOffsetUnit
{
    int32_t index; // index of the rule in rule_array array
    int32_t offset;          // the offset of the unit in data     
};   // local offset unit can be completely initialized at the beginning

struct RuleData
{
    int32_t num_subrules; // LOCAL HEADER
    LocalOffsetUnit* local_offset_table;
    char* symbol_array;
    int32_t num_symbols;  // no acctually write into rule array
}; // rule data can be completely initialized at the beginning

struct CompressedData
{
    int32_t num_rules; // GLOBAL HEADER
    int32_t* global_offset_table;
    RuleData** rule_array;
}; // Compressed Data should be gradualy completed

/* 
 * discover a new rule that is not recorded in compressed_data's ruleDatas array 
 * rule: discoverd rule
 * compressed_data: used to record all discoverd rule
*/
int32_t discover(Rule* rule, CompressedData* compressed_data) {
    // detected and record in rules
    rule->index = detectedRulesNum; 
    rules[detectedRulesNum] = rule;
    detectedRulesNum++;

    RuleData* rule_data = (RuleData*)malloc(sizeof(rule_data));
    int num_symbols = 0, i = 0, j = 0; // i is number of detected symbols,  j is number of detected symbol-rules 
    // check out all non-terminals of rule
    for (Symbol* s = first(rule); !isGuard(s); s = s->n, num_symbols++)
        if (nonTerminal(s)) 
            rule_data->num_subrules++;
    rule_data->num_symbols = num_symbols;
    rule_data->symbol_array = (char*)malloc(sizeof(char) * num_symbols);
    rule_data->local_offset_table = (LocalOffsetUnit*)malloc(sizeof(LocalOffsetUnit) * rule_data->num_subrules);
    // traverse all the symbol in rule
    for (Symbol* s = first(rule); !isGuard(s); s = s->n, i++) {
        if (nonTerminal(s)) { // if s is a rule
            rule_data->symbol_array[i] = '$'; // use $ as a placeholder for rule
            rule_data->local_offset_table[j].offset = i;
            // the rule has not been discoverd will have an index 0
            // if the rule has been discovered, derectly push back is OK
            if (getRule(s)->index == 0) {
                discover(getRule(s), compressed_data); // DFS
            }
            // rule.index is also the index in the rule array (of global structure)
            rule_data->local_offset_table[j].index = getRule(s)->index;
            j++;
        } 
        // if s is not a rule, write into rule_data derectly
        else {
            rule_data->symbol_array[i] = (char)(s->value);
        }
    }
    // write rule data into compressed_data
    compressed_data->rule_array[rule->index] = rule_data;
}

/*
 * write rule data into an address 
 * dest: address to be written
 * rule_data: target rule(struct RuleData)
 * attention: no need to write `num_symbols`
 * return: how many byte of the rule data
*/
int32_t writeRuleData(void* dest, RuleData* rule_data) {
    ((int*)dest)[0] = rule_data->num_subrules;
    for (int i = 0; i < rule_data->num_subrules; ++i) {
        ((LocalOffsetUnit*)dest)[i] = rule_data->local_offset_table[i];
    }
    void* symbol_array_start = ((char*)dest) + (sizeof(int32_t)+sizeof(LocalOffsetUnit)*rule_data->num_subrules)/sizeof(char);
    memcpy(
        symbol_array_start, 
        rule_data->symbol_array, 
        rule_data->num_symbols
    );
    return sizeof(int32_t) + sizeof(LocalOffsetUnit) * rule_data->num_subrules + rule_data->num_symbols;
}

/*
 * read rule data from src
 * len: length of the rule
 * return rule data
*/
RuleData* readRuleData(void* src, int32_t len) {
    RuleData* rule_data = (RuleData*)malloc(sizeof(RuleData));
    rule_data->num_subrules = ((int*)src)[0];
    LocalOffsetUnit* local_offset_unit_start = (LocalOffsetUnit*)(((int*)src) + 1);
    for (int i = 0; i < rule_data->num_subrules; ++i) {
        rule_data->local_offset_table[i] = local_offset_unit_start[i];
    }
    int32_t metadata_len = (sizeof(int32_t)+sizeof(LocalOffsetUnit)*rule_data->num_subrules) / sizeof(char);
    void* symbol_array_start = ((char*)src) + metadata_len;
    rule_data->num_symbols = len - metadata_len;
    rule_data->symbol_array = (char*)malloc(rule_data->num_subrules * sizeof(char));
    memcpy(rule_data->symbol_array, symbol_array_start, rule_data->num_subrules);
    return rule_data;
}

/*
 * write compressed data to dest
*/
int32_t writeCompressedData(void* dest, CompressedData* compressed_data) {
    ((int32_t*)dest)[0] = compressed_data->num_rules; // global header 
    char* rule_array_start = (char*)(compressed_data->num_rules * sizeof(int32_t) / sizeof(char));
    int32_t first_rule_data_len = writeRuleData(rule_array_start, compressed_data->rule_array[0]);
    char* rule_array_offset = rule_array_start + first_rule_data_len;
    int32_t* global_offset_table_start = ((int32_t*)dest) + 1;
    for (int i = 0; i < compressed_data->num_rules  - 1; ++i) {
        global_offset_table_start[i] = compressed_data->global_offset_table[i]  = rule_array_offset - rule_array_start;
        int32_t write_bytes = writeRuleData(rule_array_offset, compressed_data->rule_array[i+1]);
        rule_array_offset += write_bytes;
    }
    return rule_array_offset - (char*)dest;
}

void displayCompressedData(CompressedData* compressed_data) {
    for (int i = 0; i < compressed_data->num_rules; ++i) {
        RuleData* rule_data = compressed_data->rule_array[i];
        printf("rule $%d: ", i);
        for (int j = 0, k = 0; j < rule_data->num_symbols; ++j) { // double pointers
            // the symbol is a rule symbol
            if (j == rule_data->local_offset_table[k].offset) {
                printf("$%d ", rule_data->local_offset_table[k].index);
                k++;
            }   
            // the symbol is a normal symbol
            else {
                printf("%c ", rule_data->symbol_array[j]);
            }
        }
        printf("\n");
    }
}

void destroyCompressedData(CompressedData* compressed_data) {
    for (int i = 0; i < compressed_data->num_rules; ++i) {
        free(compressed_data->rule_array[i]);
    }
    free(compressed_data->global_offset_table);
}

int32_t restore(void* dest, RuleData* rule_data, CompressedData* compressed_data) {
    char* write_pointer = dest;
    for (int j = 0, k = 0; j < rule_data->num_symbols; j++) { // double pointers
        // the symbol is a rule symbol
        if (j == rule_data->local_offset_table[k].offset) {
            int32_t index = rule_data->local_offset_table[k].index;
            write_pointer += restore(write_pointer, compressed_data->rule_array[index], compressed_data);
            k++;
        }   
        // the symbol is a normal symbol
        else {
            write_pointer[0] = rule_data->symbol_array[j];
            write_pointer += 1;
        }
    }
    return write_pointer - ((char*)dest);
}

/*
 * tadoc_compress -
 *            Compress Source into dest. 
 *            Return the number of bytes written in buffer dest, 
 *            or -1 if compression fails.
*/
int32_t tadoc_compress(const char *source, int32_t slen, char *dest) {
    initHashTable(PRIME);
    Rule *S = genRule();
    insertAfter(last(S), genSymbol1(source[0]));
    // generate Rule S
    for (int i = 1; i < slen; ++i) {
        char ch = source[i];
        debug("compress ch %c\n", ch);
        insertAfter(last(S), genSymbol1(ch));
        check_put(last(S)->p);
    }   
    // now all the rules are stored in heap memory, we need to parse them
    CompressedData* compressed_data = (CompressedData*)malloc(sizeof(CompressedData));
    compressed_data->num_rules = num_rules;
    compressed_data->rule_array = (RuleData**)malloc(sizeof(RuleData*) * num_rules);
    compressed_data->global_offset_table = (int32_t*)malloc(sizeof(int32_t) * (num_rules - 1));
    discover(S, compressed_data);
    // display rules
    if (DISPLAY) {
        displayCompressedData(compressed_data);
    }
    int32_t totalWriteBytes = writeCompressedData(dest, compressed_data);
    // free rules
    for (int i  = 0; i < detectedRulesNum; ++i) {
        destroyRule(rules[i]);
    }
    destroyCompressedData(compressed_data);
    clearHashTable();
    debug("finish tadoc compress function\n");
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
    CompressedData *compressed_data = (CompressedData*)malloc(sizeof(CompressedData));
    // 1.1 decode compress data header
    compressed_data->num_rules = ((int32_t*)source)[0];
    // 1.2 decode compress data offset table
    compressed_data->global_offset_table = (int32_t*)malloc(sizeof(int32_t) * num_rules - 1);
    int32_t* global_offset_table_start = ((int32_t*)source) + 1;
    for (int i = 0; i < compressed_data->num_rules - 1; ++i) {
        compressed_data->global_offset_table[i] = global_offset_table_start[i];
    }
    // 1.3 decode compress data rules array
    compressed_data->rule_array = (RuleData**)malloc(sizeof(RuleData*) * compressed_data->num_rules);
    char* rule_array_start = ((char*)source) + sizeof(int32_t) * compressed_data->num_rules / sizeof(char*);
    char* rule_array_offset = rule_array_start;
    // 1.4 decode rules array
    compressed_data->rule_array[0] = readRuleData(rule_array_start, compressed_data->global_offset_table[0]);
    for (int i = 1; i < compressed_data->num_rules; ++i) {
        int32_t length = compressed_data->global_offset_table[i] - compressed_data->global_offset_table[i-1];
        rule_array_offset = rule_array_start + compressed_data->global_offset_table[i-1];
        compressed_data->rule_array[i] = readRuleData(rule_array_offset, length);
    }
    // 1.5 display rules
    if (DISPLAY) {
        displayCompressedData(compressed_data);
    }
    // 2. decompress RuleData algorithm (recurse)
    int32_t write_bytes = restore(dest, compressed_data->rule_array[0], compressed_data);   
    if (check_complete) {
        if (write_bytes != rawsize) {
            printf("error decompress!\n");
        }
    }
    destroyCompressedData(compressed_data);
    return write_bytes;
}

int main() {
    char src[100] = "abcdbcabcd";
    char compressed_dest[1000] = {0};
    int32_t src_len = 10;
    int32_t compressed_bytes = tadoc_compress(src, src_len, compressed_dest);
    char decompressed_dest[1000] = {0};
    int32_t decompressed_bytes = tadoc_decompress(compressed_dest, compressed_bytes, decompressed_dest, src_len, 1);
    return 0;
}