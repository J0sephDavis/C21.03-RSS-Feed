#ifndef regex_c
#define regex_c
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#define PRINT_MESSAGES 0
/***                           	  the regex class                                   ***/
struct regex_t {
	int rule;
	int literal;
	struct regex_t* next;
	struct regex_t* alternate;
	struct regex_t* child;
};
typedef struct regex_t* regex;
//the rules that change functionality
//some rules such as using a "\" to indicate a symbol is literal,
//are handled during compilation & thus don't need a named rule.
//
//using an enum is restricting, unless I guess we just make every combination of values.. handling (a*bc)? means a parent of R_OPT with child a* who has alternates b & c.
//but if we want to use alternates,
enum rules {
	R_CHAR = 0,
	R_STAR,
	R_PLUS,
	R_OPT,
	R_DIGIT, 	//DIGIT - numbers 0-9
	R_ALPHA, 	//ALPHA - letters a-zA-Z
	R_ALPHANUMERIC, //ALPHANUMERIC - any letter or number
	R_ANCHOR_START,
	R_ANCHOR_END,
};
const char* rules_names[] = {
	"char",
	"star",
	"plus",
	"opt",
	"digit",
	"letter",
	"alphanumeric",
	"start anchor",
	"end anchor",
};
/** 	      prototypes 	        **/
regex re_create(int, int);
void re_destroy(regex);   
int re_getRule(regex instance);
void re_setRule(regex, int);
int re_getLiteral(regex);
regex re_getNext(regex);
void re_setNext(regex, regex);
void re_setAlternate(regex, regex);
regex re_getAlternate(regex);
void re_addChild(regex, regex);
regex re_getChild(regex);
void re_print(regex);
//creates a regex object with specified rule & literal
regex re_create(int _rule, int _literal) {
	regex instance = (regex_t *)calloc(1, sizeof(struct regex_t));
	instance->rule = _rule;
	instance->literal = _literal;
	instance->next = NULL;
	instance->alternate = NULL; 	//should we only find these in a regex child? I think so, imagine the tree, it has to split only at child nodes.
//we can reduce our levels of recursion by reducing the regex objects we create
//E.g., creating a char* containing a set of characters to match, but if we had a (ab)*
//we would be in a situation where we have to memorize which character, a OR b, we are currently allowing to repeat.
//because (ab)* is similar to (a*b*), but in the future we will make these an entry more similar to a*.next = b*
//and b*.next is whatever the parent node's next is. Then, (a*b*) != (ab)*. (ab)* will become (a*|b*) or maybe (a|b)*
//this will likely take some time to fully explore, but I ought to see how other regex systems handle all this
	instance->child = NULL;
	return instance;
}
//destroys the SPECIFIED node, not its children or related nodes
void re_destroy(regex instance) {
	if(PRINT_MESSAGES)
		{printf("FREE:"); re_print(instance); printf("\n");}
	free(instance);
}
//returns the rule of the node
int re_getRule(regex instance) {
	return instance->rule;
}
//sets the rule of the node
void re_setRule(regex instance, int _rule) {
	instance->rule = _rule;
}
//gets the literal of the node
int re_getLiteral(regex instance) {
	return instance->literal;
}
//returns the next node from the node
regex re_getNext(regex instance) {
	return instance->next;
}
//sets the next node of the current node
void re_setNext(regex instance, regex next_instance) {
	instance->next = next_instance;
}
regex re_getAlternate(regex instance) {
	return instance->alternate;
}
void re_setAlternate(regex instance, regex alternate) {
	if (instance->alternate != NULL)
		re_setAlternate(instance->alternate, alternate);
	else
		instance->alternate = alternate;
}
void re_addChild(regex instance, regex child) {
	if (instance->child) {
		re_setAlternate(instance->child,child);
	}
	else
		instance->child = child;
}
regex re_getChild(regex instance) {
	return instance->child;
}
//prints the nodes information
void re_print(regex instance) {
	if (instance)
		printf("[Rule: %s | Literal: %c | Next? : %s | Alt?: %s | Child?: %s]",
				rules_names[instance->rule], instance->literal,
				(instance->next) ? "Yes" : "No",
				(re_getAlternate(instance) ? "yes" : "no"),
				(re_getChild(instance) ? "yes" : "no"));
	else printf("[-]");
}
/*** 				regex compilation 				    ***/
/* The following code is for compiling the    *//** 	      prototypes 	        **/
/*regex from an input string into the linked  */bool re_char_to_reptition_rule(regex, char);
/*regex nodes.                                */int char_to_substitution_rule(char);
/*                                            */regex re_create_f_str(const char*);
//if the character is a repetition rule, the RE's rule will be changed. otherwise it is not touched & false is returned
bool re_char_to_reptition_rule(regex instance, char c) {
	switch(c) {
		case('*'):
			re_setRule(instance,R_STAR);
			return true;
		case('?'):
			re_setRule(instance,R_OPT);
			return true;
		case('+'):
			re_setRule(instance,R_PLUS);
			return true;
		default:
			return false;
	}
}
//return either the matching substitution rule or R_CHAR
int char_to_substitution_rule(char c) {
	switch(c) {
		case('.'):
			return R_ALPHANUMERIC;
		case('#'):
			return R_DIGIT;
		case('&'):
			return R_ALPHA;
	}
	return R_CHAR;
}
//create a node-tree of REs from an input string
regex re_create_f_str(const char* regexp) {
	if (regexp == NULL) 			//if we are given an empty string
		return NULL; 			//return nothing
	//
	regex first_node = NULL;	 	//pointer to the first node (also returned at the end)
	regex last_node = NULL; 		//point to the last node made.
	regex parent_node = NULL; 		//pointer to the parent_node that needs alternates.
	//
	int literal; 				//stores the current character in the regexp
	int rule; 				//stores the current rule
	//
	bool in_parent = false; 		//is the character string still inside the parentheses?
	bool last_backslash = false; 		//isntead of using a regexp[a] == '\' && regexp[a+1] == symbol... we just set a flag. probably inefficient
	bool child_next = true; 		//if the node should be the NEXT (true) pointer for a child, or if it should be an alternate (FALSE)
	//
	int a; 					//iterator
//iterate over all the passed characters
	for (a = 0; regexp[a] != '\0'; a++) {
		rule = R_CHAR;
		literal = regexp[a];
		if (PRINT_MESSAGES) printf("-PROCESSING:%c\n", literal);
//RULE APPLICATION
		if (!last_backslash) {
			if (PRINT_MESSAGES) printf("I:!backslash\n");
			if (regexp[a] == '\\') {
				if (PRINT_MESSAGES) printf("\ta:\\-skip\n");
				last_backslash = true;
				continue;
			}
			else if (regexp[a] == ')') {
				if (PRINT_MESSAGES) printf("\tb:')'-skip\n");
				in_parent = false;
				last_node = parent_node; //if the next character is a repetition symbol, it will apply to the parent!
				continue;
			}
			else if (!in_parent && regexp[a] == '(') {
				if (PRINT_MESSAGES) printf("\tc:'('\n");
				in_parent = true;
			}
			//because we are not dealing with a literal, apply the rule to the last node.
			else if (re_char_to_reptition_rule(last_node, regexp[a])) {
				if (PRINT_MESSAGES) printf("\td:rep apply-skip\n");
				continue;
			}
			else if (in_parent && regexp[a] == '|') {
				if (PRINT_MESSAGES) printf("\te:'|': child-next->false-skip\n");
				child_next = false;
				continue;
			}
			else {
				if (PRINT_MESSAGES) printf("\te:sub rule\n");
				rule = char_to_substitution_rule(regexp[a]);
			}

		}
		else { 
			if (PRINT_MESSAGES) printf("II:backslash\n");
			last_backslash = false;
		}
//CREATE NODE
		regex tmp_re = re_create(rule,literal);
//LINK NODE(S)
		if (!first_node) {
			if (PRINT_MESSAGES) printf("III:!first_node - skip\n");
			first_node = tmp_re;
			last_node = tmp_re;
			if (in_parent && !parent_node){
				if (PRINT_MESSAGES) printf("\ta:in_parent && !parent_node: parent = new\n");
				parent_node = tmp_re;
			}
			continue;
		}
		if (in_parent) {
			if (PRINT_MESSAGES) printf("IV:in_parent\n");
			if (parent_node) {
					if (PRINT_MESSAGES) printf("\ta:parent_node: ADD CHILD(parent, new)\n");
					//if there is a child & we want to append a child to the last
					if (re_getChild(parent_node) && child_next) {
						if (PRINT_MESSAGES) printf("\t\ti:already has a child && child_next, append to child\n");
						re_setNext(last_node, tmp_re);
					}
					//either no previous child, or we don't want to append a child to the last
					else {
						if (PRINT_MESSAGES) printf("\t\tii:add a child\n");
						re_addChild(parent_node, tmp_re);
					}
			}
			else {
				if (PRINT_MESSAGES) printf("\tb:!parent_node: SET NEXT & parent = new. last.next=parent\n");
				parent_node = tmp_re;
				re_setNext(last_node, parent_node);
			}
		}
		else {
			if (PRINT_MESSAGES) printf("V:!in_parent\n");
			if (parent_node) {
				if (PRINT_MESSAGES) printf("\ta:parent_node: EACH:parent.child.alternate.next = new\n");
				regex tmp_child = re_getChild(parent_node);
				while (tmp_child != NULL) {
					if (re_getNext(tmp_child)) {
						if (PRINT_MESSAGES) printf("\t\ti:child has next, find end & link\n");
						regex tmp_subChild = re_getNext(tmp_child);
						while (re_getNext(tmp_subChild))
							tmp_subChild = re_getNext(tmp_subChild);
						re_setNext(tmp_subChild, tmp_re);
					}
					else {
						if (PRINT_MESSAGES) printf("\t\tii:child has no descendants,link to next\n");
						re_setNext(tmp_child, tmp_re);
					}
					tmp_child = re_getAlternate(tmp_child);
				}
				re_setNext(parent_node, tmp_re);
				parent_node = NULL;
			}
			else {
				if (PRINT_MESSAGES) printf("\tb:!parent_node: set next(last->new)\n");
				re_setNext(last_node, tmp_re);
			}
		}
		if (PRINT_MESSAGES) printf("VII:last->new\n");
		//reset the var
		if (!child_next) child_next = true;
		if (PRINT_MESSAGES&&!child_next) printf("VIII:child_next = true\n");
		last_node = tmp_re;
	}
	return first_node;
}
/*** 					matching 				    ***/
/* match is called from the root node & text. *//** 	      prototypes 	        **/
/* match-here is called when the text is to be*/bool match(regex instance, const char* text);
/* moved.                                     */bool m_here(regex instance, const char* text);

