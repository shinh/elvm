#include "calc.h"

typedef enum {
    NUM, ADD, MUL
} Nodetype;

typedef struct Node {
    Nodetype type;
    union {
        int val;
        struct Node* l;
    };
    struct Node* r;
} Node;

Node* parseFactor();
Node* parseTerm();
Node* parseExpr();

Node* parseFactor() {
    Node* node;
    char c = getchar();
    if (c == '(') {
        node = parseExpr();
        getchar(); // Pop the incoming ')' from the buffer
    } else {
        node = malloc(sizeof(Node));
        node->type = NUM;
        node->val = c - '0';
    }
    return node;
}

Node* parseLoop(Node* (*self)(), Node* (*looptoken)(), Nodetype nodetype, char op) {
    Node* retnode = (*looptoken)();
    if (curchar() == op) {
        getchar();
        Node* newnode = malloc(sizeof(Node));
        newnode->type = nodetype;
        newnode->l = retnode;
        newnode->r = (*self)();
        retnode = newnode;
    }
    return retnode;
}

Node* parseTerm() {
    return parseLoop(&parseTerm, &parseFactor, MUL, '*');
}

Node* parseExpr() {
    return parseLoop(&parseExpr, &parseTerm, ADD, '+');
}

int eval (Node* node) {
    if (node->type == NUM) {
        return node->val;
    }
    int v_l = eval(node->l);
    int v_r = eval(node->r);
    return node->type == ADD ? (v_l + v_r) : mul(v_l, v_r);
}

int main (void) {
    Node* parsed = parseExpr();
    putchar('0' + eval(parsed));
}
