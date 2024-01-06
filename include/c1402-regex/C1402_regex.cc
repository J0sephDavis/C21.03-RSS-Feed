/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef c1402_regex_implement
#define c1402_regex_implement
#include "C1402_regex.h"
using namespace C1402_regex;
//TODO: Implemented longest-match functionality for repetition options.
//creates a regex object with specified rule & literal
regex::regex(int _literal, substitution_type _sub_rule = S_LITERAL) {
	literal = _literal;
	next = NULL;
	alternate = NULL; 	//should we only find these in a regex child? I think so, imagine the tree, it has to split only at child nodes.
	sub_rule = _sub_rule;
}
bool regex::accepts(int character) {
	switch (sub_rule) {
		case(S_ALNUM):
			return std::isalnum(character);
		case(S_ALPHA):
			return std::isalpha(character);
		case(S_DIGIT):
			return std::isdigit(character);
		default:
			[[fallthrough]];
		case(S_LITERAL):
			return character == literal;
	}
}
//gets the literal of the node
int regex::getLiteral() {
	return literal;
}
//returns the next node from the node
regex* regex::getNext() {
	return next;
}
//sets the next node of the current node
void regex::setNext(regex* next_instance) {
	next = next_instance;
	//if there are alternates, set their next value to the nodes next value
	for (regex* alt = alternate; alt != NULL; alt = alt->getAlternate())
		alt->setNext(next_instance);
}
regex* regex::getAlternate() {
	return alternate;
}
void regex::addAlternate(regex* _alternate) {
	if (alternate != NULL)
		alternate->addAlternate(_alternate);
	else
		alternate = _alternate;
}
/*** 					matching 				    ***/
char* regex::match_here(char *text) {
/* Match
 * process current rule.
 * If returns true: process next.
 * If returns false && has alternate: process alternate.
 * Else: return false
 * */
	if (accepts(*text)) {
		if (next == NULL) {
			return text+1;
		}
		return (next->match_here(text+1));
	}
	else if (alternate != NULL) {
		return alternate->match_here(text);
	}
	//no valid expressions, returns false by default
	return NULL;
}
char* regex_star::match_here(char *text) {
	//perform the absolute shortest match if we have no options.
	if (next == NULL) {
		//the shortest possible match in R_STAR is NO match
		return text;
	}
	//continue attempting the current rule
	else {
		char* tmp_text = text;
		do {
			auto retVal = next->match_here(tmp_text);
			if(retVal != NULL) return retVal;
		} while (*tmp_text != '\0' && accepts(*tmp_text++));
	}
	//Failed to match expressions. Attempt alternate
	if (alternate != NULL) {
		auto retVal = alternate->match_here(text);
		if (retVal != NULL) return retVal;
	}
	//no valid expressions, returns false by default
	return NULL;
}

char* regex_plus::match_here(char* text) {
	if (next == NULL && accepts(*text)) return text+1;
	if (next != NULL) {
		for (char* tmp_text = text;*tmp_text != '\0' && accepts(*tmp_text);tmp_text++) {
			auto retVal = next->match_here(tmp_text+1);
			if (retVal != NULL) return retVal;
		}
	}
	if (alternate != NULL) {
		auto retVal = alternate->match_here(text);
		if (retVal != NULL) return retVal;
	}
	return NULL;
}
char* regex_opt::match_here(char *text) {
	if (accepts(*text)) {
		if (next != NULL ) return next->match_here(++text);
		else return text+1;
	}
	else {
		auto retVal = next->match_here(text);
		if (retVal) return retVal;
		else if (alternate != NULL) {
			retVal = alternate->match_here(text);
			if (retVal != NULL) return retVal;
		}
	}
	return NULL;
}