/* calls the proper rule for the instance     */bool m_rule(regex instance, const char* text);
/*                                            */bool m_char(regex instance, const char* text);
/*                                            */bool m_star(regex instance, const char* text);
/*                                            */bool m_plus(regex instance, const char* text);
/*                                            */bool m_optional(regex instance, const char* text);

/*                                            */bool m_alpha(regex instance, const char* text);
/*                                            */bool m_digit(regex instance, const char* text);
/*                                            */bool m_alphanum(regex instance, const char* text);

/* calls the proper parent function for the   */bool m_parent(regex instance, const char* text);
/*instance                                    */bool m_parent_char(regex instance, const char* text);
/*                                            */bool m_parent_star(regex instance, const char* text);
/*                                            */bool m_parent_plus(regex instance, const char* text);
/*                                            */bool m_parent_optional(regex instance, const char* text);
/** 	      implementation         **/
//returns true if the expression found a match in the text, otherwise it returns false.
bool match(regex expression, const char* text) {
	do {
		if(PRINT_MESSAGES)printf("<-match-loop->\n");
		if (m_here(expression, text))
			return true;
	} while(*text++ != '\0');
	return false; 
}
bool m_parent(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("M_PARENT:%s: ", rules_names[instance->rule]); re_print(instance); printf("%s\n", text);}
	switch(re_getRule(instance)) {
		default:
		case (R_CHAR):
			return m_parent_char(re_getChild(instance),text);
		case (R_STAR):
			return m_parent_star(re_getChild(instance), text);
		case (R_PLUS):
			return m_parent_plus(re_getChild(instance),text);
		case (R_OPT):
			return m_parent_optional(re_getChild(instance), text);
	}
}
bool m_parent_char(regex instance, const char* text) {
	while(instance) {
		if (PRINT_MESSAGES) printf("<mp_char loop>\n");
		if (m_here(instance, text))
			return true;
		instance = re_getAlternate(instance);
	}
	return false;
}
bool m_parent_star(regex instance, const char* text) {	
	while (instance) {
		const char* tmp_text = text;
		do {
			if (PRINT_MESSAGES) printf("<mp_star loop>\n");
			if (m_here(instance, text))
				return true;
		} while (*text != '\0' && re_getLiteral(instance) == *tmp_text++); //I have a feeling this logic will bite me one day
		instance = re_getAlternate(instance);	
	}
	return true;
}
bool m_parent_plus(regex instance, const char* text) {
	while(instance) {
		if (PRINT_MESSAGES) printf("<mp_plus loop>\n");
		const char* tmp_text = text;
		while (*text != '\0' && re_getLiteral(instance) == *++tmp_text) {
			if (m_here(instance,text))
				return true;
		};
		instance = re_getAlternate(instance);
	}
	return false;
}
bool m_parent_optional(regex instance, const char* text) {
	if (PRINT_MESSAGES) printf("<mp_plus loop>\n");

	if (instance != NULL && text != NULL)
		return false;
	else return false;
}
bool m_rule(regex instance, const char* text) {
	switch(re_getRule(instance)) {
		case (R_CHAR):
			return m_char(instance,text);
		case (R_STAR):
			return m_star(instance, text);
		case (R_PLUS):
			return m_plus(instance,text);
		case (R_OPT):
			return m_optional(instance, text);
		case (R_ALPHA):
			return m_alpha(instance, text);
		case (R_DIGIT):
			return m_digit(instance, text);
		default: //doubtful that it can reach this point
			return false;
	}
}
bool m_here(regex instance, const char* text) {
//	if(PRINT_MESSAGES){printf("m_HERE: "); re_print(instance); printf("\n");}
	//if the instance is empty, we are out of rules!
	if (!instance)
		return true;
	//process rules
	regex child = re_getChild(instance);
	//attempt to match from the children,
	if (!child && m_rule(instance,text))
		return true;
	else if (child && m_parent(instance, text))
		return true;
	//if there was no match before, try the alternative branch
	//if we couldn't find any matches... RIP
	else return false;
}
//match a single literal
bool m_char(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_char: "); re_print(instance); printf("%s\n", text);}
	if(re_getLiteral(instance) == *text){
		return m_here(re_getNext(instance), text+1);
	}
	else return false;
}
//match the instance 0 or more times
bool m_star(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_star: "); re_print(instance); printf("%s\n", text);}
	do {
		if (m_here(re_getNext(instance), text))
			return true;
	} while (*text != '\0' && re_getLiteral(instance) == *text++);
	return false;
}
//match one or more times
bool m_plus(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_plus: "); re_print(instance); printf("%s\n", text);}
	while (*text != '\0' && re_getLiteral(instance) == *++text) {
		if (m_here(re_getNext(instance), text+1)) {
			return true;
		}
	}
	return 0;
}
//match ZERO or ONE times
bool m_optional(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_opt: "); re_print(instance); printf("%s\n", text);}
	if (m_char(instance, text)) 		//if we find the character
		return m_here(re_getNext(instance), text+1);
	else 	//if we did not find the literal. Don't move text, but bring the regex forward
		return m_here(re_getNext(instance), text);
}
//mathes an alphabetical character
bool m_alpha(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_alpha: "); re_print(instance); printf("%s\n", text);}
	if (isalpha(*text))
		return m_here(re_getNext(instance), text+1);
	return false;
}
bool m_digit(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_digit: "); re_print(instance); printf("%s\n", text);}
	if (isdigit(*text))
		return m_here(re_getNext(instance), text+1);
	return false;
}
bool m_alphanum(regex instance, const char* text) {
	if(PRINT_MESSAGES){printf("m_alphanum: "); re_print(instance); printf("%s\n", text);}
	if (isalnum(*text))
		return m_here(re_getNext(instance), text+1);
	return false;
}
#endif