/*** 				regex compilation 				    ***/
//Return the rule associated with a symbol. R_DEFAULT if there is no matching rule
rules C1402_regex::utils::symbol_to_rrule(char c) {
	switch(c) {
		case('*'):
			return R_STAR;
		case('?'):
			return R_OPT;
		case('+'):
			return R_PLUS;
		default:
			return R_DEFAULT;
	}
}
//Return the substitution rule associated with the symbol. S_LITERAL for non-matching symbol
substitution_type C1402_regex::utils::symbol_to_srule(char c) {
	switch(c) {
		case('.'):
			return S_ALNUM;
		case('#'):
			return S_DIGIT;
		case('&'):
			return S_ALPHA;
		default:
			return S_LITERAL;
	}
}
static std::string utils::rule_to_string(rules rule) {
	switch(rule) {
		default:
			[[fallthrough]];
		case(R_DEFAULT):
			return "DEFAULT";
		case(R_OPT):
			return "OPTION";
		case(R_PLUS): //visit notes and recall what the name of this is. kleene closure or something
			return "PLUS";
		case(R_STAR):
			return "STAR";
	}
}
//TODO lint input, e.g., (ab)* is invalid, but (a*b*) is valid
//TODO implement this as a constructor of regex
//create a node-tree of REs from an input string
regex* C1402_regex::utils::create_from_string(std::string regex_tape) {
	regex* root_node = NULL;
	regex* last_node = NULL;
	regex* alternate_node = NULL; //holds the node which has alternates
	bool do_alternative = false;
	bool escaped_symbol = false;
	for (size_t i = 0; i < regex_tape.size(); i++) {
	//begin compilation
		regex* current_node;
		rules current_rule = R_DEFAULT;
		if (i+1 < regex_tape.size())
			current_rule = symbol_to_rrule(regex_tape.at(i+1));
		auto subrule = symbol_to_srule(regex_tape.at(i));
		int literal = regex_tape.at(i);
		if (literal == '(') {
			do_alternative = true;
			continue;
		}
		else if (literal == ')') {
			do_alternative = false;
			continue;
		}
		else if (literal == '\\' and escaped_symbol == false) {
			escaped_symbol = true;
			continue;
		}
		if (escaped_symbol) {
			subrule = S_LITERAL;
			escaped_symbol = false;
		}
		switch (current_rule) {
			case R_DEFAULT:
				current_node = new regex(literal, subrule);
				break;
			case R_OPT:
				current_node = new regex_opt(literal, subrule);
				break;
			case R_PLUS:
				current_node = new regex_plus(literal, subrule);
				break;
			case R_STAR:
				current_node = new regex_star(literal, subrule);
				break;
		}
		if (root_node == NULL) {
			root_node = current_node;
			last_node = current_node;
			if (do_alternative && alternate_node == NULL) {
				//the alternate node would likely be anything BUT null at this time
				alternate_node = current_node;
			}
		}
		else if (do_alternative) {
			if (alternate_node == NULL)
				alternate_node = current_node;
			else
				alternate_node->addAlternate(current_node);
		}
		else {
			if (alternate_node != NULL) {
				last_node->setNext(alternate_node);
				last_node = alternate_node;
				alternate_node = NULL;
			}
			last_node->setNext(current_node);
			last_node = current_node;
		}
		if (current_rule != R_DEFAULT) i+=1; //consume an extra character if we set a rule
	}//end compilation
	if (alternate_node != NULL) {
		//This occurs when the regex string ends with a ')'
		last_node->setNext(alternate_node);
	}
	return root_node;	
}
#ifdef REGEX_MAIN
/*** 				    main 					    ***/
//returns true if the expression found a match in the text, otherwise it returns false.
bool match(regex* expression, std::string input_text) {
	char* text = (char*)calloc(1,input_text.size());
	char* start_of_text = text;
	strncpy(text, input_text.c_str(), input_text.size());
	char* end_of_match = NULL;
	do {
		end_of_match = expression->match_here(text);
		if (end_of_match != NULL)
			break;
	} while(*text++ != '\0');
	if (end_of_match != NULL) {
		std::cout << "found match of length:" << (end_of_match - text) << "\n";
		std::cout << "match begins after " << (text - start_of_text) << " chars\n";
		char* matched_text = (char*)calloc(1, end_of_match - text);
		strncpy(matched_text, text, end_of_match-text);
		std::cout << "matched text: [" << std::string(matched_text) << "]\n";
		return true;
	}
	return false; 
	free(start_of_text);
}
//prints the nodes information
void re_print(regex* instance) {
	if (instance)
		printf("[Rule: %s | Literal: %c | Next? : %s | Alt?: %s]",
				utils::rule_to_string(instance->getRule()).c_str(),
				instance->getLiteral(),
				(instance->getNext()) ? "Yes" : "No",
				(instance->getAlternate() ? "yes" : "no"));
	else printf("[-]");
}
int main(int argc, char** argv) {
	int a; 				//iterator
	int input_len = 0; 			//the length of the input string, for malloc
	char* regex_expression = NULL; 	//the regex given by input
	char* input_text = NULL; 	//the text to find a match in
	char* input_anchor = NULL; 	//pointer to somewhere in input_text
	//
	if (argc < 3) return -1;
//set regex_expression
	regex_expression = argv[1];
//allocate input_text
	for (a = 2; a < argc; a++)
		input_len += strlen(argv[a]);
	input_len += (argc-2); 		//because spaces are considered separators
					//we have to add them back in.
	input_text = (char*)calloc(1,input_len); //calloc to initialize memory to 0. unsure of the importance, but valgrind wasn't happy without it
//set input text
	input_anchor = input_text; //set the anchor to the beginning of the input text
	for (a = 2; a < argc; a++) { 			//for-each arg after 1
		int arg_len = strlen(argv[a]); 		//length of the sub-string
		strncpy(input_anchor, argv[a], arg_len); 	//copy the sub-string in
		input_anchor+=arg_len; 		//move the pointer down
		if ((a+1) < argc) 			//if not last word
			*input_anchor++ = ' '; //add a space BETWIXT words
	}
//output
	printf("REGEX:%s\nTEXT:%s\n", regex_expression, input_text);
	regex* regexpr = utils::create_from_string(regex_expression);

	regex* instance = regexpr;
	while(instance && PRINT_MESSAGES) {
		re_print(instance);
		printf("\n");
		if (instance->getAlternate()) {
			instance = instance->getAlternate();
			printf("*\t");
		}
		//no kids or alternate
		else {
			instance = instance->getNext();
		}
	}
	printf("%s\n", (match(regexpr, input_text))? "match" : "no match");
	free(input_text);
	delete (regexpr);
}
#endif
#endif
